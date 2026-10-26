#include "player_action_fixture.h"
#include "object_interactions.h"
#include "inventory.h"
#include "turn_runner.h"
#include "renderer.h"
#include "research_archive.h"
#include "nlohmann/json.hpp"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <stdexcept>
using namespace liminal;
using json=nlohmann::json;
#define REQUIRE(c) do {if(!(c))throw std::runtime_error(std::string("Object journey line ")+std::to_string(__LINE__)+": "+#c);} while(false)

namespace {
const char* base=R"(room "Object fixture"
camera eye(0,1.6,-6) target(0,0.6,4) up(0,1,0) fov(50)
sky zenith(0.1) horizon(0.4) nadir(0.05) band(0.35) curve(1.6) noise(0)
distant_light direction(-0.6,-1,0.35) intensity(3.5) color(1,0.88,0.55) angular_radius(0.65)
box "floor" pos(0,-0.1,0) size(20,0.2,20) gray(0.6)
box "wall" pos(-4,2,1) size(1,4,8) gray(0.6)
)";
const std::string a="generated:room_000001",b="generated:room_000002";
SessionState Fixture() {
    SessionState s;char error[512]={};REQUIRE(InitializeSessionState(kLocationQuarryThreshold,&s,error,sizeof(error)));
    s.language=kGameLanguageFrench;s.current_place_id=a;s.origin_place_id=a;
    s.hard_state.current_location_id=kLocationUnknown;s.spatial_state=SpatialState();
    s.spatial_state.room_title="Le seuil clair";s.spatial_state.room_summary="Un petit caillou repose ici. Un tas de gravats borde le mur.";
    s.spatial_state.blocked_exits={"east","west"};s.generated_rooms.clear();s.room_links.clear();s.invisible_barriers.clear();
    for(const auto& id:{a,b}) {GeneratedRoom r;r.room_id=id;r.spatial_state=s.spatial_state;r.scene_text=base;r.scene_contract_version=0;
        r.arrival_narration=r.spatial_state.room_summary;s.generated_rooms.push_back(r);s.reference_descriptions[id]=r.arrival_narration;}
    RoomLink ab;ab.from_place_id=a;ab.to_place_id=b;ab.direction=kDirectionNorth;s.room_links.push_back(ab);
    RoomLink ba;ba.from_place_id=b;ba.to_place_id=a;ba.direction=kDirectionSouth;s.room_links.push_back(ba);
    return s;
}
struct Harness {
    HeadlessTurnConfig cfg;int interpretations=0,narrations=0;std::string mode;
    json proposal;bool cancel=false;std::string observed_prose;
    Harness() {
        cfg.local_scenes=true;cfg.generation_config.prompt_token_counter=[](const std::vector<LlmPromptMessage>& messages,int* count,std::string*) {
            size_t n=0;for(const auto& m:messages)n+=m.content.size();*count=static_cast<int>(n/4+1);return true;};
        cfg.dynamic_scene_config.completion=[](const LlmGenerationConfig&,const std::vector<LlmPromptMessage>&,LlmStreamCallback,void*,LlmGenerationResult*)->bool{
            throw std::runtime_error("Object action composed a room");};
        cfg.interaction_completion=[this](const LlmGenerationConfig& config,const std::vector<LlmPromptMessage>& messages,LlmStreamCallback,void*,LlmGenerationResult* r) {
            ++interpretations;REQUIRE(config.purpose=="player_interpretation_v2");const auto context=json::parse(messages[1].content);
            r->success=true;r->reached_eog=true;
            if(mode=="malformed"){r->response_text="{}";return true;}
            auto p=FixturePlayerAction(context,proposal);r->response_text=FixtureWireAction(p,context).dump();return true;
        };
        cfg.narrative_completion=[this](const LlmGenerationConfig& config,const std::vector<LlmPromptMessage>& messages,LlmStreamCallback,void*,LlmGenerationResult* r) {
            ++narrations;const auto request=json::parse(messages[1].content);
            REQUIRE(config.temperature==ComputeLlmSamplingTemperature(request["temperature_body_c"].get<float>()));
            if(mode=="cancel")cancel=true;
            if(mode=="failure")return false;
            json ids=json::array();std::string prose="La lumière glisse sur les masses silencieuses. Votre souffle accompagne cette pause derrière la visière.";
            for(const auto& item:request.at("objects").at("deposited")) {
                ids.push_back(item["id"]);
                if(mode!="omit")prose+=" Ici repose "+item["name"].get<std::string>()+".";
            }
            if(mode=="reverse")prose="Vous sentez la craie quitter vos mains et se déposer sur le sol ocre.";
            if(mode=="invent")prose="Vous ramassez une clé dorée et la rangez dans votre équipement.";
            if(mode=="observe")prose=observed_prose;
            if(mode=="resurrect")prose="Un petit caillou repose ici, toujours disponible à vos pieds.";
            json response={{"title","Le seuil clair"},{"summary","Les masses claires bordent ce passage ouvert."},{"narration",prose},
                {"thermal",{{"external_temperature_c",464},{"body_temperature_c",request["temperature_body_c"].get<float>()+.1f},{"reason","Une brève exposition."}}},
                {"exits",request["exit_proposal"]["fixed"]}};
            if(json::parse(config.json_schema)["properties"].contains("deposited_ids"))response["deposited_ids"]=ids;
            r->success=true;r->reached_eog=true;r->response_text=response.dump();return true;
        };
    }
    void Implicit(const std::string& quote,const std::string& label) {
        proposal={{"action","take"},{"origin","implicit"},{"object_id",""},{"quote",quote},{"label",label},{"visual_id",""},
            {"verdict","allow"},{"durable",true},{"material_source",false},{"reason","Objet décrit, détaché et accessible."},{"revision",0}};
    }
    HeadlessTurnResult Run(SessionState& s,const char* command,bool publish=true) {
        HeadlessTurnResult t;char error[512]={};
        const bool ok=RunHeadlessTurnFromState(s,command,cfg,&t,error,sizeof(error));
        if(!ok)throw std::runtime_error(std::string("Object turn ")+command+": "+error);
        if(publish)UpdateSessionStateFromTurn(command,t,&s);return t;
    }
};
bool Has(const SessionState& s,const std::string& id) {return std::find(s.hard_state.inventory_items.begin(),s.hard_state.inventory_items.end(),id)!=s.hard_state.inventory_items.end();}
std::string Snapshot(const SessionState& s) {std::string text;REQUIRE(SerializeSessionStateToJsonString(s,&text));return text;}
}

