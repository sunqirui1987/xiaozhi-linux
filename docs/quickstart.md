# LinxSDK 快速开始指南

本指南将帮助您快速上手LinxSDK，从环境搭建到运行第一个语音交互应用。

## 环境要求

### 操作系统支持
- macOS 10.15+
- Linux (Ubuntu 18.04+, CentOS 7+)

### 编译工具
- CMake 3.22+
- GCC 7+ 或 Clang 10+
- C++17 支持

## 安装依赖

### macOS

使用Homebrew安装依赖：

```bash
# 方法1：使用项目提供的脚本
make prepare

# 方法2：手动安装
brew install websocketpp portaudio opus curl openssl cmake
```

### Linux (Ubuntu/Debian)

```bash
# 安装基础依赖
sudo apt update
sudo apt install -y build-essential cmake pkg-config

# 安装音频和网络库
sudo apt install -y libasound2-dev libopus-dev libcurl4-openssl-dev
sudo apt install -y libssl-dev libwebsockets-dev

# 安装PortAudio（可选，用于跨平台兼容）
sudo apt install -y portaudio19-dev
```

### Linux (CentOS/RHEL)

```bash
# 安装基础依赖
sudo yum groupinstall -y "Development Tools"
sudo yum install -y cmake3 pkgconfig

# 安装音频和网络库
sudo yum install -y alsa-lib-devel opus-devel libcurl-devel
sudo yum install -y openssl-devel libwebsockets-devel
```

## 编译项目

### 方法1：使用Makefile（推荐）

```bash
# 克隆项目
git clone <repository-url>
cd xiaozhi-linux

# 编译项目
make build

# 运行示例
make run
```

### 方法2：使用CMake

```bash
# 创建构建目录
mkdir build && cd build

# 配置项目
cmake -DCMAKE_BUILD_TYPE=Debug \
      -DBUILD_SHARED_LIBS=OFF \
      -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
      ..

# 编译
make -j$(nproc)

# 安装
make install
```

## 第一个应用：音频录制

创建一个简单的音频录制应用：

```cpp
// simple_record.cpp
#include "AudioInterface.h"
#include "Log.h"
#include <iostream>
#include <thread>
#include <chrono>

using namespace linx;

int main() {
    try {
        // 1. 创建音频接口
        auto audio = CreateAudioInterface();
        
        // 2. 初始化音频系统
        audio->Init();
        
        // 3. 配置音频参数
        // 参数：采样率16kHz, 帧大小320, 单声道, 4个周期, 缓冲区4096, 周期大小1024
        audio->SetConfig(16000, 320, 1, 4, 4096, 1024);
        
        // 4. 开始录制
        audio->Record();
        
        INFO("开始录制音频，按回车键停止...");
        
        // 5. 录制循环
        short buffer[320]; // 20ms的音频数据
        bool recording = true;
        
        std::thread record_thread([&]() {
            while (recording) {
                if (audio->Read(buffer, 320)) {
                    // 这里可以处理音频数据
                    // 例如：计算音量、保存到文件等
                    
                    // 计算音频能量（简单的音量指示）
                    long energy = 0;
                    for (int i = 0; i < 320; i++) {
                        energy += abs(buffer[i]);
                    }
                    
                    if (energy > 1000000) { // 检测到声音
                        INFO("检测到声音，能量值: {}", energy);
                    }
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        });
        
        // 等待用户输入
        std::cin.get();
        recording = false;
        
        // 等待录制线程结束
        record_thread.join();
        
        INFO("录制结束");
        
    } catch (const std::exception& e) {
        ERROR("错误: {}", e.what());
        return -1;
    }
    
    return 0;
}
```

编译并运行：

```bash
# 编译
g++ -std=c++17 simple_record.cpp -I../linxsdk/audio/include \
    -I../linxsdk/log/include -I../linxsdk/thirdparty/spdlog/include \
    -L../build/install/lib -llinx -o simple_record

# 运行
./simple_record
```

## 第二个应用：WebSocket通信

创建一个WebSocket客户端应用：

```cpp
// websocket_client.cpp
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
            
            // 发送hello消息
            json hello_msg = {
                {"type", "hello"},
                {"message", "Hello from LinxSDK!"}
            };
            
            return hello_msg.dump();
        });
        
        // 3. 设置消息接收回调
        ws_client.SetOnMessageCallback([](const std::string& msg, bool binary) -> std::string {
            if (binary) {
                INFO("收到二进制数据，大小: {} 字节", msg.size());
            } else {
                INFO("收到文本消息: {}", msg);
                
                try {
                    json received = json::parse(msg);
                    INFO("解析JSON成功: type={}", received["type"].get<std::string>());
                } catch (const json_exception& e) {
                    WARN("JSON解析失败: {}", e.what());
                }
            }
            return ""; // 不需要回复
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
        std::thread sender([&ws_client]() {
            std::this_thread::sleep_for(std::chrono::seconds(2));
            
            for (int i = 0; i < 5; i++) {
                json test_msg = {
                    {"type", "test"},
                    {"sequence", i},
                    {"timestamp", std::time(nullptr)}
                };
                
                ws_client.send_text(test_msg.dump());
                INFO("发送测试消息 #{}", i);
                
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
        });
        
        INFO("WebSocket客户端运行中，按回车键退出...");
        std::cin.get();
        
        sender.join();
        
    } catch (const std::exception& e) {
        ERROR("错误: {}", e.what());
        return -1;
    }
    
    return 0;
}
```

## 第三个应用：完整的语音交互

参考项目中的 `demo/linx.cc` 文件，这是一个完整的语音交互应用示例，包含：

- 实时音频录制
- Opus音频编码
- WebSocket通信
- TTS音频播放
- 多线程协调

运行完整示例：

```bash
# 编译并运行demo
make build
make run
```

## 常见问题

### 1. 音频设备权限问题（macOS）

如果遇到音频设备访问权限问题：

```bash
# 检查音频设备
system_profiler SPAudioDataType

# 在系统偏好设置中授予麦克风权限
# 系统偏好设置 -> 安全性与隐私 -> 隐私 -> 麦克风
```

### 2. 编译错误：找不到头文件

确保所有依赖都已正确安装：

```bash
# 检查pkg-config
pkg-config --list-all | grep -E "opus|websockets|alsa"

# 检查头文件路径
find /usr/include /usr/local/include -name "opus.h" 2>/dev/null
```

### 3. 运行时错误：找不到动态库

设置库路径：

```bash
# Linux
export LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH

# macOS
export DYLD_LIBRARY_PATH=/usr/local/lib:$DYLD_LIBRARY_PATH
```

### 4. WebSocket连接失败

检查网络连接和防火墙设置：

```bash
# 测试网络连接
curl -I https://example.com

# 检查WebSocket连接
wscat -c wss://echo.websocket.org
```

## 下一步

- 阅读 [模块使用指南](modules/) 了解各模块的详细用法
- 查看 [API参考文档](api/) 了解完整的API
- 研究 [示例代码](examples/) 学习最佳实践

## 获取帮助

如果遇到问题，可以：

1. 查看项目的Issue页面
2. 阅读相关模块的详细文档
3. 检查日志输出获取错误信息
4. 参考demo代码的实现方式