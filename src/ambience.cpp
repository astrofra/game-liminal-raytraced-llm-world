#include "ambience.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <set>
#include <stdexcept>
#include "nlohmann/json.hpp"

namespace liminal {
using nlohmann::json;
namespace {
bool Fail(std::string* error, const std::string& why) { if(error)*error=why; return false; }
// Reject duplicate keys, including repeated IDs, instead of silently taking the last.
json StrictJson(const std::string& text) {
    std::vector<std::set<std::string>> keys;
    return json::parse(text,[&](int, json::parse_event_t event, json& value) {
        if(event==json::parse_event_t::object_start)keys.emplace_back();
        if(event==json::parse_event_t::key&&!keys.back().insert(value.get<std::string>()).second)
            throw std::runtime_error("duplicate JSON key");
        if(event==json::parse_event_t::object_end)keys.pop_back();
        return true;
    });
}
bool ReadGains(const json& data,const AmbienceCatalog& catalog,AudioGains* output,std::string* error) {
    if(!data.is_object()||data.size()!=kAmbienceTracks)return Fail(error,"expected exactly four audio IDs");
    AudioGains gains{};
    for(size_t i=0;i<gains.size();++i) {
        const auto entry=data.find(kAmbienceIds[i]);
        if(entry==data.end()||!entry->is_number())return Fail(error,"missing, unknown or nonnumeric audio ID");
        const double value=entry->get<double>();
        if(!std::isfinite(value)||value<0||value>catalog.tracks[i].max_gain)
            return Fail(error,"audio gain outside allowed bounds");
        gains[i]=static_cast<float>(value);
    }
    *output=gains;return true;
}
}
bool LoadAmbienceCatalog(const std::string& filename,AmbienceCatalog* output,std::string* error) {
    try {
        const auto path=std::filesystem::u8path(filename);
        if(std::filesystem::file_size(path)>1024*1024)throw std::runtime_error("audio catalog exceeds 1 MiB");
        std::ifstream input(path,std::ios::binary);
        const std::string text((std::istreambuf_iterator<char>(input)),{});
        const auto root=StrictJson(text);
        if(root.at("schema_version")!=1)throw std::runtime_error("unsupported audio catalog schema");
        AmbienceCatalog candidate;candidate.version=root.at("catalog_version").get<std::string>();
        const auto hex=[](const std::string& s){return s.size()==64&&s.find_first_not_of("0123456789abcdef")==std::string::npos;};
        if(!hex(candidate.version))throw std::runtime_error("invalid catalog version");
        const auto& tracks=root.at("tracks");
        if(!tracks.is_array()||tracks.size()!=kAmbienceTracks)throw std::runtime_error("expected four catalog tracks");
        std::set<std::string> seen;
        for(const auto& source:tracks) {
            const auto id=source.at("id").get<std::string>();
            const auto found=std::find(kAmbienceIds.begin(),kAmbienceIds.end(),id);
            if(found==kAmbienceIds.end()||!seen.insert(id).second)throw std::runtime_error("unknown/duplicate catalog ID");
            auto& track=candidate.tracks[static_cast<size_t>(found-kAmbienceIds.begin())];
            track.path=source.at("path").get<std::string>();
            if(track.path!="assets/sfx/"+id+"-loop.ogg")throw std::runtime_error("unexpected audio asset path");
            track.sha256=source.at("sha256").get<std::string>();
            if(!hex(track.sha256))throw std::runtime_error("invalid audio SHA256");
            track.description=source.at("description").get<std::string>();
            if(track.description.empty()||track.description.size()>1600)throw std::runtime_error("invalid audio card size");
            if(source.contains("annotation")&&!source["annotation"].is_null()) {
                const auto& annotation=source["annotation"];
                if(annotation.at("provenance")!="human")throw std::runtime_error("annotation must be human");
                track.annotation=annotation.at("text").get<std::string>();
                if(track.annotation.size()>1600)throw std::runtime_error("audio annotation too long");
            }
            const auto& playback=source.at("playback");
            track.trim_db=playback.at("trim_db").get<float>();track.max_gain=playback.at("max_gain").get<float>();
            if(!std::isfinite(track.trim_db)||track.trim_db < -24||track.trim_db > 6||
               !std::isfinite(track.max_gain)||track.max_gain<0||track.max_gain>1||
               (track.trim_db!=0&&!playback.at("trim_validated").get<bool>()))
                throw std::runtime_error("invalid/unvalidated audio playback settings");
        }
        *output=std::move(candidate);return true;
    } catch(const std::exception& e) { return Fail(error,e.what()); }
}
std::string AudioGainsJson(const AudioGains& gains) {
    json data=json::object();for(size_t i=0;i<gains.size();++i)data[kAmbienceIds[i]]=gains[i];return data.dump();
}
bool ParseAudioGains(const std::string& text,const AmbienceCatalog& catalog,AudioGains* output,std::string* error) {
    try { return ReadGains(StrictJson(text),catalog,output,error); }
    catch(const std::exception& e) {return Fail(error,e.what());}
}
AudioMixDecision ValidateAudioMix(const std::string& response,const AmbienceCatalog& catalog) {
    AudioMixDecision decision;decision.catalog_version=catalog.version;
    try {
        const auto data=StrictJson(response);
        if(!data.is_object()||!data.contains("audio_mix")){decision.reason="audio_mix_absent";return decision;}
        decision.requested_json=data["audio_mix"].dump();
        decision.valid=ReadGains(data["audio_mix"],catalog,&decision.targets,&decision.reason);
        if(decision.valid)decision.reason="validated";
    } catch(const std::exception& e) {decision.reason=e.what();}
    return decision;
}
std::string AmbiencePrompt(const AmbienceCatalog& catalog,const AudioGains& targets,const AudioGains* current) {
    json cards=json::array();
    for(size_t i=0;i<kAmbienceTracks;++i) {
        const auto& track=catalog.tracks[i];
        cards.push_back({{"id",kAmbienceIds[i]},{"acoustics",track.description},
            {"human_annotation",track.annotation},{"max_gain",track.max_gain}});
    }
    return json{{"catalog_version",catalog.version},{"tracks",cards},{"current_targets",json::parse(AudioGainsJson(targets))},
        {"current_gains",current?json::parse(AudioGainsJson(*current)):json(nullptr)},
        {"rule","Propose audio_mix with exactly blade, bug, night, static gains in [0,max_gain]. Gains are independent, NOT probabilities; they need not sum to one. You may preserve targets or silence every track. Associate measured sound qualities freely with the current place and validated situation. IDs are arbitrary labels, NOT sound descriptions. Audio is artistic expression, never evidence of creatures, objects, topology or events. Five-second continuous transitions are controlled by the engine."}}.dump();
}
std::string AudioMixSchema(const AmbienceCatalog& catalog) {
    json schema={{"type","object"},{"properties",json::object()},{"required",kAmbienceIds},{"additionalProperties",false}};
    for(size_t i=0;i<kAmbienceTracks;++i)schema["properties"][kAmbienceIds[i]]={{"type","number"},{"minimum",0},{"maximum",catalog.tracks[i].max_gain}};
    return schema.dump();
}
std::string AudioDecisionJson(const AudioMixDecision& decision,bool committed) {
    return json{{"catalog_version",decision.catalog_version},{"requested",json::parse(decision.requested_json,nullptr,false)},
        {"valid",decision.valid},{"committed",committed&&decision.valid},{"reason",decision.reason},
        {"previous_targets",json::parse(AudioGainsJson(decision.previous_targets))},
        {"retained_targets",json::parse(AudioGainsJson(committed&&decision.valid?decision.targets:decision.previous_targets))},
        {"validated_targets",decision.valid?json::parse(AudioGainsJson(decision.targets)):json(nullptr)}}.dump();
}
bool AmbienceMixer::Initialize(std::array<std::vector<float>,kAmbienceTracks> pcm,
                              const AmbienceCatalog& catalog,int sample_rate,std::string* error) {
    if(sample_rate<8000||sample_rate>192000)return Fail(error,"invalid mixer sample rate");
    float worst=0;
    for(size_t i=0;i<pcm.size();++i) {
        if(pcm[i].empty()||pcm[i].size()%2)return Fail(error,"empty or incomplete stereo PCM");
        float peak=0;
        for(float value:pcm[i]) {
            if(!std::isfinite(value))return Fail(error,"nonfinite PCM sample");
            peak=std::max(peak,std::abs(value));
        }
        const auto& track=catalog.tracks[i];
        if(!std::isfinite(track.trim_db)||track.trim_db < -24||track.trim_db>6||
           !std::isfinite(track.max_gain)||track.max_gain<0||track.max_gain>1)return Fail(error,"invalid mixer gains");
        trim_[i]=std::pow(10.f,track.trim_db/20.f);max_gain_[i]=track.max_gain;
        worst+=peak*trim_[i]*max_gain_[i];
    }
    // Conservative worst-case coherent sum, including Vorbis floating-point overs.
    master_=worst>0?std::min(.25f,.8f/worst):.25f;
    sample_rate_=sample_rate;pcm_=std::move(pcm);position_={};current_={};target_={};start_={};
    ramp_frames_=ramp_position_=frames_=0;return true;
}
bool AmbienceMixer::SetTargets(const AudioGains& gains,double seconds) {
    if(!std::isfinite(seconds)||seconds<0||seconds>60)return false;
    for(size_t i=0;i<gains.size();++i)if(!std::isfinite(gains[i])||gains[i]<0||gains[i]>max_gain_[i])return false;
    if(gains==target_)return true; // Read-only turns must not lengthen an existing ramp.
    start_=current_;target_=gains;ramp_position_=0;
    ramp_frames_=static_cast<uint64_t>(std::llround(seconds*sample_rate_));
    if(!ramp_frames_)current_=target_;
    return true;
}
void AmbienceMixer::Render(float* output,size_t frames) {
    for(size_t frame=0;frame<frames;++frame) {
        if(ramp_position_<ramp_frames_) {
            ++ramp_position_;const double fraction=static_cast<double>(ramp_position_)/ramp_frames_;
            for(size_t i=0;i<kAmbienceTracks;++i)current_[i]=static_cast<float>(start_[i]+(target_[i]-start_[i])*fraction);
        }
        float left=0,right=0;
        for(size_t i=0;i<kAmbienceTracks;++i) {
            if(pcm_[i].empty())continue;
            const auto at=position_[i]*2;const float gain=current_[i]*trim_[i]*master_;
            left+=pcm_[i][at]*gain;right+=pcm_[i][at+1]*gain;
            position_[i]=(position_[i]+1)%(pcm_[i].size()/2);
        }
        output[frame*2]=std::clamp(left,-.98f,.98f);output[frame*2+1]=std::clamp(right,-.98f,.98f);
    }
    frames_+=frames;
}
}
