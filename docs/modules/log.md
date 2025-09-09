# 日志模块使用指南

日志模块基于spdlog库实现，提供了高性能、多线程安全的日志记录功能，支持多种日志级别、格式化输出和文件轮转，是LinxSDK中进行调试和监控的重要工具。

## 模块概述

### 核心组件

- **spdlog**: 高性能C++日志库
- **预定义宏**: 简化日志记录的宏定义
- **调试宏**: 开发阶段的详细调试信息

### 主要功能

- 多级别日志记录（TRACE、DEBUG、INFO、WARN、ERROR、CRITICAL）
- 格式化输出支持
- 多线程安全
- 异步日志记录
- 文件轮转和大小限制
- 控制台和文件同时输出
- 条件编译的调试日志

## API参考

### 日志级别宏

```cpp
// 基本日志宏
TRACE(format, ...)     // 跟踪级别，最详细的日志
INFO(format, ...)      // 信息级别，一般信息
WARN(format, ...)      // 警告级别，潜在问题
ERROR(format, ...)     // 错误级别，错误信息
CRITICAL(format, ...)  // 严重级别，严重错误

// 调试宏（仅在DEBUG模式下有效）
DEBUG(format, ...)     // 调试信息，包含函数名、参数、文件和行号
```

### 调试宏详细信息

```cpp
#ifdef DEBUG
#define DEBUG(format, ...) \
    spdlog::debug("[{}:{}:{}] " format, __FUNCTION__, __FILE__, __LINE__, ##__VA_ARGS__)
#else
#define DEBUG(format, ...)
#endif
```

### spdlog核心功能

```cpp
// 设置日志级别
spdlog::set_level(spdlog::level::debug);

// 设置日志格式
spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");

// 创建文件日志器
auto file_logger = spdlog::basic_logger_mt("file_logger", "logs/app.log");

// 创建轮转文件日志器
auto rotating_logger = spdlog::rotating_logger_mt("rotating_logger", 
                                                "logs/app.log", 1024*1024*5, 3);

// 创建异步日志器
spdlog::init_thread_pool(8192, 1);
auto async_logger = spdlog::basic_logger_mt<spdlog::async_factory>("async_logger", "logs/async.log");
```

## 使用示例

### 基本日志记录

```cpp
#include "Log.h"
#include <string>

using namespace linx;

int main() {
    // 1. 基本日志记录
    TRACE("这是跟踪信息");
    INFO("应用程序启动");
    WARN("这是一个警告");
    ERROR("发生了错误");
    CRITICAL("严重错误，程序可能崩溃");
    
    // 2. 格式化日志
    std::string user_name = "张三";
    int user_id = 12345;
    double balance = 1234.56;
    
    INFO("用户登录: 用户名={}, ID={}, 余额={:.2f}", user_name, user_id, balance);
    
    // 3. 调试日志（仅在DEBUG模式下输出）
    DEBUG("调试信息: 变量值={}", user_id);
    
    // 4. 条件日志
    bool is_error = true;
    if (is_error) {
        ERROR("条件错误: 状态={}", is_error);
    }
    
    // 5. 循环中的日志
    for (int i = 0; i < 5; i++) {
        TRACE("循环迭代: {}", i);
    }
    
    return 0;
}
```

### 高级日志配置

