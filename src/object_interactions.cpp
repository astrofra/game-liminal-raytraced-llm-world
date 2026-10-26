#include "object_interactions.h"
#include "inventory.h"
#include "nlohmann/json.hpp"
#include <algorithm>
#include <cmath>
#include <cctype>
#include <set>
#include <sstream>

namespace liminal {
using json = nlohmann::json;
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ObjectInstance, id, label, description, aliases, origin_place, source_quote, visual_id, created_turn, portable)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ObjectPlacement, object_id, x, y, z)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ObjectSource, place_id, quote, label, object_id, reason, visual_id, allowed, remaining)
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
bool ForbiddenTarget(const std::string& label) {
    const auto text=NormalizeObjectText(label);
    for(const char* term:{"mur","murs","wall","walls","sol","floor","plafond","ceiling","portal","portail",
        "monumental","structure","porte","door","wand","boden","vegg","gulv","vaeg","parete","pavimento"})
        if(Word(text,term))return true;
    return false;
}
bool UnsupportedSource(const std::string& quote) {
    const auto text=NormalizeObjectText(quote);
    for(const char* term:{"peut etre","imaginaire","hypothetique","pourrait","comme si","aucun","sans","pas de",
        "might","imaginary","hypothetical","could","no","without","kein","nessun","ingen"})
        if(Word(text,term))return true;
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
    const auto query=expand(NormalizeObjectText(command));
    std::istringstream words(expand(NormalizeObjectText(label)));std::string word;
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
        const auto reference=state.reference_descriptions.find(state.current_place_id);
        if(reference==state.reference_descriptions.end()||reference->second.find(p.quote)==std::string::npos)return fail("unattested_source");
        const auto quote=SourceSentence(reference->second,p.quote);
        if(!Word(NormalizeObjectText(quote),NormalizeObjectText(p.label))||UnsupportedSource(quote))return fail("unattested_source");
        if(ForbiddenTarget(p.label))return fail("fixed_scenery");
        for(const auto* anchored:{"scelle","scellee","solidaire","encastre","encastree","attached","fixed","anchored","fest verankert"})
            if(Word(NormalizeObjectText(quote),anchored))return fail("fixed_scenery");
        const auto norm=NormalizeObjectText(quote);
        for(const auto& source:state.objects.sources)if(source.place_id==state.current_place_id&&NormalizeObjectText(source.quote)==norm)
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
        bool matter=false;
        for(const auto* term:{"gravats","rubble","tas","pile","heap","schutt","macerie","grus"})
            if(Word(NormalizeObjectText(p.label),term))matter=true;
        object.label=matter?std::string(Text(state.language,"fragment of ","fragment de ","fragment av ","fragment af ","Fragment von ","frammento di "))+p.label:p.label;
        object.aliases={p.label};object.description=quote;object.origin_place=state.current_place_id;
        object.source_quote=quote;object.created_turn=state.hard_state.turn_number;object.portable=true;object.visual_id=p.visual_id;
        next.objects.instances.emplace(object.id,object);carried.push_back(object.id);
        ObjectSource source;source.place_id=state.current_place_id;source.quote=quote;source.label=p.label;
        source.object_id=object.id;source.allowed=true;source.remaining=0;source.reason=p.reason;source.visual_id=p.visual_id;
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
std::string ObjectContext(const SessionState& state) {
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
    return json{{"revision",state.objects.revision},{"place_id",state.current_place_id},
        {"reference_description",reference==state.reference_descriptions.end()?state.spatial_state.room_summary:reference->second},
        {"carried",carried},{"deposited",deposited},{"resolved_sources",sources}}.dump();
}
std::string ObjectPresenceText(const SessionState& state) {
    std::string text;auto room=state.objects.deposits.find(state.current_place_id);
    if(room!=state.objects.deposits.end()&&!room->second.empty()) {
        text=Text(state.language,"Set down here: ","Objets déposés ici : ","Lagt ned her: ","Lagt her: ","Hier abgelegt: ","Oggetti posati qui: ");
        for(const auto& p:room->second){if(text.back()!=' ')text+=", ";text+=ObjectName(state,p.object_id);}text+=".";
    }
    for(const auto& source:state.objects.sources)if(source.place_id==state.current_place_id&&!source.object_id.empty()) {
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
