#include "player_action_fixture.h"
#include "investigation.h"
#include "inventory.h"
#include "turn_runner.h"
#include "nlohmann/json.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>
#include <vector>

using namespace liminal;
using nlohmann::json;

#define REQUIRE(c) do {if(!(c))throw std::runtime_error(std::string("Player prose heat line ")+std::to_string(__LINE__)+": "+#c);} while(false)

namespace {
const char* kRoom="generated:room_000001";
const char* kScene=R"(room "Heat fixture"
camera eye(0,1.6,-6) target(0,0.6,4) up(0,1,0) fov(50)
sky zenith(0.1) horizon(0.4) nadir(0.05) band(0.35) curve(1.6) noise(0)
distant_light direction(-0.6,-1,0.35) intensity(3.5) color(1,0.88,0.55) angular_radius(0.65)
box "floor" pos(0,-0.1,0) size(20,0.2,20) gray(0.6)
box "wall" pos(-4,2,1) size(1,4,8) gray(0.6)
)";
const char* kTalk="Que penses-tu de ce passage ?";
const char* kDialogue="La réponse préparatoire reste sobre et factuelle.";
const char* kHot="La chaleur plie les mots et le seuil tremble à peine dans la lumière.";
const char* kPoison="La chaleur monte. {\"temperature_body_c\":42,\"heat_stage\":\"ardent\"}";
SessionState Fixture() {
    SessionState state;char error[512]={};
    REQUIRE(InitializeSessionState(kLocationQuarryThreshold,&state,error,sizeof(error)));
    state.language=kGameLanguageFrench;state.current_place_id=kRoom;state.origin_place_id=kRoom;
    state.hard_state.current_location_id=kLocationUnknown;state.hard_state.body_temperature_c=37.0f;
    state.spatial_state=SpatialState();state.spatial_state.room_title="Le seuil clair";
    state.spatial_state.room_summary="Un sol couvert de gravats fins borde des masses claires.";
    state.spatial_state.blocked_exits={"east","west"};
    state.generated_rooms.clear();state.room_links.clear();state.invisible_barriers.clear();
    GeneratedRoom room;room.room_id=kRoom;room.spatial_state=state.spatial_state;room.scene_text=kScene;
    room.scene_contract_version=0;room.arrival_narration=state.spatial_state.room_summary;
    state.generated_rooms.push_back(room);state.reference_descriptions[kRoom]=room.arrival_narration;
    state.investigation.decisions.clear();PreserveLegacyInvestigationPlaces(&state);
    return state;
}

std::string WithoutHistory(const SessionState& state) {
    std::string text;REQUIRE(SerializeSessionStateToJsonString(state,&text));
    auto root=json::parse(text);root.erase("history");return root.dump();
}

