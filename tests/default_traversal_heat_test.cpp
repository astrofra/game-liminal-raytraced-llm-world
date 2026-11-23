#include "player_action_fixture.h"
#include "investigation.h"
#include "turn_runner.h"
#include "renderer.h"
#include "nlohmann/json.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>

// New engine seam under review: the implementation lands with the engine fix.
namespace liminal {
float TraversalBodyTemperature(const HardState& state);
}

using namespace liminal;
using nlohmann::json;

#define REQUIRE(c) do {if(!(c))throw std::runtime_error(std::string("Default traversal heat line ")+std::to_string(__LINE__)+": "+#c);} while(false)

namespace {
const char* kFixtureScene=R"(room "Default traversal fixture"
camera eye(0,1.6,-6) target(0,0.6,4) up(0,1,0) fov(50)
sky zenith(0.1) horizon(0.4) nadir(0.05) band(0.35) curve(1.6) noise(0)
distant_light direction(-0.6,-1,0.35) intensity(3.5) color(1,0.88,0.55) angular_radius(0.65)
box "floor" pos(0,-0.1,0) size(20,0.2,20) gray(0.6)
box "wall" pos(-4,2,1) size(1,4,8) gray(0.6)
)";
const std::string kRoomA="generated:room_000001";
const std::string kRoomB="generated:room_000002";

SessionState Fixture() {
    SessionState state;char error[512]={};
    REQUIRE(InitializeSessionState(kLocationQuarryThreshold,&state,error,sizeof(error)));
    state.language=kGameLanguageFrench;state.current_place_id=kRoomA;state.origin_place_id=kRoomA;
    state.hard_state.current_location_id=kLocationUnknown;state.hard_state.body_temperature_c=37.0f;
    state.hard_state.thermal_debug.enabled=false;state.spatial_state=SpatialState();
    state.spatial_state.room_title="Le seuil clair";
    state.spatial_state.room_summary="Un sol couvert de gravats fins borde des masses claires.";
    state.spatial_state.blocked_exits={"east","west"};
    state.generated_rooms.clear();state.room_links.clear();state.invisible_barriers.clear();
    for(const std::string& id:{kRoomA,kRoomB}) {
        GeneratedRoom room;room.room_id=id;room.spatial_state=state.spatial_state;
        room.scene_text=kFixtureScene;room.scene_contract_version=0;room.arrival_narration=state.spatial_state.room_summary;
        state.generated_rooms.push_back(room);state.reference_descriptions[id]=room.arrival_narration;
    }
    RoomLink north;north.from_place_id=kRoomA;north.to_place_id=kRoomB;north.direction=kDirectionNorth;
    state.room_links.push_back(north);
    RoomLink south;south.from_place_id=kRoomB;south.to_place_id=kRoomA;south.direction=kDirectionSouth;
    state.room_links.push_back(south);
    state.investigation.decisions.clear();PreserveLegacyInvestigationPlaces(&state);
    return state;
}

struct Harness {
    HeadlessTurnConfig cfg;
    int normal_calls=0,hallucination_calls=0;
    float last_body_temperature=std::numeric_limits<float>::quiet_NaN();
    bool fail_narration=false,cancel=false,cancel_on_narration=false;
    Harness() {
        cfg.local_scenes=true;cfg.investigation_opportunities=false;
        cfg.generation_config.prompt_token_counter=[](const std::vector<LlmPromptMessage>& messages,int* count,std::string*) {
            size_t total=0;for(const auto& message:messages)total+=message.content.size();
            *count=static_cast<int>(total/4+1);return true;};
        cfg.interaction_completion=FixturePlayerCompletion;
        cfg.dynamic_scene_config.completion=[](const LlmGenerationConfig&,const std::vector<LlmPromptMessage>&,LlmStreamCallback,void*,LlmGenerationResult*)->bool {
            throw std::runtime_error("A cached traversal composed a new room");};
        cfg.narrative_completion=[this](const LlmGenerationConfig& config,const std::vector<LlmPromptMessage>& messages,LlmStreamCallback,void*,LlmGenerationResult* result) {
            if(config.purpose=="inventory_paraphrase_v1")return false;
            if(config.purpose=="scene_hallucination_v1") {
                ++hallucination_calls;
                result->success=true;result->reached_eog=true;
                result->response_text=json({{"fragments","Les arêtes claires restent stables autour de vous. Le sol montre les mêmes gravats fins, et la lumière tombe sans changer. Votre souffle marque seul le rythme, tandis que les murs gardent leur distance. Aucun détail nouveau ne se détache dans la pièce."}}).dump();
                return true;
            }
            ++normal_calls;
            const auto request=json::parse(messages.at(1).content);
            last_body_temperature=request.at("temperature_body_c").get<float>();
            if(fail_narration)return false;
            json response={
                {"title","Le seuil clair"},
                {"summary","Les masses claires bordent ce passage ouvert, sous une lumière égale."},
                {"narration","Vous franchissez le seuil et retrouvez les masses claires. Le sol garde la trace fine des gravats, et la lumière tombe sans changer d'un mur à l'autre. Votre souffle ralentit dans l'air immobile avant le pas suivant."},
                {"thermal",{{"external_temperature_c",request.at("temperature_external_c")},
                            {"body_temperature_c",request.at("temperature_body_c")},
                            {"reason","Protection stable."}}},
                {"exits",request.at("exit_proposal").at("fixed")}};
            if(json::parse(config.json_schema)["properties"].contains("deposited_ids"))response["deposited_ids"]=json::array();
            result->success=true;result->reached_eog=true;result->response_text=response.dump();
            if(cancel_on_narration)cancel=true;
            return true;
        };
    }
    HeadlessTurnResult Run(SessionState& state,const char* command) {
        HeadlessTurnResult turn;char error[512]={};
        const bool ok=RunHeadlessTurnFromState(state,command,cfg,&turn,error,sizeof(error));
        if(!ok)throw std::runtime_error(std::string("Default traversal turn ")+command+": "+error);
        UpdateSessionStateFromTurn(command,turn,&state);
        return turn;
    }
};

