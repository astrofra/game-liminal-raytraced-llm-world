#include "archive_camera.h"
#include "renderer.h"
#include <cstdlib>
#include <iostream>

using namespace liminal;
static void Check(bool condition, const char* message)
{
    if (!condition) { std::cerr << message << '\n'; std::exit(1); }
}

int main()
{
    Scene scene; char error[512] = {};
    Check(LoadSceneFromSceneText("camera test",
        "plane \"floor\" pos(0,0,0) normal(0,1,0) size(1000,1000) gray(0.3)\n"
        "box \"rotated\" pos(7,4,2) size(5,8,3) rot(17,35,8) gray(0.5)\n"
        "distant_light direction(-1,-1,0) intensity(2)\n", &scene,error,sizeof(error)),error);
    RenderConfig config; config.width=config.height=96; config.samples_per_pixel=2;
    Check(config.orthographic_height==0,"Existing perspective must remain the default");
    FrameArchiveScene(&scene,&config,45,40);
    Check(config.orthographic_height<30,"Large ground must not dominate the framing");
    const Vec3 forward=Normalize(scene.camera.target-scene.camera.eye);
    const Vec3 right=Normalize(Cross(forward,scene.camera.up));
    const Vec3 up=Normalize(Cross(right,forward));
    for (const auto& box:scene.boxes) for (const auto& p:box.corners) {
        const auto delta=p-scene.camera.target;
        Check(std::abs(Dot(delta,right))<config.orthographic_height*.5f,"Horizontal clipping");
        Check(std::abs(Dot(delta,up))<config.orthographic_height*.5f,"Vertical clipping");
    }
    std::vector<unsigned char> first,second;
    Check(RenderSceneToPixels(scene,config,&first),"First render");
    scene.camera.eye=scene.camera.eye-forward*100;
    Check(RenderSceneToPixels(scene,config,&second),"Second render");
    // Orthographic magnification must be independent of camera distance.
    // Floating-point intersection differences can affect isolated edge pixels.
    int changed=0;
    for (size_t i=0;i<first.size();i+=3)
        if (std::abs(int(first[i])-second[i])>3 || std::abs(int(first[i+1])-second[i+1])>3 ||
            std::abs(int(first[i+2])-second[i+2])>3) ++changed;
    Check(changed<20,"Orthographic image changed when camera moved backward");
    const auto before=scene.bounds;
    for (auto& t:scene.triangles) { t.v0.x+=50; t.v1.x+=50; t.v2.x+=50; }
    RebuildSceneGeometry(&scene);
    Check(std::abs(scene.bounds.min.x-before.min.x-50)<.001f,"Rebuilt bounds");
    Hit hit;
    Check(ProbeSceneRay(scene,{Vec3(57,30,2),Vec3(0,-1,0)},100,&hit),"Rebuilt BVH must hit translated geometry");
    Check(hit.position.y>1,"Translated box must be above ground");
    std::cout << "Archive camera and assembled BVH checks passed\n";
}
