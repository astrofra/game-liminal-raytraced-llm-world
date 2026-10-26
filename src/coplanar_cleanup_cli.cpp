#include "coplanar_cleanup.h"
#include "glass_supports.h"
#include "scene_hash.h"
#include "animated_view.h"
#include "renderer.h"
#include "nlohmann/json.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

using namespace liminal;
using json=nlohmann::json;

int main(int argc,char** argv) {
    if(argc<3) {
        std::cerr<<"Usage: liminal_coplanar_cleanup <input.scene> <output-prefix> [--analyze] [--render] [--glass] [--samples N] [--fixed ID]\n";
        return 2;
    }
    try {
        CoplanarCleanupOptions options;bool render=false,glass=false;
        RenderConfig config;config.width=640;config.height=320;config.samples_per_pixel=32;
        for(int i=3;i<argc;++i) {
            const std::string arg=argv[i];
            if(arg=="--analyze")options.apply_translations=false;
            else if(arg=="--render")render=true;
            else if(arg=="--glass")glass=true;
            else if(arg=="--fixed"&&i+1<argc)options.fixed_ids.push_back(argv[++i]);
            else if(arg=="--samples"&&i+1<argc)config.samples_per_pixel=std::clamp(std::stoi(argv[++i]),1,1024);
            else throw std::runtime_error("Unknown option: "+arg);
        }
        std::ifstream input(std::filesystem::u8path(argv[1]),std::ios::binary);
        if(!input)throw std::runtime_error("Cannot read source");
        std::ostringstream data;data<<input.rdbuf();
        Scene original;char error[512]={};
        if(!LoadSceneFromSceneText("before",OpaqueSceneWithoutGlassSupports(data.str()).c_str(),&original,error,sizeof(error)))throw std::runtime_error(error);
        const auto pose_b=BuildBreathingPoseB(original.camera,AnimatedViewConfig());
        for(int i=1;i<8;++i)options.extra_cameras.push_back(CalculateAnimatedCameraPose(original.camera,pose_b,i));
        const auto result=CleanupCoplanarFaces(data.str(),options);auto report=json::parse(result.report_json);
        const std::string prefix=argv[2];const auto parent=std::filesystem::u8path(prefix).parent_path();
        if(!parent.empty())std::filesystem::create_directories(parent);
        auto write=[&](const std::string& suffix,const std::string& text) {
            std::ofstream file(std::filesystem::u8path(prefix+suffix),std::ios::binary);file<<text;file.flush();
            if(!file)throw std::runtime_error("Cannot write artifact");
        };
        write(".before.scene",result.source_scene_text);write(".corrected.scene",result.scene_text);
        std::string final=result.scene_text;
        if(glass&&result.success&&options.apply_translations) {
            const auto support=BuildGlassSupports(final);final=support.scene_text;
            auto s=json::parse(support.report_json);s["opaque_sha256"]=SceneTextHash(result.scene_text);s["final_sha256"]=SceneTextHash(final);
            write(".supports.json",s.dump(2));
        }
        write(".scene",final);report["final_sha256"]=SceneTextHash(final);
        if(render&&result.success) {
            std::vector<std::pair<std::string,std::string>> stages={{"before",result.source_scene_text},{"after",result.scene_text}};
            if(glass)stages.push_back({"glass",final});
            for(const auto& stage:stages) {
                Scene scene;if(!LoadSceneFromSceneText(stage.first.c_str(),stage.second.c_str(),&scene,error,sizeof(error)))throw std::runtime_error(error);
                for(int i=0;i<8;++i) {
                    scene.camera=CalculateAnimatedCameraPose(original.camera,pose_b,i);
                    const auto start=std::chrono::steady_clock::now();
                    const auto file=prefix+"."+stage.first+"-"+std::to_string(i)+".png";
                    if(!RenderSceneToImage(scene,config,file.c_str()))throw std::runtime_error("Render failed");
                    report["renders"][stage.first].push_back({{"frame",i},{"eye",{scene.camera.eye.x,scene.camera.eye.y,scene.camera.eye.z}},
                        {"target",{scene.camera.target.x,scene.camera.target.y,scene.camera.target.z}},
                        {"up",{scene.camera.up.x,scene.camera.up.y,scene.camera.up.z}},{"fov",scene.camera.vertical_fov_degrees},
                        {"milliseconds",std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()}});
                }
            }
            report["render_config"]={{"width",config.width},{"height",config.height},{"samples",config.samples_per_pixel},
                {"seed",config.seed},{"bounces",config.max_bounces},{"glass_bounces",config.max_glass_bounces}};
        }
        write(".coplanar.json",report.dump(2)+"\n");
        std::cout<<"conflicts="<<result.initial_conflicts<<" remaining="<<result.remaining_conflicts<<" moves="<<result.accepted_translations<<'\n';
        if(!result.success)std::cerr<<result.error<<'\n';return result.success?0:1;
    } catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