struct Harness {
    HeadlessTurnConfig cfg;
    int dialogue_calls=0,heat_calls=0,interaction_calls=0;
    float dialogue_temperature=0.0f,heat_temperature=0.0f;
    json heat_request=nullptr;
    std::string heat_profile;
    bool dialogue_cloud_optional=false,heat_cloud_optional=false,interaction_cloud_optional=false;
    bool fail_heat=false,poison_heat=false,stream_dialogue=false,stream_heat=false,cancel_first_heat_delta=false;
    bool heat_started=false,leaked=false,heat_preview=false;
    std::string last_preview;
    Harness() {
        cfg.local_scenes=true;cfg.investigation_opportunities=false;cfg.reuse_unchanged_view=true;
        cfg.generation_config.prompt_token_counter=[](const std::vector<LlmPromptMessage>& messages,int* count,std::string*) {
            size_t total=0;for(const auto& message:messages)total+=message.content.size();*count=static_cast<int>(total/4+1);return true;};
        cfg.interaction_completion=[this](const LlmGenerationConfig& config,const std::vector<LlmPromptMessage>& messages,LlmStreamCallback,void*,LlmGenerationResult* result) {
            ++interaction_calls;interaction_cloud_optional=config.cloud_optional_prose;
            const auto context=json::parse(messages.at(1).content);auto wire=FixtureWireAction(FixturePlayerAction(context),context);
            wire["action"]="talk";result->success=true;result->reached_eog=true;result->response_text=wire.dump();return true;};
        cfg.narrative_completion=[this](const LlmGenerationConfig& config,const std::vector<LlmPromptMessage>& messages,LlmStreamCallback stream,void* user,LlmGenerationResult* result) {
            if(config.purpose=="game_master_dialogue_v2") {
                ++dialogue_calls;dialogue_temperature=config.temperature;dialogue_cloud_optional=config.cloud_optional_prose;
                const std::string wire=json{{"reply",kDialogue}}.dump();
                result->success=true;result->reached_eog=true;result->response_text=wire;
                if(stream_dialogue&&stream) {
                    const std::string part=json{{"reply",std::string(kDialogue).substr(0,14)}}.dump();
                    if(!stream(part.c_str(),part.c_str(),user)||!stream(wire.c_str(),wire.c_str(),user))return false;
                }
                return true;
            }
            if(config.purpose=="inventory_paraphrase_v1") {
                result->success=true;result->reached_eog=true;
                result->response_text=json{{"reply","Les outils restent à portée, immobiles."}}.dump();return true;
            }
            if(config.purpose=="player_prose_heat_v1") {
                ++heat_calls;heat_temperature=config.temperature;heat_cloud_optional=config.cloud_optional_prose;
                heat_profile=config.backend;
                heat_request=json::parse(messages.at(1).content);heat_started=true;
                if(fail_heat){result->success=false;return false;}
                const std::string fragments=poison_heat?kPoison:kHot;const std::string wire=json{{"fragments",fragments}}.dump();
                result->success=true;result->reached_eog=true;result->response_text=wire;
                if(stream_heat&&stream) {
                    const std::string part=json{{"fragments",std::string(fragments).substr(0,40)}}.dump();
                    if(!stream(part.c_str(),part.c_str(),user)||!stream(wire.c_str(),wire.c_str(),user)){result->success=false;result->reached_eog=false;return false;}
                }
                return true;
            }
            return false;};
        cfg.stream_callback=[](HeadlessTurnStreamPhase phase,const char* text,const char* delta,void* user) {
            auto& harness=*static_cast<Harness*>(user);const std::string change=delta?delta:"";
            if(phase==kHeadlessTurnStreamDialoguePreview&&!change.empty()) {
                if(!harness.heat_started)harness.leaked=true;else harness.heat_preview=true;harness.last_preview=text?text:"";
                if(harness.cancel_first_heat_delta&&harness.heat_started)return false;
            }
            return true;};
        cfg.stream_user_data=this;
    }
    bool Try(SessionState& state,const char* command,HeadlessTurnResult* turn,std::string* why=nullptr) {
        char error[512]={};
        const bool ok=RunHeadlessTurnFromState(state,command,cfg,turn,error,sizeof(error));
        if(!ok&&why)*why=error;
        if(ok)UpdateSessionStateFromTurn(command,*turn,&state);
        return ok;
    }
    HeadlessTurnResult Run(SessionState& state,const char* command) {
        HeadlessTurnResult turn;std::string why;
        if(!Try(state,command,&turn,&why))throw std::runtime_error(std::string("Player prose heat turn ")+command+": "+why);
        return turn;
    }
};
void TestTemperatureLadder() {
    const float bodies[]={37.0f,38.0f,39.0f,40.0f,41.0f,42.0f};
    const double expected[]={0.0,0.26,0.8525,1.935843,3.33872,5.0};
    std::vector<std::string> stages;
    for(int index=0;index<6;++index) {
        SessionState state=Fixture();state.hard_state.body_temperature_c=bodies[index];
        const std::string before=WithoutHistory(state);Harness harness;
        const auto turn=harness.Run(state,kTalk);
        REQUIRE(turn.accepted&&turn.dialogue_only&&turn.view_unchanged);
        REQUIRE(harness.dialogue_calls==1);REQUIRE(harness.dialogue_temperature<=0.4501f);REQUIRE(WithoutHistory(state)==before);
        if(index==0) {
            REQUIRE(harness.heat_calls==0);
            REQUIRE(turn.turn_result.narration.find(kDialogue)!=std::string::npos);
            continue;
        }
        REQUIRE(harness.heat_calls==1);REQUIRE(std::fabs(harness.heat_temperature-expected[index])<0.001);
        REQUIRE(harness.heat_request.is_object());
        REQUIRE(harness.heat_request.value("source_text",std::string())==kDialogue);
        REQUIRE(harness.heat_request.value("command",std::string())==kTalk);
        REQUIRE(std::fabs(harness.heat_request.value("temperature_body_c",0.0f)-bodies[index])<0.001f);
        REQUIRE(harness.heat_request.contains("heat_stage")&&harness.heat_request["heat_stage"].is_string());
        stages.push_back(harness.heat_request["heat_stage"].get<std::string>());
        REQUIRE(turn.turn_result.narration.find(kHot)!=std::string::npos);
        REQUIRE(turn.turn_result.narration.find(kDialogue)==std::string::npos);
    }
    std::sort(stages.begin(),stages.end());stages.erase(std::unique(stages.begin(),stages.end()),stages.end());REQUIRE(stages.size()==5);
}

