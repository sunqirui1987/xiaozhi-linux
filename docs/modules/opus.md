# Opus音频编解码模块使用指南

Opus模块提供了高质量的音频编解码功能，基于Opus音频编解码器实现，支持低延迟、高压缩比的音频处理，是LinxSDK中实现实时语音传输的关键组件。

## 模块概述

### 核心类

- **OpusAudio**: Opus音频编解码器封装类

### 主要功能

- 高质量音频编码和解码
- 支持多种采样率（8kHz - 48kHz）
- 可变比特率和恒定比特率模式
- 低延迟音频处理（最低2.5ms帧长）
- 自适应比特率控制
- 错误恢复和丢包处理

## API参考

### OpusAudio 类

```cpp
class OpusAudio {
public:
    // 构造函数，指定采样率和声道数
    OpusAudio(int sample_rate, int channels);
    ~OpusAudio();
    
    // 编码器管理
    OpusEncoder* CreateEncoder();
    void DestroyEncoder(OpusEncoder* encoder);
    
    // 解码器管理
    OpusDecoder* CreateDecoder();
    void DestroyDecoder(OpusDecoder* decoder);
    
    // 音频编码
    int Encode(unsigned char* data, int max_data_bytes, 
               const opus_int16* pcm, int frame_size);
    
    // 音频解码
    int Decode(opus_int16* pcm, int frame_size, 
               const unsigned char* data, int len);
    
    // 错误检查
    static bool CheckError(int error_code);
    
private:
    int sample_rate_;
    int channels_;
    OpusEncoder* encoder_;
    OpusDecoder* decoder_;
};
```

### 参数说明

#### 采样率支持
- **8000 Hz**: 窄带语音
- **12000 Hz**: 中等质量语音
- **16000 Hz**: 宽带语音（推荐）
- **24000 Hz**: 超宽带语音
- **48000 Hz**: 全带音频

#### 声道数
- **1**: 单声道（语音应用推荐）
- **2**: 立体声

#### 帧长度
- **2.5ms**: 超低延迟（120样本@48kHz）
- **5ms**: 低延迟（240样本@48kHz）
- **10ms**: 标准延迟（480样本@48kHz）
- **20ms**: 高效率（960样本@48kHz，推荐）
- **40ms**: 最高效率（1920样本@48kHz）
- **60ms**: 最大效率（2880样本@48kHz）

## 使用示例

### 基本编解码示例

```cpp
#include "Opus.h"
#include "Log.h"
#include <vector>
#include <iostream>

using namespace linx;

int main() {
    try {
        // 1. 创建Opus编解码器（16kHz，单声道）
        OpusAudio opus(16000, 1);
        
        // 2. 准备测试数据
        const int frame_size = 320; // 20ms @ 16kHz
        std::vector<opus_int16> input_pcm(frame_size);
        std::vector<unsigned char> encoded_data(1024);
        std::vector<opus_int16> decoded_pcm(frame_size);
        
        // 生成测试音频（1kHz正弦波）
        for (int i = 0; i < frame_size; i++) {
            double t = (double)i / 16000.0;
            input_pcm[i] = (opus_int16)(32767.0 * 0.5 * sin(2.0 * M_PI * 1000.0 * t));
        }
        
        // 3. 编码音频
        int encoded_bytes = opus.Encode(
            encoded_data.data(), encoded_data.size(),
            input_pcm.data(), frame_size
        );
        
        if (encoded_bytes > 0) {
            INFO("编码成功: {} 字节", encoded_bytes);
            
            // 4. 解码音频
            int decoded_samples = opus.Decode(
                decoded_pcm.data(), frame_size,
                encoded_data.data(), encoded_bytes
            );
            
            if (decoded_samples > 0) {
                INFO("解码成功: {} 样本", decoded_samples);
                
                // 5. 计算编解码质量
                double mse = 0.0;
                for (int i = 0; i < frame_size; i++) {
                    double diff = input_pcm[i] - decoded_pcm[i];
                    mse += diff * diff;
                }
                mse /= frame_size;
                
                double snr = 10.0 * log10(32767.0 * 32767.0 / mse);
                INFO("信噪比: {:.2f} dB", snr);
                
            } else {
                ERROR("解码失败: {}", decoded_samples);
            }
        } else {
            ERROR("编码失败: {}", encoded_bytes);
        }
        
    } catch (const std::exception& e) {
        ERROR("错误: {}", e.what());
        return -1;
    }
    
    return 0;
}
```

