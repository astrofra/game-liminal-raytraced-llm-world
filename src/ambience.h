#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace liminal {
constexpr size_t kAmbienceTracks = 4;
using AudioGains = std::array<float, kAmbienceTracks>;
inline constexpr std::array<const char*, kAmbienceTracks> kAmbienceIds = {"blade", "bug", "night", "static"};

struct AmbienceTrack {
    std::string path, sha256, description, annotation;
    float trim_db = 0, max_gain = 1;
};
struct AmbienceCatalog {
    std::string version;
    std::array<AmbienceTrack, kAmbienceTracks> tracks;
};
struct AudioMixDecision {
    bool valid = false;
    AudioGains targets{};
    AudioGains previous_targets{};
    std::string requested_json = "null", reason = "no_narrative_audio", catalog_version;
};

bool LoadAmbienceCatalog(const std::string& filename, AmbienceCatalog* output, std::string* error);
std::string AudioGainsJson(const AudioGains& gains);
bool ParseAudioGains(const std::string& text, const AmbienceCatalog& catalog, AudioGains* output, std::string* error);
AudioMixDecision ValidateAudioMix(const std::string& response, const AmbienceCatalog& catalog);
std::string AmbiencePrompt(const AmbienceCatalog& catalog, const AudioGains& targets,
                           const AudioGains* current = nullptr);
std::string AudioMixSchema(const AmbienceCatalog& catalog);
std::string AudioDecisionJson(const AudioMixDecision& decision, bool committed);

// Pure sample-clock mixer. Ownership is the audio thread; the SDL adapter serializes
// target/snapshot access with the stream lock. No decoder or allocation in Render.
class AmbienceMixer {
public:
    bool Initialize(std::array<std::vector<float>, kAmbienceTracks> stereo_pcm,
                    const AmbienceCatalog& catalog, int sample_rate, std::string* error);
    bool SetTargets(const AudioGains& targets, double ramp_seconds);
    void Render(float* interleaved_stereo, size_t frames);
    const AudioGains& Current() const { return current_; }
    const AudioGains& Targets() const { return target_; }
    const std::array<size_t, kAmbienceTracks>& Positions() const { return position_; }
    uint64_t Frames() const { return frames_; }
    float MasterGain() const { return master_; }
private:
    std::array<std::vector<float>, kAmbienceTracks> pcm_;
    std::array<size_t, kAmbienceTracks> position_{};
    AudioGains current_{}, target_{}, start_{}, trim_{}, max_gain_{};
    uint64_t ramp_frames_ = 0, ramp_position_ = 0, frames_ = 0;
    int sample_rate_ = 48000;
    float master_ = 0;
};
}
