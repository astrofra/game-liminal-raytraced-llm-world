#include "ollama_cloud.h"
#include <iostream>
#include <stdexcept>
using namespace liminal;
using json=nlohmann::json;
static void Check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
static CloudTransport Stream(const std::string& content,const std::string& stop="stop",bool terminal=true) {
    return [=](const std::string&,const CloudChunk& chunk,const CloudAlive&,std::string*) {
        std::string bytes=json{{"model","fixture"},{"message",{{"content",content}}},{"done",false}}.dump()+"\n";
        if(terminal)bytes+=json{{"done",true},{"done_reason",stop},{"prompt_eval_count",80},{"eval_count",12}}.dump()+"\n";
        for(char c:bytes)if(!chunk(std::string(1,c)))return false;
        return true;
    };
}
int main() {
    try {
        LlmGenerationConfig c;c.backend="ollama-cloud";c.model_path="missing.gguf";c.n_ctx=32768;
        std::vector<LlmPromptMessage> m={{"user","Bonjour"}};LlmGenerationResult r;
        Check(GenerateOllamaCompletion(c,m,nullptr,nullptr,&r,Stream(u8"La lumière frémit.")),"split UTF8 stream");
        Check(r.response_text==u8"La lumière frémit."&&r.usage_reported&&r.generated_tokens==12,"content and usage");
        for(const auto& ending:{"length","unknown",""})
            Check(!GenerateOllamaCompletion(c,m,nullptr,nullptr,&r,Stream("text",ending)),"truncation rejected");
        Check(!GenerateOllamaCompletion(c,m,nullptr,nullptr,&r,Stream("text","stop",false)),"missing done rejected");
        Check(!GenerateOllamaCompletion(c,m,nullptr,nullptr,&r,Stream("<think>private</think>")),"private channel rejected");
        auto cancel=[](const char*,const char*,void*){return false;};
        Check(!GenerateOllamaCompletion(c,m,cancel,nullptr,&r,Stream("text"))&&r.stop_reason=="cancelled","cancel before send");
        for(const std::string code:{"http_401","http_403","http_429","http_500","interrupted_stream"}) {
            Check(!GenerateOllamaCompletion(c,m,nullptr,nullptr,&r,[&](const auto&,const auto&,const auto&,std::string* e){*e=code;return false;}),"HTTP failure");
            Check(r.stop_reason==code&&r.response_text.empty(),"HTTP mapping");
        }
        c.use_json_grammar=true;
        c.json_schema=R"({"type":"object","properties":{"reply":{"type":"string","minLength":2,"maxLength":8},"n":{"type":"integer","minimum":1,"maximum":3},"id":{"enum":["known"]}},"required":["reply","n","id"],"additionalProperties":false})";
        auto schema=json::parse(c.json_schema);std::string error;
        const json valid={{"reply",u8"été"},{"n",2},{"id","known"}};
        Check(ValidateCloudJson(valid,schema,&error),"valid schema unicode length");
        for(const json bad:{json{{"reply","ok"},{"n",2},{"id","unknown"}},json{{"reply","ok"},{"n",2.5},{"id","known"}},json{{"reply","ok"},{"n",8},{"id","known"}},json{{"reply","ok"},{"n",2},{"id","known"},{"extra",0}},json{{"n",2},{"id","known"}}})
            Check(!ValidateCloudJson(bad,schema,&error),"bad schema rejected");
        Check(!ValidateCloudJson("x",json{{"type","string"},{"unknownKeyword",true}},&error),"unknown schema fails closed");
        Check(!ValidateCloudJson(2,json{{"anyOf",json::array({json{{"const",1}},json{{"const",3}}})}},&error),"anyOf enforced");
        const json exits={{"north",true},{"east",true},{"west",true}};
        const json exit_schema={{"anyOf",json::array({
            json{{"properties",{{"north",{{"enum",json::array({false})}}}}}},
            json{{"properties",{{"east",{{"enum",json::array({false})}}}}}}
        })}};
        Check(!ValidateCloudJson(exits,exit_schema,&error)&&error.find("$.north: value outside enum [false]")!=std::string::npos&&
            error.find("$.east: value outside enum [false]")!=std::string::npos,"repair explains each rejected exit alternative");
        Check(ValidateCloudJson(json{{"north",true},{"east",false},{"west",true}},exit_schema,&error),"valid alternative still accepted");
        Check(!ValidateCloudJson("UPPER",json{{"pattern","^[a-z]+$"}},&error),"pattern enforced");
        Check(!ValidateCloudJson(json::array({1,1}),json{{"type","array"},{"uniqueItems",true}},&error),"unique items");
        int calls=0,visible=0,traces=0;
        c.research_trace=[&](bool,const std::string& data,const std::string&){++traces;Check(data.find("Authorization")==std::string::npos,"no auth trace");};
        auto repaired=[&](const std::string& body,const CloudChunk& chunk,const CloudAlive& alive,std::string* e){
            auto payload=json::parse(body);Check(!payload.contains("format")&&!payload["think"].get<bool>(),"no unsupported format");
            if(calls==1) {
                Check(payload["options"]["temperature"].get<float>()<=0.2f,"repair sampling bounded");
                const auto& messages=payload["messages"];
                Check(messages[messages.size()-2]["role"]=="assistant"&&messages[messages.size()-2]["content"]=="{}","repair includes rejected candidate");
            }
            ++calls;return Stream(calls==1?"{}":valid.dump())(body,chunk,alive,e);
        };
        Check(GenerateOllamaCompletion(c,m,[](const char*,const char* delta,void* p){if(*delta)++*static_cast<int*>(p);return true;},&visible,&r,repaired),"one repair succeeds");
        Check(calls==2&&visible==1&&traces==4&&r.generated_tokens==24,"repair hidden and both attempts traced");
        calls=0;
        Check(!GenerateOllamaCompletion(c,m,nullptr,nullptr,&r,[&](const auto& body,const auto& chunk,const auto& alive,std::string* e){++calls;return Stream("{}")(body,chunk,alive,e);})&&calls==2,"at most one repair");
        for(const std::string fence:{"```json\n","```\r\n"}) {
            const std::string raw=fence+valid.dump()+"\n```\n";
            Check(GenerateOllamaCompletion(c,m,nullptr,nullptr,&r,Stream(raw))&&r.cloud_attempts==1,"whole JSON fence accepted without repair");
            Check(json::parse(r.response_text)==valid&&r.raw_text.find("```")!=std::string::npos,"fence stripped only from validated final, raw retained");
        }
        for(const auto& bad:{std::string("```json\n{}\n```"), "Here is the JSON:\n"+valid.dump(), "```json\n"+valid.dump()+"\n```\nCommentary"})
            Check(!GenerateOllamaCompletion(c,m,nullptr,nullptr,&r,Stream(bad)),"fence never bypasses schema or extracts chatter");
        c.temperature=5.0f;c.cloud_optional_prose=true;
        calls=0;
        Check(GenerateOllamaCompletion(c,m,nullptr,nullptr,&r,[&](const auto& body,const auto& chunk,const auto& alive,std::string* e){
            const auto payload=json::parse(body);const auto temp=payload["options"]["temperature"].template get<float>();
            Check(temp==(calls?0.2f:1.0f),"optional prose wire temperature capped, including repair");
            ++calls;return Stream(calls==1?"{}":valid.dump())(body,chunk,alive,e);
        })&&c.temperature==5.0f,"thermal request unchanged");
        for(const bool optional:{false,true}) {
            c.cloud_optional_prose=optional;
            for(int failure=0;failure<3;++failure) {
                c.cloud_budget=std::make_shared<LlmCloudBudget>();
                const auto transport=failure==0?Stream("{}"):failure==1?Stream(valid.dump(),"length"):Stream("<think>private</think>");
                Check(!GenerateOllamaCompletion(c,m,nullptr,nullptr,&r,transport),"invalid content rejected");
                Check(c.cloud_budget->failed==!optional&&r.response_text.empty(),"only optional content failure may retain prepared prose");
            }
        }
        for(const std::string code:{"http_401","http_429","http_500","interrupted_stream"}) {
            c.cloud_budget=std::make_shared<LlmCloudBudget>();
            Check(!GenerateOllamaCompletion(c,m,nullptr,nullptr,&r,[&](const auto&,const auto&,const auto&,std::string* e){*e=code;return false;})&&c.cloud_budget->failed,"optional transport failure is fatal");
        }
        c.cloud_budget=std::make_shared<LlmCloudBudget>();
        Check(!GenerateOllamaCompletion(c,m,cancel,nullptr,&r,Stream(valid.dump()))&&c.cloud_budget->failed,"optional cancellation remains fatal");
        c.cloud_budget=std::make_shared<LlmCloudBudget>();c.cloud_budget->remaining_calls=0;calls=0;
        Check(!GenerateOllamaCompletion(c,m,nullptr,nullptr,&r,repaired)&&calls==0,"interaction call cap");
        c.cloud_budget=std::make_shared<LlmCloudBudget>();c.cloud_budget->deadline=std::chrono::steady_clock::now();
        Check(!GenerateOllamaCompletion(c,m,nullptr,nullptr,&r,repaired)&&r.stop_reason=="timeout"&&c.cloud_budget->failed,"optional deadline remains fatal");
        c.cloud_budget.reset();c.n_ctx=128;
        Check(!GenerateOllamaCompletion(c,m,nullptr,nullptr,&r,repaired)&&r.stop_reason=="context_estimate_limit","context cap");
        c.n_ctx=32768;c.use_json_grammar=false;c.research_trace={};
        Check(!GenerateOllamaCompletion(c,m,nullptr,nullptr,&r,[](const auto&,const auto& chunk,const auto&,std::string*){return chunk(std::string(2*1024*1024+1,'x'));}),"body cap");
        Check(!GenerateOllamaCompletion(c,m,nullptr,nullptr,&r,[](const auto&,const auto& chunk,const auto&,std::string*){return chunk("not json\n");}),"malformed record");
        traces=0;c.research_trace=[&](bool,const auto&,const auto&){++traces;};
        Check(!GenerateOllamaCompletion(c,m,nullptr,nullptr,&r,[](const auto&,const auto& chunk,const auto&,std::string*){return chunk("{\"message\":{\"content\":42}}\n");})&&traces==2,"malformed type retains failure trace");
        int count=0;Check(CountChatPromptTokens(c,m,nullptr,nullptr,&count,&error)&&count>0,"cloud counting without GGUF");
        std::cout<<"Ollama cloud protocol, schema, budgets and repair checks passed\n";
        return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}
}