void RunObjectJourneyTests() {
    auto s=Fixture();Harness h;std::string why;
    auto t=h.Run(s,"déposer la craie");
    REQUIRE(!Has(s,"boundary chalk"));REQUIRE(s.objects.deposits.at(a).size()==1);REQUIRE(h.interpretations==1&&h.narrations==1);
    REQUIRE(t.rendered_scene_text.find("mobile_")!=std::string::npos);REQUIRE(t.turn_result.narration.find("Vous déposez ici")!=std::string::npos);
    REQUIRE(std::abs(s.hard_state.body_temperature_c-37.1f)<.001f);
    const auto pos=s.objects.deposits.at(a)[0];const auto original=s.generated_rooms[0].scene_text;
    auto once=Snapshot(s);UpdateSessionStateFromTurn("déposer la craie",t,&s);REQUIRE(Snapshot(s)==once);
    RenderConfig render;render.width=320;render.height=200;render.samples_per_pixel=2;
    const auto artifacts=std::filesystem::temp_directory_path()/"liminal-object-tests";
    std::filesystem::create_directories(artifacts);
    REQUIRE(RenderSceneToImage(t.rendered_scene,render,(artifacts/"chalk.png").string().c_str()));
    h.Run(s,"north");REQUIRE(s.current_place_id==b);
    for(int i=0;i<14;++i)h.Run(s,"look around");
    h.Run(s,"south");REQUIRE(s.current_place_id==a);
    REQUIRE(s.objects.deposits.at(a)[0].x==pos.x&&s.objects.deposits.at(a)[0].z==pos.z);
    h.mode="omit";t=h.Run(s,"regarder tout autour");
    REQUIRE(t.turn_result.narration.find("craie")!=std::string::npos);REQUIRE(!t.used_turn_fallback);
    REQUIRE(t.rendered_scene_text.find("mobile_")!=std::string::npos);
    h.mode="observe";h.observed_prose="La craie de délimitation que vous avez déposée repose au sol. La lumière en souligne la surface claire.";
    t=h.Run(s,"regarder");REQUIRE(!t.used_turn_fallback&&t.turn_result.narration.find("que vous avez déposée")!=std::string::npos);
    h.mode="";t=h.Run(s,"reprendre la craie");REQUIRE(Has(s,"boundary chalk"));REQUIRE(s.objects.deposits.at(a).empty());
    REQUIRE(t.rendered_scene_text==original);REQUIRE(RenderSceneToImage(t.rendered_scene,render,(artifacts/"retrieved.png").string().c_str()));
    const auto calls=h.narrations;t=h.Run(s,"prendre la craie");REQUIRE(h.narrations==calls);REQUIRE(t.turn_result.narration.find("déjà")!=std::string::npos);
    REQUIRE(ValidateObjectState(s,&why));

    s=Fixture();h.Run(s,"poser craie");h.mode="reverse";t=h.Run(s,"prendre craie");
    REQUIRE(Has(s,"boundary chalk")&&t.used_turn_fallback);
    REQUIRE(t.turn_result.narration.find("quitter vos mains")==std::string::npos);h.mode="";

    // First implicit acquisition, transition to a deposit elsewhere, persistent source.
    s=Fixture();h.Implicit("Un petit caillou repose ici.","caillou");t=h.Run(s,"prendre le caillou");
    REQUIRE(s.hard_state.inventory_items.size()==4);const auto id=s.hard_state.inventory_items.back();
    REQUIRE(s.objects.sources.size()==1&&s.objects.sources[0].remaining==0);REQUIRE(s.objects.instances.at(id).origin_place==a);
    REQUIRE(s.objects.instances.at(id).source_quote=="Un petit caillou repose ici.");
    h.Run(s,"north");h.Run(s,"déposer le caillou");REQUIRE(!Has(s,id)&&s.objects.deposits.at(b)[0].object_id==id);
    h.Run(s,"prendre le caillou");REQUIRE(Has(s,id)&&s.objects.deposits.at(b).empty());h.Run(s,"south");
    h.Run(s,"prendre le petit caillou");REQUIRE(s.hard_state.inventory_items.size()==4&&s.objects.instances.size()==4);
    h.mode="resurrect";t=h.Run(s,"look around");REQUIRE(t.turn_result.narration.find("toujours disponible")==std::string::npos);
    REQUIRE(t.turn_result.narration.find("Déjà prélevé")!=std::string::npos);h.mode="";
    SessionState restored;char error[512]={};auto saved=Snapshot(s);
    REQUIRE(ParseSessionStateFromJson(saved.c_str(),&restored,error,sizeof(error)));REQUIRE(SerializeObjectState(restored.objects)==SerializeObjectState(s.objects));
    auto old=json::parse(saved);old.erase("objects");old["hard_state"]["inventory_items"]={"legacy keepsake"};
    REQUIRE(ParseSessionStateFromJson(old.dump().c_str(),&restored,error,sizeof(error)));REQUIRE(restored.objects.instances.size()==1&&!restored.objects.instances.begin()->second.portable);
    REQUIRE(Fixture().hard_state.inventory_items.size()==3);

    // Observations are durable evidence only after accepted publication. They
    // survive history trimming, revisits and diagnostic serialization.
    s=Fixture();s.reference_descriptions[a]="Le sol ocre s'étend entre les masses rocheuses.";
    h.mode="observe";h.observed_prose="Une fine poussière minérale couvre le sol à vos pieds. La lumière en souligne les grains.";
    t=h.Run(s,"regarder par terre",false);REQUIRE(s.observed_descriptions.empty());
    UpdateSessionStateFromTurn("regarder par terre",t,&s);
    REQUIRE(ObjectEvidence(s,a).find("poussière minérale")!=std::string::npos);
    const auto retained=s.observed_descriptions;UpdateSessionStateFromTurn("regarder par terre",t,&s);
    REQUIRE(s.observed_descriptions==retained);
    h.mode="";for(int i=0;i<14;++i)h.Run(s,"look around");
    h.Run(s,"north");h.Run(s,"south");s.history.clear();
    saved=Snapshot(s);REQUIRE(ParseSessionStateFromJson(saved.c_str(),&restored,error,sizeof(error)));
    REQUIRE(restored.observed_descriptions==s.observed_descriptions);s=restored;
    h.Implicit("Une fine poussière minérale couvre le sol à vos pieds.","poussière minérale");
    h.proposal["material_source"]=true;
    t=h.Run(s,"recueillir un peu de cette poudre");
    REQUIRE(t.turn_result.intent=="object_take"&&s.objects.instances.size()==4);
    const auto dust=s.hard_state.inventory_items.back();REQUIRE(s.objects.sources.back().material_source);
    REQUIRE(s.objects.instances.at(dust).label.find("échantillon")!=std::string::npos);
    h.Run(s,"north");const int sample_interpretations=h.interpretations;
    h.Run(s,"déposer cet échantillon");REQUIRE(h.interpretations==sample_interpretations+1);h.Run(s,"regarder");
    REQUIRE(ObjectEvidence(s,b).find("poussière")==std::string::npos);
    h.Run(s,"prendre poussière minérale");REQUIRE(Has(s,dust));h.Run(s,"south");
    h.mode="observe";h.observed_prose="La poussière minérale couvre encore le sol dans la lumière ocre.";
    t=h.Run(s,"regarder par terre");REQUIRE(!t.used_turn_fallback);
    REQUIRE(s.observed_descriptions.at(a)==retained.at(a)||
        ObjectEvidence(s,a).find("couvre encore")==std::string::npos);
    // Rephrasing a sampled source never creates another instance.
    h.mode="";h.Run(s,"ramasser de la poussière");REQUIRE(s.objects.instances.size()==4);
    saved=Snapshot(s);REQUIRE(ParseSessionStateFromJson(saved.c_str(),&restored,error,sizeof(error)));
    REQUIRE(restored.objects.sources.back().material_source);
    auto legacy_source=json::parse(saved);legacy_source["objects"]["sources"][0].erase("material_source");
    REQUIRE(ParseSessionStateFromJson(legacy_source.dump().c_str(),&restored,error,sizeof(error)));
    REQUIRE(!restored.objects.sources[0].material_source&&Has(restored,dust));
    legacy_source["objects"]["sources"][0].erase("allowed");
    REQUIRE(!ParseSessionStateFromJson(legacy_source.dump().c_str(),&restored,error,sizeof(error)));
    // Failed/cancelled prose and notebook claims are not source evidence.
    s=Fixture();h.mode="invent";h.Run(s,"regarder");REQUIRE(ObjectEvidence(s,a).find("clé dorée")==std::string::npos);
    h.mode="";h.Run(s,"noter un diamant repose au sol");REQUIRE(ObjectEvidence(s,a).find("diamant")==std::string::npos);
    h.mode="observe";h.observed_prose="Un diamant pourrait reposer ici. Vous ne voyez aucun cristal au sol.";
    h.Run(s,"regarder");REQUIRE(ObjectEvidence(s,a).find("diamant")==std::string::npos);
    h.mode="cancel";h.cancel=false;
    h.cfg.stream_callback=[](HeadlessTurnStreamPhase,const char*,const char*,void* opaque){return !static_cast<Harness*>(opaque)->cancel;};
    h.cfg.stream_user_data=&h;const auto observation_before=Snapshot(s);
    REQUIRE(!RunHeadlessTurnFromState(s,"regarder",h.cfg,&t,error,sizeof(error)));
    UpdateSessionStateFromTurn("regarder",t,&s);REQUIRE(Snapshot(s)==observation_before&&t.accepted_observation.empty());
    h.cfg.stream_callback=nullptr;h.mode="";h.cancel=false;

    // Malformed, nonexistent, fixed, hypothetical and finite matter targets.
    s=Fixture();h.mode="malformed";t=h.Run(s,"prendre le cristal");REQUIRE(s.hard_state.inventory_items.size()==3);
    h.mode="";h.Implicit("Un cristal repose ici.","cristal");h.Run(s,"prendre le cristal");REQUIRE(s.objects.sources.empty());
    h.Implicit("Un petit caillou repose ici.","caillou");h.proposal["action"]="none";h.proposal["origin"]="none";h.proposal["verdict"]="forbid";h.Run(s,"prendre le cristal");REQUIRE(s.objects.sources.empty());
    s.reference_descriptions[a]="Un mur ferme la pièce.";h.Implicit(s.reference_descriptions[a],"mur");h.Run(s,"prendre le mur");REQUIRE(s.objects.sources.empty());
    s.reference_descriptions[a]="Un caillou pourrait reposer ici.";h.Implicit(s.reference_descriptions[a],"caillou");h.Run(s,"prendre le caillou");REQUIRE(s.objects.sources.empty());
    s.reference_descriptions[a]="Un tas de gravats repose ici.";h.Implicit(s.reference_descriptions[a],"gravats");h.proposal["material_source"]=true;
    h.Run(s,"prendre gravats");REQUIRE(s.objects.sources.size()==1);const auto fragment=s.hard_state.inventory_items.back();
    h.Run(s,"déposer le fragment de gravats");h.Run(s,"north");h.Run(s,"south");
    h.Run(s,"prendre gravats");REQUIRE(Has(s,fragment)&&s.objects.instances.size()==4);

    // Stable refusal, including a later favorable model answer for the same source.
    s=Fixture();h.Implicit("Un petit caillou repose ici.","caillou");h.proposal["verdict"]="forbid";
    h.Run(s,"prendre le caillou");REQUIRE(s.objects.sources.size()==1&&!s.objects.sources[0].allowed&&s.objects.instances.size()==3);
    h.proposal["verdict"]="allow";h.Run(s,"emporter le petit caillou");REQUIRE(s.objects.instances.size()==3);

    // Empty inventory, missing tools, ownership in candidate narration, failed prose.
    s=Fixture();h.mode="failure";
    for(const auto* command:{"déposer la craie","déposer la tablette","déposer les pinces"}) {
        t=h.Run(s,command);REQUIRE(t.turn_result.narration.find("Vous déposez ici")!=std::string::npos);
    }
    REQUIRE(s.hard_state.inventory_items.empty());t=h.Run(s,"inventaire");REQUIRE(t.turn_result.narration.find("rien")!=std::string::npos);
    t=h.Run(s,"examiner la craie");REQUIRE(t.turn_result.intent=="inventory_item_absent");
    for(const auto* command:{"take chalk","take slate","take tongs"})h.Run(s,command);
    REQUIRE(s.hard_state.inventory_items.size()==3);
    h.mode="";const auto before=s.hard_state.inventory_items;
    for(const auto* command:{"donner la craie","jeter les pinces","give the chalk","throw away the chalk","prendre la craie et la tablette"}) {
        h.proposal={{"action","none"},{"origin","none"},{"object_id",""},{"quote",""},{"label",""},{"visual_id",""},
            {"verdict","forbid"},{"durable",false},{"material_source",false},{"reason","Unavailable"},{"revision",0}};
        h.Run(s,command);REQUIRE(s.hard_state.inventory_items==before);
    }

    // Cancellation after a first implicit candidate and after the narrative callback.
    s=Fixture();h.Implicit("Un petit caillou repose ici.","caillou");h.mode="cancel";
    h.cfg.stream_callback=[](HeadlessTurnStreamPhase,const char* text,const char*,void* opaque){
        REQUIRE(std::string(text).empty());return !static_cast<Harness*>(opaque)->cancel;};h.cfg.stream_user_data=&h;
    const auto before_cancel=Snapshot(s);
    REQUIRE(!RunHeadlessTurnFromState(s,"prendre le caillou",h.cfg,&t,error,sizeof(error)));
    REQUIRE(!t.accepted&&!t.objects_prepared&&t.rendered_scene_text.empty());UpdateSessionStateFromTurn("prendre le caillou",t,&s);
    REQUIRE(Snapshot(s)==before_cancel&&s.objects.sources.empty());h.cfg.stream_callback=nullptr;h.cancel=false;h.mode="";

    // A bound small depicted object disappears without changing architectural boxes.
    s=Fixture();s.generated_rooms[0].scene_text+=std::string("box \"pebble\" pos(0,0.1,-3) size(0.2,0.2,0.2) gray(0.9)\n");
    h.Implicit("Un petit caillou repose ici.","caillou");h.proposal["visual_id"]="pebble";
    t=h.Run(s,"prendre le caillou");REQUIRE(s.objects.instances.size()==4);REQUIRE(t.rendered_scene_text.find("box \"pebble\"")==std::string::npos);
    REQUIRE(t.rendered_scene_text.find("box \"wall\"")!=std::string::npos);REQUIRE(s.generated_rooms[0].scene_text.find("box \"pebble\"")!=std::string::npos);
    s=Fixture();s.generated_rooms[0].scene_text+="box \"pebble\" pos(0,0.1,-3) size(0.2,0.2,0.2) gray(0.9)\n";
    h.proposal["visual_id"]="";h.proposal["material_source"]=true;
    // A model-declared material sample leaves its parent; the engine no longer
    // infers a different semantic kind or visual ID from a word in the label.
    t=h.Run(s,"prendre le caillou");REQUIRE(t.rendered_scene_text.find("box \"pebble\"")!=std::string::npos);
    // A structural binding is never removed, even with a favorable semantic verdict.
    s=Fixture();h.proposal["visual_id"]="wall";h.Run(s,"prendre le caillou");REQUIRE(s.objects.instances.size()==3);

    // Canonical threshold also supports a visible drop; no generated room is needed.
    REQUIRE(InitializeSessionState(kLocationQuarryThreshold,&s,error,sizeof(error)));s.language=kGameLanguageFrench;
    t=h.Run(s,"déposer la craie");REQUIRE(!Has(s,"boundary chalk"));REQUIRE(t.generated_rooms_to_add.empty());
    REQUIRE(RenderSceneToImage(t.rendered_scene,render,(artifacts/"canonical-chalk.png").string().c_str()));
    ResearchArchive archive;const auto archive_root=(artifacts/"archive");
    REQUIRE(archive.Start(archive_root.string(),s,h.cfg.generation_config,render));REQUIRE(archive.Command("look around"));
    std::vector<unsigned char> pixels;REQUIRE(RenderSceneToPixels(t.rendered_scene,render,&pixels));
    REQUIRE(archive.Turn(s,t,pixels));REQUIRE(std::filesystem::exists(std::filesystem::path(archive.Path())/"turns/turn-00001.png"));
    REQUIRE(archive.Close(s));

    // No valid support means no drop, even when the object is owned.
    s=Fixture();const std::string floor="box \"floor\" pos(0,-0.1,0) size(20,0.2,20) gray(0.6)\n";
    auto& no_floor=s.generated_rooms[0].scene_text;no_floor.erase(no_floor.find(floor),floor.size());
    t=h.Run(s,"déposer la craie");REQUIRE(Has(s,"boundary chalk"));REQUIRE(t.turn_result.narration.find("Aucun emplacement")!=std::string::npos);

    // Homonyms are presented with distinct selectors and clarify without heat.
    s=Fixture();
    for(const auto* pebble_id:{"pebble-a","pebble-b"}) {
        ObjectInstance item;item.id=pebble_id;item.label="caillou";item.portable=true;
        s.objects.instances[pebble_id]=item;ObjectPlacement p;p.object_id=pebble_id;p.x=std::string(pebble_id)=="pebble-a"?-1.f:1.f;p.z=-3;
        s.objects.deposits[a].push_back(p);
    }
    const auto heat=s.hard_state.body_temperature_c;const auto narrative_count=h.narrations;
    t=h.Run(s,"prendre le caillou");REQUIRE(t.turn_result.intent=="clarify_object"&&h.narrations==narrative_count);
    REQUIRE(s.hard_state.body_temperature_c==heat&&s.objects.deposits[a].size()==2);
    h.Run(s,"prendre caillou 2");REQUIRE(Has(s,"pebble-b")&&s.objects.deposits[a].size()==1);

    // All selected languages retain local fallbacks with exact localized tool names.
    for(int language=0;language<6;++language) {
        s=Fixture();s.language=static_cast<GameLanguage>(language);h.mode="failure";
        t=h.Run(s,"drop chalk");REQUIRE(!Has(s,"boundary chalk"));
        REQUIRE(t.turn_result.narration.find(InventoryItemName("boundary chalk",s.language))!=std::string::npos);
        t=h.Run(s,"look around");REQUIRE(t.turn_result.narration.find(InventoryItemName("boundary chalk",s.language))!=std::string::npos);
        h.Run(s,"take chalk");REQUIRE(Has(s,"boundary chalk"));
    }
}