```cpp
#include "Log.h"
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/async.h>
#include <memory>

using namespace linx;

class LogManager {
private:
    std::shared_ptr<spdlog::logger> console_logger;
    std::shared_ptr<spdlog::logger> file_logger;
    std::shared_ptr<spdlog::logger> error_logger;
    
public:
    LogManager() {
        setupLoggers();
    }
    
    void setupLoggers() {
        try {
            // 1. 创建控制台日志器（带颜色）
            auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
            console_sink->set_level(spdlog::level::info);
            console_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v");
            
            // 2. 创建文件日志器（轮转）
            auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
                "logs/app.log", 1024*1024*10, 5); // 10MB，保留5个文件
            file_sink->set_level(spdlog::level::trace);
            file_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] [%t] %v");
            
            // 3. 创建错误日志器
            auto error_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("logs/error.log");
            error_sink->set_level(spdlog::level::err);
            error_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] [%s:%#] %v");
            
            // 4. 组合日志器（同时输出到控制台和文件）
            std::vector<spdlog::sink_ptr> sinks {console_sink, file_sink};
            auto combined_logger = std::make_shared<spdlog::logger>("combined", sinks.begin(), sinks.end());
            combined_logger->set_level(spdlog::level::trace);
            
            // 5. 设置为默认日志器
            spdlog::set_default_logger(combined_logger);
            
            // 6. 创建专用错误日志器
            error_logger = std::make_shared<spdlog::logger>("error", error_sink);
            error_logger->set_level(spdlog::level::err);
            
            // 7. 注册日志器
            spdlog::register_logger(combined_logger);
            spdlog::register_logger(error_logger);
            
            // 8. 设置刷新策略
            spdlog::flush_every(std::chrono::seconds(3));
            spdlog::flush_on(spdlog::level::err);
            
            INFO("日志系统初始化完成");
            
        } catch (const spdlog::spdlog_ex& ex) {
            std::cerr << "日志初始化失败: " << ex.what() << std::endl;
        }
    }
    
    void logError(const std::string& message) {
        if (error_logger) {
            error_logger->error(message);
        }
    }
    
    void setLogLevel(spdlog::level::level_enum level) {
        spdlog::set_level(level);
        INFO("日志级别设置为: {}", spdlog::level::to_string_view(level));
    }
    
    void enableAsyncLogging() {
        try {
            // 初始化异步日志线程池
            spdlog::init_thread_pool(8192, 1);
            
            // 创建异步文件日志器
            auto async_file = spdlog::basic_logger_mt<spdlog::async_factory>(
                "async_file", "logs/async.log");
            async_file->set_level(spdlog::level::trace);
            
            spdlog::set_default_logger(async_file);
            
            INFO("异步日志已启用");
            
        } catch (const spdlog::spdlog_ex& ex) {
            ERROR("异步日志启用失败: {}", ex.what());
        }
    }
    
    ~LogManager() {
        // 确保所有日志都被写入
        spdlog::shutdown();
    }
};

int main() {
    LogManager log_manager;
    
    // 设置日志级别
    log_manager.setLogLevel(spdlog::level::debug);
    
    // 测试不同级别的日志
    TRACE("跟踪信息: 程序开始执行");
    DEBUG("调试信息: 变量初始化完成");
    INFO("信息: 用户操作记录");
    WARN("警告: 内存使用率较高");
    ERROR("错误: 网络连接失败");
    CRITICAL("严重: 数据库连接丢失");
    
    // 专用错误日志
    log_manager.logError("这是一个专用错误日志");
    
    // 启用异步日志
    log_manager.enableAsyncLogging();
    
    // 测试异步日志性能
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 10000; i++) {
        INFO("异步日志测试: {}", i);
    }
    auto end = std::chrono::high_resolution_clock::now();
    
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    INFO("10000条日志耗时: {}ms", duration.count());
    
    return 0;
}
```

### 应用程序日志集成