### 实时音频流编解码

```cpp
#include "Opus.h"
#include "AudioInterface.h"
#include "Log.h"
#include <thread>
#include <atomic>
#include <queue>
#include <mutex>

using namespace linx;

class OpusAudioStreamer {
private:
    OpusAudio opus;
    std::unique_ptr<AudioInterface> audio;
    
    std::atomic<bool> running{true};
    
    // 编码队列
    std::queue<std::vector<opus_int16>> encode_queue;
    std::mutex encode_mutex;
    
    // 解码队列
    std::queue<std::vector<unsigned char>> decode_queue;
    std::mutex decode_mutex;
    
    // 播放队列
    std::queue<std::vector<opus_int16>> playback_queue;
    std::mutex playback_mutex;
    
    const int sample_rate = 16000;
    const int channels = 1;
    const int frame_size = 320; // 20ms
    
public:
    OpusAudioStreamer() : opus(sample_rate, channels) {
        setupAudio();
    }
    
    void setupAudio() {
        audio = CreateAudioInterface();
        audio->Init();
        audio->SetConfig(sample_rate, frame_size, channels, 4, 4096, 1024);
        audio->Record();
        audio->Play();
    }
    
    void start() {
        // 启动录音线程
        std::thread record_thread([this]() {
            recordingLoop();
        });
        
        // 启动编码线程
        std::thread encode_thread([this]() {
            encodingLoop();
        });
        
        // 启动解码线程
        std::thread decode_thread([this]() {
            decodingLoop();
        });
        
        // 启动播放线程
        std::thread playback_thread([this]() {
            playbackLoop();
        });
        
        INFO("音频流处理已启动，按回车键停止...");
        std::cin.get();
        
        running = false;
        
        record_thread.join();
        encode_thread.join();
        decode_thread.join();
        playback_thread.join();
    }
    
    void recordingLoop() {
        std::vector<opus_int16> buffer(frame_size);
        
        while (running) {
            if (audio->Read(buffer.data(), frame_size)) {
                // 添加到编码队列
                std::lock_guard<std::mutex> lock(encode_mutex);
                encode_queue.push(buffer);
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
    
    void encodingLoop() {
        std::vector<unsigned char> encoded_buffer(1024);
        
        while (running) {
            std::vector<opus_int16> pcm_data;
            
            {
                std::lock_guard<std::mutex> lock(encode_mutex);
                if (!encode_queue.empty()) {
                    pcm_data = encode_queue.front();
                    encode_queue.pop();
                }
            }
            
            if (!pcm_data.empty()) {
                // 编码音频
                int encoded_bytes = opus.Encode(
                    encoded_buffer.data(), encoded_buffer.size(),
                    pcm_data.data(), frame_size
                );
                
                if (encoded_bytes > 0) {
                    // 模拟网络传输（这里直接添加到解码队列）
                    std::vector<unsigned char> packet(encoded_buffer.begin(), 
                                                     encoded_buffer.begin() + encoded_bytes);
                    
                    std::lock_guard<std::mutex> lock(decode_mutex);
                    decode_queue.push(packet);
                    
                    TRACE("编码完成: {} 字节", encoded_bytes);
                } else {
                    ERROR("编码失败: {}", encoded_bytes);
                }
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    }
    
    void decodingLoop() {
        std::vector<opus_int16> decoded_buffer(frame_size);
        
        while (running) {
            std::vector<unsigned char> encoded_data;
            
            {
                std::lock_guard<std::mutex> lock(decode_mutex);
                if (!decode_queue.empty()) {
                    encoded_data = decode_queue.front();
                    decode_queue.pop();
                }
            }
            
            if (!encoded_data.empty()) {
                // 解码音频
                int decoded_samples = opus.Decode(
                    decoded_buffer.data(), frame_size,
                    encoded_data.data(), encoded_data.size()
                );
                
                if (decoded_samples > 0) {
                    // 添加到播放队列
                    std::vector<opus_int16> audio_frame(decoded_buffer.begin(),
                                                       decoded_buffer.begin() + decoded_samples);
                    
                    std::lock_guard<std::mutex> lock(playback_mutex);
                    playback_queue.push(audio_frame);
                    
                    TRACE("解码完成: {} 样本", decoded_samples);
                } else {
                    ERROR("解码失败: {}", decoded_samples);
                }
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    }
    
    void playbackLoop() {
        std::vector<opus_int16> silence(frame_size, 0);
        
        while (running) {
            std::vector<opus_int16> audio_data;
            
            {
                std::lock_guard<std::mutex> lock(playback_mutex);
                if (!playback_queue.empty()) {
                    audio_data = playback_queue.front();
                    playback_queue.pop();
                }
            }
            
            if (!audio_data.empty()) {
                // 播放音频
                audio->Write(audio_data.data(), audio_data.size());
            } else {
                // 播放静音
                audio->Write(silence.data(), silence.size());
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
    }
    
    // 模拟接收网络音频数据
    void receiveNetworkAudio(const unsigned char* data, size_t size) {
        std::vector<unsigned char> packet(data, data + size);
        
        std::lock_guard<std::mutex> lock(decode_mutex);
        decode_queue.push(packet);
    }
};

int main() {
    try {
        OpusAudioStreamer streamer;
        streamer.start();
        
    } catch (const std::exception& e) {
        ERROR("错误: {}", e.what());
        return -1;
    }
    
    return 0;
}
```