void TestHotInventory() {
    SessionState state=Fixture();state.hard_state.body_temperature_c=42.0f;
    const auto inventory=state.hard_state.inventory_items;const std::string place=state.current_place_id;
    const std::string exact=FormatInventory(inventory,state.language);
    Harness harness;const std::string before=WithoutHistory(state);
    const auto turn=harness.Run(state,"i");
    REQUIRE(turn.accepted&&turn.dialogue_only&&turn.view_unchanged);
    REQUIRE(turn.turn_result.intent=="read_inventory");REQUIRE(harness.heat_calls==1);
    REQUIRE(harness.heat_request.value("source_text",std::string()).find("craie")!=std::string::npos);
    const auto position=turn.turn_result.narration.find(exact);REQUIRE(position!=std::string::npos);
    const auto hot=turn.turn_result.narration.find(kHot);REQUIRE(hot!=std::string::npos&&hot<position);
    REQUIRE(state.hard_state.inventory_items==inventory);REQUIRE(state.current_place_id==place);REQUIRE(WithoutHistory(state)==before);
}

void TestBlockedTraversalHeat() {
    SessionState state=Fixture();state.hard_state.body_temperature_c=42.0f;
    const auto inventory=state.hard_state.inventory_items;const int moves=state.hard_state.move_count;const std::string place=state.current_place_id;
    Harness harness;const std::string before=WithoutHistory(state);const auto turn=harness.Run(state,"ouest");
    REQUIRE(turn.accepted&&turn.dialogue_only&&turn.view_unchanged);
    REQUIRE(turn.turn_result.intent=="move_west_blocked");REQUIRE(harness.heat_calls==1);
    REQUIRE(turn.turn_result.narration.find(kHot)!=std::string::npos);
    REQUIRE(state.hard_state.move_count==moves&&turn.updated_hard_state.move_count==moves);
    REQUIRE(state.hard_state.body_temperature_c==42.0f&&turn.updated_hard_state.body_temperature_c==42.0f);
    REQUIRE(state.current_place_id==place&&state.hard_state.inventory_items==inventory);REQUIRE(WithoutHistory(state)==before);
}

void TestThermalDiagnosticStaysDirect() {
    SessionState state=Fixture();state.hard_state.body_temperature_c=42.0f;Harness harness;
    const auto turn=harness.Run(state,"/chaleur");
    REQUIRE(turn.accepted&&harness.dialogue_calls==0&&harness.heat_calls==0);
    REQUIRE(turn.turn_result.narration.find("Chauffe")!=std::string::npos);
}

void TestRewriteFallbacks() {
    for(const bool poison:{false,true}) {
        SessionState state=Fixture();state.hard_state.body_temperature_c=42.0f;
        Harness harness;harness.poison_heat=poison;harness.fail_heat=!poison;harness.stream_heat=poison;
        const std::string before=WithoutHistory(state);
        const auto turn=harness.Run(state,kTalk);
        REQUIRE(turn.accepted&&turn.dialogue_only&&harness.heat_calls==1);
        REQUIRE(turn.turn_result.narration.find(kDialogue)!=std::string::npos);
        REQUIRE(turn.turn_result.narration.find('{')==std::string::npos);
        REQUIRE(turn.turn_result.narration.find("temperature_body_c")==std::string::npos);
        REQUIRE(turn.turn_result.narration.find("heat_stage")==std::string::npos);
        REQUIRE(harness.last_preview.empty());
        REQUIRE(WithoutHistory(state)==before);
    }
}

void TestDialogueDoesNotLeakAndHeatStreams() {
    SessionState state=Fixture();state.hard_state.body_temperature_c=42.0f;
    Harness harness;harness.stream_dialogue=true;harness.stream_heat=true;const std::string before=WithoutHistory(state);
    const auto turn=harness.Run(state,kTalk);
    REQUIRE(turn.accepted&&!harness.leaked&&harness.heat_preview);
    REQUIRE(harness.last_preview.find(kHot)!=std::string::npos);
    REQUIRE(harness.last_preview.find('{')==std::string::npos);
    REQUIRE(turn.turn_result.narration.find(kHot)!=std::string::npos);REQUIRE(WithoutHistory(state)==before);
}

