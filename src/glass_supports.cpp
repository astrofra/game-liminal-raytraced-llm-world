#include "glass_supports.h"
#include "nlohmann/json.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <locale>
#include <map>
#include <numeric>
#include <set>
#include <sstream>
#include <stdexcept>

namespace liminal {
namespace {
using json = nlohmann::json;
constexpr double kTolerance = 1e-6;
constexpr double kWeld = 1e-5;
constexpr double kContact = .01;
constexpr double kGap = .05;
constexpr double kThickness = .25;
const std::string kBegin = "\n# engine-glass-supports-v1 begin\n";
const std::string kEnd = "# engine-glass-supports-v1 end\n";
struct P { double x, z; };
using Polygon = std::vector<P>;
P operator+(P a,P b) {return {a.x+b.x,a.z+b.z};}
P operator-(P a,P b) {return {a.x-b.x,a.z-b.z};}
P operator*(P a,double s) {return {a.x*s,a.z*s};}
double Cross2(P a,P b) {return a.x*b.z-a.z*b.x;}
double Dot2(P a,P b) {return a.x*b.x+a.z*b.z;}
double Area(const Polygon& p) {
    double area=0;for(size_t i=0;i<p.size();++i)area+=Cross2(p[i],p[(i+1)%p.size()]);
    return area*.5;
}
void Clean(Polygon& p) {
    Polygon q;
    for(P v:p)if(q.empty()||Dot2(v-q.back(),v-q.back())>kWeld*kWeld)q.push_back(v);
    if(q.size()>1&&Dot2(q.front()-q.back(),q.front()-q.back())<kWeld*kWeld)q.pop_back();
    if(q.size()<3||std::abs(Area(q))<kTolerance)q.clear();
    else if(Area(q)<0)std::reverse(q.begin(),q.end());
    p=std::move(q);
}
Polygon Hull(Polygon points) {
    std::sort(points.begin(),points.end(),[](P a,P b){return a.x<b.x||(a.x==b.x&&a.z<b.z);});
    Polygon h;
    for(P p:points) {
        while(h.size()>1&&Cross2(h.back()-h[h.size()-2],p-h.back())<=kTolerance)h.pop_back();
        h.push_back(p);
    }
    const size_t lower=h.size();
    for(auto i=points.rbegin()+1;i!=points.rend();++i) {
        while(h.size()>lower&&Cross2(h.back()-h[h.size()-2],*i-h.back())<=kTolerance)h.pop_back();
        h.push_back(*i);
    }
    if(!h.empty())h.pop_back();Clean(h);return h;
}
// Retain dot(p, normal) >= offset. Distances use doubles; snapping and welding
// happen once before triangulation so shared boundaries have identical vertices.
Polygon Clip(const Polygon& p,P normal,double offset) {
    Polygon q;if(p.empty())return q;
    P a=p.back();double da=Dot2(a,normal)-offset;
    for(P b:p) {
        const double db=Dot2(b,normal)-offset;
        if((da>=0)!=(db>=0))q.push_back(a+(b-a)*(da/(da-db)));
        if(db>=0)q.push_back(b);
        a=b;da=db;
    }
    Clean(q);return q;
}
Polygon Intersect(Polygon p,const Polygon& clip) {
    for(size_t i=0;i<clip.size()&&!p.empty();++i) {
        const P a=clip[i],e=clip[(i+1)%clip.size()]-a,n={-e.z,e.x};
        p=Clip(p,n,Dot2(a,n));
    }
    return p;
}
Polygon Footprint(const SceneBox& b) {
    Polygon p;for(const auto& v:b.corners)p.push_back({v.x,v.z});return Hull(p);
}
Aabb Bounds(const SceneBox& b) {Aabb a;for(const auto& p:b.corners)a.Expand(p);return a;}
struct LowerFace { Polygon footprint; Vec3 normal, point; };
std::vector<LowerFace> LowerFaces(const SceneBox& b) {
    static const int indices[6][4]={{1,2,6,5},{0,4,7,3},{3,7,6,2},{0,1,5,4},{4,5,6,7},{0,3,2,1}};
    std::vector<LowerFace> faces;
    for(int i=0;i<6;++i) {
        Vec3 normal=b.axes[i/2]*(i%2?-1.f:1.f);
        if(normal.y>=-1e-6f)continue;
        LowerFace face;face.normal=normal;face.point=b.corners[indices[i][0]];
        for(int j:indices[i])face.footprint.push_back({b.corners[j].x,b.corners[j].z});
        Clean(face.footprint);if(!face.footprint.empty())faces.push_back(face);
    }
    return faces;
}
double Height(P p,const std::vector<LowerFace>& faces) {
    double y=-1e30;
    for(const auto& f:faces)y=std::max(y,double(f.point.y)-
        (f.normal.x*(p.x-f.point.x)+f.normal.z*(p.z-f.point.z))/f.normal.y);
    return y;
}
struct Mesh { std::vector<Vec3> vertices; std::vector<std::array<int,3>> triangles; };
struct Shell { Mesh mesh; std::vector<Polygon> cells; std::vector<LowerFace> faces; Polygon inner; };
Shell BuildShell(const SceneBox& box,const Polygon& outer) {
    Shell shell;shell.faces=LowerFaces(box);
    Polygon remaining=outer;
    std::vector<Polygon> bands;
    for(size_t i=0;i<outer.size()&&!remaining.empty();++i) {
        const P a=outer[i],e=outer[(i+1)%outer.size()]-a;
        const double length=std::sqrt(Dot2(e,e));
        const P n={-e.z/length,e.x/length};const double d=Dot2(a,n)+kThickness;
        auto band=Clip(remaining,n*-1,-d);if(!band.empty())bands.push_back(band);
        remaining=Clip(remaining,n,d);
    }
    shell.inner=remaining;
    if(remaining.empty()||Area(remaining)<1e-4)return shell;
    for(const auto& band:bands)for(const auto& face:shell.faces) {
        auto cell=Intersect(band,face.footprint);
        if(!cell.empty())shell.cells.push_back(cell);
    }
    Polygon points;
    auto index=[&](P p) {
        for(size_t i=0;i<points.size();++i)if(Dot2(p-points[i],p-points[i])<kWeld*kWeld)return static_cast<int>(i);
        points.push_back(p);return static_cast<int>(points.size()-1);
    };
    std::vector<std::vector<int>> cells;
    for(const auto& cell:shell.cells) {
        std::vector<int> ids;for(P p:cell)ids.push_back(index(p));cells.push_back(ids);
    }
    const int count=static_cast<int>(points.size());
    for(P p:points)shell.mesh.vertices.push_back(Vec3(float(p.x),float(Height(p,shell.faces)),float(p.z)));
    for(P p:points)shell.mesh.vertices.push_back(Vec3(float(p.x),0,float(p.z)));
    std::map<std::pair<int,int>,std::vector<std::pair<int,int>>> edges;
    for(const auto& cell:cells) {
        std::vector<int> perimeter;
        // Split T junctions introduced by independent convex clipping.
        for(size_t i=0;i<cell.size();++i) {
            const int a=cell[i],b=cell[(i+1)%cell.size()];const P e=points[b]-points[a];
            const double len2=Dot2(e,e);std::vector<std::pair<double,int>> along={{0,a}};
            if(len2<kWeld*kWeld)throw std::runtime_error("collapsed shell edge");
            for(int j=0;j<count;++j)if(j!=a&&j!=b) {
                const P v=points[j]-points[a];const double t=Dot2(v,e)/len2;
                if(t>kTolerance&&t<1-kTolerance&&std::abs(Cross2(v,e))<kWeld*std::sqrt(len2))along.push_back({t,j});
            }
            std::sort(along.begin(),along.end());for(const auto& v:along)perimeter.push_back(v.second);
        }
        P center{0,0};for(int j:perimeter)center=center+points[j];center=center*(1.0/perimeter.size());
        const int top=static_cast<int>(shell.mesh.vertices.size()),bottom=top+1;
        shell.mesh.vertices.push_back(Vec3(float(center.x),float(Height(center,shell.faces)),float(center.z)));
        shell.mesh.vertices.push_back(Vec3(float(center.x),0,float(center.z)));
        for(size_t i=0;i<perimeter.size();++i) {
            int a=perimeter[i],b=perimeter[(i+1)%perimeter.size()];
            shell.mesh.triangles.push_back({top,b,a}); // XZ CCW points down.
            shell.mesh.triangles.push_back({bottom,a+count,b+count});
            edges[std::minmax(a,b)].push_back({a,b});
        }
    }
    for(const auto& entry:edges) {
        const auto& uses=entry.second;
        if(uses.size()==2) {
            if(uses[0].first!=uses[1].second||uses[0].second!=uses[1].first)
                throw std::runtime_error("inconsistent shell seam");
            continue; // No internal dielectric surfaces.
        }
        if(uses.size()!=1)throw std::runtime_error("non manifold shell seam");
        const int a=uses[0].first,b=uses[0].second;
        shell.mesh.triangles.push_back({a,b,b+count});
        shell.mesh.triangles.push_back({a,b+count,a+count});
    }
    std::map<std::pair<int,int>,int> closed;
    double volume=0;
    for(const auto& t:shell.mesh.triangles) {
        const auto a=shell.mesh.vertices[t[0]],b=shell.mesh.vertices[t[1]],c=shell.mesh.vertices[t[2]];
        if(LengthSquared(Cross(b-a,c-a))<1e-14f)throw std::runtime_error("degenerate shell triangle");
        volume+=Dot(a,Cross(b,c))/6.;
        for(int i=0;i<3;++i)++closed[std::minmax(t[i],t[(i+1)%3])];
    }
    for(const auto& edge:closed)if(edge.second!=2)throw std::runtime_error("open shell edge");
    if(volume<=kTolerance)throw std::runtime_error("invalid shell orientation");
    return shell;
}
bool IntersectsReserved(const Shell& shell,const GlassSupportExclusion& r) {
    const Vec3 lo=r.center-r.size*.5f,hi=r.center+r.size*.5f;
    if(hi.y<0)return false;
    const Polygon rect={{lo.x,lo.z},{hi.x,lo.z},{hi.x,hi.z},{lo.x,hi.z}};
    for(const auto& cell:shell.cells) {
        const auto p=Intersect(cell,rect);
        for(P v:p)if(Height(v,shell.faces)>=lo.y-kTolerance)return true;
    }
    return false;
}
bool IntersectsBox(const Shell& shell,const SceneBox& box) {
    // Each clipped cell is convex between a flat base and one affine top.
    // SAT on face normals and edge cross-products detects volume overlap;
    // mere contact with an existing assembly is allowed.
    for(const auto& cell:shell.cells) {
        std::vector<Vec3> vertices,edges,axes(box.axes.begin(),box.axes.end());
        for(P p:cell)vertices.push_back(Vec3(float(p.x),float(Height(p,shell.faces)),float(p.z)));
        const size_t n=vertices.size();
        for(size_t i=0;i<n;++i)vertices.push_back(Vec3(vertices[i].x,0,vertices[i].z));
        axes.push_back(Vec3(0,1,0));
        for(size_t i=0;i<n;++i) {
            const size_t j=(i+1)%n;
            edges.push_back(vertices[j]-vertices[i]);edges.push_back(vertices[j+n]-vertices[i+n]);
            edges.push_back(vertices[i+n]-vertices[i]);
            axes.push_back(Cross(vertices[j]-vertices[i],Vec3(0,1,0)));
            axes.push_back(Cross(vertices[j]-vertices[i],vertices[(j+1)%n]-vertices[j]));
        }
        for(const auto& e:edges)for(const auto& a:box.axes)axes.push_back(Cross(e,a));
        bool separated=false;
        for(auto axis:axes) {
            const float length=Length(axis);if(length<1e-6f)continue;axis=axis/length;
            double low=1e30,high=-1e30,blo=1e30,bhi=-1e30;
            for(const auto& v:vertices){const double d=Dot(v,axis);low=std::min(low,d);high=std::max(high,d);}
            for(const auto& v:box.corners){const double d=Dot(v,axis);blo=std::min(blo,d);bhi=std::max(bhi,d);}
            if(std::min(high,bhi)-std::max(low,blo)<=1e-4){separated=true;break;}
        }
        if(!separated)return true;
    }
    return false;
}
json PointsJson(const Polygon& p) {json j=json::array();for(P v:p)j.push_back({v.x,v.z});return j;}
std::string SerializeMesh(const Mesh& mesh,const std::string& id) {
    std::ostringstream out;out.imbue(std::locale::classic());out<<std::setprecision(9);
    for(const auto& t:mesh.triangles) {
        out<<"engine_glass_triangle \""<<id<<"\"";
        for(int i=0;i<3;++i) {const auto& v=mesh.vertices[t[i]];out<<' '<<char('a'+i)<<'('<<v.x<<','<<v.y<<','<<v.z<<')';}
        out<<'\n';
    }
    return out.str();
}
} // namespace

std::string OpaqueSceneWithoutGlassSupports(const std::string& program) {
    const auto begin=program.find(kBegin);
    if(begin!=std::string::npos&&program.size()>=kEnd.size()&&
       program.compare(program.size()-kEnd.size(),kEnd.size(),kEnd)==0) {
        // A lookalike comment pair in authored text must never erase opaque boxes.
        const auto first=begin+kBegin.size(),last=program.size()-kEnd.size();
        if(first>=last)return program;
        std::istringstream input(program.substr(first,last-first));std::string line;
        while(std::getline(input,line))if(line.compare(0,22,"engine_glass_triangle ")!=0)return program;
        return program.substr(0,begin);
    }
    return program;
}

GlassSupportResult BuildGlassSupports(const std::string& program,const GlassSupportOptions& options) {
    GlassSupportResult result;result.opaque_scene_text=OpaqueSceneWithoutGlassSupports(program);
    result.scene_text=result.opaque_scene_text;
    const auto start=std::chrono::steady_clock::now();
    json report={{"algorithm","glass-supports-v1"},{"floor_y",0},{"thickness",kThickness},
        {"contact_tolerance",kContact},{"minimum_gap",kGap},{"blocks",json::array()},
        {"mechanical_stability",false},{"max_triangles",std::min<size_t>(options.max_triangles,4096)}};
    try {
        Scene scene;char error[512]={};
        if(!LoadSceneFromSceneText("glass-support-source",result.opaque_scene_text.c_str(),&scene,error,sizeof(error)))
            throw std::runtime_error(error);
        if(scene.boxes.size()>96)throw std::runtime_error("support analysis limited to 96 original boxes");
        std::vector<SceneBox> boxes;
        std::set<std::string> names;
        for(const auto& b:scene.boxes)if(!b.glass) {
            if(!names.insert(b.id).second)throw std::runtime_error("duplicate source box id");
            for(const auto& p:b.corners)if(!std::isfinite(p.x)||!std::isfinite(p.y)||!std::isfinite(p.z)||
                std::abs(p.x)>1024||std::abs(p.y)>1024||std::abs(p.z)>1024)throw std::runtime_error("invalid source box");
            boxes.push_back(b);
        }
        std::sort(boxes.begin(),boxes.end(),[](const SceneBox& a,const SceneBox& b){return a.id<b.id;});
        const size_t n=boxes.size();std::vector<int> component(n,-1);std::vector<bool> ground(n);
        std::vector<std::vector<size_t>> neighbors(n);
        for(size_t i=0;i<n;++i) {
            ground[i]=Bounds(boxes[i]).min.y<=kContact+kTolerance;
            for(size_t j=0;j<i;++j)if(SceneBoxesTouch(boxes[i],boxes[j],float(kContact))){neighbors[i].push_back(j);neighbors[j].push_back(i);}
        }
        int next_component=0;std::vector<bool> grounded;
        for(size_t root=0;root<n;++root)if(component[root]<0) {
            std::vector<size_t> pending={root};component[root]=next_component;bool anchored=false;
            for(size_t k=0;k<pending.size();++k) {
                const size_t i=pending[k];anchored=anchored||ground[i];
                for(size_t j:neighbors[i])if(component[j]<0){component[j]=next_component;pending.push_back(j);}
            }
            grounded.push_back(anchored);++next_component;
        }
        for(size_t i=0;i<n;++i) {
            json adjacent=json::array();for(size_t j:neighbors[i])adjacent.push_back(boxes[j].id);
            report["blocks"].push_back({{"id",boxes[i].id},{"min_y",Bounds(boxes[i]).min.y},
                {"ground_contact",bool(ground[i])},{"grounded",bool(grounded[component[i]])},{"neighbors",adjacent},
                {"component",component[i]},{"footprint",PointsJson(Footprint(boxes[i]))},{"decision","candidate"}});
        }
        auto reserved=options.reserved;
        // Camera motion/breathing envelope, including the viewpoint itself.
        reserved.push_back({"camera",scene.camera.eye,Vec3(.5f,.5f,.5f)});
        if(options.protect_local_approaches) {
            reserved.push_back({"back",Vec3(0,1.25f,-9),Vec3(1.6f,2,6)});
            reserved.push_back({"forward",Vec3(0,1.25f,-6),Vec3(1.6f,2,6)});
            reserved.push_back({"left",Vec3(3,1.25f,-10),Vec3(6,2,1.6f)});
            reserved.push_back({"right",Vec3(-3,1.25f,-10),Vec3(6,2,1.6f)});
        }
        std::vector<size_t> order(n);std::iota(order.begin(),order.end(),0);
        std::stable_sort(order.begin(),order.end(),[&](size_t a,size_t b){return Bounds(boxes[a]).min.y<Bounds(boxes[b]).min.y;});
        std::set<int> supported;std::string geometry;
        std::vector<Aabb> added_bounds;
        for(size_t i:order) {
            auto& record=report["blocks"][i];std::string decision;
            if(grounded[component[i]])decision="grounded";
            else if(supported.count(component[i]))decision="component_supported";
            else if(Bounds(boxes[i]).min.y<=kGap+kTolerance)decision="below_gap";
            else if(!options.build_supports)decision="candidate";
            else {
                try {
                    const auto shell=BuildShell(boxes[i],Footprint(boxes[i]));
                    if(shell.inner.empty()||shell.mesh.triangles.empty())decision="narrow_footprint";
                    else if(result.added_triangles+shell.mesh.triangles.size()>std::min<size_t>(options.max_triangles,4096))decision="triangle_budget";
                    else {
                        for(const auto& r:reserved)if(IntersectsReserved(shell,r)){decision="reserved_volume";record["exclusion"]=r.name;break;}
                        // Existing opaque and authored glass volumes are unchanged.
                        for(size_t j=0;j<n&&decision.empty();++j)if(j!=i) {
                            if(IntersectsBox(shell,boxes[j])) {
                                decision="opaque_overlap";record["exclusion"]=boxes[j].id;
                            }
                        }
                        for(const auto& b:scene.boxes)if(b.glass&&decision.empty()&&IntersectsBox(shell,b)) {
                            decision="existing_glass_overlap";record["exclusion"]=b.id;
                        }
                        for(const auto& a:added_bounds)if(decision.empty()&&
                            IntersectsReserved(shell,{"previous_support",(a.min+a.max)*.5f,a.max-a.min}))decision="support_overlap";
                        if(decision.empty()) {
                            const auto id="glass_support_"+std::to_string(i);
                            for(const auto& material:scene.materials)if(material.name==id)throw std::runtime_error("reserved support identifier collision");
                            geometry+=SerializeMesh(shell.mesh,id);decision="added";supported.insert(component[i]);
                            ++result.added_supports;result.added_triangles+=shell.mesh.triangles.size();
                            record["support_id"]=id;record["triangles"]=shell.mesh.triangles.size();record["inner_footprint"]=PointsJson(shell.inner);
                            Aabb a;for(const auto& v:shell.mesh.vertices)a.Expand(v);added_bounds.push_back(a);
                        }
                    }
                } catch(const std::exception& e) {decision="geometry_omitted";record["reason"]=e.what();}
            }
            record["decision"]=decision;
        }
        if(!geometry.empty())result.scene_text+=kBegin+geometry+kEnd;
        // Exercise the persisted representation, including BVH and optical material.
        Scene reloaded;
        if(!LoadSceneFromSceneText("glass-support-final",result.scene_text.c_str(),&reloaded,error,sizeof(error)))throw std::runtime_error(error);
        result.success=true;
    } catch(const std::exception& e) {
        result.error=e.what();result.scene_text=result.opaque_scene_text;result.added_supports=0;result.added_triangles=0;
        for(auto& block:report["blocks"])if(block["decision"]=="added"||block["decision"]=="component_supported")
            block["decision"]="pass_failed";
    }
    report["success"]=result.success;report["error"]=result.error;
    report["added_supports"]=result.added_supports;report["added_triangles"]=result.added_triangles;
    report["construction_ms"]=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
    result.report_json=report.dump(2);return result;
}
} // namespace liminal
