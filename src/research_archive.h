#ifndef LIMINAL_RESEARCH_ARCHIVE_H
#define LIMINAL_RESEARCH_ARCHIVE_H

#include "turn_runner.h"
#include <filesystem>
#include <functional>
#include <map>
#include <atomic>

namespace liminal {

// Write-only research output. Never used as an input to a new game.
class ResearchArchive {
public:
    bool Start(const std::string& root, const SessionState& state,
               const LlmGenerationConfig& model, const RenderConfig& render);
    bool Image(const SessionState& state, const std::vector<unsigned char>& pixels);
    bool Command(const std::string& command, const SessionState* before = nullptr);
    bool InitialPresentation(const SessionState& state);
    decltype(LlmGenerationConfig::research_trace) CallTrace();
    bool Presentation(const SessionState& state, const std::string& audio_snapshot,
                      const std::string& displayed, uint64_t generation, int frame,
                      bool presented, uint64_t elapsed_ms, int width, int height, int pitch, const void* rgb,
                      const std::string& capture_error = {});
    bool Turn(const SessionState& state, const HeadlessTurnResult& turn,
              const std::vector<unsigned char>& pixels);
    bool Failure(const std::string& error, const HeadlessTurnResult& turn);
    bool Close(const SessionState& state);
    bool AudioEvent(const std::string& event_json);
    const std::string& Error() const { return error_; }
    std::string Path() const { return path_.u8string(); }
    bool Pending() const { return pending_; }

private:
    bool Guard(const std::function<void()>& action);
    void Write(const std::string& name, const std::string& text, bool append=false);
    void Snapshot(const SessionState& state);
    void Place(const SessionState& state);
    void WriteImage(const SessionState& state, const std::vector<unsigned char>& pixels);
    void Result(const std::string& status, const HeadlessTurnResult& turn, const std::string& error);
    std::string InteractionPath() const;
    void InteractionState(const std::string& name, const SessionState& state);
    std::filesystem::path path_;
    std::map<std::string,std::string> places_;
    std::string error_, command_, last_visual_reference_;
    RenderConfig render_;
    unsigned int command_index_=0;
    bool pending_=false;
    bool record_presentation_=false;
    bool interaction_ok_=true;
    int expected_calls_=0;
    std::atomic<int> recorded_calls_{0};
    std::atomic<bool> calls_ok_{true};
    std::string interaction_status_, effective_scene_reference_, effective_scene_hash_;
};

} // namespace liminal
#endif
