#include "research_archive.h"
#include "object_interactions.h"
#include "investigation.h"
#include "renderer.h"
#include "scene_compiler.h"
#include "scene_design.h"
#include "nlohmann/json.hpp"
#include "stb_image_write.h"
#include <chrono>
#include <cstdlib>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <random>
#include <sstream>
#include <stdexcept>

namespace liminal {
namespace {
using json=nlohmann::json;
std::string UtcTime() {
    const auto now=std::chrono::system_clock::now();
    const std::time_t value=std::chrono::system_clock::to_time_t(now);
    std::tm utc{};
#if defined(_WIN32)
    gmtime_s(&utc,&value);
#else
    gmtime_r(&value,&utc);
#endif
    std::ostringstream out;out<<std::put_time(&utc,"%Y%m%dT%H%M%SZ");return out.str();
}
std::string Number(unsigned int value) {
    std::ostringstream out;out<<std::setw(5)<<std::setfill('0')<<value;return out.str();
}
std::filesystem::path DefaultRoot() {
    if(const char* local=std::getenv("LOCALAPPDATA"))return std::filesystem::u8path(local)/"WithinTheLatentWalls"/"research";
    if(const char* data=std::getenv("XDG_DATA_HOME"))return std::filesystem::u8path(data)/"within-the-latent-walls"/"research";
    if(const char* home=std::getenv("HOME"))return std::filesystem::u8path(home)/".local"/"share"/"within-the-latent-walls"/"research";
    throw std::runtime_error("No persistent user directory available for research archives");
}
}

bool ResearchArchive::Guard(const std::function<void()>& action) {
    try {action();return true;}
    catch(const std::exception& e) {error_=e.what();std::fprintf(stderr,"Research archive: %s\n",error_.c_str());return false;}
}
void ResearchArchive::Write(const std::string& name,const std::string& text,bool append) {
    std::ofstream file(path_/name,std::ios::binary|(append?std::ios::app:std::ios::trunc));
    file.write(text.data(),static_cast<std::streamsize>(text.size()));file.flush();
    if(!file)throw std::runtime_error("Cannot write research artifact: "+(path_/name).u8string());
}
void ResearchArchive::Snapshot(const SessionState& state) {
    std::string text;
    if(!SerializeSessionStateToJsonString(state,&text))throw std::runtime_error("Cannot serialize research snapshot");
    Write("state.json",text);
}
void ResearchArchive::Place(const SessionState& state) {
    if(places_.count(state.current_place_id))return;
    const std::string stem="places/place-"+Number(static_cast<unsigned int>(places_.size()));
    std::string scene;json provenance;
    if(IsGeneratedPlaceId(state.current_place_id)) {
        const GeneratedRoom* found=nullptr;
        for(const auto& room:state.generated_rooms)if(room.room_id==state.current_place_id){found=&room;break;}
        if(!found)throw std::runtime_error("Cannot archive a missing generated place");
        scene=found->scene_text;
        if(!found->pre_coplanar_scene_text.empty())Write(stem+".pre-coplanar.scene",found->pre_coplanar_scene_text);
        if(!found->coplanar_cleanup_json.empty())Write(stem+".coplanar.json",found->coplanar_cleanup_json+"\n");
        if(!found->opaque_scene_text.empty())Write(stem+".opaque.scene",found->opaque_scene_text);
        if(!found->glass_supports_json.empty())Write(stem+".supports.json",found->glass_supports_json+"\n");
        provenance={{"model",found->scene_model_filename},{"seed",found->scene_seed},
            {"brief_and_landmark",found->scene_design_json},{"prompt_version",found->scene_prompt_version},
            {"repairs",found->scene_repairs},{"scene_source",found->scene_source}};
    } else {
        char error[512]={};
        if(!BuildSceneTextFromSpatialState(state.spatial_state,&scene,error,sizeof(error)))throw std::runtime_error(error);
        provenance={{"scene_source","authored_start"}};
    }
    const std::string title=DescribeCurrentPlaceLabel(state);
    std::string description=state.spatial_state.room_summary;
    if(provenance.contains("brief_and_landmark")) {
        const auto brief=json::parse(provenance["brief_and_landmark"].get<std::string>(),nullptr,false);
        if(brief.is_object())description+="\n\nBrief de composition :\n"+brief.value("brief",std::string());
    }
    Write(stem+".scene",scene);
    Write(stem+".txt",title+"\n\n"+description+"\n");
    Write(stem+".json",json({{"place_id",state.current_place_id},{"title",title},
        {"description",state.spatial_state.room_summary},{"scene_sha256",SceneTextHash(scene)},
        {"provenance",provenance}}).dump(2)+"\n");
    Write("README.md","\n## "+title+"\n\n"+state.spatial_state.room_summary+
        "\n\n[Scène]("+stem+".scene) · [Image]("+stem+".png) · [Provenance]("+stem+".json)\n",true);
    places_[state.current_place_id]=stem;
}
void ResearchArchive::WriteImage(const SessionState& state,const std::vector<unsigned char>& pixels) {
    const auto found=places_.find(state.current_place_id);
    if(found==places_.end())throw std::runtime_error("Image has no archived place");
    const auto file=path_/(found->second+".png");
    const bool first=!std::filesystem::exists(file);
    const bool objects=state.objects.revision>0&&pending_;
    last_visual_reference_=objects?"turns/turn-"+Number(command_index_):found->second;
    if(!first&&!objects)return;
    if(pixels.size()!=static_cast<size_t>(render_.width)*render_.height*3)
        throw std::runtime_error("Invalid research image dimensions");
    // Encode in memory so Windows Unicode paths are handled by std::filesystem.
    std::vector<char> png;
    auto append=[](void* context,void* bytes,int count) {
        auto& data=*static_cast<std::vector<char>*>(context);const auto* begin=static_cast<const char*>(bytes);
        data.insert(data.end(),begin,begin+count);
    };
    if(!stbi_write_png_to_func(append,&png,render_.width,render_.height,3,pixels.data(),render_.width*3))
        throw std::runtime_error("Cannot encode research image");
    if(first)Write(found->second+".png",std::string(png.begin(),png.end()));
    if(objects)Write("turns/turn-"+Number(command_index_)+".png",std::string(png.begin(),png.end()));
}
bool ResearchArchive::Start(const std::string& root,const SessionState& state,
                            const LlmGenerationConfig& model,const RenderConfig& render) {
    return Guard([&] {
        render_=render;const auto parent=root.empty()?DefaultRoot():std::filesystem::u8path(root);
        std::filesystem::create_directories(parent);
        bool created=false;
        for(int attempt=0;attempt<32&&!created;++attempt) {
            path_=parent/("run-"+UtcTime()+"-"+std::to_string(std::random_device{}()));
            created=std::filesystem::create_directory(path_);
        }
        if(!created)throw std::runtime_error("Cannot allocate a unique research archive");
        std::filesystem::create_directory(path_/"places");std::filesystem::create_directory(path_/"turns");
        Write("manifest.json",json({{"schema",1},{"started_utc",UtcTime()},{"status","running"},
            {"new_game",true},{"llm_seed",model.seed},{"model",model.model_path},
            {"backend","llama.cpp"},{"width",render.width},{"height",render.height},
            {"samples",render.samples_per_pixel},{"render_seed",render.seed},
            {"scene_system_prompt",LocalSceneSystemPrompt()},{"repair_prefix",LocalSceneRepairPrefix()},
            {"image_kind","first displayed frame, post-processing included, without HUD"}}).dump(2)+"\n");
        Write("README.md","# Archive de partie\n\nNouvelle partie. Archive de recherche uniquement : jamais chargée automatiquement par le jeu.\n\n"
            "Descriptions, scènes acceptées, première image de chaque lieu, commandes et résultats sont écrits au fil de la partie. "
            "`state.json` est un instantané de recherche, pas une reprise de partie. "
            "Les tentatives interrompues ou rejetées figurent dans `turns/`. "
            "`closed.json` indique une fermeture normale ; son absence signale une interruption possible.\n");
        Write("events.jsonl",json({{"event","start"},{"utc",UtcTime()}}).dump()+"\n");
        Place(state);Snapshot(state);
        std::printf("Research archive: %s\n",Path().c_str());std::fflush(stdout);
    });
}
bool ResearchArchive::Image(const SessionState& state,const std::vector<unsigned char>& pixels) {
    return Guard([&]{WriteImage(state,pixels);});
}
bool ResearchArchive::Command(const std::string& command) {
    return Guard([&]{++command_index_;command_=command;pending_=true;
        Write("events.jsonl",json({{"event","command"},{"utc",UtcTime()},
            {"index",command_index_},{"command",command}}).dump()+"\n",true);
    });
}
void ResearchArchive::Result(const std::string& status,const HeadlessTurnResult& turn,const std::string& error) {
    const std::string stem="turns/turn-"+Number(command_index_);
    Write(stem+".json",json({{"utc",UtcTime()},{"command",command_},{"status",status},{"error",error},
        {"from",turn.initial_place_id},{"to",turn.updated_place_id},{"description",turn.turn_result.narration},
        {"generated_room",turn.generated_room_created},{"prompt",turn.prompt_text},{"intent",turn.turn_result.intent},
        {"request",turn.request_text},{"engine_outcome",turn.engine_outcome},
        {"view_reused",turn.view_unchanged},{"visual_reference",last_visual_reference_},
        {"inventory_before",turn.initial_hard_state.inventory_items},{"inventory_after",turn.updated_hard_state.inventory_items},
        {"interaction",turn.interaction_metrics_json},{"objects_prepared",turn.objects_prepared},
        {"investigation",{{"before",json::parse(SerializeInvestigation(turn.initial_investigation))},
            {"candidate",json::parse(SerializeInvestigation(turn.updated_investigation))},
            {"published",status=="accepted"&&turn.investigation_prepared},
            {"placement_rejected",turn.investigation_placement_rejected},
            {"decision",json::parse(turn.investigation_metrics_json,nullptr,false).is_object()?
                json::parse(turn.investigation_metrics_json):json(nullptr)}}},
        {"object_revision_before",turn.initial_object_revision},{"objects_after",json::parse(SerializeObjectState(turn.updated_objects))},
        {"raw_response",turn.raw_response_text},{"repair_response",turn.repair_response_text},
        {"narrative_metrics",turn.narrative_metrics_json},
        {"audio",json::parse(AudioDecisionJson(turn.audio_mix,status=="accepted"))},
        {"scene_provenance",turn.dynamic_scene_design_json},{"scene_metrics",turn.dynamic_scene_metrics_json},
        {"thermal_prompt",turn.thermal_prompt_text},{"thermal_response",turn.thermal_response_text}}).dump(2)+"\n");
    if(!turn.rendered_scene_text.empty())Write(stem+".scene",turn.rendered_scene_text);
    if(status=="accepted"&&turn.updated_objects.revision>0&&!turn.rendered_scene.triangles.empty()) {
        // Recompute from the composed revision; never reuse the architecture's
        // visibility/depth evidence after a mobile object changes.
        const auto& scene=turn.rendered_scene;
        const auto forward=Normalize(scene.camera.target-scene.camera.eye);
        const auto right=Normalize(Cross(forward,scene.camera.up)),up=Normalize(Cross(right,forward));
        const float scale=std::tan(scene.camera.vertical_fov_degrees*3.14159265f/360.f);
        std::vector<std::string> names(1,"");
        for(const auto& material:scene.materials)names.push_back(material.name);
        std::string ids="P5\n256 128\n65535\n",depth="P5\n256 128\n65535\n";json metres=json::array();
        for(int y=0;y<128;++y)for(int x=0;x<256;++x) {
            const auto direction=Normalize(forward+right*((2.f*(x+.5f)/256.f-1.f)*2.f*scale)+up*((1.f-2.f*(y+.5f)/128.f)*scale));
            Hit hit;unsigned int id=0,d=65535;
            if(ProbeSceneRay(scene,{scene.camera.eye,direction},kHuge,&hit)) {
                id=static_cast<unsigned int>(scene.triangles[hit.triangle_index].material_index+1);
                d=static_cast<unsigned int>(std::min(65534.f,hit.t*1000.f));metres.push_back(hit.t);
            } else metres.push_back(nullptr);
            ids+=static_cast<char>(id>>8);ids+=static_cast<char>(id&255);
            depth+=static_cast<char>(d>>8);depth+=static_cast<char>(d&255);
        }
        Write(stem+".objects-ids.pgm",ids);Write(stem+".objects-depth-mm.pgm",depth);
        Write(stem+".objects-visibility.json",json{{"width",256},{"height",128},{"revision",turn.updated_objects.revision},
            {"object_names",names},{"metres",metres},{"kind","opaque first-hit evidence, before visor optics"}}.dump());
    }
    if(!turn.raw_scene_audit_response_text.empty())Write(stem+".candidate.scene",turn.raw_scene_audit_response_text);
    for(size_t i=0;i<turn.scene_attempts.size();++i) {
        const auto& attempt=turn.scene_attempts[i];const auto prefix=stem+"-attempt-"+std::to_string(i+1);
        Write(prefix+".raw.txt",attempt.raw);Write(prefix+".normalized.scene",attempt.normalized);
        Write(prefix+".scene",attempt.final_program);
        Write(prefix+".json",json({{"seed",attempt.seed},{"normalizations",attempt.normalizations},
            {"removed_objects",attempt.removed_objects},{"error",attempt.error},
            {"local_repair_committed",!attempt.removed_objects.empty()&&attempt.normalized!=attempt.final_program},
            {"local_repair_programs",attempt.repair_programs},
            {"raw_sha256",SceneTextHash(attempt.raw)},{"final_sha256",SceneTextHash(attempt.final_program)},
            {"before",json::parse(SerializeSceneAudit(attempt.before))},
            {"after",json::parse(SerializeSceneAudit(attempt.after))}}).dump(2));
        auto bitmap=[&](const SceneAuditReport& report,const std::string& suffix) {
            if(report.object_ids.size()!=256*128)return;
            std::string ids="P5\n256 128\n65535\n",depth="P5\n256 128\n65535\n";
            for(size_t pixel=0;pixel<report.object_ids.size();++pixel) {
                const auto id=report.object_ids[pixel];ids+=static_cast<char>(id>>8);ids+=static_cast<char>(id&255);
                // Research preview in millimetres, 65535 denotes background; floats retained below.
                const auto d=static_cast<uint16_t>(std::isfinite(report.depth[pixel])?
                    std::min(65534.0f,report.depth[pixel]*1000.0f):65535.0f);
                depth+=static_cast<char>(d>>8);depth+=static_cast<char>(d&255);
            }
            Write(prefix+suffix+".ids.pgm",ids);Write(prefix+suffix+".depth-mm.pgm",depth);
            Write(prefix+suffix+".depth.json",json({{"width",256},{"height",128},{"metres",report.depth},
                {"background","null (infinite depth)"},{"object_names",report.object_names}}).dump());
        };
        bitmap(attempt.before,"-before");bitmap(attempt.after,"-after");
    }
    Write("events.jsonl",json({{"event","result"},{"utc",UtcTime()},{"index",command_index_},
        {"status",status},{"error",error}}).dump()+"\n",true);
    Write("transcript.txt","> "+command_+"\n["+status+"] "+(error.empty()?turn.turn_result.narration:error)+"\n\n",true);
    pending_=false;
}
bool ResearchArchive::Turn(const SessionState& state,const HeadlessTurnResult& turn,
                           const std::vector<unsigned char>& pixels) {
    return Guard([&]{Place(state);if(!turn.view_unchanged)WriteImage(state,pixels);
        Snapshot(state);Result("accepted",turn,"");});
}
bool ResearchArchive::Failure(const std::string& error,const HeadlessTurnResult& turn) {
    return Guard([&]{Result("not_committed",turn,error);});
}
bool ResearchArchive::Close(const SessionState& state) {
    return Guard([&]{Snapshot(state);Write("closed.json",json({{"ended_utc",UtcTime()},
        {"commands",command_index_},{"places",places_.size()},{"archive_error",error_}}).dump(2)+"\n");});
}
bool ResearchArchive::AudioEvent(const std::string& event_json) {
    return Guard([&]{auto event=json::parse(event_json);event["utc"]=UtcTime();
        event["command_index"]=command_index_;Write("audio-events.jsonl",event.dump()+"\n",true);});
}
} // namespace liminal
