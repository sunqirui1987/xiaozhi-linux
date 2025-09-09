# 文件流模块使用指南

文件流模块提供了高效的文件读写操作，特别针对音频文件处理进行了优化，支持WAV格式的读写、PCM数据处理以及音频格式转换，是LinxSDK中处理音频文件的核心组件。

## 模块概述

### 核心组件

- **FileStream类**: 基础文件流操作类
- **WAVE格式支持**: WAV文件头解析和生成
- **音频转换函数**: PCM到WAV、WAV到MP3等格式转换
- **断言宏**: 调试和错误检查支持

### 主要功能

- 标准文件操作（打开、读取、写入、关闭）
- WAV音频文件的读写
- PCM原始音频数据处理
- 音频格式转换
- 单声道音频文件保存
- 文件指针操作和定位
- 内存安全的文件操作

## 数据结构

### WAV文件格式结构

```cpp
// WAV文件头结构
struct WAVE_HEADER {
    char riff[4];           // "RIFF"
    uint32_t file_size;     // 文件大小 - 8
    char wave[4];           // "WAVE"
};

// WAV格式块结构
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

// WAV数据块结构
struct WAVE_DATA {
    char data[4];           // "data"
    uint32_t data_size;     // 数据大小
};
```

## API参考

### FileStream类

```cpp
class FileStream {
public:
    // 构造函数和析构函数
    FileStream();
    ~FileStream();
    
    // 基本文件操作
    FILE* fopen(const char* filename, const char* mode);
    size_t fwrite(const void* ptr, size_t size, size_t count);
    size_t fread(void* ptr, size_t size, size_t count);
    void rewind();
    int fclose();
    
    // WAV文件操作
    FILE* wavfopen(const char* filename, const char* mode, 
                   int sample_rate, int channels, int bits_per_sample);
    void saveWavWithOneChannel(const char* filename, const short* data, 
                              size_t samples, int sample_rate);
    
private:
    FILE* fp_;              // 文件指针
    std::string filePath_;  // 文件路径
};

// 全局音频转换函数
void pcm2wav(const char* pcm_file, const char* wav_file, 
             int sample_rate, int channels, int bits_per_sample);
void wav2mp3(const char* wav_file, const char* mp3_file);
```

### 断言宏

```cpp
// 调试断言宏
#ifdef DEBUG
#define ASSERT(condition, message) \
    do { \
        if (!(condition)) { \
            std::cerr << "断言失败: " << message << std::endl; \
            std::abort(); \
        } \
    } while(0)
#else
#define ASSERT(condition, message)
#endif
```

## 使用示例

### 基本文件操作

```cpp
#include "FileStream.h"
#include <iostream>
#include <vector>

using namespace linx;

int main() {
    FileStream fs;
    
    // 1. 写入文本文件
    FILE* write_file = fs.fopen("test.txt", "w");
    if (write_file) {
        const char* text = "Hello, LinxSDK FileStream!";
        size_t written = fs.fwrite(text, sizeof(char), strlen(text));
        std::cout << "写入字节数: " << written << std::endl;
        fs.fclose();
    }
    
    // 2. 读取文本文件
    FILE* read_file = fs.fopen("test.txt", "r");
    if (read_file) {
        char buffer[256];
        size_t read_bytes = fs.fread(buffer, sizeof(char), sizeof(buffer) - 1);
        buffer[read_bytes] = '\0';
        std::cout << "读取内容: " << buffer << std::endl;
        fs.fclose();
    }
    
    // 3. 二进制文件操作
    std::vector<int> data = {1, 2, 3, 4, 5};
    
    // 写入二进制数据
    FILE* bin_write = fs.fopen("data.bin", "wb");
    if (bin_write) {
        fs.fwrite(data.data(), sizeof(int), data.size());
        fs.fclose();
    }
    
    // 读取二进制数据
    FILE* bin_read = fs.fopen("data.bin", "rb");
    if (bin_read) {
        std::vector<int> read_data(5);
        size_t read_count = fs.fread(read_data.data(), sizeof(int), 5);
        std::cout << "读取数据: ";
        for (size_t i = 0; i < read_count; i++) {
            std::cout << read_data[i] << " ";
        }
        std::cout << std::endl;
        fs.fclose();
    }
    
    return 0;
}
```

