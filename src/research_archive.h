#ifndef LIMINAL_RESEARCH_ARCHIVE_H
#define LIMINAL_RESEARCH_ARCHIVE_H

#include "turn_runner.h"
#include <filesystem>
#include <functional>
#include <map>

namespace liminal {

// Write-only research output. Never used as an input to a new game.
class ResearchArchive {
public:
    bool Start(const std::string& root, const SessionState& state,
               const LlmGenerationConfig& model, const RenderConfig& render);
    bool Image(const SessionState& state, const std::vector<unsigned char>& pixels);
    bool Command(const std::string& command);
    bool Turn(const SessionState& state, const HeadlessTurnResult& turn,
              const std::vector<unsigned char>& pixels);
    bool Failure(const std::string& error, const HeadlessTurnResult& turn);
    bool Close(const SessionState& state);
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
    std::filesystem::path path_;
    std::map<std::string,std::string> places_;
    std::string error_, command_, last_visual_reference_;
    RenderConfig render_;
    unsigned int command_index_=0;
    bool pending_=false;
};

} // namespace liminal
#endif