### 高级编码器配置

```cpp
#include "Opus.h"
#include "Log.h"
#include <opus/opus.h>

using namespace linx;

class AdvancedOpusEncoder {
private:
    OpusAudio opus;
    OpusEncoder* encoder;
    OpusDecoder* decoder;
    
    int sample_rate;
    int channels;
    int frame_size;
    
public:
    AdvancedOpusEncoder(int sr, int ch, int fs) 
        : opus(sr, ch), sample_rate(sr), channels(ch), frame_size(fs) {
        encoder = opus.CreateEncoder();
        decoder = opus.CreateDecoder();
        
        if (!encoder || !decoder) {
            throw std::runtime_error("Failed to create Opus encoder/decoder");
        }
        
        configureEncoder();
    }
    
    ~AdvancedOpusEncoder() {
        if (encoder) opus.DestroyEncoder(encoder);
        if (decoder) opus.DestroyDecoder(decoder);
    }
    
    void configureEncoder() {
        int error;
        
        // 设置应用类型（语音优化）
        error = opus_encoder_ctl(encoder, OPUS_SET_APPLICATION(OPUS_APPLICATION_VOIP));
        if (error != OPUS_OK) {
            ERROR("设置应用类型失败: {}", opus_strerror(error));
        }
        
        // 设置比特率（32kbps）
        error = opus_encoder_ctl(encoder, OPUS_SET_BITRATE(32000));
        if (error != OPUS_OK) {
            ERROR("设置比特率失败: {}", opus_strerror(error));
        }
        
        // 启用可变比特率
        error = opus_encoder_ctl(encoder, OPUS_SET_VBR(1));
        if (error != OPUS_OK) {
            ERROR("启用VBR失败: {}", opus_strerror(error));
        }
        
        // 设置复杂度（0-10，10为最高质量）
        error = opus_encoder_ctl(encoder, OPUS_SET_COMPLEXITY(8));
        if (error != OPUS_OK) {
            ERROR("设置复杂度失败: {}", opus_strerror(error));
        }
        
        // 启用DTX（不连续传输）
        error = opus_encoder_ctl(encoder, OPUS_SET_DTX(1));
        if (error != OPUS_OK) {
            ERROR("启用DTX失败: {}", opus_strerror(error));
        }
        
        // 设置前向纠错
        error = opus_encoder_ctl(encoder, OPUS_SET_INBAND_FEC(1));
        if (error != OPUS_OK) {
            ERROR("启用FEC失败: {}", opus_strerror(error));
        }
        
        // 设置丢包期望率（5%）
        error = opus_encoder_ctl(encoder, OPUS_SET_PACKET_LOSS_PERC(5));
        if (error != OPUS_OK) {
            ERROR("设置丢包率失败: {}", opus_strerror(error));
        }
        
        INFO("Opus编码器配置完成");
    }
    
    void printEncoderInfo() {
        int value;
        
        opus_encoder_ctl(encoder, OPUS_GET_BITRATE(&value));
        INFO("当前比特率: {} bps", value);
        
        opus_encoder_ctl(encoder, OPUS_GET_VBR(&value));
        INFO("VBR模式: {}", value ? "启用" : "禁用");
        
        opus_encoder_ctl(encoder, OPUS_GET_COMPLEXITY(&value));
        INFO("复杂度: {}", value);
        
        opus_encoder_ctl(encoder, OPUS_GET_DTX(&value));
        INFO("DTX: {}", value ? "启用" : "禁用");
        
        opus_encoder_ctl(encoder, OPUS_GET_INBAND_FEC(&value));
        INFO("FEC: {}", value ? "启用" : "禁用");
    }
    
    int encode(const opus_int16* pcm, unsigned char* data, int max_bytes) {
        return opus_encode(encoder, pcm, frame_size, data, max_bytes);
    }
    
    int decode(const unsigned char* data, int len, opus_int16* pcm) {
        return opus_decode(decoder, data, len, pcm, frame_size, 0);
    }
    
    // 模拟丢包解码
    int decodeLost(opus_int16* pcm) {
        return opus_decode(decoder, nullptr, 0, pcm, frame_size, 0);
    }
    
    void setBitrate(int bitrate) {
        int error = opus_encoder_ctl(encoder, OPUS_SET_BITRATE(bitrate));
        if (error == OPUS_OK) {
            INFO("比特率已设置为: {} bps", bitrate);
        } else {
            ERROR("设置比特率失败: {}", opus_strerror(error));
        }
    }
    
    void setComplexity(int complexity) {
        int error = opus_encoder_ctl(encoder, OPUS_SET_COMPLEXITY(complexity));
        if (error == OPUS_OK) {
            INFO("复杂度已设置为: {}", complexity);
        } else {
            ERROR("设置复杂度失败: {}", opus_strerror(error));
        }
    }
};

// 使用示例
int main() {
    try {
        AdvancedOpusEncoder encoder(16000, 1, 320);
        encoder.printEncoderInfo();
        
        // 测试不同比特率
        std::vector<int> bitrates = {8000, 16000, 32000, 64000};
        
        for (int bitrate : bitrates) {
            encoder.setBitrate(bitrate);
            
            // 这里可以进行编码测试
            INFO("测试比特率: {} bps", bitrate);
        }
        
        // 测试丢包恢复
        std::vector<opus_int16> input_pcm(320, 1000); // 测试信号
        std::vector<unsigned char> encoded_data(1024);
        std::vector<opus_int16> decoded_pcm(320);
        
        // 正常编解码
        int encoded_bytes = encoder.encode(input_pcm.data(), encoded_data.data(), 1024);
        if (encoded_bytes > 0) {
            int decoded_samples = encoder.decode(encoded_data.data(), encoded_bytes, decoded_pcm.data());
            INFO("正常解码: {} 样本", decoded_samples);
        }
        
        // 模拟丢包
        int lost_samples = encoder.decodeLost(decoded_pcm.data());
        INFO("丢包恢复: {} 样本", lost_samples);
        
    } catch (const std::exception& e) {
        ERROR("错误: {}", e.what());
        return -1;
    }
    
    return 0;
}
```

