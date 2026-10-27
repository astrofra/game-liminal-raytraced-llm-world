#include "ambience.h"
#include "place_proposal.h"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include "nlohmann/json.hpp"
using namespace liminal;
using nlohmann::json;
static void Check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
int main() {
    try {
        AmbienceCatalog catalog;catalog.version="test";std::string why;
        const std::string valid=R"({"audio_mix":{"blade":1,"bug":1,"night":1,"static":1}})";
        Check(ValidateAudioMix(valid,catalog).valid,"gains need not sum to one");
        for(const auto* text:{R"({})",R"({"audio_mix":null})",R"({"audio_mix":{"blade":-1,"bug":0,"night":0,"static":0}})",
            R"({"audio_mix":{"blade":2,"bug":0,"night":0,"static":0}})",R"({"audio_mix":{"blade":true,"bug":0,"night":0,"static":0}})",
            R"({"audio_mix":{"blade":0,"bug":0,"night":0,"alien":0}})",R"({"audio_mix":{"blade":0,"blade":1,"bug":0,"night":0,"static":0}})",
            R"({"audio_mix":{"blade":1e999,"bug":0,"night":0,"static":0}})"})
            Check(!ValidateAudioMix(text,catalog).valid,"invalid audio accepted");
        AudioGains unchanged{.2f,.3f,.4f,.5f};auto old=unchanged;
        Check(!ParseAudioGains("{}",catalog,&unchanged,&why)&&unchanged==old,"invalid gains mutate output");
        catalog.tracks[0].max_gain=.4f;Check(!ValidateAudioMix(valid,catalog).valid,"per-track limit ignored");catalog.tracks[0].max_gain=1;
        const auto schema=json::parse(AudioMixSchema(catalog));Check(schema["required"].size()==4,"incomplete audio schema");
        const auto prompt=json::parse(AmbiencePrompt(catalog,old,&old));Check(prompt["current_gains"]["bug"].get<float>()==old[1],"current gains missing");

        // Bad audio must not poison a valid mechanical proposal.
        auto proposal=json::parse(R"({"title":"Quarry","summary":"A quarry.","narration":"You stand here.","thermal":{"external_temperature_c":464,"body_temperature_c":37,"reason":"stable"},"exits":{"north":true,"east":false,"south":false,"west":false}})");
        proposal["audio_mix"]={{"alien",-100}};PlaceProposal place;
        Check(ParsePlaceProposal(proposal.dump(),464,37,{}, {"north"},&place,&why),"audio rejected mechanical proposal");
        Check(!ValidateAudioMix(proposal.dump(),catalog).valid,"invalid audio not isolated");

        std::array<std::vector<float>,4> pcm;
        for(auto& track:pcm)track={1,1,.5f,.5f,-1,-1};
        AmbienceMixer mixer;Check(mixer.Initialize(pcm,catalog,8000,&why),"mixer init");
        std::vector<float> buffer(16000);
        mixer.Render(buffer.data(),7);Check(mixer.Positions()[0]==1,"silent loops stopped");
        for(float v:buffer)Check(v==0,"startup not silent");
        Check(mixer.SetTargets({1,1,1,1},1),"target rejected");
        mixer.Render(buffer.data(),4000);Check(std::abs(mixer.Current()[0]-.5f)<1e-6,"half ramp incorrect");
        auto position=mixer.Positions();
        Check(mixer.SetTargets({},1),"retarget rejected");
        Check(mixer.Current()[0]==.5f&&mixer.Positions()==position,"retarget jumps or restarts");
        mixer.Render(buffer.data(),1);Check(mixer.Current()[0]<.5f&&mixer.Current()[0]>.499f,"retarget discontinuity");
        mixer.Render(buffer.data(),7999);Check(mixer.Current()[0]==0,"ramp does not finish");
        auto invalid=old;invalid[2]=std::numeric_limits<float>::infinity();
        Check(!mixer.SetTargets(invalid,1)&&mixer.Targets()==AudioGains{},"nonfinite target changed state");
        for(auto& track:catalog.tracks)track.trim_db=6;
        for(auto& track:pcm)track={2,2,2,2}; // Overshoot in a floating Vorbis decode.
        Check(mixer.Initialize(pcm,catalog,8000,&why)&&mixer.SetTargets({1,1,1,1},0),"coherent test init");
        mixer.Render(buffer.data(),8000);
        for(float value:buffer)Check(std::isfinite(value)&&std::abs(value)<=.800001f,"coherent sum clips");
        // Same duration split into arbitrary callback chunks produces the same gains/positions.
        AmbienceMixer a,b;Check(a.Initialize(pcm,catalog,8000,&why)&&b.Initialize(pcm,catalog,8000,&why),"chunk test init");
        a.SetTargets(old,.75);b.SetTargets(old,.75);
        a.Render(buffer.data(),8000);for(int i=0;i<80;++i)b.Render(buffer.data(),100);
        Check(a.Current()==b.Current()&&a.Positions()==b.Positions(),"ramp depends on callback chunk size");
        std::cout<<"Audio validation, independent proposals, ramps, silence, cursor continuity and coherent headroom passed.\n";
        return 0;
    } catch(const std::exception& e) {std::cerr<<e.what()<<"\n";return 1;}
}
