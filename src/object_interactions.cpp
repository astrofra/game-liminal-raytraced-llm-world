#include "object_interactions.h"
#include "inventory.h"
#include "nlohmann/json.hpp"
#include <algorithm>
#include <cmath>
#include <cctype>
#include <set>
#include <regex>
#include <sstream>

namespace liminal {
using json = nlohmann::json;
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ObjectInstance, id, label, description, aliases, origin_place, source_quote, visual_id, created_turn, portable)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ObjectPlacement, object_id, x, y, z)
void to_json(json& node,const ObjectSource& source) {
    node={{"place_id",source.place_id},{"quote",source.quote},{"label",source.label},{"object_id",source.object_id},
        {"reason",source.reason},{"visual_id",source.visual_id},{"allowed",source.allowed},{"remaining",source.remaining},
        {"material_source",source.material_source}};
}
void from_json(const json& node,ObjectSource& source) {
    node.at("place_id").get_to(source.place_id);node.at("quote").get_to(source.quote);node.at("label").get_to(source.label);
    node.at("object_id").get_to(source.object_id);node.at("reason").get_to(source.reason);node.at("visual_id").get_to(source.visual_id);
    node.at("allowed").get_to(source.allowed);node.at("remaining").get_to(source.remaining);
    source.material_source=node.value("material_source",false);
}
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ObjectState, version, revision, next_id, instances, deposits, sources)

namespace {
bool Contains(const std::vector<std::string>& v, const std::string& id) {
    return std::find(v.begin(), v.end(), id) != v.end();
}
const char* Text(GameLanguage language, const char* en, const char* fr, const char* no,
                 const char* da, const char* de, const char* it) {
    const char* values[]={en,fr,no,da,de,it};
    int i=static_cast<int>(language);return values[i>=0&&i<6?i:0];
}
bool Word(const std::string& text, const std::string& term) {
    return (" "+text+" ").find(" "+term+" ")!=std::string::npos;
}
std::string FoldNumber(const std::string& text) {
    std::istringstream words(text);std::string word,out;
    while(words>>word) {
        if(word.size()>4&&word.back()=='s'&&word[word.size()-2]!='s')word.pop_back();
        if(!out.empty())out+=' ';out+=word;
    }
    return out;
}
bool ForbiddenTarget(const std::string& label) {
    const auto text=NormalizeObjectText(label);
    for(const char* term:{"mur","murs","wall","walls","sol","floor","plafond","ceiling","portal","portail",
        "monumental","structure","porte","door","wand","boden","vegg","gulv","vaeg","parete","pavimento"})
        if(Word(text,term))return true;
    return false;
}
bool UnsupportedSource(const std::string& quote,const std::string& label={}) {
    const auto text=NormalizeObjectText(quote);
    for(const char* term:{"peut etre","imaginaire","hypothetique","pourrait","comme si",
        "might","imaginary","hypothetical","could"})
        if(Word(text,term))return true;
    if(!label.empty()) {
        const auto target=NormalizeObjectText(label);
        const std::regex negation("\\b(?:aucun|aucune|aucuns|aucunes|sans|pas de|no|without|kein|keine|nessun|ingen) (?:[a-z]+ ){0,3}"+target+"\\b");
        if(std::regex_search(text,negation))return true;
    }
    return false;
}
std::string SourceSentence(const std::string& description,const std::string& quote) {
    const auto pos=description.find(quote);
    if(pos==std::string::npos)return {};
    const auto begin=description.find_last_of(".!?\n",pos?pos-1:0);
    const auto end=description.find_first_of(".!?\n",pos+quote.size()-(quote.empty()?0:1));
    const size_t first=begin==std::string::npos?0:begin+1;
    if(first>pos)return quote;
    return description.substr(first,(end==std::string::npos?description.size():end+1)-first);
}
} // namespace