### 文件编解码示例

```cpp
#include "Opus.h"
#include "FileStream.h"
#include "Log.h"
#include <fstream>

using namespace linx;

class OpusFileProcessor {
private:
    OpusAudio opus;
    int sample_rate;
    int channels;
    int frame_size;
    
public:
    OpusFileProcessor(int sr, int ch, int fs) 
        : opus(sr, ch), sample_rate(sr), channels(ch), frame_size(fs) {}
    
    bool encodeWavToOpus(const std::string& wav_file, const std::string& opus_file) {
        try {
            // 读取WAV文件
            FileStream wav_stream;
            if (!wav_stream.wavfopen(wav_file.c_str(), "rb")) {
                ERROR("无法打开WAV文件: {}", wav_file);
                return false;
            }
            
            // 创建Opus文件
            std::ofstream opus_stream(opus_file, std::ios::binary);
            if (!opus_stream) {
                ERROR("无法创建Opus文件: {}", opus_file);
                return false;
            }
            
            // 写入Opus文件头（简化版）
            writeOpusHeader(opus_stream);
            
            std::vector<opus_int16> pcm_buffer(frame_size);
            std::vector<unsigned char> opus_buffer(1024);
            
            size_t total_frames = 0;
            size_t total_bytes = 0;
            
            while (true) {
                // 读取PCM数据
                size_t samples_read = wav_stream.fread(pcm_buffer.data(), 
                                                     sizeof(opus_int16), frame_size);
                
                if (samples_read == 0) break;
                
                // 如果不足一帧，用零填充
                if (samples_read < frame_size) {
                    std::fill(pcm_buffer.begin() + samples_read, pcm_buffer.end(), 0);
                }
                
                // 编码
                int encoded_bytes = opus.Encode(
                    opus_buffer.data(), opus_buffer.size(),
                    pcm_buffer.data(), frame_size
                );
                
                if (encoded_bytes > 0) {
                    // 写入帧长度（4字节）
                    uint32_t frame_len = encoded_bytes;
                    opus_stream.write(reinterpret_cast<char*>(&frame_len), sizeof(frame_len));
                    
                    // 写入帧数据
                    opus_stream.write(reinterpret_cast<char*>(opus_buffer.data()), encoded_bytes);
                    
                    total_frames++;
                    total_bytes += encoded_bytes;
                } else {
                    ERROR("编码失败: {}", encoded_bytes);
                    return false;
                }
                
                if (samples_read < frame_size) break;
            }
            
            INFO("编码完成: {} 帧, {} 字节", total_frames, total_bytes);
            return true;
            
        } catch (const std::exception& e) {
            ERROR("编码过程中出错: {}", e.what());
            return false;
        }
    }
    
    bool decodeOpusToWav(const std::string& opus_file, const std::string& wav_file) {
        try {
            // 打开Opus文件
            std::ifstream opus_stream(opus_file, std::ios::binary);
            if (!opus_stream) {
                ERROR("无法打开Opus文件: {}", opus_file);
                return false;
            }
            
            // 跳过文件头
            skipOpusHeader(opus_stream);
            
            // 创建WAV文件
            FileStream wav_stream;
            if (!wav_stream.wavfopen(wav_file.c_str(), "wb")) {
                ERROR("无法创建WAV文件: {}", wav_file);
                return false;
            }
            
            std::vector<unsigned char> opus_buffer(1024);
            std::vector<opus_int16> pcm_buffer(frame_size);
            
            size_t total_frames = 0;
            size_t total_samples = 0;
            
            while (opus_stream.good()) {
                // 读取帧长度
                uint32_t frame_len;
                opus_stream.read(reinterpret_cast<char*>(&frame_len), sizeof(frame_len));
                
                if (!opus_stream.good() || frame_len == 0 || frame_len > 1024) {
                    break;
                }
                
                // 读取帧数据
                opus_stream.read(reinterpret_cast<char*>(opus_buffer.data()), frame_len);
                
                if (!opus_stream.good()) {
                    break;
                }
                
                // 解码
                int decoded_samples = opus.Decode(
                    pcm_buffer.data(), frame_size,
                    opus_buffer.data(), frame_len
                );
                
                if (decoded_samples > 0) {
                    // 写入PCM数据
                    wav_stream.fwrite(pcm_buffer.data(), sizeof(opus_int16), decoded_samples);
                    
                    total_frames++;
                    total_samples += decoded_samples;
                } else {
                    ERROR("解码失败: {}", decoded_samples);
                    return false;
                }
            }
            
            INFO("解码完成: {} 帧, {} 样本", total_frames, total_samples);
            return true;
            
        } catch (const std::exception& e) {
            ERROR("解码过程中出错: {}", e.what());
            return false;
        }
    }
    
private:
    void writeOpusHeader(std::ofstream& stream) {
        // 简化的Opus文件头
        struct OpusHeader {
            char signature[8] = {'O', 'p', 'u', 's', 'H', 'e', 'a', 'd'};
            uint8_t version = 1;
            uint8_t channels;
            uint16_t pre_skip = 0;
            uint32_t sample_rate;
            uint16_t gain = 0;
            uint8_t mapping_family = 0;
        } header;
        
        header.channels = channels;
        header.sample_rate = sample_rate;
        
        stream.write(reinterpret_cast<char*>(&header), sizeof(header));
    }
    
    void skipOpusHeader(std::ifstream& stream) {
        // 跳过简化的文件头
        stream.seekg(sizeof(char) * 8 + sizeof(uint8_t) * 3 + 
                    sizeof(uint16_t) * 2 + sizeof(uint32_t));
    }
};

int main() {
    try {
        OpusFileProcessor processor(16000, 1, 320);
        
        // 编码WAV到Opus
        if (processor.encodeWavToOpus("input.wav", "output.opus")) {
            INFO("WAV到Opus编码成功");
        }
        
        // 解码Opus到WAV
        if (processor.decodeOpusToWav("output.opus", "decoded.wav")) {
            INFO("Opus到WAV解码成功");
        }
        
    } catch (const std::exception& e) {
        ERROR("错误: {}", e.what());
        return -1;
    }
    
    return 0;
}
```