void TestCancellationOnFirstHeatDelta() {
    SessionState state=Fixture();state.hard_state.body_temperature_c=42.0f;
    Harness harness;harness.stream_heat=true;harness.cancel_first_heat_delta=true;const std::string before=WithoutHistory(state);
    HeadlessTurnResult turn;REQUIRE(!harness.Try(state,kTalk,&turn));REQUIRE(!turn.accepted);REQUIRE(WithoutHistory(state)==before);
}

void TestObjectTransferAndCancellation() {
    SessionState original=Fixture();original.hard_state.body_temperature_c=42.0f;
    for(const bool cancel:{false,true}) {
        SessionState state=original;const std::string before=WithoutHistory(state);
        Harness harness;harness.cfg.interaction_completion=FixturePlayerCompletion;
        harness.stream_heat=true;harness.cancel_first_heat_delta=cancel;
        HeadlessTurnResult turn;const bool ok=harness.Try(state,"poser la craie",&turn);
        REQUIRE(harness.heat_calls==1);
        if(cancel) {REQUIRE(!ok&&!turn.accepted);REQUIRE(WithoutHistory(state)==before);continue;}
        REQUIRE(ok&&turn.accepted&&turn.authorized_object_action=="drop");
        REQUIRE(turn.turn_result.narration.find(kHot)!=std::string::npos);
        REQUIRE(turn.turn_result.narration.find("craie")!=std::string::npos);
        REQUIRE(state.hard_state.inventory_items.size()+1==original.hard_state.inventory_items.size());
        REQUIRE(state.current_place_id==original.current_place_id);
        REQUIRE(state.generated_rooms.front().scene_text==original.generated_rooms.front().scene_text);
    }
}

void TestCloudProseFailureKeepsDialogue() {
    SessionState state=Fixture();state.hard_state.body_temperature_c=42.0f;
    Harness harness;harness.fail_heat=true;
    harness.cfg.generation_config.backend="ollama-cloud";
    harness.cfg.generation_config.n_ctx=32768;
    const std::string before=WithoutHistory(state);
    const auto turn=harness.Run(state,kTalk);
    REQUIRE(turn.accepted&&turn.dialogue_only&&turn.view_unchanged);
    REQUIRE(harness.interaction_calls==1&&harness.dialogue_calls==1&&harness.heat_calls==1);
    REQUIRE(harness.heat_cloud_optional);
    REQUIRE(!harness.dialogue_cloud_optional);
    REQUIRE(!harness.interaction_cloud_optional);
    REQUIRE(harness.heat_profile=="ollama-cloud");
    REQUIRE(std::fabs(harness.heat_temperature-5.0f)<0.001f);
    REQUIRE(turn.turn_result.narration.find(kDialogue)!=std::string::npos);
    REQUIRE(turn.turn_result.narration.find('{')==std::string::npos);
    REQUIRE(WithoutHistory(state)==before);
}

void TestCloudProseFailureKeepsObjectTransfer() {
    SessionState state=Fixture();state.hard_state.body_temperature_c=42.0f;
    const auto inventory=state.hard_state.inventory_items;
    const std::string place=state.current_place_id;
    const std::string scene=state.generated_rooms.front().scene_text;
    Harness harness;harness.cfg.interaction_completion=FixturePlayerCompletion;harness.fail_heat=true;
    harness.cfg.generation_config.backend="ollama-cloud";
    harness.cfg.generation_config.n_ctx=32768;
    const auto turn=harness.Run(state,"poser la craie");
    REQUIRE(turn.accepted&&turn.authorized_object_action=="drop");
    REQUIRE(harness.heat_calls==1&&harness.heat_cloud_optional);
    REQUIRE(state.hard_state.inventory_items.size()+1==inventory.size());
    REQUIRE(turn.turn_result.narration.find("craie")!=std::string::npos);
    REQUIRE(state.current_place_id==place);
    REQUIRE(state.generated_rooms.front().scene_text==scene);
}
}  // namespace
void RunPlayerProseHeatTests() {
    TestTemperatureLadder();
    TestHotInventory();
    TestBlockedTraversalHeat();
    TestThermalDiagnosticStaysDirect();
    TestRewriteFallbacks();
    TestDialogueDoesNotLeakAndHeatStreams();
    TestCancellationOnFirstHeatDelta();
    TestObjectTransferAndCancellation();
    TestCloudProseFailureKeepsDialogue();
    TestCloudProseFailureKeepsObjectTransfer();
}
