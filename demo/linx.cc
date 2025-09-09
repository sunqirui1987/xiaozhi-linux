#include <atomic>
#include <condition_variable>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

#include "AudioInterface.h"
#include "HttpClient.h"
#include "Json.h"
#include "Log.h"
#include "Opus.h"
#include "Websocket.h"

using namespace linx;

// 全局配置
const std::string ota_url = "https://xrobo.qiniuapi.com/v1/ota/";
const std::string ws_url = "wss://xrobo-io.qiniuapi.com/v1/ws/";
const std::string access_token = "test-token";
const std::string device_mac = "30:ed:a0:30:cd:b4";
const std::string device_uuid = "30:ed:a0:30:cd:b4";

const int CHUNK = 960;
const int SAMPLE_RATE = 16000;
const int CHANNELS = 1;

// 全局状态
struct AudioState {
    std::atomic<bool> running{true};
    std::string listen_state = "stop";
    std::string tts_state = "idle";
    std::string session_id;
};

std::unique_ptr<AudioInterface> audio;
OpusAudio opus(SAMPLE_RATE, CHANNELS);
AudioState linx_state;
WebSocketClient ws_client(ws_url);

// 发送硬件信息，获取固件版本
void get_ota_version() {
    json ota_post_data = {
        {"flash_size", 16777216},
        {"minimum_free_heap_size", 8318916},
        {"mac_address", device_mac},
        {"chip_model_name", "esp32s3"},
        {"chip_info", {{"model", 9}, {"cores", 2}, {"revision", 2}, {"features", 18}}},
        {"application", {{"name", "Linx"}, {"version", "1.6.0"}}},
        {"partition_table", json::array()},
        {"ota", {{"label", "factory"}}},
        {"board", {{"type", "bread-compact-wifi"}, {"ip", "192.168.124.38"}, {"mac", device_mac}}}};

    std::string post_data = ota_post_data.dump();
    std::string response;
    linx::HttpClient hc;
    hc.reset(ota_url);
    std::map<std::string, std::string> header;
    header["Device-Id"] = device_mac;
    hc.postJson(response, post_data, header);

    INFO("OTA Request:{}", post_data);
    INFO("OTA Response:{}", response);
}

int main() {
    try {
        // 获取OTA信息
        get_ota_version();
        
        // 初始化音频接口
        audio = CreateAudioInterface();
        audio->Init();
        audio->SetConfig(SAMPLE_RATE, 320, CHANNELS, 4, 4096, 1024);
        audio->Record();

        // 启动音频线程
        std::thread audio_thread = std::thread([]() {
            std::vector<unsigned char> opus_buffer(CHUNK);
            short buffer[CHUNK] = {};

            while (linx_state.running) {
                audio->Read(buffer, CHUNK);

                if (linx_state.listen_state != "start") {
                    usleep(10 * 1000);
                    continue;
                }

                opus_buffer.insert(opus_buffer.end(), buffer, buffer + CHUNK);
                int encoded = opus.Encode(opus_buffer.data(), 2 * CHUNK, buffer, CHUNK);
                if (encoded > 0) {
                    ws_client.send_binary(opus_buffer.data(), encoded);
                }
                opus_buffer.clear();
                usleep(10 * 1000);
            }
        });

        // 启动websocket线程
        std::thread ws_thread = std::thread([]() {
            std::map<std::string, std::string> headers;
            headers["Authorization"] = "Bearer " + access_token;
            headers["Protocol-Version"] = "1";
            headers["Device-Id"] = device_mac;
            headers["Client-Id"] = device_uuid;

            ws_client.SetWsHeaders(headers);
            ws_client.SetOnOpenCallback([&]() -> std::string {
                INFO("on open");
                json hello_msg = {{"type", "hello"},
                                  {"version", 1},
                                  {"transport", "websocket"},
                                  {"audio_params",
                                   {{"format", "opus"},
                                    {"sample_rate", SAMPLE_RATE},
                                    {"channels", CHANNELS},
                                    {"frame_duration", 60}}}};
                return hello_msg.dump();
            });

            ws_client.SetOnCloseCallback([]() {
                linx_state.listen_state = "stop";
                linx_state.running = false;
                INFO("WebSocket disconnected");
            });

            ws_client.SetOnFailCallback([]() { ERROR("WebSocket connection failed"); });

            ws_client.SetOnMessageCallback([](const std::string& msg, bool binary) -> std::string {
                if (binary) {
                    // INFO("<< binary data");
                    // 处理二进制音频数据
                    std::vector<opus_int16> pcm_data(CHUNK);
                    int decoded = opus.Decode(pcm_data.data(), CHUNK, (unsigned char*)(msg.data()),
                                              msg.size());
                    if (decoded > 0) {
                        short buffer[CHUNK] = {};
                        memcpy((char*)&buffer[0], (char*)&pcm_data[0], CHUNK * 2);
                        audio->Write(buffer, CHUNK);
                    }
                    return "";
                } else {
                    // 处理文本消息
                    try {
                        std::string resmsg = msg;
                        INFO("<< {}", resmsg);
                        
                        // 检查消息是否为有效的JSON格式
                        if (resmsg.empty() || resmsg[0] != '{') {
                            WARN("Received non-JSON message, ignoring: {}", resmsg);
                            return "";
                        }
                        
                        json received_msg = json::parse(resmsg);
                        if (received_msg["type"] == "hello") {
                            linx_state.session_id = received_msg["session_id"];
                            json start_msg = {{"session_id", linx_state.session_id},
                                              {"type", "listen"},
                                              {"state", "start"},
                                              {"mode", "auto"}};

                            linx_state.listen_state = "start";
                            INFO("");
                            return start_msg.dump();
                        }

                        if (received_msg["type"] == "tts") {
                            linx_state.tts_state = received_msg["state"];
                        }

                        if (linx_state.tts_state == "stop") {
                            linx_state.session_id = received_msg["session_id"];
                            json start_msg = {{"session_id", linx_state.session_id},
                                              {"type", "listen"},
                                              {"state", "start"},
                                              {"mode", "auto"}};

                            linx_state.listen_state = "start";
                            INFO("");
                            return start_msg.dump();
                        }

                        if (linx_state.tts_state == "start") {
                            linx_state.listen_state = "stop";
                        }

                        if (received_msg["type"] == "goodbye" &&
                            linx_state.session_id == received_msg["session_id"]) {
                            INFO("<< Goodbye");
                            linx_state.session_id = "";
                        }

                    } catch (const std::exception& e) {
                        ERROR("JSON parse error: {}", e.what());
                        ERROR("Raw message content (first 100 chars): {}", msg.substr(0, 100));
                        // 输出消息的十六进制表示以便调试
                        std::string hex_dump;
                        for (size_t i = 0; i < std::min(msg.size(), size_t(50)); ++i) {
                            char buf[4];
                            sprintf(buf, "%02x ", (unsigned char)msg[i]);
                            hex_dump += buf;
                        }
                        ERROR("Message hex dump: {}", hex_dump);
                    }
                }
                return "";
            });

            ws_client.start();
        });

        // 主线程等待
        INFO("Press Enter to exit...");
        std::cin.get();
        linx_state.running = false;
        if (audio_thread.joinable()) {
            audio_thread.join();
        }
        if (ws_thread.joinable()) {
            ws_thread.join();
        }

    } catch (const std::exception& e) {
        ERROR("Fatal error: {}", e.what());
        return -1;
    }

    return 0;
}