## 性能优化

### 1. 内存池管理

```cpp
class OpusBufferPool {
private:
    std::queue<std::vector<unsigned char>> encode_buffers;
    std::queue<std::vector<opus_int16>> decode_buffers;
    std::mutex pool_mutex;
    
    const size_t max_pool_size = 10;
    const size_t encode_buffer_size = 1024;
    const size_t decode_buffer_size = 960;
    
public:
    std::vector<unsigned char> getEncodeBuffer() {
        std::lock_guard<std::mutex> lock(pool_mutex);
        
        if (!encode_buffers.empty()) {
            auto buffer = std::move(encode_buffers.front());
            encode_buffers.pop();
            return buffer;
        }
        
        return std::vector<unsigned char>(encode_buffer_size);
    }
    
    void returnEncodeBuffer(std::vector<unsigned char>&& buffer) {
        std::lock_guard<std::mutex> lock(pool_mutex);
        
        if (encode_buffers.size() < max_pool_size) {
            buffer.clear();
            buffer.resize(encode_buffer_size);
            encode_buffers.push(std::move(buffer));
        }
    }
    
    std::vector<opus_int16> getDecodeBuffer() {
        std::lock_guard<std::mutex> lock(pool_mutex);
        
        if (!decode_buffers.empty()) {
            auto buffer = std::move(decode_buffers.front());
            decode_buffers.pop();
            return buffer;
        }
        
        return std::vector<opus_int16>(decode_buffer_size);
    }
    
    void returnDecodeBuffer(std::vector<opus_int16>&& buffer) {
        std::lock_guard<std::mutex> lock(pool_mutex);
        
        if (decode_buffers.size() < max_pool_size) {
            buffer.clear();
            buffer.resize(decode_buffer_size);
            decode_buffers.push(std::move(buffer));
        }
    }
};
```