### WAV音频文件操作

```cpp
#include "FileStream.h"
#include <iostream>
#include <vector>
#include <cmath>

using namespace linx;

class AudioFileProcessor {
private:
    FileStream fs;
    
public:
    // 生成正弦波音频数据
    std::vector<short> generateSineWave(double frequency, double duration, 
                                       int sample_rate, double amplitude = 0.5) {
        size_t samples = static_cast<size_t>(duration * sample_rate);
        std::vector<short> audio_data(samples);
        
        for (size_t i = 0; i < samples; i++) {
            double t = static_cast<double>(i) / sample_rate;
            double value = amplitude * sin(2.0 * M_PI * frequency * t);
            audio_data[i] = static_cast<short>(value * 32767);
        }
        
        return audio_data;
    }
    
    // 保存单声道WAV文件
    bool saveSingleChannelWav(const std::string& filename, 
                             const std::vector<short>& audio_data, 
                             int sample_rate) {
        try {
            fs.saveWavWithOneChannel(filename.c_str(), audio_data.data(), 
                                   audio_data.size(), sample_rate);
            std::cout << "WAV文件保存成功: " << filename << std::endl;
            std::cout << "样本数: " << audio_data.size() << std::endl;
            std::cout << "采样率: " << sample_rate << " Hz" << std::endl;
            std::cout << "时长: " << (double)audio_data.size() / sample_rate << " 秒" << std::endl;
            return true;
        } catch (const std::exception& e) {
            std::cerr << "保存WAV文件失败: " << e.what() << std::endl;
            return false;
        }
    }
    
    // 创建WAV文件并写入数据
    bool createWavFile(const std::string& filename, 
                      const std::vector<short>& audio_data,
                      int sample_rate, int channels = 1, int bits_per_sample = 16) {
        FILE* wav_file = fs.wavfopen(filename.c_str(), "wb", 
                                   sample_rate, channels, bits_per_sample);
        if (!wav_file) {
            std::cerr << "无法创建WAV文件: " << filename << std::endl;
            return false;
        }
        
        // 写入音频数据
        size_t written = fs.fwrite(audio_data.data(), sizeof(short), audio_data.size());
        fs.fclose();
        
        if (written == audio_data.size()) {
            std::cout << "WAV文件创建成功: " << filename << std::endl;
            return true;
        } else {
            std::cerr << "写入音频数据不完整" << std::endl;
            return false;
        }
    }
    
    // 读取WAV文件数据
    std::vector<short> readWavFile(const std::string& filename, 
                                  int& sample_rate, int& channels) {
        std::vector<short> audio_data;
        
        FILE* wav_file = fs.fopen(filename.c_str(), "rb");
        if (!wav_file) {
            std::cerr << "无法打开WAV文件: " << filename << std::endl;
            return audio_data;
        }
        
        // 读取WAV文件头
        WAVE_HEADER header;
        WAVE_FMT fmt;
        WAVE_DATA data_header;
        
        fs.fread(&header, sizeof(WAVE_HEADER), 1);
        fs.fread(&fmt, sizeof(WAVE_FMT), 1);
        fs.fread(&data_header, sizeof(WAVE_DATA), 1);
        
        // 验证WAV文件格式
        if (strncmp(header.riff, "RIFF", 4) != 0 || 
            strncmp(header.wave, "WAVE", 4) != 0 ||
            strncmp(data_header.data, "data", 4) != 0) {
            std::cerr << "无效的WAV文件格式" << std::endl;
            fs.fclose();
            return audio_data;
        }
        
        // 提取音频参数
        sample_rate = fmt.sample_rate;
        channels = fmt.channels;
        
        // 读取音频数据
        size_t samples = data_header.data_size / sizeof(short);
        audio_data.resize(samples);
        
        size_t read_samples = fs.fread(audio_data.data(), sizeof(short), samples);
        fs.fclose();
        
        if (read_samples == samples) {
            std::cout << "WAV文件读取成功: " << filename << std::endl;
            std::cout << "采样率: " << sample_rate << " Hz" << std::endl;
            std::cout << "声道数: " << channels << std::endl;
            std::cout << "样本数: " << samples << std::endl;
        } else {
            std::cerr << "读取音频数据不完整" << std::endl;
            audio_data.clear();
        }
        
        return audio_data;
    }
    
    // 音频数据分析
    void analyzeAudioData(const std::vector<short>& audio_data, int sample_rate) {
        if (audio_data.empty()) {
            std::cout << "音频数据为空" << std::endl;
            return;
        }
        
        // 计算基本统计信息
        short min_val = *std::min_element(audio_data.begin(), audio_data.end());
        short max_val = *std::max_element(audio_data.begin(), audio_data.end());
        
        long long sum = 0;
        for (short sample : audio_data) {
            sum += sample;
        }
        double mean = static_cast<double>(sum) / audio_data.size();
        
        // 计算RMS值
        long long sum_squares = 0;
        for (short sample : audio_data) {
            long long diff = sample - static_cast<long long>(mean);
            sum_squares += diff * diff;
        }
        double rms = sqrt(static_cast<double>(sum_squares) / audio_data.size());
        
        // 输出分析结果
        std::cout << "=== 音频数据分析 ===" << std::endl;
        std::cout << "样本数: " << audio_data.size() << std::endl;
        std::cout << "时长: " << (double)audio_data.size() / sample_rate << " 秒" << std::endl;
        std::cout << "最小值: " << min_val << std::endl;
        std::cout << "最大值: " << max_val << std::endl;
        std::cout << "平均值: " << mean << std::endl;
        std::cout << "RMS值: " << rms << std::endl;
        std::cout << "动态范围: " << (max_val - min_val) << std::endl;
    }
};

int main() {
    AudioFileProcessor processor;
    
    // 1. 生成不同频率的正弦波
    std::cout << "生成音频数据..." << std::endl;
    
    auto sine_440 = processor.generateSineWave(440.0, 2.0, 44100);  // A4音符，2秒
    auto sine_880 = processor.generateSineWave(880.0, 1.5, 44100);  // A5音符，1.5秒
    auto sine_220 = processor.generateSineWave(220.0, 3.0, 44100);  // A3音符，3秒
    
    // 2. 保存为WAV文件
    std::cout << "\n保存WAV文件..." << std::endl;
    processor.saveSingleChannelWav("sine_440hz.wav", sine_440, 44100);
    processor.saveSingleChannelWav("sine_880hz.wav", sine_880, 44100);
    processor.saveSingleChannelWav("sine_220hz.wav", sine_220, 44100);
    
    // 3. 创建复合音频（混合两个频率）
    std::vector<short> mixed_audio(sine_440.size());
    for (size_t i = 0; i < sine_440.size() && i < sine_880.size(); i++) {
        mixed_audio[i] = (sine_440[i] + sine_880[i]) / 2;
    }
    processor.createWavFile("mixed_audio.wav", mixed_audio, 44100);
    
    // 4. 读取并分析WAV文件
    std::cout << "\n读取和分析WAV文件..." << std::endl;
    int sample_rate, channels;
    auto read_audio = processor.readWavFile("sine_440hz.wav", sample_rate, channels);
    processor.analyzeAudioData(read_audio, sample_rate);
    
    return 0;
}
```

