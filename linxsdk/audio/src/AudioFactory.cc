#include "AudioInterface.h"

#ifdef __APPLE__
#include "PortAudioImpl.h"
#else
#include "AlsaAudio.h"
#endif

namespace linx {

std::unique_ptr<AudioInterface> CreateAudioInterface() {
#ifdef __APPLE__
    return std::make_unique<PortAudioImpl>();
#else
    return std::make_unique<AlsaAudio>();
#endif
}

}  // namespace linx