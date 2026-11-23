#include "ollama_cloud.h"
#include "llm_channels.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <mutex>
#include <condition_variable>
#include <regex>
#include <set>
#include <stdexcept>
#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>
#include <winhttp.h>
#endif

namespace liminal {
namespace {
using json = nlohmann::json;
using Clock = std::chrono::steady_clock;
constexpr size_t kBodyLimit = 2 * 1024 * 1024;
double Ms(Clock::time_point start) { return std::chrono::duration<double,std::milli>(Clock::now()-start).count(); }

// Accept only a whole Markdown envelope, never fish JSON out of commentary.
// The unwrapped candidate must still pass the complete schema below.
bool UnwrapJsonFence(std::string* content) {
    const auto first=content->find_first_not_of(" \r\n\t");
    if(first==std::string::npos)return false;
    const auto last=content->find_last_not_of(" \r\n\t");
    const auto newline=content->find('\n',first);
    if(newline==std::string::npos||last<first+6)return false;
    auto opening=content->substr(first,newline-first);
    if(!opening.empty()&&opening.back()=='\r')opening.pop_back();
    if((opening!="```json"&&opening!="```")||content->substr(last-2,3)!="```")return false;
    const auto closing=content->rfind('\n',last-3);
    if(closing==std::string::npos||closing<=newline||
        content->substr(closing+1,last-closing)!="```")return false;
    *content=content->substr(newline+1,closing-newline-1);
    return true;
}

void Validate(const json& v, const json& s, const std::string& path, int depth) {
    auto require = [&](bool ok, const std::string& why) { if (!ok) throw std::runtime_error(path + ": " + why); };
    require(depth < 48, "schema depth exceeded");
    if (s.is_boolean()) { require(s.get<bool>(), "disallowed value"); return; }
    require(s.is_object(), "invalid schema");
    static const std::set<std::string> supported = {"type","properties","required","additionalProperties",
        "items","minItems","maxItems","uniqueItems","minLength","maxLength","pattern","minimum","maximum",
        "enum","const","anyOf","oneOf","allOf","description","title","$schema"};
    for (auto it=s.begin(); it!=s.end(); ++it) require(supported.count(it.key())!=0,"unsupported schema keyword " + it.key());
    if (s.contains("type")) {
        const auto t=s.at("type").get<std::string>();
        require((t=="object"&&v.is_object())||(t=="array"&&v.is_array())||(t=="string"&&v.is_string())||
            (t=="boolean"&&v.is_boolean())||(t=="number"&&v.is_number())||(t=="integer"&&v.is_number_integer())||
            (t=="null"&&v.is_null()),"expected " + t);
    }
    if (s.contains("enum")) require(std::find(s["enum"].begin(),s["enum"].end(),v)!=s["enum"].end(),"value outside enum " + s["enum"].dump().substr(0,512));
    if (s.contains("const")) require(v==s["const"],"constant differs");
    for (const char* key : {"anyOf","oneOf","allOf"}) if (s.contains(key)) {
        int matches=0;std::string failures;
        for (const auto& branch:s[key]) {
            try { Validate(v,branch,path,depth+1); ++matches; }
            catch (const std::exception& e) {
                if(failures.size()<2048)failures+=" ["+std::string(e.what()).substr(0,512)+"]";
            }
        }
        require(std::string(key)=="anyOf" ? matches>0 : std::string(key)=="oneOf" ? matches==1 : matches==s[key].size(),
            std::string(key)+" mismatch; branch failures:"+failures);
    }
    if (v.is_object()) {
        if (s.contains("required")) for (const auto& key:s["required"]) require(v.contains(key.get<std::string>()),"missing " + key.get<std::string>());
        const auto props=s.value("properties",json::object());
        for (auto it=v.begin();it!=v.end();++it) {
            if (props.contains(it.key())) Validate(it.value(),props[it.key()],path+"."+it.key(),depth+1);
            else if (s.contains("additionalProperties")) Validate(it.value(),s["additionalProperties"],path+"."+it.key(),depth+1);
        }
    }
    if (v.is_array()) {
        require(v.size()>=s.value("minItems",size_t(0))&&v.size()<=s.value("maxItems",kBodyLimit),"array length");
        for (size_t i=0;i<v.size();++i) {
            if (s.contains("items")) Validate(v[i],s["items"],path+"["+std::to_string(i)+"]",depth+1);
            if (s.value("uniqueItems",false)) for(size_t j=0;j<i;++j) require(v[i]!=v[j],"duplicate item");
        }
    }
    if (v.is_string()) {
        const auto& str=v.get_ref<const std::string&>();
        size_t length=0; for(unsigned char c:str) if((c&0xc0)!=0x80) ++length;
        require(length>=s.value("minLength",size_t(0))&&length<=s.value("maxLength",kBodyLimit),"string length");
        if (s.contains("pattern")) require(std::regex_search(str,std::regex(s["pattern"].get<std::string>())),"pattern mismatch");
    }
    if (v.is_number()) {
        const double n=v.get<double>();require(std::isfinite(n),"non-finite number");
        if(s.contains("minimum")) require(n>=s["minimum"].get<double>(),"below minimum");
        if(s.contains("maximum")) require(n<=s["maximum"].get<double>(),"above maximum");
    }
}

std::vector<LlmPromptMessage> CloudMessages(const LlmGenerationConfig& c, const std::vector<LlmPromptMessage>& input) {
    auto messages=input;
    if(c.use_json_grammar) {
        const std::string instruction="\nReturn exactly one JSON object, without Markdown, commentary or thinking. "
            "All required fields, types, constants, enums and bounds in this JSON Schema are binding:\n"+
            (c.json_schema.empty()?"{\"type\":\"object\"}":c.json_schema);
        if(!messages.empty()&&messages.front().role=="system") messages.front().content+=instruction;
        else messages.insert(messages.begin(),{"system",instruction});
    }
    return messages;
}

#if defined(_WIN32)
struct HttpCompletion {
    std::mutex mutex;
    std::condition_variable changed;
    bool ready=false, closed=false;
    DWORD status=0, bytes=0;
};
void CALLBACK HttpStatus(HINTERNET, DWORD_PTR context, DWORD status, LPVOID, DWORD bytes) {
    if(!context)return;
    auto& state=*reinterpret_cast<HttpCompletion*>(context);
    std::lock_guard<std::mutex> lock(state.mutex);
    if(status==WINHTTP_CALLBACK_STATUS_HANDLE_CLOSING)state.closed=true;
    else if(status==WINHTTP_CALLBACK_STATUS_SENDREQUEST_COMPLETE || status==WINHTTP_CALLBACK_STATUS_HEADERS_AVAILABLE ||
            status==WINHTTP_CALLBACK_STATUS_READ_COMPLETE || status==WINHTTP_CALLBACK_STATUS_REQUEST_ERROR) {
        state.ready=true;state.status=status;state.bytes=bytes;
    }
    state.changed.notify_all();
}
#endif

// Asynchronous WinHTTP operations are polled by the inference thread. SDL remains
// independent; cancellation closes a pending async request. Context and buffers
// survive until HANDLE_CLOSING, including callbacks that race with cancellation.
bool Https(const std::string& body,const CloudChunk& consume,const CloudAlive& alive,std::string* error) {
#if defined(_WIN32)
    const char* env=std::getenv("OLLAMA_API_KEY");
    if(!env||!*env) { *error="missing_api_key";return false; }
    const std::string key=env;
    struct Handle { HINTERNET h=nullptr; ~Handle(){if(h)WinHttpCloseHandle(h);} } session, connection;
    session.h=WinHttpOpen(L"WithinTheLatentWalls/1",WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,nullptr,nullptr,WINHTTP_FLAG_ASYNC);
    if(!session.h) { *error="http_session";return false; }
    WinHttpSetTimeouts(session.h,5000,5000,5000,15000);
    connection.h=WinHttpConnect(session.h,L"ollama.com",INTERNET_DEFAULT_HTTPS_PORT,0);
    if(!connection.h) { *error="http_connect";return false; }
    HttpCompletion state;
    char buffer[8192]; // Must outlive request teardown, even when a read is cancelled.
    struct Request {
        HINTERNET h=nullptr;HttpCompletion& state;bool notifications=false;
        ~Request() {
            if(h)WinHttpCloseHandle(h);
            if(notifications) {
                std::unique_lock<std::mutex> lock(state.mutex);
                state.changed.wait(lock,[&]{return state.closed;});
            }
        }
    } request{nullptr,state};
    request.h=WinHttpOpenRequest(connection.h,L"POST",L"/api/chat",nullptr,WINHTTP_NO_REFERER,WINHTTP_DEFAULT_ACCEPT_TYPES,WINHTTP_FLAG_SECURE);
    if(!request.h) { *error="http_request";return false; }
    DWORD redirect=WINHTTP_OPTION_REDIRECT_POLICY_NEVER;
    DWORD_PTR context=reinterpret_cast<DWORD_PTR>(&state);
    if(!WinHttpSetOption(request.h,WINHTTP_OPTION_REDIRECT_POLICY,&redirect,sizeof(redirect)) ||
       !WinHttpSetOption(request.h,WINHTTP_OPTION_CONTEXT_VALUE,&context,sizeof(context))) {
        *error="http_options";return false;
    }
    if(WinHttpSetStatusCallback(request.h,HttpStatus,WINHTTP_CALLBACK_FLAG_ALL_COMPLETIONS|WINHTTP_CALLBACK_FLAG_HANDLES,0)==WINHTTP_INVALID_STATUS_CALLBACK) {
        *error="http_callback";return false;
    }
    request.notifications=true;
    std::wstring headers=L"Content-Type: application/json\r\nAuthorization: Bearer ";
    headers.append(key.begin(),key.end());headers+=L"\r\n";
    auto begin=[&]{std::lock_guard<std::mutex> lock(state.mutex);state.ready=false;state.status=0;state.bytes=0;};
    auto wait=[&](BOOL issued,DWORD expected) {
        if(!issued){*error="http_io";return false;}
        while(true) {
            if(!alive())return false;
            std::unique_lock<std::mutex> lock(state.mutex);
            if(state.ready) {
                if(state.status!=expected){*error="http_io";return false;}
                return true;
            }
            state.changed.wait_for(lock,std::chrono::milliseconds(20));
        }
    };
    begin();
    if(!wait(WinHttpSendRequest(request.h,headers.c_str(),static_cast<DWORD>(headers.size()),
        const_cast<char*>(body.data()),static_cast<DWORD>(body.size()),static_cast<DWORD>(body.size()),context),
        WINHTTP_CALLBACK_STATUS_SENDREQUEST_COMPLETE))return false;
    begin();
    if(!wait(WinHttpReceiveResponse(request.h,nullptr),WINHTTP_CALLBACK_STATUS_HEADERS_AVAILABLE))return false;
    DWORD status=0,size=sizeof(status);
    if(!WinHttpQueryHeaders(request.h,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,nullptr,&status,&size,nullptr)) {
        *error="http_status";return false;
    }
    if(status!=200){*error="http_"+std::to_string(status);return false;}
    size_t total=0;
    while(true) {
        begin();
        if(!wait(WinHttpReadData(request.h,buffer,sizeof(buffer),nullptr),WINHTTP_CALLBACK_STATUS_READ_COMPLETE))return false;
        DWORD got;{std::lock_guard<std::mutex> lock(state.mutex);got=state.bytes;}
        if(!got)return true;
        total+=got;if(total>kBodyLimit){*error="response_too_large";return false;}
        if(!consume(std::string(buffer,got)))return false;
    }
#else
    (void)body;(void)consume;(void)alive;*error="cloud_requires_windows";return false;
#endif
}
}

bool ValidateCloudJson(const json& value,const json& schema,std::string* error) {
    try {Validate(value,schema,"$",0);return true;}
    catch(const std::exception& e){if(error)*error=e.what();return false;}
}
bool CheckOllamaConfiguration(const LlmGenerationConfig& c,std::string* error) {
    if(c.cloud_model.empty()||c.cloud_model.size()>128||c.cloud_model.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789:._-/")!=std::string::npos) {
        *error="Invalid --cloud-model identifier.";return false;
    }
    const char* key=std::getenv("OLLAMA_API_KEY");
    if(!key||!*key){*error="Ollama Cloud requires OLLAMA_API_KEY in the process environment.";return false;}
    for(const unsigned char* p=reinterpret_cast<const unsigned char*>(key);*p;++p) if(*p<=32||*p>=127) {
        *error="OLLAMA_API_KEY contains invalid header characters.";return false;
    }
    return true;
}
int EstimateCloudPromptTokens(const LlmGenerationConfig& c,const std::vector<LlmPromptMessage>& input) {
    // Conservative byte-based estimate, not the hosted tokenizer. Independent of GGUF.
    size_t bytes=64;
    for(const auto& m:CloudMessages(c,input)) bytes+=m.role.size()+m.content.size()+32;
    return static_cast<int>(std::min(bytes,size_t(10000000)));
}

bool GenerateOllamaCompletion(const LlmGenerationConfig& c,const std::vector<LlmPromptMessage>& input,
    LlmStreamCallback callback,void* user,LlmGenerationResult* out,const CloudTransport& transport) {
    if(!out)return false;
    *out=LlmGenerationResult();
    auto& r=*out;const auto started=Clock::now();
    auto deadline=started+std::chrono::milliseconds(c.timeout_ms);
    if(c.cloud_budget)deadline=std::min(deadline,c.cloud_budget->deadline);
    auto fail=[&](const std::string& why) {
        r.success=false;r.response_text.clear();r.stop_reason=why;r.error_message="Ollama Cloud: "+why;
        r.wall_time_ms=Ms(started);
        const bool cosmetic_content=c.cloud_optional_prose&&c.use_json_grammar&&
            (why=="invalid_schema"||why=="token_limit"||why=="unexpected_channel");
        if(c.cloud_budget&&!cosmetic_content)c.cloud_budget->failed=true;
        return false;
    };
    try {
        std::string why;
        if(!transport&&!CheckOllamaConfiguration(c,&why))return fail("configuration_error");
        if(input.empty()||c.n_ctx<128||c.n_ctx>32768||c.n_predict<1||c.n_predict>16384||c.timeout_ms<1||
            !std::isfinite(c.temperature)||c.temperature<0||!std::isfinite(c.top_p)||c.top_p<0||c.top_p>1||c.top_k<1) return fail("invalid_bounds");
        auto messages=CloudMessages(c,input);
        const json schema=json::parse(c.json_schema.empty()?"{\"type\":\"object\"}":c.json_schema);
        std::string repair;
        const int max_attempts=c.use_json_grammar?2:1;
        for(int attempt=0;attempt<max_attempts;++attempt) {
            if(c.cloud_budget&&(c.cloud_budget->failed||c.cloud_budget->remaining_calls<=0||c.cloud_budget->remaining_output_tokens<c.n_predict))return fail("interaction_budget");
            if(c.cloud_budget)--c.cloud_budget->remaining_calls;
            if(attempt)messages.push_back({"user","The previous response failed validation at "+repair+
                ". Produce a fresh complete JSON object satisfying the schema and original request. No Markdown."});
            json wire_messages=json::array();size_t estimated=64;
            for(const auto& m:messages){wire_messages.push_back({{"role",m.role},{"content",m.content}});estimated+=m.role.size()+m.content.size()+32;}
            if(estimated+static_cast<size_t>(c.n_predict)>static_cast<size_t>(c.n_ctx))return fail("context_estimate_limit");
            // Cloud has no grammar constraint. Keep thermal style in the prompt,
            // without sampling so wildly that the JSON protocol itself breaks.
            const float temperature=attempt?std::min(c.temperature,0.2f):
                c.cloud_optional_prose?std::min(c.temperature,1.0f):c.temperature;
            json payload={{"model",c.cloud_model},{"messages",wire_messages},{"stream",true},{"think",false},
                {"options",{{"temperature",temperature},{"seed",c.seed},{"top_k",c.top_k},{"top_p",c.top_p},{"num_predict",c.n_predict}}}};
            const std::string body=payload.dump();
            ++r.cloud_attempts;
            // The provider template/revision are unavailable. Exact wire payload is archived instead.
            if(c.research_trace)c.research_trace(true,json{{"backend","ollama-cloud"},{"purpose",c.purpose},{"attempt",attempt+1},
                {"payload",payload},{"json_schema",c.json_schema},{"formatted_prompt",nullptr},{"model_revision",nullptr},
                {"prompt_token_estimate",estimated},{"token_count_precision","utf8_bytes_estimate"},{"constrained_schema",false},
                {"sampling_verification","requested; provider does not report effective settings or determinism"},
                {"n_ctx",c.n_ctx},{"timeout_ms",c.timeout_ms},{"repair_reason",repair},
                {"requested_temperature",c.temperature},{"optional_prose",c.cloud_optional_prose}}.dump(),{});
            std::string pending,raw,content,displayed,stop,error;bool done=false;size_t total=0;
            int prompt_tokens=0,output_tokens=0;bool usage=false;
            auto alive=[&]() {
                if(Clock::now()>=deadline){error="timeout";return false;}
                if(callback&&!callback(displayed.c_str(),"",user)){error="cancelled";return false;}return true;
            };
            auto record=[&](const std::string& line) {
                try {
                if(line.find_first_not_of(" \r\n\t")==std::string::npos)return true;
                if(done){error="data_after_done";return false;}
                const auto j=json::parse(line,nullptr,false);
                if(!j.is_object()||j.contains("error")){error="invalid_stream_record";return false;}
                if(j.contains("model"))r.reported_model=j.at("model").get<std::string>();
                if(j.contains("message")) {
                    const auto& msg=j.at("message");
                    if(!msg.is_object()||msg.value("role",std::string("assistant"))!="assistant"||
                        !msg.value("thinking",std::string()).empty()||(msg.contains("tool_calls")&&!msg["tool_calls"].empty())) {error="unexpected_channel";return false;}
                    content+=msg.value("content",std::string());
                    if(content.size()>static_cast<size_t>(c.n_predict)*32+4096){error="output_too_large";return false;}
                    const auto guarded=GuardFinalChannel(content,false);
                    if(!guarded.error.empty()){error="unexpected_channel";return false;}
                    // Structured candidates are held until schema validation, so rejected attempts never leak into the HUD.
                    if(!c.use_json_grammar&&guarded.text.size()>displayed.size()) {
                        auto delta=guarded.text.substr(displayed.size());displayed=guarded.text;
                        if(callback&&!callback(displayed.c_str(),delta.c_str(),user)){error="cancelled";return false;}
                    }
                }
                if(j.value("done",false)) {
                    done=true;stop=j.value("done_reason",std::string());
                    usage=j.contains("prompt_eval_count")&&j.contains("eval_count");
                    prompt_tokens=j.value("prompt_eval_count",0);output_tokens=j.value("eval_count",0);
                    if(prompt_tokens<0||output_tokens<0){error="invalid_usage";return false;}
                    r.prompt_time_ms=j.value("prompt_eval_duration",0.0)/1e6;
                    r.generation_time_ms=j.value("eval_duration",0.0)/1e6;
                }
                return true;
                } catch (...) {error="invalid_stream_record";return false;}
            };
            auto chunk=[&](const std::string& bytes) {
                total+=bytes.size();if(total>kBodyLimit){error="response_too_large";return false;}
                raw+=bytes;pending+=bytes;
                size_t newline;
                while((newline=pending.find('\n'))!=std::string::npos) {
                    auto line=pending.substr(0,newline);pending.erase(0,newline+1);
                    if(!record(line))return false;
                }
                return alive();
            };
            bool ok=alive();
            std::string transport_error;
            if(ok) {
                try {ok=(transport?transport:CloudTransport(Https))(body,chunk,alive,&transport_error);}
                catch (...) {ok=false;error="transport_exception";}
            }
            if(!transport_error.empty()&&error.empty())error=transport_error;
            if(ok&&!pending.empty())ok=record(pending);
            if(ok&&!done){error="incomplete_stream";ok=false;}
            if(ok&&stop!="stop"){error=stop=="length"?"token_limit":"unknown_completion_reason";ok=false;}
            if(ok&&output_tokens>c.n_predict){error="token_limit";ok=false;}
            const auto guarded=GuardFinalChannel(content,true);
            if(ok&&!guarded.error.empty()){error="unexpected_channel";ok=false;}
            bool invalid_schema=false,unwrapped_fence=false;
            if(ok&&c.use_json_grammar) {
                unwrapped_fence=UnwrapJsonFence(&content);
                const auto parsed=json::parse(content,nullptr,false);
                invalid_schema=parsed.is_discarded()||!parsed.is_object();
                repair="$: expected JSON object";
                if(!invalid_schema)invalid_schema=!ValidateCloudJson(parsed,schema,&repair);
                if(invalid_schema){ok=false;error="invalid_schema";}
            }
            if(ok)ok=alive();
            if(ok&&c.use_json_grammar&&callback&&!callback(content.c_str(),content.c_str(),user)) {
                ok=false;error="cancelled";
            }
            r.raw_text+=raw;r.prompt_tokens+=prompt_tokens;r.generated_tokens+=output_tokens;r.final_tokens+=output_tokens;
            r.usage_reported=usage;r.wall_time_ms=Ms(started);r.inference_time_ms=r.wall_time_ms;
            if(c.cloud_budget)c.cloud_budget->remaining_output_tokens-=usage?output_tokens:c.n_predict;
            if(c.research_trace)c.research_trace(false,json{{"backend","ollama-cloud"},{"purpose",c.purpose},{"attempt",attempt+1},
                {"reported_model",r.reported_model},{"success",ok},{"final",content},{"stop",stop},{"error",error},
                {"unwrapped_json_fence",unwrapped_fence},{"optional_prose",c.cloud_optional_prose},
                {"validation_error",invalid_schema?repair:""},{"prompt_tokens",usage?json(prompt_tokens):json(nullptr)},
                {"generated_tokens",usage?json(output_tokens):json(nullptr)},{"usage_reported",usage},{"wall_ms",r.wall_time_ms}}.dump(),raw);
            if(ok) {
                r.response_text=content;r.success=true;r.reached_eog=true;r.stop_reason="eos";return true;
            }
            if(!invalid_schema||attempt+1==max_attempts)return fail(error.empty()?"transport_failure":error);
            // Give the repair the actual rejected candidate; the next iteration
            // includes it in the conservative context check before sending.
            messages.push_back({"assistant",content});
        }
    } catch (...) {return fail("protocol_error");} // Never expose provider bodies or headers in exceptions.
    return fail("invalid_schema");
}
}