```cpp
#include "Log.h"
#include "AudioInterface.h"
#include "Websocket.h"
#include "HttpClient.h"
#include <thread>
#include <chrono>

using namespace linx;

class AudioApplication {
private:
    std::unique_ptr<AudioInterface> audio;
    std::unique_ptr<WebSocketClient> ws_client;
    std::unique_ptr<HttpClient> http_client;
    
    std::atomic<bool> running{true};
    std::string session_id;
    
public:
    AudioApplication() {
        setupLogging();
        initializeComponents();
    }
    
    void setupLogging() {
        // 设置日志格式
        spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%t] %v");
        
        // 设置日志级别
        spdlog::set_level(spdlog::level::info);
        
        // 在调试模式下启用更详细的日志
#ifdef DEBUG
        spdlog::set_level(spdlog::level::trace);
        INFO("调试模式已启用，日志级别设置为TRACE");
#endif
        
        INFO("音频应用程序启动");
    }
    
    void initializeComponents() {
        try {
            // 1. 初始化音频接口
            INFO("初始化音频接口...");
            audio = CreateAudioInterface();
            if (!audio) {
                CRITICAL("音频接口创建失败");
                throw std::runtime_error("音频接口初始化失败");
            }
            
            audio->Init();
            audio->SetConfig(16000, 320, 1, 4, 4096, 1024);
            INFO("音频接口初始化成功: 采样率=16000Hz, 帧大小=320, 声道=1");
            
            // 2. 初始化WebSocket客户端
            INFO("初始化WebSocket客户端...");
            ws_client = std::make_unique<WebSocketClient>("wss://api.example.com/ws");
            setupWebSocketCallbacks();
            INFO("WebSocket客户端初始化成功");
            
            // 3. 初始化HTTP客户端
            INFO("初始化HTTP客户端...");
            http_client = std::make_unique<HttpClient>();
            http_client->SetTimeout(30);
            INFO("HTTP客户端初始化成功");
            
        } catch (const std::exception& e) {
            CRITICAL("组件初始化失败: {}", e.what());
            throw;
        }
    }
    
    void setupWebSocketCallbacks() {
        // 连接成功回调
        ws_client->SetOnOpenCallback([this]() -> std::string {
            INFO("WebSocket连接已建立");
            
            json hello_msg = {
                {"type", "hello"},
                {"version", 1},
                {"timestamp", std::time(nullptr)}
            };
            
            INFO("发送hello消息: {}", hello_msg.dump());
            return hello_msg.dump();
        });
        
        // 消息接收回调
        ws_client->SetOnMessageCallback([this](const std::string& msg, bool binary) -> std::string {
            if (binary) {
                TRACE("收到二进制数据: {} 字节", msg.size());
                return handleBinaryMessage(msg);
            } else {
                TRACE("收到文本消息: {}", msg);
                return handleTextMessage(msg);
            }
        });
        
        // 连接关闭回调
        ws_client->SetOnCloseCallback([this]() {
            WARN("WebSocket连接已关闭");
            session_id.clear();
        });
        
        // 连接失败回调
        ws_client->SetOnFailCallback([this]() {
            ERROR("WebSocket连接失败");
        });
    }
    
    std::string handleTextMessage(const std::string& msg) {
        try {
            json received = json::parse(msg);
            std::string type = received["type"];
            
            INFO("处理消息类型: {}", type);
            
            if (type == "hello") {
                session_id = received["session_id"];
                INFO("获得会话ID: {}", session_id);
                
                // 开始音频录制
                startAudioRecording();
                
            } else if (type == "tts") {
                std::string state = received["state"];
                INFO("TTS状态变化: {}", state);
                
                if (state == "start") {
                    stopAudioRecording();
                } else if (state == "stop") {
                    startAudioRecording();
                }
                
            } else if (type == "error") {
                std::string error_msg = received["message"];
                ERROR("服务器错误: {}", error_msg);
            }
            
        } catch (const json_exception& e) {
            ERROR("JSON解析失败: {}, 原始消息: {}", e.what(), msg);
        }
        
        return "";
    }
    
    std::string handleBinaryMessage(const std::string& data) {
        // 处理音频数据
        DEBUG("处理音频数据: {} 字节", data.size());
        
        // 这里可以解码并播放音频
        // ...
        
        return "";
    }
    
    void startAudioRecording() {
        try {
            audio->Record();
            INFO("音频录制已开始");
        } catch (const std::exception& e) {
            ERROR("启动音频录制失败: {}", e.what());
        }
    }
    
    void stopAudioRecording() {
        try {
            // 停止录制的具体实现
            INFO("音频录制已停止");
        } catch (const std::exception& e) {
            ERROR("停止音频录制失败: {}", e.what());
        }
    }
    
    void run() {
        try {
            INFO("启动应用程序主循环");
            
            // 启动WebSocket连接
            ws_client->start();
            
            // 主循环
            while (running) {
                // 处理音频数据
                processAudioData();
                
                // 发送心跳
                sendHeartbeat();
                
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            
        } catch (const std::exception& e) {
            CRITICAL("应用程序运行时错误: {}", e.what());
            throw;
        }
    }
    
    void processAudioData() {
        try {
            short buffer[320];
            if (audio->Read(buffer, 320)) {
                TRACE("读取音频数据: 320 样本");
                
                // 发送音频数据到服务器
                ws_client->send_binary(buffer, sizeof(buffer));
            }
        } catch (const std::exception& e) {
            ERROR("音频数据处理失败: {}", e.what());
        }
    }
    
    void sendHeartbeat() {
        static auto last_heartbeat = std::chrono::steady_clock::now();
        auto now = std::chrono::steady_clock::now();
        
        if (now - last_heartbeat >= std::chrono::seconds(30)) {
            try {
                json heartbeat = {
                    {"type", "heartbeat"},
                    {"session_id", session_id},
                    {"timestamp", std::time(nullptr)}
                };
                
                ws_client->send_text(heartbeat.dump());
                TRACE("发送心跳: {}", session_id);
                
                last_heartbeat = now;
            } catch (const std::exception& e) {
                WARN("发送心跳失败: {}", e.what());
            }
        }
    }
    
    void stop() {
        INFO("停止应用程序...");
        running = false;
    }
    
    ~AudioApplication() {
        INFO("音频应用程序关闭");
    }
};

int main() {
    try {
        AudioApplication app;
        
        // 设置信号处理
        std::signal(SIGINT, [](int) {
            INFO("收到中断信号，准备退出...");
            // 这里应该设置全局退出标志
        });
        
        app.run();
        
    } catch (const std::exception& e) {
        CRITICAL("应用程序异常退出: {}", e.what());
        return -1;
    }
    
    INFO("应用程序正常退出");
    return 0;
}
```

