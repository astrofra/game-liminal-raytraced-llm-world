#include "renderer.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

using namespace liminal;

static void Check(bool value, const char* message)
{
    if (!value) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
}

static Scene Load(const std::string& text)
{
    Scene scene;
    char error[512] = {};
    Check(LoadSceneFromSceneText("distant-test", text.c_str(), &scene, error, sizeof(error)), error);
    return scene;
}

static RenderConfig Config()
{
    RenderConfig cfg;
    cfg.width = 8; cfg.height = 8; cfg.samples_per_pixel = 64;
    cfg.max_bounces = 1; cfg.direct_light_samples = 4;
    return cfg;
}

static std::vector<unsigned char> Render(const Scene& scene, const RenderConfig& cfg)
{
    std::vector<unsigned char> pixels;
    Check(RenderSceneToPixels(scene, cfg, &pixels), "render");
    return pixels;
}

static double Mean(const std::vector<unsigned char>& pixels)
{
    double sum = 0;
    for (unsigned char value : pixels) sum += value;
    return sum / pixels.size();
}

int main()
{
    const std::string floor =
        "camera eye(0,5,0) target(0,0,0) up(0,0,1) fov(0.01)\n"
        "plane \"floor\" pos(0,0,0) normal(0,1,0) size(1000,1000) gray(0.5)\n";
    const std::string sun = "distant_light direction(-1,-1,0) intensity(2)\n";
    Scene scene = Load(floor + sun);
    Check(scene.distant_light.enabled, "light enabled");
    Check(std::fabs(Length(scene.distant_light.direction) - 1.0f) < 1e-6f, "direction normalized");
    Check(scene.emissive_triangles.empty() && scene.triangles.size() == 2, "no light geometry");
    Check(scene.distant_light.angular_radius_degrees == 0, "sharp by default");
    RenderConfig cfg = Config();
    const auto lit = Render(scene, cfg);
    Check(Mean(lit) > 70, "sun lights diffuse surface");
    scene.camera.eye.x = 200; scene.camera.target.x = 200;
    Check(Render(scene, cfg) == lit, "infinite source does not attenuate or move with camera");
    scene.distant_light.direction = -scene.distant_light.direction;
    Check(Mean(Render(scene, cfg)) == 0, "back-facing surface receives no direct sunlight");
    scene.distant_light.intensity = 0;
    Check(Mean(Render(scene, cfg)) == 0, "zero intensity");
    char error[512] = {};
    Check(LoadSceneFromSceneText("reset", floor.c_str(), &scene, error, sizeof(error)), "reload");
    Check(!scene.distant_light.enabled, "reload resets absent sun");
    Check(Mean(Render(scene, cfg)) == 0, "legacy scene stays unlit without sources");
    for (const char* bad : {
        "direction(0,0,0) intensity(1)", "direction(nan,0,0) intensity(1)",
        "direction(0,-1,0) intensity(inf)", "direction(0,-1,0) intensity(-1)",
        "direction(0,-1,0) intensity(1) angular_radius(-1)",
        "direction(0,-1,0) intensity(1) angular_radius(11)",
        "direction(0,-1,0) intensity(1) angular_radius(oops)",
        "direction(0,-1,0) intensity(1) color(1,2,1)",
        "direction(0,-1,0) intensity(1) color(1,nan,1)",
        "direction(0,-1,0) intensity(1) color(oops)", "intensity(1)"}) {
        const std::string text = floor + "distant_light " + bad + "\n";
        Check(!LoadSceneFromSceneText("invalid", text.c_str(), &scene, error, sizeof(error)), bad);
    }
    scene = Load(floor + sun + "box \"occluder\" pos(1.6,2,0) size(0.8,0.8,4) gray(0.5)\n");
    Check(Mean(Render(scene, cfg)) == 0, "opaque geometry blocks sun");
    scene.camera.eye.x = 0.5f; scene.camera.target.x = 0.5f;
    const double sharp = Mean(Render(scene, cfg));
    Check(sharp > 70, "sample just outside hard shadow is lit");
    scene.distant_light.angular_radius_degrees = 10;
    const double soft = Mean(Render(scene, cfg));
    Check(soft > 5 && soft < sharp - 5, "finite angle produces a penumbra");
    scene = Load(floor + sun + "distant_light direction(-1,-1,0) intensity(2) angular_radius(10)\n");
    Check(std::fabs(Mean(Render(scene, cfg)) - Mean(lit)) < 2, "softness preserves unoccluded energy");
    scene = Load(floor + "distant_light direction(0,-1,0) intensity(2) color(1,0.5,0.1)\n");
    auto colored = Render(scene, cfg);
    Check(colored[0] > colored[1] && colored[1] > colored[2], "light color reaches surface");
    scene = Load(
        "camera eye(0,1,-3) target(0,1,0) up(0,1,0) fov(10)\n"
        "plane \"wall\" pos(0,1,0) normal(0,0,-1) size(6,4) gray(0.5)\n"
        "plane \"floor\" pos(0,0,0) normal(0,1,0) size(10,10) gray(0.5)\n"
        "distant_light direction(0,-1,0) intensity(3)\n");
    Check(Mean(Render(scene, cfg)) == 0, "wall has no direct light from vertical sun");
    cfg.max_bounces = 3;
    Check(Mean(Render(scene, cfg)) > 10, "sunlight reaches wall through diffuse bounce");
    std::puts("Distant light: parsing, reset, direction, distance, occlusion, softness, color and GI passed.");
    return 0;
}
