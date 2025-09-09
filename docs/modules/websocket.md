# WebSocket模块使用指南

WebSocket模块提供了基于libwebsockets的实时双向通信功能，支持文本和二进制数据传输，是LinxSDK中实现实时语音交互的核心组件。

## 模块概述

### 核心类

- **WebSocketClient**: WebSocket客户端实现

### 主要功能

- 支持WSS（安全WebSocket）和WS协议
- 异步连接和消息处理
- 文本和二进制数据传输
- 自定义HTTP头部支持
- 连接状态回调机制
- 线程安全的消息发送

## API参考

### WebSocketClient 类

```cpp
class WebSocketClient {
public:
    // 构造函数，需要提供WebSocket URL
    WebSocketClient(const std::string& ws_url);
    ~WebSocketClient();
    
    // 设置HTTP头部（认证、协议版本等）
    void SetWsHeaders(const std::map<std::string, std::string>& ws_headers);
    
    // 启动WebSocket连接
    void start();
    
    // 发送文本消息
    void send_text(const std::string& message);
    
    // 发送二进制数据
    void send_binary(const void* data, size_t len);
    
    // 设置回调函数
    void SetOnOpenCallback(std::function<std::string(void)> cb);
    void SetOnCloseCallback(std::function<void(void)> cb);
    void SetOnFailCallback(std::function<void(void)> cb);
    void SetOnMessageCallback(std::function<std::string(const std::string&, bool)> cb);
};
```

### 回调函数说明

#### OnOpenCallback
- **触发时机**: WebSocket连接成功建立时
- **返回值**: 字符串，作为连接建立后的第一条消息发送
- **用途**: 发送握手消息、认证信息等

#### OnMessageCallback
- **参数1**: 消息内容（文本或二进制数据）
- **参数2**: 是否为二进制数据（true=二进制，false=文本）
- **返回值**: 字符串，作为回复消息发送（空字符串表示不回复）
- **用途**: 处理服务器发送的消息

#### OnCloseCallback
- **触发时机**: WebSocket连接关闭时
- **用途**: 清理资源、记录日志等

#### OnFailCallback
- **触发时机**: WebSocket连接失败时
- **用途**: 错误处理、重连逻辑等

## 使用示例

### 基本WebSocket客户端

```cpp
#include "Websocket.h"
#include "Json.h"
#include "Log.h"
#include <iostream>
#include <thread>

using namespace linx;

int main() {
    try {
        // 1. 创建WebSocket客户端
        WebSocketClient ws_client("wss://echo.websocket.org");
        
        // 2. 设置连接成功回调
        ws_client.SetOnOpenCallback([]() -> std::string {
            INFO("WebSocket连接成功");
            return "Hello WebSocket!"; // 连接后发送的第一条消息
        });
        
        // 3. 设置消息接收回调
        ws_client.SetOnMessageCallback([](const std::string& msg, bool binary) -> std::string {
            if (binary) {
                INFO("收到二进制数据: {} 字节", msg.size());
            } else {
                INFO("收到文本消息: {}", msg);
            }
            return ""; // 不回复
        });
        
        // 4. 设置连接关闭回调
        ws_client.SetOnCloseCallback([]() {
            INFO("WebSocket连接已关闭");
        });
        
        // 5. 设置连接失败回调
        ws_client.SetOnFailCallback([]() {
            ERROR("WebSocket连接失败");
        });
        
        // 6. 启动连接
        ws_client.start();
        
        // 7. 发送测试消息
        std::this_thread::sleep_for(std::chrono::seconds(1));
        ws_client.send_text("测试消息");
        
        // 8. 等待用户输入
        INFO("按回车键退出...");
        std::cin.get();
        
    } catch (const std::exception& e) {
        ERROR("错误: {}", e.what());
        return -1;
    }
    
    return 0;
}
```

### 带认证的WebSocket客户端