### 性能监控日志

```cpp
#include "Log.h"
#include <chrono>
#include <map>
#include <mutex>

using namespace linx;

class PerformanceLogger {
private:
    struct PerformanceMetrics {
        std::chrono::high_resolution_clock::time_point start_time;
        std::chrono::milliseconds total_duration{0};
        size_t call_count = 0;
        std::chrono::milliseconds min_duration{std::chrono::milliseconds::max()};
        std::chrono::milliseconds max_duration{0};
        
        void addSample(std::chrono::milliseconds duration) {
            total_duration += duration;
            call_count++;
            min_duration = std::min(min_duration, duration);
            max_duration = std::max(max_duration, duration);
        }
        
        double getAverageDuration() const {
            return call_count > 0 ? (double)total_duration.count() / call_count : 0.0;
        }
    };
    
    std::map<std::string, PerformanceMetrics> metrics;
    std::mutex metrics_mutex;
    
public:
    class Timer {
    private:
        PerformanceLogger& logger;
        std::string name;
        std::chrono::high_resolution_clock::time_point start_time;
        
    public:
        Timer(PerformanceLogger& l, const std::string& n) 
            : logger(l), name(n), start_time(std::chrono::high_resolution_clock::now()) {
            TRACE("性能计时开始: {}", name);
        }
        
        ~Timer() {
            auto end_time = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
            
            logger.recordDuration(name, duration);
            TRACE("性能计时结束: {}, 耗时: {}ms", name, duration.count());
        }
    };
    
    void recordDuration(const std::string& name, std::chrono::milliseconds duration) {
        std::lock_guard<std::mutex> lock(metrics_mutex);
        metrics[name].addSample(duration);
        
        // 如果耗时过长，记录警告
        if (duration > std::chrono::milliseconds(1000)) {
            WARN("操作耗时过长: {} = {}ms", name, duration.count());
        }
    }
    
    Timer createTimer(const std::string& name) {
        return Timer(*this, name);
    }
    
    void printReport() {
        std::lock_guard<std::mutex> lock(metrics_mutex);
        
        INFO("=== 性能报告 ===");
        for (const auto& [name, metric] : metrics) {
            INFO("操作: {}", name);
            INFO("  调用次数: {}", metric.call_count);
            INFO("  总耗时: {}ms", metric.total_duration.count());
            INFO("  平均耗时: {:.2f}ms", metric.getAverageDuration());
            INFO("  最小耗时: {}ms", metric.min_duration.count());
            INFO("  最大耗时: {}ms", metric.max_duration.count());
        }
    }
    
    void reset() {
        std::lock_guard<std::mutex> lock(metrics_mutex);
        metrics.clear();
        INFO("性能指标已重置");
    }
};

// 性能计时宏
#define PERF_TIMER(logger, name) auto timer = logger.createTimer(name)

// 使用示例
class AudioProcessor {
private:
    PerformanceLogger perf_logger;
    
public:
    void processAudioFrame(const short* data, size_t samples) {
        PERF_TIMER(perf_logger, "processAudioFrame");
        
        // 模拟音频处理
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        
        INFO("处理音频帧: {} 样本", samples);
    }
    
    void encodeAudio(const short* data, size_t samples) {
        PERF_TIMER(perf_logger, "encodeAudio");
        
        // 模拟音频编码
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        
        INFO("编码音频: {} 样本", samples);
    }
    
    void sendToServer(const void* data, size_t size) {
        PERF_TIMER(perf_logger, "sendToServer");
        
        // 模拟网络发送
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        
        INFO("发送数据: {} 字节", size);
    }
    
    void printPerformanceReport() {
        perf_logger.printReport();
    }
};

int main() {
    AudioProcessor processor;
    
    // 模拟音频处理循环
    for (int i = 0; i < 100; i++) {
        short audio_data[320];
        
        processor.processAudioFrame(audio_data, 320);
        processor.encodeAudio(audio_data, 320);
        processor.sendToServer(audio_data, sizeof(audio_data));
        
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    
    // 打印性能报告
    processor.printPerformanceReport();
    
    return 0;
}
```

