#include "known_barrier.h"
#include "nlohmann/json.hpp"
#include <cmath>
#include <iomanip>
#include <sstream>

namespace liminal {
namespace {
const std::string begin="\n# engine-known-obstructions-v1 begin\n";
const std::string end="# engine-known-obstructions-v1 end\n";
bool Cardinal(CardinalDirection d) {return d>=kDirectionNorth&&d<=kDirectionWest;}
CardinalDirection Facing(const SessionState& state) {
    if(state.current_place_id=="canonical:quarry_threshold")return kDirectionEast;
    for(const auto& room:state.generated_rooms)if(room.room_id==state.current_place_id) {
        const auto provenance=nlohmann::json::parse(room.scene_design_json,nullptr,false);
        if(!provenance.is_object())return kDirectionUnknown;
        for(const auto* key:{"forward_direction","return_direction"})if(provenance.contains(key)&&provenance[key].is_string()) {
            CardinalDirection direction=kDirectionUnknown;
            if(ParseCardinalDirection(provenance[key].get<std::string>().c_str(),&direction)&&Cardinal(direction))
                return std::string(key)=="return_direction"?OppositeCardinalDirection(direction):direction;
        }
    }
    return kDirectionUnknown;
}
}
bool HasKnownBarrier(const SessionState& state,CardinalDirection direction) {
    if(!Cardinal(direction))return false;
    for(const auto& b:state.invisible_barriers)
        if(b.place_id==state.current_place_id&&b.direction==direction&&b.discovered)return true;
    return false;
}
BarrierViewSide KnownBarrierSide(const SessionState& state,CardinalDirection direction) {
    const auto facing=Facing(state);
    if(!Cardinal(facing)||!Cardinal(direction))return BarrierViewSide::unknown;
    const BarrierViewSide sides[]={BarrierViewSide::front,BarrierViewSide::right,BarrierViewSide::back,BarrierViewSide::left};
    return sides[(static_cast<int>(direction)-static_cast<int>(facing)+4)%4];
}
bool BuildKnownBarrierScene(const std::string& base,const SessionState& state,
    std::string* composed,Scene* scene,std::string* error) {
    if(!composed||!scene){if(error)*error="missing barrier render target";return false;}
    // This suffix belongs exclusively to the engine. Rebuilding never stacks it.
    std::string program=base;
    const auto start=program.rfind(begin);
    if(start!=std::string::npos&&program.size()>=end.size()&&
       program.compare(program.size()-end.size(),end.size(),end)==0)program.resize(start);
    Scene source;char why[512]={};
    if(!LoadSceneFromSceneText("known-obstruction-base",program.c_str(),&source,why,sizeof(why))) {
        if(error)*error=why;return false;
    }
    const auto forward=Normalize(Vec3(source.camera.target.x-source.camera.eye.x,0,source.camera.target.z-source.camera.eye.z));
    const auto right=Normalize(Cross(forward,Vec3(0,1,0)));
    const float yaw=std::atan2(forward.x,forward.z)*180.f/3.14159265f;
    std::ostringstream layer;layer.imbue(std::locale::classic());layer<<std::fixed<<std::setprecision(5);
    bool added=false;
    for(const auto d:{kDirectionNorth,kDirectionEast,kDirectionSouth,kDirectionWest}) {
        if(!HasKnownBarrier(state,d))continue;
        const auto side=KnownBarrierSide(state,d);
        if(side==BarrierViewSide::unknown||side==BarrierViewSide::back)continue;
        // Near-field slivers make the plane readable without replacing the vista.
        // Thin glass supplies its own reflection/refraction, with no opaque frame.
        Vec3 center,size;float rotation=yaw;
        if(side==BarrierViewSide::front) {
            center=source.camera.eye+forward*3.1f+right*1.1f;
            size=Vec3(1.9f,2.8f,.065f);
        } else {
            const float sign=side==BarrierViewSide::right?1.f:-1.f;
            center=source.camera.eye+forward*3.3f+right*(sign*1.75f);
            size=Vec3(.065f,2.8f,2.8f);
        }
        // Keep the base near the local floor while allowing authored elevated eyes.
        center.y=source.camera.eye.y-1.7f+size.y*.5f;
        layer<<"glass_box \"known_obstruction_"<<CardinalDirectionToString(d)<<"\" pos("
             <<center.x<<','<<center.y<<','<<center.z<<") size("<<size.x<<','<<size.y<<','<<size.z
             <<") rot(0,"<<rotation<<",0) gray(0.85)\n";
        added=true;
    }
    if(added) {
        program+=begin+layer.str()+end;
        Scene rendered;
        if(!LoadSceneFromSceneText("known-obstructions",program.c_str(),&rendered,why,sizeof(why))) {
            if(error)*error=why;return false;
        }
        *scene=std::move(rendered);
    } else *scene=std::move(source);
    *composed=std::move(program);if(error)error->clear();return true;
}
}