### 2. 批量处理

```cpp
class BatchOpusProcessor {
private:
    OpusAudio opus;
    OpusBufferPool buffer_pool;
    
public:
    BatchOpusProcessor(int sample_rate, int channels) : opus(sample_rate, channels) {}
    
    std::vector<std::vector<unsigned char>> encodeBatch(
        const std::vector<std::vector<opus_int16>>& pcm_frames) {
        
        std::vector<std::vector<unsigned char>> encoded_frames;
        encoded_frames.reserve(pcm_frames.size());
        
        for (const auto& pcm_frame : pcm_frames) {
            auto encode_buffer = buffer_pool.getEncodeBuffer();
            
            int encoded_bytes = opus.Encode(
                encode_buffer.data(), encode_buffer.size(),
                pcm_frame.data(), pcm_frame.size()
            );
            
            if (encoded_bytes > 0) {
                encode_buffer.resize(encoded_bytes);
                encoded_frames.push_back(std::move(encode_buffer));
            } else {
                buffer_pool.returnEncodeBuffer(std::move(encode_buffer));
                ERROR("批量编码失败: {}", encoded_bytes);
            }
        }
        
        return encoded_frames;
    }
    
    std::vector<std::vector<opus_int16>> decodeBatch(
        const std::vector<std::vector<unsigned char>>& encoded_frames,
        int frame_size) {
        
        std::vector<std::vector<opus_int16>> decoded_frames;
        decoded_frames.reserve(encoded_frames.size());
        
        for (const auto& encoded_frame : encoded_frames) {
            auto decode_buffer = buffer_pool.getDecodeBuffer();
            
            int decoded_samples = opus.Decode(
                decode_buffer.data(), frame_size,
                encoded_frame.data(), encoded_frame.size()
            );
            
            if (decoded_samples > 0) {
                decode_buffer.resize(decoded_samples);
                decoded_frames.push_back(std::move(decode_buffer));
            } else {
                buffer_pool.returnDecodeBuffer(std::move(decode_buffer));
                ERROR("批量解码失败: {}", decoded_samples);
            }
        }
        
        return decoded_frames;
    }
};
```

