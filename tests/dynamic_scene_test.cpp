#include "dynamic_scene.h"
#include "renderer.h"
#include "turn_runner.h"
#include "nlohmann/json.hpp"
#include <cstdio>
#include <stdexcept>
#include <thread>
#include <chrono>
using namespace liminal;
using json=nlohmann::json;
#define CHECK(c) do { if(!(c)) throw std::runtime_error(std::string("Check failed at ")+std::to_string(__LINE__)+": "+#c); } while(false)

static const char* program=R"SCENE(room "Open arch"
camera eye(0,1.6,-6) target(0,1,5) up(0,1,0) fov(50)
sky zenith(0.06) horizon(0.4) nadir(0.01) band(0.35) curve(1.6) noise(0)
distant_light direction(-0.6,-1,0.35) intensity(3.5) color(1,0.88,0.55) angular_radius(0.65)
box "floor_desert" pos(0,-0.1,0) size(20,0.2,20) gray(0.6)
box "arch_left" pos(-3,1.75,2) size(1,3.5,1) gray(0.65)
box "arch_right" pos(3,1.75,2) size(1,3.5,1) gray(0.65)
box "arch_top" pos(0,3.75,2) size(7,0.5,1) gray(0.65)
box "marker" pos(0,0.5,5) size(1,1,1) gray(0.8)
)SCENE";
static SceneDesignV1 Design() {
    SceneDesignV1 d;d.title="Open arch";d.intent="A monumental arch around a low marker";
    d.arrival="The low marker stands beyond an open arch.";d.focal_object_id="marker";
    d.bounds_center=Vec3(0,3,0);d.bounds_size=Vec3(24,8,24);d.floor_y=0;
    d.camera.eye=Vec3(0,1.6f,-6);d.camera.target=Vec3(0,1,5);d.camera.up=Vec3(0,1,0);d.camera.vertical_fov_degrees=50;
    d.masses={{"floor","ground","none",Vec3(0,-.1f,0),Vec3(20,.2f,20),Vec3(0)},
        {"arch","structure","ground",Vec3(0,2,2),Vec3(7,4,1),Vec3(0)},
        {"marker","prop","ground",Vec3(0,.5f,5),Vec3(1),Vec3(0)}};
    d.interaction_bindings={{"datum","marker"}};
    d.reserved_passages={{"north",Vec3(0,1.2f,8),Vec3(2,2.2f,2)},
                         {"south",Vec3(0,1.2f,-8),Vec3(2,2.2f,2)}};
    d.lighting_mode="day";d.lighting_intent="Low sunlight from the west";d.continuity="A low marker echoes the preceding place";
    return d;
}
static std::string Replace(std::string text,const std::string& from,const std::string& to) {
    const auto p=text.find(from);CHECK(p!=std::string::npos);text.replace(p,from.size(),to);return text;
}
static void Geometry() {
    CHECK(SceneTextHash("abc")=="ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    SceneDesignContext ctx;SceneDesignV1 d=Design(),parsed;Scene scene;SceneAuditReport audit;
    CHECK(ParseSceneDesign(SerializeSceneDesign(d),ctx,&parsed,&audit));
    auto described=d;described.masses[1].shape="Two posts and a beam; empty center opening";
    CHECK(ParseSceneDesign(SerializeSceneDesign(described),ctx,&parsed,&audit));
    CHECK(parsed.masses[1].shape==described.masses[1].shape);
    CHECK(BuildSceneProgramPrompt(ctx,&parsed).find(described.masses[1].shape)!=std::string::npos);
    auto ground_notation=json::parse(SerializeSceneDesign(d));ground_notation["ground"]={{"id","floor"},{"size",{20,20}},{"center",{0,0}}};
    ground_notation["masses"].erase(ground_notation["masses"].begin());
    CHECK(ParseSceneDesign(ground_notation.dump(),ctx,&parsed,&audit));
    CHECK(parsed.masses[0].center.y==d.floor_y&&parsed.masses[0].size.y==0);
    auto short_floor=ground_notation;short_floor["ground"]["size"]={2,2};
    CHECK(!ParseSceneDesign(short_floor.dump(),ctx,&parsed,&audit));
    CHECK(SerializeSceneAudit(audit).find("ground.size [width X,depth Z]")!=std::string::npos);
    auto named_floor=ground_notation;
    named_floor["ground"]={{"id","floor"},{"width_x",20},{"depth_z",20},{"center_x",0},{"center_z",0}};
    CHECK(ParseSceneDesign(named_floor.dump(),ctx,&parsed,&audit));
    CHECK(parsed.masses[0].size.x==20&&parsed.masses[0].size.z==20);
    named_floor["ground"]["depth_z"]=2;
    CHECK(!ParseSceneDesign(named_floor.dump(),ctx,&parsed,&audit));
    CHECK(SerializeSceneAudit(audit).find("ground.depth_z")!=std::string::npos);
    ground_notation["focal_mass"]=ground_notation["masses"][1];ground_notation["masses"].erase(ground_notation["masses"].begin()+1);
    ground_notation.erase("focal_object_id");
    CHECK(ParseSceneDesign(ground_notation.dump(),ctx,&parsed,&audit));CHECK(parsed.focal_object_id=="marker");
    auto automatic=json::parse(SerializeSceneDesign(d));automatic["bounds"]="auto";
    CHECK(ParseSceneDesign(automatic.dump(),ctx,&parsed,&audit));CHECK(audit.bounds_derived);
    CHECK(Length(parsed.masses[1].center-d.masses[1].center)==0);
    CHECK(ParseSceneDesign(SerializeSceneDesign(d),ctx,&parsed,&audit));
    auto planar=d;planar.masses.push_back({"panel","light","none",Vec3(0,3,0),Vec3(2,0,2),Vec3(0)});
    CHECK(ParseSceneDesign(SerializeSceneDesign(planar),ctx,&parsed,&audit));
    CHECK(ParseSceneDesign(SerializeSceneDesign(d),ctx,&parsed,&audit));
    CHECK(AuditDynamicScene(program,ctx,&parsed,&scene,&audit));
    CHECK(audit.triangles==60);CHECK(audit.focal_coverage>0);
    CHECK(AuditDynamicScene(Replace(program," up(0,1,0)",""),ctx,&d,&scene,&audit));
    auto prefixed=d;prefixed.masses[2].id="arch_marker";
    prefixed.focal_object_id="arch_marker";prefixed.interaction_bindings[0].mass_id="arch_marker";
    CHECK(AuditDynamicScene(Replace(program,"box \"marker\"","box \"arch_marker\""),ctx,&prefixed,&scene,&audit));
    auto j=json::parse(SerializeSceneDesign(d));j["untrusted_state_delta"]={{"score",999}};
    CHECK(!ParseSceneDesign(j.dump(),ctx,&parsed,&audit));
    auto bad_height=d;bad_height.masses[2].center.y=3;bad_height.reserved_passages[0].center.y=.1f;
    CHECK(!ParseSceneDesign(SerializeSceneDesign(bad_height),ctx,&parsed,&audit));
    CHECK(audit.diagnostics.size()>=2); // Report all independent arithmetic errors in one repair.
    bad_height.masses[1].support="absent_support";
    CHECK(!ParseSceneDesign(SerializeSceneDesign(bad_height),ctx,&parsed,&audit));CHECK(audit.diagnostics.size()>=3);
    auto cyclic=d;cyclic.masses[1].support="marker";cyclic.masses[2].support="arch";
    CHECK(!ParseSceneDesign(SerializeSceneDesign(cyclic),ctx,&parsed,&audit));
    j=json::parse(SerializeSceneDesign(d));j["reserved_passages"][1]["direction"]="east";
    CHECK(!ParseSceneDesign(j.dump(),ctx,&parsed,&audit));
    for(const auto& malformed:{
        std::string(program)+"sphere \"bad\" pos(0,1,0) size(1) gray(0.5)\n",
        Replace(program,"gray(0.8)","gray(nan)"),Replace(program,"size(1,1,1)","size(-1,1,1)"),
        Replace(program,"size(1,1,1)","size(1,1,1) texture(4)"),
        Replace(program,"pos(0,0.5,5)","pos(0,0.5,5000)"),
        Replace(program,"marker\"","arch_left\""),
        Replace(program,"fov(50)","fov(0)"),
        Replace(program,"up(0,1,0)","up(0,0,0)")})
        CHECK(!AuditDynamicScene(malformed,ctx,&d,&scene,&audit));
    CHECK(!AuditDynamicScene(std::string(program)+"box \"bad_a\" pos(0,1,0)\nbox \"bad_b\" pos(1,1,0)\n",ctx,&d,&scene,&audit));
    CHECK(audit.diagnostics.size()>=4);
    auto blocked=d;blocked.reserved_passages[0].center=Vec3(0,1.2f,5);
    CHECK(!AuditDynamicScene(program,ctx,&blocked,&scene,&audit));
    auto open=d;open.reserved_passages[0].center=Vec3(0,1.2f,2);
    CHECK(AuditDynamicScene(program,ctx,&open,&scene,&audit)); // Inside envelope, clear actual arch.
    std::string occluded=std::string(program)+"box \"arch_front\" pos(0,1.75,1.6) size(7,3.5,0.2) gray(0.6)\n";
    CHECK(!AuditDynamicScene(occluded,ctx,&d,&scene,&audit));
    CHECK(audit.diagnostics.front().code=="focal_object_occluded");
    auto inside=d;inside.camera.eye=Vec3(-3,1.6f,2);
    CHECK(!AuditDynamicScene(Replace(program,"eye(0,1.6,-6)","eye(-3,1.6,2)"),ctx,&inside,&scene,&audit));
    auto rotated=inside;rotated.masses[1].size=Vec3(12,8,12);
    CHECK(!AuditDynamicScene(Replace(Replace(program,"eye(0,1.6,-6)","eye(-3,1.6,2)"),
        "pos(-3,1.75,2) size(1,3.5,1)","pos(-3,1.75,2) size(1,3.5,1) rot(0,37,0)"),ctx,&rotated,&scene,&audit));
    auto enveloped=d;enveloped.camera.eye=Vec3(0,1.6f,2);
    CHECK(AuditDynamicScene(Replace(program,"eye(0,1.6,-6)","eye(0,1.6,2)"),ctx,&enveloped,&scene,&audit));
    auto prefab=d;prefab.camera.eye=Vec3(-1,1.6f,2);prefab.masses[1].size=Vec3(10,5,3);
    std::string prefab_program=program;
    const auto first=prefab_program.find("box \"arch_left\"");
    const auto last=prefab_program.find("box \"marker\"");
    prefab_program.replace(first,last-first,"prefab_gate \"arch\" pos(0,2,2) size(7,4,1) gray(0.65) detail(0.3) bars(1)\n");
    CHECK(AuditDynamicScene(Replace(prefab_program,"eye(0,1.6,-6)","eye(-1,1.6,2)"),ctx,&prefab,&scene,&audit));
    CHECK(AuditDynamicScene(Replace(program,"pos(0,3.75,2)","pos(0,3.3,2)"),ctx,&d,&scene,&audit));
    auto support=d;support.masses[2].center.y=2;
    CHECK(!AuditDynamicScene(Replace(program,"pos(0,0.5,5)","pos(0,2,5)"),ctx,&support,&scene,&audit));
    auto hidden=d;hidden.focal_object_id="marker";
    CHECK(!AuditDynamicScene(Replace(program,"target(0,1,5)","target(0,1,-20)"),ctx,&hidden,&scene,&audit));
    std::vector<unsigned char> black(256*128*3,0);audit=SceneAuditReport();
    CHECK(!AuditDynamicPreview(black,"day",&audit));
    CHECK(AuditDynamicScene(program,ctx,&d,&scene,&audit));
    RenderConfig rc;rc.width=32;rc.height=16;rc.samples_per_pixel=1;
    std::vector<unsigned char> pixels;
    int polls=0;RenderControl control{[](void* p){return ++*static_cast<int*>(p)<3;},&polls};
    CHECK(!RenderSceneToPixels(scene,rc,&pixels,&control));CHECK(polls==3);
}
static SceneCompletion Completion(int* calls,int invalid_programs=0,bool eos=true) {
    return [=](const LlmGenerationConfig& c,const std::vector<LlmPromptMessage>& messages,LlmStreamCallback cb,void* user,LlmGenerationResult* r) {
        ++*calls;CHECK(!messages.empty());
        if(cb&&!cb("","",user))return false;
        r->success=true;r->reached_eog=eos;r->stop_reason=eos?"eos":"token_limit";
        if(c.purpose=="scene_design_v1") {CHECK(!c.json_schema.empty());r->response_text=SerializeSceneDesign(Design());}
        else if(c.purpose=="scene_program_repair_v1") {
            CHECK(c.use_json_grammar);r->response_text=json({{"replacements",json::array({{{"line",1},{"text",*calls<=1+invalid_programs?"invalid scene":program}}})}}).dump();
        } else {CHECK(!c.use_json_grammar);r->response_text=*calls<=1+invalid_programs?"invalid scene":program;}
        return true;
    };
}
static void Pipeline() {
    SceneDesignContext ctx;LlmGenerationConfig model;DynamicSceneConfig cfg;DynamicSceneResult r;int calls=0;
    cfg.completion=Completion(&calls,1);CHECK(GenerateDynamicScene(ctx,model,cfg,nullptr,nullptr,&r));
    CHECK(calls==3&&r.repairs==1&&r.scene_hash==SceneTextHash(program));
    CHECK(json::parse(r.metrics_json)["program_patch_repairs"]==1);
    for(int invalid_patch_kind=0;invalid_patch_kind<3;++invalid_patch_kind) {
    calls=0;
    cfg.completion=[&](const LlmGenerationConfig& c,const std::vector<LlmPromptMessage>& messages,LlmStreamCallback,void*,LlmGenerationResult* out) {
        ++calls;out->success=true;out->reached_eog=true;
        if(calls==4)CHECK(messages.back().content.find("property_range")!=std::string::npos);
        if(c.purpose=="scene_design_v1")out->response_text=SerializeSceneDesign(Design());
        else if(c.purpose=="scene_program_v1")out->response_text=Replace(program,"gray(0.8)","gray(2)");
        else {
            json edits=json::array({{{"line",calls==3&&invalid_patch_kind==0?999:9},{"text","box \"marker\" pos(0,0.5,5) size(1,1,1) gray(0.8)"}}});
            if(calls==3&&invalid_patch_kind==1)edits.push_back(edits[0]);
            out->response_text=json({{"replacements",edits}}).dump();
            if(calls==3&&invalid_patch_kind==2){out->response_text="{\"replacements\":[{";out->reached_eog=false;}
        }
        return true;
    };
    CHECK(GenerateDynamicScene(ctx,model,cfg,nullptr,nullptr,&r));
    CHECK(calls==4&&r.repairs==2&&r.scene_text==program); // Bad patch never publishes or edits unrelated lines.
    }
    cfg.completion=[](const LlmGenerationConfig& c,const std::vector<LlmPromptMessage>&,LlmStreamCallback,void*,LlmGenerationResult* r) {
        r->success=true;r->reached_eog=true;r->response_text=c.use_json_grammar?SerializeSceneDesign(Design()):std::string("```scene\n")+program+"```\n";return true;
    };
    CHECK(GenerateDynamicScene(ctx,model,cfg,nullptr,nullptr,&r));CHECK(r.repairs==0&&r.scene_text==program);
    CHECK(json::parse(r.metrics_json)["removed_code_wrappers"]==1);
    cfg.direct=true;cfg.max_repairs=0;
    cfg.completion=[](const LlmGenerationConfig&,const std::vector<LlmPromptMessage>&,LlmStreamCallback,void*,LlmGenerationResult* r) {
        r->success=true;r->reached_eog=true;r->response_text=std::string("Here is your scene:\n```scene\n")+program+"```\n";return true;
    };
    CHECK(!GenerateDynamicScene(ctx,model,cfg,nullptr,nullptr,&r));
    cfg.direct=false;cfg.max_repairs=2;
    calls=0;cfg.completion=Completion(&calls,10);CHECK(!GenerateDynamicScene(ctx,model,cfg,nullptr,nullptr,&r));
    CHECK(calls==4&&r.repairs==2&&r.scene.triangles.empty());
    calls=0;cfg.completion=Completion(&calls,0,false);CHECK(!GenerateDynamicScene(ctx,model,cfg,nullptr,nullptr,&r));
    CHECK(calls==3&&r.scene.triangles.empty()); // EOS is mandatory, even for parseable JSON.
    calls=0;cfg.completion=Completion(&calls);
    auto stop=[](DynamicScenePhase phase,const char*,const char*,void*) {return phase!=DynamicScenePhase::Preview;};
    CHECK(!GenerateDynamicScene(ctx,model,cfg,stop,nullptr,&r));CHECK(r.cancelled&&r.preview_pixels.empty());
    CHECK(calls==2);
    for(const auto phase:{DynamicScenePhase::Design,DynamicScenePhase::Program,DynamicScenePhase::Repair,DynamicScenePhase::Audit}) {
        calls=0;cfg.completion=Completion(&calls,phase==DynamicScenePhase::Repair?1:0);
        auto stage=phase;
        auto cancel=[](DynamicScenePhase p,const char*,const char*,void* data) {return p!=*static_cast<DynamicScenePhase*>(data);};
        CHECK(!GenerateDynamicScene(ctx,model,cfg,cancel,&stage,&r));CHECK(r.cancelled);
    }
    cfg.total_timeout_ms=1;
    cfg.completion=[](const LlmGenerationConfig&,const std::vector<LlmPromptMessage>&,LlmStreamCallback,void*,LlmGenerationResult*) {
        std::this_thread::sleep_for(std::chrono::milliseconds(3));return false;
    };
    CHECK(!GenerateDynamicScene(ctx,model,cfg,nullptr,nullptr,&r));CHECK(json::parse(r.metrics_json)["timeout"]==true);
    cfg.total_timeout_ms=900000;calls=0;
    cfg.completion=[&](const LlmGenerationConfig& c,const std::vector<LlmPromptMessage>&,LlmStreamCallback,void*,LlmGenerationResult* r) {
        ++calls;r->success=true;r->reached_eog=true;
        r->response_text=calls<=2?"{}":c.purpose=="scene_design_v1"?SerializeSceneDesign(Design()):"bad";return true;
    };
    CHECK(!GenerateDynamicScene(ctx,model,cfg,nullptr,nullptr,&r));CHECK(calls==4&&r.repairs==2);
}
static void PersistenceAndNavigation() {
    SessionState state;char error[1024]={};CHECK(InitializeSessionState(kLocationSurveyPlateau,&state,error,sizeof(error)));
    // Isolated branch fixture: generated places do not inherit canonical links.
    state.current_place_id="generated:room_900000";state.origin_place_id=state.current_place_id;
    state.hard_state.current_location_id=kLocationUnknown;state.spatial_state=SpatialState();
    state.spatial_state.room_title="Origin";state.spatial_state.room_summary="Open arch";
    state.spatial_state.visible_objects={"datum"};state.generated_rooms.clear();state.room_links.clear();
    state.invisible_barriers.clear();state.next_generated_room_index=900001;
    GeneratedRoom original;original.room_id=state.current_place_id;original.spatial_state=state.spatial_state;original.scene_text=program;
    state.generated_rooms.push_back(original);
    std::string before;CHECK(SerializeSessionStateToJsonString(state,&before));
    HeadlessTurnConfig cfg;cfg.dynamic_scenes=true;cfg.request_candidate_scene=false;int calls=0;
    cfg.dynamic_scene_config.completion=Completion(&calls,10);
    HeadlessTurnResult turn;CHECK(!RunHeadlessTurnFromState(state,"north",cfg,&turn,error,sizeof(error)));
    std::string unchanged;CHECK(SerializeSessionStateToJsonString(state,&unchanged));CHECK(before==unchanged);
    CHECK(turn.generated_rooms_to_add.empty()&&turn.room_links_to_add.empty());
    CHECK(!turn.dynamic_scene_metrics_json.empty());
    CHECK(!json::parse(turn.dynamic_scene_metrics_json).at("success").get<bool>());
    UpdateSessionStateFromTurn("north",turn,&state);
    CHECK(SerializeSessionStateToJsonString(state,&unchanged));CHECK(before==unchanged);
    calls=0;cfg.dynamic_scene_config.completion=Completion(&calls);
    CHECK(RunHeadlessTurnFromState(state,"north",cfg,&turn,error,sizeof(error)));
    CHECK(turn.generated_room_created&&turn.generated_rooms_to_add.size()==1&&calls==2);
    UpdateSessionStateFromTurn("north",turn,&state);
    CHECK(state.hard_state.move_count==1);CHECK(state.generated_rooms.back().scene_source=="dynamic_v1");
    CHECK(SerializeSessionStateToJsonString(state,&before));SessionState restored;
    CHECK(ParseSessionStateFromJson(before.c_str(),&restored,error,sizeof(error)));
    CHECK(restored.generated_rooms.back().scene_text==program);
    CHECK(restored.generated_rooms.back().scene_hash==SceneTextHash(program));
    auto j=json::parse(before);j["generated_rooms"].back()["scene_text"]="tampered";
    CHECK(!ParseSessionStateFromJson(j.dump().c_str(),&restored,error,sizeof(error)));
    CHECK(RunHeadlessTurnFromState(state,"south",cfg,&turn,error,sizeof(error)));CHECK(calls==2);
    UpdateSessionStateFromTurn("south",turn,&state);
    CHECK(RunHeadlessTurnFromState(state,"north",cfg,&turn,error,sizeof(error)));CHECK(calls==2);
    CHECK(turn.rendered_scene_text==program&&!turn.generated_room_created);
    UpdateSessionStateFromTurn("north",turn,&state);
    CHECK(RunHeadlessTurnFromState(state,"look",cfg,&turn,error,sizeof(error)));CHECK(calls==2);
    CHECK(turn.rendered_scene_text==program&&!turn.generated_room_cache_refreshed);
    // Five unseen neighbors, with no source substitution, then a saved long session.
    for(int i=0;i<4;++i) {
        CHECK(RunHeadlessTurnFromState(state,"north",cfg,&turn,error,sizeof(error)));
        CHECK(turn.generated_room_created&&turn.generated_room_scene_source=="dynamic_v1");
        UpdateSessionStateFromTurn("north",turn,&state);
    }
    CHECK(state.generated_rooms.size()==6&&calls==10);
    const auto stable_hash=state.generated_rooms.back().scene_hash;
    for(int i=0;i<40;++i) {
        CHECK(RunHeadlessTurnFromState(state,"look",cfg,&turn,error,sizeof(error)));
        UpdateSessionStateFromTurn("look",turn,&state);
        CHECK(state.generated_rooms.back().scene_hash==stable_hash&&calls==10);
    }
    CHECK(RunHeadlessTurnFromState(state,"east",cfg,&turn,error,sizeof(error)));
    CHECK(!turn.generated_room_created&&calls==10);
    CHECK(turn.accepted);
    CHECK(SerializeSessionStateToJsonString(state,&before));
    cfg.stream_callback=[](HeadlessTurnStreamPhase,const char*,const char*,void*) { return false; };
    CHECK(!RunHeadlessTurnFromState(state,"north",cfg,&turn,error,sizeof(error)));
    CHECK(!turn.accepted);
    UpdateSessionStateFromTurn("north",turn,&state);
    CHECK(SerializeSessionStateToJsonString(state,&unchanged));CHECK(before==unchanged);
}
static void CanonicalBranches() {
    auto design=Design();
    design.lighting_mode="night"; // An artistic label must not override engine time.
    design.reserved_passages.push_back({"east",Vec3(8,1.2f,0),Vec3(2,2.2f,2)});
    design.reserved_passages.push_back({"west",Vec3(-8,1.2f,0),Vec3(2,2.2f,2)});
    HeadlessTurnConfig cfg;cfg.dynamic_scenes=true;cfg.request_candidate_scene=false;
    cfg.dynamic_scene_config.completion=[&](const LlmGenerationConfig& c,const std::vector<LlmPromptMessage>&,LlmStreamCallback,void*,LlmGenerationResult* r) {
        r->success=true;r->reached_eog=true;r->response_text=c.use_json_grammar?SerializeSceneDesign(design):program;return true;
    };
    const std::pair<LocationId,const char*> branches[]={{kLocationExtractionField,"east"},{kLocationScannerStation,"east"},
        {kLocationSurveyPlateau,"north"},{kLocationLabyrinthThreshold,"south"},{kLocationProspectShelter,"east"}};
    for(const auto& branch:branches) {
        SessionState state;char error[1024]={};HeadlessTurnResult turn;
        CHECK(InitializeSessionState(branch.first,&state,error,sizeof(error)));
        CHECK(RunHeadlessTurnFromState(state,branch.second,cfg,&turn,error,sizeof(error)));
        CHECK(turn.generated_room_created&&turn.generated_room_scene_source=="dynamic_v1");
        CHECK(turn.updated_spatial_state.time_of_day==turn.prospective_spatial_state.time_of_day);
        CHECK(turn.room_links_to_add.size()==2&&turn.rendered_scene_text==program);
    }
}
int main() {
    try {Geometry();Pipeline();PersistenceAndNavigation();CanonicalBranches();std::puts("Dynamic scenes: geometry, repairs, cancellation, atomic navigation and save integrity passed");return 0;}
    catch(const std::exception& e){std::fprintf(stderr,"%s\n",e.what());return 1;}
}