```cpp
#include "Websocket.h"
#include "Json.h"
#include "Log.h"

using namespace linx;

class AuthenticatedWebSocketClient {
private:
    WebSocketClient ws_client;
    std::string access_token;
    std::string device_id;
    std::string session_id;
    
public:
    AuthenticatedWebSocketClient(const std::string& url, 
                               const std::string& token,
                               const std::string& device)
        : ws_client(url), access_token(token), device_id(device) {
        setupCallbacks();
        setupHeaders();
    }
    
    void setupHeaders() {
        std::map<std::string, std::string> headers;
        headers["Authorization"] = "Bearer " + access_token;
        headers["Device-Id"] = device_id;
        headers["Protocol-Version"] = "1";
        
        ws_client.SetWsHeaders(headers);
    }
    
    void setupCallbacks() {
        // 连接成功回调
        ws_client.SetOnOpenCallback([this]() -> std::string {
            INFO("认证WebSocket连接成功");
            
            // 发送hello消息
            json hello_msg = {
                {"type", "hello"},
                {"version", 1},
                {"device_id", device_id},
                {"capabilities", json::array({"audio", "tts"})}
            };
            
            return hello_msg.dump();
        });
        
        // 消息接收回调
        ws_client.SetOnMessageCallback([this](const std::string& msg, bool binary) -> std::string {
            return handleMessage(msg, binary);
        });
        
        // 连接关闭回调
        ws_client.SetOnCloseCallback([this]() {
            INFO("认证WebSocket连接已关闭");
            session_id.clear();
        });
        
        // 连接失败回调
        ws_client.SetOnFailCallback([this]() {
            ERROR("认证WebSocket连接失败");
        });
    }
    
    std::string handleMessage(const std::string& msg, bool binary) {
        if (binary) {
            // 处理二进制数据（如音频数据）
            INFO("收到二进制数据: {} 字节", msg.size());
            processBinaryData(msg);
            return "";
        }
        
        try {
            // 解析JSON消息
            json received = json::parse(msg);
            INFO("收到消息: type={}", received["type"].get<std::string>());
            
            return handleJsonMessage(received);
            
        } catch (const json_exception& e) {
            ERROR("JSON解析失败: {}", e.what());
            return "";
        }
    }
    
    std::string handleJsonMessage(const json& msg) {
        std::string type = msg["type"];
        
        if (type == "hello") {
            // 服务器hello响应
            session_id = msg["session_id"];
            INFO("获得会话ID: {}", session_id);
            
            // 开始语音会话
            json start_msg = {
                {"type", "start_session"},
                {"session_id", session_id},
                {"mode", "voice_chat"}
            };
            
            return start_msg.dump();
        }
        else if (type == "session_started") {
            INFO("语音会话已开始");
        }
        else if (type == "error") {
            ERROR("服务器错误: {}", msg["message"].get<std::string>());
        }
        
        return "";
    }
    
    void processBinaryData(const std::string& data) {
        // 处理二进制音频数据
        // 这里可以解码音频并播放
        INFO("处理音频数据: {} 字节", data.size());
    }
    
    void start() {
        ws_client.start();
    }
    
    void sendAudioData(const void* data, size_t size) {
        ws_client.send_binary(data, size);
    }
    
    void sendCommand(const json& command) {
        ws_client.send_text(command.dump());
    }
};

int main() {
    try {
        AuthenticatedWebSocketClient client(
            "wss://api.example.com/ws",
            "your-access-token",
            "device-12345"
        );
        
        client.start();
        
        // 模拟发送音频数据
        std::thread audio_sender([&client]() {
            std::this_thread::sleep_for(std::chrono::seconds(2));
            
            // 发送模拟音频数据
            std::vector<uint8_t> audio_data(1024, 0x80); // 静音数据
            client.sendAudioData(audio_data.data(), audio_data.size());
        });
        
        INFO("按回车键退出...");
        std::cin.get();
        
        audio_sender.join();
        
    } catch (const std::exception& e) {
        ERROR("错误: {}", e.what());
        return -1;
    }
    
    return 0;
}
```

### 语音交互WebSocket客户端