### 音频格式转换

```cpp
#include "FileStream.h"
#include <iostream>
#include <vector>
#include <filesystem>

using namespace linx;
namespace fs = std::filesystem;

class AudioConverter {
public:
    // PCM转WAV
    static bool convertPcmToWav(const std::string& pcm_file, 
                               const std::string& wav_file,
                               int sample_rate = 16000, 
                               int channels = 1, 
                               int bits_per_sample = 16) {
        try {
            pcm2wav(pcm_file.c_str(), wav_file.c_str(), 
                   sample_rate, channels, bits_per_sample);
            
            std::cout << "PCM转WAV成功:" << std::endl;
            std::cout << "  输入: " << pcm_file << std::endl;
            std::cout << "  输出: " << wav_file << std::endl;
            std::cout << "  参数: " << sample_rate << "Hz, " 
                     << channels << "声道, " << bits_per_sample << "位" << std::endl;
            
            return true;
        } catch (const std::exception& e) {
            std::cerr << "PCM转WAV失败: " << e.what() << std::endl;
            return false;
        }
    }
    
    // WAV转MP3
    static bool convertWavToMp3(const std::string& wav_file, 
                               const std::string& mp3_file) {
        try {
            wav2mp3(wav_file.c_str(), mp3_file.c_str());
            
            std::cout << "WAV转MP3成功:" << std::endl;
            std::cout << "  输入: " << wav_file << std::endl;
            std::cout << "  输出: " << mp3_file << std::endl;
            
            return true;
        } catch (const std::exception& e) {
            std::cerr << "WAV转MP3失败: " << e.what() << std::endl;
            return false;
        }
    }
    
    // 批量转换PCM文件
    static void batchConvertPcmToWav(const std::string& input_dir, 
                                    const std::string& output_dir,
                                    int sample_rate = 16000) {
        // 创建输出目录
        fs::create_directories(output_dir);
        
        std::cout << "开始批量转换PCM文件..." << std::endl;
        std::cout << "输入目录: " << input_dir << std::endl;
        std::cout << "输出目录: " << output_dir << std::endl;
        
        int converted_count = 0;
        int failed_count = 0;
        
        try {
            for (const auto& entry : fs::directory_iterator(input_dir)) {
                if (entry.is_regular_file() && 
                    entry.path().extension() == ".pcm") {
                    
                    std::string pcm_file = entry.path().string();
                    std::string wav_file = output_dir + "/" + 
                                         entry.path().stem().string() + ".wav";
                    
                    std::cout << "转换: " << entry.path().filename() << std::endl;
                    
                    if (convertPcmToWav(pcm_file, wav_file, sample_rate)) {
                        converted_count++;
                    } else {
                        failed_count++;
                    }
                }
            }
        } catch (const std::exception& e) {
            std::cerr << "批量转换过程中发生错误: " << e.what() << std::endl;
        }
        
        std::cout << "批量转换完成:" << std::endl;
        std::cout << "  成功: " << converted_count << " 个文件" << std::endl;
        std::cout << "  失败: " << failed_count << " 个文件" << std::endl;
    }
    
    // 获取文件信息
    static void printFileInfo(const std::string& filename) {
        try {
            if (!fs::exists(filename)) {
                std::cout << "文件不存在: " << filename << std::endl;
                return;
            }
            
            auto file_size = fs::file_size(filename);
            auto last_write = fs::last_write_time(filename);
            
            std::cout << "文件信息: " << filename << std::endl;
            std::cout << "  大小: " << file_size << " 字节" << std::endl;
            std::cout << "  大小: " << (double)file_size / 1024 << " KB" << std::endl;
            
            // 如果是音频文件，估算时长
            std::string ext = fs::path(filename).extension();
            if (ext == ".pcm") {
                // 假设16位单声道16kHz PCM
                double duration = (double)file_size / (2 * 16000);
                std::cout << "  估算时长: " << duration << " 秒 (假设16位单声道16kHz)" << std::endl;
            }
            
        } catch (const std::exception& e) {
            std::cerr << "获取文件信息失败: " << e.what() << std::endl;
        }
    }
};

// 音频文件工具类
class AudioFileUtils {
public:
    // 创建测试PCM文件
    static bool createTestPcmFile(const std::string& filename, 
                                 double frequency, double duration, 
                                 int sample_rate = 16000) {
        FileStream fs;
        FILE* file = fs.fopen(filename.c_str(), "wb");
        if (!file) {
            std::cerr << "无法创建PCM文件: " << filename << std::endl;
            return false;
        }
        
        size_t samples = static_cast<size_t>(duration * sample_rate);
        
        for (size_t i = 0; i < samples; i++) {
            double t = static_cast<double>(i) / sample_rate;
            double value = 0.5 * sin(2.0 * M_PI * frequency * t);
            short sample = static_cast<short>(value * 32767);
            fs.fwrite(&sample, sizeof(short), 1);
        }
        
        fs.fclose();
        
        std::cout << "测试PCM文件创建成功: " << filename << std::endl;
        std::cout << "  频率: " << frequency << " Hz" << std::endl;
        std::cout << "  时长: " << duration << " 秒" << std::endl;
        std::cout << "  采样率: " << sample_rate << " Hz" << std::endl;
        
        return true;
    }
    
    // 比较两个音频文件
    static bool compareAudioFiles(const std::string& file1, 
                                 const std::string& file2) {
        FileStream fs1, fs2;
        
        FILE* f1 = fs1.fopen(file1.c_str(), "rb");
        FILE* f2 = fs2.fopen(file2.c_str(), "rb");
        
        if (!f1 || !f2) {
            std::cerr << "无法打开文件进行比较" << std::endl;
            if (f1) fs1.fclose();
            if (f2) fs2.fclose();
            return false;
        }
        
        // 比较文件大小
        auto size1 = fs::file_size(file1);
        auto size2 = fs::file_size(file2);
        
        if (size1 != size2) {
            std::cout << "文件大小不同: " << size1 << " vs " << size2 << std::endl;
            fs1.fclose();
            fs2.fclose();
            return false;
        }
        
        // 逐字节比较
        const size_t buffer_size = 4096;
        char buffer1[buffer_size], buffer2[buffer_size];
        
        bool identical = true;
        size_t total_read = 0;
        
        while (true) {
            size_t read1 = fs1.fread(buffer1, 1, buffer_size);
            size_t read2 = fs2.fread(buffer2, 1, buffer_size);
            
            if (read1 != read2) {
                identical = false;
                break;
            }
            
            if (read1 == 0) break;
            
            if (memcmp(buffer1, buffer2, read1) != 0) {
                identical = false;
                break;
            }
            
            total_read += read1;
        }
        
        fs1.fclose();
        fs2.fclose();
        
        std::cout << "文件比较结果: " << (identical ? "相同" : "不同") << std::endl;
        std::cout << "比较字节数: " << total_read << std::endl;
        
        return identical;
    }
};

int main() {
    // 1. 创建测试PCM文件
    std::cout << "创建测试PCM文件..." << std::endl;
    AudioFileUtils::createTestPcmFile("test_440hz.pcm", 440.0, 2.0, 16000);
    AudioFileUtils::createTestPcmFile("test_880hz.pcm", 880.0, 1.5, 16000);
    AudioFileUtils::createTestPcmFile("test_220hz.pcm", 220.0, 3.0, 16000);
    
    // 2. 显示文件信息
    std::cout << "\n文件信息:" << std::endl;
    AudioConverter::printFileInfo("test_440hz.pcm");
    AudioConverter::printFileInfo("test_880hz.pcm");
    AudioConverter::printFileInfo("test_220hz.pcm");
    
    // 3. 单个文件转换
    std::cout << "\n单个文件转换:" << std::endl;
    AudioConverter::convertPcmToWav("test_440hz.pcm", "test_440hz.wav", 16000, 1, 16);
    AudioConverter::convertPcmToWav("test_880hz.pcm", "test_880hz.wav", 16000, 1, 16);
    
    // 4. 批量转换
    std::cout << "\n批量转换:" << std::endl;
    AudioConverter::batchConvertPcmToWav(".", "./wav_output", 16000);
    
    // 5. WAV转MP3（如果支持）
    std::cout << "\nWAV转MP3:" << std::endl;
    AudioConverter::convertWavToMp3("test_440hz.wav", "test_440hz.mp3");
    
    // 6. 文件比较
    std::cout << "\n文件比较:" << std::endl;
    AudioFileUtils::compareAudioFiles("test_440hz.pcm", "test_880hz.pcm");
    
    return 0;
}
```

