#include "narrative_language.h"
#include "dynamic_scene.h"
#include "renderer.h"
#include "turn_runner.h"
#include "research_archive.h"
#include "coplanar_cleanup.h"
#include "exploration_policy.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include "nlohmann/json.hpp"
#include <cstdio>
#include <stdexcept>
#include <thread>
#include <chrono>
using namespace liminal;
using json=nlohmann::json;
#define CHECK(c) do { if(!(c)) throw std::runtime_error(std::string("Check failed at ")+std::to_string(__LINE__)+": "+#c); } while(false)

// A deterministic fake tokenizer; real Gemma counts are qualified separately.
static bool FakePromptCount(const std::vector<LlmPromptMessage>& messages,int* count,std::string*) {
    size_t bytes=0;for(const auto& m:messages)bytes+=m.content.size()+16;
    *count=static_cast<int>((bytes+3)/4);return true;
}
// Simulate constrained decoding in ordinary narrative fixtures. Choose the
// allowed map closest to the fixture's intended directions, independently of
// the engine's repair algorithm. Adversarial tests deliberately bypass this.
static json FitTestExitSchema(const LlmGenerationConfig& config,const json& preferred) {
    const auto schema=json::parse(config.json_schema).at("properties").at("exits");
    if(!schema.contains("anyOf"))return preferred;
    json selected;int best=17;
    for(const auto& option:schema.at("anyOf")) {
        json exits=json::object();int distance=0;
        for(const auto* direction:{"north","east","south","west"}) {
            exits[direction]=option.at("properties").at(direction).at("enum").at(0);
            if(exits[direction]!=preferred.at(direction))distance+=preferred.at(direction).get<bool>()?4:1;
        }
        if(distance<best){best=distance;selected=exits;}
    }
    CHECK(!selected.is_null());return selected;
}
static json GeometryContinuity(const std::vector<LlmPromptMessage>& messages) {
    const std::string marker="CONTINUITY_V1:\n";
    const auto pos=messages.at(1).content.find(marker);CHECK(pos!=std::string::npos);
    // A recomposition suffix may follow the JSON object.
    const auto tail=messages.at(1).content.substr(pos+marker.size());
    std::istringstream stream(tail);json context;stream>>context;return context;
}

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
    std::string windows_program;
    for(const char c:std::string(program)){if(c=='\n')windows_program+='\r';windows_program+=c;}
    CHECK(AuditDynamicScene(windows_program,ctx,nullptr,&scene,&audit));
    CHECK(AuditDynamicScene(Replace(program,"arch_left","arch_Left"),ctx,nullptr,&scene,&audit));
    {
    SceneViewConstraints view;view.landmark="marker";view.near_distance=3;view.max_near_coverage=.9f;
    CHECK(AuditDynamicScene(program,ctx,nullptr,&scene,&audit,&view));
    CHECK(audit.focal_coverage>0);
    auto hidden=std::string(program)+"box \"screen\" pos(0,2,3) size(12,4,0.5) rot(0,12,0) gray(0.5)\n";
    CHECK(!AuditDynamicScene(hidden,ctx,nullptr,&scene,&audit,&view));
    CHECK(audit.diagnostics[0].code=="landmark_not_visible");
    CHECK(audit.diagnostics[0].detail.find("screen blocks")!=std::string::npos);
    auto occluded=std::string(program)+"box \"cooling_block_4\" pos(0,2,-5) size(12,8,0.3) gray(0.5)\n";
    CHECK(!AuditDynamicScene(occluded,ctx,nullptr,&scene,&audit,&view));
    bool named_blocker=false;
    for(const auto& diagnostic:audit.diagnostics)
        if(diagnostic.code=="foreground_occlusion"&&diagnostic.object_id=="cooling_block_4")named_blocker=true;
    CHECK(named_blocker);
    auto inside=std::string(program)+"box \"tilted\" pos(0,1.6,-6) size(1,3,1) rot(20,35,10) gray(0.5)\n";
    CHECK(!AuditDynamicScene(inside,ctx,nullptr,&scene,&audit,&view));
    CHECK(audit.diagnostics[0].code=="camera_inside_mass");
    view.landmark="absent";
    CHECK(!AuditDynamicScene(program,ctx,nullptr,&scene,&audit,&view));
    view.landmark="marker";view.aspect=0;
    CHECK(!AuditDynamicScene(program,ctx,nullptr,&scene,&audit,&view));
    CHECK(audit.diagnostics[0].code=="invalid_view_contract");
    }
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
    SessionState state;char error[1024]={};CHECK(InitializeSessionState(kLocationQuarryThreshold,&state,error,sizeof(error)));
    // Isolated branch fixture: generated places do not inherit canonical links.
    state.current_place_id="generated:room_900000";state.origin_place_id=state.current_place_id;
    state.hard_state.current_location_id=kLocationUnknown;state.spatial_state=SpatialState();
    state.spatial_state.room_title="Origin";state.spatial_state.room_summary="Open arch";
    state.spatial_state.visible_objects={"datum"};state.generated_rooms.clear();state.room_links.clear();
    state.invisible_barriers.clear();state.next_generated_room_index=900001;
    GeneratedRoom original;original.room_id=state.current_place_id;original.spatial_state=state.spatial_state;original.scene_text=program;
    state.generated_rooms.push_back(original);
    std::string before;CHECK(SerializeSessionStateToJsonString(state,&before));
    HeadlessTurnConfig cfg;cfg.generation_config.prompt_token_counter=FakePromptCount;cfg.dynamic_scenes=true;cfg.request_candidate_scene=false;int calls=0;
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
    HeadlessTurnConfig cfg;cfg.generation_config.prompt_token_counter=FakePromptCount;cfg.dynamic_scenes=true;cfg.request_candidate_scene=false;
    cfg.dynamic_scene_config.completion=[&](const LlmGenerationConfig& c,const std::vector<LlmPromptMessage>&,LlmStreamCallback,void*,LlmGenerationResult* r) {
        r->success=true;r->reached_eog=true;r->response_text=c.use_json_grammar?SerializeSceneDesign(design):program;return true;
    };
    const std::pair<LocationId,const char*> branches[]={{kLocationQuarryThreshold,"east"}};
    for(const auto& branch:branches) {
        SessionState state;char error[1024]={};HeadlessTurnResult turn;
        CHECK(InitializeSessionState(branch.first,&state,error,sizeof(error)));
        CHECK(RunHeadlessTurnFromState(state,branch.second,cfg,&turn,error,sizeof(error)));
        CHECK(turn.generated_room_created&&turn.generated_room_scene_source=="dynamic_v1");
        CHECK(turn.updated_spatial_state.time_of_day==turn.prospective_spatial_state.time_of_day);
        CHECK(turn.room_links_to_add.size()==2&&turn.rendered_scene_text==program);
    }
}
static std::string LocalFixture() {
    return LocalSceneHeader()+
        "box \"portal\" pos(0,1,8) size(1,2,1) gray(0.4)\n"
        "box \"left\" pos(-4,3,10) size(2,6,6) gray(0.5)\n"
        "box \"right\" pos(4,3,10) size(2,6,6) gray(0.5)\n"
        "box \"lintel\" pos(0,6.5,10) size(10,1,6) gray(0.5)\n"
        "box \"slab_a\" pos(-7,1,15) size(2,2,4) gray(0.5)\n"
        "box \"slab_b\" pos(7,1,15) size(2,2,4) gray(0.5)\n"
        "box \"slab_c\" pos(-8,2,20) size(2,4,4) gray(0.5)\n"
        "box \"slab_d\" pos(8,2,20) size(2,4,4) gray(0.5)\n";
}
static void LocalPilot() {
    const auto local=LocalFixture();
    std::string problem;
    CHECK(ValidateLocalSceneContract(local,&problem));
    CHECK(ValidateLocalSceneContract(Replace(local,"camera eye(0,1.7,-10) target(0,2,8) up(0,1,0) fov(55)",
        "camera  fov(55.0) up(0,1,0) target(0,2,8) eye(0,1.7,-10)"),&problem));
    CHECK(!ValidateLocalSceneContract(Replace(local,"eye(0,1.7,-10)","eye(0,1.7,-9)"),&problem));
    SceneDesignContext context;Scene scene;SceneAuditReport report;SceneViewConstraints view;
    view.landmark="portal";view.require_return_clearance=true;
    CHECK(AuditDynamicScene(local,context,nullptr,&scene,&report,&view));
    {
        // An otherwise valid cul-de-sac is rejected only on the last frontier.
        const auto sides=local+
            "box \"left_limit\" pos(3,0.5,-10) size(0.2,1,2) gray(0.5)\n"
            "box \"right_limit\" pos(-3,0.5,-10) size(0.2,1,2) gray(0.5)\n";
        const auto culdesac=sides+"box \"forward_limit\" pos(0.7,0.5,-4) size(0.1,1,0.2) gray(0.5)\n";
        CHECK(AuditDynamicScene(culdesac,context,nullptr,&scene,&report,&view));
        CHECK(LocalSceneClearApproaches(scene)==std::vector<std::string>{"back"});
        auto last_frontier=context;last_frontier.require_onward_exit=true;
        CHECK(!AuditDynamicScene(culdesac,last_frontier,nullptr,&scene,&report,&view));
        CHECK(SerializeSceneAudit(report).find("exploration_continuation_blocked")!=std::string::npos);
        CHECK(AuditDynamicScene(sides,last_frontier,nullptr,&scene,&report,&view));
        CHECK(LocalSceneClearApproaches(scene)==std::vector<std::string>({"back","forward"}));
        DynamicSceneConfig guard;guard.fixed_frame=true;guard.landmark="portal";
        guard.coplanar_cleanup=false;guard.glass_supports=false;
        int attempts=0;bool repair=true;
        guard.completion=[&](const LlmGenerationConfig&,const std::vector<LlmPromptMessage>& messages,
            LlmStreamCallback,void*,LlmGenerationResult* r) {
            ++attempts;CHECK(messages[1].content.find("EXPLORATION:")!=std::string::npos);
            if(attempts==2)CHECK(messages.back().content.find("exploration_continuation_blocked")!=std::string::npos);
            r->success=true;r->reached_eog=true;r->response_text=repair&&attempts>1?sides:culdesac;return true;
        };
        LlmGenerationConfig model;model.prompt_token_counter=FakePromptCount;DynamicSceneResult generated;
        CHECK(GenerateDynamicScene(last_frontier,model,guard,nullptr,nullptr,&generated));
        CHECK(attempts==2&&generated.scene_text==sides);
        repair=false;attempts=0;
        CHECK(!GenerateDynamicScene(last_frontier,model,guard,nullptr,nullptr,&generated));
        CHECK(attempts==3&&generated.attempts.size()==3);
    }
    CHECK(!AuditDynamicScene(local+"box \"behind\" pos(0,1,-11) size(2,2,0.5) gray(0.5)\n",context,nullptr,&scene,&report,&view));
    CHECK(report.diagnostics[0].code=="return_clearance_blocked");
    // Remove a genuine opaque occluder locally, but never its supported child.
    const auto obstructed=local+"box \"screen\" pos(0,4,3) size(20,8,0.2) gray(0.5)\n";
    CHECK(!AuditDynamicScene(obstructed,context,nullptr,&scene,&report,&view));
    std::string repaired,removed;
    CHECK(RemoveLocalOccluder(obstructed,"portal",report,&repaired,&removed));
    CHECK(removed=="screen"&&repaired==local);
    const auto supported=obstructed+"box \"top\" pos(0,8.5,3) size(1,1,0.2) gray(0.5)\n";
    CHECK(!AuditDynamicScene(supported,context,nullptr,&scene,&report,&view));
    CHECK(!RemoveLocalOccluder(supported,"portal",report,&repaired,&removed));
    DynamicSceneConfig repair_config;repair_config.fixed_frame=true;repair_config.landmark="portal";
    int compositions=0;
    repair_config.completion=[&](const LlmGenerationConfig&,const std::vector<LlmPromptMessage>&,LlmStreamCallback,void*,LlmGenerationResult* r) {
        ++compositions;r->success=true;r->reached_eog=true;r->response_text=obstructed;return true;
    };
    DynamicSceneResult candidate;LlmGenerationConfig model;
    CHECK(GenerateDynamicScene(context,model,repair_config,nullptr,nullptr,&candidate));
    CHECK(compositions==1&&candidate.scene_text==local&&candidate.attempts.size()==1);
    CHECK(candidate.attempts[0].removed_objects==std::vector<std::string>{"screen"});
    std::vector<unsigned int> seeds;
    repair_config.completion=[&](const LlmGenerationConfig& c,const std::vector<LlmPromptMessage>& m,LlmStreamCallback,void*,LlmGenerationResult* r) {
        seeds.push_back(c.seed);r->success=true;r->reached_eog=true;r->response_text=seeds.size()==3?local:"broken";
        if(seeds.size()==3)CHECK(m.size()==2); // Recompose, don't replay the previous candidate.
        return true;
    };
    CHECK(GenerateDynamicScene(context,model,repair_config,nullptr,nullptr,&candidate));
    CHECK(seeds.size()==3&&seeds[0]==seeds[1]&&seeds[2]!=seeds[0]&&candidate.attempts.size()==3);
    seeds.clear();repair_config.max_repairs=1;
    CHECK(!GenerateDynamicScene(context,model,repair_config,nullptr,nullptr,&candidate));CHECK(seeds.size()==2);
    SessionState state;char error[1024]={};
    CHECK(InitializeSessionState(kLocationQuarryThreshold,&state,error,sizeof(error)));
    state.language=kGameLanguageFrench;
    HeadlessTurnConfig cfg;cfg.generation_config.prompt_token_counter=FakePromptCount;cfg.local_scenes=true; // Composition seam; no real inference in this test.
    int narratives=0;
    cfg.narrative_completion=[&](const LlmGenerationConfig& c,const std::vector<LlmPromptMessage>& messages,
        LlmStreamCallback,void*,LlmGenerationResult* r) {
        ++narratives;CHECK(messages[0].content.find("ENGINE OUTCOME")!=std::string::npos);
        CHECK(messages[1].content.find("portal")==std::string::npos);
        r->success=true;r->reached_eog=true;
        r->response_text=R"({"title":"Les masses et le ciel","summary":"Le passage s'inscrit entre deux masses sombres.","narration":"Les blocs découpent une bande de ciel. Vous êtes au milieu des masses, votre regard suit la distance qui les sépare. La lumière s’attarde sur leurs arêtes, tandis que le sol prend devant vous la teinte sourde des masses environnantes."})";
        auto response=json::parse(r->response_text);
        const auto request=json::parse(messages[1].content);
        CHECK(request.at("carried_items").size()==3);
        CHECK(request.at("carried_items")[0].at("id")=="survey map slate");
        response["thermal"]={{"external_temperature_c",request.at("temperature_external_c")},
            {"body_temperature_c",request.at("temperature_body_c")},{"reason","Protection stable."}};
        response["exits"]={{"north",false},{"east",true},{"south",false},{"west",true}};
        for(auto it=request["exit_proposal"]["fixed"].begin();it!=request["exit_proposal"]["fixed"].end();++it)response["exits"][it.key()]=it.value();
        response["exits"]=FitTestExitSchema(c,response["exits"]);
        r->response_text=response.dump();
        return true;
    };
    int calls=0;bool bad=true,truncated=false;
    cfg.dynamic_scene_config.completion=[&](const LlmGenerationConfig& c,const std::vector<LlmPromptMessage>& messages,
        LlmStreamCallback cb,void* data,LlmGenerationResult* r) {
        ++calls;CHECK(c.n_predict==2048);CHECK(messages[0].content==LocalSceneSystemPrompt());
        if(cb&&!cb("","",data))return false;
        r->success=true;r->reached_eog=!truncated;r->response_text=bad?"broken":local;return true;
    };
    std::string before,after;CHECK(SerializeSessionStateToJsonString(state,&before));HeadlessTurnResult turn;
    CHECK(!RunHeadlessTurnFromState(state,"east",cfg,&turn,error,sizeof(error)));
    CHECK(calls==3&&turn.generated_rooms_to_add.empty()&&turn.room_links_to_add.empty());
    UpdateSessionStateFromTurn("east",turn,&state);CHECK(SerializeSessionStateToJsonString(state,&after));CHECK(before==after);
    bad=false;truncated=true;calls=0;
    CHECK(!RunHeadlessTurnFromState(state,"east",cfg,&turn,error,sizeof(error)));CHECK(calls==3);
    truncated=false;calls=0;
    cfg.stream_callback=[](HeadlessTurnStreamPhase,const char*,const char*,void*){return false;};
    CHECK(!RunHeadlessTurnFromState(state,"east",cfg,&turn,error,sizeof(error)));CHECK(calls==0);
    CHECK(turn.generated_rooms_to_add.empty()&&turn.room_links_to_add.empty());cfg.stream_callback=nullptr;
    CHECK(RunHeadlessTurnFromState(state,"east",cfg,&turn,error,sizeof(error)));CHECK(calls==1);
    CHECK(turn.generated_room_scene_source=="local_direct_v2");
    CHECK(turn.generated_rooms_to_add[0].spatial_state.room_title=="Les masses et le ciel");
    UpdateSessionStateFromTurn("east",turn,&state);CHECK(state.generated_rooms.back().scene_contract_version==3);
    CHECK(SerializeSessionStateToJsonString(state,&after));SessionState restored;
    CHECK(ParseSessionStateFromJson(after.c_str(),&restored,error,sizeof(error)));
    CHECK(RunHeadlessTurnFromState(restored,"look",cfg,&turn,error,sizeof(error)));CHECK(calls==1);
    CHECK(turn.rendered_scene_text==local&&!turn.generated_room_cache_refreshed);
    CHECK(RunHeadlessTurnFromState(restored,"west",cfg,&turn,error,sizeof(error)));UpdateSessionStateFromTurn("west",turn,&restored);
    CHECK(RunHeadlessTurnFromState(restored,"east",cfg,&turn,error,sizeof(error)));CHECK(calls==1&&turn.rendered_scene_text==local);
    UpdateSessionStateFromTurn("east",turn,&restored);
    CHECK(RunHeadlessTurnFromState(restored,"east",cfg,&turn,error,sizeof(error)));
    CHECK(calls==2&&turn.generated_room_created);
    UpdateSessionStateFromTurn("east",turn,&restored);
    CHECK(restored.generated_rooms.size()==2&&restored.room_links.size()==4);
    CHECK(restored.generated_rooms[0].scene_seed!=restored.generated_rooms[1].scene_seed);
    CHECK(json::parse(restored.generated_rooms[0].scene_design_json)["brief"]==
          json::parse(restored.generated_rooms[1].scene_design_json)["brief"]);
    CHECK(restored.generated_rooms[0].scene_prompt_version!=restored.generated_rooms[1].scene_prompt_version);
    const auto stable_scene=restored.generated_rooms.back().scene_text;
    const auto stable_inventory=restored.hard_state.inventory_items;
    // A pre-existing room with prose in another language is repaired in place.
    {
        SessionState italian=restored;italian.language=kGameLanguageItalian;
        italian.spatial_state.room_summary="Vous restez dans la carrière, devant les masses de pierre. Votre souffle demeure ici, sous la lumière du ciel.";
        for(auto& cached:italian.generated_rooms)if(cached.room_id==italian.current_place_id)
            cached.spatial_state.room_summary=italian.spatial_state.room_summary;
        HeadlessTurnConfig translated=cfg;int localization_calls=0;
        translated.narrative_completion=[&](const LlmGenerationConfig&,const std::vector<LlmPromptMessage>& messages,
            LlmStreamCallback,void*,LlmGenerationResult* r) {
            ++localization_calls;const auto request=json::parse(messages.at(1).content);
            CHECK(request.at("language_code")=="it");CHECK(request.at("exit_proposal").at("may_improvise")==false);
            CHECK(!request.at("metadata_language_repair").get<std::string>().empty());
            r->success=true;r->reached_eog=true;
            r->response_text=json({{"title","La pietra immobile"},{"summary","La luce resta sulle pareti della cava, davanti a te."},
                {"narration","Rimani davanti alle masse della cava. La luce ne segue gli spigoli, mentre il tuo respiro misura il silenzio."},
                {"thermal",{{"external_temperature_c",request.at("temperature_external_c")},
                    {"body_temperature_c",request.at("temperature_body_c")},{"reason","Stabile"}}},
                {"exits",request.at("exit_proposal").at("fixed")}}).dump();return true;
        };
        const int compositions_before=calls;HeadlessTurnResult localized;
        CHECK(RunHeadlessTurnFromState(italian,"guarda",translated,&localized,error,sizeof(error)));
        CHECK(!localized.used_turn_fallback&&localization_calls==1&&calls==compositions_before);
        CHECK(localized.rendered_scene_text==stable_scene);
        CHECK(localized.updated_spatial_state.blocked_exits==italian.spatial_state.blocked_exits);
        CHECK(localized.updated_spatial_state.room_title=="La pietra immobile");
        CHECK(localized.generated_rooms_to_add.size()==1);
        UpdateSessionStateFromTurn("guarda",localized,&italian);
        CHECK(italian.language==kGameLanguageItalian);
        std::string saved;CHECK(SerializeSessionStateToJsonString(italian,&saved));SessionState reload;
        CHECK(ParseSessionStateFromJson(saved.c_str(),&reload,error,sizeof(error)));
        CHECK(reload.spatial_state.room_title=="La pietra immobile");
        bool found=false;for(const auto& cached:reload.generated_rooms)if(cached.room_id==reload.current_place_id) {
            found=true;CHECK(cached.spatial_state.room_title=="La pietra immobile");CHECK(cached.scene_text==stable_scene);
        }
        CHECK(found);
    }

    const int moves=restored.hard_state.move_count;
    for(const auto* command:{"regarder la carriere","regarder entre la fente","prendre le bloc","noter les distances","mesurer la temperature"}) {
        CHECK(RunHeadlessTurnFromState(restored,command,cfg,&turn,error,sizeof(error)));
        CHECK(!turn.turn_result.narration.empty());
        CHECK(turn.rendered_scene_text==stable_scene);
        CHECK(turn.updated_hard_state.move_count==moves&&turn.updated_hard_state.inventory_items==stable_inventory);
        UpdateSessionStateFromTurn(command,turn,&restored);
    }
    CHECK(restored.field_notes.size()==2&&narratives>=7);
    CHECK(SerializeSessionStateToJsonString(restored,&before));
    SessionState saved_notes;CHECK(ParseSessionStateFromJson(before.c_str(),&saved_notes,error,sizeof(error)));
    CHECK(saved_notes.field_notes==restored.field_notes);
    // A narrative failure preserves the accepted scene and commits no invented effect.
    cfg.narrative_completion=[](const LlmGenerationConfig&,const std::vector<LlmPromptMessage>&,LlmStreamCallback,void*,LlmGenerationResult*){return false;};
    CHECK(RunHeadlessTurnFromState(restored,"regarder le bloc",cfg,&turn,error,sizeof(error)));
    CHECK(turn.used_turn_fallback&&turn.rendered_scene_text==stable_scene&&turn.updated_hard_state.inventory_items==stable_inventory);
    bad=true;
    CHECK(!RunHeadlessTurnFromState(restored,"east",cfg,&turn,error,sizeof(error)));
    CHECK(calls==5&&turn.generated_rooms_to_add.empty()&&turn.room_links_to_add.empty());
    UpdateSessionStateFromTurn("east",turn,&restored);
    std::string failed;CHECK(SerializeSessionStateToJsonString(restored,&failed));CHECK(failed==before);
    CHECK(RunHeadlessTurnFromState(restored,"west",cfg,&turn,error,sizeof(error)));
    UpdateSessionStateFromTurn("west",turn,&restored);
    CHECK(RunHeadlessTurnFromState(restored,"west",cfg,&turn,error,sizeof(error)));
    UpdateSessionStateFromTurn("west",turn,&restored);
    CHECK(restored.current_place_id=="canonical:quarry_threshold"&&calls==5);
    auto tampered=json::parse(after);tampered["generated_rooms"].back()["scene_text"]="changed";
    CHECK(!ParseSessionStateFromJson(tampered.dump().c_str(),&restored,error,sizeof(error)));

    // Coupled proposals: branch in every cardinal direction, heat once per turn,
    // and next-call sampling. These tests use deterministic seams, not a real LLM.
    CHECK(InitializeSessionState(kLocationQuarryThreshold,&state,error,sizeof(error)));
    state.language=kGameLanguageFrench;
    cfg.generation_config.temperature=0;
    std::vector<float> narrative_temperatures,scene_temperatures;
    int proposal_calls=0;bool refuse_proposal=false,excessive_heat=false,cancel_after_proposal=false,cancel_now=false;
    std::string next_exit="north";
    cfg.dynamic_scene_config.completion=[&](const LlmGenerationConfig& c,const std::vector<LlmPromptMessage>&,
        LlmStreamCallback,void*,LlmGenerationResult* r) {
        scene_temperatures.push_back(c.temperature);r->success=true;r->reached_eog=true;r->response_text=local;return true;
    };
    cfg.narrative_completion=[&](const LlmGenerationConfig& c,const std::vector<LlmPromptMessage>& messages,
        LlmStreamCallback,void*,LlmGenerationResult* r) {
        ++proposal_calls;narrative_temperatures.push_back(c.temperature);
        if(refuse_proposal)return false;
        const auto request=json::parse(messages[1].content);
        const auto& exits=request.at("exit_proposal");
        json proposed={{"north",false},{"east",false},{"south",false},{"west",false}};
        if(exits.at("may_improvise").get<bool>())proposed[next_exit]=true;
        for(auto it=exits.at("fixed").begin();it!=exits.at("fixed").end();++it)proposed[it.key()]=it.value();
        proposed=FitTestExitSchema(c,proposed);
        r->success=true;r->reached_eog=true;
        r->response_text=json({{"title","Les distances "+std::to_string(proposal_calls)},
            {"summary","Vous retrouvez la ligne des masses sous le ciel."},
            {"narration","Vous distinguez des masses sombres sous le ciel. Leurs arêtes prolongent la perspective, sans livrer de mesure certaine. Devant cette étendue, votre regard suit patiemment les intervalles et revient aux distances. "+std::to_string(proposal_calls)},
            {"exits",proposed},{"thermal",{{"external_temperature_c",request.at("temperature_external_c")},
            {"body_temperature_c",request.at("temperature_body_c").get<float>()+(excessive_heat?2.f:.2f)},
            {"reason","Exposition modérée pendant cette action."}}}}).dump();
        cancel_now=cancel_after_proposal;return true;
    };
    auto act=[&](const char* command) {
        CHECK(RunHeadlessTurnFromState(state,command,cfg,&turn,error,sizeof(error)));
        CHECK(json::parse(turn.narrative_metrics_json).at("proposal_accepted")==true);
        UpdateSessionStateFromTurn(command,turn,&state);
    };
    act("east");
    CHECK(fabsf(state.hard_state.body_temperature_c-37.2f)<.001f);
    CHECK(std::find(state.spatial_state.blocked_exits.begin(),state.spatial_state.blocked_exits.end(),"north")==state.spatial_state.blocked_exits.end());
    CHECK(state.spatial_state.blocked_exits.size()==4-DrawExplorationExitTarget(cfg.generation_config.seed,1));
    CHECK(fabsf(narrative_temperatures.back()-.1f)<.001f&&fabsf(scene_temperatures.back()-.1f)<.001f);
    const auto first_scene=state.generated_rooms[0].scene_text;
    act("attendre");CHECK(fabsf(state.hard_state.body_temperature_c-37.4f)<.001f);
    CHECK(fabsf(narrative_temperatures.back()-.132f)<.001f);
    CHECK(scene_temperatures.size()==1);
    next_exit="east";act("north");CHECK(turn.generated_room_created);
    CHECK(json::parse(state.generated_rooms.back().scene_design_json).at("return_direction")=="south");
    CHECK(fabsf(scene_temperatures.back()-.164f)<.001f);
    act("south");CHECK(turn.rendered_scene_text==first_scene&&scene_temperatures.size()==2);
    act("north");CHECK(scene_temperatures.size()==2&&proposal_calls==5);
    next_exit="south";act("east");next_exit="west";act("south");next_exit="north";act("west");
    CHECK(state.generated_rooms.size()==5&&state.room_links.size()==10);
    CHECK(json::parse(state.generated_rooms.back().scene_design_json).at("return_direction")=="east");
    CHECK(SerializeSessionStateToJsonString(state,&before));
    CHECK(ParseSessionStateFromJson(before.c_str(),&restored,error,sizeof(error)));
    CHECK(restored.spatial_state.blocked_exits==state.spatial_state.blocked_exits);
    CHECK(restored.hard_state.body_temperature_c==state.hard_state.body_temperature_c);
    const auto body=state.hard_state.body_temperature_c;
    excessive_heat=true;
    CHECK(RunHeadlessTurnFromState(state,"wait",cfg,&turn,error,sizeof(error)));
    CHECK(turn.used_turn_fallback&&turn.updated_hard_state.body_temperature_c==body);
    CHECK(!json::parse(turn.narrative_metrics_json).at("proposal_accepted").get<bool>());
    excessive_heat=false;cancel_after_proposal=true;
    cfg.stream_user_data=&cancel_now;
    cfg.stream_callback=[](HeadlessTurnStreamPhase,const char*,const char*,void* data) {return !*static_cast<bool*>(data);};
    CHECK(!RunHeadlessTurnFromState(state,"wait",cfg,&turn,error,sizeof(error)));
    UpdateSessionStateFromTurn("wait",turn,&state);
    CHECK(SerializeSessionStateToJsonString(state,&after));CHECK(after==before);
    cfg.stream_callback=nullptr;cancel_after_proposal=false;cancel_now=false;
    refuse_proposal=true;
    CHECK(RunHeadlessTurnFromState(state,"north",cfg,&turn,error,sizeof(error)));
    UpdateSessionStateFromTurn("north",turn,&state);
    CHECK(state.generated_rooms.back().metadata_fallback_used);
    const auto pending_geometry=state.generated_rooms.back().scene_text;
    const auto compositions_before=scene_temperatures.size();
    CHECK(SerializeSessionStateToJsonString(state,&before));
    CHECK(ParseSessionStateFromJson(before.c_str(),&restored,error,sizeof(error)));
    CHECK(restored.generated_rooms.back().metadata_fallback_used);
    const int proposals_before_inventory=proposal_calls;
    const auto pending_body=state.hard_state.body_temperature_c;
    CHECK(RunHeadlessTurnFromState(state,"inventaire",cfg,&turn,error,sizeof(error)));
    CHECK(turn.turn_result.intent=="read_inventory"&&turn.generated_rooms_to_add.empty());
    CHECK(turn.rendered_scene_text==pending_geometry&&turn.updated_hard_state.body_temperature_c==pending_body);
    UpdateSessionStateFromTurn("inventaire",turn,&state);
    CHECK(state.generated_rooms.back().metadata_fallback_used&&proposal_calls==proposals_before_inventory);
    CHECK(state.hard_state.inventory_items==MakeInitialHardState().inventory_items);
    refuse_proposal=false;next_exit="east";act("look");
    CHECK(!state.generated_rooms.back().metadata_fallback_used);
    CHECK(state.generated_rooms.back().scene_text==pending_geometry&&scene_temperatures.size()==compositions_before);
    CHECK(SerializeSessionStateToJsonString(state,&before));
    CHECK(ParseSessionStateFromJson(before.c_str(),&restored,error,sizeof(error)));
    auto bad_provenance=json::parse(before);
    auto design=json::parse(bad_provenance["generated_rooms"].back()["scene_generation"]["design"].get<std::string>());
    design["return_direction"]="north";
    bad_provenance["generated_rooms"].back()["scene_generation"]["design"]=design.dump();
    bad_provenance["generated_rooms"].back()["scene_generation"]["plan_sha256"]=SceneTextHash(design.dump());
    CHECK(!ParseSessionStateFromJson(bad_provenance.dump().c_str(),&restored,error,sizeof(error)));
    // Actual triangle clearance: a transparent side obstacle blocks navigation too.
    CHECK(LoadSceneFromSceneText("clear.scene",local.c_str(),&scene,error,sizeof(error)));
    CHECK(LocalSceneClearApproaches(scene).size()==4);
    const auto side_wall=local+"glass_box \"side_wall\" pos(3,1,-10) size(0.2,2,2) gray(0.5)\n";
    CHECK(LoadSceneFromSceneText("side.scene",side_wall.c_str(),&scene,error,sizeof(error)));
    const auto approaches=LocalSceneClearApproaches(scene);
    CHECK(approaches.size()==3&&std::find(approaches.begin(),approaches.end(),"left")==approaches.end());
}
static void ExplorationContinuity() {
    // Adversarial narrator: it requests no branches at all. Test the actual
    // traversal pipeline, including geometry, fallback, persistence and rollback.
    for(int scenario=0;scenario<6;++scenario) {
        SessionState state;HeadlessTurnResult turn;char error[1024]={};
        CHECK(InitializeSessionState(kLocationQuarryThreshold,&state,error,sizeof(error)));
        HeadlessTurnConfig cfg;cfg.local_scenes=true;cfg.generation_config.prompt_token_counter=FakePromptCount;
        if(scenario>=4)while(DrawExplorationExitTarget(cfg.generation_config.seed,3)!=(scenario==4?3:2))++cfg.generation_config.seed;
        cfg.dynamic_scene_config.coplanar_cleanup=false;cfg.dynamic_scene_config.glass_supports=false;
        int compositions=0,narrations=0;bool fail=false,violate=false,cancel=false,cancelled=false;
        json request,schema;
        const std::string geometry=LocalFixture()+(scenario==3?
            "box \"left_limit\" pos(3,0.5,-10) size(0.2,1,2) gray(0.5)\n"
            "box \"right_limit\" pos(-3,0.5,-10) size(0.2,1,2) gray(0.5)\n":"");
        cfg.dynamic_scene_config.completion=[&](const LlmGenerationConfig&,const std::vector<LlmPromptMessage>&,
            LlmStreamCallback,void*,LlmGenerationResult* r) {
            ++compositions;r->success=true;r->reached_eog=true;r->response_text=geometry;return true;
        };
        cfg.narrative_completion=[&](const LlmGenerationConfig& c,const std::vector<LlmPromptMessage>& messages,
            LlmStreamCallback,void*,LlmGenerationResult* r) {
            ++narrations;request=json::parse(messages[1].content);schema=json::parse(c.json_schema);
            if(fail)return false;
            json exits={{"north",false},{"east",false},{"south",false},{"west",false}};
            for(auto it=request["exit_proposal"]["fixed"].begin();it!=request["exit_proposal"]["fixed"].end();++it)
                exits[it.key()]=it.value();
            if(scenario>=4&&compositions==3&&request["exit_proposal"]["may_improvise"]==true) {
                int extra=0;
                for(const auto& possible:request["exit_proposal"]["geometrically_possible"]) {
                    const auto direction=possible.get<std::string>();
                    if(!exits[direction].get<bool>()&&(scenario==5||extra==0)){exits[direction]=true;++extra;}
                }
            } else exits=FitTestExitSchema(c,exits);
            if(violate)exits["north"]=false; // Break the established return, never silently repair it.
            r->success=true;r->reached_eog=true;
            r->response_text=json({{"title","Still stone"},{"summary","You study the stone under a pale sky."},
                {"narration","You remain before the still stone. The pale light rests along its edges, and the dust settles slowly around your visor. You study the intervals between the blocks while your breath marks the pause. The distant sky holds its muted colour. "+std::to_string(narrations)},
                {"thermal",{{"external_temperature_c",request["temperature_external_c"]},
                    {"body_temperature_c",request["temperature_body_c"].get<float>()+(violate?.1f:0.f)},{"reason","Stable protection."}}},
                {"exits",exits}}).dump();
            if(cancel)cancelled=true;
            return true;
        };
        auto act=[&](const std::string& command) {
            CHECK(RunHeadlessTurnFromState(state,command.c_str(),cfg,&turn,error,sizeof(error)));
            UpdateSessionStateFromTurn(command.c_str(),turn,&state);
        };
        auto save=[&]() {std::string text;CHECK(SerializeSessionStateToJsonString(state,&text));return text;};
        // Two optional dead ends exhaust all but the starting southern route.
        act("north");CHECK(state.spatial_state.blocked_exits.size()==3);act("south");
        act("east");CHECK(state.spatial_state.blocked_exits.size()==3);act("west");
        CHECK(compositions==2&&state.current_place_id=="canonical:quarry_threshold");
        fail=scenario==1;violate=scenario==2;
        const auto body=state.hard_state.body_temperature_c;
        act("south");
        const auto third=state.generated_rooms.back();
        const auto policy=json::parse(third.scene_design_json).at("exit_balance");
        std::vector<std::string> reserved;
        for(const auto* direction:{"east","south","west"})
            if(std::find(third.spatial_state.blocked_exits.begin(),third.spatial_state.blocked_exits.end(),direction)==third.spatial_state.blocked_exits.end())reserved.push_back(direction);
        const int expected=scenario==3?2:policy.at("target_open_exits").get<int>();
        CHECK(reserved.size()==expected-1);
        CHECK(state.spatial_state.blocked_exits.size()==3-reserved.size());
        CHECK(third.metadata_fallback_used==(fail||violate));
        CHECK(state.hard_state.body_temperature_c==body&&compositions==3);
        CHECK(policy.at("allow_dead_end")==false);
        for(const auto& option:schema["properties"]["exits"]["anyOf"]) {
            int count=0;for(const auto* direction:{"north","east","south","west"})
                count+=option["properties"][direction]["enum"][0].get<bool>()?1:0;
            CHECK(count==expected&&option["properties"]["north"]["enum"]==json::array({true}));
        }
        for(const auto& direction:reserved) {
            CHECK(direction!="north"); // Return is not a new branch.
        }
        if(scenario>=4) {
            const auto metrics=json::parse(turn.narrative_metrics_json);
            CHECK(metrics.at("fallback_reason")=="exit_count_adjusted");
            CHECK(metrics.at("exit_adjustments").at(scenario==4?"opened":"closed").size()==(scenario==4?1:2));
            CHECK(turn.used_turn_fallback&&turn.turn_result.narration!=json::parse(turn.raw_response_text).at("narration"));
            CHECK(state.spatial_state.room_title!=json::parse(turn.raw_response_text).at("title"));
        }
        CHECK(ParseSessionStateFromJson(save().c_str(),&state,error,sizeof(error)));
        if(fail||violate) {
            fail=false;violate=false;act("look");
            CHECK(!state.generated_rooms.back().metadata_fallback_used);
            CHECK(state.spatial_state.blocked_exits==third.spatial_state.blocked_exits);
            for(const auto& direction:reserved)CHECK(request["exit_proposal"]["fixed"][direction]==true);
        }
        act("north");act("south");
        CHECK(compositions==3&&turn.rendered_scene_text==geometry);
        CHECK(state.current_place_id==third.room_id&&state.spatial_state.blocked_exits==third.spatial_state.blocked_exits);
        CHECK(json::parse(state.generated_rooms.back().scene_design_json).at("exit_balance")==policy);
        if(scenario==0) {
            // Reject tampered reservation metadata even if its hash is updated.
            const auto snapshot=json::parse(save());
            for(const auto& invalid:{json::array({"north"}),json::array({"up"}),json::array({reserved[0],reserved[0]})}) {
                auto altered=snapshot;auto& generation=altered["generated_rooms"].back()["scene_generation"];
                auto provenance=json::parse(generation["design"].get<std::string>());
                provenance["exploration_required"]=invalid;generation["design"]=provenance.dump();
                generation["plan_sha256"]=SceneTextHash(provenance.dump());
                SessionState rejected;CHECK(!ParseSessionStateFromJson(altered.dump().c_str(),&rejected,error,sizeof(error)));
            }
            // Cancellation on the next discovery leaves the whole graph intact.
            const auto before=save();cancel=true;cfg.stream_user_data=&cancelled;
            cfg.stream_callback=[](HeadlessTurnStreamPhase,const char*,const char*,void* data){return !*static_cast<bool*>(data);};
            CHECK(!RunHeadlessTurnFromState(state,reserved[0].c_str(),cfg,&turn,error,sizeof(error)));
            CHECK(cancelled&&turn.generated_rooms_to_add.empty()&&turn.room_links_to_add.empty());
            UpdateSessionStateFromTurn(reserved[0].c_str(),turn,&state);CHECK(save()==before);
            cfg.stream_callback=nullptr;cancel=false;
            // Follow the unknown branches depth first, backtracking out of dead
            // ends. The frontier must be replenished repeatedly, beyond 3 rooms.
            for(int steps=0;state.generated_rooms.size()<8&&steps<30;++steps) {
                std::string direction;
                for(const auto* candidate:{"north","east","south","west"}) {
                    if(std::find(state.spatial_state.blocked_exits.begin(),state.spatial_state.blocked_exits.end(),candidate)!=state.spatial_state.blocked_exits.end())continue;
                    bool known=false;for(const auto& link:state.room_links)
                        if(link.from_place_id==state.current_place_id&&CardinalDirectionToString(link.direction)==std::string(candidate))known=true;
                    if(!known){direction=candidate;break;}
                }
                if(direction.empty())for(const auto& room:state.generated_rooms)if(room.room_id==state.current_place_id)
                    direction=json::parse(room.scene_design_json).at("return_direction").get<std::string>();
                CHECK(!direction.empty());act(direction);
            }
            CHECK(state.generated_rooms.size()==8);
        }
    }
}
static void ContinuityJourney() {
    SessionState state;HeadlessTurnResult turn;char error[1024]={};
    CHECK(InitializeSessionState(kLocationQuarryThreshold,&state,error,sizeof(error)));
    state.language=kGameLanguageFrench;
    HeadlessTurnConfig cfg;cfg.local_scenes=true;cfg.generation_config.prompt_token_counter=FakePromptCount;
    int scene_calls=0,narrative_calls=0;bool reject_first=true;
    // This continuity fixture needs two distinct forward branches in A and C.
    while(DrawExplorationExitTarget(cfg.generation_config.seed,1)!=3||
          DrawExplorationExitTarget(cfg.generation_config.seed,3)!=3)++cfg.generation_config.seed;
    json last_geometry;std::string expected_origin,expected_command;
    std::vector<std::string> geometry_prompts;
    cfg.dynamic_scene_config.completion=[&](const LlmGenerationConfig&,const std::vector<LlmPromptMessage>& messages,
        LlmStreamCallback,void*,LlmGenerationResult* result) {
        ++scene_calls;last_geometry=GeometryContinuity(messages);geometry_prompts.push_back(messages[1].content);
        CHECK(last_geometry.at("origin").at("id")==expected_origin);
        CHECK(last_geometry.at("transition").at("command")==expected_command);
        CHECK(!last_geometry.at("origin").at("description").get<std::string>().empty());
        CHECK(last_geometry.at("origin").at("accepted_geometry").contains("visible_geometry"));
        CHECK(messages[1].content.find("colossal abandoned extraction block")==std::string::npos);
        CHECK(messages[1].content.find("stepped quarry court")==std::string::npos);
        CHECK(messages[1].content.find("internal ID, not a door")!=std::string::npos);
        if(scene_calls==2)CHECK(messages.size()==4); // Repair retains the same origin and event data.
        result->success=true;result->reached_eog=true;
        result->response_text=reject_first?"broken":LocalFixture()+
            "box \"screen\" pos(0,4,3) size(20,8,0.2) gray(0.5)\n";
        reject_first=false;return true;
    };
    cfg.narrative_completion=[&](const LlmGenerationConfig& c,const std::vector<LlmPromptMessage>& messages,
        LlmStreamCallback,void*,LlmGenerationResult* result) {
        ++narrative_calls;const auto request=json::parse(messages[1].content);
        CHECK(request.at("continuity").at("origin").at("id")==expected_origin);
        if(request.at("exit_proposal").at("may_improvise")==true) {
            CHECK(request.at("continuity").at("origin")==last_geometry.at("origin"));
            CHECK(request.at("continuity").at("recent_events")==last_geometry.at("recent_events"));
            CHECK(request.at("facts").contains("visible_geometry"));
            for(const auto& mass:request.at("facts").at("visible_geometry"))
                CHECK(mass.at("dimensions_metres")!=json::array({20,8,.2})); // Removed screen is absent.
        }
        json exits={{"north",true},{"east",true},{"south",true},{"west",true}};
        for(auto it=request["exit_proposal"]["fixed"].begin();it!=request["exit_proposal"]["fixed"].end();++it)exits[it.key()]=it.value();
        exits=FitTestExitSchema(c,exits);
        result->success=true;result->reached_eog=true;
        result->response_text=json({{"title","Les masses "+std::to_string(narrative_calls)},
            {"summary","Vous retrouvez la lumière sur les masses de pierre. Les intervalles restent visibles. "+std::to_string(narrative_calls)},
            {"narration","Vous distinguez les masses de pierre sous le ciel. Leurs arêtes dessinent une perspective silencieuse, tandis que votre regard parcourt les intervalles. La lumière souligne leur hauteur et votre souffle accompagne cette observation attentive. "+std::to_string(narrative_calls)},
            {"exits",exits},{"thermal",{{"external_temperature_c",request.at("temperature_external_c")},
            {"body_temperature_c",request.at("temperature_body_c")},{"reason","Protection stable."}}}}).dump();return true;
    };
    auto act=[&](const char* command) {
        expected_origin=state.current_place_id;expected_command=command;
        CHECK(RunHeadlessTurnFromState(state,command,cfg,&turn,error,sizeof(error)));
        UpdateSessionStateFromTurn(command,turn,&state);
    };
    act("prendre le bloc");CHECK(state.history.back().engine_outcome.find("No movement or object manipulation")!=std::string::npos);
    act("east");CHECK(scene_calls==2&&narrative_calls==1); // One repair, no planning call.
    CHECK(last_geometry.at("recent_events").size()==1);
    CHECK(last_geometry["recent_events"][0]["outcome"].get<std::string>().find("No movement or object manipulation")!=std::string::npos);
    CHECK(geometry_prompts[0]==geometry_prompts[1]);
    const auto a=state.current_place_id,description_a=state.spatial_state.room_summary;
    const auto geometry_a=state.generated_rooms.back().scene_text;
    CHECK(geometry_a==LocalFixture());
    act("north");const auto b=state.current_place_id;
    act("south");CHECK(state.current_place_id==a&&scene_calls==3);
    CHECK(state.spatial_state.room_summary==description_a&&turn.rendered_scene_text==geometry_a);
    // A -> B -> A -> new C: origin is A although B was the last room created.
    act("east");CHECK(scene_calls==4&&narrative_calls==4);
    CHECK(last_geometry["origin"]["id"]==a&&last_geometry["origin"]["description"]==description_a);
    CHECK(last_geometry["recent_events"].back()["place_id"]==a);
    CHECK(state.generated_rooms.size()==3&&state.reference_descriptions.count(a)&&state.reference_descriptions.count(b));
    const auto metrics=json::parse(turn.dynamic_scene_metrics_json);
    CHECK(metrics["calls"][0]["messages"][1]["content"]==geometry_prompts.back());
    CHECK(metrics["calls"][0]["continuity_selection"]["success"]==true);
    // Only technical IDs/seeds vary with discovery index. Prompts stay the same.
    auto altered=state;altered.next_generated_room_index+=100;
    expected_origin=state.current_place_id;expected_command="east";
    cfg.narrative_completion={};
    // Stop after capturing composition so this comparison has no narrative side effects.
    cfg.dynamic_scene_config.completion=[&](const LlmGenerationConfig&,const std::vector<LlmPromptMessage>& m,
        LlmStreamCallback,void*,LlmGenerationResult* r) {geometry_prompts.push_back(m[1].content);r->stop_reason="cancelled";return false;};
    CHECK(!RunHeadlessTurnFromState(state,"east",cfg,&turn,error,sizeof(error)));
    CHECK(!RunHeadlessTurnFromState(altered,"east",cfg,&turn,error,sizeof(error)));
    CHECK(geometry_prompts[geometry_prompts.size()-1]==geometry_prompts[geometry_prompts.size()-2]);
    auto contrasting=state;
    contrasting.history.back().place_description="Vous retrouvez une étendue claire. La lumière souligne la profondeur.";
    CHECK(!RunHeadlessTurnFromState(contrasting,"east",cfg,&turn,error,sizeof(error)));
    CHECK(geometry_prompts.back()!=geometry_prompts[geometry_prompts.size()-2]);
    CHECK(geometry_prompts.back().find("une étendue claire")!=std::string::npos);
    // An oversized history is selected for inference without deleting saved events or places.
    auto long_history=state;
    for(int i=0;i<30;++i) {
        auto event=state.history.back();event.player_command="observer "+std::to_string(i);
        event.place_description="Une lumière pâle éclaire la pierre. ";
        for(int sentence=0;sentence<40;++sentence)event.place_description+="Les masses demeurent sous le ciel étoilé. ";
        long_history.history.push_back(event);
    }
    std::string long_before,long_after;CHECK(SerializeSessionStateToJsonString(long_history,&long_before));
    CHECK(!RunHeadlessTurnFromState(long_history,"east",cfg,&turn,error,sizeof(error)));
    auto selection=json::parse(turn.dynamic_scene_metrics_json)["calls"][0]["continuity_selection"];
    CHECK(selection["success"]==true&&selection["omitted_events"].get<int>()>0);
    CHECK(selection["memory_tokens"].get<int>()<=1024);
    CHECK(SerializeSessionStateToJsonString(long_history,&long_after));CHECK(long_before==long_after);
    const auto calls_before=geometry_prompts.size();cfg.generation_config.n_ctx=128;
    CHECK(!RunHeadlessTurnFromState(state,"east",cfg,&turn,error,sizeof(error)));
    CHECK(geometry_prompts.size()==calls_before); // No backend request with insufficient mandatory context.
    CHECK(json::parse(turn.dynamic_scene_metrics_json)["error"].get<std::string>().find("context_limit")!=std::string::npos);
    cfg.generation_config.n_ctx=8192;
    // Neither rejected candidate entered the published state or research snapshot.
    std::string before,after;CHECK(SerializeSessionStateToJsonString(state,&before));
    UpdateSessionStateFromTurn("east",turn,&state);CHECK(SerializeSessionStateToJsonString(state,&after));CHECK(before==after);
    SessionState reloaded;CHECK(ParseSessionStateFromJson(before.c_str(),&reloaded,error,sizeof(error)));
    CHECK(reloaded.reference_descriptions==state.reference_descriptions);
    CHECK(reloaded.history.back().engine_outcome==state.history.back().engine_outcome);
    CHECK(reloaded.generated_rooms[0].scene_text==geometry_a);
}

