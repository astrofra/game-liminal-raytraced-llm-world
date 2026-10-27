#include "player_action_fixture.h"
#include "research_archive.h"
#include <filesystem>
#include <fstream>
#include <stdexcept>
using namespace liminal;
using nlohmann::json;
static void RequireAudio(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
void RunAudioTurnTests() {
    SessionState state;char error[1024]={};
    RequireAudio(InitializeSessionState(kLocationQuarryThreshold,&state,error,sizeof(error)),"audio initial state");
    state.language=kGameLanguageEnglish;state.audio_targets={.1f,.2f,.3f,.4f};
    HeadlessTurnConfig config;config.local_scenes=true;config.reuse_unchanged_view=true;
    config.interaction_completion=FixturePlayerCompletion;
    config.generation_config.prompt_token_counter=[](const std::vector<LlmPromptMessage>&,int* count,std::string*){*count=1000;return true;};
    auto catalog=std::make_shared<AmbienceCatalog>();catalog->version=std::string(64,'a');
    for(auto& track:catalog->tracks)track.description="Measured low frequency energy, stable envelope.";
    config.ambience_catalog=catalog;config.audio_current_known=true;config.audio_current_gains={.05f,.1f,.15f,.2f};
    int calls=0;json mix={{"blade",.8},{"bug",.7},{"night",.6},{"static",.5}};
    bool omit=false,fallback=false,cancel=false,delivered=false;
    config.narrative_completion=[&](const LlmGenerationConfig& cfg,const std::vector<LlmPromptMessage>& messages,
        LlmStreamCallback,void*,LlmGenerationResult* result) {
        ++calls;delivered=true;
        const auto schema=json::parse(cfg.json_schema);
        RequireAudio(schema["properties"].contains("audio_mix"),"audio not in grammar");
        const auto context=json::parse(messages.at(1).content);
        RequireAudio(context["ambience"]["tracks"].size()==4,"missing sound cards");
        RequireAudio(context["ambience"]["current_gains"]["blade"].get<float>()==.05f,"missing live gain snapshot");
        json response={{"title","Quarry threshold"},{"summary","Ochre ground beneath a distant pale sky."},
            {"narration","You study the ochre ground through the visor. Pale light settles across the rough surface while your breathing stays steady."},
            {"thermal",{{"external_temperature_c",464},{"body_temperature_c",37},{"reason","Stable exposure."}}},
            {"exits",context["exit_proposal"]["fixed"]}};
        if(!omit)response["audio_mix"]=mix;
        result->response_text=response.dump();result->success=!fallback;result->reached_eog=true;return !fallback;
    };
    config.stream_user_data=&cancel;
    config.stream_callback=[](HeadlessTurnStreamPhase,const char*,const char*,void* p){return !*static_cast<bool*>(p);};
    HeadlessTurnResult turn;
    RequireAudio(RunHeadlessTurnFromState(state,"look",config,&turn,error,sizeof(error)),error);
    RequireAudio(calls==1&&turn.audio_mix.valid&&!turn.used_turn_fallback,"audio required extra call or invalid turn");
    const auto before=state.audio_targets;
    RequireAudio(before[0]==.1f,"uncommitted turn mutated audio");
    UpdateSessionStateFromTurn("look",turn,&state);
    RequireAudio(state.audio_targets==turn.audio_mix.targets,"committed audio missing");
    const auto accepted=state.audio_targets;
    for(int variant=0;variant<3;++variant) {
        omit=variant==0;mix={{"alien",-1}};fallback=variant==2;
        RequireAudio(RunHeadlessTurnFromState(state,"look",config,&turn,error,sizeof(error)),error);
        RequireAudio(!turn.audio_mix.valid,"bad/missing/fallback audio accepted");
        if(variant<2)RequireAudio(!turn.used_turn_fallback,"bad audio rejected narrative");
        UpdateSessionStateFromTurn("look",turn,&state);RequireAudio(state.audio_targets==accepted,"bad audio lost last mix");
    }
    fallback=false;omit=false;mix={{"blade",0},{"bug",0},{"night",0},{"static",0}};
    // Cancel only once the model has returned a valid proposed new mix.
    config.stream_user_data=&delivered;delivered=false;
    config.stream_callback=[](HeadlessTurnStreamPhase,const char*,const char*,void* p){return !*static_cast<bool*>(p);};
    RequireAudio(!RunHeadlessTurnFromState(state,"look",config,&turn,error,sizeof(error))&&!turn.accepted,"cancel not observed");
    UpdateSessionStateFromTurn("look",turn,&state);RequireAudio(state.audio_targets==accepted,"cancel published new targets");
    const auto archived=json::parse(AudioDecisionJson(turn.audio_mix,false));
    RequireAudio(archived["retained_targets"]==json::parse(AudioGainsJson(accepted))&&!archived["committed"].get<bool>(),"cancel archive claims new targets");
    config.stream_callback=nullptr;
    const int previous_calls=calls;
    RequireAudio(RunHeadlessTurnFromState(state,"i",config,&turn,error,sizeof(error)),error);
    UpdateSessionStateFromTurn("i",turn,&state);
    RequireAudio(calls==previous_calls&&state.audio_targets==accepted,"inventory changed audio or called model");
    std::string saved;RequireAudio(SerializeSessionStateToJsonString(state,&saved),"audio snapshot serialization");
    SessionState restored;RequireAudio(ParseSessionStateFromJson(saved.c_str(),&restored,error,sizeof(error)),error);
    RequireAudio(restored.audio_targets==accepted&&restored.audio_catalog_version==catalog->version,"audio snapshot lost");
}