### 实时音频流处理

```cpp
#include "FileStream.h"
#include "AudioInterface.h"
#include <iostream>
#include <thread>
#include <atomic>
#include <queue>
#include <mutex>
#include <condition_variable>

using namespace linx;

class AudioStreamRecorder {
private:
    FileStream fs;
    std::unique_ptr<AudioInterface> audio;
    
    std::atomic<bool> recording{false};
    std::thread record_thread;
    
    // 音频数据队列
    std::queue<std::vector<short>> audio_queue;
    std::mutex queue_mutex;
    std::condition_variable queue_cv;
    
    // 文件写入线程
    std::thread write_thread;
    std::atomic<bool> writing{false};
    
    std::string output_file;
    int sample_rate;
    int frame_size;
    
public:
    AudioStreamRecorder(const std::string& filename, int sr = 16000, int fs = 320) 
        : output_file(filename), sample_rate(sr), frame_size(fs) {
        
        // 初始化音频接口
        audio = CreateAudioInterface();
        if (!audio) {
            throw std::runtime_error("无法创建音频接口");
        }
        
        audio->Init();
        audio->SetConfig(sample_rate, frame_size, 1, 4, 4096, 1024);
        
        std::cout << "音频流录制器初始化完成" << std::endl;
        std::cout << "输出文件: " << output_file << std::endl;
        std::cout << "采样率: " << sample_rate << " Hz" << std::endl;
        std::cout << "帧大小: " << frame_size << " 样本" << std::endl;
    }
    
    void startRecording() {
        if (recording) {
            std::cout << "录制已在进行中" << std::endl;
            return;
        }
        
        std::cout << "开始录制音频流..." << std::endl;
        
        recording = true;
        writing = true;
        
        // 启动录制线程
        record_thread = std::thread(&AudioStreamRecorder::recordLoop, this);
        
        // 启动写入线程
        write_thread = std::thread(&AudioStreamRecorder::writeLoop, this);
        
        // 开始音频录制
        audio->Record();
    }
    
    void stopRecording() {
        if (!recording) {
            std::cout << "录制未在进行" << std::endl;
            return;
        }
        
        std::cout << "停止录制音频流..." << std::endl;
        
        recording = false;
        
        // 等待录制线程结束
        if (record_thread.joinable()) {
            record_thread.join();
        }
        
        // 停止写入线程
        writing = false;
        queue_cv.notify_all();
        
        if (write_thread.joinable()) {
            write_thread.join();
        }
        
        std::cout << "录制完成，文件保存为: " << output_file << std::endl;
    }
    
private:
    void recordLoop() {
        std::vector<short> buffer(frame_size);
        
        while (recording) {
            try {
                if (audio->Read(buffer.data(), frame_size)) {
                    // 将音频数据加入队列
                    {
                        std::lock_guard<std::mutex> lock(queue_mutex);
                        audio_queue.push(buffer);
                    }
                    queue_cv.notify_one();
                } else {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
            } catch (const std::exception& e) {
                std::cerr << "录制线程错误: " << e.what() << std::endl;
                break;
            }
        }
        
        std::cout << "录制线程结束" << std::endl;
    }
    
    void writeLoop() {
        // 创建WAV文件
        FILE* wav_file = fs.wavfopen(output_file.c_str(), "wb", 
                                   sample_rate, 1, 16);
        if (!wav_file) {
            std::cerr << "无法创建输出文件: " << output_file << std::endl;
            return;
        }
        
        size_t total_samples = 0;
        
        while (writing || !audio_queue.empty()) {
            std::unique_lock<std::mutex> lock(queue_mutex);
            
            // 等待音频数据或停止信号
            queue_cv.wait(lock, [this] { 
                return !audio_queue.empty() || !writing; 
            });
            
            // 处理队列中的所有数据
            while (!audio_queue.empty()) {
                auto audio_data = audio_queue.front();
                audio_queue.pop();
                lock.unlock();
                
                // 写入音频数据
                size_t written = fs.fwrite(audio_data.data(), 
                                          sizeof(short), audio_data.size());
                total_samples += written;
                
                lock.lock();
            }
        }
        
        fs.fclose();
        
        double duration = (double)total_samples / sample_rate;
        std::cout << "写入线程结束" << std::endl;
        std::cout << "总样本数: " << total_samples << std::endl;
        std::cout << "录制时长: " << duration << " 秒" << std::endl;
    }
    
public:
    ~AudioStreamRecorder() {
        if (recording) {
            stopRecording();
        }
    }
};

// 使用示例
int main() {
    try {
        AudioStreamRecorder recorder("stream_recording.wav", 16000, 320);
        
        std::cout << "按回车键开始录制..." << std::endl;
        std::cin.get();
        
        recorder.startRecording();
        
        std::cout << "录制中... 按回车键停止录制" << std::endl;
        std::cin.get();
        
        recorder.stopRecording();
        
        // 分析录制的文件
        AudioConverter::printFileInfo("stream_recording.wav");
        
    } catch (const std::exception& e) {
        std::cerr << "程序异常: " << e.what() << std::endl;
        return -1;
    }
    
    return 0;
}
```

