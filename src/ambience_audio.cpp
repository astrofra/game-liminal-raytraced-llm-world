#include "ambience_audio.h"
#include "resource_paths.h"
#include "scene_hash.h"
#include <algorithm>
#include <fstream>
#include <memory>
#include <stdexcept>
#include "nlohmann/json.hpp"
#define STB_VORBIS_NO_STDIO
#define STB_VORBIS_NO_PUSHDATA_API
#ifdef _MSC_VER
#pragma warning(push, 0)
#endif
#include "stb_vorbis.c"
#ifdef _MSC_VER
#pragma warning(pop)
#endif

namespace liminal {
namespace { constexpr int kRate=48000; constexpr size_t kChunkFrames=1024; }
bool DecodeAmbience(const AmbienceCatalog& catalog,std::array<std::vector<float>,kAmbienceTracks>* pcm,std::string* error) {
    try {
        std::array<std::vector<float>,kAmbienceTracks> decoded;
        for(size_t i=0;i<kAmbienceTracks;++i) {
            const auto path=std::filesystem::u8path(ResourcePath(catalog.tracks[i].path));
            if(std::filesystem::file_size(path)>16*1024*1024)throw std::runtime_error("OGG exceeds 16 MiB");
            std::ifstream file(path,std::ios::binary);
            const std::string bytes((std::istreambuf_iterator<char>(file)),{});
            if(SceneTextHash(bytes)!=catalog.tracks[i].sha256)throw std::runtime_error("audio SHA256 mismatch: "+catalog.tracks[i].path);
            int code=0;
            std::unique_ptr<stb_vorbis,decltype(&stb_vorbis_close)> decoder(
                stb_vorbis_open_memory(reinterpret_cast<const unsigned char*>(bytes.data()),static_cast<int>(bytes.size()),&code,nullptr),stb_vorbis_close);
            if(!decoder)throw std::runtime_error("Vorbis decode open error "+std::to_string(code));
            const auto info=stb_vorbis_get_info(decoder.get());
            const auto frames=stb_vorbis_stream_length_in_samples(decoder.get());
            if((info.channels!=1&&info.channels!=2)||info.sample_rate<8000||info.sample_rate>96000||
               !frames||frames>info.sample_rate*120u)throw std::runtime_error("unsupported Vorbis format/duration (mono/stereo, <=120s)");
            std::vector<float> native(static_cast<size_t>(frames)*info.channels);
            const int actual=stb_vorbis_get_samples_float_interleaved(decoder.get(),info.channels,native.data(),static_cast<int>(native.size()));
            if(actual!=static_cast<int>(frames)||stb_vorbis_get_error(decoder.get())!=VORBIS__no_error)
                throw std::runtime_error("incomplete Vorbis audio");
            const SDL_AudioSpec source={SDL_AUDIO_F32,info.channels,static_cast<int>(info.sample_rate)};
            const SDL_AudioSpec destination={SDL_AUDIO_F32,2,kRate};
            Uint8* buffer=nullptr;int length=0;
            if(!SDL_ConvertAudioSamples(&source,reinterpret_cast<const Uint8*>(native.data()),
                static_cast<int>(native.size()*sizeof(float)),&destination,&buffer,&length))throw std::runtime_error(SDL_GetError());
            std::unique_ptr<void,decltype(&SDL_free)> converted(buffer,SDL_free);
            if(length<=0||length%static_cast<int>(2*sizeof(float)))throw std::runtime_error("invalid converted PCM");
            const auto* floats=reinterpret_cast<const float*>(buffer);
            decoded[i].assign(floats,floats+length/sizeof(float));
        }
        *pcm=std::move(decoded);return true;
    } catch(const std::exception& e) {if(error)*error=e.what();return false;}
}
bool AmbienceAudio::Open(const AmbienceCatalog& catalog,std::string* error) {
    Close();
    if(!SDL_InitSubSystem(SDL_INIT_AUDIO)){if(error)*error=SDL_GetError();return false;}
    initialized_=true;
    std::array<std::vector<float>,kAmbienceTracks> pcm;
    if(!DecodeAmbience(catalog,&pcm,error)||!mixer_.Initialize(std::move(pcm),catalog,kRate,error)){Close();return false;}
    version_=catalog.version;callback_failed_=false;
    const SDL_AudioSpec spec={SDL_AUDIO_F32,2,kRate};
    stream_=SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,&spec,Callback,this);
    if(!stream_||!SDL_ResumeAudioStreamDevice(stream_)) {if(error)*error=SDL_GetError();Close();return false;}
    return true;
}
void AmbienceAudio::Close() {
    if(stream_){SDL_DestroyAudioStream(stream_);stream_=nullptr;}
    if(initialized_){SDL_QuitSubSystem(SDL_INIT_AUDIO);initialized_=false;}
}
void SDLCALL AmbienceAudio::Callback(void* user,SDL_AudioStream* stream,int additional,int) {
    auto* self=static_cast<AmbienceAudio*>(user);
    std::array<float,kChunkFrames*2> scratch{};
    // SDL asks in source bytes. Fixed scratch and bounded chunks; no asset decoding,
    // inference, file IO or application heap allocation in this callback.
    while(additional>0) {
        const size_t frames=std::min(kChunkFrames,(static_cast<size_t>(additional)+7)/8);
        self->mixer_.Render(scratch.data(),frames);
        const int bytes=static_cast<int>(frames*2*sizeof(float));
        if(!SDL_PutAudioStreamData(stream,scratch.data(),bytes)){self->callback_failed_=true;break;}
        additional-=bytes;
    }
}
bool AmbienceAudio::SetTargets(const AudioGains& gains,double seconds) {
    if(!stream_||!SDL_LockAudioStream(stream_))return false;
    const bool ok=mixer_.SetTargets(gains,seconds);
    SDL_UnlockAudioStream(stream_);return ok;
}
AudioGains AmbienceAudio::Current() {
    if(!stream_||!SDL_LockAudioStream(stream_))return {};
    const auto result=mixer_.Current();SDL_UnlockAudioStream(stream_);return result;
}
std::string AmbienceAudio::SnapshotJson(const std::string& event) {
    using nlohmann::json;
    if(!stream_||!SDL_LockAudioStream(stream_))return json{{"event",event},{"active",false},{"catalog_version",version_}}.dump();
    const auto current=mixer_.Current(),targets=mixer_.Targets();const auto positions=mixer_.Positions();
    const auto frames=mixer_.Frames();const auto master=mixer_.MasterGain();const auto failed=callback_failed_;
    SDL_UnlockAudioStream(stream_);
    return json{{"event",event},{"active",true},{"catalog_version",version_},{"sample_rate",kRate},{"frames",frames},
        {"positions",positions},{"current",json::parse(AudioGainsJson(current))},{"targets",json::parse(AudioGainsJson(targets))},
        {"master_gain",master},{"callback_failed",failed}}.dump();
}
}