std::string Snapshot(const SessionState& state) {
    std::string text;REQUIRE(SerializeSessionStateToJsonString(state,&text));return text;
}
}

void RunDefaultTraversalHeatTests() {
    // 1. The pure helper heats by 0.5 C, caps at 43 C, never mutates its input,
    // and leaves explicitly enabled thermal diagnostics unchanged.
    {
        HardState state;state.body_temperature_c=37.0f;
        const int moves=state.move_count;
        float temperature=state.body_temperature_c;
        for(int step=1;step<=10;++step) {
            state.body_temperature_c=temperature;
            const float before=state.body_temperature_c;
            const SuitThermalDebug debug_before=state.thermal_debug;
            temperature=TraversalBodyTemperature(state);
            REQUIRE(std::abs(temperature-(37.0f+0.5f*step))<1e-3f);
            REQUIRE(state.body_temperature_c==before);
            REQUIRE(state.move_count==moves);
            REQUIRE(state.thermal_debug.enabled==debug_before.enabled);
        }
        REQUIRE(std::abs(temperature-42.0f)<1e-3f);
        state.body_temperature_c=42.8f;REQUIRE(std::abs(TraversalBodyTemperature(state)-43.0f)<1e-3f);
        state.body_temperature_c=43.0f;REQUIRE(std::abs(TraversalBodyTemperature(state)-43.0f)<1e-3f);
        state.body_temperature_c=41.2f;state.thermal_debug.enabled=true;
        REQUIRE(std::abs(TraversalBodyTemperature(state)-41.2f)<1e-3f);
        state.body_temperature_c=42.8f;REQUIRE(std::abs(TraversalBodyTemperature(state)-42.8f)<1e-3f);
    }

    // 2. Normal play over two linked generated rooms: every accepted local move
    // adds exactly 0.5 C before narration, revisits included.
    {
        SessionState state=Fixture();
        Harness harness;
        REQUIRE(state.hard_state.body_temperature_c==37.0f);
        REQUIRE(!state.hard_state.thermal_debug.enabled);
        const auto initial_inventory=state.hard_state.inventory_items;
        const auto initial_rooms=state.generated_rooms;
        const auto initial_links=state.room_links;
        const int initial_moves=state.hard_state.move_count;
        auto geometry_unchanged=[&](const SessionState& current) {
            if(current.generated_rooms.size()!=initial_rooms.size()||
               current.room_links.size()!=initial_links.size())return false;
            for(size_t i=0;i<initial_rooms.size();++i)
                if(current.generated_rooms[i].room_id!=initial_rooms[i].room_id||
                   current.generated_rooms[i].scene_text!=initial_rooms[i].scene_text)return false;
            for(size_t i=0;i<initial_links.size();++i)
                if(current.room_links[i].from_place_id!=initial_links[i].from_place_id||
                   current.room_links[i].direction!=initial_links[i].direction||
                   current.room_links[i].to_place_id!=initial_links[i].to_place_id)return false;
            return true;
        };
        REQUIRE(geometry_unchanged(state));
        const float temperature_cap=kMaxBodyTemperatureC;
        const float initial_force=RenderConfigForVisor(RenderConfig(),37.0f,kRoomA).visor_drop_strength;
        REQUIRE(!std::isnan(initial_force)&&std::abs(initial_force)<1e-3f);

        for(int round=0;round<5;++round) {
            for(int leg=0;leg<2;++leg) {
                const int move=round*2+leg+1;
                const char* command=leg==0?"north":"south";
                const std::string expected_place=leg==0?kRoomB:kRoomA;
                const float expected=std::min(37.0f+0.5f*move,temperature_cap);
                const int normal_before=harness.normal_calls;
                const int hallucination_before=harness.hallucination_calls;
                const HeadlessTurnResult turn=harness.Run(state,command);
                REQUIRE(turn.accepted&&!turn.used_turn_fallback);
                REQUIRE(state.current_place_id==expected_place);
                REQUIRE(turn.updated_place_id==expected_place);
                REQUIRE(std::abs(state.hard_state.body_temperature_c-expected)<1e-3f);
                REQUIRE(std::abs(turn.updated_hard_state.body_temperature_c-expected)<1e-3f);
                REQUIRE(state.hard_state.move_count==initial_moves+move);
                REQUIRE(state.hard_state.inventory_items==initial_inventory);
                REQUIRE(geometry_unchanged(state));
                REQUIRE(turn.rendered_scene_text==kFixtureScene);
                if(harness.normal_calls>normal_before)
                    REQUIRE(std::abs(harness.last_body_temperature-expected)<1e-3f);
                if(move==6)
                    REQUIRE(RenderConfigForVisor(RenderConfig(),state.hard_state.body_temperature_c,state.current_place_id).visor_drop_strength>0.0f);
                if(move==10) {
                    const float force=RenderConfigForVisor(RenderConfig(),state.hard_state.body_temperature_c,state.current_place_id).visor_drop_strength;
                    REQUIRE(std::abs(force-1.0f)<1e-3f);
                    REQUIRE(harness.hallucination_calls>hallucination_before);
                }
            }
        }
        REQUIRE(std::abs(state.hard_state.body_temperature_c-42.0f)<1e-3f);
        REQUIRE(state.current_place_id==kRoomA);
        REQUIRE(state.hard_state.move_count==initial_moves+10);
        REQUIRE(harness.hallucination_calls>0);
    }

    // 3. A blocked exact compass shortcut neither displaces nor heats.
    {
        SessionState state=Fixture();
        Harness harness;
        const auto inventory=state.hard_state.inventory_items;
        const float temperature=state.hard_state.body_temperature_c;
        const int moves=state.hard_state.move_count;
        const std::string place=state.current_place_id;
        HeadlessTurnResult turn;char error[512]={};
        REQUIRE(RunHeadlessTurnFromState(state,"east",harness.cfg,&turn,error,sizeof(error)));
        REQUIRE(turn.accepted&&turn.turn_result.intent=="move_east_blocked");
        UpdateSessionStateFromTurn("east",turn,&state);
        REQUIRE(state.current_place_id==place);
        REQUIRE(state.hard_state.move_count==moves&&turn.updated_hard_state.move_count==moves);
        REQUIRE(state.hard_state.body_temperature_c==temperature);
        REQUIRE(turn.updated_hard_state.body_temperature_c==temperature);
        REQUIRE(state.hard_state.inventory_items==inventory);
    }

    // 4. A failed narration on an accepted revisit advances and heats once.
    {
        SessionState state=Fixture();
        Harness harness;const int moves=state.hard_state.move_count;
        harness.Run(state,"north");
        REQUIRE(state.current_place_id==kRoomB);
        REQUIRE(std::abs(state.hard_state.body_temperature_c-37.5f)<1e-3f);
        harness.fail_narration=true;
        const HeadlessTurnResult turn=harness.Run(state,"south");
        REQUIRE(turn.accepted&&turn.used_turn_fallback);
        REQUIRE(state.current_place_id==kRoomA&&state.hard_state.move_count==moves+2);
        REQUIRE(std::abs(state.hard_state.body_temperature_c-38.0f)<1e-3f);
        REQUIRE(std::abs(turn.updated_hard_state.body_temperature_c-38.0f)<1e-3f);
    }

    // 5. Cancellation during narration leaves the published session untouched.
    {
        SessionState state=Fixture();
        Harness harness;
        harness.cancel_on_narration=true;
        harness.cfg.stream_callback=[](HeadlessTurnStreamPhase,const char*,const char*,void* user) {
            return !static_cast<Harness*>(user)->cancel;};
        harness.cfg.stream_user_data=&harness;
        const std::string before=Snapshot(state);
        HeadlessTurnResult turn;char error[512]={};
        REQUIRE(!RunHeadlessTurnFromState(state,"north",harness.cfg,&turn,error,sizeof(error)));
        REQUIRE(!turn.accepted);
        UpdateSessionStateFromTurn("north",turn,&state);
        REQUIRE(Snapshot(state)==before);
    }
}