static void InventoryTurns() {
    SessionState state;char error[1024]={};HeadlessTurnResult turn;HeadlessTurnConfig cfg;cfg.generation_config.prompt_token_counter=FakePromptCount;
    CHECK(InitializeSessionState(kLocationQuarryThreshold,&state,error,sizeof(error)));
    state.language=kGameLanguageFrench;cfg.local_scenes=true;
    int narrative_calls=0,scene_calls=0;
    cfg.narrative_completion=[&](const LlmGenerationConfig&,const std::vector<LlmPromptMessage>&,
        LlmStreamCallback,void*,LlmGenerationResult*) {++narrative_calls;return false;};
    cfg.dynamic_scene_config.completion=[&](const LlmGenerationConfig&,const std::vector<LlmPromptMessage>&,
        LlmStreamCallback,void*,LlmGenerationResult*) {++scene_calls;return false;};
    const auto possessions=state.hard_state.inventory_items;
    const auto blocked=state.spatial_state.blocked_exits;
    std::string canonical;
    for(const auto* command:{"i","  INVENTAIRE ! ","Qu’est-ce que je porte ?","examiner la tablette",
        "regarder la craie","examiner les pinces","prendre la craie","utiliser les pinces",
        "poser la tablette","jeter les pinces","prendre le portail","prendre la craie avec les pinces",
        "examiner la craie et les pinces"}) {
        CHECK(RunHeadlessTurnFromState(state,command,cfg,&turn,error,sizeof(error)));
        CHECK(json::parse(turn.narrative_metrics_json).at("engine_only")==true);
        CHECK(turn.updated_hard_state.inventory_items==possessions);
        CHECK(turn.updated_hard_state.body_temperature_c==37.f&&turn.updated_hard_state.external_temperature_c==464);
        CHECK(turn.updated_hard_state.move_count==0&&turn.updated_spatial_state.blocked_exits==blocked);
        CHECK(turn.generated_rooms_to_add.empty()&&turn.room_links_to_add.empty()&&turn.llm_call_count==0);
        if(canonical.empty())canonical=turn.rendered_scene_text;
        CHECK(turn.rendered_scene_text==canonical&&!turn.turn_result.narration.empty());
        UpdateSessionStateFromTurn(command,turn,&state);
    }
    CHECK(narrative_calls==0&&scene_calls==0&&state.history.size()==13);
    CHECK(RunHeadlessTurnFromState(state,"prendre la craie",cfg,&turn,error,sizeof(error)));
    CHECK(turn.turn_result.intent=="inventory_already_carried");
    CHECK(turn.turn_result.narration.find("déjà")!=std::string::npos);
    state.hard_state.inventory_items={"legacy keepsake"};
    CHECK(RunHeadlessTurnFromState(state,"inventaire",cfg,&turn,error,sizeof(error)));
    CHECK(turn.turn_result.narration.find("legacy keepsake")!=std::string::npos);
    CHECK(RunHeadlessTurnFromState(state,"examiner la craie",cfg,&turn,error,sizeof(error)));
    CHECK(turn.turn_result.intent=="inventory_item_absent");
    CHECK(RunHeadlessTurnFromState(state,"utiliser les pinces",cfg,&turn,error,sizeof(error)));
    CHECK(turn.turn_result.intent=="inventory_item_absent");
    std::string serialized;CHECK(SerializeSessionStateToJsonString(state,&serialized));SessionState restored;
    CHECK(ParseSessionStateFromJson(serialized.c_str(),&restored,error,sizeof(error)));
    CHECK(restored.hard_state.inventory_items==state.hard_state.inventory_items);
    state.hard_state.inventory_items.clear();cfg.local_scenes=false;
    CHECK(RunHeadlessTurnFromState(state,"i",cfg,&turn,error,sizeof(error)));
    CHECK(turn.turn_result.intent=="read_inventory"&&narrative_calls==0&&scene_calls==0);
    CHECK(turn.turn_result.narration.find("craie")==std::string::npos);
    cfg.local_scenes=true;
    // Prose cannot invent an acquisition, even on the general narrative path.
    cfg.narrative_completion=[&](const LlmGenerationConfig&,const std::vector<LlmPromptMessage>& messages,
        LlmStreamCallback,void*,LlmGenerationResult* r) {
        const auto request=json::parse(messages[1].content);
        CHECK(request.at("carried_items").empty());
        r->success=true;r->reached_eog=true;
        r->response_text=json({{"title","Le seuil"},{"summary","Le sol sous le ciel."},
            {"narration","Vous ramassez une clé dorée et la rangez avec vos outils pour poursuivre votre voyage."},
            {"exits",request.at("exit_proposal").at("fixed")},
            {"thermal",{{"external_temperature_c",464},{"body_temperature_c",37},{"reason","Stable."}}}}).dump();return true;
    };
    CHECK(RunHeadlessTurnFromState(state,"observer le ciel",cfg,&turn,error,sizeof(error)));
    CHECK(turn.used_turn_fallback&&turn.updated_hard_state.inventory_items.empty());
    CHECK(json::parse(turn.narrative_metrics_json).at("fallback_reason")=="unvalidated_inventory_effect");
    CHECK(turn.turn_result.narration.find("clé")==std::string::npos);
    CHECK(SerializeSessionStateToJsonString(state,&serialized));
    cfg.stream_callback=[](HeadlessTurnStreamPhase,const char*,const char*,void*) {return false;};
    CHECK(!RunHeadlessTurnFromState(state,"inventaire",cfg,&turn,error,sizeof(error)));
    UpdateSessionStateFromTurn("inventaire",turn,&state);
    std::string unchanged;CHECK(SerializeSessionStateToJsonString(state,&unchanged));CHECK(unchanged==serialized);
}

