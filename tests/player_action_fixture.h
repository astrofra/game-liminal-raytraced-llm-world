#pragma once
#include "object_interactions.h"
#include "inventory.h"
#include "turn_runner.h"
#include "nlohmann/json.hpp"

// Deterministic model answers for the existing engine/scene suites. This helper
// is test-only: free player language is interpreted by Gemma in the application.
inline nlohmann::json FixtureWireAction(nlohmann::json proposal,const nlohmann::json& context) {
    int source=0;
    if(proposal.value("origin","")=="implicit") {
        source=999;const auto& quotes=context["eligible_source_sentences"];
        for(size_t i=0;i<quotes.size();++i)if(quotes[i]==proposal["quote"])source=static_cast<int>(i+1);
    }
    if(proposal.value("verdict","")=="forbid"&&proposal.value("action","")=="take")proposal["action"]="refuse";
    proposal.erase("origin");proposal.erase("quote");proposal.erase("verdict");proposal["source"]=source;return proposal;
}
inline nlohmann::json FixturePlayerAction(const nlohmann::json& context,
    const nlohmann::json& literary=nullptr) {
    using namespace liminal;using json=nlohmann::json;
    const auto command=NormalizeObjectText(context.at("command").get<std::string>());
    const auto request=ParseInventoryRequest(command);
    json response={{"action","perceive"},{"origin","none"},{"object_id",""},{"quote",""},{"label",""},
        {"visual_id",""},{"verdict","allow"},{"durable",false},{"material_source",false},{"reason",""},{"revision",context["revision"]}};
    SessionState local;local.current_place_id="fixture";local.language=kGameLanguageFrench;
    for(const auto* kind:{"carried","deposited"})for(const auto& entry:context[kind]) {
        const auto id=entry["id"].get<std::string>();ObjectInstance object;object.id=id;object.label=entry["name"];
        local.objects.instances[id]=object;
        if(std::string(kind)=="carried")local.hard_state.inventory_items.push_back(id);
        else {ObjectPlacement placement;placement.object_id=id;local.objects.deposits[local.current_place_id].push_back(placement);}
    }
    const auto matches=MatchLocalObjects(local,request.target);
    if(request.command==InventoryCommand::List)response["action"]="inventory";
    else if(request.command==InventoryCommand::Ambiguous||matches.size()>1) {
        response["action"]="clarify";response["verdict"]="uncertain";
    } else if(request.command==InventoryCommand::Unsupported) {
        response["action"]="none";response["verdict"]="forbid";
    } else if(request.command==InventoryCommand::Take||request.command==InventoryCommand::Drop||
              request.command==InventoryCommand::Inspect||request.command==InventoryCommand::Use) {
        if(matches.size()==1) {
            response["action"]=request.command==InventoryCommand::Take?"take":request.command==InventoryCommand::Drop?"drop":request.command==InventoryCommand::Inspect?"inspect":"use";
            response["origin"]="explicit";response["object_id"]=matches[0];
        } else if((request.command==InventoryCommand::Take||request.command==InventoryCommand::Drop)&&literary.is_object())response=literary;
        else if(request.command==InventoryCommand::Take||request.command==InventoryCommand::Drop||!request.item_id.empty()) {
            response["action"]="none";response["verdict"]="forbid";
        }
    } else if(command.find("noter ")==0)response["action"]="note";
    else if(command.find("mesurer ")==0)response["action"]="measure";
    else if(command=="lire le carnet")response["action"]="read_notes";
    else if(literary.is_object()&&MayRequestObjectTransfer(command))response=literary;
    response["revision"]=context["revision"];return response;
}
inline bool FixturePlayerCompletion(const liminal::LlmGenerationConfig&,
    const std::vector<liminal::LlmPromptMessage>& messages,liminal::LlmStreamCallback,void*,liminal::LlmGenerationResult* result) {
    result->success=true;result->reached_eog=true;
    const auto context=nlohmann::json::parse(messages.at(1).content);
    result->response_text=FixtureWireAction(FixturePlayerAction(context),context).dump();return true;
}