```cpp
#include "Websocket.h"
#include "AudioInterface.h"
#include "Opus.h"
#include "Json.h"
#include "Log.h"
#include <thread>
#include <atomic>
#include <queue>
#include <mutex>

using namespace linx;

class VoiceWebSocketClient {
private:
    WebSocketClient ws_client;
    std::unique_ptr<AudioInterface> audio;
    OpusAudio opus;
    
    std::atomic<bool> running{true};
    std::atomic<bool> recording{false};
    std::string session_id;
    
    // 音频缓冲区
    std::queue<std::vector<short>> audio_buffer;
    std::mutex buffer_mutex;
    
public:
    VoiceWebSocketClient(const std::string& url)
        : ws_client(url), opus(16000, 1) {
        setupAudio();
        setupWebSocket();
    }
    
    void setupAudio() {
        audio = CreateAudioInterface();
        audio->Init();
        audio->SetConfig(16000, 320, 1, 4, 4096, 1024);
        audio->Record();
        audio->Play();
    }
    
    void setupWebSocket() {
        // 设置认证头部
        std::map<std::string, std::string> headers;
        headers["Authorization"] = "Bearer your-token";
        ws_client.SetWsHeaders(headers);
        
        // 连接成功回调
        ws_client.SetOnOpenCallback([this]() -> std::string {
            INFO("语音WebSocket连接成功");
            
            json hello_msg = {
                {"type", "hello"},
                {"audio_params", {
                    {"format", "opus"},
                    {"sample_rate", 16000},
                    {"channels", 1},
                    {"frame_duration", 20}
                }}
            };
            
            return hello_msg.dump();
        });
        
        // 消息接收回调
        ws_client.SetOnMessageCallback([this](const std::string& msg, bool binary) -> std::string {
            if (binary) {
                // 接收到TTS音频数据
                return handleAudioData(msg);
            } else {
                // 接收到控制消息
                return handleControlMessage(msg);
            }
        });
        
        ws_client.SetOnCloseCallback([this]() {
            INFO("语音WebSocket连接已关闭");
            running = false;
        });
        
        ws_client.SetOnFailCallback([this]() {
            ERROR("语音WebSocket连接失败");
            running = false;
        });
    }
    
    std::string handleAudioData(const std::string& opus_data) {
        // 解码Opus音频数据
        std::vector<opus_int16> pcm_data(320);
        int decoded = opus.Decode(pcm_data.data(), 320,
                                (unsigned char*)opus_data.data(), opus_data.size());
        
        if (decoded > 0) {
            // 将解码的音频添加到播放缓冲区
            std::lock_guard<std::mutex> lock(buffer_mutex);
            std::vector<short> audio_chunk(pcm_data.begin(), pcm_data.begin() + decoded);
            audio_buffer.push(audio_chunk);
        }
        
        return "";
    }
    
    std::string handleControlMessage(const std::string& msg) {
        try {
            json received = json::parse(msg);
            std::string type = received["type"];
            
            if (type == "hello") {
                session_id = received["session_id"];
                INFO("获得会话ID: {}", session_id);
                
                // 开始录音
                startRecording();
            }
            else if (type == "tts") {
                std::string state = received["state"];
                if (state == "start") {
                    // TTS开始，停止录音避免回音
                    stopRecording();
                } else if (state == "stop") {
                    // TTS结束，重新开始录音
                    startRecording();
                }
            }
            
        } catch (const json_exception& e) {
            ERROR("JSON解析失败: {}", e.what());
        }
        
        return "";
    }
    
    void startRecording() {
        recording = true;
        INFO("开始录音");
        
        json start_msg = {
            {"type", "listen"},
            {"session_id", session_id},
            {"state", "start"}
        };
        
        ws_client.send_text(start_msg.dump());
    }
    
    void stopRecording() {
        recording = false;
        INFO("停止录音");
    }
    
    void start() {
        ws_client.start();
        
        // 启动音频录制线程
        std::thread record_thread([this]() {
            recordingLoop();
        });
        
        // 启动音频播放线程
        std::thread playback_thread([this]() {
            playbackLoop();
        });
        
        record_thread.join();
        playback_thread.join();
    }
    
    void recordingLoop() {
        short buffer[320];
        std::vector<unsigned char> opus_buffer(320);
        
        while (running) {
            if (recording && audio->Read(buffer, 320)) {
                // 编码为Opus格式
                int encoded = opus.Encode(opus_buffer.data(), 320, buffer, 320);
                
                if (encoded > 0) {
                    // 发送音频数据
                    ws_client.send_binary(opus_buffer.data(), encoded);
                }
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
    
    void playbackLoop() {
        while (running) {
            std::vector<short> audio_chunk;
            
            {
                std::lock_guard<std::mutex> lock(buffer_mutex);
                if (!audio_buffer.empty()) {
                    audio_chunk = audio_buffer.front();
                    audio_buffer.pop();
                }
            }
            
            if (!audio_chunk.empty()) {
                // 播放音频
                audio->Write(audio_chunk.data(), audio_chunk.size());
            } else {
                // 播放静音
                short silence[320] = {0};
                audio->Write(silence, 320);
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
    }
};

int main() {
    try {
        VoiceWebSocketClient client("wss://voice-api.example.com/ws");
        client.start();
        
    } catch (const std::exception& e) {
        ERROR("错误: {}", e.what());
        return -1;
    }
    
    return 0;
}
```