static void LanguageContinuity() {
    const GameLanguage languages[]={kGameLanguageEnglish,kGameLanguageFrench,kGameLanguageNorwegian,
        kGameLanguageDanish,kGameLanguageGerman,kGameLanguageItalian};
    const char* prose[]={
        "You remain here, before the stone and the light. Through your visor you see the edges ahead; beneath them the ground remains still. Your breath marks the pause, while the quarry holds its silence around you.",
        "Vous restez ici, devant la pierre et la lumière. À travers votre visière, vous regardez les arêtes des masses ; sous elles, le sol demeure immobile. Votre souffle mesure cette pause, dans le silence de la carrière.",
        "Du står her foran steinen og lyset. Gjennom visiret ser du kantene foran deg; under dem ligger bakken stille. Pusten din måler denne pausen, mens steinbruddet holder på stillheten rundt deg. Du kjenner draktens vekt.",
        "Du står her foran stenen og lyset. Gennem visiret ser du kanterne foran dig; under dem ligger jorden stille. Dit åndedræt måler denne pause, mens stenbruddet holder på stilheden omkring dig. Du mærker dragtens vægt.",
        "Du stehst hier vor dem Stein und dem Licht. Durch dein Visier siehst du die Kanten vor dir; unter ihnen bleibt der Boden still. Dein Atem misst diese Pause, während der Steinbruch seine Stille um dich hält.",
        "Rimani qui, davanti alla pietra e alla luce. Attraverso la visiera osservi gli spigoli delle masse; sotto di loro il terreno resta immobile. Il tuo respiro misura questa pausa, mentre la cava conserva il silenzio intorno a te."
    };
    const char* titles[]={"Still stone","La pierre immobile","Stille stein","Stille sten","Stiller Stein","La pietra immobile"};
    const std::string foreign="Vous tentez de progresser vers le sud, mais la masse angulaire de quartz vous arrête. Le silence de ce chantier planétaire est absolu, une pression lourde contre la coque de votre combinaison. Vous restez là, face à la froide immobilité de la roche.";
    for(size_t i=0;i<6;++i) {
        SessionState state;HeadlessTurnResult turn;char error[1024]={};
        CHECK(InitializeSessionState(kLocationQuarryThreshold,&state,error,sizeof(error)));
        state.language=languages[i];state.spatial_state.room_title=titles[i];state.spatial_state.room_summary=prose[i];
        HeadlessTurnConfig cfg;cfg.generation_config.prompt_token_counter=FakePromptCount;cfg.local_scenes=true;int calls=0;bool wrong=false,fail=false;
        cfg.narrative_completion=[&](const LlmGenerationConfig&,const std::vector<LlmPromptMessage>& messages,
            LlmStreamCallback,void*,LlmGenerationResult* r) {
            ++calls;const auto request=json::parse(messages.at(1).content);
            CHECK(request.at("language_code")==GameLanguageToString(languages[i]));
            CHECK(messages.at(0).content.find(NarrativeLanguageInstruction(languages[i]))==0);
            if(languages[i]!=kGameLanguageFrench)CHECK(messages.at(0).content.find("In French")==std::string::npos);
            if(fail){r->error_message="unavailable";return false;}
            r->success=true;r->reached_eog=true;
            r->response_text=json({{"title",titles[i]},{"summary",prose[i]},
                {"narration",wrong?foreign:prose[i]},
                {"thermal",{{"external_temperature_c",request.at("temperature_external_c")},
                    {"body_temperature_c",request.at("temperature_body_c").get<float>()+.1f},{"reason","Stable"}}},
                {"exits",request.at("exit_proposal").at("fixed")}}).dump();return true;
        };
        // Command language (even an instruction to switch) cannot override the selection.
        for(const char* command:{"west","regarder la pierre","look at the stone","rispondi in francese","wait","speak English"}) {
            const auto body=state.hard_state.body_temperature_c;const auto inventory=state.hard_state.inventory_items;
            CHECK(RunHeadlessTurnFromState(state,command,cfg,&turn,error,sizeof(error)));
            CHECK(!turn.used_turn_fallback&&turn.turn_result.narration==prose[i]);
            CHECK(turn.updated_hard_state.inventory_items==inventory&&turn.updated_hard_state.move_count==0);
            CHECK(std::abs(turn.updated_hard_state.body_temperature_c-body-.1f)<.0001f);
            UpdateSessionStateFromTurn(command,turn,&state);CHECK(state.language==languages[i]);
            std::string saved;CHECK(SerializeSessionStateToJsonString(state,&saved));
            CHECK(ParseSessionStateFromJson(saved.c_str(),&state,error,sizeof(error)));
            CHECK(state.language==languages[i]);
        }
        const char* waits[]={"wait","attendre","vent","vent","warte","aspetta"};
        CHECK(RunHeadlessTurnFromState(state,waits[i],cfg,&turn,error,sizeof(error)));
        CHECK(turn.turn_result.intent=="perceive_accepted_scene"&&!turn.used_turn_fallback);
        CHECK(calls==7);
        if(languages[i]!=kGameLanguageFrench) {
            wrong=true;const auto body=state.hard_state.body_temperature_c;
            CHECK(RunHeadlessTurnFromState(state,"west",cfg,&turn,error,sizeof(error)));
            CHECK(calls==8&&turn.used_turn_fallback&&turn.turn_result.narration!=foreign);
            const auto metrics=json::parse(turn.narrative_metrics_json);
            CHECK(metrics.at("fallback_reason")=="wrong_output_language");
            CHECK(metrics.at("proposal_accepted")==true&&metrics.at("language")==GameLanguageToString(languages[i]));
            CHECK(std::abs(turn.updated_hard_state.body_temperature_c-body-.1f)<.0001f);
            CHECK(!IsClearlyWrongNarrativeLanguage(turn.turn_result.narration,languages[i]));
            wrong=false;
        }
        fail=true;
        // A foreign cached description must not leak through a failed backend call.
        state.spatial_state.room_summary=languages[i]==kGameLanguageFrench?prose[0]:foreign;
        CHECK(RunHeadlessTurnFromState(state,"look",cfg,&turn,error,sizeof(error)));
        CHECK(turn.used_turn_fallback&&!IsClearlyWrongNarrativeLanguage(turn.turn_result.narration,languages[i]));
        CHECK(!IsClearlyWrongNarrativeLanguage(turn.updated_spatial_state.room_summary,languages[i]));
        CHECK(turn.updated_hard_state.body_temperature_c==state.hard_state.body_temperature_c);
        UpdateSessionStateFromTurn("look",turn,&state);CHECK(state.language==languages[i]);
    }
}

