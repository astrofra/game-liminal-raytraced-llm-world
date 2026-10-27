#include "ambience_audio.h"
#include "resource_paths.h"
#include "nlohmann/json.hpp"
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>

namespace {
void Word(std::ostream& out,uint32_t value,int bytes) {
    for(int i=0;i<bytes;++i)out.put(static_cast<char>((value>>(i*8))&255));
}
// Offline diagnostic sequence: all tracks, each solo, retarget during a ramp,
// all at maximum, silence. Output has the same sample-clock mixing as SDL.
bool Wave(const std::string& path,const liminal::AmbienceCatalog& catalog,std::string* error) {
    std::array<std::vector<float>,liminal::kAmbienceTracks> pcm;
    liminal::AmbienceMixer mixer;
    if(!liminal::DecodeAmbience(catalog,&pcm,error)||!mixer.Initialize(std::move(pcm),catalog,48000,error))return false;
    std::ofstream out(std::filesystem::u8path(path),std::ios::binary);
    if(!out){*error="cannot create WAV";return false;}
    constexpr uint32_t frames=48000*48,bytes=frames*4;
    out.write("RIFF",4);Word(out,36+bytes,4);out.write("WAVEfmt ",8);Word(out,16,4);
    Word(out,1,2);Word(out,2,2);Word(out,48000,4);Word(out,48000*4,4);Word(out,4,2);Word(out,16,2);
    out.write("data",4);Word(out,bytes,4);
    float peak=0;std::array<float,960> buffer{};
    for(uint32_t frame=0;frame<frames;frame+=480) {
        if(frame%(48000*6)==0) {
            const auto step=frame/(48000*6);liminal::AudioGains gains{};
            if(step==0||step==6)for(size_t i=0;i<4;++i)gains[i]=catalog.tracks[i].max_gain;
            else if(step<=4)gains[step-1]=catalog.tracks[step-1].max_gain;
            mixer.SetTargets(gains,5);
        }
        if(frame==48000*32) {
            liminal::AudioGains gains{};for(size_t i=0;i<4;++i)gains[i]=(.1f*static_cast<float>(i+1))*catalog.tracks[i].max_gain;
            mixer.SetTargets(gains,5);
        }
        mixer.Render(buffer.data(),480);
        for(float sample:buffer){peak=std::max(peak,std::abs(sample));Word(out,static_cast<uint16_t>(static_cast<int16_t>(std::lround(sample*32767))),2);}
    }
    if(!out){*error="WAV write failed";return false;}
    std::cout<<nlohmann::json{{"wav",path},{"seconds",48},{"peak",peak},{"master_gain",mixer.MasterGain()},
        {"catalog_version",catalog.version}}.dump()<<"\n";
    return true;
}
}
int main(int argc,char** argv) {
    double seconds=0,ramp=5;std::string wav;
    try {
        for(int i=1;i<argc;++i) {
            const std::string arg=argv[i];
            if(arg=="--seconds"&&i+1<argc)seconds=std::stod(argv[++i]);
            else if(arg=="--ramp"&&i+1<argc)ramp=std::stod(argv[++i]);
            else if(arg=="--wav"&&i+1<argc)wav=argv[++i];
            else {std::cout<<"liminal_audio_diagnostic [--seconds N] [--ramp 0..60] [--wav output.wav]\n"
                "Interactive: four gains (blade bug night static), or q. No LLM.\n";return arg=="--help"?0:1;}
        }
        if(!std::isfinite(seconds)||seconds<0||seconds>600||!std::isfinite(ramp)||ramp<0||ramp>60)throw std::runtime_error("invalid duration");
        liminal::AmbienceCatalog catalog;std::string error;
        if(!liminal::LoadAmbienceCatalog(liminal::ResourcePath("assets/sfx/ambience_features.json"),&catalog,&error))throw std::runtime_error(error);
        if(!wav.empty()){if(!Wave(wav,catalog,&error))throw std::runtime_error(error);return 0;}
        liminal::AmbienceAudio audio;
        if(!audio.Open(catalog,&error))throw std::runtime_error(error);
        std::cout<<audio.SnapshotJson("started")<<"\n";
        if(seconds>0) {
            liminal::AudioGains gains{};for(size_t i=0;i<4;++i)gains[i]=catalog.tracks[i].max_gain;
            audio.SetTargets(gains,ramp);
            SDL_Delay(static_cast<Uint32>(seconds*1000));
        } else {
            std::cout<<"Four gains in [0,1]: blade bug night static; q quits. Ramp "<<ramp<<" s.\n";
            std::string line;
            while(std::getline(std::cin,line)&&line!="q") {
                std::istringstream input(line);liminal::AudioGains gains{};std::string extra;
                if(!(input>>gains[0]>>gains[1]>>gains[2]>>gains[3])||(input>>extra)||!audio.SetTargets(gains,ramp))
                    std::cerr<<"Invalid gains; previous mix retained.\n";
                std::cout<<audio.SnapshotJson("manual_target")<<"\n";
            }
        }
        const auto snapshot=audio.SnapshotJson("stopped");std::cout<<snapshot<<"\n";
        const auto data=nlohmann::json::parse(snapshot);
        return data.value("callback_failed",false)||(seconds>0&&data.value("frames",0)==0)?1:0;
    } catch(const std::exception& e) {std::cerr<<"Audio diagnostic: "<<e.what()<<"\n";return 1;}
}
