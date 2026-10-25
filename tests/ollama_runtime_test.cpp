#include "ollama_runtime.h"
#include "cpp-httplib/httplib.h"
#include "nlohmann/json.hpp"
#include <atomic>
#include <cstdio>
#include <stdexcept>
#include <thread>
using namespace liminal;
using json=nlohmann::json;
#define CHECK(c) do {if(!(c))throw std::runtime_error(std::string("Check failed: ")+#c);}while(false)
int main() {
    httplib::Server server;
    std::atomic<int> mode{0},calls{0};
    server.Post("/api/show",[&](const httplib::Request&,httplib::Response& r){
        if(mode==6)r.set_content(R"({"remote_host":"https://remote.invalid"})","application/json");
        else r.set_content(R"({"capabilities":["completion","thinking"]})","application/json");
    });
    server.Post("/api/chat",[&](const httplib::Request& req,httplib::Response& r){
        ++calls;const auto input=json::parse(req.body);
        if(input.at("messages").size()!=1||input.at("think")!=false||input.at("keep_alive")!="10m") {r.status=400;return;}
        if(mode==1) {
            r.set_chunked_content_provider("application/x-ndjson",[](size_t,httplib::DataSink& sink){
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
                const std::string partial="{\"message\":{\"content\":\"x\"},\"done\":false}\n";
                return sink.write(partial.data(),partial.size());
            });return;
        }
        if(mode==2){r.set_content("{bad}\n","application/x-ndjson");return;}
        if(mode==3){r.status=503;return;}
        if(mode==4){r.set_content("{\"done\":false}\n","application/x-ndjson");return;}
        const auto final=json({{"done",true},{"done_reason",mode==5?"length":"stop"},{"prompt_eval_count",12},
            {"eval_count",2},{"load_duration",1000000},{"prompt_eval_duration",2000000},{"eval_duration",3000000},
            {"message",{{"content","hello"},{"thinking","must not reach the callback"}}}}).dump()+"\n";
        r.set_content(final,"application/x-ndjson");
    });
    const int port=server.bind_to_any_port("127.0.0.1");
    if(port<=0)return 1;
    std::thread listener([&]{server.listen_after_bind();});
    try {
        LlmGenerationConfig cfg;cfg.use_ollama=true;cfg.ollama_port=port;cfg.timeout_ms=2000;
        LlmGenerationResult result;
        const std::vector<LlmPromptMessage> messages={{"user","hello"}};
        const auto thread=std::this_thread::get_id();
        struct State {std::thread::id caller;int polls=0;bool valid=true;} state{thread};
        auto callback=[](const char* text,const char*,void* data){
            auto& s=*static_cast<State*>(data);++s.polls;
            s.valid=s.valid&&std::this_thread::get_id()==s.caller&&std::string(text).find("must not")==std::string::npos;
            return true;
        };
        CHECK(GenerateOllamaCompletion(cfg,messages,callback,&state,&result));
        CHECK(result.response_text=="hello"&&result.load_time_ms==1&&result.prompt_time_ms==2&&result.generation_time_ms==3);
        CHECK(state.valid&&state.polls>0);
        CHECK(GenerateOllamaCompletion(cfg,messages,nullptr,nullptr,&result));CHECK(calls==2);
        for(int rejected:{2,3,4,5,6}) {
            mode=rejected;CHECK(!GenerateOllamaCompletion(cfg,messages,nullptr,nullptr,&result));CHECK(result.response_text.empty());
        }
        mode=1;cfg.timeout_ms=120;
        auto start=std::chrono::steady_clock::now();
        CHECK(!GenerateOllamaCompletion(cfg,messages,nullptr,nullptr,&result));CHECK(result.stop_reason=="timeout");
        CHECK(std::chrono::steady_clock::now()-start<std::chrono::seconds(1));
        cfg.timeout_ms=2000;int polls=0;
        auto cancel=[](const char*,const char*,void* data){return ++*static_cast<int*>(data)<5;};
        start=std::chrono::steady_clock::now();
        CHECK(!GenerateOllamaCompletion(cfg,messages,cancel,&polls,&result));CHECK(result.stop_reason=="cancelled");
        CHECK(std::chrono::steady_clock::now()-start<std::chrono::seconds(1));
        server.stop();listener.join();
        std::puts("Ollama loopback: independent requests, stream, metrics, errors, deadline and cancellation passed");return 0;
    } catch(const std::exception& e) {server.stop();listener.join();std::fprintf(stderr,"%s\n",e.what());return 1;}
}
