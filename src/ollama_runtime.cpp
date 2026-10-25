#include "ollama_runtime.h"
#include "cpp-httplib/httplib.h"
#include "nlohmann/json.hpp"
#include <atomic>
#include <chrono>
#include <cmath>
#include <mutex>
#include <thread>

namespace liminal {
bool GenerateOllamaCompletion(const LlmGenerationConfig& config,const std::vector<LlmPromptMessage>& messages,
                             LlmStreamCallback callback,void* user,LlmGenerationResult* result) {
    if(!result)return false;
    *result=LlmGenerationResult();
    using json=nlohmann::json;
    using Clock=std::chrono::steady_clock;
    const auto deadline=Clock::now()+std::chrono::milliseconds(config.timeout_ms);
    auto fail=[&](const char* reason,const std::string& error) {
        result->success=false;result->response_text.clear();result->stop_reason=reason;result->error_message=error;return false;
    };
    if(messages.empty()||config.timeout_ms<=0||config.n_ctx<128||config.n_ctx>32768||
       config.n_predict<1||config.n_predict>16384||!std::isfinite(config.temperature)||config.temperature<0||
       config.reasoning||config.ollama_port<1||config.ollama_port>65535||config.ollama_model.empty()||config.ollama_model.find("cloud")!=std::string::npos)
        return fail("error","Invalid local Ollama configuration");
    json request={{"model",config.ollama_model},{"stream",true},{"keep_alive","10m"},
        {"messages",json::array()},{"options",{{"num_ctx",config.n_ctx},{"num_predict",config.n_predict},
            {"temperature",config.temperature},{"seed",config.seed},{"top_k",config.top_k},{"top_p",config.top_p}}}};
    for(const auto& message:messages)request["messages"].push_back({{"role",message.role},{"content",message.content}});
    if(config.use_json_grammar) {
        if(config.json_schema.empty())request["format"]="json";
        else {
            const auto schema=json::parse(config.json_schema,nullptr,false);
            if(schema.is_discarded())return fail("error","Invalid Ollama JSON schema");
            request["format"]=schema;
        }
    }
    result->prompt_text=request["messages"].dump();
    httplib::Client client("127.0.0.1",config.ollama_port);
    client.set_connection_timeout(std::chrono::milliseconds(std::min(config.timeout_ms,3000)));
    client.set_read_timeout(std::chrono::milliseconds(config.timeout_ms));
    client.set_write_timeout(std::chrono::milliseconds(config.timeout_ms));
    client.set_max_timeout(std::chrono::milliseconds(config.timeout_ms));
    std::mutex mutex;
    std::string pending,error;
    size_t received=0;
    std::atomic<bool> done{false},abort_requested{false};
    int status=0;
    // Only this worker touches HTTP. All caller callbacks execute on the calling
    // thread, including liveness polls during loading and prompt evaluation.
    std::thread worker([&] {
        try {
            auto show=client.Post("/api/show",json({{"model",config.ollama_model}}).dump(),"application/json");
            if(abort_requested.load()){done.store(true);return;}
            if(!show||show->status!=200)throw std::runtime_error("Local Ollama model is unavailable");
            const auto info=json::parse(show->body);
            if(info.contains("remote_host")||info.contains("remote_model"))throw std::runtime_error("Cloud inference is disabled for the game");
            bool supports_thinking=false;
            if(info.contains("capabilities"))for(const auto& capability:info["capabilities"])
                if(capability=="thinking")supports_thinking=true;
            if(supports_thinking)request["think"]=config.reasoning;
            else if(config.reasoning)throw std::runtime_error("This Ollama model has no thinking mode");
            auto response=client.Post("/api/chat",httplib::Headers{},request.dump(),"application/json",
                [&](const char* data,size_t size) {
                    std::lock_guard<std::mutex> lock(mutex);
                    received+=size;
                    if(received>2*1024*1024)return false;
                    pending.append(data,size);return true;
                });
            if(response)status=response->status;
            else error=std::string("Ollama transport: ")+httplib::to_string(response.error());
        } catch(const std::exception& e) {error=e.what();}
        done.store(true);
    });
    // Joining is mandatory before any return, including malformed streams and
    // exceptions: neither HTTP nor callbacks can outlive the caller's state.
    struct Join {
        httplib::Client& client;std::thread& worker;std::atomic<bool>& done;std::atomic<bool>& abort;
        ~Join(){
            abort.store(true);
            // Stop again if cancellation raced the transition from /show to /chat.
            while(!done.load()){client.stop();std::this_thread::sleep_for(std::chrono::milliseconds(10));}
            if(worker.joinable())worker.join();
        }
    } join{client,worker,done,abort_requested};
    std::string buffer,final_text;
    json completion;
    bool saw_done=false;
    try {
        for(;;) {
            if(Clock::now()>=deadline)return fail("timeout","Ollama call exceeded its shared deadline");
            if(callback&&!callback(final_text.c_str(),"",user))return fail("cancelled","Ollama call cancelled");
            const bool finished=done.load();
            {std::lock_guard<std::mutex> lock(mutex);buffer+=pending;pending.clear();}
            size_t newline;
            while((newline=buffer.find('\n'))!=std::string::npos) {
                const auto line=buffer.substr(0,newline);buffer.erase(0,newline+1);
                if(line.empty()||line=="\r")continue;
                if(saw_done)return fail("protocol_error","Data after Ollama completion");
                const auto event=json::parse(line);
                if(event.contains("error"))return fail("error",event["error"].get<std::string>());
                const auto message=event.value("message",json::object());
                const std::string delta=message.value("content",std::string());
                final_text+=delta;
                if(callback&&!delta.empty()&&!callback(final_text.c_str(),delta.c_str(),user))
                    return fail("cancelled","Ollama call cancelled");
                if(event.value("done",false)){completion=event;saw_done=true;}
            }
            if(finished)break;
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        worker.join();
        if(status!=200)return fail("error",error.empty()?"Ollama returned HTTP "+std::to_string(status):error);
        if(!saw_done||buffer.find_first_not_of(" \r\n\t")!=std::string::npos)
            return fail("protocol_error","Incomplete Ollama stream");
        result->prompt_tokens=completion.value("prompt_eval_count",0);
        result->generated_tokens=completion.value("eval_count",0);
        result->final_tokens=result->generated_tokens; // Chosen local profile disables thinking.
        result->load_time_ms=completion.value("load_duration",0.0)/1e6;
        result->prompt_time_ms=completion.value("prompt_eval_duration",0.0)/1e6;
        result->generation_time_ms=completion.value("eval_duration",0.0)/1e6;
        result->inference_time_ms=result->prompt_time_ms+result->generation_time_ms;
        result->reached_eog=completion.value("done_reason",std::string())=="stop";
        result->raw_text=final_text;
        if(!result->reached_eog)return fail("token_limit","Ollama generation did not finish with stop");
        if(final_text.empty())return fail("protocol_error","Empty final Ollama response");
        if(config.use_json_grammar) {
            const auto parsed=json::parse(final_text,nullptr,false);
            if(parsed.is_discarded()||!parsed.is_object())return fail("invalid_final_json","Ollama final output is not a JSON object");
        }
        if(Clock::now()>=deadline)return fail("timeout","Ollama call exceeded its shared deadline");
        result->response_text=final_text;result->stop_reason="eos";result->success=true;return true;
    } catch(const std::exception& e) {return fail("protocol_error",e.what());}
}
} // namespace liminal