### 结构化日志

```cpp
#include "Log.h"
#include "Json.h"
#include <spdlog/fmt/fmt.h>

using namespace linx;

class StructuredLogger {
public:
    // 用户操作日志
    static void logUserAction(const std::string& user_id, const std::string& action, 
                             const json& details = json::object()) {
        json log_entry = {
            {"type", "user_action"},
            {"timestamp", std::time(nullptr)},
            {"user_id", user_id},
            {"action", action},
            {"details", details}
        };
        
        INFO("USER_ACTION: {}", log_entry.dump());
    }
    
    // 系统事件日志
    static void logSystemEvent(const std::string& event_type, const std::string& message,
                              const json& context = json::object()) {
        json log_entry = {
            {"type", "system_event"},
            {"timestamp", std::time(nullptr)},
            {"event_type", event_type},
            {"message", message},
            {"context", context}
        };
        
        INFO("SYSTEM_EVENT: {}", log_entry.dump());
    }
    
    // 性能指标日志
    static void logPerformanceMetric(const std::string& metric_name, double value,
                                   const std::string& unit = "") {
        json log_entry = {
            {"type", "performance_metric"},
            {"timestamp", std::time(nullptr)},
            {"metric_name", metric_name},
            {"value", value},
            {"unit", unit}
        };
        
        INFO("PERFORMANCE: {}", log_entry.dump());
    }
    
    // 错误日志
    static void logError(const std::string& error_code, const std::string& error_message,
                        const json& error_context = json::object()) {
        json log_entry = {
            {"type", "error"},
            {"timestamp", std::time(nullptr)},
            {"error_code", error_code},
            {"error_message", error_message},
            {"context", error_context}
        };
        
        ERROR("ERROR: {}", log_entry.dump());
    }
    
    // 音频会话日志
    static void logAudioSession(const std::string& session_id, const std::string& event,
                               const json& session_data = json::object()) {
        json log_entry = {
            {"type", "audio_session"},
            {"timestamp", std::time(nullptr)},
            {"session_id", session_id},
            {"event", event},
            {"data", session_data}
        };
        
        INFO("AUDIO_SESSION: {}", log_entry.dump());
    }
};

// 使用示例
int main() {
    // 用户操作日志
    json user_details = {
        {"ip_address", "192.168.1.100"},
        {"user_agent", "LinxSDK/1.0"},
        {"session_duration", 1800}
    };
    StructuredLogger::logUserAction("user123", "login", user_details);
    
    // 系统事件日志
    json system_context = {
        {"memory_usage", "75%"},
        {"cpu_usage", "45%"},
        {"active_connections", 150}
    };
    StructuredLogger::logSystemEvent("high_load", "系统负载较高", system_context);
    
    // 性能指标日志
    StructuredLogger::logPerformanceMetric("audio_latency", 45.2, "ms");
    StructuredLogger::logPerformanceMetric("throughput", 1024.5, "KB/s");
    
    // 错误日志
    json error_context = {
        {"function", "connectToServer"},
        {"file", "network.cpp"},
        {"line", 123},
        {"retry_count", 3}
    };
    StructuredLogger::logError("NETWORK_001", "连接服务器失败", error_context);
    
    // 音频会话日志
    json session_data = {
        {"sample_rate", 16000},
        {"channels", 1},
        {"codec", "opus"},
        {"duration", 120}
    };
    StructuredLogger::logAudioSession("session_456", "started", session_data);
    StructuredLogger::logAudioSession("session_456", "ended", session_data);
    
    return 0;
}
```

