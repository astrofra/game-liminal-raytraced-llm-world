#include "investigation.h"
#include "object_interactions.h"
#include "turn_runner.h"
#include "scene_design.h"
#include "exploration_policy.h"
#include "narrative_language.h"
#include "nlohmann/json.hpp"
#include <algorithm>
#include <set>
#include <stdexcept>
using namespace liminal;
using nlohmann::json;
#define INV(c) do {if(!(c))throw std::runtime_error(std::string("Investigation journey ")+std::to_string(__LINE__)+": "+#c);}while(false)
namespace {
std::string SceneProgram() {
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
std::string Snapshot(const SessionState& s) {std::string text;INV(SerializeSessionStateToJsonString(s,&text));return text;}
std::string NextStep(const SessionState& s,const std::string& destination={}) {
    std::vector<std::pair<std::string,std::string>> queue{{s.current_place_id,""}};std::set<std::string> seen{s.current_place_id};
    for(size_t i=0;i<queue.size();++i) {
        const auto node=queue[i];SpatialState spatial=s.spatial_state;
        if(node.first==s.origin_place_id)spatial.blocked_exits={"west"};
        else for(const auto& room:s.generated_rooms)if(room.room_id==node.first)spatial=room.spatial_state;
        for(const auto d:{kDirectionNorth,kDirectionEast,kDirectionSouth,kDirectionWest}) {
            const std::string name=CardinalDirectionToString(d);
            if(std::find(spatial.blocked_exits.begin(),spatial.blocked_exits.end(),name)!=spatial.blocked_exits.end())continue;
            bool barrier=false;for(const auto& b:s.invisible_barriers)if(b.place_id==node.first&&b.direction==d)barrier=true;
            if(barrier)continue;
            std::string target;for(const auto& link:s.room_links)if(link.from_place_id==node.first&&link.direction==d)target=link.to_place_id;
            const auto first=node.second.empty()?name:node.second;
            if(destination.empty()&&target.empty())return first;
            if(!destination.empty()&&target==destination)return first;
            if(!target.empty()&&seen.insert(target).second)queue.push_back({target,first});
        }
    }
    return {};
}
struct Harness {
    HeadlessTurnConfig cfg;
    std::string action="review_investigation",clue,object;
    bool delivered=false,broken=false,stale=false;
    int geometry_calls=0,interpretation_calls=0,narration_calls=0,creation_calls=0;
    std::vector<std::vector<std::string>> schedule;
    std::string proposal_fault;
    bool bad_prose=false,bad_thermal=false,unexpected=false;
    Harness(unsigned int seed) {
        schedule=seed==0?std::vector<std::vector<std::string>>{{"crystal_sample"},{"invisible_obstruction"},{"vey_equipment"},{"vey_survey"}}:
            std::vector<std::vector<std::string>>{{"vey_survey","vey_equipment"},{},{"crystal_sample","invisible_obstruction"}};
        cfg.local_scenes=true;cfg.generation_config.seed=seed;cfg.generation_config.n_ctx=16384;
        cfg.generation_config.prompt_token_counter=[](const std::vector<LlmPromptMessage>& messages,int* count,std::string*) {
            size_t n=0;for(const auto& m:messages)n+=m.content.size();*count=static_cast<int>(n/4);return true;};
        cfg.dynamic_scene_config.completion=[this](const LlmGenerationConfig&,const std::vector<LlmPromptMessage>& messages,LlmStreamCallback,void*,LlmGenerationResult* r) {
            ++geometry_calls;INV(messages[1].content.find("CONTINUITY_V1")!=std::string::npos);
            r->success=r->reached_eog=true;r->response_text=broken?"broken":SceneProgram();return true;};
        cfg.interaction_completion=[this](const LlmGenerationConfig&,const std::vector<LlmPromptMessage>& messages,LlmStreamCallback,void*,LlmGenerationResult* r) {
            ++interpretation_calls;delivered=true;const auto p=json::parse(messages[1].content);
            r->success=r->reached_eog=true;
            r->response_text=json({{"action",action},{"object_id",object},{"source",0},{"label",""},{"visual_id",""},
                {"material_source",false},{"durable",false},{"reason","Requested action"},{"revision",p["revision"]},
                {"investigation_id",clue},{"investigation_revision",p["investigation"]["revision"].get<int>()+(stale?1:0)}}).dump();return true;};
        cfg.narrative_completion=[this](const LlmGenerationConfig& c,const std::vector<LlmPromptMessage>& messages,LlmStreamCallback,void*,LlmGenerationResult* r) {
            ++narration_calls;delivered=true;const auto p=json::parse(messages[1].content);
            const auto schema=json::parse(c.json_schema)["properties"]["exits"];
            auto exits=p["exit_proposal"]["fixed"];
            if(schema.contains("anyOf"))for(auto it=schema["anyOf"][0]["properties"].begin();it!=schema["anyOf"][0]["properties"].end();++it)exits[it.key()]=it.value()["enum"][0];
            for(const auto* d:{"north","east","south","west"})if(!exits.contains(d))exits[d]=false;
            json deposits=json::array();for(const auto& item:p["objects"]["deposited"])deposits.push_back(item["id"]);
            r->success=r->reached_eog=true;
            auto response=json({{"title","Quarry study "+std::to_string(narration_calls)},
                {"summary","Pale light falls across the quarry floor and the distant stone masses."},
                {"narration","You study the quiet space through the visor. Pale light settles on the stone edges beyond the open ground. Your breathing marks a pause before the distant masses, whose surfaces remain still beneath the broad Venusian sky."},
                {"thermal",{{"external_temperature_c",p["temperature_external_c"]},{"body_temperature_c",p["temperature_body_c"]},{"reason","Steady exposure"}}},
                {"exits",exits}});
            if(p.contains("investigation_authoring")) {
                INV(json::parse(c.json_schema)["properties"].contains("investigation_placement"));
                const auto& author=p["investigation_authoring"];
                const auto selected=creation_calls<static_cast<int>(schedule.size())?schedule[creation_calls]:std::vector<std::string>();
                ++creation_calls;json placements=json::array();std::set<std::string> used;
                std::map<std::string,bool> fixed=p["exit_proposal"]["fixed"].get<std::map<std::string,bool>>();
                auto balance=p["exit_proposal"]["balance"];
                for(const auto& id:selected) {
                    for(const auto& option:author["available"])if(option["id"]==id) {
                        std::string site;for(const auto& offered:option["sites"])if(!used.count(offered.get<std::string>())){site=offered;break;}
                        INV(!site.empty());used.insert(site);
                        placements.push_back({{"opportunity_id",id},{"site_id",site}});
                        if(id=="invisible_obstruction") {
                            for(const auto& offered:author["sites"])if(offered["id"]==site)fixed[offered["direction"].get<std::string>()]=true;
                            if(!balance["allow_dead_end"].get<bool>())balance["target_open_exits"]=3;
                        } else deposits.push_back(option["object_id"]);
                    }
                }
                ExitBalancePolicy policy;policy.target_open_exits=balance["target_open_exits"];
                policy.allow_dead_end=balance["allow_dead_end"];policy.selection_seed=balance["selection_seed"];
                response["exits"]=BalanceExplorationExits(fixed,fixed,p["exit_proposal"]["geometrically_possible"].get<std::vector<std::string>>(),policy);
                response["investigation_placement"]={{"revision",author["revision"]},{"placements",placements}};
                if(proposal_fault=="missing")response.erase("investigation_placement");
                else if(proposal_fault=="stale")response["investigation_placement"]["revision"]=author["revision"].get<int>()+1;
                else if(proposal_fault=="invalid")response["investigation_placement"]["placements"].push_back({{"opportunity_id","fabricated"},{"site_id","ground_01"}});
                else if(proposal_fault=="closed_barrier")for(const auto& entry:placements)if(entry["opportunity_id"]=="invisible_obstruction")
                    for(const auto& site:author["sites"])if(site["id"]==entry["site_id"])response["exits"][site["direction"].get<std::string>()]=false;
            } else {
                INV(!json::parse(c.json_schema)["properties"].contains("investigation_placement"));
                if(unexpected)response["investigation_placement"]={{"revision",0},{"placements",json::array()}};
            }
            if(bad_prose)response["narration"]="bad";
            if(bad_thermal)response["thermal"]["body_temperature_c"]=90;
            if(json::parse(c.json_schema)["properties"].contains("deposited_ids"))response["deposited_ids"]=deposits;
            if(proposal_fault=="invented_deposit")response["deposited_ids"]=json::array({"clue:crystal_sample"});
            if(proposal_fault=="incomplete")r->reached_eog=false;
            r->response_text=response.dump();
            if(proposal_fault=="duplicate")r->response_text.insert(1,"\"investigation_placement\":{\"revision\":0,\"placements\":[]},");
            return true;};
    }
    HeadlessTurnResult Turn(SessionState& s,const std::string& command) {
        const auto before=Snapshot(s);HeadlessTurnResult r;char error[1024]={};
        if(!RunHeadlessTurnFromState(s,command.c_str(),cfg,&r,error,sizeof(error)))throw std::runtime_error(error);
        INV(Snapshot(s)==before);INV(r.accepted);
        UpdateSessionStateFromTurn(command.c_str(),r,&s);
        std::string why;INV(ValidateInvestigation(s,&why));INV(ValidateObjectState(s,&why));return r;
    }
    void Examine(SessionState& s,const std::string& id) {
        action="examine_clue";clue=id;object.clear();
        if(id=="vey_equipment")object="clue:"+id; // Consistent redundant reference is harmless.
        if(id=="vey_survey") {action="inspect";object="clue:"+id;clue.clear();} // Structured object inspection.
        if(id=="crystal_sample") {action="perceive";object="clue:"+id;clue.clear();} // Targeted observation.
        Turn(s,"examine the indicated clue");clue.clear();object.clear();
        bool observed=false;
        const auto context=json::parse(InvestigationContext(s));
        for(const auto& accessible:context["accessible_clues"])
            if(accessible["id"]==id)observed=accessible["already_observed"].get<bool>();
        INV(observed);
    }
};
void Journey(unsigned int seed) {
    SessionState s;char error[1024]={};INV(InitializeSessionState(kLocationQuarryThreshold,&s,error,sizeof(error)));Harness h(seed);
    const int score=s.hard_state.score;
    // Scene rejection and late cancellation must leave the opportunity available.
    const auto baseline=Snapshot(s);HeadlessTurnResult refused;h.broken=true;h.cfg.dynamic_scene_config.max_repairs=0;
    INV(!RunHeadlessTurnFromState(s,"north",h.cfg,&refused,error,sizeof(error)));UpdateSessionStateFromTurn("north",refused,&s);INV(Snapshot(s)==baseline);
    h.broken=false;h.delivered=false;h.cfg.stream_user_data=&h.delivered;
    h.cfg.stream_callback=[](HeadlessTurnStreamPhase,const char*,const char*,void* p){return !*static_cast<bool*>(p);};
    INV(!RunHeadlessTurnFromState(s,"north",h.cfg,&refused,error,sizeof(error)));UpdateSessionStateFromTurn("north",refused,&s);
    INV(!refused.accepted&&Snapshot(s)==baseline&&refused.updated_investigation.evidence.empty());h.cfg.stream_callback=nullptr;
    h.creation_calls=0; // The discarded fixture response is not the next published scene.
    std::set<std::string> processed;std::string deposit_place;bool dropped=false;
    for(int step=0;step<30&&processed.size()<4;++step) {
        const auto command=NextStep(s);INV(!command.empty());const auto r=h.Turn(s,command);
        if(!r.generated_room_created)continue;
        INV(s.generated_rooms.size()<=6);
        // Deposit the acquired sample in a different room, then retrieve it after the exploration.
        if(!dropped&&std::find(s.hard_state.inventory_items.begin(),s.hard_state.inventory_items.end(),"clue:crystal_sample")!=s.hard_state.inventory_items.end()) {
            h.action="drop";h.object="clue:crystal_sample";h.clue.clear();h.Turn(s,"leave the sample here");
            deposit_place=s.current_place_id;dropped=true;h.object.clear();
        }
        auto opportunities=s.investigation.opportunities;
        for(const auto& o:opportunities)if(o.place_id==s.current_place_id&&processed.insert(o.id).second) {
            if(o.id=="invisible_obstruction") {
                CardinalDirection direction;INV(ParseCardinalDirection(o.direction.c_str(),&direction));
                for(const auto& link:s.room_links)INV(link.from_place_id!=o.place_id||link.direction!=direction);
                const auto current=s.current_place_id;const auto moves=s.hard_state.move_count;
                h.Turn(s,o.direction);INV(s.current_place_id==current&&s.hard_state.move_count==moves);
                const auto evidence=SerializeInvestigation(s.investigation);h.Turn(s,o.direction);INV(SerializeInvestigation(s.investigation)==evidence);
                INV(!NextStep(s).empty());h.Examine(s,o.id);
            } else {
                INV(r.rendered_scene_text.find(ObjectVisualName(o.object_id))!=std::string::npos);
                h.action="examine_clue";h.clue=o.id;h.stale=true;const auto before=SerializeInvestigation(s.investigation);
                h.Turn(s,"examine stale reference");INV(SerializeInvestigation(s.investigation)==before);h.stale=false;
                h.delivered=false;h.cfg.stream_user_data=&h.delivered;
                h.cfg.stream_callback=[](HeadlessTurnStreamPhase,const char*,const char*,void* p){return !*static_cast<bool*>(p);};
                const auto unmodified=Snapshot(s);HeadlessTurnResult cancelled;
                INV(!RunHeadlessTurnFromState(s,"examine, then cancel",h.cfg,&cancelled,error,sizeof(error)));
                UpdateSessionStateFromTurn("examine, then cancel",cancelled,&s);INV(Snapshot(s)==unmodified);
                h.cfg.stream_callback=nullptr;
                h.Examine(s,o.id);
                if(o.id=="crystal_sample") {h.action="take";h.object=o.object_id;h.Turn(s,"collect this specimen");h.object.clear();}
            }
        }
    }
    INV(processed.size()==4&&s.investigation.evidence.size()==6&&s.hard_state.score==score);
    if(!dropped) {h.action="drop";h.object="clue:crystal_sample";h.Turn(s,"leave it here");deposit_place=s.current_place_id;dropped=true;h.object.clear();}
    for(int i=0;i<40;++i) {SessionTurnRecord old;old.engine_outcome="No effect";s.history.push_back(old);}
    h.action="review_investigation";auto review=h.Turn(s,"what do we know about Vey?");
    INV(review.turn_result.intent=="review_investigation");INV(review.turn_result.narration.find("carried")==std::string::npos);
    for(int i=0;s.current_place_id!=deposit_place&&i<30;++i) {const auto command=NextStep(s,deposit_place);INV(!command.empty());h.Turn(s,command);}
    INV(s.current_place_id==deposit_place);const auto evidence=SerializeInvestigation(s.investigation);
    h.action="take";h.object="clue:crystal_sample";h.Turn(s,"put it back with my belongings");
    INV(SerializeInvestigation(s.investigation)==evidence);INV(s.hard_state.score==score);
    h.action="review_investigation";h.object.clear();h.clue="crystal_sample";
    const auto recap=h.Turn(s,"compare this specimen with the earlier evidence");
    INV(recap.turn_result.intent=="review_investigation"&&SerializeInvestigation(s.investigation)==evidence);
    auto serialized=Snapshot(s);SessionState restored;INV(ParseSessionStateFromJson(serialized.c_str(),&restored,error,sizeof(error)));
    INV(SerializeInvestigation(restored.investigation)==SerializeInvestigation(s.investigation));
    auto stale_projection=s;stale_projection.soft_state.active_hypotheses={"unsupported legacy conclusion"};
    INV(json::parse(Snapshot(stale_projection))["soft_state"]["active_hypotheses"]==json::parse(serialized)["soft_state"]["active_hypotheses"]);
    INV(stale_projection.soft_state.active_hypotheses==std::vector<std::string>{"unsupported legacy conclusion"});
    for(const char* field:{"id","source_event","object_id"}) {
        auto tampered=json::parse(serialized);tampered["investigation"]["evidence"][0][field]="fabricated";
        INV(!ParseSessionStateFromJson(tampered.dump().c_str(),&restored,error,sizeof(error)));
    }
    for(int language=0;language<6;++language) {
        s.language=static_cast<GameLanguage>(language);
        INV(!IsClearlyWrongNarrativeLanguage(InvestigationJournal(s),s.language));
        INV(!IsClearlyWrongNarrativeLanguage(InvestigationMandate(s.language),s.language));
    }
}
void LastFrontier() {
    SessionState s;char error[1024]={};INV(InitializeSessionState(kLocationQuarryThreshold,&s,error,sizeof(error)));
    // Imported legacy barriers close the other starting frontiers. New barrier
    // distribution must protect a continuation beyond the north discovery.
    for(const auto direction:{kDirectionEast,kDirectionSouth}) {
        InvisibleBarrier b;b.place_id=s.current_place_id;b.direction=direction;s.invisible_barriers.push_back(b);
    }
    Harness h(1);h.schedule={{"invisible_obstruction","crystal_sample","vey_survey"}};const auto result=h.Turn(s,"north");
    INV(s.investigation.opportunities.size()==3);
    const auto& opportunity=s.investigation.opportunities.front();INV(opportunity.id=="invisible_obstruction");
    INV(s.spatial_state.blocked_exits.size()<=1);INV(!NextStep(s).empty());
    const auto provenance=json::parse(s.generated_rooms.back().scene_design_json);
    INV(provenance["other_frontier_exits"]==0);
    const auto constraints=json::parse(result.narrative_metrics_json)["constraints"]["possible"];
    INV(std::find(constraints.begin(),constraints.end(),json(opportunity.direction))!=constraints.end());
    auto linked=s;RoomLink link;link.from_place_id=s.current_place_id;link.direction=kDirectionSouth;link.to_place_id=s.origin_place_id;
    linked.investigation.opportunities.clear();linked.invisible_barriers.clear();linked.room_links.push_back(link);
    SessionState untouched=linked;const auto before=Snapshot(untouched);std::string why;
    INV(!PrepareInvestigationOpportunity(linked,"invisible_obstruction",linked.current_place_id,kDirectionSouth,nullptr,&untouched,&why));
    INV(Snapshot(untouched)==before);
}
void DistributionPublication() {
    SessionState initial;char error[1024]={};INV(InitializeSessionState(kLocationQuarryThreshold,&initial,error,sizeof(error)));
    // Same geometry and seed; only the model's explicit selection changes.
    for(const auto& selected:std::vector<std::vector<std::string>>{{},{"vey_survey"},{"crystal_sample","vey_equipment","vey_survey","invisible_obstruction"}}) {
        auto state=initial;Harness h(0);h.schedule={selected};
        const auto turn=h.Turn(state,"north");
        INV(h.geometry_calls==1&&h.narration_calls==1&&h.interpretation_calls==0);
        INV(state.investigation.decisions.back().opportunity_ids==selected);
        INV(state.investigation.evidence.empty());
        INV(state.objects.instances.size()==3+selected.size()-(selected.size()==4?1:0));
        auto journal=state.investigation;h.Turn(state,"south");h.Turn(state,"north");
        INV(h.creation_calls==1&&SerializeInvestigation(state.investigation)==SerializeInvestigation(journal));
    }
    for(const auto* fault:{"missing","stale","invalid","incomplete","closed_barrier","duplicate"}) {
        auto state=initial;const auto before=Snapshot(state);Harness h(0);h.schedule={{"crystal_sample","invisible_obstruction"}};h.proposal_fault=fault;
        HeadlessTurnResult turn;
        INV(!RunHeadlessTurnFromState(state,"north",h.cfg,&turn,error,sizeof(error)));
        UpdateSessionStateFromTurn("north",turn,&state);INV(Snapshot(state)==before);
        INV(turn.investigation_placement_rejected&&!turn.accepted&&!turn.generated_room_created);
    }
    {
        auto state=initial;Harness h(0);h.schedule={{}};h.proposal_fault="invented_deposit";
        const auto turn=h.Turn(state,"north");
        INV(turn.used_turn_fallback&&state.investigation.decisions.back().opportunity_ids.empty());
        INV(state.objects.instances.size()==3);
        INV(json::parse(turn.narrative_metrics_json)["fallback_reason"]=="dynamic_object_coverage");
    }
    for(const auto language:{kGameLanguageEnglish,kGameLanguageFrench,kGameLanguageNorwegian,
                            kGameLanguageDanish,kGameLanguageGerman,kGameLanguageItalian}) {
        auto state=initial;state.language=language;Harness h(0);h.schedule={{"crystal_sample","vey_survey"}};h.bad_prose=true;
        const auto turn=h.Turn(state,"north");
        INV(turn.used_turn_fallback&&!IsClearlyWrongNarrativeLanguage(turn.turn_result.narration,language));
        INV(turn.turn_result.narration.find(InvestigationHint("crystal_sample",language))!=std::string::npos);
        INV(turn.turn_result.narration.find(InvestigationHint("vey_survey",language))!=std::string::npos);
        INV(state.investigation.evidence.empty());
    }
    for(bool thermal:{false,true}) {
        auto state=initial;Harness h(0);h.schedule=thermal?std::vector<std::vector<std::string>>{{}}:
            std::vector<std::vector<std::string>>{{"crystal_sample","vey_equipment"}};
        h.bad_thermal=thermal;h.bad_prose=!thermal;
        const auto turn=h.Turn(state,"north");INV(turn.used_turn_fallback);
        const auto decision=SerializeInvestigation(state.investigation);
        if(thermal)INV(state.generated_rooms.back().metadata_fallback_used);
        else INV(turn.turn_result.narration.find("crystalline specimen")!=std::string::npos);
        h.bad_thermal=false;h.bad_prose=false;h.action="perceive";
        h.Turn(state,"describe the place again");
        INV(!state.generated_rooms.back().metadata_fallback_used);
        INV(h.creation_calls==1&&SerializeInvestigation(state.investigation)==decision);
        h.unexpected=true;h.Turn(state,"look again");
        INV(SerializeInvestigation(state.investigation)==decision);
    }
    {
        auto state=initial;Harness h(0);h.schedule={{"crystal_sample","vey_equipment","invisible_obstruction"}};
        h.delivered=false;h.cfg.stream_user_data=&h.delivered;
        h.cfg.stream_callback=[](HeadlessTurnStreamPhase,const char*,const char*,void* p){return !*static_cast<bool*>(p);};
        const auto before=Snapshot(state);HeadlessTurnResult turn;
        INV(!RunHeadlessTurnFromState(state,"north",h.cfg,&turn,error,sizeof(error)));
        UpdateSessionStateFromTurn("north",turn,&state);INV(Snapshot(state)==before);
        INV(!turn.investigation_prepared&&!turn.objects_prepared&&h.creation_calls==1);
        INV(json::parse(turn.investigation_metrics_json)["prepared"]==true); // Prepared then cancelled, not a missing selection.
    }
}
}
void RunInvestigationJourneyTests() {DistributionPublication();Journey(0);Journey(1);LastFrontier();}