static void SingleStart() {
    CHECK(RunNavigationSelfTest(stderr));
    SessionState state,restored;char error[1024]={};
    CHECK(InitializeSessionState(kLocationQuarryThreshold,&state,error,sizeof(error)));
    CHECK(state.generated_rooms.empty()&&state.room_links.empty()&&state.invisible_barriers.empty());
    CHECK(state.spatial_state.blocked_exits==std::vector<std::string>({"west"}));
    std::string saved;CHECK(SerializeSessionStateToJsonString(state,&saved));
    for(const char* removed:{"extraction_field","crystal_cut","scanner_station","survey_plateau","labyrinth_threshold","prospect_shelter"}) {
        LocationId id=kLocationUnknown;CHECK(!ParseLocationId(removed,&id));
        auto old=json::parse(saved);old["hard_state"]["current_location_id"]=removed;
        CHECK(!ParseSessionStateFromJson(old.dump().c_str(),&restored,error,sizeof(error)));
        CHECK(std::string(error).find("removed location")!=std::string::npos);
        old=json::parse(saved);old["origin_place_id"]=std::string("canonical:")+removed;
        CHECK(!ParseSessionStateFromJson(old.dump().c_str(),&restored,error,sizeof(error)));
    }
    CHECK(ParseSessionStateFromJson(saved.c_str(),&restored,error,sizeof(error)));
    Scene scene;
    CHECK(LoadSceneFromPath("assets/scenes/eryx_quarry_threshold.scene",&scene,error,sizeof(error)));
    int glass_triangles=0;
    for(const auto& triangle:scene.triangles)
        if(scene.materials[triangle.material_index].model==kMaterialModelDielectricGlass)++glass_triangles;
    CHECK(glass_triangles==12); // A closed cuboid, not a non-refracting plane.
    SceneDesignContext context;SceneAuditReport audit;
    const auto glass=Replace(program,"box \"arch_left\"","glass_box \"arch_left\"");
    CHECK(AuditDynamicScene(glass,context,nullptr,&scene,&audit));
    CHECK(!AuditDynamicScene(glass+"glass_box \"camera_cage\" pos(0,1.6,-6) size(1,1,1) gray(0.6)\n",context,nullptr,&scene,&audit));
    CHECK(audit.diagnostics[0].code=="camera_inside_mass");
}
static void ResearchArchives() {
    const auto root=std::filesystem::temp_directory_path()/
        ("liminal-archive-test-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    SessionState initial;char error[1024]={};
    CHECK(InitializeSessionState(kLocationQuarryThreshold,&initial,error,sizeof(error)));
    LlmGenerationConfig model;RenderConfig render;render.width=render.height=1;
    ResearchArchive first,second;
    CHECK(first.Start(root.u8string(),initial,model,render));
    CHECK(first.Image(initial,{80,90,100}));
    CHECK(first.Command("east"));CHECK(first.Pending());
    HeadlessTurnResult failure;failure.raw_scene_audit_response_text="unfinished scene";
    CHECK(first.Failure("cancelled",failure));CHECK(!first.Pending());
    CHECK(first.Close(initial));
    CHECK(second.Start(root.u8string(),initial,model,render));CHECK(first.Path()!=second.Path());
    CHECK(second.Image(initial,{80,90,100}));CHECK(second.Close(initial));
    for(const auto* archive:{&first,&second}) {
        const auto dir=std::filesystem::u8path(archive->Path());
        CHECK(std::filesystem::exists(dir/"places/place-00000.scene"));
        CHECK(std::filesystem::exists(dir/"places/place-00000.txt"));
        CHECK(std::filesystem::file_size(dir/"places/place-00000.png")>20);
        std::ifstream saved(dir/"state.json");json state; saved>>state;
        CHECK(state["generated_rooms"].empty());
        CHECK(state["current_place_id"]=="canonical:quarry_threshold");
    }
    CHECK(std::filesystem::exists(std::filesystem::u8path(first.Path())/"turns/turn-00001.candidate.scene"));
    CHECK(!std::filesystem::exists(std::filesystem::u8path(second.Path())/"turns/turn-00001.json"));
    // Only our uniquely created temporary test directory is removed.
    CHECK(std::filesystem::equivalent(root.parent_path(),std::filesystem::temp_directory_path()));
    std::filesystem::remove_all(root);
}
static void GlassSupportJourney() {
    const auto opaque=Replace(LocalFixture(),"pos(-7,1,15)","pos(-7,4,15)");
    SessionState state;char error[1024]={};
    CHECK(InitializeSessionState(kLocationQuarryThreshold,&state,error,sizeof(error)));
    HeadlessTurnConfig cfg;cfg.local_scenes=true;cfg.generation_config.prompt_token_counter=FakePromptCount;
    int compositions=0,narratives=0;
    cfg.dynamic_scene_config.completion=[&](const LlmGenerationConfig&,const std::vector<LlmPromptMessage>& messages,
            LlmStreamCallback,void*,LlmGenerationResult* r) {
        ++compositions;
        for(const auto& m:messages)CHECK(m.content.find("engine_glass_triangle")==std::string::npos);
        r->success=true;r->reached_eog=true;r->response_text=opaque;return true;
    };
    cfg.narrative_completion=[&](const LlmGenerationConfig&,const std::vector<LlmPromptMessage>& messages,
            LlmStreamCallback,void*,LlmGenerationResult*) {
        ++narratives;const auto request=json::parse(messages[1].content);
        if(narratives==1) {
            CHECK(request["facts"]["engine_glass_supports"]["ids"].size()==1);
            CHECK(!request["facts"]["visible_geometry"].empty());
        }
        return false; // Narrative fallback must retain augmented geometry.
    };
    HeadlessTurnResult turn;CHECK(RunHeadlessTurnFromState(state,"east",cfg,&turn,error,sizeof(error)));
    CHECK(compositions==1&&narratives==1);
    const auto& room=turn.generated_rooms_to_add.at(0);
    CHECK(room.opaque_scene_text==opaque);CHECK(room.scene_text!=opaque);
    const auto supports=json::parse(room.glass_supports_json);
    CHECK(supports["status"]=="applied"&&supports["added_supports"]==1);
    CHECK(supports["final_sha256"]==room.scene_hash);
    CHECK(turn.scene_attempts[0].final_program==opaque);
    std::string invalid;CHECK(!ValidateLocalSceneContract(room.scene_text,&invalid));
    const auto final=room.scene_text;
    UpdateSessionStateFromTurn("east",turn,&state);
    std::string saved;CHECK(SerializeSessionStateToJsonString(state,&saved));SessionState restored;
    CHECK(ParseSessionStateFromJson(saved.c_str(),&restored,error,sizeof(error)));
    CHECK(restored.generated_rooms.back().scene_text==final);
    CHECK(restored.generated_rooms.back().glass_supports_json==room.glass_supports_json);
    auto bad=json::parse(saved);bad["generated_rooms"].back()["opaque_scene_text"]="tampered";
    SessionState rejected;CHECK(!ParseSessionStateFromJson(bad.dump().c_str(),&rejected,error,sizeof(error)));
    for(const auto* command:{"west","east"}) {
        CHECK(RunHeadlessTurnFromState(restored,command,cfg,&turn,error,sizeof(error)));
        UpdateSessionStateFromTurn(command,turn,&restored);
    }
    CHECK(compositions==1&&turn.rendered_scene_text==final&&!turn.generated_room_cache_refreshed);
    const auto root=std::filesystem::temp_directory_path()/
        ("liminal-glass-archive-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    ResearchArchive archive;RenderConfig render;render.width=render.height=1;
    CHECK(archive.Start(root.u8string(),restored,cfg.generation_config,render));
    CHECK(archive.Image(restored,{80,90,100}));CHECK(archive.Close(restored));
    auto read=[&](const char* suffix) {
        std::ifstream file(std::filesystem::u8path(archive.Path())/(std::string("places/place-00000")+suffix),std::ios::binary);
        CHECK(file.good());std::ostringstream text;text<<file.rdbuf();return text.str();
    };
    CHECK(read(".scene")==final&&read(".opaque.scene")==opaque);
    CHECK(json::parse(read(".supports.json"))["final_sha256"]==SceneTextHash(final));
    Scene reloaded;CHECK(LoadSceneFromSceneText("archive",read(".scene").c_str(),&reloaded,error,sizeof(error)));
    CHECK(reloaded.triangles.size()==turn.rendered_scene.triangles.size());
    CHECK(std::filesystem::equivalent(root.parent_path(),std::filesystem::temp_directory_path()));
    std::filesystem::remove_all(root);
}
static void CoplanarSupportJourney() {
    const auto source=LocalFixture()+
        "box \"floating_a\" pos(-.5,4,5) size(3,2,2) gray(.25)\n"
        "box \"floating_b\" pos(.5,4,5) size(3,2,2) gray(.75)\n";
    SessionState state;char error[1024]={};
    CHECK(InitializeSessionState(kLocationQuarryThreshold,&state,error,sizeof(error)));
    HeadlessTurnConfig cfg;cfg.local_scenes=true;cfg.generation_config.prompt_token_counter=FakePromptCount;
    int compositions=0,narratives=0;json facts;
    cfg.dynamic_scene_config.completion=[&](const LlmGenerationConfig&,const std::vector<LlmPromptMessage>&,
            LlmStreamCallback,void*,LlmGenerationResult* r) {
        ++compositions;r->success=true;r->reached_eog=true;r->response_text=source;return true;
    };
    cfg.narrative_completion=[&](const LlmGenerationConfig&,const std::vector<LlmPromptMessage>& messages,
            LlmStreamCallback,void*,LlmGenerationResult*) {
        if(++narratives==1)facts=json::parse(messages[1].content)["facts"];
        return false;
    };
    HeadlessTurnResult turn;CHECK(RunHeadlessTurnFromState(state,"east",cfg,&turn,error,sizeof(error)));
    CHECK(compositions==1&&narratives==1);
    const auto room=turn.generated_rooms_to_add.at(0);
    const auto report=json::parse(room.coplanar_cleanup_json),support=json::parse(room.glass_supports_json);
    CHECK(report["success"]==true&&report["accepted_translations"].get<int>()>0&&report["remaining_conflicts"]==0);
    CHECK(report["camera_eyes"].size()==8);
    CHECK(room.pre_coplanar_scene_text==source&&room.opaque_scene_text!=source&&room.scene_text!=room.opaque_scene_text);
    CHECK(report["source_sha256"]==SceneTextHash(source));
    CHECK(report["corrected_sha256"]==SceneTextHash(room.opaque_scene_text));
    CHECK(support["opaque_sha256"]==report["corrected_sha256"]);
    CHECK(support["status"]=="applied"&&support["added_supports"].get<int>()>0&&support["final_sha256"]==room.scene_hash);
    CHECK(turn.scene_attempts.size()==1&&turn.scene_attempts[0].raw==source&&turn.scene_attempts[0].final_program==source);
    CHECK(facts["engine_glass_supports"]["ids"].size()==support["added_supports"].get<size_t>());
    // Independently sample the corrected scene: the narrator must receive its
    // visible coverage, not a fresh audit of the raw model program.
    Scene corrected;SceneAuditReport audited;SceneDesignContext context;
    CHECK(AuditDynamicScene(room.opaque_scene_text,context,nullptr,&corrected,&audited));
    std::vector<int> coverage;
    for(size_t id=1;id<audited.object_names.size();++id) {
        int count=0;for(const auto pixel:audited.object_ids)if(pixel==id)++count;
        if(count)coverage.push_back(count);
    }
    std::sort(coverage.begin(),coverage.end(),std::greater<int>());
    CHECK(facts["visible_geometry"].size()==std::min<size_t>(10,coverage.size()));
    for(size_t i=0;i<facts["visible_geometry"].size();++i)
        CHECK(facts["visible_geometry"][i]["visible_percent"]==std::round(coverage[i]*1000.0/(256*128))/10);
    for(const auto& move:report["modified_blocks"]) {
        bool found=false;
        for(const auto& box:corrected.boxes)if(box.id==move["id"]) {
            found=true;CHECK((json{box.center.x,box.center.y,box.center.z}==move["final_position"]));
        }
        CHECK(found);
    }
    CHECK(CleanupCoplanarFaces(room.scene_text).scene_text==room.opaque_scene_text);
    UpdateSessionStateFromTurn("east",turn,&state);
    std::string saved;CHECK(SerializeSessionStateToJsonString(state,&saved));SessionState restored;
    CHECK(ParseSessionStateFromJson(saved.c_str(),&restored,error,sizeof(error)));
    CHECK(restored.generated_rooms.back().coplanar_cleanup_json==room.coplanar_cleanup_json);
    for(const auto* field:{"pre_coplanar_scene_text","opaque_scene_text"}) {
        auto corrupt=json::parse(saved);corrupt["generated_rooms"].back()[field]="tampered";
        SessionState rejected;CHECK(!ParseSessionStateFromJson(corrupt.dump().c_str(),&rejected,error,sizeof(error)));
    }
    for(const auto* command:{"west","east"}) {
        CHECK(RunHeadlessTurnFromState(restored,command,cfg,&turn,error,sizeof(error)));
        UpdateSessionStateFromTurn(command,turn,&restored);
    }
    CHECK(compositions==1&&turn.rendered_scene_text==room.scene_text&&!turn.generated_room_cache_refreshed);
    const auto root=std::filesystem::temp_directory_path()/
        ("liminal-coplanar-archive-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    ResearchArchive archive;RenderConfig render;render.width=render.height=1;
    CHECK(archive.Start(root.u8string(),restored,cfg.generation_config,render));CHECK(archive.Close(restored));
    auto read=[&](const char* suffix) {
        std::ifstream file(std::filesystem::u8path(archive.Path())/(std::string("places/place-00000")+suffix),std::ios::binary);
        CHECK(file.good());std::ostringstream text;text<<file.rdbuf();return text.str();
    };
    CHECK(read(".pre-coplanar.scene")==source&&read(".opaque.scene")==room.opaque_scene_text&&read(".scene")==room.scene_text);
    CHECK(json::parse(read(".coplanar.json"))==report&&json::parse(read(".supports.json"))==support);
    CHECK(std::filesystem::equivalent(root.parent_path(),std::filesystem::temp_directory_path()));
    std::filesystem::remove_all(root);
}
int main() {
    try {Geometry();Pipeline();PersistenceAndNavigation();CanonicalBranches();LocalPilot();ExplorationContinuity();ContinuityJourney();InventoryTurns();LanguageContinuity();SingleStart();ResearchArchives();GlassSupportJourney();CoplanarSupportJourney();std::puts("Dynamic/local scenes: geometry, single start, exploration, archives, repairs, cancellation, COP before glass and save integrity passed");return 0;}
    catch(const std::exception& e){std::fprintf(stderr,"%s\n",e.what());return 1;}
}