std::string NormalizeObjectText(const std::string& text) {
    std::string out=text;
    const std::pair<const char*,const char*> folds[]={
        {"À","a"},{"Â","a"},{"Ä","a"},{"à","a"},{"â","a"},{"ä","a"},{"É","e"},{"È","e"},{"Ê","e"},
        {"é","e"},{"è","e"},{"ê","e"},{"ë","e"},{"î","i"},{"ï","i"},{"ô","o"},{"ö","o"},
        {"ù","u"},{"û","u"},{"ü","u"},{"ç","c"},{"Ç","c"},{"ø","o"},{"å","a"},{"æ","ae"},
        {"’"," "},{"‘"," "},{"—"," "},{"–"," "},{"ß","ss"}};
    for(const auto& fold:folds) {
        size_t p=0;while((p=out.find(fold.first,p))!=std::string::npos){out.replace(p,std::string(fold.first).size(),fold.second);p+=std::string(fold.second).size();}
    }
    for(char& c:out)if(static_cast<unsigned char>(c)<128)c=std::isalnum(static_cast<unsigned char>(c))?static_cast<char>(std::tolower(static_cast<unsigned char>(c))):' ';
    std::istringstream stream(out);std::string word;out.clear();
    while(stream>>word){if(!out.empty())out+=' ';out+=word;}return out;
}
bool MayRequestObjectTransfer(const std::string& command) {
    const auto text=NormalizeObjectText(command);
    for(const auto* term:{"prendre","prends","ramasse","ramasser","emporte","emporter","emparer","recuperer",
        "depose","deposer","pose","poser","take","pick","collect","retrieve","carry","put","set down",
        "prelever","preleve","recolter","recueillir","recueille","scoop","gather","pick up",
        "aufheben","nehmen","ablegen","plukke","samle","legge","laegge","raccogliere","raccogli","prendere","posare"})
        if(Word(text,term))return true;
    return false;
}
bool ObjectTargetGrounded(const std::string& command,const std::string& label) {
    auto expand=[](std::string text) {
        for(const auto& group:std::vector<std::vector<std::string>>{
            {"stone","pebble","caillou","pierre","stein","steinen","sten","sasso","pietra"},
            {"rubble","gravats","debris","schutt","macerie","grus"},
            {"chalk","craie","kreide","kritt","kridt","gesso"},
            {"slate","tablette","map","carte","kartbrett","kartentafel","mappa","tavoletta"},
            {"tongs","pinces","provetang","probenzange","pinze"}}) {
            bool found=false;for(const auto& word:group)if(Word(text,word))found=true;
            if(found)for(const auto& word:group)text+=' '+word;
        }
        return text;
    };
    const auto query=FoldNumber(expand(NormalizeObjectText(command)));
    std::istringstream words(FoldNumber(expand(NormalizeObjectText(label))));std::string word;
    const std::string stop=" petit petite petits petites small little detache detachee loose here ici porte main seul seul objet object fragment morceau piece ground sol solitario single disponible portee avec dans pour devant derriere sous sur ces cette the une mes mon ";
    while(words>>word)if(word.size()>=3&&stop.find(" "+word+" ")==std::string::npos&&Word(query,word))return true;
    return false;
}

