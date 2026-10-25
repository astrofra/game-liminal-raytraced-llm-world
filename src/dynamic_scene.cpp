#include "dynamic_scene.h"
#include "renderer.h"
#include "nlohmann/json.hpp"
#include <algorithm>
#include <chrono>
#include <map>
#include <sstream>

namespace liminal {
namespace {
using Clock = std::chrono::steady_clock;
using json = nlohmann::json;
std::string PatchSchema() {
    return R"({"type":"object","properties":{"replacements":{"type":"array","minItems":1,"maxItems":24,"items":{"type":"object","properties":{"line":{"type":"integer","minimum":1},"text":{"type":"string","maxLength":12000}},"required":["line","text"],"additionalProperties":false}}},"required":["replacements"],"additionalProperties":false})";
}
std::string NumberedProgram(const std::string& program) {
    std::istringstream input(program);std::string line,result;int number=0;
    while(std::getline(input,line))result+=std::to_string(++number)+": "+line+"\n";
    return result;
}
bool ApplyProgramPatch(const std::string& previous,const std::string& patch,std::string* output,std::string* error) {
    try {
        const auto root=json::parse(patch);
        if(!root.is_object()||root.size()!=1||!root.contains("replacements"))throw std::runtime_error("Expected replacements only");
        const auto& edits=root.at("replacements");
        if(!edits.is_array()||edits.empty()||edits.size()>24)throw std::runtime_error("Expected 1..24 replacements");
        std::vector<std::string> lines;
        for(size_t start=0;start<previous.size();) {
            const auto newline=previous.find('\n',start);
            const auto end=newline==std::string::npos?previous.size():newline+1;
            lines.push_back(previous.substr(start,end-start));start=end;
        }
        std::map<size_t,std::string> changes;
        for(const auto& edit:edits) {
            if(!edit.is_object()||edit.size()!=2||!edit.contains("line")||!edit.contains("text")||
               !edit.at("line").is_number_integer()||!edit.at("text").is_string())throw std::runtime_error("Each replacement needs integer line and string text");
            const auto number=edit.at("line").get<int64_t>();
            const auto text=edit.at("text").get<std::string>();
            if(number<1||number>static_cast<int64_t>(lines.size()+1)||text.size()>12000||
               !changes.emplace(static_cast<size_t>(number-1),text).second)throw std::runtime_error("Invalid, duplicate or oversized replacement");
        }
        std::string result;
        for(size_t i=0;i<=lines.size();++i) {
            const auto found=changes.find(i);
            if(found==changes.end()) {if(i<lines.size())result+=lines[i];continue;}
            if(found->second.empty())continue;
            if(i==lines.size()&&!result.empty()&&result.back()!='\n')result+='\n';
            result+=found->second;
            if(result.back()!='\n'&&(i==lines.size()||lines[i].back()=='\n'))result+='\n';
        }
        if(result.size()>65536)throw std::runtime_error("Patched program exceeds text budget");
        *output=std::move(result);return true;
    } catch(const std::exception& e) {*error=std::string("program_patch_rejected: ")+e.what();return false;}
}
// Accept a single transport wrapper, never search prose for an embedded program.
// Geometry inside the wrapper is preserved byte-for-byte and still fully audited.
bool UnwrapSceneBlock(std::string* text) {
    const auto begin=text->find_first_not_of(" \t\r\n");
    const auto end=text->find_last_not_of(" \t\r\n");
    if(begin==std::string::npos||text->compare(begin,3,"```")!=0)return false;
    const auto line=text->find('\n',begin);
    if(line==std::string::npos)return false;
    auto label=text->substr(begin,line-begin);
    if(!label.empty()&&label.back()=='\r')label.pop_back();
    if(label!="```"&&label!="```scene"&&label!="```.scene")return false;
    if(end<line+4||text->compare(end-2,3,"```")!=0)return false;
    const auto close=end-2;
    if((*text)[close-1]!='\n'||text->find("```",line+1)!=close)return false;
    *text=text->substr(line+1,close-line-1);return true;
}
struct Gate {
    DynamicSceneProgress progress;
    void* user;
    Clock::time_point deadline;
    DynamicScenePhase phase = DynamicScenePhase::Design;
    bool cancelled = false, expired = false;
    std::string last_final;
    bool Poll(const char* text = "", const char* delta = "") {
        if(cancelled || expired) return false;
        expired = Clock::now() >= deadline;
        if(text && *text)last_final=text;
        if(!expired && progress && !progress(phase,text,delta,user)) cancelled=true;
        return !cancelled && !expired;
    }
    bool Stage(DynamicScenePhase next) { phase=next; return Poll(); }
    static bool Stream(const char* text,const char* delta,void* p) {return static_cast<Gate*>(p)->Poll(text,delta);}
    static bool Render(void* p) {return static_cast<Gate*>(p)->Poll();}
};
}

bool GenerateDynamicScene(const SceneDesignContext& context,const LlmGenerationConfig& model,
                          const DynamicSceneConfig& config,DynamicSceneProgress progress,void* user,
                          DynamicSceneResult* result) {
    if(!result) return false;
    *result=DynamicSceneResult();
    const auto start=Clock::now();
    Gate gate{progress,user,start+std::chrono::milliseconds(std::max(1,config.total_timeout_ms))};
    json calls=json::array(),audits=json::array();
    int removed_wrappers=0,patch_repairs=0;
    int last_program_tokens=0;
    double audit_ms=0,preview_ms=0;
    auto record_audit=[&](const char* stage) {
        const auto report=json::parse(SerializeSceneAudit(result->audit));
        audits.push_back({{"stage",stage},{"after_call",result->calls},{"report",report}});
        if(!result->audit.accepted) {
            fprintf(stderr,"[dynamic-scene-audit] %s %s\n",stage,report.dump().c_str());fflush(stderr);
        }
    };
    auto finish=[&](bool success) {
        result->success=success;result->cancelled=gate.cancelled;
        if(gate.cancelled) result->error="Scene generation cancelled";
        else if(gate.expired) result->error="Scene generation exceeded the total timeout";
        if(!success) { result->scene=Scene();result->preview_pixels.clear(); }
        result->metrics_json=json({{"version",1},{"method",config.fixed_frame?"local-fixed-v1":config.direct?"direct":"two-pass"},
            {"success",success},{"cancelled",gate.cancelled},{"timeout",gate.expired},
            {"repairs",result->repairs},{"calls",calls},{"seed",model.seed},{"profile",model.profile_name},{"temperature",model.temperature},
            {"model_path",model.model_path},
            {"backend",model.use_ollama?"ollama":"llama.cpp"},{"ollama_model",model.use_ollama?model.ollama_model:""},
            {"removed_code_wrappers",removed_wrappers},
            {"program_patch_repairs",patch_repairs},
            {"audit_ms",audit_ms},{"preview_ms",preview_ms},
            {"elapsed_ms",std::chrono::duration<double,std::milli>(Clock::now()-start).count()},
            {"audit",json::parse(SerializeSceneAudit(result->audit))},{"audit_history",audits},{"error",result->error}}).dump(2);
        return success;
    };
    auto generate=[&](bool plan,const std::string& prompt,const std::string& previous,
                      const std::string& diagnosis,std::string* text) {
        gate.last_final.clear();
        if(!gate.Stage(diagnosis.empty()?(plan?DynamicScenePhase::Design:DynamicScenePhase::Program):DynamicScenePhase::Repair))return false;
        LlmGenerationConfig cfg=model;
        if(!plan&&!config.direct)cfg.temperature=std::min(cfg.temperature,.2f);
        if(config.fixed_frame)cfg.temperature=.2f;
        // Repeated diagnostics must not replay an identical seeded completion.
        cfg.seed=(model.seed+104729u*static_cast<unsigned int>(result->repairs))&0x7fffffffu;
        if(config.fixed_frame)cfg.seed=model.seed;
        cfg.n_predict=plan?config.plan_tokens:config.program_tokens;
        cfg.use_json_grammar=plan;cfg.json_schema=plan?SceneDesignSchema():"";cfg.grammar_path.clear();
        cfg.timeout_ms=static_cast<int>(std::max<int64_t>(1,std::chrono::duration_cast<std::chrono::milliseconds>(gate.deadline-Clock::now()).count()));
        cfg.purpose=plan?"scene_design_v1":"scene_program_v1";
        bool patch_repair=false;
        std::vector<LlmPromptMessage> messages={
            {"system",plan?"You design coherent 3D places. Return only the requested SceneDesignV1 JSON. Follow the coordinate arithmetic and physical support rules exactly.":
                "You write executable 3D scene programs. Return only .scene directives, one complete directive per line. Follow the coordinate conventions and accepted spatial plan exactly. Do not include Markdown or explanations."},
            {"user",prompt+"\nCreate the place described by the engine context. Return the complete requested artifact now."}};
        if(config.fixed_frame) {
            messages={{"system",LocalSceneSystemPrompt()},{"user",prompt}};
            if(!diagnosis.empty()) {
                messages.push_back({"assistant",previous});
                messages.push_back({"user",LocalSceneRepairPrefix()+diagnosis});
            }
        }
        if(!diagnosis.empty()&&!config.fixed_frame) {
            const std::string repair="\nValidation rejected the previous attempt. Return a corrected COMPLETE replacement, preserving the accepted plan and requested composition. Fix these diagnostics:\n"+diagnosis.substr(0,plan?5000:2400);
            // Short failed programs fit alongside their replacement at the measured
            // size plus margin. Longer ones are reconstructed from plan/diagnostics.
            const bool short_program=!plan&&!previous.empty()&&previous.size()<=6000&&last_program_tokens<=1536;
            if(short_program)cfg.n_predict=std::min(cfg.n_predict,2048);
            if(short_program&&!config.direct) {
                patch_repair=true;cfg.purpose="scene_program_repair_v1";cfg.use_json_grammar=true;cfg.json_schema=PatchSchema();
                messages[0].content="You correct specific lines in a 3D scene program. Return only JSON matching the replacement schema. Keep all unaffected lines unchanged.";
                messages[1].content=prompt+"\nThe candidate below was rejected. Return ONLY a JSON object with replacements: [{line: integer, text: string}]. "
                    "Line numbers refer to this ORIGINAL candidate. An empty text deletes that line; text can contain multiple complete directives separated by newlines. "
                    "Line count+1 appends. Do not include line numbers inside text. Do not draw gaps or clearance volumes. "
                    "Fix all reported errors while preserving the accepted plan. Every unlisted line remains unchanged.\nNUMBERED CANDIDATE:\n"+
                    NumberedProgram(previous)+"\nDIAGNOSTICS:\n"+diagnosis.substr(0,2400)+"\nREPLACEMENT SCHEMA:\n"+cfg.json_schema;
            } else if((plan||short_program)&&!previous.empty()) {
                messages.push_back({"assistant",previous});messages.push_back({"user",repair});
            } else messages.back().content+=repair;
        }
        LlmGenerationResult generation;
        const bool ok=config.completion?config.completion(cfg,messages,Gate::Stream,&gate,&generation):
            GenerateChatCompletion(cfg,messages,Gate::Stream,&gate,&generation);
        ++result->calls;
        if(!plan&&!patch_repair)last_program_tokens=generation.final_tokens;
        calls.push_back({{"purpose",cfg.purpose},{"success",ok},{"eos",generation.reached_eog},
            {"seed",cfg.seed},{"temperature",cfg.temperature},
            {"prompt_sha256",SceneTextHash(generation.prompt_text)},
            {"stop_reason",generation.stop_reason},{"prompt_tokens",generation.prompt_tokens},
            {"generated_tokens",generation.generated_tokens},{"reasoning_tokens",generation.reasoning_tokens},
            {"final_tokens",generation.final_tokens},{"offloaded_layers",generation.offloaded_layers},{"load_ms",generation.load_time_ms},
            {"inference_ms",generation.inference_time_ms},{"wall_ms",generation.wall_time_ms}});
        *text=generation.response_text.empty()?gate.last_final:generation.response_text;
        const std::string generated_text=*text;
        if(patch_repair)*text=previous; // A truncated/failed patch cannot replace the candidate.
        if(!gate.Poll())return false;
        if(!ok){result->error=generation.error_message.empty()?"LLM generation failed":generation.error_message;
            if(patch_repair)result->error+="\nUnchanged scene diagnostics: "+diagnosis;return false;}
        if(!generation.reached_eog){result->error="incomplete_output: generation must end with EOS, not a token limit";
            if(patch_repair)result->error+="\nUnchanged scene diagnostics: "+diagnosis;return false;}
        if(patch_repair) {
            ++patch_repairs;
            if(!ApplyProgramPatch(previous,generated_text,text,&result->error)) {
                result->error+="\nUnchanged scene diagnostics: "+diagnosis;return false;
            }
        }
        return true;
    };
    const int repair_limit=std::clamp(config.max_repairs,0,config.fixed_frame?1:2);
    if(!config.direct&&!config.fixed_frame) {
        std::string prior,diagnosis;
        for(;;) {
            bool ok=generate(true,BuildSceneDesignPrompt(context),prior,diagnosis,&result->plan_json);
            if(ok) {
                const auto audit_start=Clock::now();
                ok=ParseSceneDesign(result->plan_json,context,&result->design,&result->audit);
                audit_ms+=std::chrono::duration<double,std::milli>(Clock::now()-audit_start).count();
                record_audit("design");
                if(!ok)result->error=SerializeSceneAudit(result->audit);
            }
            if(ok) { result->plan_json=SerializeSceneDesign(result->design);break; }
            if(!gate.Poll() || result->repairs>=repair_limit)return finish(false);
            prior=result->plan_json;diagnosis=result->error;++result->repairs;
        }
    }
    const SceneDesignV1* design=(config.direct||config.fixed_frame)?nullptr:&result->design;
    result->audit=SceneAuditReport();
    std::string prior,diagnosis;
    for(;;) {
        bool ok=generate(false,config.fixed_frame?BuildLocalSceneProgramPrompt(context.brief,config.landmark):
            BuildSceneProgramPrompt(context,design),prior,diagnosis,&result->scene_text);
        if(ok&&UnwrapSceneBlock(&result->scene_text))++removed_wrappers;
        if(ok&&config.fixed_frame)ok=ValidateLocalSceneContract(result->scene_text,&result->error);
        if(ok && gate.Stage(DynamicScenePhase::Audit)) {
            const auto audit_start=Clock::now();
            SceneViewConstraints view;view.landmark=config.landmark;view.near_distance=3;view.max_near_coverage=.9f;
            view.require_return_clearance=true;
            ok=AuditDynamicScene(result->scene_text,context,design,&result->scene,&result->audit,config.fixed_frame?&view:nullptr);
            audit_ms+=std::chrono::duration<double,std::milli>(Clock::now()-audit_start).count();
            record_audit("geometry");
            if(!ok)result->error=SerializeSceneAudit(result->audit);
        } else ok=false;
        if(ok && gate.Stage(DynamicScenePhase::Preview)) {
            const auto preview_start=Clock::now();
            RenderConfig preview;preview.width=256;preview.height=128;preview.samples_per_pixel=4;
            preview.seed=model.seed;preview.max_bounces=3;
            RenderControl control{Gate::Render,&gate};
            ok=RenderSceneToPixels(result->scene,preview,&result->preview_pixels,&control);
            if(ok)ok=AuditDynamicPreview(result->preview_pixels,design?design->lighting_mode:"night",&result->audit);
            preview_ms+=std::chrono::duration<double,std::milli>(Clock::now()-preview_start).count();
            record_audit("preview");
            if(!ok)result->error=SerializeSceneAudit(result->audit);
        } else ok=false;
        if(!gate.Poll())return finish(false);
        if(ok) { result->scene_hash=SceneTextHash(result->scene_text);result->error.clear();return finish(true); }
        if(result->repairs>=repair_limit)return finish(false);
        prior=result->scene_text;diagnosis=result->error;++result->repairs;
    }
}
} // namespace liminal
