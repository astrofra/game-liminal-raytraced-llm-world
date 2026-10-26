#include "coplanar_cleanup.h"
#include "glass_supports.h"
#include "scene_hash.h"
#include "animated_view.h"
#include "nlohmann/json.hpp"
#include <cstdio>
#include <map>
#include <iomanip>
#include <cmath>
#include <sstream>
#include <stdexcept>
using namespace liminal;
using json=nlohmann::json;
#define CHECK(c) do {if(!(c))throw std::runtime_error(std::string("line ")+std::to_string(__LINE__)+": "+#c);} while(false)
const std::string header="room \"geometry\"\ncamera eye(0,1.7,-10) target(0,1.7,5) up(0,1,0) fov(50)\n"
    "plane \"floor\" pos(0,0,0) normal(0,1,0) size(100,100) gray(.2)\n";
const std::string pair="box \"a\" pos(-.5,1,5) size(3,2,1) gray(.25)\nbox \"b\" pos(.5,1,5) size(3,2,1) gray(.75)\n";
Scene Load(const std::string& t){Scene s;char e[256]={};CHECK(LoadSceneFromSceneText("test",t.c_str(),&s,e,sizeof(e)));return s;}
bool Reason(const CoplanarCleanupResult& r,const std::string& reason) {
    const auto report=json::parse(r.report_json);
    for(const auto& t:report["trials"])if(t["reason"].get<std::string>().find(reason)!=std::string::npos)return true;
    return false;
}
int main() {
    try {
        CoplanarCleanupOptions analyze;analyze.apply_translations=false;
        // A small angular difference is not enough: far corners violate 1 mm.
        auto large=CleanupCoplanarFaces(header+
            "box \"a\" pos(-.5,1,80) size(120,2,1) gray(.25)\n"
            "box \"b\" pos(.5,1,80) size(120,2,1) rot(0,.05,0) gray(.75)\n",analyze);
        CHECK(large.success&&large.initial_conflicts==0);
        auto thin=CleanupCoplanarFaces(header+
            "box \"a\" pos(0,.05,5) size(2,.1,1) gray(.25)\n"
            "box \"b\" pos(1.9999995,.05,5) size(2,.1,1) gray(.75)\n",analyze);
        CHECK(thin.success&&thin.initial_conflicts==0);
        // World AABBs overlap, but these thin rotated rectangles do not.
        auto disjoint=CleanupCoplanarFaces(header+
            "box \"a\" pos(0,3,5) size(4,.2,1) rot(0,0,45) gray(.25)\n"
            "box \"b\" pos(-.212132,3.212132,5) size(4,.2,1) rot(0,0,45) gray(.75)\n",analyze);
        CHECK(disjoint.success&&disjoint.initial_conflicts==0);
        CHECK(json::parse(disjoint.report_json)["precise_polygon_tests"].get<int>()>0);
        // Only the back faces coincide. A second camera makes them relevant.
        const auto dorsal=header+"box \"a\" pos(-.5,1,5) size(3,2,1) gray(.25)\n"
            "box \"b\" pos(.5,1,4.5) size(3,2,2) gray(.75)\n";
        CHECK(CleanupCoplanarFaces(dorsal,analyze).initial_conflicts==0);
        Camera behind;behind.eye=Vec3(0,1.7f,20);analyze.extra_cameras={behind};
        CHECK(CleanupCoplanarFaces(dorsal,analyze).initial_conflicts>0);
        auto camera=CleanupCoplanarFaces(header+
            "box \"a\" pos(0,1.7,-9.492) size(3,2,1) gray(.25)\n"
            "box \"b\" pos(0,1.7,-9.492) size(3,2,1) gray(.75)\n");
        CHECK(camera.success&&Reason(camera,"camera_inside_mass"));CHECK(camera.remaining_conflicts==0);
        auto approach=CleanupCoplanarFaces(header+
            "box \"a\" pos(-.5,1,-2.495) size(3,2,1) gray(.25)\n"
            "box \"b\" pos(.5,1,-2.495) size(3,2,1) gray(.75)\n");
        CHECK(approach.success&&Reason(approach,"reserved_approach"));CHECK(approach.remaining_conflicts==0);
        auto floor=CleanupCoplanarFaces(header+
            "box \"a\" pos(-.5,.1,5) size(3,.2,1) gray(.25)\n"
            "box \"b\" pos(.5,.1,5.2) size(3,.2,1.3) gray(.75)\n");
        CHECK(floor.success&&Reason(floor,"floor_penetration")&&floor.remaining_conflicts==0);
        CHECK(floor.scene_text.find("plane \"floor\" pos(0,0,0)")!=std::string::npos);
        auto bounds=CleanupCoplanarFaces(header+
            "box \"a\" pos(255.5,1,5) size(1,2,3) gray(.25)\n"
            "box \"b\" pos(255.5,1,5.5) size(1,2,3) gray(.75)\n");
        CHECK(bounds.success&&Reason(bounds,"spatial_bounds")&&bounds.remaining_conflicts==0);
        const auto rotated=MakeSceneBox("a",Vec3(0,5,8),Vec3(3,2,1),Vec3(12,23,17));
        const auto shifted=rotated.center+rotated.axes[0]*.7f;
        std::ostringstream xyz;xyz<<std::setprecision(9)<<shifted.x<<','<<shifted.y<<','<<shifted.z;
        auto tilted=CleanupCoplanarFaces(header+
            "box \"a\" pos(0,5,8) size(3,2,1) rot(12,23,17) gray(.25)\n"
            "box \"b\" pos("+xyz.str()+") size(3,2,1) rot(12,23,17) gray(.75)\n");
        if(!tilted.success||!tilted.initial_conflicts||tilted.remaining_conflicts)std::fprintf(stderr,"%s\n",tilted.report_json.c_str());
        CHECK(tilted.success&&tilted.initial_conflicts>0&&tilted.remaining_conflicts==0);
        const auto three=header+pair+"box \"c\" pos(.7,1,4.99) size(3,2,1) gray(.5)\n";
        const auto global=CleanupCoplanarFaces(three);
        CHECK(global.success&&Reason(global,"new_coplanar_conflict"));CHECK(global.remaining_conflicts==0);
        const auto report=json::parse(global.report_json);
        std::map<std::string,double> cumulative;
        for(const auto& m:report["moves"]) {
            const auto d=m["delta"];const double length=std::sqrt(d[0].get<double>()*d[0].get<double>()+
                d[1].get<double>()*d[1].get<double>()+d[2].get<double>()*d[2].get<double>());
            cumulative[m["block"].get<std::string>()]+=length;CHECK(m["conflicts_after"]<m["conflicts_before"]);
        }
        for(const auto& c:cumulative)CHECK(c.second<=.050001);
        // One immutable geometry covers all eight real breathing poses.
        auto source=Load(header+pair);const auto b=BuildBreathingPoseB(source.camera,AnimatedViewConfig());
        CoplanarCleanupOptions breathing;
        for(int i=1;i<8;++i)breathing.extra_cameras.push_back(CalculateAnimatedCameraPose(source.camera,b,i));
        const auto fixed=CleanupCoplanarFaces(header+pair,breathing);CHECK(fixed.remaining_conflicts==0);
        CHECK(CleanupCoplanarFaces(fixed.scene_text,breathing).scene_text==fixed.scene_text);
        // A valid marker prevents residual retries even if later options differ.
        CoplanarCleanupOptions blocked;blocked.fixed_ids={"a","b"};
        auto residual=CleanupCoplanarFaces(header+pair,blocked);
        CHECK(residual.remaining_conflicts>0);auto repeated=CleanupCoplanarFaces(residual.scene_text);
        CHECK(repeated.accepted_translations==0&&repeated.scene_text==residual.scene_text);
        // Existing glass is stripped before an opaque edit, then rebuilt from
        // corrected positions. A stale support must never survive that edit.
        const auto floating=header+"box \"a\" pos(-.5,4,5) size(3,2,2) gray(.25)\n"
            "box \"b\" pos(.5,4,5) size(3,2,2) gray(.75)\n";
        const auto old_glass=BuildGlassSupports(floating);CHECK(old_glass.added_supports>0);
        const auto clean=CleanupCoplanarFaces(old_glass.scene_text);CHECK(clean.accepted_translations>0);
        CHECK(clean.scene_text.find("engine_glass_triangle")==std::string::npos);
        const auto new_glass=BuildGlassSupports(clean.scene_text);CHECK(new_glass.added_supports>0);
        CHECK(new_glass.opaque_scene_text==clean.scene_text);
        CHECK(SceneTextHash(new_glass.scene_text)!=SceneTextHash(old_glass.scene_text));
        const auto again=CleanupCoplanarFaces(new_glass.scene_text);CHECK(again.scene_text==clean.scene_text&&again.accepted_translations==0);
        // Materials, rotations and sizes retain their exact source tokens.
        CHECK(clean.scene_text.find("size(3,2,2) gray(.25)")!=std::string::npos);
        CHECK(clean.scene_text.find("size(3,2,2) gray(.75)")!=std::string::npos);
        std::puts("Coplanar geometry: plane distances, thin overlaps, cameras, approaches, global rollback, breathing, budgets and glass ordering passed");return 0;
    }catch(const std::exception& e){std::fprintf(stderr,"%s\n",e.what());return 1;}
}
