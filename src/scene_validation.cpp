#include "scene_design.h"
#include "renderer.h"
#include <cmath>
#include <cstdlib>
#include <map>
#include <limits>
#include <regex>
#include <set>
#include <sstream>

namespace liminal {
namespace {
struct Directive { std::string type,name; std::map<std::string,std::vector<float>> p; };
struct Property { int arity; bool required; };
using Properties=std::map<std::string,Property>;

void Error(SceneAuditReport* r,const std::string& code,const std::string& id,const std::string& detail) {
    for(const auto& old:r->diagnostics)if(old.code==code&&old.object_id==id&&old.detail==detail)return;
    if(r->diagnostics.size()<24)r->diagnostics.push_back({code,id,detail});
}
bool Prefix(const std::string& name,const std::string& id) {
    return name==id || name.compare(0,id.size()+1,id+"_")==0;
}
const DesignMass* Group(const SceneDesignV1* d,const std::string& name) {
    const DesignMass* best=nullptr;
    if(d)for(const auto& m:d->masses)
        if(Prefix(name,m.id)&&(!best||m.id.size()>best->id.size()))best=&m;
    return best;
}
Vec3 InverseRotate(Vec3 v,const Vec3& degrees) {
    const Vec3 a=degrees*(-kPi/180.0f);
    v=Vec3(cosf(a.z)*v.x-sinf(a.z)*v.y,sinf(a.z)*v.x+cosf(a.z)*v.y,v.z);
    v=Vec3(cosf(a.y)*v.x+sinf(a.y)*v.z,v.y,-sinf(a.y)*v.x+cosf(a.y)*v.z);
    return Vec3(v.x,cosf(a.x)*v.y-sinf(a.x)*v.z,sinf(a.x)*v.y+cosf(a.x)*v.z);
}
bool InsideBounds(const Vec3& p,const Vec3& c,const Vec3& size,float epsilon=.15f) {
    const Vec3 v=p-c;
    return fabsf(v.x)<=size.x*.5f+epsilon&&fabsf(v.y)<=size.y*.5f+epsilon&&fabsf(v.z)<=size.z*.5f+epsilon;
}
bool RayTriangle(const Vec3& o,const Vec3& dir,const Triangle& t,float* distance) {
    const Vec3 a=t.v1-t.v0,b=t.v2-t.v0,p=Cross(dir,b);const float det=Dot(a,p);
    if(fabsf(det)<1e-8f)return false;
    const float inv=1/det;const Vec3 s=o-t.v0;
    const float u=Dot(s,p)*inv;if(u<0||u>1)return false;
    const Vec3 q=Cross(s,a);const float v=Dot(dir,q)*inv;
    if(v<0||u+v>1)return false;
    *distance=Dot(b,q)*inv;return *distance>1e-5f;
}
bool InsideSolid(const Vec3& point,const std::vector<const Triangle*>& triangles) {
    if(triangles.size()<4)return false; // A standalone plane has no solid interior.
    std::vector<float> hits;
    const Vec3 direction=Normalize(Vec3(.371f,.597f,.711f));
    for(const auto* t:triangles){float d;if(RayTriangle(point,direction,*t,&d))hits.push_back(d);}
    std::sort(hits.begin(),hits.end());size_t crossings=0;float previous=-1;
    for(float d:hits)if(previous<0||fabsf(d-previous)>.001f){++crossings;previous=d;}
    return crossings%2!=0;
}
bool TriangleBox(const Triangle& triangle,const Vec3& center,const Vec3& size) {
    const Vec3 v[3]={triangle.v0-center,triangle.v1-center,triangle.v2-center};
    const Vec3 e[3]={v[1]-v[0],v[2]-v[1],v[0]-v[2]};
    const Vec3 basis[3]={Vec3(1,0,0),Vec3(0,1,0),Vec3(0,0,1)};
    std::vector<Vec3> axes={basis[0],basis[1],basis[2],Cross(e[0],e[1])};
    for(const auto& edge:e)for(const auto& b:basis)axes.push_back(Cross(edge,b));
    for(const auto& a:axes) {
        if(LengthSquared(a)<1e-12f)continue;
        const float r=.5f*(size.x*fabsf(a.x)+size.y*fabsf(a.y)+size.z*fabsf(a.z));
        const float p0=Dot(a,v[0]),p1=Dot(a,v[1]),p2=Dot(a,v[2]);
        if(std::min(p0,std::min(p1,p2))>r+.0001f || std::max(p0,std::max(p1,p2))<-r-.0001f)return false;
    }
    return true;
}
bool PointTouchesTriangle(const Vec3& p,const Triangle& t) {
    if(fabsf(Dot(p-t.v0,t.normal))>.15f)return false;
    const Vec3 q=p-t.normal*Dot(p-t.v0,t.normal);
    const Vec3 a=t.v1-t.v0,b=t.v2-t.v0,c=q-t.v0;
    const float aa=Dot(a,a),ab=Dot(a,b),bb=Dot(b,b),ac=Dot(a,c),bc=Dot(b,c);
    const float denominator=aa*bb-ab*ab;
    if(fabsf(denominator)<1e-10f)return false;
    const float u=(bb*ac-ab*bc)/denominator,v=(aa*bc-ab*ac)/denominator;
    return u>=-.001f&&v>=-.001f&&u+v<=1.001f;
}
bool Touch(const std::vector<const Triangle*>& a,const std::vector<const Triangle*>& b) {
    for(const auto* t:a)for(const auto* u:b) {
        if(PointTouchesTriangle(t->v0,*u)||PointTouchesTriangle(t->v1,*u)||PointTouchesTriangle(t->v2,*u)||
           PointTouchesTriangle(u->v0,*t)||PointTouchesTriangle(u->v1,*t)||PointTouchesTriangle(u->v2,*t))return true;
        const Vec3 p[3]={t->v0,t->v1,t->v2},q[3]={u->v0,u->v1,u->v2};
        for(int i=0;i<3;++i) {
            float distance;
            if(RayTriangle(p[i],p[(i+1)%3]-p[i],*u,&distance)&&distance<=1.0001f)return true;
            if(RayTriangle(q[i],q[(i+1)%3]-q[i],*t,&distance)&&distance<=1.0001f)return true;
        }
    }
    return false;
}

bool ParseProgram(const std::string& text,std::vector<Directive>* output,SceneAuditReport* r) {
    if(text.empty()||text.size()>65536){Error(r,"scene_text_budget","","Expected 1..65536 bytes");return false;}
    const std::set<std::string> prefabs={"prefab_gate","prefab_crate","prefab_survey_beacon","prefab_crystal_scanner",
        "prefab_crystal_cluster","prefab_extraction_rig","prefab_prospect_shelter","prefab_quarry_pylon",
        "prefab_atmospheric_processor","prefab_cactus_sentinel","prefab_cactus_fork","prefab_cactus_cluster",
        "prefab_rock_low","prefab_rock_wide","prefab_rock_tall","prefab_rock_spire"};
    const std::regex line_pattern(R"rx(^\s*([a-z_]+)(?:\s+"([^"\r\n]*)")?\s*(.*)$)rx");
    const std::regex property_pattern(R"rx(^\s*([a-z_]+)\(([^()]*)\)\s*)rx");
    const std::regex name_pattern("[A-Za-z][A-Za-z0-9_]{0,63}");
    std::istringstream input(text);std::string line;std::set<std::string> names,singletons;
    size_t prefab_count=0;
    while(std::getline(input,line)) {
        if(!line.empty()&&line.back()=='\r')line.pop_back(); // Windows-authored scene files.
        const size_t comment=line.find('#');if(comment!=std::string::npos)line.resize(comment);
        if(line.find_first_not_of(" \t\r")==std::string::npos)continue;
        auto parse_line=[&]() -> bool {
        if(line.size()>1023){Error(r,"scene_line_budget","","Line exceeds 1023 bytes");return false;}
        std::smatch match;
        if(!std::regex_match(line,match,line_pattern)){Error(r,"scene_syntax","",line.substr(0,120));return false;}
        Directive d;d.type=match[1];d.name=match[2];std::string rest=match[3];
        Properties props;bool geometry=false;
        if(d.type=="room") {if(d.name.empty()){Error(r,"room_name","","Room needs a quoted name");return false;}}
        else if(d.type=="camera")props={{"eye",{3,true}},{"target",{3,true}},{"up",{3,false}},{"fov",{1,false}}};
        else if(d.type=="sky")props={{"zenith",{1,true}},{"horizon",{1,true}},{"nadir",{1,true}},{"band",{1,true}},
            {"curve",{1,true}},{"noise",{1,true}},{"stars",{3,false}},{"seed",{1,false}}};
        else if(d.type=="distant_light")props={{"direction",{3,true}},{"intensity",{1,true}},{"color",{3,false}},{"angular_radius",{1,false}}};
        else if(d.type=="spotlight")props={{"panel",{2,true}},{"offset",{3,true}},{"range",{1,true}},{"cone",{2,true}},{"intensity",{1,true}}};
        else if(d.type=="box"||d.type=="glass_box"||d.type=="plane"||prefabs.count(d.type)) {
            geometry=true;props={{"pos",{3,true}},{"size",{d.type=="plane"?2:3,true}},{"gray",{1,true}}};
            if(d.type=="box"||d.type=="glass_box"){props["rot"]={3,false};props["emit"]={1,false};}
            else if(d.type=="plane"){props["normal"]={3,true};props["emit"]={1,false};}
            else {++prefab_count;props["detail"]={1,false};if(d.type=="prefab_gate")props["bars"]={1,false};
                if(d.type=="prefab_crystal_cluster"||d.type=="prefab_crystal_scanner")props["glow"]={1,false};}
            if(!std::regex_match(d.name,name_pattern)||!names.insert(d.name).second){
                Error(r,"object_identifier",d.name,"Unique ASCII geometry name required");return false;}
            ++r->geometry_directives;
        } else {Error(r,"unknown_directive",d.type,"Directive is not in dynamic scene v1");return false;}
        if(!geometry && !singletons.insert(d.type).second){Error(r,"duplicate_directive",d.type,"Singleton declared twice");return false;}
        if(!geometry && d.type!="room"&&!d.name.empty()){Error(r,"unexpected_name",d.type,"Only geometry and room have names");return false;}
        while(!rest.empty()) {
            if(rest.find_first_not_of(" \t\r")==std::string::npos)break;
            std::smatch property;
            if(!std::regex_search(rest,property,property_pattern)){Error(r,"scene_syntax",d.name,rest.substr(0,120));return false;}
            const std::string key=property[1];const auto spec=props.find(key);
            if(spec==props.end()||d.p.count(key)){Error(r,"unknown_or_duplicate_property",d.name,key);return false;}
            const std::string value_text=property[2];
            if(value_text.empty() || value_text.find_last_not_of(" \t\r")==std::string::npos ||
               value_text[value_text.find_last_not_of(" \t\r")]==',') {
                Error(r,"invalid_number",d.name,key);return false;
            }
            std::istringstream values(value_text);std::string token;std::vector<float> numbers;
            while(std::getline(values,token,',')) {
                char* end=nullptr;const float value=strtof(token.c_str(),&end);
                if(end==token.c_str()||!std::isfinite(value)){Error(r,"non_finite_or_invalid_number",d.name,key);return false;}
                while(*end==' '||*end=='\t'||*end=='\r')++end;
                if(*end){Error(r,"invalid_number",d.name,key);return false;}numbers.push_back(value);
            }
            if(numbers.size()!=static_cast<size_t>(spec->second.arity)){Error(r,"property_arity",d.name,key);return false;}
            for(float value:numbers) {
                bool ok=true;
                if(key=="pos"||key=="eye"||key=="target"||key=="offset")ok=fabsf(value)<=256;
                else if(key=="size"||key=="panel"||key=="range")ok=value>0&&value<=256;
                else if(key=="gray"||key=="detail")ok=value>=0&&value<=.95f;
                else if(key=="emit"||key=="glow")ok=value>=0&&value<=20;
                else if(key=="bars")ok=value>=1&&value<=12&&value==floorf(value);
                else if(key=="rot")ok=fabsf(value)<=180;
                else if(key=="seed")ok=value>=0&&value<=1000000000&&value==floorf(value);
                else if(key=="intensity")ok=value>=0&&value<=500;
                else if(key=="color")ok=value>=0&&value<=1;
                else if(key=="angular_radius")ok=value>=0&&value<=10;
                else if(key=="fov")ok=value>=25&&value<=85;
                else if(key=="normal"||key=="up"||key=="direction")ok=fabsf(value)<=256;
                else if(key=="cone")ok=value>0&&value<90;
                else if(key=="band")ok=value>0&&value<=1;
                else if(key=="curve")ok=value>0&&value<=10;
                else if(key=="stars")ok=value>=0&&value<=2;
                else ok=value>=0&&value<=2;
                if(!ok){Error(r,"property_range",d.name.empty()?d.type:d.name,
                    key+" observed="+std::to_string(value)+
                    (key=="fov"?"; expected vertical degrees in [25,85]":key=="gray"?"; expected [0,0.95]":"; outside the authoring limits"));return false;}
            }
            d.p[key]=numbers;rest=property.suffix();
        }
        bool missing=false;
        for(const auto& p:props)if(p.second.required&&!d.p.count(p.first)){
            Error(r,"missing_property",d.name.empty()?d.type:d.name,
                "Required "+p.first+"() with "+std::to_string(p.second.arity)+" numeric values"+
                (p.first=="gray"?"; emissive primitives also need gray(0..0.95)":""));missing=true;}
        if(missing)return false;
        if(d.type=="plane"||(d.type=="camera"&&d.p.count("up"))) {
            const auto& v=d.p[d.type=="plane"?"normal":"up"];
            if(v[0]*v[0]+v[1]*v[1]+v[2]*v[2]<.0001f){Error(r,"degenerate_basis",d.name,"Zero normal/up");return false;}
        }
        if(r->geometry_directives>96||prefab_count>8){Error(r,"geometry_budget","","At most 96 primitives and 8 prefabs");return false;}
        output->push_back(d);
        return true;
        };
        parse_line(); // Gather independent line failures for one bounded repair.
    }
    if(!singletons.count("room")||!singletons.count("camera")||!singletons.count("sky")){
        Error(r,"missing_directive","","room, camera and sky are mandatory");return false;}
    return r->diagnostics.empty();
}
}

std::vector<std::string> LocalSceneClearApproaches(const Scene& scene) {
    std::map<int,std::vector<const Triangle*>> solids;
    for(const auto& t:scene.triangles)solids[t.material_index].push_back(&t);
    struct Approach { const char* name; Vec3 center,size; };
    const Approach approaches[]={
        {"back",Vec3(0,1.25f,-9),Vec3(1.6f,2,6)},
        {"forward",Vec3(0,1.25f,-6),Vec3(1.6f,2,6)},
        {"left",Vec3(3,1.25f,-10),Vec3(6,2,1.6f)},
        {"right",Vec3(-3,1.25f,-10),Vec3(6,2,1.6f)}};
    std::vector<std::string> clear;
    for(const auto& approach:approaches) {
        bool blocked=false;
        for(const auto& solid:solids) {
            if(InsideSolid(approach.center,solid.second)){blocked=true;break;}
            for(const auto* t:solid.second)if(TriangleBox(*t,approach.center,approach.size)){blocked=true;break;}
            if(blocked)break;
        }
        // Sample supporting floor along the approach, including both ends.
        for(int sample=-1;sample<=1&&!blocked;++sample) {
            Ray ray;ray.origin=Vec3(approach.center.x,1.7f,approach.center.z);
            if(approach.size.x>approach.size.z)ray.origin.x+=sample*approach.size.x*.45f;
            else ray.origin.z+=sample*approach.size.z*.45f;
            ray.direction=Vec3(0,-1,0);Hit hit;
            if(!ProbeSceneRay(scene,ray,6,&hit)||hit.t<.35f)blocked=true;
        }
        if(!blocked)clear.push_back(approach.name);
    }
    return clear;
}

bool AuditDynamicScene(const std::string& program,const SceneDesignContext& context,
                       const SceneDesignV1* design,Scene* output,SceneAuditReport* r,
                       const SceneViewConstraints* view) {
    *r=SceneAuditReport();std::vector<Directive> directives;
    const SceneViewConstraints limits = view ? *view : SceneViewConstraints();
    if(!std::isfinite(limits.aspect)||limits.aspect<=0 ||
       !std::isfinite(limits.near_distance)||limits.near_distance<=0 ||
       !std::isfinite(limits.max_near_coverage)||limits.max_near_coverage<0||limits.max_near_coverage>1) {
        Error(r,"invalid_view_contract","","Invalid aspect or near-view threshold");return false;
    }
    if(!ParseProgram(program,&directives,r))return false;
    if(design)for(const auto& d:directives)
        if(!d.name.empty()&&d.type!="room"&&!Group(design,d.name))Error(r,"unbound_geometry",d.name,"Use a planned group ID prefix");
    if(!r->diagnostics.empty())return false;
    Scene scene;char error[512]={};
    if(!LoadSceneFromSceneText("dynamic.scene",program.c_str(),&scene,error,sizeof(error))){
        Error(r,"scene_parser","",error);return false;}
    r->triangles=scene.triangles.size();r->materials=scene.materials.size();
    if(r->triangles>8192||r->materials>512){Error(r,"expanded_geometry_budget_exceeded","","Limit: 8192 triangles / 512 materials");return false;}
    const Camera& camera=scene.camera;
    const Vec3 forward=camera.target-camera.eye;
    if(Length(forward)<.1f||Length(camera.up)<.1f||Length(Cross(Normalize(forward),Normalize(camera.up)))<.1f)
        Error(r,"degenerate_camera","","Camera forward/up must form a basis");
    if(design && (Length(camera.eye-design->camera.eye)>.02f||Length(camera.target-design->camera.target)>.02f||
        Length(camera.up-design->camera.up)>.02f||fabsf(camera.vertical_fov_degrees-design->camera.vertical_fov_degrees)>.02f))
        Error(r,"camera_plan_mismatch","","Preserve the accepted camera exactly");
    std::map<int,std::vector<const Triangle*>> solids;
    std::map<std::string,std::vector<const Triangle*>> groups;
    for(const auto& t:scene.triangles) {
        solids[t.material_index].push_back(&t);
        const std::string& name=scene.materials[t.material_index].name;
        const auto* group=Group(design,name);if(group)groups[group->id].push_back(&t);
        for(const auto& p:{t.v0,t.v1,t.v2}) {
            if(!InsideBounds(p,Vec3(0),Vec3(512)))Error(r,"expanded_geometry_bounds",name,"Expanded geometry exceeds local coordinate budget");
            if(design&&!InsideBounds(p,design->bounds_center,design->bounds_size))
                Error(r,"place_bounds_mismatch",name,"Child vertex outside accepted place bounds");
            if(group&&!InsideBounds(InverseRotate(p-group->center,group->rotation),Vec3(0),group->size))
                Error(r,"group_envelope_mismatch",group->id,"Child vertex outside the accepted oriented envelope");
        }
    }
    for(const auto& solid:solids)if(InsideSolid(camera.eye,solid.second))
        Error(r,"camera_inside_mass",scene.materials[solid.first].name,"Camera is inside actual closed geometry");
    Ray floor_probe;floor_probe.origin=camera.eye;floor_probe.direction=Vec3(0,-1,0);Hit floor_hit;
    if(!ProbeSceneRay(scene,floor_probe,6.0f,&floor_hit) || floor_hit.t<.35f)
        Error(r,"camera_without_support","","Camera needs a supporting surface 0.35..6 m below the eye");
    if(design) {
        for(const auto& m:design->masses) {
            const auto found=groups.find(m.id);
            if(found==groups.end()){Error(r,"missing_group",m.id,"No geometry for planned group");continue;}
            if(m.role=="ground"||m.support=="none")continue;
            if(m.support=="ground") {
                float bottom=kHuge;
                for(const auto* t:found->second)bottom=std::min(bottom,std::min(t->v0.y,std::min(t->v1.y,t->v2.y)));
                if(fabsf(bottom-design->floor_y)>.25f)Error(r,"unsupported_mass",m.id,
                    "Geometry bottom="+std::to_string(bottom)+" must meet floor_y="+std::to_string(design->floor_y));
            } else if(!Touch(found->second,groups[m.support]))
                Error(r,"unsupported_mass",m.id,"No surface contact with declared support "+m.support);
        }
        for(const auto& p:design->reserved_passages) {
            bool blocked=false;std::string blocker;
            for(const auto& t:scene.triangles)if(TriangleBox(t,p.center,p.size)){
                blocked=true;blocker=scene.materials[t.material_index].name;break;
            }
            if(!blocked)for(const auto& s:solids)if(InsideSolid(p.center,s.second)){
                blocked=true;blocker=scene.materials[s.first].name;break;
            }
            if(blocked)Error(r,"reserved_passage_blocked",p.direction,"Geometry "+blocker+" occupies reserved clearance; split or relocate that part within its group");
            Ray down;down.origin=Vec3(p.center.x,design->floor_y+.5f,p.center.z);down.direction=Vec3(0,-1,0);
            Hit h;if(!ProbeSceneRay(scene,down,1.0f,&h)||fabsf(h.position.y-design->floor_y)>.3f)
                Error(r,"passage_without_floor",p.direction,"No supporting floor near the declared reference level");
        }
    }
    if(!r->diagnostics.empty())return false;
    const Vec3 f=Normalize(forward),right=Normalize(Cross(f,camera.up)),up=Normalize(Cross(right,f));
    if(limits.require_return_clearance) {
        const Vec3 center(0,1.25f,-9),size(1.6f,2.0f,6.0f);
        for(const auto& solid:solids) {
            bool blocked=InsideSolid(center,solid.second);
            for(const auto* triangle:solid.second)if(TriangleBox(*triangle,center,size)){blocked=true;break;}
            if(blocked) {
                Aabb bounds;
                for(const auto* t:solid.second){bounds.Expand(t->v0);bounds.Expand(t->v1);bounds.Expand(t->v2);}
                const Vec3 half=(bounds.max-bounds.min)*.5f;
                Error(r,"return_clearance_blocked",scene.materials[solid.first].name,
                    "Keep the approach clear: x[-0.8,0.8], y[0.25,2.25], z[-12,-6]. "
                    "For the current dimensions and rotation, a sufficient translation places its centre at x < "+
                    std::to_string(-.85f-half.x)+" or x > "+std::to_string(.85f+half.x)+
                    ", OR z < "+std::to_string(-12.05f-half.z)+" or z > "+std::to_string(-5.95f+half.z)+
                    ". Keep other objects unchanged; a centre on the boundary does not clear the full solid.");
            }
        }
    }
    if(limits.require_return_clearance&&context.require_onward_exit) {
        const auto approaches=LocalSceneClearApproaches(scene);
        if(std::none_of(approaches.begin(),approaches.end(),[](const std::string& name){return name!="back";}))
            Error(r,"exploration_continuation_blocked","",
                "This is the last unexplored route. Keep supporting floor and clear space for at least one onward approach "
                "(forward x[-0.8,0.8] z[-9,-3], left x[0,6] z[-10.8,-9.2], or right x[-6,0] z[-10.8,-9.2], y[0.25,2.25]). Return alone is insufficient.");
    }
    const float scale=tanf(camera.vertical_fov_degrees*kPi/360.0f);
    constexpr int width=256,height=128;
    int focal=0,sky=0,near=0,landmark_hits=0;
    std::map<std::string,int> near_objects,blockers;
    r->object_names.push_back("");
    for(const auto& d:directives)if(d.type!="room"&&!d.name.empty())r->object_names.push_back(d.name);
    std::vector<uint16_t> material_ids(scene.materials.size(),0);
    for(size_t m=0;m<scene.materials.size();++m) {
        size_t longest=0;
        for(size_t id=1;id<r->object_names.size();++id)
            if(Prefix(scene.materials[m].name,r->object_names[id])&&r->object_names[id].size()>longest) {
                material_ids[m]=static_cast<uint16_t>(id);longest=r->object_names[id].size();
            }
    }
    const std::string focal_name=limits.landmark.empty()?(design?design->focal_object_id:""):limits.landmark;
    std::vector<const Triangle*> target_triangles;
    for(const auto& t:scene.triangles) {
        const auto& name=r->object_names[material_ids[t.material_index]];
        const auto* group=Group(design,name);
        if(name==focal_name||(group&&group->id==focal_name))target_triangles.push_back(&t);
    }
    if(!focal_name.empty()&&target_triangles.empty())Error(r,"missing_landmark",focal_name,"Required object is absent");
    r->object_ids.assign(width*height,0);
    r->depth.assign(width*height,std::numeric_limits<float>::infinity());
    for(int y=0;y<height;++y)for(int x=0;x<width;++x) {
        const int pixel=y*width+x;
        Ray ray;ray.origin=camera.eye;
        ray.direction=Normalize(f+right*((2*(x+.5f)/width-1)*limits.aspect*scale)+up*((1-2*(y+.5f)/height)*scale));
        float target_depth=kHuge;
        for(const auto* t:target_triangles) {float d;if(RayTriangle(ray.origin,ray.direction,*t,&d))target_depth=std::min(target_depth,d);}
        if(target_depth<kHuge)++r->landmark_projected_pixels;
        // Straight visibility through dielectric interfaces, not a refraction simulation.
        Hit hit;float travelled=0;bool opaque=false;
        for(size_t step=0;step<=scene.triangles.size();++step) {
            hit=Hit();
            if(!ProbeSceneRay(scene,ray,kHuge,&hit))break;
            const auto& triangle=scene.triangles[hit.triangle_index];
            if(scene.materials[triangle.material_index].model==kMaterialModelDiffuse) {opaque=true;break;}
            const float advance=hit.t+.0001f;travelled+=advance;ray.origin+=ray.direction*advance;
        }
        if(!opaque){++sky;continue;}
        const float distance=travelled+hit.t;
        const auto id=material_ids[scene.triangles[hit.triangle_index].material_index];
        const auto& name=r->object_names[id];
        r->object_ids[pixel]=id;r->depth[pixel]=distance;
        if(distance<limits.near_distance){++near;++near_objects[name];}
        if(name==limits.landmark)++landmark_hits;
        if(const auto* group=Group(design,name)) {
            if(group->id==design->focal_object_id)++focal;
        }
        if(target_depth<kHuge&&distance+.01f<target_depth) {++r->landmark_occluded_pixels;++blockers[name];}
    }
    r->focal_coverage=static_cast<double>(limits.landmark.empty()?focal:landmark_hits)/(width*height);
    r->sky_coverage=static_cast<double>(sky)/(width*height);r->near_coverage=static_cast<double>(near)/(width*height);
    // No rejection based solely on missing samples of a thin object. Same fixed grid
    // for target projection and opaque depth; no adaptive or centroid probes.
    if(r->landmark_projected_pixels>=8&&r->landmark_occluded_pixels>.95*r->landmark_projected_pixels) {
        std::string blocker;int count=0;
        for(const auto& item:blockers)if(item.second>count){blocker=item.first;count=item.second;}
        Error(r,design?"focal_object_occluded":"landmark_not_visible",blocker,blocker+" blocks "+std::to_string(r->landmark_occluded_pixels)+
            "/"+std::to_string(r->landmark_projected_pixels)+" projected pixels of "+focal_name);
    }
    if(r->near_coverage>limits.max_near_coverage) {
        std::string blocker;int count=0;
        for(const auto& item:near_objects)if(item.second>count){blocker=item.first;count=item.second;}
        Error(r,"foreground_occlusion",blocker,"Near geometry covers "+std::to_string(100*r->near_coverage)+
            "% of view within "+std::to_string(limits.near_distance)+" m; maximum="+
            std::to_string(100*limits.max_near_coverage)+"%. Move or resize the foreground while retaining the composition.");
    }
    if(r->sky_coverage>.995)Error(r,"empty_view","","Almost all view probes miss geometry");
    r->accepted=r->diagnostics.empty();if(r->accepted)*output=std::move(scene);return r->accepted;
}

bool RemoveLocalOccluder(const std::string& program,const std::string& landmark,
                         const SceneAuditReport& report,std::string* repaired,std::string* removed) {
    if(report.diagnostics.empty())return false;
    std::string blocker;
    for(const auto& d:report.diagnostics) {
        if(d.code!="landmark_not_visible"&&d.code!="foreground_occlusion")return false;
        if(blocker.empty())blocker=d.object_id;
    }
    if(blocker.empty()||blocker==landmark)return false;
    SceneAuditReport parsed;std::vector<Directive> directives;
    if(!ParseProgram(program,&directives,&parsed))return false;
    const Directive* candidate=nullptr;int boxes=0;
    for(const auto& d:directives) {
        if(d.type=="box")++boxes;
        if(d.name==blocker)candidate=&d;
        // Conservative support analysis only for axis-aligned local boxes.
        if(d.p.count("rot"))for(float angle:d.p.at("rot"))if(angle!=0)return false;
    }
    if(!candidate||candidate->type!="box"||boxes<=8)return false;
    const auto& p=candidate->p.at("pos");const auto& s=candidate->p.at("size");
    const float top=p[1]+s[1]*.5f;
    for(const auto& d:directives)if(d.type=="box"&&d.name!=blocker) {
        const auto& q=d.p.at("pos");const auto& z=d.p.at("size");
        if(fabsf(q[0]-p[0])<(z[0]+s[0])*.5f+.01f&&fabsf(q[2]-p[2])<(z[2]+s[2])*.5f+.01f&&
           fabsf(q[1]-z[1]*.5f-top)<.25f)return false;
    }
    std::istringstream input(program);std::string line,result;
    const std::regex named_box("^\\s*box\\s+\""+blocker+"\"\\s");
    bool erased=false;
    while(std::getline(input,line)) {
        if(std::regex_search(line,named_box)){erased=true;continue;}
        result+=line+"\n";
    }
    if(!erased)return false;
    *repaired=std::move(result);*removed=blocker;return true;
}

bool AuditDynamicPreview(const std::vector<unsigned char>& pixels,const std::string& lighting_mode,SceneAuditReport* r) {
    if(pixels.empty()||pixels.size()%3){Error(r,"invalid_preview","","RGB preview missing");r->accepted=false;return false;}
    double sum=0,squared=0;
    for(size_t i=0;i<pixels.size();i+=3){const double l=.2126*pixels[i]+.7152*pixels[i+1]+.0722*pixels[i+2];sum+=l;squared+=l*l;}
    const double n=static_cast<double>(pixels.size()/3);r->mean_luminance=sum/n;
    r->luminance_deviation=sqrt(std::max(0.0,squared/n-r->mean_luminance*r->mean_luminance));
    const double minimum=lighting_mode=="night"? .5:2.0;
    if(r->mean_luminance<minimum)Error(r,"preview_too_dark","","Preview is effectively black for the declared lighting mode");
    if(r->mean_luminance>253)Error(r,"preview_saturated","","Preview is almost uniformly white");
    if(r->luminance_deviation<1)Error(r,"preview_flat","","Preview lacks measurable luminance variation");
    r->accepted=r->diagnostics.empty();return r->accepted;
}
} // namespace liminal
