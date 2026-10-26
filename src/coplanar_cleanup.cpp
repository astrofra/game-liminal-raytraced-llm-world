#include "coplanar_cleanup.h"
#include "glass_supports.h"
#include "scene_hash.h"
#include "nlohmann/json.hpp"
#include <chrono>
#include <cmath>
#include <iomanip>
#include <limits>
#include <locale>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <tuple>

namespace liminal {
namespace {
using json=nlohmann::json;
using Key=std::tuple<std::string,int,std::string,int>;
constexpr const char* kMarker="\n# engine-coplanar-v1 sha256=";
double DotD(const Vec3& a,const Vec3& b) {return double(a.x)*b.x+double(a.y)*b.y+double(a.z)*b.z;}
double Distance(const Vec3& n,const Vec3& p,const Vec3& origin) {
    return double(n.x)*(double(p.x)-origin.x)+double(n.y)*(double(p.y)-origin.y)+double(n.z)*(double(p.z)-origin.z);
}
json V(Vec3 p){return {p.x,p.y,p.z};}
struct P {double x,y;};
P operator-(P a,P b){return {a.x-b.x,a.y-b.y};}
P operator+(P a,P b){return {a.x+b.x,a.y+b.y};}
P operator*(P a,double t){return {a.x*t,a.y*t};}
double Cross2(P a,P b){return a.x*b.y-a.y*b.x;}
using Polygon=std::vector<P>;
double Area(const Polygon& p) {
    double a=0;for(size_t i=0;i<p.size();++i)a+=Cross2(p[i],p[(i+1)%p.size()]);return a*.5;
}
Polygon Project(const SceneBoxFace& face,const Vec3& origin,const Vec3& u,const Vec3& v) {
    Polygon p;for(const auto& c:face.corners)p.push_back({Distance(u,c,origin),Distance(v,c,origin)});
    if(Area(p)<0)std::reverse(p.begin(),p.end());return p;
}
double Overlap(const SceneBoxFace& a,const SceneBoxFace& b) {
    const Vec3 seed=std::abs(a.normal.x)<.8f?Vec3(1,0,0):Vec3(0,1,0);
    const auto u=Normalize(Cross(a.normal,seed)),v=Normalize(Cross(a.normal,u));
    auto subject=Project(a,a.center,u,v);const auto clip=Project(b,a.center,u,v);
    for(size_t i=0;i<clip.size()&&!subject.empty();++i) {
        const P origin=clip[i],edge=clip[(i+1)%clip.size()]-origin;Polygon out;
        P p=subject.back();double d=Cross2(edge,p-origin);
        for(P q:subject) {
            const double e=Cross2(edge,q-origin);
            if((d>=0)!=(e>=0))out.push_back(p+(q-p)*(d/(d-e)));
            if(e>=0)out.push_back(q);p=q;d=e;
        }
        subject=std::move(out);
    }
    return std::abs(Area(subject));
}
template<class Points>Aabb Bounds(const Points& points){Aabb a;for(const auto& p:points)a.Expand(p);return a;}
bool Near(const Aabb& a,const Aabb& b,double t) {
    return a.min.x<=b.max.x+t&&b.min.x<=a.max.x+t&&a.min.y<=b.max.y+t&&b.min.y<=a.max.y+t&&
           a.min.z<=b.max.z+t&&b.min.z<=a.max.z+t;
}
struct Pair {
    size_t a,b;int fa,fb;bool opposed;Key key;json detail;
};
struct Detection {
    std::vector<Pair> pairs;std::set<Key> conflicts;size_t precise_tests=0,plane_tests=0;
};
Detection Detect(const std::vector<SceneBox>& boxes,const std::vector<Camera>& cameras,const CoplanarCleanupOptions& o) {
    Detection d;
    std::vector<std::array<SceneBoxFace,6>> faces;
    std::vector<std::array<double,6>> facing;
    for(const auto& box:boxes) {
        faces.push_back(SceneBoxFaces(box));std::array<double,6> f{};
        for(int i=0;i<6;++i) {
            f[i]=-1;for(const auto& camera:cameras) {
                const auto v=camera.eye-faces.back()[i].center;
                if(LengthSquared(v)>1e-12f)f[i]=std::max(f[i],DotD(faces.back()[i].normal,v)/Length(v));
            }
        }
        facing.push_back(f);
    }
    const double cosine=std::cos(o.angle_tolerance_degrees*3.14159265358979323846/180.);
    for(size_t i=0;i<boxes.size();++i)for(size_t j=i+1;j<boxes.size();++j) {
        if(!Near(Bounds(boxes[i].corners),Bounds(boxes[j].corners),o.plane_tolerance))continue;
        for(int a=0;a<6;++a)for(int b=0;b<6;++b) {
            if(std::max(facing[i][a],facing[j][b])<=o.facing_min)continue;
            const auto& x=faces[i][a];const auto& y=faces[j][b];
            if(!Near(Bounds(x.corners),Bounds(y.corners),o.plane_tolerance))continue;
            ++d.plane_tests;
            const double dot=std::clamp(DotD(x.normal,y.normal),-1.,1.);
            if(std::abs(dot)<cosine)continue;
            double max_distance=0;json ab=json::array(),ba=json::array();
            for(const auto& p:y.corners){const double value=Distance(x.normal,p,x.corners[0]);ab.push_back(value);max_distance=std::max(max_distance,std::abs(value));}
            for(const auto& p:x.corners){const double value=Distance(y.normal,p,y.corners[0]);ba.push_back(value);max_distance=std::max(max_distance,std::abs(value));}
            if(max_distance>o.plane_tolerance)continue;
            ++d.precise_tests;const double area=Overlap(x,y);
            if(area<=o.overlap_area_min)continue;
            Pair p{i,j,a,b,dot<0,Key{boxes[i].id,a,boxes[j].id,b},{}};
            p.detail={{"block_a",boxes[i].id},{"block_b",boxes[j].id},{"face_a",a},{"face_b",b},
                {"facing_a",facing[i][a]},{"facing_b",facing[j][b]},{"normal_dot",dot},
                {"normal_angle_degrees",std::acos(dot)*180./3.14159265358979323846},
                {"distances_b_to_a",ab},{"distances_a_to_b",ba},{"max_plane_distance",max_distance},{"overlap_area",area},
                {"classification",p.opposed?"opposed_joint":"same_direction_conflict"}};
            if(!p.opposed)d.conflicts.insert(p.key);d.pairs.push_back(std::move(p));
        }
    }
    return d;
}
json Pairs(const Detection& d){json a=json::array();for(const auto& p:d.pairs)a.push_back(p.detail);return a;}
std::vector<SceneBox> Boxes(const Scene& scene) {
    std::vector<SceneBox> boxes;std::set<std::string> ids;
    for(const auto& b:scene.boxes)if(!b.glass) {
        if(!ids.insert(b.id).second)throw std::runtime_error("duplicate opaque box identifier");
        for(const auto& p:b.corners)if(!std::isfinite(p.x)||!std::isfinite(p.y)||!std::isfinite(p.z))
            throw std::runtime_error("non-finite box geometry");
        boxes.push_back(b);
    }
    if(boxes.size()>96)throw std::runtime_error("at most 96 opaque boxes");
    std::sort(boxes.begin(),boxes.end(),[](const SceneBox& a,const SceneBox& b){return a.id<b.id;});return boxes;
}
Scene Load(const std::string& text) {
    Scene s;char error[512]={};
    if(!LoadSceneFromSceneText("coplanar",text.c_str(),&s,error,sizeof(error)))throw std::runtime_error(error);
    return s;
}
std::string SetPosition(const std::string& program,const std::string& id,Vec3 position) {
    // Only serialize a parsed object's position. No geometry or rotations are
    // inferred from text; all detection uses the engine's retained SceneBox.
    for(size_t start=0;start<program.size();) {
        auto end=program.find('\n',start);if(end==std::string::npos)end=program.size();
        const auto line=program.substr(start,end-start);const auto first=line.find_first_not_of(" \t\r");
        if(first!=std::string::npos&&line.compare(first,4,"box ")==0) {
            const auto q=line.find('"',first),r=q==std::string::npos?q:line.find('"',q+1);
            if(r!=std::string::npos&&line.substr(q+1,r-q-1)==id) {
                const auto p=line.find("pos(",r),close=p==std::string::npos?p:line.find(')',p);
                if(close==std::string::npos)throw std::runtime_error("missing parsed position");
                std::ostringstream value;value.imbue(std::locale::classic());value<<std::setprecision(std::numeric_limits<float>::max_digits10)
                    <<position.x<<','<<position.y<<','<<position.z;
                auto output=program;output.replace(start+p+4,close-p-4,value.str());return output;
            }
        }
        start=end+1;
    }
    throw std::runtime_error("parsed box has no editable source position");
}
bool Separated(const SceneBoxFace& a,const SceneBoxFace& b,double margin) {
    for(const auto* reference:{&a,&b}) {
        const auto& other=reference==&a?b:a;double lo=1e30,hi=-1e30;
        for(const auto& p:other.corners){const double d=Distance(reference->normal,p,reference->corners[0]);lo=std::min(lo,d);hi=std::max(hi,d);}
        if(!(lo>margin||hi<-margin))return false;
    }
    return true;
}
bool Inside(Vec3 p,const SceneBox& b) {
    const Vec3 v=p-b.center;const float h[]={b.size.x*.5f,b.size.y*.5f,b.size.z*.5f};
    for(int i=0;i<3;++i)if(std::abs(DotD(v,b.axes[i]))>h[i]+1e-5)return false;return true;
}
int LostContacts(size_t moved,const std::vector<SceneBox>& before,const std::vector<SceneBox>& after) {
    int loss=0;
    if(Bounds(before[moved].corners).min.y<=.01f&&Bounds(after[moved].corners).min.y>.01f)++loss;
    for(size_t i=0;i<before.size();++i)if(i!=moved&&SceneBoxesTouch(before[moved],before[i])&&!SceneBoxesTouch(after[moved],after[i]))++loss;
    return loss;
}
std::string StripMarker(const std::string& program,bool* recognized) {
    *recognized=false;const auto start=program.rfind(kMarker);if(start==std::string::npos)return program;
    const size_t hash_start=start+std::char_traits<char>::length(kMarker);
    const auto end=program.find('\n',hash_start);
    if(end!=program.size()-1||end-hash_start!=64)return program;
    const auto body=program.substr(0,start);
    *recognized=program.substr(hash_start,64)==SceneTextHash(body);
    return body; // A stale marker is invalidated when geometry has changed.
}
} // namespace

CoplanarCleanupResult CleanupCoplanarFaces(const std::string& program,const CoplanarCleanupOptions& options) {
    const auto start=std::chrono::steady_clock::now();
    CoplanarCleanupResult result;result.source_scene_text=OpaqueSceneWithoutGlassSupports(program);result.scene_text=result.source_scene_text;
    CoplanarCleanupOptions o=options;
    o.max_translations=std::min<size_t>(o.max_translations,16);o.max_candidate_trials=std::min<size_t>(o.max_candidate_trials,512);
    o.max_cumulative_distance=std::min(o.max_cumulative_distance,.05);
    json report={{"algorithm","coplanar-cleanup-v1"},{"initial_pairs",json::array()},{"final_pairs",json::array()},
        {"trials",json::array()},{"moves",json::array()},{"modified_blocks",json::array()},{"status","failed"},
        {"invalidated_glass",program!=result.source_scene_text},
        {"parameters",{{"facing_min",o.facing_min},{"angle_tolerance_degrees",o.angle_tolerance_degrees},
            {"plane_tolerance",o.plane_tolerance},{"overlap_area_min",o.overlap_area_min},
            {"steps_metres",{.01,-.01,.02,-.02,.04,-.04}},{"separation_margin",o.plane_tolerance+.001},
            {"max_translations",o.max_translations},{"max_candidate_trials",o.max_candidate_trials},
            {"max_cumulative_distance",o.max_cumulative_distance},{"fixed_ids",o.fixed_ids}}}};
    size_t precise=0,planes=0;double detection_ms=0;
    try {
        if(!std::isfinite(o.facing_min)||o.facing_min<-1||o.facing_min>1||
           !std::isfinite(o.angle_tolerance_degrees)||o.angle_tolerance_degrees<=0||o.angle_tolerance_degrees>1||
           !std::isfinite(o.plane_tolerance)||o.plane_tolerance<=0||o.plane_tolerance>.01||
           !std::isfinite(o.overlap_area_min)||o.overlap_area_min<0||
           !std::isfinite(options.max_cumulative_distance)||options.max_cumulative_distance<0||o.extra_cameras.size()>8)
            throw std::runtime_error("invalid coplanar pilot options");
        bool processed=false;auto current=StripMarker(result.source_scene_text,&processed);
        const auto scene=Load(current);auto boxes=Boxes(scene);const auto original=boxes;
        std::vector<Camera> cameras={scene.camera};cameras.insert(cameras.end(),o.extra_cameras.begin(),o.extra_cameras.end());
        for(const auto& c:cameras)if(!std::isfinite(c.eye.x)||!std::isfinite(c.eye.y)||!std::isfinite(c.eye.z))
            throw std::runtime_error("non-finite camera eye");
        report["reference_camera"]={{"eye",V(scene.camera.eye)},{"target",V(scene.camera.target)},{"up",V(scene.camera.up)},
            {"fov",scene.camera.vertical_fov_degrees}};
        report["camera_eyes"]=json::array();for(const auto& c:cameras)report["camera_eyes"].push_back(V(c.eye));
        report["camera_poses"]=json::array();
        for(const auto& c:cameras)report["camera_poses"].push_back({{"eye",V(c.eye)},{"target",V(c.target)},
            {"up",V(c.up)},{"fov",c.vertical_fov_degrees}});
        auto detect=[&](const std::vector<SceneBox>& b){
            const auto began=std::chrono::steady_clock::now();auto d=Detect(b,cameras,o);
            detection_ms+=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-began).count();
            precise+=d.precise_tests;planes+=d.plane_tests;return d;
        };
        auto detection=detect(boxes);result.initial_conflicts=detection.conflicts.size();
        report["initial_pairs"]=Pairs(detection);
        std::map<std::string,double> cumulative;
        const std::set<std::string> fixed(o.fixed_ids.begin(),o.fixed_ids.end());
        std::vector<SceneBox> clear_volumes;
        if(o.protect_local_approaches) {
            const SceneBox volumes[]={MakeSceneBox("back",Vec3(0,1.25f,-9),Vec3(1.6f,2,6),Vec3(0)),
                MakeSceneBox("forward",Vec3(0,1.25f,-6),Vec3(1.6f,2,6),Vec3(0)),
                MakeSceneBox("left",Vec3(3,1.25f,-10),Vec3(6,2,1.6f),Vec3(0)),
                MakeSceneBox("right",Vec3(-3,1.25f,-10),Vec3(6,2,1.6f),Vec3(0))};
            for(const auto& volume:volumes) {
                bool blocked=false;for(const auto& b:scene.boxes)if(SceneBoxesTouch(b,volume,0)){blocked=true;break;}
                if(!blocked)clear_volumes.push_back(volume);
            }
        }
        size_t trials=0;
        while(o.apply_translations&&!processed&&!detection.conflicts.empty()&&result.accepted_translations<o.max_translations&&trials<o.max_candidate_trials) {
            bool moved=false;
            for(const auto& conflict:detection.pairs) {
                if(conflict.opposed)continue;
                const auto normal=SceneBoxFaces(boxes[conflict.a])[conflict.fa].normal;
                for(float step:{.01f,.02f,.04f}) {
                    for(size_t which:{conflict.b,conflict.a}) {
                        const auto& box=boxes[which];if(fixed.count(box.id))continue;
                        if(cumulative[box.id]+step>o.max_cumulative_distance+1e-8)continue;
                        struct Choice { std::string text;std::vector<SceneBox> boxes;Detection detection;size_t trial;int loss;Vec3 delta; };
                        std::vector<Choice> choices;
                        for(float sign:{1.f,-1.f}) {
                            if(trials>=o.max_candidate_trials)break;
                            ++trials;const Vec3 delta=normal*(step*sign);
                            auto text=SetPosition(current,box.id,box.center+delta);auto candidate=Boxes(Load(text));
                            std::string reason;
                            const auto bounds=Bounds(candidate[which].corners),old=Bounds(original[which].corners);
                            if(bounds.min.x<-256||bounds.max.x>256||bounds.min.y<-256||bounds.max.y>256||bounds.min.z<-256||bounds.max.z>256)reason="spatial_bounds";
                            if(bounds.min.y<std::min(0.f,old.min.y)-.001f)reason="floor_penetration";
                            for(const auto& camera:cameras)if(Inside(camera.eye,candidate[which]))reason="camera_inside_mass";
                            for(const auto& volume:clear_volumes)if(SceneBoxesTouch(candidate[which],volume,0))reason="reserved_approach:"+volume.id;
                            auto next=detect(candidate);
                            for(const auto& key:next.conflicts)if(!detection.conflicts.count(key)){reason="new_coplanar_conflict";break;}
                            if(reason.empty()&&next.conflicts.size()>=detection.conflicts.size())reason="conflict_count_not_reduced";
                            if(reason.empty()&&!Separated(SceneBoxFaces(candidate[conflict.a])[conflict.fa],
                                SceneBoxFaces(candidate[conflict.b])[conflict.fb],o.plane_tolerance+.001))reason="insufficient_plane_separation";
                            if(reason.empty()&&o.validate&&!o.validate(text,&reason)&&reason.empty())reason="authoritative_audit_rejected";
                            const int loss=LostContacts(which,boxes,candidate);
                            report["trials"].push_back({{"block",box.id},{"delta",V(delta)},{"from",V(box.center)},
                                {"to",V(candidate[which].center)},{"reason",reason.empty()?"admissible_not_selected":reason},
                                {"lost_contacts",loss},{"conflicts_before",detection.conflicts.size()},{"conflicts_after",next.conflicts.size()},
                                {"accepted",false}});
                            if(reason.empty())choices.push_back({std::move(text),std::move(candidate),std::move(next),trials-1,loss,delta});
                        }
                        if(!choices.empty()) {
                            const auto best=std::min_element(choices.begin(),choices.end(),[](const Choice& a,const Choice& b){return a.loss<b.loss;});
                            const std::string id=box.id;
                            cumulative[id]+=Length(best->delta);++result.accepted_translations;
                            report["trials"][best->trial]["accepted"]=true;report["trials"][best->trial]["reason"]="accepted";
                            report["moves"].push_back(report["trials"][best->trial]);
                            current=best->text;boxes=best->boxes;detection=best->detection;moved=true;break;
                        }
                    }
                    if(moved||trials>=o.max_candidate_trials)break;
                }
                if(moved||trials>=o.max_candidate_trials)break;
            }
            if(!moved)break;
        }
        result.remaining_conflicts=detection.conflicts.size();report["final_pairs"]=Pairs(detection);
        for(size_t i=0;i<boxes.size();++i)if(cumulative[boxes[i].id]>0)
            report["modified_blocks"].push_back({{"id",boxes[i].id},{"initial_position",V(original[i].center)},
                {"final_position",V(boxes[i].center)},{"cumulative_distance",cumulative[boxes[i].id]}});
        if(!o.apply_translations||processed)result.scene_text=result.source_scene_text;
        else {
            result.scene_text=current;
            if(result.initial_conflicts)result.scene_text+=std::string(kMarker)+SceneTextHash(current)+"\n";
        }
        report["status"]=!o.apply_translations?"analyzed":processed?"already_processed":result.remaining_conflicts?"residual_conflicts":"resolved";
        report["stop_reason"]=!o.apply_translations?"analysis_only":processed?"content_marker":!result.remaining_conflicts?"no_targeted_conflict":
            trials>=o.max_candidate_trials?"candidate_trial_budget":result.accepted_translations>=o.max_translations?"translation_budget":"no_admissible_translation";
        result.success=true;
    } catch(const std::exception& e) {
        result.error=e.what();result.scene_text=result.source_scene_text;result.accepted_translations=0;
        result.remaining_conflicts=result.initial_conflicts;
        report["status"]="opaque_fallback";report["moves"]=json::array();report["modified_blocks"]=json::array();
        report["final_pairs"]=report["initial_pairs"];
        for(auto& trial:report["trials"])if(trial["accepted"].get<bool>()) {
            trial["accepted"]=false;trial["reason"]="pass_failed_rollback";
        }
    }
    report["success"]=result.success;report["error"]=result.error;
    report["source_sha256"]=SceneTextHash(result.source_scene_text);report["corrected_sha256"]=SceneTextHash(result.scene_text);
    report["initial_conflicts"]=result.initial_conflicts;report["remaining_conflicts"]=result.remaining_conflicts;
    report["accepted_translations"]=result.accepted_translations;report["precise_polygon_tests"]=precise;report["plane_tests"]=planes;
    report["candidate_trials"]=report["trials"].size();
    report["detection_ms"]=detection_ms;
    report["elapsed_ms"]=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
    result.report_json=report.dump(2);return result;
}
} // namespace liminal
