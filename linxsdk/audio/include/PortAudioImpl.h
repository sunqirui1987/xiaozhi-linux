#pragma once

#ifdef __APPLE__

#include "AudioInterface.h"
#include <portaudio.h>
#include <vector>
#include "Log.h"
#include "FileStream.h"

namespace linx {

class PortAudioImpl : public AudioInterface {
public:
    PortAudioImpl();
    ~PortAudioImpl() override;
    
    void Init() override;
    void SetConfig(unsigned int sample_rate, int frame_size, int channels, 
                  int periods, int buffer_size, int period_size) override;
    bool Read(short* buffer, size_t frame_size) override;
    bool Write(short* buffer, size_t frame_size) override;
    void Record() override;
    void Play() override;

private:
    PaStream* input_stream_;
    PaStream* output_stream_;
    std::vector<short> audio_data_;
    
    unsigned int sample_rate_ = 16000;
    int frame_size_ = 320;
    int channels_ = 1;
    int chunk_ = frame_size_ * 3;
    int periods_ = 4;
    int buffer_size_ = 4096;
    int period_size_ = 1024;
    
    static int RecordCallback(const void* inputBuffer, void* outputBuffer,
                             unsigned long framesPerBuffer,
                             const PaStreamCallbackTimeInfo* timeInfo,
                             PaStreamCallbackFlags statusFlags,
                             void* userData);
                             
    static int PlayCallback(const void* inputBuffer, void* outputBuffer,
                           unsigned long framesPerBuffer,
                           const PaStreamCallbackTimeInfo* timeInfo,
                           PaStreamCallbackFlags statusFlags,
                           void* userData);
};

}  // namespace linx

#endif  // __APPLE__