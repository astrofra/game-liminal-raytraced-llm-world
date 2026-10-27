#include "investigation.h"
#include "object_interactions.h"
#include "nlohmann/json.hpp"
#include <algorithm>
#include <cmath>
#include <set>
#include <stdexcept>

namespace liminal {
using json=nlohmann::json;
namespace {
bool Fail(std::string* error,const std::string& reason) {if(error)*error=reason;return false;}
bool Ground(const InvestigationSite& site) {return site.kind=="ground";}
bool Overlap(const ObjectPlacement& a,const ObjectPlacement& b) {
    const double x=a.x-b.x,y=a.y-b.y,z=a.z-b.z;return x*x+y*y+z*z<.36;
}
}
std::vector<std::string> AvailableInvestigationOpportunities(const SessionState& state) {
    std::vector<std::string> result;
    // Catalogue order carries no scheduling priority. All entries are optional.
    for(const auto* id:{"crystal_sample","invisible_obstruction","vey_equipment","vey_survey"}) {
        bool found=false;for(const auto& o:state.investigation.opportunities)if(o.id==id)found=true;
        if(!found)result.push_back(id);
    }
    return result;
}
std::string InvestigationAuthoringContext(const SessionState& state,const std::vector<InvestigationSite>& sites) {
    json available=json::array(),locations=json::array(),placed=json::array(),decisions=json::array();
    for(const auto& id:AvailableInvestigationOpportunities(state)) {
        std::string object_id="clue:"+id;
        for(size_t suffix=1;state.objects.instances.count(object_id);++suffix)
            object_id="clue:"+id+":"+std::to_string(suffix);
        json compatible=json::array();for(const auto& site:sites)
            if((id=="invisible_obstruction")==!Ground(site))compatible.push_back(site.id);
        // No hidden identification, survey contents or fate are disclosed here.
        available.push_back({{"id",id},{"public_name",id=="invisible_obstruction"?"":InvestigationHint(id,state.language)},
            {"kind",id=="invisible_obstruction"?"approach":"ground"},{"sites",compatible},
            {"object_id",id=="invisible_obstruction"?"":object_id},
            {"arrival_rule",id=="invisible_obstruction"?"Invisible and uncontacted: do not announce it or describe a collision.":
                "Describe the public object only. Its identification/content requires later examination; do not call it Vey's."}});
    }
    for(const auto& site:sites) {
        json item={{"id",site.id},{"kind",site.kind}};
        if(Ground(site))item["position"]={site.placement.x,site.placement.y,site.placement.z};
        else item["direction"]=CardinalDirectionToString(site.direction);
        locations.push_back(item);
    }
    for(const auto& o:state.investigation.opportunities)placed.push_back({{"id",o.id},{"place",o.place_id},{"object",o.object_id}});
    int empty_since=0;
    for(auto it=state.investigation.decisions.rbegin();it!=state.investigation.decisions.rend();++it) {
        if(it->place_id==state.origin_place_id)continue;
        if(!it->opportunity_ids.empty())break;
        ++empty_since;
    }
    for(const auto& d:state.investigation.decisions)decisions.push_back({d.place_id,d.origin,d.opportunity_ids});
    return json({{"revision",state.investigation.revision},{"available",available},{"sites",locations},
        {"placed",placed},{"decisions",decisions},{"accepted_places_since_last_distribution",empty_since},
        {"rule","AUTHORING ONLY, not player knowledge. Decide now which available opportunities belong in this NEW place. Choose zero, one or several compatible entries, in any order; an empty placements array is a deliberate permanent choice. No priority or quota. Shape the pacing, allow pauses and connections, and offer concrete leads for unresolved questions. Choose only supplied sites, at most one object per site. A selected obstruction's direction MUST be open in exits until contact; preserve another useful continuation. Do not reveal hidden identity, contents, contact or causes. Write narration and deposited_ids against your selected public objects. Never grant evidence on arrival."}}).dump();
}
std::string InvestigationPlacementSchema(const SessionState& state,const std::vector<InvestigationSite>& sites) {
    json choices=json::array();
    for(const auto& id:AvailableInvestigationOpportunities(state)) {
        json allowed=json::array();for(const auto& site:sites)
            if((id=="invisible_obstruction")==!Ground(site))allowed.push_back(site.id);
        if(allowed.empty())continue;
        choices.push_back({{"type","object"},{"properties",{
            {"opportunity_id",{{"enum",json::array({id})}}},{"site_id",{{"enum",allowed}}}}},
            {"required",{"opportunity_id","site_id"}},{"additionalProperties",false}});
    }
    json placements={{"type","array"},{"maxItems",std::min<size_t>(4,choices.size())}};
    placements["items"]=choices.empty()?json({{"type","object"}}):json({{"anyOf",choices}});
    return json({{"type","object"},{"properties",{{"revision",{{"enum",{state.investigation.revision}}}},
        {"placements",placements}}},{"required",{"revision","placements"}},{"additionalProperties",false}}).dump();
}
bool PrepareInvestigationDistribution(const SessionState& state,const std::string& place,
    const std::vector<InvestigationSite>& sites,const std::string& text,SessionState* candidate,std::string* error) {
    if(!candidate||state.investigation.schema_version!=2)return Fail(error,"invalid distribution state");
    bool known=false;for(const auto& room:state.generated_rooms)if(room.room_id==place)known=true;
    if(!known||place==state.origin_place_id||place!=state.current_place_id)return Fail(error,"distribution requires the new current place");
    for(const auto& d:state.investigation.decisions)if(d.place_id==place)return Fail(error,"placement decision is immutable");
    try {
        if(text.size()>4096)return Fail(error,"oversized placement proposal");
        std::vector<std::set<std::string>> keys;
        auto callback=[&](int,json::parse_event_t event,json& value) {
            if(event==json::parse_event_t::object_start)keys.emplace_back();
            if(event==json::parse_event_t::key&&!keys.back().insert(value.get<std::string>()).second)
                throw std::runtime_error("duplicate placement key");
            if(event==json::parse_event_t::object_end)keys.pop_back();
            return true;
        };
        const auto proposal=json::parse(text,callback);
        if(!proposal.is_object()||proposal.size()!=2||!proposal.contains("revision")||!proposal.contains("placements")||
           !proposal["revision"].is_number_integer()||proposal["revision"]!=state.investigation.revision||
           !proposal["placements"].is_array()||proposal["placements"].size()>4)return Fail(error,"invalid placement envelope or revision");
        std::set<std::string> site_ids;
        for(const auto& s:sites) {
            if(s.id.empty()||!site_ids.insert(s.id).second||(s.kind!="ground"&&s.kind!="approach"))return Fail(error,"invalid offered site");
            if(Ground(s)) {
                if(s.direction!=kDirectionUnknown||!std::isfinite(s.placement.x)||!std::isfinite(s.placement.y)||!std::isfinite(s.placement.z))
                    return Fail(error,"invalid offered position");
            } else if(s.direction!=kDirectionNorth&&s.direction!=kDirectionEast&&s.direction!=kDirectionSouth&&s.direction!=kDirectionWest)
                return Fail(error,"invalid offered approach");
        }
        auto next=state;std::set<std::string> selected,used;
        const auto available=AvailableInvestigationOpportunities(state);
        std::vector<ObjectPlacement> occupied;
        const auto deposit=state.objects.deposits.find(place);if(deposit!=state.objects.deposits.end())occupied=deposit->second;
        InvestigationDecision decision;decision.place_id=place;decision.origin="llm";decision.committed_turn=state.hard_state.turn_number;
        for(const auto& entry:proposal["placements"]) {
            if(!entry.is_object()||entry.size()!=2||!entry.contains("opportunity_id")||!entry.contains("site_id")||
               !entry["opportunity_id"].is_string()||!entry["site_id"].is_string())return Fail(error,"invalid placement entry");
            const auto id=entry["opportunity_id"].get<std::string>(),site_id=entry["site_id"].get<std::string>();
            if(std::find(available.begin(),available.end(),id)==available.end()||!selected.insert(id).second||!used.insert(site_id).second)
                return Fail(error,"unavailable or repeated placement");
            const auto site=std::find_if(sites.begin(),sites.end(),[&](const InvestigationSite& s){return s.id==site_id;});
            if(site==sites.end()||((id=="invisible_obstruction")==Ground(*site)))return Fail(error,"unknown or incompatible site");
            if(Ground(*site)) {
                for(const auto& p:occupied)if(Overlap(p,site->placement))return Fail(error,"overlapping selected objects");
                occupied.push_back(site->placement);
            }
            SessionState prepared;
            if(!PrepareInvestigationOpportunity(next,id,place,site->direction,Ground(*site)?&site->placement:nullptr,&prepared,error))return false;
            next=std::move(prepared);decision.opportunity_ids.push_back(id);
        }
        next.investigation.decisions.push_back(decision);++next.investigation.revision;
        if(!ValidateInvestigation(next,error)||!ValidateObjectState(next,error))return false;
        *candidate=std::move(next);if(error)error->clear();return true;
    } catch(const std::exception& e) {return Fail(error,e.what());}
}
}