## 配置和优化

### 日志配置文件

```cpp
#include "Log.h"
#include "Json.h"
#include <fstream>

class LogConfig {
private:
    json config;
    
public:
    bool loadFromFile(const std::string& config_file) {
        try {
            std::ifstream file(config_file);
            if (!file.is_open()) {
                ERROR("无法打开日志配置文件: {}", config_file);
                return false;
            }
            
            file >> config;
            INFO("日志配置加载成功: {}", config_file);
            return true;
            
        } catch (const std::exception& e) {
            ERROR("加载日志配置失败: {}", e.what());
            return false;
        }
    }
    
    void applyConfig() {
        try {
            // 设置日志级别
            if (config.contains("level")) {
                std::string level_str = config["level"];
                auto level = spdlog::level::from_str(level_str);
                spdlog::set_level(level);
                INFO("日志级别设置为: {}", level_str);
            }
            
            // 设置日志格式
            if (config.contains("pattern")) {
                std::string pattern = config["pattern"];
                spdlog::set_pattern(pattern);
                INFO("日志格式设置为: {}", pattern);
            }
            
            // 设置刷新策略
            if (config.contains("flush_every_seconds")) {
                int seconds = config["flush_every_seconds"];
                spdlog::flush_every(std::chrono::seconds(seconds));
                INFO("日志刷新间隔设置为: {}秒", seconds);
            }
            
            // 设置异步日志
            if (config.contains("async") && config["async"].get<bool>()) {
                int queue_size = config.value("async_queue_size", 8192);
                int thread_count = config.value("async_thread_count", 1);
                
                spdlog::init_thread_pool(queue_size, thread_count);
                INFO("异步日志已启用: 队列大小={}, 线程数={}", queue_size, thread_count);
            }
            
        } catch (const std::exception& e) {
            ERROR("应用日志配置失败: {}", e.what());
        }
    }
    
    // 示例配置文件内容
    void createSampleConfig(const std::string& config_file) {
        json sample_config = {
            {"level", "info"},
            {"pattern", "[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%t] %v"},
            {"flush_every_seconds", 3},
            {"async", true},
            {"async_queue_size", 8192},
            {"async_thread_count", 1},
            {"files", {
                {"app_log", {
                    {"path", "logs/app.log"},
                    {"max_size", "10MB"},
                    {"max_files", 5}
                }},
                {"error_log", {
                    {"path", "logs/error.log"},
                    {"level", "error"}
                }}
            }}
        };
        
        std::ofstream file(config_file);
        file << sample_config.dump(2);
        
        INFO("示例配置文件已创建: {}", config_file);
    }
};
```

## 最佳实践

1. **选择合适的日志级别**：
   - TRACE: 最详细的调试信息
   - DEBUG: 开发阶段的调试信息
   - INFO: 一般信息和重要事件
   - WARN: 警告信息
   - ERROR: 错误信息
   - CRITICAL: 严重错误

2. **使用结构化日志**：便于日志分析和监控

3. **性能考虑**：
   - 使用异步日志提高性能
   - 避免在热点路径中使用过多日志
   - 使用条件编译控制调试日志

4. **日志轮转**：防止日志文件过大

5. **错误处理**：确保日志记录不会影响主程序逻辑

6. **安全性**：避免在日志中记录敏感信息

7. **监控和告警**：基于日志建立监控和告警机制

8. **日志分析**：使用工具分析日志，发现问题和优化点