## 错误处理和调试

### 常见错误处理

```cpp
#include "FileStream.h"
#include <iostream>
#include <stdexcept>

using namespace linx;

class SafeFileStream {
private:
    FileStream fs;
    FILE* current_file = nullptr;
    std::string current_filename;
    
public:
    ~SafeFileStream() {
        if (current_file) {
            fs.fclose();
        }
    }
    
    bool openFile(const std::string& filename, const std::string& mode) {
        try {
            // 关闭之前的文件
            if (current_file) {
                fs.fclose();
                current_file = nullptr;
            }
            
            current_file = fs.fopen(filename.c_str(), mode.c_str());
            if (!current_file) {
                std::cerr << "无法打开文件: " << filename 
                         << ", 模式: " << mode << std::endl;
                return false;
            }
            
            current_filename = filename;
            std::cout << "文件打开成功: " << filename << std::endl;
            return true;
            
        } catch (const std::exception& e) {
            std::cerr << "打开文件异常: " << e.what() << std::endl;
            return false;
        }
    }
    
    bool writeData(const void* data, size_t size, size_t count) {
        if (!current_file) {
            std::cerr << "文件未打开，无法写入" << std::endl;
            return false;
        }
        
        try {
            size_t written = fs.fwrite(data, size, count);
            if (written != count) {
                std::cerr << "写入数据不完整: 期望 " << count 
                         << ", 实际 " << written << std::endl;
                return false;
            }
            return true;
        } catch (const std::exception& e) {
            std::cerr << "写入数据异常: " << e.what() << std::endl;
            return false;
        }
    }
    
    bool readData(void* data, size_t size, size_t count, size_t& read_count) {
        if (!current_file) {
            std::cerr << "文件未打开，无法读取" << std::endl;
            return false;
        }
        
        try {
            read_count = fs.fread(data, size, count);
            return true;
        } catch (const std::exception& e) {
            std::cerr << "读取数据异常: " << e.what() << std::endl;
            return false;
        }
    }
    
    void closeFile() {
        if (current_file) {
            fs.fclose();
            current_file = nullptr;
            std::cout << "文件关闭: " << current_filename << std::endl;
            current_filename.clear();
        }
    }
    
    bool isOpen() const {
        return current_file != nullptr;
    }
    
    const std::string& getCurrentFilename() const {
        return current_filename;
    }
};
```

