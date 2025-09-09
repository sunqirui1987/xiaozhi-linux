# LinxSDK API 参考文档

本文档提供了LinxSDK所有公共API的详细参考信息，包括类、方法、参数和返回值的完整说明。

## 目录

- [音频接口 (AudioInterface)](#音频接口-audiointerface)
- [WebSocket客户端 (WebSocketClient)](#websocket客户端-websocketclient)
- [HTTP客户端 (HttpClient)](#http客户端-httpclient)
- [Opus编解码 (OpusAudio)](#opus编解码-opusaudio)
- [文件流 (FileStream)](#文件流-filestream)
- [日志系统 (Log)](#日志系统-log)
- [JSON处理 (Json)](#json处理-json)
- [数据类型和常量](#数据类型和常量)
- [错误码](#错误码)

---

## 音频接口 (AudioInterface)

### 概述

`AudioInterface` 是音频操作的抽象基类，提供了音频录制、播放和配置的统一接口。

### 头文件

```cpp
#include "AudioInterface.h"
```

### 工厂函数

#### CreateAudioInterface

```cpp
std::unique_ptr<AudioInterface> CreateAudioInterface();
```

**描述**: 创建音频接口实例

**返回值**: 
- `std::unique_ptr<AudioInterface>`: 音频接口智能指针，失败时返回nullptr

**示例**:
```cpp
auto audio = CreateAudioInterface();
if (!audio) {
    // 处理创建失败
}
```

### 类方法

#### Init

```cpp
virtual bool Init() = 0;
```

**描述**: 初始化音频接口

**返回值**: 
- `bool`: 成功返回true，失败返回false

**示例**:
```cpp
if (!audio->Init()) {
    std::cerr << "音频接口初始化失败" << std::endl;
}
```

#### SetConfig

```cpp
virtual bool SetConfig(int sample_rate, int frame_size, int channels, 
                      int input_buffer_count, int input_buffer_size,
                      int output_buffer_size) = 0;
```

**描述**: 设置音频配置参数

**参数**:
- `sample_rate`: 采样率 (Hz)，常用值：8000, 16000, 44100, 48000
- `frame_size`: 每帧样本数，通常为采样率的1/50 (20ms)
- `channels`: 声道数，1为单声道，2为立体声
- `input_buffer_count`: 输入缓冲区数量
- `input_buffer_size`: 输入缓冲区大小 (字节)
- `output_buffer_size`: 输出缓冲区大小 (字节)

**返回值**: 
- `bool`: 配置成功返回true，失败返回false

**示例**:
```cpp
// 配置16kHz单声道音频，20ms帧长
bool success = audio->SetConfig(
    16000,  // 采样率
    320,    // 帧大小 (16000 * 0.02)
    1,      // 单声道
    4,      // 4个输入缓冲区
    4096,   // 输入缓冲区4KB
    1024    // 输出缓冲区1KB
);
```

#### Record

```cpp
virtual bool Record() = 0;
```

**描述**: 开始音频录制

**返回值**: 
- `bool`: 成功开始录制返回true，失败返回false

**注意**: 调用此方法前必须先调用 `Init()` 和 `SetConfig()`

#### Play

```cpp
virtual bool Play() = 0;
```

**描述**: 开始音频播放

**返回值**: 
- `bool`: 成功开始播放返回true，失败返回false

#### Read

```cpp
virtual bool Read(short* data, int samples) = 0;
```

**描述**: 读取录制的音频数据

**参数**:
- `data`: 输出缓冲区，存储读取的音频样本
- `samples`: 要读取的样本数

**返回值**: 
- `bool`: 成功读取返回true，无数据或失败返回false

**示例**:
```cpp
short buffer[320];  // 20ms @ 16kHz
if (audio->Read(buffer, 320)) {
    // 处理音频数据
    processAudioData(buffer, 320);
}
```

#### Write

```cpp
virtual bool Write(const short* data, int samples) = 0;
```

**描述**: 写入音频数据进行播放

**参数**:
- `data`: 音频样本数据
- `samples`: 样本数量

**返回值**: 
- `bool`: 成功写入返回true，失败返回false

**示例**:
```cpp
short playback_data[320];
// ... 填充播放数据 ...
if (!audio->Write(playback_data, 320)) {
    std::cerr << "音频写入失败" << std::endl;
}
```

---

## WebSocket客户端 (WebSocketClient)

### 概述

`WebSocketClient` 提供了WebSocket客户端功能，支持文本和二进制消息的发送接收。

### 头文件

```cpp
#include "Websocket.h"
```

### 构造函数

```cpp
WebSocketClient(const std::string& url);
```

**参数**:
- `url`: WebSocket服务器URL，支持ws://和wss://协议

**示例**:
```cpp
WebSocketClient client("wss://api.example.com/ws");
```

### 回调函数类型

#### OnOpenCallback

```cpp
using OnOpenCallback = std::function<std::string()>;
```

**描述**: 连接建立时的回调函数

**返回值**: 连接建立后要发送的初始消息（可选）

#### OnMessageCallback

```cpp
using OnMessageCallback = std::function<std::string(const std::string&, bool)>;
```

**描述**: 接收到消息时的回调函数

**参数**:
- `const std::string&`: 接收到的消息内容
- `bool`: 是否为二进制消息（true为二进制，false为文本）

**返回值**: 响应消息（可选）

#### OnCloseCallback

```cpp
using OnCloseCallback = std::function<void()>;
```

**描述**: 连接关闭时的回调函数

#### OnFailCallback

```cpp
using OnFailCallback = std::function<void()>;
```

**描述**: 连接失败时的回调函数

### 方法

#### SetOnOpenCallback

```cpp
void SetOnOpenCallback(OnOpenCallback callback);
```

**描述**: 设置连接建立回调

**参数**:
- `callback`: 回调函数

**示例**:
```cpp
client.SetOnOpenCallback([]() -> std::string {
    json hello = {
        {"type", "hello"},
        {"version", 1}
    };
    return hello.dump();
});
```

#### SetOnMessageCallback

```cpp
void SetOnMessageCallback(OnMessageCallback callback);
```

**描述**: 设置消息接收回调

**参数**:
- `callback`: 回调函数

**示例**:
```cpp
client.SetOnMessageCallback([](const std::string& msg, bool binary) -> std::string {
    if (binary) {
        // 处理二进制数据
        processAudioData(msg);
    } else {
        // 处理文本消息
        json received = json::parse(msg);
        processTextMessage(received);
    }
    return ""; // 无响应消息
});
```

#### SetOnCloseCallback

```cpp
void SetOnCloseCallback(OnCloseCallback callback);
```

**描述**: 设置连接关闭回调

#### SetOnFailCallback

```cpp
void SetOnFailCallback(OnFailCallback callback);
```

**描述**: 设置连接失败回调

#### start

```cpp
void start();
```

**描述**: 启动WebSocket客户端，开始连接

**注意**: 此方法是异步的，连接结果通过回调函数通知

#### send_text

```cpp
void send_text(const std::string& message);
```

**描述**: 发送文本消息

**参数**:
- `message`: 要发送的文本消息

**示例**:
```cpp
json message = {
    {"type", "chat"},
    {"content", "Hello, server!"}
};
client.send_text(message.dump());
```

#### send_binary

```cpp
void send_binary(const void* data, size_t length);
```

**描述**: 发送二进制数据

**参数**:
- `data`: 二进制数据指针
- `length`: 数据长度（字节）

**示例**:
```cpp
short audio_data[320];
// ... 填充音频数据 ...
client.send_binary(audio_data, sizeof(audio_data));
```

---

## HTTP客户端 (HttpClient)

### 概述

`HttpClient` 提供了HTTP客户端功能，支持POST请求、文件上传等操作。

### 头文件

```cpp
#include "HttpClient.h"
```

### 回调函数类型

#### ResponseCallback

```cpp
using ResponseCallback = std::function<void(int, const std::string&)>;
```

**描述**: HTTP响应回调函数

**参数**:
- `int`: HTTP状态码（如200, 404, 500等）
- `const std::string&`: 响应内容

### 构造函数

```cpp
HttpClient();
```

### 方法

#### SetTimeout

```cpp
void SetTimeout(int timeout_seconds);
```

**描述**: 设置请求超时时间

**参数**:
- `timeout_seconds`: 超时时间（秒）

**示例**:
```cpp
HttpClient client;
client.SetTimeout(30); // 30秒超时
```

#### PostJson

```cpp
void PostJson(const std::string& url, const std::string& json_data, 
              ResponseCallback callback);
```

**描述**: 发送JSON POST请求

**参数**:
- `url`: 请求URL
- `json_data`: JSON数据字符串
- `callback`: 响应回调函数

**示例**:
```cpp
json request_data = {
    {"username", "user123"},
    {"message", "Hello, API!"}
};

client.PostJson("https://api.example.com/messages", 
               request_data.dump(),
               [](int status, const std::string& response) {
                   if (status == 200) {
                       std::cout << "请求成功: " << response << std::endl;
                   } else {
                       std::cerr << "请求失败: " << status << std::endl;
                   }
               });
```

#### UploadFile

```cpp
void UploadFile(const std::string& url, const std::string& file_path,
                const std::string& field_name, ResponseCallback callback);
```

**描述**: 上传文件

**参数**:
- `url`: 上传URL
- `file_path`: 本地文件路径
- `field_name`: 表单字段名
- `callback`: 响应回调函数

**示例**:
```cpp
client.UploadFile("https://api.example.com/upload",
                 "./audio.wav",
                 "audio_file",
                 [](int status, const std::string& response) {
                     if (status == 200) {
                         json result = json::parse(response);
                         std::cout << "文件上传成功: " << result["file_id"] << std::endl;
                     }
                 });
```

---

## Opus编解码 (OpusAudio)

### 概述

`OpusAudio` 提供了Opus音频编解码功能，支持高质量的音频压缩。

### 头文件

```cpp
#include "Opus.h"
```

### 方法

#### CreateEncoder

```cpp
static OpusEncoder* CreateEncoder(int sample_rate, int channels, int application);
```

**描述**: 创建Opus编码器

**参数**:
- `sample_rate`: 采样率 (8000, 12000, 16000, 24000, 48000)
- `channels`: 声道数 (1或2)
- `application`: 应用类型
  - `OPUS_APPLICATION_VOIP`: VoIP应用
  - `OPUS_APPLICATION_AUDIO`: 音频应用
  - `OPUS_APPLICATION_RESTRICTED_LOWDELAY`: 低延迟应用

**返回值**: 
- `OpusEncoder*`: 编码器指针，失败时返回nullptr

**示例**:
```cpp
OpusEncoder* encoder = OpusAudio::CreateEncoder(16000, 1, OPUS_APPLICATION_VOIP);
if (!encoder) {
    std::cerr << "创建Opus编码器失败" << std::endl;
}
```

#### CreateDecoder

```cpp
static OpusDecoder* CreateDecoder(int sample_rate, int channels);
```

**描述**: 创建Opus解码器

**参数**:
- `sample_rate`: 采样率
- `channels`: 声道数

**返回值**: 
- `OpusDecoder*`: 解码器指针，失败时返回nullptr

#### DestroyEncoder

```cpp
static void DestroyEncoder(OpusEncoder* encoder);
```

**描述**: 销毁编码器

**参数**:
- `encoder`: 要销毁的编码器指针

#### DestroyDecoder

```cpp
static void DestroyDecoder(OpusDecoder* decoder);
```

**描述**: 销毁解码器

**参数**:
- `decoder`: 要销毁的解码器指针

#### Encode

```cpp
static int Encode(OpusEncoder* encoder, const short* pcm, int frame_size,
                 unsigned char* data, int max_data_bytes);
```

**描述**: 编码PCM音频数据

**参数**:
- `encoder`: 编码器指针
- `pcm`: PCM音频数据
- `frame_size`: 帧大小（样本数）
- `data`: 输出缓冲区
- `max_data_bytes`: 输出缓冲区最大字节数

**返回值**: 
- `int`: 编码后的字节数，负数表示错误

**示例**:
```cpp
short pcm_data[320];  // 20ms @ 16kHz
unsigned char opus_data[1024];

int encoded_bytes = OpusAudio::Encode(encoder, pcm_data, 320, 
                                     opus_data, sizeof(opus_data));
if (encoded_bytes > 0) {
    // 发送编码后的数据
    sendOpusData(opus_data, encoded_bytes);
} else {
    std::cerr << "Opus编码失败: " << encoded_bytes << std::endl;
}
```

#### Decode

```cpp
static int Decode(OpusDecoder* decoder, const unsigned char* data, int len,
                 short* pcm, int frame_size, int decode_fec);
```

**描述**: 解码Opus数据

**参数**:
- `decoder`: 解码器指针
- `data`: Opus编码数据
- `len`: 数据长度
- `pcm`: 输出PCM缓冲区
- `frame_size`: 期望的帧大小
- `decode_fec`: 是否使用前向纠错 (0或1)

**返回值**: 
- `int`: 解码的样本数，负数表示错误

**示例**:
```cpp
unsigned char opus_data[256];
int opus_len = receiveOpusData(opus_data, sizeof(opus_data));

short pcm_output[320];
int decoded_samples = OpusAudio::Decode(decoder, opus_data, opus_len,
                                       pcm_output, 320, 0);
if (decoded_samples > 0) {
    // 播放解码后的PCM数据
    playPcmData(pcm_output, decoded_samples);
}
```

#### CheckError

```cpp
static std::string CheckError(int error_code);
```

**描述**: 检查Opus错误码并返回错误描述

**参数**:
- `error_code`: Opus错误码

**返回值**: 
- `std::string`: 错误描述字符串

**示例**:
```cpp
int result = OpusAudio::Encode(encoder, pcm_data, 320, opus_data, sizeof(opus_data));
if (result < 0) {
    std::string error_msg = OpusAudio::CheckError(result);
    std::cerr << "Opus编码错误: " << error_msg << std::endl;
}
```

---

## 文件流 (FileStream)

### 概述

`FileStream` 提供了文件操作和音频文件处理功能。

### 头文件

```cpp
#include "FileStream.h"
```

### 数据结构

#### WAVE_HEADER

```cpp
struct WAVE_HEADER {
    char riff[4];           // "RIFF"
    uint32_t file_size;     // 文件大小 - 8
    char wave[4];           // "WAVE"
};
```

#### WAVE_FMT

```cpp
struct WAVE_FMT {
    char fmt[4];            // "fmt "
    uint32_t fmt_size;      // 格式块大小
    uint16_t audio_format;  // 音频格式 (1 = PCM)
    uint16_t channels;      // 声道数
    uint32_t sample_rate;   // 采样率
    uint32_t byte_rate;     // 字节率
    uint16_t block_align;   // 块对齐
    uint16_t bits_per_sample; // 每样本位数
};
```

#### WAVE_DATA

```cpp
struct WAVE_DATA {
    char data[4];           // "data"
    uint32_t data_size;     // 数据大小
};
```

### 构造函数

```cpp
FileStream();
```

### 方法

#### fopen

```cpp
FILE* fopen(const char* filename, const char* mode);
```

**描述**: 打开文件

**参数**:
- `filename`: 文件名
- `mode`: 打开模式 ("r", "w", "rb", "wb"等)

**返回值**: 
- `FILE*`: 文件指针，失败时返回nullptr

#### fwrite

```cpp
size_t fwrite(const void* ptr, size_t size, size_t count);
```

**描述**: 写入数据到文件

**参数**:
- `ptr`: 数据指针
- `size`: 每个元素的大小
- `count`: 元素数量

**返回值**: 
- `size_t`: 实际写入的元素数量

#### fread

```cpp
size_t fread(void* ptr, size_t size, size_t count);
```

**描述**: 从文件读取数据

**参数**:
- `ptr`: 输出缓冲区
- `size`: 每个元素的大小
- `count`: 要读取的元素数量

**返回值**: 
- `size_t`: 实际读取的元素数量

#### wavfopen

```cpp
FILE* wavfopen(const char* filename, const char* mode, 
               int sample_rate, int channels, int bits_per_sample);
```

**描述**: 创建WAV文件并写入文件头

**参数**:
- `filename`: 文件名
- `mode`: 打开模式
- `sample_rate`: 采样率
- `channels`: 声道数
- `bits_per_sample`: 每样本位数

**返回值**: 
- `FILE*`: 文件指针

#### saveWavWithOneChannel

```cpp
void saveWavWithOneChannel(const char* filename, const short* data, 
                          size_t samples, int sample_rate);
```

**描述**: 保存单声道WAV文件

**参数**:
- `filename`: 输出文件名
- `data`: 音频数据
- `samples`: 样本数量
- `sample_rate`: 采样率

### 全局函数

#### pcm2wav

```cpp
void pcm2wav(const char* pcm_file, const char* wav_file, 
             int sample_rate, int channels, int bits_per_sample);
```

**描述**: 将PCM文件转换为WAV文件

#### wav2mp3

```cpp
void wav2mp3(const char* wav_file, const char* mp3_file);
```

**描述**: 将WAV文件转换为MP3文件

---

## 日志系统 (Log)

### 概述

基于spdlog的日志系统，提供多级别日志记录功能。

### 头文件

```cpp
#include "Log.h"
```

### 日志宏

#### TRACE

```cpp
TRACE(format, ...)
```

**描述**: 记录跟踪级别日志

**参数**:
- `format`: 格式字符串
- `...`: 格式参数

#### INFO

```cpp
INFO(format, ...)
```

**描述**: 记录信息级别日志

#### WARN

```cpp
WARN(format, ...)
```

**描述**: 记录警告级别日志

#### ERROR

```cpp
ERROR(format, ...)
```

**描述**: 记录错误级别日志

#### CRITICAL

```cpp
CRITICAL(format, ...)
```

**描述**: 记录严重错误级别日志

#### DEBUG

```cpp
DEBUG(format, ...)
```

**描述**: 记录调试级别日志（仅在DEBUG模式下有效）

**示例**:
```cpp
INFO("用户登录: ID={}, 时间={}", user_id, timestamp);
ERROR("网络连接失败: 错误码={}", error_code);
DEBUG("变量值: x={}, y={}", x, y);
```

---

## JSON处理 (Json)

### 概述

基于nlohmann::json的JSON处理功能。

### 头文件

```cpp
#include "Json.h"
```

### 类型定义

#### json

```cpp
using json = nlohmann::json;
```

**描述**: JSON对象类型

#### json_exception

```cpp
using json_exception = nlohmann::json::exception;
```

**描述**: JSON异常类型

### 使用示例

```cpp
// 创建JSON对象
json obj = {
    {"name", "张三"},
    {"age", 25},
    {"skills", {"C++", "Python", "JavaScript"}}
};

// 序列化为字符串
std::string json_str = obj.dump();

// 从字符串解析
try {
    json parsed = json::parse(json_str);
    std::string name = parsed["name"];
    int age = parsed["age"];
} catch (const json_exception& e) {
    ERROR("JSON解析失败: {}", e.what());
}
```

---

## 数据类型和常量

### 音频参数常量

```cpp
// 常用采样率
const int SAMPLE_RATE_8K = 8000;
const int SAMPLE_RATE_16K = 16000;
const int SAMPLE_RATE_44K = 44100;
const int SAMPLE_RATE_48K = 48000;

// 常用帧大小 (20ms)
const int FRAME_SIZE_8K = 160;   // 8kHz * 0.02s
const int FRAME_SIZE_16K = 320;  // 16kHz * 0.02s
const int FRAME_SIZE_44K = 882;  // 44.1kHz * 0.02s
const int FRAME_SIZE_48K = 960;  // 48kHz * 0.02s

// 声道数
const int MONO = 1;
const int STEREO = 2;

// 位深度
const int BITS_PER_SAMPLE_16 = 16;
const int BITS_PER_SAMPLE_24 = 24;
const int BITS_PER_SAMPLE_32 = 32;
```

### Opus应用类型

```cpp
// Opus应用类型
const int OPUS_APPLICATION_VOIP = 2048;
const int OPUS_APPLICATION_AUDIO = 2049;
const int OPUS_APPLICATION_RESTRICTED_LOWDELAY = 2051;
```

### HTTP状态码

```cpp
// 常用HTTP状态码
const int HTTP_OK = 200;
const int HTTP_BAD_REQUEST = 400;
const int HTTP_UNAUTHORIZED = 401;
const int HTTP_FORBIDDEN = 403;
const int HTTP_NOT_FOUND = 404;
const int HTTP_INTERNAL_SERVER_ERROR = 500;
```

---

## 错误码

### 音频错误码

```cpp
enum AudioError {
    AUDIO_SUCCESS = 0,
    AUDIO_ERROR_INIT_FAILED = -1,
    AUDIO_ERROR_CONFIG_FAILED = -2,
    AUDIO_ERROR_DEVICE_NOT_FOUND = -3,
    AUDIO_ERROR_PERMISSION_DENIED = -4,
    AUDIO_ERROR_BUFFER_OVERFLOW = -5,
    AUDIO_ERROR_BUFFER_UNDERFLOW = -6
};
```

### Opus错误码

```cpp
// Opus错误码 (负数表示错误)
const int OPUS_OK = 0;
const int OPUS_BAD_ARG = -1;
const int OPUS_BUFFER_TOO_SMALL = -2;
const int OPUS_INTERNAL_ERROR = -3;
const int OPUS_INVALID_PACKET = -4;
const int OPUS_UNIMPLEMENTED = -5;
const int OPUS_INVALID_STATE = -6;
const int OPUS_ALLOC_FAIL = -7;
```

### WebSocket错误码

```cpp
enum WebSocketError {
    WS_SUCCESS = 0,
    WS_ERROR_CONNECTION_FAILED = -1,
    WS_ERROR_HANDSHAKE_FAILED = -2,
    WS_ERROR_SEND_FAILED = -3,
    WS_ERROR_RECEIVE_FAILED = -4,
    WS_ERROR_PROTOCOL_ERROR = -5
};
```

---

## 完整示例

### 音频录制和WebSocket传输

```cpp
#include "AudioInterface.h"
#include "Websocket.h"
#include "Opus.h"
#include "Log.h"
#include "Json.h"
#include <thread>
#include <atomic>

using namespace linx;

class AudioWebSocketClient {
private:
    std::unique_ptr<AudioInterface> audio;
    std::unique_ptr<WebSocketClient> ws_client;
    OpusEncoder* encoder = nullptr;
    OpusDecoder* decoder = nullptr;
    
    std::atomic<bool> running{false};
    std::thread audio_thread;
    
public:
    AudioWebSocketClient(const std::string& ws_url) {
        // 初始化音频接口
        audio = CreateAudioInterface();
        if (!audio || !audio->Init()) {
            throw std::runtime_error("音频接口初始化失败");
        }
        
        audio->SetConfig(16000, 320, 1, 4, 4096, 1024);
        
        // 创建Opus编解码器
        encoder = OpusAudio::CreateEncoder(16000, 1, OPUS_APPLICATION_VOIP);
        decoder = OpusAudio::CreateDecoder(16000, 1);
        
        if (!encoder || !decoder) {
            throw std::runtime_error("Opus编解码器创建失败");
        }
        
        // 初始化WebSocket客户端
        ws_client = std::make_unique<WebSocketClient>(ws_url);
        setupWebSocketCallbacks();
        
        INFO("音频WebSocket客户端初始化完成");
    }
    
    void setupWebSocketCallbacks() {
        ws_client->SetOnOpenCallback([this]() -> std::string {
            INFO("WebSocket连接已建立");
            
            json hello = {
                {"type", "hello"},
                {"audio_config", {
                    {"sample_rate", 16000},
                    {"channels", 1},
                    {"codec", "opus"}
                }}
            };
            
            return hello.dump();
        });
        
        ws_client->SetOnMessageCallback([this](const std::string& msg, bool binary) -> std::string {
            if (binary) {
                // 解码接收到的音频数据
                const unsigned char* opus_data = reinterpret_cast<const unsigned char*>(msg.data());
                short pcm_output[320];
                
                int decoded_samples = OpusAudio::Decode(decoder, opus_data, msg.size(),
                                                       pcm_output, 320, 0);
                if (decoded_samples > 0) {
                    audio->Write(pcm_output, decoded_samples);
                }
            } else {
                // 处理文本消息
                try {
                    json received = json::parse(msg);
                    std::string type = received["type"];
                    
                    if (type == "start_recording") {
                        startRecording();
                    } else if (type == "stop_recording") {
                        stopRecording();
                    }
                } catch (const json_exception& e) {
                    ERROR("JSON解析失败: {}", e.what());
                }
            }
            
            return "";
        });
        
        ws_client->SetOnCloseCallback([this]() {
            WARN("WebSocket连接已关闭");
            stopRecording();
        });
        
        ws_client->SetOnFailCallback([this]() {
            ERROR("WebSocket连接失败");
        });
    }
    
    void start() {
        ws_client->start();
    }
    
    void startRecording() {
        if (running) return;
        
        INFO("开始音频录制");
        running = true;
        audio->Record();
        
        audio_thread = std::thread([this]() {
            short pcm_buffer[320];
            unsigned char opus_buffer[1024];
            
            while (running) {
                if (audio->Read(pcm_buffer, 320)) {
                    // 编码PCM数据
                    int encoded_bytes = OpusAudio::Encode(encoder, pcm_buffer, 320,
                                                         opus_buffer, sizeof(opus_buffer));
                    if (encoded_bytes > 0) {
                        // 发送编码后的音频数据
                        ws_client->send_binary(opus_buffer, encoded_bytes);
                    }
                }
                
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        });
    }
    
    void stopRecording() {
        if (!running) return;
        
        INFO("停止音频录制");
        running = false;
        
        if (audio_thread.joinable()) {
            audio_thread.join();
        }
    }
    
    ~AudioWebSocketClient() {
        stopRecording();
        
        if (encoder) {
            OpusAudio::DestroyEncoder(encoder);
        }
        if (decoder) {
            OpusAudio::DestroyDecoder(decoder);
        }
    }
};

int main() {
    try {
        AudioWebSocketClient client("wss://api.example.com/audio");
        client.start();
        
        // 等待用户输入
        std::cout << "按回车键退出..." << std::endl;
        std::cin.get();
        
    } catch (const std::exception& e) {
        CRITICAL("程序异常: {}", e.what());
        return -1;
    }
    
    return 0;
}
```

---

## 版本信息

- **LinxSDK版本**: 1.0.0
- **API版本**: 1.0
- **文档版本**: 1.0.0
- **最后更新**: 2024年1月

## 许可证

本API参考文档遵循LinxSDK的许可证条款。

## 技术支持

如有API使用问题，请参考：
- [快速开始指南](quickstart.md)
- [模块使用文档](modules/)
- [示例代码](../demo/)

---

*注意：本文档中的所有示例代码仅供参考，实际使用时请根据具体需求进行调整。*