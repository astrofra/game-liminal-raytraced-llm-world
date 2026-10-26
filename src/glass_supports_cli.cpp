#include "glass_supports.h"
#include "renderer.h"
#include "nlohmann/json.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

// Offline pilot: no model, no game state, identical render settings for both sides.
int main(int argc,char** argv) {
    if(argc<3) {
        std::cerr<<"Usage: liminal_glass_supports <input.scene> <output-prefix> [--analyze] [--render] [--samples N]\n";
        return 2;
    }
    try {
        liminal::GlassSupportOptions options;bool render=false;
        liminal::RenderConfig config;config.width=640;config.height=320;config.samples_per_pixel=32;
        for(int i=3;i<argc;++i) {
            const std::string arg=argv[i];
            if(arg=="--analyze")options.build_supports=false;
            else if(arg=="--render")render=true;
            else if(arg=="--samples"&&i+1<argc)config.samples_per_pixel=std::clamp(std::stoi(argv[++i]),1,1024);
            else throw std::runtime_error("Unknown option: "+arg);
        }
        std::ifstream input(std::filesystem::u8path(argv[1]),std::ios::binary);
        if(!input)throw std::runtime_error("Cannot read source scene");
        std::ostringstream data;data<<input.rdbuf();
        auto result=liminal::BuildGlassSupports(data.str(),options);
        auto report=nlohmann::json::parse(result.report_json);
        const std::string prefix=argv[2];
        const auto parent=std::filesystem::u8path(prefix).parent_path();
        if(!parent.empty())std::filesystem::create_directories(parent);
        auto write=[&](const char* suffix,const std::string& text) {
            std::ofstream file(std::filesystem::u8path(prefix+suffix),std::ios::binary);
            file<<text;file.flush();if(!file)throw std::runtime_error("Cannot write pilot artifact");
        };
        write(".opaque.scene",result.opaque_scene_text);write(".scene",result.scene_text);
        if(render&&result.success) {
            for(const std::string side:{"before","after"}) {
                const auto& text=side=="before"?result.opaque_scene_text:result.scene_text;
                liminal::Scene scene;char error[512]={};
                if(!liminal::LoadSceneFromSceneText("pilot",text.c_str(),&scene,error,sizeof(error)))throw std::runtime_error(error);
                const auto start=std::chrono::steady_clock::now();
                if(!liminal::RenderSceneToImage(scene,config,(prefix+"."+side+".png").c_str()))throw std::runtime_error("Render failed");
                report["renders"][side]={{"triangles",scene.triangles.size()},
                    {"milliseconds",std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()}};
            }
            report["render_config"]={{"width",config.width},{"height",config.height},{"samples",config.samples_per_pixel},
                {"seed",config.seed},{"bounces",config.max_bounces},{"glass_bounces",config.max_glass_bounces}};
        }
        write(".supports.json",report.dump(2)+"\n");
        std::cout<<"supports="<<result.added_supports<<" triangles="<<result.added_triangles<<" success="<<result.success<<"\n";
        if(!result.success)std::cerr<<result.error<<"\n";
        return result.success?0:1;
    } catch(const std::exception& e) {std::cerr<<e.what()<<"\n";return 1;}
}
