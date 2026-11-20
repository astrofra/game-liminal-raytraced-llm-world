#include "renderer.h"
#include <algorithm>
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

static size_t ChangedPixels(const std::vector<unsigned char>& a, const std::vector<unsigned char>& b)
{
    Check(a.size() == b.size() && a.size() % 3 == 0, "comparable RGB pixel buffers");
    size_t changed = 0;
    for (size_t offset = 0; offset < a.size(); offset += 3) {
        if (a[offset] != b[offset] || a[offset + 1] != b[offset + 1] || a[offset + 2] != b[offset + 2]) ++changed;
    }
    return changed;
}

static size_t ChromaticPixels(const std::vector<unsigned char>& pixels)
{
    size_t count = 0;
    for (size_t i = 0; i < pixels.size(); i += 3) {
        const auto channels = {pixels[i], pixels[i + 1], pixels[i + 2]};
        if (std::max(channels) - std::min(channels) >= 3) ++count;
    }
    return count;
}

// Neutral lighting and contrasting edges running through the droplet field.
static Scene LoadVisorDropScene()
{
    std::string text =
        "camera eye(0,10,0) target(0,0,0) up(0,0,1) fov(60)\n"
        "plane \"floor\" pos(0,0,0) normal(0,1,0) size(1000,1000) gray(0.8)\n"
        "distant_light direction(0,-1,0) intensity(2)\n";
    for (int i = -5; i <= 5; ++i) {
        char bars[256] = {};
        std::snprintf(bars, sizeof(bars),
            "box \"bar_x%d\" pos(%.2f,0.05,0) size(0.5,0.02,14) gray(0.05)\n"
            "box \"bar_z%d\" pos(0,0.05,%.2f) size(14,0.02,0.5) gray(0.05)\n",
            i, 1.25 * i, i, 1.25 * i);
        text += bars;
    }
    return Load(text);
}

static RenderConfig DropConfig(float strength, uint32_t drop_seed)
{
    RenderConfig cfg;
    cfg.width = 128; cfg.height = 96; cfg.samples_per_pixel = 4;
    cfg.max_bounces = 1; cfg.direct_light_samples = 2;
    cfg.visor_drop_strength = strength;
    cfg.visor_drop_seed = drop_seed;
    return cfg;
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

    // Visor drops: deterministic, local chromatic warps of the camera ray field.
    const Scene drop_scene = LoadVisorDropScene();
    const auto drop_base = Render(drop_scene, DropConfig(0.0f, 0u));
    Check(Render(drop_scene, DropConfig(0.0f, 57u)) == drop_base, "zero drop strength ignores the drop seed");
    const auto hot = DropConfig(1.0f, 0x1234abcdu);
    const auto dropped = Render(drop_scene, hot);
    Check(Render(drop_scene, hot) == dropped, "same drop seed reproduces the frame");
    const auto affected = ChangedPixels(drop_base, dropped);
    Check(affected > 0 && affected < drop_base.size() / 6, "visor drops alter a localized region");
    Check(Render(drop_scene, DropConfig(1.0f, 0x0badf00du)) != dropped, "another drop seed moves the warps");
    Check(Mean(drop_base) > 10 && ChromaticPixels(drop_base) == 0, "a cold visor keeps the neutral scene gray");
    const auto warm = Render(drop_scene, DropConfig(.45f, hot.visor_drop_seed));
    const auto warm_pixels = ChromaticPixels(warm);
    const auto hot_pixels = ChromaticPixels(dropped);
    Check(warm_pixels > 0 && warm_pixels < hot_pixels, "local chromatic dispersion grows with heat");
    Check(hot_pixels <= affected, "chromatic dispersion stays within the warped region");
    std::puts("Visor drops: deterministic, localized warps with heat-dependent chromatic dispersion passed.");
    std::puts("Distant light: parsing, reset, direction, distance, occlusion, softness, color and GI passed.");
    return 0;
}