## 错误处理和调试

### 常见错误码

```cpp
void handleOpusError(int error_code) {
    switch (error_code) {
        case OPUS_OK:
            // 成功，无需处理
            break;
            
        case OPUS_BAD_ARG:
            ERROR("Opus错误: 无效参数");
            break;
            
        case OPUS_BUFFER_TOO_SMALL:
            ERROR("Opus错误: 缓冲区太小");
            break;
            
        case OPUS_INTERNAL_ERROR:
            ERROR("Opus错误: 内部错误");
            break;
            
        case OPUS_INVALID_PACKET:
            ERROR("Opus错误: 无效数据包");
            break;
            
        case OPUS_UNIMPLEMENTED:
            ERROR("Opus错误: 功能未实现");
            break;
            
        case OPUS_INVALID_STATE:
            ERROR("Opus错误: 无效状态");
            break;
            
        case OPUS_ALLOC_FAIL:
            ERROR("Opus错误: 内存分配失败");
            break;
            
        default:
            ERROR("Opus错误: 未知错误码 {}", error_code);
            break;
    }
}
```

### 质量监控

```cpp
class OpusQualityMonitor {
private:
    struct QualityMetrics {
        double total_mse = 0.0;
        size_t frame_count = 0;
        size_t total_encoded_bytes = 0;
        size_t total_input_bytes = 0;
        
        double getAverageSNR() const {
            if (frame_count == 0) return 0.0;
            double avg_mse = total_mse / frame_count;
            return 10.0 * log10(32767.0 * 32767.0 / avg_mse);
        }
        
        double getCompressionRatio() const {
            if (total_input_bytes == 0) return 0.0;
            return (double)total_input_bytes / total_encoded_bytes;
        }
    };
    
    QualityMetrics metrics;
    
public:
    void addFrame(const opus_int16* original, const opus_int16* decoded, 
                  size_t samples, size_t encoded_bytes) {
        // 计算MSE
        double mse = 0.0;
        for (size_t i = 0; i < samples; i++) {
            double diff = original[i] - decoded[i];
            mse += diff * diff;
        }
        mse /= samples;
        
        metrics.total_mse += mse;
        metrics.frame_count++;
        metrics.total_encoded_bytes += encoded_bytes;
        metrics.total_input_bytes += samples * sizeof(opus_int16);
    }
    
    void printReport() {
        INFO("=== Opus质量报告 ===");
        INFO("处理帧数: {}", metrics.frame_count);
        INFO("平均信噪比: {:.2f} dB", metrics.getAverageSNR());
        INFO("压缩比: {:.2f}:1", metrics.getCompressionRatio());
        INFO("总输入字节: {}", metrics.total_input_bytes);
        INFO("总编码字节: {}", metrics.total_encoded_bytes);
    }
    
    void reset() {
        metrics = QualityMetrics{};
    }
};
```

## 最佳实践

1. **选择合适的帧长度**：20ms是语音应用的最佳选择
2. **配置合适的比特率**：语音通话建议16-32kbps
3. **启用FEC**：在网络不稳定环境下提高容错性
4. **使用DTX**：在静音期间节省带宽
5. **内存管理**：使用对象池避免频繁内存分配
6. **错误处理**：始终检查编解码返回值
7. **质量监控**：定期评估编解码质量
8. **线程安全**：编解码器不是线程安全的，需要适当同步