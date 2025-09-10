/**
 * @file linx.cc
 * @brief Linx音频客户端主程序
 * @description 实现实时音频录制、播放和WebSocket通信的跨平台音频客户端
 *              支持Opus音频编解码，提供TTS语音合成和语音识别功能
 * @author Linx Team
 * @date 2024
 */

// 标准库头文件
#include <atomic>           // 原子操作
#include <condition_variable> // 条件变量
#include <iostream>         // 输入输出流
#include <memory>           // 智能指针
#include <mutex>            // 互斥锁
#include <string>           // 字符串
#include <thread>           // 线程
#include <queue>            // 队列容器
#include <vector>           // 向量容器

// Linx SDK头文件
#include "AudioInterface.h" // 音频接口抽象类
#include "HttpClient.h"     // HTTP客户端
#include "Json.h"           // JSON处理
#include "Log.h"            // 日志系统
#include "Opus.h"           // Opus音频编解码
#include "Websocket.h"      // WebSocket客户端

using namespace linx;

// ==================== 全局配置常量 ====================

// 服务器配置
const std::string ota_url = "https://xrobo.qiniuapi.com/v1/ota/";  // OTA固件更新服务器地址
const std::string ws_url = "ws://xrobo-io.qiniuapi.com/v1/ws/";  // WebSocket服务器地址
const std::string access_token = "test-token";  // 访问令牌

// 设备标识
const std::string device_mac = "98:a3:16:f9:d9:34";   // 设备MAC地址
const std::string device_uuid = "98:a3:16:f9:d9:34";  // 设备UUID



// 音频参数配置
const int CHUNK = 960;        // 音频数据块大小（样本数）
const int SAMPLE_RATE = 16000; // 采样率（Hz）
const int CHANNELS = 1;       // 声道数（单声道）

// ==================== 音频缓冲区管理 ====================

/**
 * @brief 音频缓冲区类
 * @description 线程安全的音频数据缓冲区，用于解决TTS音频数据不规律到达导致的播放underflow问题
 *              采用生产者-消费者模式，WebSocket线程作为生产者推入音频数据，播放线程作为消费者取出数据
 */
struct AudioBuffer {
    std::queue<std::vector<short>> buffer_queue;  // 音频数据队列
    std::mutex buffer_mutex;                      // 队列访问互斥锁
    std::condition_variable buffer_cv;            // 条件变量，用于线程同步
    std::atomic<bool> has_data{false};           // 原子布尔值，标识是否有数据
    
    /**
     * @brief 向缓冲区推入音频数据
     * @param data 音频数据指针
     * @param size 数据大小（样本数）
     */
    void push(const short* data, size_t size) {
        std::lock_guard<std::mutex> lock(buffer_mutex);  // 自动加锁
        std::vector<short> chunk(data, data + size);     // 复制音频数据
        buffer_queue.push(std::move(chunk));             // 移动语义推入队列
        has_data = true;                                 // 标记有数据
        buffer_cv.notify_one();                          // 通知等待的消费者线程
    }
    
    /**
     * @brief 从缓冲区取出音频数据
     * @param chunk 输出参数，存储取出的音频数据
     * @return true表示成功取出数据，false表示队列为空
     */
    bool pop(std::vector<short>& chunk) {
        std::unique_lock<std::mutex> lock(buffer_mutex);  // 可解锁的锁
        if (buffer_queue.empty()) {
            return false;  // 队列为空，返回失败
        }
        chunk = std::move(buffer_queue.front());  // 移动语义取出数据
        buffer_queue.pop();                       // 弹出队列头部
        has_data = !buffer_queue.empty();         // 更新数据状态
        return true;
    }
    
    /**
     * @brief 等待缓冲区有数据
     * @description 阻塞当前线程直到缓冲区有数据可用
     */
    void wait_for_data() {
        std::unique_lock<std::mutex> lock(buffer_mutex);
        buffer_cv.wait(lock, [this] { return !buffer_queue.empty(); });
    }
};

// ==================== 全局状态管理 ====================

/**
 * @brief 音频状态结构体
 * @description 管理整个应用程序的运行状态和音频处理状态
 */
