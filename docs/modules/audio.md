# 音频模块使用指南

音频模块是LinxSDK的核心组件，提供跨平台的音频录制和播放功能。它通过抽象接口设计，支持不同平台的音频后端。

## 模块概述

### 核心类

- **AudioInterface**: 音频接口抽象基类
- **PortAudioImpl**: PortAudio实现（macOS/跨平台）
- **AlsaAudio**: ALSA实现（Linux）

### 主要功能

- 音频设备初始化和配置
- 实时音频录制（麦克风输入）
- 实时音频播放（扬声器输出）
- 跨平台音频后端支持
- 低延迟音频处理

## API参考

### AudioInterface 抽象接口

```cpp
class AudioInterface {
public:
    virtual ~AudioInterface() = default;
    
    // 初始化音频系统
    virtual void Init() = 0;
    
    // 配置音频参数
    virtual void SetConfig(unsigned int sample_rate, int frame_size, int channels, 
                          int periods, int buffer_size, int period_size) = 0;
    
    // 读取音频数据（录制）
    virtual bool Read(short* buffer, size_t frame_size) = 0;
    
    // 写入音频数据（播放）
    virtual bool Write(short* buffer, size_t frame_size) = 0;
    
    // 启动录制流
    virtual void Record() = 0;
    
    // 启动播放流
    virtual void Play() = 0;
};

// 工厂函数：创建平台相关的音频实现
std::unique_ptr<AudioInterface> CreateAudioInterface();
```

### 参数说明

#### SetConfig 参数详解

- **sample_rate**: 采样率（Hz）
  - 常用值：8000, 16000, 44100, 48000
  - 推荐：16000（语音应用）

- **frame_size**: 每次处理的帧大小（样本数）
  - 影响延迟：帧越小延迟越低
  - 推荐：320（20ms @ 16kHz）

- **channels**: 声道数
  - 1: 单声道（语音应用推荐）
  - 2: 立体声

- **periods**: 缓冲区周期数
  - 影响稳定性和延迟
  - 推荐：4

- **buffer_size**: 总缓冲区大小（样本数）
  - 影响延迟和稳定性
  - 推荐：4096

- **period_size**: 每个周期的大小（样本数）
  - 通常为 buffer_size / periods
  - 推荐：1024

## 使用示例

### 基本音频录制

```cpp
#include "AudioInterface.h"
#include "Log.h"
#include <vector>
#include <fstream>

using namespace linx;

int main() {
    try {
        // 1. 创建音频接口
        auto audio = CreateAudioInterface();
        
        // 2. 初始化
        audio->Init();
        
        // 3. 配置参数（16kHz, 20ms帧, 单声道）
        audio->SetConfig(16000, 320, 1, 4, 4096, 1024);
        
        // 4. 启动录制
        audio->Record();
        
        // 5. 录制音频数据
        std::vector<short> recorded_data;
        short buffer[320];
        
        INFO("开始录制5秒音频...");
        
        for (int i = 0; i < 250; i++) { // 5秒 = 250 * 20ms
            if (audio->Read(buffer, 320)) {
                // 将数据添加到录制缓冲区
                recorded_data.insert(recorded_data.end(), buffer, buffer + 320);
            }
        }
        
        INFO("录制完成，共录制 {} 个样本", recorded_data.size());
        
        // 6. 保存到文件（可选）
        std::ofstream file("recorded.pcm", std::ios::binary);
        file.write(reinterpret_cast<const char*>(recorded_data.data()), 
                   recorded_data.size() * sizeof(short));
        file.close();
        
    } catch (const std::exception& e) {
        ERROR("音频录制失败: {}", e.what());
        return -1;
    }
    
    return 0;
}
```

### 基本音频播放

```cpp
#include "AudioInterface.h"
#include "Log.h"
#include <vector>
#include <cmath>

using namespace linx;

// 生成正弦波测试音频
std::vector<short> generateSineWave(int sample_rate, int duration_ms, int frequency) {
    int num_samples = sample_rate * duration_ms / 1000;
    std::vector<short> samples(num_samples);
    
    for (int i = 0; i < num_samples; i++) {
        double t = static_cast<double>(i) / sample_rate;
        double amplitude = 0.3; // 30%音量
        samples[i] = static_cast<short>(amplitude * 32767 * sin(2 * M_PI * frequency * t));
    }
    
    return samples;
}

int main() {
    try {
        // 1. 创建音频接口
        auto audio = CreateAudioInterface();
        
        // 2. 初始化和配置
        audio->Init();
        audio->SetConfig(16000, 320, 1, 4, 4096, 1024);
        
        // 3. 启动播放
        audio->Play();
        
        // 4. 生成测试音频（440Hz A音，持续2秒）
        auto test_audio = generateSineWave(16000, 2000, 440);
        
        INFO("开始播放测试音频...");
        
        // 5. 播放音频
        size_t pos = 0;
        while (pos < test_audio.size()) {
            size_t chunk_size = std::min(size_t(320), test_audio.size() - pos);
            
            if (audio->Write(&test_audio[pos], chunk_size)) {
                pos += chunk_size;
            }
            
            // 如果数据不足一帧，用静音填充
            if (chunk_size < 320) {
                short silence[320 - chunk_size] = {0};
                audio->Write(silence, 320 - chunk_size);
                break;
            }
        }
        
        INFO("播放完成");
        
    } catch (const std::exception& e) {
        ERROR("音频播放失败: {}", e.what());
        return -1;
    }
    
    return 0;
}
```

### 实时音频处理（回声）

