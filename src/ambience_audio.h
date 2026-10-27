#pragma once
#include "ambience.h"
#include <SDL3/SDL.h>

namespace liminal {
class AmbienceAudio {
public:
    ~AmbienceAudio() { Close(); }
    AmbienceAudio() = default;
    AmbienceAudio(const AmbienceAudio&) = delete;
    AmbienceAudio& operator=(const AmbienceAudio&) = delete;
    bool Open(const AmbienceCatalog& catalog, std::string* error);
    void Close();
    bool SetTargets(const AudioGains& gains, double seconds = 5);
    AudioGains Current();
    std::string SnapshotJson(const std::string& event);
    bool Active() const { return stream_ != nullptr; }
private:
    static void SDLCALL Callback(void* user, SDL_AudioStream* stream, int additional, int total);
    SDL_AudioStream* stream_ = nullptr;
    AmbienceMixer mixer_;
    std::string version_;
    bool callback_failed_ = false, initialized_ = false;
};
// Decode and resample entire assets before playback; bounded, no work on the audio thread.
bool DecodeAmbience(const AmbienceCatalog& catalog,
                    std::array<std::vector<float>, kAmbienceTracks>* pcm, std::string* error);
}