struct AudioState {
    std::atomic<bool> running{true};        // 原子布尔值，控制所有线程的运行状态
    std::string listen_state = "stop";      // 语音识别状态："start"开始录音，"stop"停止录音
    std::string tts_state = "idle";         // TTS播放状态："start"开始播放，"stop"停止播放，"idle"空闲
    std::string session_id;                 // WebSocket会话ID
};

// ==================== 全局对象实例 ====================

AudioBuffer audio_buffer;                           // 音频缓冲区实例
std::unique_ptr<AudioInterface> audio;              // 音频接口智能指针（平台相关实现）
OpusAudio opus(SAMPLE_RATE, CHANNELS);              // Opus编解码器实例
AudioState linx_state;                              // 全局状态实例
WebSocketClient ws_client(ws_url);                  // WebSocket客户端实例

// ==================== OTA固件更新相关函数 ====================

/**
 * @brief 获取OTA固件版本信息
 * @description 向OTA服务器发送设备硬件信息，获取最新固件版本和WebSocket连接信息
 *              这是应用启动时的第一步，用于设备注册和配置获取
 */
void get_ota_version() {
    // 构建设备信息JSON数据
    json ota_post_data = {
        {"flash_size", 16777216},                    // Flash存储大小（16MB）
        {"minimum_free_heap_size", 8318916},        // 最小可用堆内存
        {"mac_address", device_mac},                // 设备MAC地址
        {"chip_model_name", "esp32s3"},             // 芯片型号
        {"chip_info", {{"model", 9}, {"cores", 2}, {"revision", 2}, {"features", 18}}}, // 芯片详细信息
        {"application", {{"name", "Linx"}, {"version", "1.6.0"}}},  // 应用程序信息
        {"partition_table", json::array()},         // 分区表信息
        {"ota", {{"label", "factory"}}},            // OTA标签
        {"board", {{"type", "bread-compact-wifi"}, {"ip", "192.168.124.38"}, {"mac", device_mac}}} // 开发板信息
    };

    // 发送HTTP POST请求
    std::string post_data = ota_post_data.dump();  // 转换为JSON字符串
    std::string response;                           // 服务器响应
    linx::HttpClient hc;                           // HTTP客户端实例
    hc.reset(ota_url);                             // 设置服务器URL
    
    // 设置HTTP请求头
    std::map<std::string, std::string> header;
    header["Device-Id"] = device_mac;              // 添加设备ID头部
    
    // 发送POST请求
    hc.postJson(response, post_data, header);

    // 记录请求和响应日志
    INFO("OTA Request:{}", post_data);
    INFO("OTA Response:{}", response);
}

// ==================== 主函数 ====================

/**
 * @brief 程序主入口函数
 * @description 初始化音频系统，启动多个工作线程，建立WebSocket连接
 *              实现完整的语音交互流程：录音->识别->TTS->播放
 * @return 0表示正常退出，-1表示异常退出
 */