```cpp
#include "AudioInterface.h"
#include "Log.h"
#include <thread>
#include <atomic>
#include <queue>
#include <mutex>

using namespace linx;

class AudioEcho {
private:
    std::queue<std::vector<short>> delay_buffer;
    std::mutex buffer_mutex;
    const size_t delay_frames = 25; // 500ms延迟 @ 20ms/frame
    
public:
    void processAudio(short* input, short* output, size_t frame_size) {
        std::lock_guard<std::mutex> lock(buffer_mutex);
        
        // 将输入添加到延迟缓冲区
        std::vector<short> input_frame(input, input + frame_size);
        delay_buffer.push(input_frame);
        
        // 如果缓冲区足够大，输出延迟的音频
        if (delay_buffer.size() > delay_frames) {
            auto delayed_frame = delay_buffer.front();
            delay_buffer.pop();
            
            // 混合原始音频和延迟音频
            for (size_t i = 0; i < frame_size; i++) {
                int mixed = input[i] + delayed_frame[i] * 0.5; // 50%回声
                output[i] = std::max(-32768, std::min(32767, mixed)); // 防止溢出
            }
        } else {
            // 缓冲区不足，直接输出原始音频
            std::copy(input, input + frame_size, output);
        }
    }
};

int main() {
    try {
        auto audio = CreateAudioInterface();
        audio->Init();
        audio->SetConfig(16000, 320, 1, 4, 4096, 1024);
        
        // 启动录制和播放
        audio->Record();
        audio->Play();
        
        AudioEcho echo_processor;
        std::atomic<bool> running{true};
        
        // 音频处理线程
        std::thread audio_thread([&]() {
            short input_buffer[320];
            short output_buffer[320];
            
            while (running) {
                if (audio->Read(input_buffer, 320)) {
                    // 处理音频（添加回声效果）
                    echo_processor.processAudio(input_buffer, output_buffer, 320);
                    
                    // 播放处理后的音频
                    audio->Write(output_buffer, 320);
                }
            }
        });
        
        INFO("实时音频回声处理启动，按回车键停止...");
        std::cin.get();
        
        running = false;
        audio_thread.join();
        
    } catch (const std::exception& e) {
        ERROR("音频处理失败: {}", e.what());
        return -1;
    }
    
    return 0;
}
```

## 平台特定说明

### macOS (PortAudio)

- 自动选择默认音频设备
- 支持Core Audio后端
- 需要麦克风权限（系统偏好设置）
- 低延迟性能优秀

### Linux (ALSA)

- 直接使用ALSA API
- 支持多种音频设备
- 可能需要配置音频设备权限
- 适合嵌入式和服务器环境

## 性能优化建议

### 1. 选择合适的帧大小

```cpp
// 低延迟应用（实时通信）
audio->SetConfig(16000, 160, 1, 4, 2048, 512); // 10ms帧

// 平衡应用（一般语音处理）
audio->SetConfig(16000, 320, 1, 4, 4096, 1024); // 20ms帧

// 高质量应用（音乐播放）
audio->SetConfig(48000, 1024, 2, 4, 8192, 2048); // ~21ms帧
```

### 2. 缓冲区管理

```cpp
// 使用环形缓冲区减少内存分配
class RingBuffer {
private:
    std::vector<short> buffer;
    size_t read_pos = 0;
    size_t write_pos = 0;
    size_t size;
    
public:
    RingBuffer(size_t capacity) : buffer(capacity), size(capacity) {}
    
    bool write(const short* data, size_t count) {
        if (available_write() < count) return false;
        
        for (size_t i = 0; i < count; i++) {
            buffer[write_pos] = data[i];
            write_pos = (write_pos + 1) % size;
        }
        return true;
    }
    
    bool read(short* data, size_t count) {
        if (available_read() < count) return false;
        
        for (size_t i = 0; i < count; i++) {
            data[i] = buffer[read_pos];
            read_pos = (read_pos + 1) % size;
        }
        return true;
    }
    
    size_t available_read() const {
        return (write_pos - read_pos + size) % size;
    }
    
    size_t available_write() const {
        return size - available_read() - 1;
    }
};
```

### 3. 线程优先级设置

```cpp
#include <pthread.h>

void setAudioThreadPriority() {
    // 设置音频线程为实时优先级
    struct sched_param param;
    param.sched_priority = 80; // 高优先级
    
    if (pthread_setschedparam(pthread_self(), SCHED_FIFO, &param) != 0) {
        WARN("无法设置实时优先级");
    }
}
```

## 故障排除

### 常见问题

1. **音频设备打开失败**
   - 检查设备权限
   - 确认设备未被其他应用占用
   - 验证音频参数是否支持

2. **音频断续或卡顿**
   - 增加缓冲区大小
   - 检查CPU使用率
   - 优化音频处理算法

3. **延迟过高**
   - 减小帧大小
   - 减少缓冲区周期数
   - 使用实时线程优先级

### 调试技巧

```cpp
// 音频数据可视化
void printAudioLevel(const short* buffer, size_t size) {
    long energy = 0;
    for (size_t i = 0; i < size; i++) {
        energy += abs(buffer[i]);
    }
    
    int level = energy / (size * 1000); // 简化的音量级别
    std::string bar(std::min(level, 50), '=');
    INFO("音频级别: [{}{}]", bar, std::string(50 - bar.length(), ' '));
}

// 音频数据保存
void saveAudioDebug(const short* buffer, size_t size, const std::string& filename) {
    static std::ofstream debug_file(filename, std::ios::binary | std::ios::app);
    debug_file.write(reinterpret_cast<const char*>(buffer), size * sizeof(short));
}
```

## 最佳实践

1. **始终检查返回值**：音频操作可能失败
2. **使用适当的缓冲区大小**：平衡延迟和稳定性
3. **实现错误恢复机制**：处理设备断开等异常情况
4. **避免在音频回调中进行重操作**：保持回调函数轻量级
5. **使用专用线程处理音频**：避免UI线程阻塞