## 协议设计建议

### 消息格式

#### 文本消息（JSON格式）

```json
{
  "type": "message_type",
  "session_id": "unique_session_id",
  "timestamp": 1234567890,
  "data": {
    // 消息特定数据
  }
}
```

#### 常见消息类型

1. **握手消息**
```json
{
  "type": "hello",
  "version": 1,
  "capabilities": ["audio", "tts"],
  "audio_params": {
    "format": "opus",
    "sample_rate": 16000,
    "channels": 1
  }
}
```

2. **控制消息**
```json
{
  "type": "listen",
  "session_id": "session_123",
  "state": "start"
}
```

3. **状态消息**
```json
{
  "type": "tts",
  "session_id": "session_123",
  "state": "start"
}
```

4. **错误消息**
```json
{
  "type": "error",
  "code": 1001,
  "message": "Invalid session ID"
}
```

### 二进制数据格式

- **音频数据**: 直接发送Opus编码的音频帧
- **大文件**: 可以分块传输，在JSON消息中协调

## 错误处理

### 连接错误处理

```cpp
class ReconnectingWebSocketClient {
private:
    WebSocketClient ws_client;
    std::atomic<bool> should_reconnect{true};
    int reconnect_delay = 1000; // 毫秒
    int max_reconnect_delay = 30000;
    
public:
    void startWithReconnect() {
        while (should_reconnect) {
            try {
                ws_client.start();
                reconnect_delay = 1000; // 重置延迟
                break;
            } catch (const std::exception& e) {
                ERROR("连接失败: {}, {}ms后重试", e.what(), reconnect_delay);
                
                std::this_thread::sleep_for(std::chrono::milliseconds(reconnect_delay));
                
                // 指数退避
                reconnect_delay = std::min(reconnect_delay * 2, max_reconnect_delay);
            }
        }
    }
};
```

### 消息发送错误处理

```cpp
class ReliableWebSocketClient {
private:
    WebSocketClient ws_client;
    std::queue<std::string> pending_messages;
    std::mutex message_mutex;
    
public:
    void sendReliable(const std::string& message) {
        try {
            ws_client.send_text(message);
        } catch (const std::exception& e) {
            // 发送失败，加入重发队列
            std::lock_guard<std::mutex> lock(message_mutex);
            pending_messages.push(message);
            ERROR("消息发送失败，加入重发队列: {}", e.what());
        }
    }
    
    void retryPendingMessages() {
        std::lock_guard<std::mutex> lock(message_mutex);
        while (!pending_messages.empty()) {
            try {
                ws_client.send_text(pending_messages.front());
                pending_messages.pop();
            } catch (const std::exception& e) {
                ERROR("重发消息失败: {}", e.what());
                break;
            }
        }
    }
};
```

## 性能优化

### 1. 消息批处理

```cpp
class BatchingWebSocketClient {
private:
    std::vector<std::string> message_batch;
    std::mutex batch_mutex;
    std::thread batch_thread;
    
public:
    void addToBatch(const std::string& message) {
        std::lock_guard<std::mutex> lock(batch_mutex);
        message_batch.push_back(message);
    }
    
    void startBatchSender() {
        batch_thread = std::thread([this]() {
            while (running) {
                std::vector<std::string> batch;
                {
                    std::lock_guard<std::mutex> lock(batch_mutex);
                    batch.swap(message_batch);
                }
                
                if (!batch.empty()) {
                    json batch_msg = {
                        {"type", "batch"},
                        {"messages", batch}
                    };
                    
                    ws_client.send_text(batch_msg.dump());
                }
                
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        });
    }
};
```

### 2. 压缩支持

```cpp
// 在WebSocket头部启用压缩
std::map<std::string, std::string> headers;
headers["Sec-WebSocket-Extensions"] = "permessage-deflate";
ws_client.SetWsHeaders(headers);
```

## 最佳实践

1. **使用心跳机制**：定期发送ping消息保持连接
2. **实现重连逻辑**：处理网络中断和服务器重启
3. **消息去重**：使用消息ID避免重复处理
4. **流量控制**：避免发送过多消息导致缓冲区溢出
5. **安全考虑**：使用WSS协议和适当的认证机制
6. **错误恢复**：实现优雅的错误处理和状态恢复
7. **日志记录**：记录连接状态和重要事件
8. **资源清理**：确保连接关闭时正确清理资源