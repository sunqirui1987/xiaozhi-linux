#pragma once

#include <vector>
#include <memory>

namespace linx {

class AudioInterface {
public:
    virtual ~AudioInterface() = default;
    
    virtual void Init() = 0;
    virtual void SetConfig(unsigned int sample_rate, int frame_size, int channels, 
                          int periods, int buffer_size, int period_size) = 0;
    virtual bool Read(short* buffer, size_t frame_size) = 0;
    virtual bool Write(short* buffer, size_t frame_size) = 0;
    virtual void Record() = 0;
    virtual void Play() = 0;
};

// Factory function to create platform-specific audio implementation
std::unique_ptr<AudioInterface> CreateAudioInterface();

}  // namespace linx