void EnsureObjectRegistry(SessionState* state) {
    if(!state)return;
    for(const auto& id:state->hard_state.inventory_items)if(!state->objects.instances.count(id)) {
        ObjectInstance object;object.id=id;object.label=InventoryItemName(id,state->language);
        object.description=InventoryItemDescription(id,state->language);object.portable=IsKnownInventoryItem(id);
        state->objects.instances.emplace(id,std::move(object));
    }
}
bool ValidateObjectState(const SessionState& state,std::string* error) {
    auto fail=[&](const char* why){if(error)*error=why;return false;};
    if(state.objects.version!=1||state.objects.revision<0||state.objects.next_id<1)return fail("invalid_object_version");
    std::map<std::string,int> counts;
    for(const auto& id:state.hard_state.inventory_items)++counts[id];
    for(const auto& room:state.objects.deposits) {
        if(room.first.empty())return fail("empty_object_place");
        for(const auto& p:room.second) {
            if(!std::isfinite(p.x)||!std::isfinite(p.y)||!std::isfinite(p.z))return fail("invalid_placement");
            ++counts[p.object_id];
        }
    }
    for(const auto& count:counts)if(count.second!=1||!state.objects.instances.count(count.first))return fail("duplicate_or_unknown_owner");
    for(const auto& entry:state.objects.instances)
        if(entry.first.empty()||entry.second.id!=entry.first||counts[entry.first]!=1)return fail("orphan_object");
    std::set<std::pair<std::string,std::string>> sources;
    for(const auto& source:state.objects.sources) {
        if(source.place_id.empty()||source.quote.empty()||source.remaining<0||source.remaining>1||
            !sources.emplace(source.place_id,NormalizeObjectText(source.quote)).second)return fail("invalid_source");
        if(!source.object_id.empty()) {
            const auto it=state.objects.instances.find(source.object_id);
            if(it==state.objects.instances.end()||it->second.origin_place!=source.place_id||
               it->second.source_quote!=source.quote||source.remaining!=0)return fail("invalid_source_object");
        }
    }
    for(const auto& entry:state.objects.instances)if(!entry.second.source_quote.empty()) {
        const auto found=std::find_if(state.objects.sources.begin(),state.objects.sources.end(),[&](const ObjectSource& source){
            return source.object_id==entry.first&&source.place_id==entry.second.origin_place&&source.quote==entry.second.source_quote;});
        if(found==state.objects.sources.end())return fail("missing_object_provenance");
    }
    if(error)error->clear();return true;
}
std::string ObjectName(const SessionState& state,const std::string& id) {
    if(IsKnownInventoryItem(id))return InventoryItemName(id,state.language);
    const auto it=state.objects.instances.find(id);
    if(it==state.objects.instances.end())return id;
    size_t count=0,ordinal=0;
    for(const auto& entry:state.objects.instances)if(entry.second.label==it->second.label) {
        ++count;if(entry.first==id)ordinal=count;
    }
    return it->second.label+(count>1?" ("+std::to_string(ordinal)+")":"");
}
std::string ObjectVisualName(const std::string& id) {
    static const char* hex="0123456789abcdef";std::string name="mobile_";
    for(unsigned char c:id){name+=hex[c>>4];name+=hex[c&15];}return name;
}
std::vector<std::string> MatchLocalObjects(const SessionState& state,const std::string& target) {
    const auto text=NormalizeObjectText(target);std::set<std::string> local(state.hard_state.inventory_items.begin(),state.hard_state.inventory_items.end());
    auto room=state.objects.deposits.find(state.current_place_id);
    if(room!=state.objects.deposits.end())for(const auto& p:room->second)local.insert(p.object_id);
    std::vector<std::string> matches;
    for(const auto& id:local) {
        const auto it=state.objects.instances.find(id);if(it==state.objects.instances.end())continue;
        const auto& object=it->second;
        bool match=text==NormalizeObjectText(id)||text==NormalizeObjectText(ObjectName(state,id));
        for(const auto& alias:object.aliases)match=match||text==NormalizeObjectText(alias);
        // A short noun may designate multiple homonyms: return all, never pick the first.
        if(!text.empty()&&Word(NormalizeObjectText(object.label),text))match=true;
        if(!text.empty()&&Word(FoldNumber(NormalizeObjectText(object.label)),FoldNumber(text)))match=true;
        const auto known=ParseInventoryRequest("take "+text);
        if(known.item_id==id)match=true;
        if(match)matches.push_back(id);
    }
    return matches;
}
bool PrepareObjectTransfer(const SessionState& state,const ObjectProposal& p,const ObjectPlacement* placement,
                           SessionState* candidate,std::string* reason) {
    auto fail=[&](const char* why){if(reason)*reason=why;return false;};
    if(!candidate||!ValidateObjectState(state,reason))return false;
    if(p.revision!=state.objects.revision)return fail("stale_revision");
    if(p.verdict!="allow")return fail(p.verdict=="forbid"?"forbidden":"clarify");
    if(p.action!="take"&&p.action!="drop")return fail("unsupported_action");
    SessionState next=state;
    auto& carried=next.hard_state.inventory_items;
    auto& deposited=next.objects.deposits[state.current_place_id];
    if(p.origin=="explicit") {
        auto object=next.objects.instances.find(p.object_id);
        if(object==next.objects.instances.end())return fail("unknown_object");
        if(!object->second.portable)return fail("unknown_capability");
        if(p.action=="drop") {
            if(!Contains(carried,p.object_id))return fail("not_carried");
            if(!placement||!std::isfinite(placement->x)||!std::isfinite(placement->y)||!std::isfinite(placement->z))return fail("no_placement");
            auto located=*placement;located.object_id=p.object_id;
            carried.erase(std::find(carried.begin(),carried.end(),p.object_id));deposited.push_back(located);
        } else {
            if(Contains(carried,p.object_id))return fail("already_carried");
            auto found=std::find_if(deposited.begin(),deposited.end(),[&](const ObjectPlacement& item){return item.object_id==p.object_id;});
            if(found==deposited.end())return fail("not_here");
            deposited.erase(found);carried.push_back(p.object_id);
        }
    } else if(p.origin=="implicit"&&p.action=="take") {
        if(!p.object_id.empty()||p.quote.empty()||p.quote.size()>1500||p.label.empty()||p.label.size()>120)return fail("invalid_source");
        const auto reference=ObjectEvidence(state,state.current_place_id);
        if(reference.find(p.quote)==std::string::npos)return fail("unattested_source");
        const auto quote=SourceSentence(reference,p.quote);
        if(!Word(NormalizeObjectText(quote),NormalizeObjectText(p.label))||UnsupportedSource(quote,p.label))return fail("unattested_source");
        if(ForbiddenTarget(p.label))return fail("fixed_scenery");
        for(const auto* anchored:{"scelle","scellee","solidaire","encastre","encastree","attached","fixed","anchored","fest verankert"})
            if(Word(NormalizeObjectText(quote),anchored))return fail("fixed_scenery");
        const auto norm=NormalizeObjectText(quote);
        for(const auto& source:state.objects.sources)if(source.place_id==state.current_place_id&&
            (NormalizeObjectText(source.quote)==norm||FoldNumber(NormalizeObjectText(source.label))==FoldNumber(NormalizeObjectText(p.label))))
            return fail(source.allowed?"source_exhausted":"forbidden");
        // Existing explicit identities take precedence over literary mentions.
        const auto matches=MatchLocalObjects(state,p.label);
        if(matches.size()>1)return fail("clarify");
        if(matches.size()==1) {
            ObjectProposal existing=p;existing.origin="explicit";existing.object_id=matches[0];
            return PrepareObjectTransfer(state,existing,nullptr,candidate,reason);
        }
        // A depiction needs a validated scene binding before it can be removed.
        // That geometric validation is performed by the runner, never by the LLM.
        ObjectInstance object;
        do {object.id="object:"+std::to_string(next.objects.next_id++);}while(next.objects.instances.count(object.id));
        const bool rubble=ObjectTargetGrounded(p.label,"gravats rubble schutt macerie grus");
        const auto prefix=rubble?Text(state.language,"fragment of ","fragment de ","fragment av ","fragment af ","Fragment von ","frammento di "):
            Text(state.language,"sample of ","échantillon de ","prøve av ","prøve af ","Probe von ","campione di ");
        object.label=p.material_source?std::string(prefix)+p.label:p.label;
        object.aliases={p.label};object.description=quote;object.origin_place=state.current_place_id;
        object.source_quote=quote;object.created_turn=state.hard_state.turn_number;object.portable=true;object.visual_id=p.visual_id;
        next.objects.instances.emplace(object.id,object);carried.push_back(object.id);
        ObjectSource source;source.place_id=state.current_place_id;source.quote=quote;source.label=p.label;
        source.object_id=object.id;source.allowed=true;source.remaining=0;source.reason=p.reason;source.visual_id=p.visual_id;
        source.material_source=p.material_source;
        next.objects.sources.push_back(source);
    } else return fail("invalid_origin");
    ++next.objects.revision;
    if(!ValidateObjectState(next,reason))return false;
    *candidate=std::move(next);if(reason)reason->clear();return true;
}
std::string SerializeObjectState(const ObjectState& state) {return json(state).dump();}
bool ParseObjectState(const std::string& text,ObjectState* state,std::string* error) {
    try {
        auto parsed=json::parse(text).get<ObjectState>();
        if(parsed.version!=1||parsed.revision<0||parsed.next_id<1)throw std::runtime_error("unsupported_object_version");
        *state=std::move(parsed);return true;
    } catch(const std::exception& e){if(error)*error=e.what();return false;}
}
std::string ObjectEvidence(const SessionState& state,const std::string& place) {
    std::string evidence;
    const auto reference=state.reference_descriptions.find(place);
    if(reference!=state.reference_descriptions.end())evidence=reference->second;
    const auto observed=state.observed_descriptions.find(place);
    if(observed!=state.observed_descriptions.end())for(const auto& text:observed->second)evidence+='\n'+text;
    return evidence;
}
bool ObjectNarrationAllowed(const SessionState& state,const std::string& text,
                           const std::string& action,const std::string& id,bool* transfer_mentioned) {
    if(transfer_mentioned)*transfer_mentioned=false;
    const auto normalized=NormalizeObjectText(text);
    if(action=="take"&&std::regex_search(normalized,std::regex(
        R"((quitter vos mains|quitte vos mains|se deposer sur le sol|se depose sur le sol|leave your hands|leaves your hands|settles on the ground))")))return false;
    // Recognize claims, then check each target against the actual authorized
    // transfer. Do not exempt a whole paragraph merely because a transfer exists.
    static const std::regex claims(
        R"(\b(vous (?:ramassez|prenez|reprenez|empoignez|saisissez|recuperez|prelevez)|vous (?:avez|venez de) (?:ramassee?s?|pris|prise|repris|reprise|recuperee?s?|prelevee?s?)|you (?:pick up|take|retrieve|collect|scoop up)|you have (?:picked up|taken|collected)|sie nehmen|du (?:tar opp|plukker opp|samler op)|voi (?:raccogliete|prendete)|raccogliete)\b|\b(vous (?:deposez|posez)|vous (?:avez|venez de) (?:deposee?s?|posee?s?)|you (?:drop|set down|put down)|you have (?:dropped|set down)|sie legen|du (?:legger ned|laegger)|voi posate|posate)\b|\b(vous (?:jetez|recevez|rangez|perdez)|vous (?:avez|venez de) (?:perdu)|you (?:acquire|discard|receive|lose)|you have (?:acquired|lost)|sie (?:werfen|erhalten)|aggiungete all inventario)\b)");
    static const std::regex articles(R"(^(?:(?:le|la|les|l|un|une|du|de|des|votre|vos|ce|cet|cette|the|a|your|den|det|die|das|il|lo|delicatement|doucement|soigneusement|carefully|gently) )*)");
    int approved=0;
    for(std::sregex_iterator it(normalized.begin(),normalized.end(),claims),end;it!=end;++it) {
        const std::string claimed=(*it)[1].matched?"take":(*it)[2].matched?"drop":"other";
        // A relative past clause describes an existing fact, not a new action:
        // "l'échantillon que vous avez déposé repose ici" is true while deposited.
        const auto phrase=it->str();
        auto before=normalized.substr(0,static_cast<size_t>(it->position()));
        bool relative_past=false;
        if(action.empty()&&(phrase.find("vous avez ")==0||phrase.find("you have ")==0)) {
            for(const auto* relative:{" que "," that "," which "}) {
                const std::string suffix=relative;
                if(before.size()>=suffix.size()&&before.compare(before.size()-suffix.size(),suffix.size(),suffix)==0) {
                    before.resize(before.size()-suffix.size());relative_past=true;break;
                }
            }
        }
        if(relative_past) {
            bool attested=false;size_t begin=before.size();
            for(int words=0;words<12;++words) {
                const auto space=begin?before.rfind(' ',begin-1):std::string::npos;
                const auto local=MatchLocalObjects(state,before.substr(space==std::string::npos?0:space+1));
                if(local.size()==1) {
                    if(claimed=="take")attested=Contains(state.hard_state.inventory_items,local[0]);
                    if(claimed=="drop") {
                        const auto room=state.objects.deposits.find(state.current_place_id);
                        if(room!=state.objects.deposits.end())for(const auto& placed:room->second)
                            if(placed.object_id==local[0])attested=true;
                    }
                    if(attested)break;
                }
                if(space==std::string::npos)break;begin=space;
            }
            if(attested)continue;
        }
        if(claimed!=action||id.empty()||++approved>1)return false;
        auto tail=normalized.substr(static_cast<size_t>(it->position()+it->length()));
        if(!tail.empty()&&tail.front()==' ')tail.erase(0,1);
        tail=std::regex_replace(tail,articles,"");
        bool found=false;size_t pos=0;
        for(int words=0;words<12;++words) {
            pos=tail.find(' ',pos);
            const auto target=tail.substr(0,pos);
            const auto local=MatchLocalObjects(state,target);
            if(local.size()==1&&local[0]==id){found=true;break;}
            if(pos==std::string::npos)break;++pos;
        }
        if(!found)return false;
    }
    if(transfer_mentioned)*transfer_mentioned=approved>0;
    return true;
}
void RetainObjectObservation(SessionState* state,const std::string& observation) {
    if(!state||observation.empty())return;
    auto evidence=NormalizeObjectText(ObjectEvidence(*state,state->current_place_id));
    size_t begin=0;
    while(begin<observation.size()) {
        const auto end=observation.find_first_of(".!?\n",begin);
        auto sentence=observation.substr(begin,(end==std::string::npos?observation.size():end+1)-begin);
        const auto first=sentence.find_first_not_of(" \t\r\n");
        if(first!=std::string::npos)sentence.erase(0,first);
        const auto normalized=NormalizeObjectText(sentence);
        bool withdrawn=false;
        for(const auto& source:state->objects.sources)if(source.place_id==state->current_place_id&&
            ObjectTargetGrounded(sentence,source.label))withdrawn=true;
        const auto deposits=state->objects.deposits.find(state->current_place_id);
        if(deposits!=state->objects.deposits.end())for(const auto& placed:deposits->second)
            if(ObjectTargetGrounded(sentence,ObjectName(*state,placed.object_id)))withdrawn=true;
        // Only accepted observational prose enters this store. Questions,
        // hypotheses, mobile deposits and resolved sources cannot create fresh evidence.
        if(normalized.size()>=12&&sentence.size()<=1500&&!UnsupportedSource(sentence)&&
           sentence.back()!='?'&&!withdrawn&&evidence.find(normalized)==std::string::npos) {
            state->observed_descriptions[state->current_place_id].push_back(sentence);
            evidence+=' '+normalized;
        }
        if(end==std::string::npos)break;begin=end+1;
    }
}
std::string ObjectContext(const SessionState& state,const std::string& focus) {
    json carried=json::array(),deposited=json::array(),sources=json::array();
    auto entry=[&](const std::string& id){return json{{"id",id},{"name",ObjectName(state,id)},
        {"portable",state.objects.instances.count(id)&&state.objects.instances.at(id).portable}};};
    for(const auto& id:state.hard_state.inventory_items)carried.push_back(entry(id));
    auto room=state.objects.deposits.find(state.current_place_id);
    if(room!=state.objects.deposits.end())for(const auto& p:room->second) {
        auto object=entry(p.object_id);object["render_prefix"]=ObjectVisualName(p.object_id);deposited.push_back(object);
    }
    for(const auto& s:state.objects.sources)if(s.place_id==state.current_place_id)sources.push_back(json(s));
    const auto reference=state.reference_descriptions.find(state.current_place_id);
    std::vector<std::string> observations;
    const auto retained=state.observed_descriptions.find(state.current_place_id);
    if(retained!=state.observed_descriptions.end()) {
        // The complete evidence stays in state. Bound prompt size, preferentially
        // retrieving requested details even when they are older than the history.
        size_t bytes=0;
        auto add=[&](const std::string& text) {
            if(observations.size()<12&&bytes+text.size()<=1800&&
               std::find(observations.begin(),observations.end(),text)==observations.end()) {
                observations.push_back(text);bytes+=text.size();
            }
        };
        for(auto it=retained->second.rbegin();it!=retained->second.rend();++it)
            if(!focus.empty()&&ObjectTargetGrounded(focus,*it))add(*it);
        for(auto it=retained->second.rbegin();it!=retained->second.rend();++it)add(*it);
    }
    return json{{"revision",state.objects.revision},{"place_id",state.current_place_id},
        {"reference_description",reference==state.reference_descriptions.end()?state.spatial_state.room_summary:reference->second},
        {"observed_details",observations},
        {"carried",carried},{"deposited",deposited},{"resolved_sources",sources}}.dump();
}
std::string ObjectPresenceText(const SessionState& state,bool include_sources) {
    std::string text;auto room=state.objects.deposits.find(state.current_place_id);
    if(room!=state.objects.deposits.end()&&!room->second.empty()) {
        text=Text(state.language,"Set down here: ","Objets déposés ici : ","Lagt ned her: ","Lagt her: ","Hier abgelegt: ","Oggetti posati qui: ");
        for(const auto& p:room->second){if(text.back()!=' ')text+=", ";text+=ObjectName(state,p.object_id);}text+=".";
    }
    for(const auto& source:state.objects.sources)if(include_sources&&source.place_id==state.current_place_id&&!source.object_id.empty()) {
        if(!text.empty())text+=' ';
        text+=Text(state.language,"Already collected from this place: ","Déjà prélevé dans ce lieu : ","Allerede hentet herfra: ","Allerede samlet herfra: ","Hier bereits entnommen: ","Già raccolto in questo luogo: ");
        text+=ObjectName(state,source.object_id)+".";
    }
    return text;
}
std::string ObjectActionText(const SessionState& state,const std::string& action,const std::string& id) {
    return std::string(action=="drop"?Text(state.language,"You set down here: ","Vous déposez ici : ","Du legger ned her: ","Du lægger her: ","Sie legen hier ab: ","Posate qui: "):
        Text(state.language,"You pick up: ","Vous ramassez : ","Du tar opp: ","Du samler op: ","Sie nehmen auf: ","Raccogliete: "))+ObjectName(state,id)+".";
}
std::string ObjectRefusalText(GameLanguage lang,const std::string& reason) {
    if(reason=="already_carried")return Text(lang,"You already carry that item.","Vous portez déjà cet objet.","Du har allerede denne gjenstanden.","Du har allerede denne genstand.","Sie tragen diesen Gegenstand bereits.","Avete già questo oggetto.");
    if(reason=="clarify")return Text(lang,"Which single object do you mean? Please specify.","Quel objet désignez-vous ? Précisez un seul objet.","Hvilken gjenstand mener du? Velg én.","Hvilken genstand mener du? Vælg én.","Welchen einzelnen Gegenstand meinen Sie?", "Quale singolo oggetto intendete?");
    if(reason=="no_placement")return Text(lang,"There is no reachable, visible place to set it down here.","Aucun emplacement accessible et visible ne permet ce dépôt ici.","Her er det ingen synlig plass innen rekkevidde.","Her er ingen synlig plads inden for rækkevidde.","Hier gibt es keinen erreichbaren, sichtbaren Ablageplatz.","Non c'è un punto accessibile e visibile dove posarlo.");
    if(reason=="source_exhausted")return Text(lang,"This source has already been collected from; nothing further is available.","Ce prélèvement a déjà été effectué ; cette source ne fournit plus d'objet.","Denne kilden er allerede brukt; ikke mer er tilgjengelig.","Denne kilde er allerede brugt; der er ikke mere.","Diese Quelle wurde bereits genutzt; nichts Weiteres ist verfügbar.","Questa fonte è già stata raccolta; non offre altri oggetti.");
    if(reason=="fixed_scenery"||reason=="forbidden")return Text(lang,"That element cannot be carried away. Nothing has been moved.","Cet élément ne peut pas être emporté. Rien n'a été déplacé.","Dette kan ikke tas med. Ingenting er flyttet.","Dette kan ikke tages med. Intet er flyttet.","Dieses Element lässt sich nicht mitnehmen. Nichts wurde bewegt.","Questo elemento non può essere portato via. Nulla è stato spostato.");
    if(reason=="unsupported_action")return Text(lang,"Giving away or throwing objects is not available. Nothing has been moved.","Donner ou jeter des objets n'est pas disponible. Rien n'a été déplacé.","Det er ikke mulig å gi bort eller kaste ting. Ingenting er flyttet.","Man kan ikke give eller kaste ting væk. Intet er flyttet.","Verschenken oder Werfen ist nicht verfügbar. Nichts wurde bewegt.","Dare o gettare oggetti non è disponibile. Nulla è stato spostato.");
    return Text(lang,"No available object could be established for that action. Nothing has been moved.","Aucun objet disponible n'a pu être établi pour ce geste. Rien n'a été déplacé.","Ingen tilgjengelig gjenstand ble funnet for handlingen. Ingenting er flyttet.","Ingen tilgængelig genstand blev fundet til handlingen. Intet er flyttet.","Für diese Handlung wurde kein verfügbares Objekt festgestellt. Nichts wurde bewegt.","Non è stato individuato un oggetto disponibile per questo gesto. Nulla è stato spostato.");
}
} // namespace liminal
