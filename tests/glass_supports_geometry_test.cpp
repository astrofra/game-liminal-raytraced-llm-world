#include "glass_supports.h"
#include "renderer.h"
#include "nlohmann/json.hpp"
#include <array>
#include <map>
#include <set>
#include <stdexcept>
#include <cstdio>
#include <sstream>

using namespace liminal;
using json=nlohmann::json;
#define CHECK(c) do {if(!(c))throw std::runtime_error(std::string("line ")+std::to_string(__LINE__)+": "+#c);} while(false)
const std::string header="room \"mesh\"\ncamera eye(20,2,-10) target(0,2,5) fov(50)\n"
    "plane \"floor\" pos(0,0,0) normal(0,1,0) size(100,100) gray(.2)\n";
Scene Load(const std::string& text) {Scene s;char error[256]={};CHECK(LoadSceneFromSceneText("test",text.c_str(),&s,error,sizeof(error)));return s;}
using Key=std::array<float,3>;
Key Point(Vec3 v){return {v.x,v.y,v.z};}
void Verify(const std::string& source) {
    auto r=BuildGlassSupports(source);CHECK(r.success);CHECK(r.added_supports==1);
    const auto scene=Load(r.scene_text),opaque=Load(source);
    CHECK(r.scene_text==BuildGlassSupports(r.scene_text).scene_text);
    std::map<std::pair<Key,Key>,std::pair<int,int>> edges;
    std::set<std::array<Key,3>> faces;
    size_t top=0,bottom=0;double volume=0;
    for(const auto& t:scene.triangles)if(scene.materials[t.material_index].model==kMaterialModelDielectricGlass) {
        const auto& m=scene.materials[t.material_index];
        CHECK(m.index_of_refraction==1.52f&&m.dispersion==.035f&&m.volume_absorption.x==.45f);
        CHECK(Length(Cross(t.v1-t.v0,t.v2-t.v0))>1e-7f);
        const Vec3 v[3]={t.v0,t.v1,t.v2};
        std::array<Key,3> sorted={Point(v[0]),Point(v[1]),Point(v[2])};
        std::sort(sorted.begin(),sorted.end());CHECK(faces.insert(sorted).second);
        for(int i=0;i<3;++i) {
            auto a=Point(v[i]),b=Point(v[(i+1)%3]);const int sign=a<b?1:-1;
            auto& count=edges[std::minmax(a,b)];++count.first;count.second+=sign;
        }
        volume+=Dot(t.v0,Cross(t.v1,t.v2))/6.;
        if(t.normal.y>.00001f) {
            ++top;
            // Independently query original rendered triangles, not construction
            // planes. Every top triangle centroid must touch the true underside.
            Ray ray;ray.origin=t.centroid-Vec3(0,.02f,0);ray.direction=Vec3(0,1,0);Hit hit;
            CHECK(ProbeSceneRay(opaque,ray,.05f,&hit));CHECK(std::abs(hit.t-.02f)<.0002f);
        } else if(t.normal.y<-.9999f) {
            ++bottom;CHECK(std::abs(t.v0.y)<1e-6&&std::abs(t.v1.y)<1e-6&&std::abs(t.v2.y)<1e-6);
        }
    }
    CHECK(top>0&&bottom>0&&volume>0);
    for(const auto& e:edges)CHECK(e.second.first==2&&e.second.second==0);
    // The centre stays empty (no full block, bottom slab or glass ceiling).
    Ray ray;ray.origin=Vec3(0,.01f,5);ray.direction=Vec3(0,1,0);Hit a,b;
    CHECK(ProbeSceneRay(opaque,ray,20,&a));CHECK(ProbeSceneRay(scene,ray,20,&b));CHECK(std::abs(a.t-b.t)<1e-4);
    CHECK(scene.materials[scene.triangles[b.triangle_index].material_index].model==kMaterialModelDiffuse);
    // Opaque triangles are exactly preserved after serialization/BVH rebuild.
    std::multiset<std::array<Key,3>> before,after;
    for(const auto& t:opaque.triangles)before.insert({Point(t.v0),Point(t.v1),Point(t.v2)});
    for(const auto& t:scene.triangles)if(scene.materials[t.material_index].model==kMaterialModelDiffuse)
        after.insert({Point(t.v0),Point(t.v1),Point(t.v2)});
    CHECK(before==after);
}
int main() {
    try {
        for(const auto& angle:std::vector<Vec3>{{0,0,0},{0,38,0},{19,27,13},{90,0,0},{89.99f,45,5},
                {0,0,90},{-36,71,-12},{45,45,45}}) {
            std::ostringstream s;s<<header<<"box \"mass\" pos(0,6,5) size(5,2,3) rot("<<angle.x<<','<<angle.y<<','<<angle.z<<") gray(.5)\n";
            Verify(s.str());
        }
        const auto shell=BuildGlassSupports(header+"box \"mass\" pos(0,4,5) size(4,2,4) gray(.5)\n");
        const auto report=json::parse(shell.report_json);const auto& block=report["blocks"][0];
        CHECK(block["inner_footprint"].size()==4);
        for(const auto& p:block["inner_footprint"])CHECK(std::abs(std::abs(p[0].get<double>())-1.75)<1e-5);
        // First candidate omitted, next member of the same component can work.
        auto fallback=BuildGlassSupports(header+
            "box \"low\" pos(0,2,5) size(.4,1,2) gray(.5)\n"
            "box \"high\" pos(1.7,2.5,5) size(3,1,2) gray(.5)\n");
        CHECK(fallback.added_supports==1);
        // Put the eye inside the actual proposed wall, not the hollow interior.
        auto camera=BuildGlassSupports("room \"camera\"\ncamera eye(1.9,1,5) target(0,1,5) fov(50)\n"
            "box \"mass\" pos(0,4,5) size(4,2,4) gray(.5)\n");
        CHECK(camera.added_supports==0);CHECK(json::parse(camera.report_json)["blocks"][0]["decision"]=="reserved_volume");
        // Glass never anchors the opaque contact graph.
        auto glass=BuildGlassSupports(header+"glass_box \"pane\" pos(0,1,5) size(3,2,3) gray(.66)\n"
            "box \"mass\" pos(0,3,5) size(3,2,3) gray(.5)\n");
        CHECK(json::parse(glass.report_json)["blocks"].size()==1);
        CHECK(!json::parse(glass.report_json)["blocks"][0]["grounded"].get<bool>());
        const auto commented=header+"\n# engine-glass-supports-v1 begin\n"
            "box \"authored\" pos(0,1,5) size(2,2,2) gray(.5)\n# engine-glass-supports-v1 end\n";
        CHECK(OpaqueSceneWithoutGlassSupports(commented)==commented);
        std::puts("Glass meshes: oriented watertight shells, true underside, hollow interior, material, exclusions and opaque preservation passed");return 0;
    } catch(const std::exception& e){std::fprintf(stderr,"%s\n",e.what());return 1;}
}
