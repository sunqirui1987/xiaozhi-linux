#ifdef __APPLE__

#include "PortAudioImpl.h"
#include <iostream>
#include <termios.h>
#include <unistd.h>

namespace linx {

// 设置终端为非规范模式，以便实时获取按键输入
void SetTerminalToNonCanonical() {
    struct termios t;
    tcgetattr(STDIN_FILENO, &t);
    t.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &t);
}

// 恢复终端为规范模式
void RestoreTerminalToCanonical() {
    struct termios t;
    tcgetattr(STDIN_FILENO, &t);
    t.c_lflag |= (ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &t);
}

// 检查是否按下了空格键
bool IsSpaceKeyPressed() {
    char ch;
    INFO("wait...");
    if (read(STDIN_FILENO, &ch, 1) == 1) {
        return ch == ' ';
    }
    return false;
}

PortAudioImpl::PortAudioImpl() : input_stream_(nullptr), output_stream_(nullptr) {
}

PortAudioImpl::~PortAudioImpl() {
    if (input_stream_) {
        Pa_CloseStream(input_stream_);
    }
    if (output_stream_) {
        Pa_CloseStream(output_stream_);
    }
    Pa_Terminate();
}

void PortAudioImpl::Init() {
    PaError err = Pa_Initialize();
    if (err != paNoError) {
        ERROR("PortAudio error: {}", Pa_GetErrorText(err));
        return;
    }
    INFO("PortAudio initialized successfully");
}

void PortAudioImpl::SetConfig(unsigned int sample_rate, int frame_size, int channels, 
                             int periods, int buffer_size, int period_size) {
    sample_rate_ = sample_rate;
    frame_size_ = frame_size;
    channels_ = channels;
    periods_ = periods;
    buffer_size_ = buffer_size;
    period_size_ = period_size;
    chunk_ = frame_size_ * 3;
}

bool PortAudioImpl::Read(short* buffer, size_t frame_size) {
    if (!input_stream_) {
        ERROR("Input stream not initialized");
        return false;
    }
    
    PaError err = Pa_ReadStream(input_stream_, buffer, frame_size);
    if (err != paNoError) {
        ERROR("PortAudio read error: {}", Pa_GetErrorText(err));
        return false;
    }
    return true;
}

bool PortAudioImpl::Write(short* buffer, size_t frame_size) {
    if (!output_stream_) {
        ERROR("Output stream not initialized");
        return false;
    }
    
    PaError err = Pa_WriteStream(output_stream_, buffer, frame_size);
    if (err != paNoError) {
        ERROR("PortAudio write error: {}", Pa_GetErrorText(err));
        return false;
    }
    return true;
}

void PortAudioImpl::Record() {
    PaStreamParameters inputParameters;
    inputParameters.device = Pa_GetDefaultInputDevice();
    if (inputParameters.device == paNoDevice) {
        ERROR("No default input device");
        return;
    }
    
    inputParameters.channelCount = channels_;
    inputParameters.sampleFormat = paInt16;
    inputParameters.suggestedLatency = Pa_GetDeviceInfo(inputParameters.device)->defaultLowInputLatency;
    inputParameters.hostApiSpecificStreamInfo = nullptr;
    
    PaError err = Pa_OpenStream(&input_stream_,
                               &inputParameters,
                               nullptr,
                               sample_rate_,
                               period_size_,
                               paClipOff,
                               nullptr,
                               nullptr);
    
    if (err != paNoError) {
        ERROR("PortAudio open input stream error: {}", Pa_GetErrorText(err));
        return;
    }
    
    err = Pa_StartStream(input_stream_);
    if (err != paNoError) {
        ERROR("PortAudio start input stream error: {}", Pa_GetErrorText(err));
        return;
    }
    
    INFO("Recording started");
}

void PortAudioImpl::Play() {
    PaStreamParameters outputParameters;
    outputParameters.device = Pa_GetDefaultOutputDevice();
    if (outputParameters.device == paNoDevice) {
        ERROR("No default output device");
        return;
    }
    
    outputParameters.channelCount = channels_;
    outputParameters.sampleFormat = paInt16;
    outputParameters.suggestedLatency = Pa_GetDeviceInfo(outputParameters.device)->defaultLowOutputLatency;
    outputParameters.hostApiSpecificStreamInfo = nullptr;
    
    PaError err = Pa_OpenStream(&output_stream_,
                               nullptr,
                               &outputParameters,
                               sample_rate_,
                               period_size_,
                               paClipOff,
                               nullptr,
                               nullptr);
    
    if (err != paNoError) {
        ERROR("PortAudio open output stream error: {}", Pa_GetErrorText(err));
        return;
    }
    
    err = Pa_StartStream(output_stream_);
    if (err != paNoError) {
        ERROR("PortAudio start output stream error: {}", Pa_GetErrorText(err));
        return;
    }
    
    INFO("Playback started");
}

int PortAudioImpl::RecordCallback(const void* inputBuffer, void* outputBuffer,
                                 unsigned long framesPerBuffer,
                                 const PaStreamCallbackTimeInfo* timeInfo,
                                 PaStreamCallbackFlags statusFlags,
                                 void* userData) {
    // Implementation for callback-based recording if needed
    return paContinue;
}

int PortAudioImpl::PlayCallback(const void* inputBuffer, void* outputBuffer,
                               unsigned long framesPerBuffer,
                               const PaStreamCallbackTimeInfo* timeInfo,
                               PaStreamCallbackFlags statusFlags,
                               void* userData) {
    // Implementation for callback-based playback if needed
    return paContinue;
}

}  // namespace linx

#endif  // __APPLE__