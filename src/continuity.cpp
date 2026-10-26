#include "continuity.h"
#include "nlohmann/json.hpp"
#include <algorithm>
#include <set>
#include <chrono>

namespace liminal {
using json = nlohmann::json;
std::string SerializeContinuityContext(const ContinuityContextV1& c) {
    auto root=json::parse(c.core_json);
    root["version"]=1;
    root["recent_events"]=json::array();
    for(const auto& event:c.events_json)root["recent_events"].push_back(json::parse(event));
    root["subjective_notes"]=c.notes;
    return root.dump();
}
static bool FirstSentence(json& value) {
    if(!value.is_string())return false;
    const auto text=value.get<std::string>();
    size_t end=std::string::npos;
    for(size_t i=0;i<text.size();++i) {
        if((text[i]=='.'||text[i]=='!'||text[i]=='?') &&
           (i+1==text.size()||text[i+1]==' '||text[i+1]=='\n')) {end=i+1;break;}
        for(const auto* stop:{u8"。",u8"！",u8"？"})
            if(text.compare(i,3,stop)==0){end=i+3;break;}
        if(end!=std::string::npos)break;
    }
    if(end==std::string::npos||end==text.size())return false;
    value=text.substr(0,end);return true; // Only complete UTF-8 sentences.
}
bool FitContinuityPrompt(const ContinuityContextV1& original,int context_tokens,int output_tokens,
                         int memory_tokens,const ContinuityPromptBuilder& build,
                         const ContinuityTokenCounter& count,std::vector<LlmPromptMessage>* messages,
                         std::string* metrics,std::string* error) {
    constexpr int margin=32;
    const auto started=std::chrono::steady_clock::now();
    ContinuityContextV1 selected=original;
    json shortened=json::array();
    int before=0,after=0,mandatory=0,available=0;
    const auto finish=[&](bool success) {
        json ids=json::array();for(const auto& event:selected.events_json)ids.push_back(json::parse(event).at("id"));
        *metrics=json({{"policy","continuity-v1/recent6-places2"},{"context_tokens",context_tokens},
            {"output_reserved",output_tokens},{"technical_margin",margin},{"memory_cap",memory_tokens},
            {"memory_available",available},{"mandatory_prompt_tokens",mandatory},
            {"prompt_tokens_before",before},{"prompt_tokens_after",after},
            {"memory_tokens",std::max(0,after-mandatory)},{"retained_event_ids",ids},
            {"omitted_events",original.events_json.size()-selected.events_json.size()},
            {"omitted_notes",original.notes.size()-selected.notes.size()},{"shortened_fields",shortened},
            {"selection_ms",std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count()},
            {"success",success},{"error",*error}}).dump();
        return success;
    };
    *error="";
    if(!count(build(selected),&before,error))return finish(false);
    // Remove repeated consultations, then select the newest relevant six events / two places.
    std::set<std::string> places,duplicates;
    selected.events_json.clear();
    for(auto it=original.events_json.rbegin();it!=original.events_json.rend();++it) {
        const auto e=json::parse(*it);const auto intent=e.value("intent",std::string());
        if(intent=="read_inventory"||intent=="inspect_inventory_item"||intent=="read_field_notes")continue;
        const auto place=e.value("place_id",std::string());
        if(!places.count(place)&&places.size()>=2)continue;
        const auto key=place+"\n"+e.value("command",std::string())+"\n"+e.value("outcome",std::string());
        if(!duplicates.insert(key).second)continue;
        places.insert(place);selected.events_json.push_back(*it);
        if(selected.events_json.size()==6)break;
    }
    std::reverse(selected.events_json.begin(),selected.events_json.end());
    // Preserve one useful event even when the only history was a consultation.
    if(selected.events_json.empty()&&!original.events_json.empty())selected.events_json.push_back(original.events_json.back());
    if(selected.notes.size()>4)selected.notes.erase(selected.notes.begin(),selected.notes.end()-4);
    bool compacted_events=false,compacted_origin=false;
    for(;;) {
        auto core=selected;core.events_json.clear();core.notes.clear();
        if(!count(build(core),&mandatory,error))return finish(false);
        available=std::max(0,std::min(memory_tokens,context_tokens-output_tokens-margin-mandatory));
        *messages=build(selected);
        if(!count(*messages,&after,error))return finish(false);
        if(after+output_tokens+margin<=context_tokens && after-mandatory<=available)return finish(true);
        if(!selected.notes.empty()){selected.notes.erase(selected.notes.begin());continue;}
        if(selected.events_json.size()>1){selected.events_json.erase(selected.events_json.begin());continue;}
        if(!compacted_events) {
            compacted_events=true;
            for(auto& text:selected.events_json) {
                auto e=json::parse(text);
                if(e.contains("description")&&FirstSentence(e["description"]))shortened.push_back(e.at("id").get<std::string>()+".description");
                text=e.dump();
            }
            continue;
        }
        if(!compacted_origin) {
            compacted_origin=true;auto root=json::parse(selected.core_json);
            if(root.contains("origin")&&root["origin"].contains("description")&&FirstSentence(root["origin"]["description"]))
                shortened.push_back("origin.description");
            selected.core_json=root.dump();continue;
        }
        *error="context_limit: mandatory continuity, latest useful result and reserved output do not fit";
        return finish(false);
    }
}
}