int main() {
    try {
        // ==================== 初始化阶段 ====================
        
        // 1. 获取OTA固件信息和服务器配置
        get_ota_version();
        
        // 2. 初始化音频接口（平台相关：Linux使用ALSA，macOS使用PortAudio）
        audio = CreateAudioInterface();                              // 创建平台相关的音频接口实例
        audio->Init();                                              // 初始化音频系统
        audio->SetConfig(SAMPLE_RATE, 320, CHANNELS, 4, 4096, 1024); // 配置音频参数
        audio->Record();                                            // 初始化录音流
        audio->Play();                                              // 初始化播放流，用于TTS音频输出

        // ==================== 启动工作线程 ====================
        
        // 3. 启动音频播放线程（消费者线程）
        // 功能：从音频缓冲区取出TTS数据并播放，防止播放underflow
        std::thread playback_thread = std::thread([]() {
            std::vector<short> audio_chunk;  // 临时存储音频数据块
            
            while (linx_state.running) {
                if (audio_buffer.pop(audio_chunk)) {
                    // 有TTS音频数据时，播放实际音频
                    audio->Write(audio_chunk.data(), audio_chunk.size());
                } else {
                    // 没有数据时播放静音，防止PortAudio输出underflow错误
                    short silence[CHUNK] = {0};  // 创建静音数据
                    audio->Write(silence, CHUNK);
                    usleep(20 * 1000);           // 休眠20毫秒，对应一个音频帧的时长
                }
            }
        });

        // 4. 启动音频录制线程（生产者线程）
        // 功能：持续录制音频，编码为Opus格式，通过WebSocket发送给服务器进行语音识别
        std::thread audio_thread = std::thread([]() {
            std::vector<unsigned char> opus_buffer(CHUNK);  // Opus编码缓冲区
            short buffer[CHUNK] = {};                       // PCM音频数据缓冲区

            while (linx_state.running) {
                // 从音频设备读取PCM数据
                audio->Read(buffer, CHUNK);

                // 检查是否处于录音状态
                if (linx_state.listen_state != "start") {
                    usleep(10 * 1000);  // 非录音状态时休眠10毫秒
                    continue;
                }

                // 将PCM数据编码为Opus格式
                opus_buffer.insert(opus_buffer.end(), buffer, buffer + CHUNK);
                int encoded = opus.Encode(opus_buffer.data(), 2 * CHUNK, buffer, CHUNK);
                
                // 如果编码成功，通过WebSocket发送二进制数据
                if (encoded > 0) {
                    ws_client.send_binary(opus_buffer.data(), encoded);
                }
                
                opus_buffer.clear();  // 清空缓冲区
                usleep(10 * 1000);    // 休眠10毫秒
            }
        });

        // 5. 启动WebSocket通信线程
        // 功能：建立WebSocket连接，处理服务器消息，管理会话状态
        std::thread ws_thread = std::thread([]() {
            // 设置WebSocket请求头
            std::map<std::string, std::string> headers;
            headers["Authorization"] = "Bearer " + access_token;  // 认证令牌
            headers["Protocol-Version"] = "1";                   // 协议版本
            headers["Device-Id"] = device_mac;                   // 设备ID
            headers["Client-Id"] = device_uuid;                  // 客户端ID

            ws_client.SetWsHeaders(headers);  // 设置WebSocket头部
            // 设置WebSocket连接建立回调
            // 功能：连接成功后发送hello消息，告知服务器音频参数
            ws_client.SetOnOpenCallback([&]() -> std::string {
                INFO("on open");  // 记录连接成功日志
                
                // 构建hello消息，包含音频参数配置
                json hello_msg = {
                    {"type", "hello"},                    // 消息类型
                    {"version", 1},                      // 协议版本
                    {"transport", "websocket"},          // 传输方式
                    {"audio_params", {                   // 音频参数
                        {"format", "opus"},              // 音频格式：Opus
                        {"sample_rate", SAMPLE_RATE},    // 采样率：16kHz
                        {"channels", CHANNELS},          // 声道数：单声道
                        {"frame_duration", 60}           // 帧持续时间：60ms
                    }}
                };
                return hello_msg.dump();  // 返回JSON字符串
            });

            // 设置WebSocket连接关闭回调
            // 功能：连接断开时清理状态，停止所有线程
            ws_client.SetOnCloseCallback([]() {
                linx_state.listen_state = "stop";  // 停止录音
                linx_state.running = false;        // 停止所有线程
                INFO("WebSocket disconnected");    // 记录断开日志
            });

            // 设置WebSocket连接失败回调
            // 功能：连接失败时记录错误日志
            ws_client.SetOnFailCallback([]() { 
                ERROR("WebSocket connection failed"); 
            });

            // 设置WebSocket消息接收回调
            // 功能：处理服务器发送的文本消息和二进制音频数据
            ws_client.SetOnMessageCallback([](const std::string& msg, bool binary) -> std::string {
                if (binary) {
                    // ==================== 处理二进制音频数据（TTS） ====================
                    // INFO("<< binary data");  // 可选：记录接收到二进制数据
                    
                    // 解码Opus音频数据为PCM格式
                    std::vector<opus_int16> pcm_data(CHUNK);  // PCM解码缓冲区
                    int decoded = opus.Decode(pcm_data.data(), CHUNK, 
                                            (unsigned char*)(msg.data()), msg.size());
                    
                    if (decoded > 0) {
                        // 解码成功，将PCM数据转换为short格式
                        short buffer[CHUNK] = {};
                        memcpy((char*)&buffer[0], (char*)&pcm_data[0], CHUNK * 2);
                        
                        // 将音频数据推入缓冲区，由播放线程异步播放
                        // 这样可以避免直接播放导致的underflow问题
                        audio_buffer.push(buffer, CHUNK);
                    }
                    return "";  // 二进制消息不需要回复
                } else {
                    // ==================== 处理文本消息（控制指令） ====================
                    try {
                        std::string resmsg = msg;
                        INFO("<< {}", resmsg);  // 记录接收到的消息
                        
                        // 验证消息格式：必须是有效的JSON
                        if (resmsg.empty() || resmsg[0] != '{') {
                            WARN("Received non-JSON message, ignoring: {}", resmsg);
                            return "";  // 忽略非JSON消息
                        }
                        
                        // 解析JSON消息
                        json received_msg = json::parse(resmsg);
                        
                        // 处理hello响应：服务器确认连接，返回会话ID
                        if (received_msg["type"] == "hello") {
                            linx_state.session_id = received_msg["session_id"];  // 保存会话ID
                            
                            // 构建开始录音消息
                            json start_msg = {
                                {"session_id", linx_state.session_id},  // 会话ID
                                {"type", "listen"},                     // 消息类型：开始监听
                                {"state", "start"},                    // 状态：开始
                                {"mode", "auto"}                       // 模式：自动
                            };

                            linx_state.listen_state = "start";  // 设置录音状态为开始
                            INFO("");                            // 空日志行，用于格式化
                            return start_msg.dump();             // 返回开始录音消息
                        }

                        // 处理TTS状态消息：服务器通知TTS播放状态变化
                        if (received_msg["type"] == "tts") {
                            linx_state.tts_state = received_msg["state"];  // 更新TTS状态
                        }

                        // TTS播放结束后，重新开始录音监听
                        if (linx_state.tts_state == "stop") {
                            linx_state.session_id = received_msg["session_id"];  // 更新会话ID
                            
                            // 构建重新开始录音消息
                            json start_msg = {
                                {"session_id", linx_state.session_id},  // 会话ID
                                {"type", "listen"},                     // 消息类型：开始监听
                                {"state", "start"},                    // 状态：开始
                                {"mode", "auto"}                       // 模式：自动
                            };

                            linx_state.listen_state = "start";  // 重新开始录音
                            INFO("");                            // 空日志行
                            return start_msg.dump();             // 返回开始录音消息
                        }

                        // TTS开始播放时，停止录音避免回音
                        if (linx_state.tts_state == "start") {
                            linx_state.listen_state = "stop";   // 停止录音
                        }

                        // 处理goodbye消息：会话结束
                        if (received_msg["type"] == "goodbye" &&
                            linx_state.session_id == received_msg["session_id"]) {
                            INFO("<< Goodbye");              // 记录会话结束
                            linx_state.session_id = "";      // 清空会话ID
                        }

                    } catch (const std::exception& e) {
                        // JSON解析异常处理
                        ERROR("JSON parse error: {}", e.what());
                        ERROR("Raw message content (first 100 chars): {}", msg.substr(0, 100));
                        
                        // 输出消息的十六进制表示，便于调试非标准JSON消息
                        std::string hex_dump;
                        for (size_t i = 0; i < std::min(msg.size(), size_t(50)); ++i) {
                            char buf[4];
                            sprintf(buf, "%02x ", (unsigned char)msg[i]);  // 转换为十六进制
                            hex_dump += buf;
                        }
                        ERROR("Message hex dump: {}", hex_dump);
                    }
                }
                return "";  // 文本消息处理完成，无需回复
            });

            // 启动WebSocket客户端，开始连接服务器
            ws_client.start();
        });

        // ==================== 主线程等待和清理 ====================
        
        // 主线程等待用户输入，按回车键退出程序
        INFO("Press Enter to exit...");
        std::cin.get();                    // 阻塞等待用户输入
        linx_state.running = false;        // 设置退出标志，通知所有线程停止
        
        // 等待所有工作线程安全结束
        if (playback_thread.joinable()) {
            playback_thread.join();         // 等待播放线程结束
        }
        if (audio_thread.joinable()) {
            audio_thread.join();            // 等待录音线程结束
        }
        if (ws_thread.joinable()) {
            ws_thread.join();               // 等待WebSocket线程结束
        }

    } catch (const std::exception& e) {
        // 捕获所有异常，记录错误日志
        ERROR("Fatal error: {}", e.what());
        return -1;  // 异常退出
    }

    return 0;  // 正常退出
}