#include "object_interactions.h"
#include "animated_view.h"
#include "renderer.h"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <set>
#include <sstream>

namespace liminal {
namespace {
bool Visible(const Scene& scene,const Vec3& point) {
    const auto pose_b=BuildBreathingPoseB(scene.camera,AnimatedViewConfig());
    for(int i=0;i<kAnimatedViewImageCapacity;++i) {
        const auto camera=CalculateAnimatedCameraPose(scene.camera,pose_b,i);
        const auto forward=Normalize(camera.target-camera.eye);
        const auto right=Normalize(Cross(forward,camera.up));
        const auto up=Normalize(Cross(right,forward));
        const auto delta=point-camera.eye;const float depth=Dot(delta,forward);
        const float limit=std::tan(camera.vertical_fov_degrees*3.14159265f/360.f)*depth*.82f;
        if(depth<=0||std::abs(Dot(delta,up))>limit||std::abs(Dot(delta,right))>limit*1.6f)return false;
        // Normalized clear areas of assets/hud-mask.png and the compiled HUD.
        // The renderer uses 16:9 or 2:1 images in the fixed 2:1 SDL viewport;
        // these conservative patches remain clear at both aspect ratios.
        const float scale=std::tan(camera.vertical_fov_degrees*3.14159265f/360.f)*depth;
        const float x=.5f+Dot(delta,right)/(4.f*scale),y=.5f-Dot(delta,up)/(2.f*scale);
        const bool upper=y>.14f&&y<.66f&&x>.12f&&x<.88f;
        const bool left_lower=x>.16f&&x<.34f&&y>.835f&&y<.885f;
        const bool right_lower=x>.81f&&x<.835f&&y>.70f&&y<.865f;
        if(!upper&&!left_lower&&!right_lower)return false;
        Hit hit;
        if(ProbeSceneRay(scene,{camera.eye,Normalize(delta)},Length(delta)-.03f,&hit))return false;
    }
    return true;
}
} // namespace
bool FindObjectPlacement(const Scene& scene,const SessionState& state,ObjectPlacement* placement) {
    const auto forward=Normalize(Vec3(scene.camera.target.x-scene.camera.eye.x,0,scene.camera.target.z-scene.camera.eye.z));
    const auto right=Normalize(Cross(forward,Vec3(0,1,0)));
    // A bounded local floor patch, visible even with the slightly upward authored
    // threshold camera. No distant surface or hidden placement is accepted.
    auto try_placement=[&](float distance,float offset,ObjectPlacement* target) {
        const auto above=scene.camera.eye+forward*distance+right*offset;
        Hit floor;
        if(!ProbeSceneRay(scene,{above,Vec3(0,-1,0)},2.2f,&floor)||floor.normal.y<.98f)return false;
        const auto point=floor.position+Vec3(0,.10f,0);
        if(!Visible(scene,point))return false;
        bool free=true;
        for(const auto& box:scene.boxes) {
            // Conservative overlap test for the complete largest mobile footprint.
            const auto mobile=MakeSceneBox("candidate",point,Vec3(.44f,.18f,.34f),Vec3(0));
            if(SceneBoxesTouch(mobile,box,0.f)){free=false;break;}
        }
        // Test the complete supporting footprint, not only its centre ray.
        for(float dx:{-.23f,.23f})for(float dz:{-.18f,.18f}) {
            Hit support;
            if(!ProbeSceneRay(scene,{floor.position+Vec3(dx,.3f,dz),Vec3(0,-1,0)},.4f,&support)||
               support.normal.y<.98f||std::abs(support.position.y-floor.position.y)>.015f)free=false;
        }
        auto room=state.objects.deposits.find(state.current_place_id);
        if(room!=state.objects.deposits.end())for(const auto& p:room->second)
            if(Length(Vec3(p.x,p.y,p.z)-floor.position)<.6f)free=false;
        if(!free)return false;
        target->x=floor.position.x;target->y=floor.position.y;target->z=floor.position.z;return true;
    };
    for(int step=0;step<=30;++step)for(int side=0;side<=22;++side) {
        const float distance=2.4f+.1f*step;
        const float offset=(1.f+.2f*(side/2))*(side%2?1.f:-1.f);
        ObjectPlacement nearest;
        if(!try_placement(distance,offset,&nearest))continue;
        // Give the first visible position 20 cm of breathing room from the
        // visor edge. Recheck visibility, support and collisions at that point.
        if(!try_placement(distance+.2f,offset,placement))*placement=nearest;
        return true;
    }
    return false;
}
bool BuildObjectScene(const std::string& base,const SessionState& state,std::string* composed,Scene* scene,std::string* error) {
    std::set<std::string> removed;
    for(const auto& source:state.objects.sources)if(source.place_id==state.current_place_id&&!source.object_id.empty()&&!source.visual_id.empty())removed.insert(source.visual_id);
    const auto deposit=state.objects.deposits.find(state.current_place_id);
    if(removed.empty()&&(deposit==state.objects.deposits.end()||deposit->second.empty())) {
        char buffer[512]={};
        if(!LoadSceneFromSceneText("objects.scene",base.c_str(),scene,buffer,sizeof(buffer))){if(error)*error=buffer;return false;}
        if(composed)*composed=base;return true;
    }
    std::istringstream input(base);std::ostringstream out;out.imbue(std::locale::classic());
    std::string line;
    while(std::getline(input,line)) {
        bool skip=false;
        for(const auto& id:removed)if(line.find("box \""+id+"\"")!=std::string::npos)skip=true;
        if(!skip)out<<line<<'\n';
    }
    out<<std::fixed<<std::setprecision(5);
    auto room=state.objects.deposits.find(state.current_place_id);
    if(room!=state.objects.deposits.end())for(const auto& p:room->second) {
        size_t index=0;
        // Names are generated by the engine: no model string enters scene syntax.
        auto box=[&](float dx,float dy,float dz,float sx,float sy,float sz,float gray) {
            out<<"box \""<<ObjectVisualName(p.object_id)<<"_"<<index++<<"\" pos("<<p.x+dx<<","<<p.y+dy<<","<<p.z+dz
               <<") size("<<sx<<","<<sy<<","<<sz<<") gray("<<gray<<")\n";
        };
        if(p.object_id=="boundary chalk")box(0,.045f,0,.25f,.09f,.09f,.97f);
        else if(p.object_id=="survey map slate") {
            box(0,.035f,0,.42f,.07f,.30f,.28f);box(0,.073f,0,.34f,.006f,.22f,.8f);
        } else if(p.object_id=="sample tongs") {
            box(-.065f,.035f,0,.035f,.07f,.30f,.8f);box(.065f,.035f,0,.035f,.07f,.30f,.8f);
            box(0,.035f,-.13f,.16f,.07f,.04f,.7f);
        } else box(0,.08f,0,.19f,.16f,.15f,.72f);
    }
    char buffer[512]={};const auto text=out.str();
    if(!LoadSceneFromSceneText("objects.scene",text.c_str(),scene,buffer,sizeof(buffer))){if(error)*error=buffer;return false;}
    if(composed)*composed=text;return true;
}
} // namespace liminal