## 性能优化

### 缓冲区优化

```cpp
#include "FileStream.h"
#include <iostream>
#include <vector>
#include <chrono>

using namespace linx;

class BufferedFileStream {
private:
    FileStream fs;
    FILE* file = nullptr;
    
    std::vector<char> write_buffer;
    size_t buffer_size;
    size_t buffer_pos = 0;
    
public:
    BufferedFileStream(size_t buf_size = 64 * 1024) // 默认64KB缓冲区
        : buffer_size(buf_size), write_buffer(buf_size) {
    }
    
    bool open(const std::string& filename, const std::string& mode) {
        file = fs.fopen(filename.c_str(), mode.c_str());
        return file != nullptr;
    }
    
    bool bufferedWrite(const void* data, size_t size) {
        if (!file) return false;
        
        const char* src = static_cast<const char*>(data);
        size_t remaining = size;
        
        while (remaining > 0) {
            size_t space_left = buffer_size - buffer_pos;
            size_t to_copy = std::min(remaining, space_left);
            
            // 复制数据到缓冲区
            std::memcpy(write_buffer.data() + buffer_pos, src, to_copy);
            buffer_pos += to_copy;
            src += to_copy;
            remaining -= to_copy;
            
            // 如果缓冲区满了，刷新到文件
            if (buffer_pos == buffer_size) {
                if (!flushBuffer()) {
                    return false;
                }
            }
        }
        
        return true;
    }
    
    bool flushBuffer() {
        if (!file || buffer_pos == 0) return true;
        
        size_t written = fs.fwrite(write_buffer.data(), 1, buffer_pos);
        bool success = (written == buffer_pos);
        buffer_pos = 0;
        
        return success;
    }
    
    void close() {
        if (file) {
            flushBuffer();
            fs.fclose();
            file = nullptr;
        }
    }
    
    ~BufferedFileStream() {
        close();
    }
};

// 性能测试
void performanceTest() {
    const size_t test_data_size = 10 * 1024 * 1024; // 10MB
    std::vector<char> test_data(test_data_size, 'A');
    
    // 测试无缓冲写入
    auto start = std::chrono::high_resolution_clock::now();
    {
        FileStream fs;
        FILE* file = fs.fopen("test_unbuffered.dat", "wb");
        if (file) {
            for (size_t i = 0; i < test_data_size; i += 1024) {
                fs.fwrite(test_data.data() + i, 1, 1024);
            }
            fs.fclose();
        }
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto unbuffered_time = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    // 测试缓冲写入
    start = std::chrono::high_resolution_clock::now();
    {
        BufferedFileStream bfs(64 * 1024);
        if (bfs.open("test_buffered.dat", "wb")) {
            for (size_t i = 0; i < test_data_size; i += 1024) {
                bfs.bufferedWrite(test_data.data() + i, 1024);
            }
        }
    }
    end = std::chrono::high_resolution_clock::now();
    auto buffered_time = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    std::cout << "性能测试结果 (" << test_data_size / 1024 / 1024 << "MB数据):" << std::endl;
    std::cout << "无缓冲写入: " << unbuffered_time.count() << "ms" << std::endl;
    std::cout << "缓冲写入: " << buffered_time.count() << "ms" << std::endl;
    std::cout << "性能提升: " << (double)unbuffered_time.count() / buffered_time.count() << "x" << std::endl;
}
```

## 最佳实践

1. **文件操作安全**：
   - 始终检查文件打开是否成功
   - 使用RAII模式管理文件资源
   - 及时关闭文件句柄

2. **错误处理**：
   - 检查读写操作的返回值
   - 使用异常处理机制
   - 提供详细的错误信息

3. **性能优化**：
   - 使用适当大小的缓冲区
   - 批量处理数据
   - 避免频繁的小块读写

4. **内存管理**：
   - 合理分配缓冲区大小
   - 及时释放不需要的内存
   - 使用智能指针管理资源

5. **音频文件处理**：
   - 验证WAV文件格式的完整性
   - 处理不同的音频参数
   - 支持多种音频格式转换

6. **并发安全**：
   - 在多线程环境中保护文件操作
   - 使用线程安全的数据结构
   - 避免竞态条件

7. **调试和监控**：
   - 记录文件操作日志
   - 监控文件大小和性能
   - 提供详细的调试信息

8. **跨平台兼容性**：
   - 处理不同操作系统的路径分隔符
   - 考虑字节序问题
   - 使用标准C++库函数