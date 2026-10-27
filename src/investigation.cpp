#include "investigation.h"
#include "object_interactions.h"
#include "nlohmann/json.hpp"
#include <algorithm>
#include <set>
#include <limits>
#include <stdexcept>

namespace liminal {
using json = nlohmann::json;
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(InvestigationThread, id, status, evidence_ids, hypothesis_ids)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(InvestigationEvidence, id, kind, place_id, object_id, direction, source_event, fact, thread_ids, turn)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(InvestigationHypothesis, id, proposition, status, supporting_evidence, contradicting_evidence)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(InvestigationOpportunity, id, kind, place_id, object_id, direction, status, turn)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(InvestigationDecision, place_id, origin, opportunity_ids, committed_turn)
void to_json(json& j,const InvestigationState& s) {
    j={{"schema_version",s.schema_version},{"revision",s.revision},{"policy",s.policy},{"threads",s.threads},
       {"evidence",s.evidence},{"hypotheses",s.hypotheses},{"opportunities",s.opportunities},{"decisions",s.decisions}};
}
void from_json(const json& j,InvestigationState& s) {
    j.at("schema_version").get_to(s.schema_version);j.at("revision").get_to(s.revision);j.at("policy").get_to(s.policy);
    j.at("threads").get_to(s.threads);j.at("evidence").get_to(s.evidence);j.at("hypotheses").get_to(s.hypotheses);
    j.at("opportunities").get_to(s.opportunities);
    if(s.schema_version==2)j.at("decisions").get_to(s.decisions);
    else s.decisions.clear();
}
namespace {
template<class T> const T* Find(const std::vector<T>& entries, const std::string& id) {
    for(const auto& entry:entries)if(entry.id==id)return &entry;
    return nullptr;
}
bool Has(const std::vector<std::string>& values,const std::string& id) {
    return std::find(values.begin(),values.end(),id)!=values.end();
}
bool KnownOpportunity(const std::string& id) {
    return id=="crystal_sample"||id=="vey_equipment"||id=="vey_survey"||id=="invisible_obstruction";
}
bool ClueIdentity(const std::string& object,const std::string& opportunity) {
    const auto base="clue:"+opportunity;
    if(object==base)return true;
    if(object.compare(0,base.size()+1,base+":")!=0)return false;
    const auto suffix=object.substr(base.size()+1);
    return !suffix.empty()&&suffix.front()!='0'&&std::all_of(suffix.begin(),suffix.end(),[](char c){return c>='0'&&c<='9';});
}
std::string OpportunityForEvidence(const std::string& id) {
    if(id=="crystal_source"||id=="crystal_acquired")return "crystal_sample";
    if(id=="obstruction_contact"||id=="obstruction_examined")return "invisible_obstruction";
    if(id=="vey_equipment"||id=="vey_survey")return id;
    return {};
}
std::vector<std::string> ThreadsFor(const std::string& id) {
    if(id=="vey_equipment"||id=="vey_survey")return {"vey","crystal"};
    if(id=="crystal_source"||id=="crystal_acquired")return {"crystal"};
    return {"obstruction"};
}
std::string KindFor(const std::string& id) {
    return id=="crystal_acquired"?"acquisition":id=="obstruction_contact"?"contact":"observation";
}
void Derive(InvestigationState* s,bool legacy=false) {
    s->threads.clear();
    for(const auto* id:{"vey","obstruction","crystal"}) {
        InvestigationThread t;t.id=id;
        for(const auto& e:s->evidence)if(Has(e.thread_ids,id))t.evidence_ids.push_back(e.id);
        if(!t.evidence_ids.empty())t.status="in_progress";
        // This question is verification of an obstruction, not an explanation of it.
        if(t.id=="obstruction"&&Find(s->evidence,"obstruction_examined"))t.status="resolved";
        if(t.id=="vey"||t.id=="crystal")t.hypothesis_ids={"vey_prospecting","vey_stayed_at_sample"};
        s->threads.push_back(t);
    }
    s->hypotheses.clear();
    for(const auto* id:{"vey_prospecting","vey_stayed_at_sample"}) {
        InvestigationHypothesis h;h.id=id;h.proposition=InvestigationHypothesisText(id,kGameLanguageEnglish);
        if((legacy||h.id=="vey_prospecting")&&Find(s->evidence,"vey_equipment"))h.supporting_evidence.push_back("vey_equipment");
        if(h.id=="vey_prospecting") {
            if(Find(s->evidence,"vey_survey"))h.supporting_evidence.push_back("vey_survey");
        } else {
            if(legacy&&Find(s->evidence,"crystal_acquired")&&Find(s->evidence,"vey_equipment"))
                h.supporting_evidence.push_back("crystal_acquired");
            if(Find(s->evidence,"vey_survey"))h.contradicting_evidence.push_back("vey_survey");
        }
        h.status=!h.contradicting_evidence.empty()?"contradicted":h.supporting_evidence.size()>=2?"supported":"open";
        s->hypotheses.push_back(h);
    }
}
void Projection(SessionState* state) {
    state->soft_state.active_hypotheses.clear();
    for(const auto& h:state->investigation.hypotheses)
        if(!h.supporting_evidence.empty()||!h.contradicting_evidence.empty())
            state->soft_state.active_hypotheses.push_back(h.status+": "+h.proposition);
    state->hard_state.unresolved_threats.clear();
    const char* labels[]={"locate prospector Vey","verify the invisible obstruction","recover a crystal sample"};
    for(size_t i=0;i<state->investigation.threads.size();++i) {
        // Compatibility projection retains the historical sample sub-question.
        if(i==2&&Find(state->investigation.evidence,"crystal_acquired"))continue;
        if(state->investigation.threads[i].status!="resolved")state->hard_state.unresolved_threats.push_back(labels[i]);
    }
}
void AddEvidence(SessionState* state,const std::string& id,const InvestigationOpportunity& o) {
    if(Find(state->investigation.evidence,id))return;
    InvestigationEvidence e;e.id=id;e.kind=KindFor(id);e.thread_ids=ThreadsFor(id);
    e.place_id=state->current_place_id;e.object_id=o.object_id;e.direction=o.direction;e.turn=state->hard_state.turn_number;
    e.source_event="turn:"+std::to_string(e.turn)+":"+e.kind+":"+o.id;
    e.fact=InvestigationFact(id,kGameLanguageEnglish);
    state->investigation.evidence.push_back(e);
    for(auto& opportunity:state->investigation.opportunities)if(opportunity.id==o.id)opportunity.status="discovered";
    ++state->investigation.revision;Derive(&state->investigation);Projection(state);
}
bool Deposited(const SessionState& s,const std::string& object,const std::string& place) {
    const auto found=s.objects.deposits.find(place);
    if(found!=s.objects.deposits.end())for(const auto& p:found->second)if(p.object_id==object)return true;
    return false;
}
bool KnownPlace(const SessionState& s,const std::string& place) {
    if(place==s.origin_place_id)return true;
    for(const auto& room:s.generated_rooms)if(room.room_id==place)return true;
    return false;
}
const InvisibleBarrier* Barrier(const SessionState& s,const std::string& place,CardinalDirection direction) {
    for(const auto& b:s.invisible_barriers)if(b.place_id==place&&b.direction==direction)return &b;
    return nullptr;
}
bool Fail(std::string* error,const char* message) {if(error)*error=message;return false;}
}

void EnsureInvestigation(SessionState* state) {
    if(!state)return;
    if(state->investigation.schema_version==1) {
        state->investigation.schema_version=2;state->investigation.policy="eryx-clues-llm-v2";
        PreserveLegacyInvestigationPlaces(state);
    }
    Derive(&state->investigation);Projection(state);
}
void PreserveLegacyInvestigationPlaces(SessionState* state) {
    std::vector<std::string> places={state->origin_place_id};
    for(const auto& room:state->generated_rooms)places.push_back(room.room_id);
    for(const auto& place:places) {
        if(place.empty())continue;
        bool found=false;for(const auto& d:state->investigation.decisions)if(d.place_id==place)found=true;
        if(found)continue;
        InvestigationDecision d;d.place_id=place;d.origin="legacy_preserved";d.committed_turn=state->hard_state.turn_number;
        for(const auto& o:state->investigation.opportunities)if(o.place_id==place)d.opportunity_ids.push_back(o.id);
        state->investigation.decisions.push_back(d);
    }
}
bool InvestigationDecisionsPreserved(const SessionState& before,const SessionState& after) {
    for(const auto& old:before.investigation.decisions) {
        bool found=false;for(const auto& d:after.investigation.decisions)
            if(d.place_id==old.place_id&&json(d)==json(old))found=true;
        if(!found)return false;
    }
    return true;
}
std::string SerializeInvestigation(const InvestigationState& state) {return json(state).dump();}
bool ParseInvestigation(const std::string& text,InvestigationState* state,std::string* error) {
    try {
        const auto document=json::parse(text);
        const auto integer=[](const json& value) {
            return value.is_number_integer()&&value>=0&&value<=std::numeric_limits<int>::max();
        };
        if(!integer(document.at("schema_version"))||!integer(document.at("revision")))
            return Fail(error,"investigation versions must be bounded integers");
        for(const auto* collection:{"evidence","opportunities"})for(const auto& entry:document.at(collection))
            if(!integer(entry.at("turn")))return Fail(error,"evidence turn must be a bounded integer");
        auto parsed=document.get<InvestigationState>();
        if((parsed.schema_version!=1&&parsed.schema_version!=2)||parsed.revision<0||
           parsed.policy!=(parsed.schema_version==1?"eryx-clues-v1":"eryx-clues-llm-v2")||
           parsed.opportunities.size()>4||parsed.evidence.size()>6||parsed.threads.size()!=3||parsed.hypotheses.size()!=2)
            return Fail(error,"invalid investigation version or bounds");
        if(parsed.schema_version==2)for(const auto& d:document.at("decisions"))
            if(!integer(d.at("committed_turn")))return Fail(error,"invalid decision turn");
        *state=std::move(parsed);return true;
    } catch(const std::exception& e) {if(error)*error=e.what();return false;}
}
bool ValidateInvestigation(const SessionState& state,std::string* error) {
    const auto& s=state.investigation;
    const bool legacy=s.schema_version==1;
    if((s.schema_version!=1&&s.schema_version!=2)||s.revision<0||
       s.policy!=(legacy?"eryx-clues-v1":"eryx-clues-llm-v2")||s.opportunities.size()>4||s.evidence.size()>6)
        return Fail(error,"invalid investigation header");
    std::set<std::string> ids;
    for(const auto& o:s.opportunities) {
        if(!KnownOpportunity(o.id)||!ids.insert(o.id).second||o.kind!=o.id||!KnownPlace(state,o.place_id)||
            (o.status!="published"&&o.status!="discovered")||o.turn<0||o.turn>state.hard_state.turn_number)
            return Fail(error,"invalid opportunity identity or location");
        if(o.id=="invisible_obstruction") {
            CardinalDirection d;
            if(!o.object_id.empty()||!ParseCardinalDirection(o.direction.c_str(),&d)||d==kDirectionUnknown||!Barrier(state,o.place_id,d))
                return Fail(error,"missing registered barrier");
            for(const auto& link:state.room_links)if(link.from_place_id==o.place_id&&link.direction==d)
                return Fail(error,"barrier overlaps a published link");
        } else {
            const auto object=state.objects.instances.find(o.object_id);
            if(!ClueIdentity(o.object_id,o.id)||!o.direction.empty()||object==state.objects.instances.end()||object->second.origin_place!=o.place_id)
                return Fail(error,"missing authorized clue object");
        }
    }
    ids.clear();
    for(const auto& e:s.evidence) {
        const auto opportunity=OpportunityForEvidence(e.id);
        const auto* o=Find(s.opportunities,opportunity);
        if(!o||!ids.insert(e.id).second||e.kind!=KindFor(e.id)||e.thread_ids!=ThreadsFor(e.id)||
            (!KnownPlace(state,e.place_id)||(o->id!="crystal_sample"&&e.place_id!=o->place_id))||e.object_id!=o->object_id||e.direction!=o->direction||
            e.fact!=InvestigationFact(e.id,kGameLanguageEnglish)||e.turn<o->turn||e.turn>state.hard_state.turn_number||
            e.source_event!="turn:"+std::to_string(e.turn)+":"+e.kind+":"+o->id||o->status!="discovered")
            return Fail(error,"invalid evidence provenance");
        if(e.kind=="contact"||e.id=="obstruction_examined") {
            CardinalDirection d;ParseCardinalDirection(e.direction.c_str(),&d);
            if(!Barrier(state,e.place_id,d)->discovered)return Fail(error,"contact not published");
        }
        if(e.id=="obstruction_examined"&&!Find(s.evidence,"obstruction_contact"))return Fail(error,"examination without contact");
    }
    for(const auto& o:s.opportunities)if(o.status=="discovered") {
        bool found=false;for(const auto& e:s.evidence)if(OpportunityForEvidence(e.id)==o.id)found=true;
        if(!found)return Fail(error,"discovered opportunity without evidence");
    }
    if(!legacy) {
        std::set<std::string> places,assigned;
        for(const auto& d:s.decisions) {
            if(!KnownPlace(state,d.place_id)||!places.insert(d.place_id).second||
               (d.origin!="llm"&&d.origin!="legacy_preserved")||d.committed_turn<0||d.committed_turn>state.hard_state.turn_number||
               (d.place_id==state.origin_place_id&&d.origin!="legacy_preserved"))return Fail(error,"invalid placement decision");
            for(const auto& id:d.opportunity_ids) {
                const auto* o=Find(s.opportunities,id);
                if(!o||o->place_id!=d.place_id||!assigned.insert(id).second||
                   (d.origin=="llm"&&o->turn!=d.committed_turn))return Fail(error,"decision disagrees with opportunity");
            }
        }
        if(!places.count(state.origin_place_id)||assigned.size()!=s.opportunities.size())return Fail(error,"missing placement decision");
        for(const auto& room:state.generated_rooms)if(!places.count(room.room_id))return Fail(error,"undecided accepted place");
    }
    auto derived=s;Derive(&derived,legacy);
    if(json(derived.threads)!=json(s.threads)||json(derived.hypotheses)!=json(s.hypotheses))return Fail(error,"unjustified conclusion");
    return true;
}

bool PrepareInvestigationOpportunity(const SessionState& state,const std::string& id,const std::string& place,
    CardinalDirection direction,const ObjectPlacement* placement,SessionState* candidate,std::string* error) {
    if(!candidate||!KnownOpportunity(id)||Find(state.investigation.opportunities,id)||!KnownPlace(state,place))
        return Fail(error,"unknown, repeated or unavailable opportunity");
    InvestigationOpportunity o;o.id=id;o.kind=id;o.place_id=place;o.turn=state.hard_state.turn_number;
    auto next=state;EnsureInvestigation(&next);
    if(id=="invisible_obstruction") {
        if(direction==kDirectionUnknown||Barrier(state,place,direction))return Fail(error,"invalid barrier direction");
        for(const auto& link:state.room_links)if(link.from_place_id==place&&link.direction==direction)
            return Fail(error,"established link protected");
        o.direction=CardinalDirectionToString(direction);
        InvisibleBarrier b;b.place_id=place;b.direction=direction;b.evidence="Registered contact with a transparent, impassable plane.";
        next.invisible_barriers.push_back(b);
    } else {
        if(!placement||direction!=kDirectionUnknown)return Fail(error,"clue requires a validated placement");
        o.object_id="clue:"+id;
        // An imported legacy object may already use the preferred ID. Keep it
        // intact and bind this future opportunity to a fresh instance instead.
        for(size_t suffix=1;next.objects.instances.count(o.object_id);++suffix)
            o.object_id="clue:"+id+":"+std::to_string(suffix);
        ObjectInstance object;object.id=o.object_id;object.label=InvestigationHint(id,state.language);
        object.description=object.label;object.origin_place=place;object.created_turn=o.turn;
        object.portable=id=="crystal_sample"; // Scenario provenance is the opportunity, not a literary source.
        next.objects.instances[object.id]=object;auto p=*placement;p.object_id=object.id;
        next.objects.deposits[place].push_back(p);++next.objects.revision;
    }
    next.investigation.opportunities.push_back(o);++next.investigation.revision;
    *candidate=std::move(next);return true;
}
bool PrepareInvestigationObservation(const SessionState& state,const std::string& id,int revision,
    SessionState* candidate,std::string* error) {
    const auto* o=Find(state.investigation.opportunities,id);
    if(!candidate||revision!=state.investigation.revision)return Fail(error,"stale investigation revision");
    if(!o||(o->id!="crystal_sample"&&o->place_id!=state.current_place_id))return Fail(error,"clue is not accessible here");
    if(id=="invisible_obstruction") {
        if(!Find(state.investigation.evidence,"obstruction_contact"))return Fail(error,"contact required before examination");
    } else if(!Deposited(state,o->object_id,state.current_place_id)&&!Has(state.hard_state.inventory_items,o->object_id))
        return Fail(error,"object is elsewhere");
    auto next=state;
    AddEvidence(&next,id=="crystal_sample"?"crystal_source":id=="invisible_obstruction"?"obstruction_examined":id,*o);
    *candidate=std::move(next);return true;
}
void RecordInvestigationTransfer(const SessionState& before,const std::string& action,const std::string& id,SessionState* candidate) {
    const auto* o=Find(before.investigation.opportunities,"crystal_sample");
    if(!candidate||action!="take"||!o||o->object_id!=id||!Deposited(before,id,before.current_place_id)||
       Has(before.hard_state.inventory_items,id)||!Has(candidate->hard_state.inventory_items,id)||
       Deposited(*candidate,id,before.current_place_id)||candidate->objects.revision!=before.objects.revision+1)return;
    AddEvidence(candidate,"crystal_acquired",*o);
}
void RecordInvestigationContact(const SessionState& before,CardinalDirection direction,SessionState* candidate) {
    const auto* o=Find(before.investigation.opportunities,"invisible_obstruction");
    const auto* prior=Barrier(before,before.current_place_id,direction);
    const auto* after=candidate?Barrier(*candidate,before.current_place_id,direction):nullptr;
    if(!o||o->place_id!=before.current_place_id||o->direction!=CardinalDirectionToString(direction)||
       !prior||prior->discovered||!after||!after->discovered||candidate->current_place_id!=before.current_place_id)return;
    AddEvidence(candidate,"obstruction_contact",*o);
}

std::string InvestigationContext(const SessionState& state) {
    const auto& s=state.investigation;
    json known=json::array(),accessible=json::array(),hypotheses=json::array();
    for(const auto& e:s.evidence) {
        json item={{"id",e.id},{"fact",e.fact},{"place_id",e.place_id},{"object_id",e.object_id},{"direction",e.direction}};
        if(!e.object_id.empty()) {
            std::string location=Has(state.hard_state.inventory_items,e.object_id)?"carried":"unknown";
            for(const auto& room:state.objects.deposits)if(Deposited(state,e.object_id,room.first))location=room.first;
            item["current_object_location"]=location;
        }
        known.push_back(item);
    }
    for(const auto& o:s.opportunities)if(o.place_id==state.current_place_id||
        (o.id=="crystal_sample"&&(Has(state.hard_state.inventory_items,o.object_id)||Deposited(state,o.object_id,state.current_place_id)))) {
        if(o.id=="invisible_obstruction"&&!Find(s.evidence,"obstruction_contact"))continue;
        if(!o.object_id.empty()&&!Deposited(state,o.object_id,state.current_place_id)&&!Has(state.hard_state.inventory_items,o.object_id))continue;
        const auto observed=o.id=="crystal_sample"?"crystal_source":
            o.id=="invisible_obstruction"?"obstruction_examined":o.id;
        accessible.push_back({{"id",o.id},{"hint",InvestigationHint(o.id,state.language)},{"object_id",o.object_id},
            {"action","examine_clue"},{"already_observed",Find(s.evidence,observed)!=nullptr}});
    }
    // Do not leak even a hypothesis's hidden premise before there is evidence.
    for(const auto& h:s.hypotheses)if(!h.supporting_evidence.empty()||!h.contradicting_evidence.empty())hypotheses.push_back(h);
    return json({{"revision",s.revision},{"questions",s.threads},{"known_evidence",known},{"hypotheses",hypotheses},
        {"accessible_clues",accessible},{"rule","Only known_evidence are established facts. Clues require examine_clue; taking requires a validated object transfer. A player note and a title confer no ownership or discovery. Vey remains missing. Supported hypotheses remain interpretations. Unknown causes and fate must remain unknown."}}).dump();
}
}
