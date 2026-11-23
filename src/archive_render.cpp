#include "archive_camera.h"
#include "archive_caption.h"
#include "renderer.h"
#include "scene_hash.h"
#include "nlohmann/json.hpp"
#include "stb_image_write.h"
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_WINDOWS_UTF8
#include "stb/stb_image.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <queue>
#include <set>
#include <sstream>
#include <cstdlib>

namespace fs = std::filesystem;
using nlohmann::json;
using namespace liminal;

namespace {
json ReadJson(const fs::path& path)
{
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("Cannot read " + path.u8string());
    return json::parse(input);
}

Scene Load(const fs::path& path)
{
    Scene result;
    char error[1024] = {};
    if (!LoadSceneFromPath(path.u8string().c_str(), &result, error, sizeof(error)))
        throw std::runtime_error(error);
    return result;
}

fs::path LatestArchive(const fs::path& root)
{
    fs::path latest;
    std::string latest_key;
    for (const auto& entry : fs::directory_iterator(root)) {
        if (!entry.is_directory() || !fs::is_regular_file(entry.path() / "manifest.json")) continue;
        const auto manifest = ReadJson(entry.path() / "manifest.json");
        const auto key = manifest.at("started_utc").get<std::string>() + entry.path().filename().u8string();
        if (key > latest_key) { latest_key = key; latest = entry.path(); }
    }
    if (latest.empty()) throw std::runtime_error("No research archive in " + root.u8string());
    return latest;
}

struct Place {
    std::string id, title, stem, hash;
    Scene scene;
    int grid_x = 0, grid_z = 0;
    float yaw = 0, scale = 1;
    Vec3 center, anchor;
};

std::pair<int, int> Step(const std::string& direction)
{
    if (direction == "north") return {0, 1};
    // +Z is north; camera right when facing north is -X.
    if (direction == "east") return {-1, 0};
    if (direction == "south") return {0, -1};
    if (direction == "west") return {1, 0};
    throw std::runtime_error("Unknown archived direction: " + direction);
}

std::vector<Place> ReadPlaces(const fs::path& archive, const json& state)
{
    std::vector<fs::path> paths;
    for (const auto& file : fs::directory_iterator(archive / "places")) {
        const auto stem = file.path().stem().u8string();
        if (file.path().extension() == ".json" && stem.rfind("place-", 0) == 0 &&
            stem.find('.') == std::string::npos) paths.push_back(file.path());
    }
    std::sort(paths.begin(), paths.end());
    std::vector<Place> result;
    for (const auto& path : paths) {
        const auto metadata = ReadJson(path);
        Place place;
        place.id = metadata.at("place_id");
        place.title = metadata.at("title");
        place.stem = path.stem().u8string();
        place.hash = metadata.value("scene_sha256", "");
        const auto scene_path=archive / "places" / (place.stem + ".scene");
        std::ifstream scene_file(scene_path,std::ios::binary);
        if (!scene_file) throw std::runtime_error("Missing accepted scene: "+scene_path.u8string());
        std::ostringstream source; source<<scene_file.rdbuf();
        if (place.hash.empty() || SceneTextHash(source.str())!=place.hash)
            throw std::runtime_error("Scene hash mismatch: "+place.stem);
        place.scene = Load(scene_path);
        bool facing_known=false;
        // Same canonical facing as KnownBarrierSide in known_barrier.cpp.
        if (place.id=="canonical:quarry_threshold") { place.yaw=-kPi*.5f; facing_known=true; }
        if (metadata.contains("provenance")) {
            const auto brief = metadata.at("provenance").value("brief_and_landmark", "");
            if (!brief.empty() && brief.front() == '{') {
                const auto provenance = json::parse(brief);
                if (provenance.contains("forward_direction")) {
                    const auto step = Step(provenance.at("forward_direction"));
                    place.yaw = std::atan2(static_cast<float>(step.first), static_cast<float>(step.second));
                    facing_known=true;
                } else if (provenance.contains("return_direction")) {
                    // Legacy local-topology archives store only the way back.
                    // Same opposite-direction rule as KnownBarrierSide.
                    const auto back=Step(provenance.at("return_direction"));
                    place.yaw=std::atan2(static_cast<float>(-back.first),static_cast<float>(-back.second));
                    facing_known=true;
                }
            }
        }
        if (!facing_known) throw std::runtime_error("No archived facing for "+place.id);
        const auto local_forward=place.scene.camera.target-place.scene.camera.eye;
        place.yaw-=std::atan2(local_forward.x,local_forward.z);
        result.push_back(std::move(place));
    }
    if (result.empty()) throw std::runtime_error("Archive contains no accepted places");
    std::set<std::string> expected{state.at("origin_place_id").get<std::string>()};
    for (const auto& room : state.at("generated_rooms"))
        expected.insert(room.at("room_id").get<std::string>());
    std::set<std::string> actual;
    for (const auto& place : result)
        if (!actual.insert(place.id).second) throw std::runtime_error("Duplicate archived place");
    if (actual != expected) throw std::runtime_error("Accepted place files do not match state.json");
    return result;
}

void Layout(std::vector<Place>& places, const json& state)
{
    std::map<std::string, std::pair<int, int>> positions;
    const std::string origin = state.at("origin_place_id");
    positions[origin] = {0, 0};
    std::queue<std::string> queue;
    queue.push(origin);
    while (!queue.empty()) {
        const auto from = queue.front(); queue.pop();
        for (const auto& edge : state.at("room_links")) {
            if (edge.at("from_place_id") != from) continue;
            const std::string to = edge.at("to_place_id");
            const auto delta = Step(edge.at("direction"));
            const auto next = std::make_pair(positions.at(from).first + delta.first,
                                             positions.at(from).second + delta.second);
            const auto inserted = positions.emplace(to, next);
            if (inserted.second) queue.push(to);
            else if (inserted.first->second != next)
                throw std::runtime_error("Graph has inconsistent cardinal positions; cannot lay out faithfully");
        }
    }
    std::set<std::pair<int, int>> occupied;
    for (auto& place : places) {
        if (!positions.count(place.id)) throw std::runtime_error("Disconnected place: " + place.id);
        const auto p = positions.at(place.id);
        if (!occupied.insert(p).second) throw std::runtime_error("Overlapping graph coordinates; manual layout required");
        place.grid_x = p.first; place.grid_z = p.second;
    }
    if (positions.size() != places.size()) throw std::runtime_error("Graph references unarchived places");
}

Vec3 Rotate(const Vec3& p, float yaw)
{
    return Vec3(std::cos(yaw) * p.x + std::sin(yaw) * p.z, p.y,
                -std::sin(yaw) * p.x + std::cos(yaw) * p.z);
}

void AppendScene(Scene& target, const Scene& source)
{
    const int offset = static_cast<int>(target.materials.size());
    target.materials.insert(target.materials.end(), source.materials.begin(), source.materials.end());
    for (auto triangle : source.triangles) {
        triangle.material_index += offset;
        target.triangles.push_back(triangle);
    }
    target.boxes.insert(target.boxes.end(), source.boxes.begin(), source.boxes.end());
}

void AddScript(Scene& target, const std::string& text)
{
    Scene props; char error[1024] = {};
    if (!LoadSceneFromSceneText("atlas presentation", text.c_str(), &props, error, sizeof(error)))
        throw std::runtime_error(error);
    AppendScene(target, props);
}

Scene Assemble(std::vector<Place>& places, const json& state)
{
    Scene world;
    // Shared presentation light: independent room suns cannot be combined.
    world.distant_light.enabled = true;
    world.distant_light.direction = Normalize(Vec3(-.6f, -1, .35f));
    world.distant_light.color = Vec3(1, .92f, .78f);
    world.distant_light.intensity = 3.0f;
    world.distant_light.angular_radius_degrees = 1.0f;
    world.sky_background.enabled = true;
    world.sky_background.zenith_luminance = .16f;
    world.sky_background.horizon_luminance = .16f;
    world.sky_background.nadir_luminance = .16f;
    world.sky_background.noise_amount = 0;
    world.sky_background.star_density = 0;
    std::ostringstream props;
    constexpr float pitch = 64, tile = 49;
    for (auto& place : places) {
        Aabb bounds;
        if (place.scene.boxes.empty()) throw std::runtime_error("No authored boxes for atlas framing: " + place.id);
        for (const auto& box : place.scene.boxes)
            for (const auto& p : box.corners) bounds.Expand(Rotate(p, place.yaw));
        const Vec3 extent = bounds.max - bounds.min;
        place.center = (bounds.min + bounds.max) * .5f;
        place.center.y = 0;
        place.scale = 40.0f / std::max(1.0f, std::max(extent.x, extent.z));
        const Vec3 offset(place.grid_x * pitch, 0, place.grid_z * pitch);
        place.anchor = offset + Vec3(-tile * .38f, 0, -tile * .38f);
        Scene local = place.scene;
        std::vector<int> material_uses(local.materials.size(),0);
        for (const auto& triangle:local.triangles) ++material_uses.at(triangle.material_index);
        // Remove only large horizontal floor triangles at y=0. Keep every
        // authored volume, including below-ground parts, glass and supports.
        local.triangles.erase(std::remove_if(local.triangles.begin(), local.triangles.end(),
            [&](const Triangle& t) {
                return std::abs(t.v0.y) < .001f && std::abs(t.v1.y) < .001f &&
                       std::abs(t.v2.y) < .001f && t.area > 100.0f && material_uses.at(t.material_index)==2;
            }), local.triangles.end());
        auto transform = [&](const Vec3& p) { return (Rotate(p, place.yaw) - place.center) * place.scale + offset; };
        for (auto& triangle : local.triangles) {
            triangle.v0 = transform(triangle.v0);
            triangle.v1 = transform(triangle.v1);
            triangle.v2 = transform(triangle.v2);
        }
        for (auto& box : local.boxes) {
            box.center = transform(box.center); box.size *= place.scale;
            for (auto& axis : box.axes) axis = Rotate(axis, place.yaw);
            for (auto& corner : box.corners) corner = transform(corner);
        }
        AppendScene(world, local);
        props << "box \"tile\" pos(" << offset.x << ",-0.85," << offset.z
              << ") size(" << tile << ",1.7," << tile << ") gray(0.29)\n";
    }
    std::map<std::string, const Place*> lookup;
    for (const auto& place : places) lookup[place.id] = &place;
    std::set<std::pair<std::string, std::string>> drawn;
    for (const auto& edge : state.at("room_links")) {
        const std::string a = edge.at("from_place_id"), b = edge.at("to_place_id");
        if (!drawn.emplace(std::min(a,b), std::max(a,b)).second) continue;
        const auto& first = *lookup.at(a); const auto& second = *lookup.at(b);
        const float x = (first.grid_x + second.grid_x) * pitch * .5f;
        const float z = (first.grid_z + second.grid_z) * pitch * .5f;
        props << "box \"graph_link\" pos(" << x << ",-0.7," << z << ") size("
              << (first.grid_x == second.grid_x ? 1.0f : pitch) << ",0.5,"
              << (first.grid_z == second.grid_z ? 1.0f : pitch) << ") gray(0.75)\n";
    }
    AddScript(world, props.str());
    RebuildSceneGeometry(&world);
    return world;
}

// Small built-in display font: no platform font, SDL, or image dependencies.
const char* Glyph(char c)
{
    static const char* letters[] = {
        "01110100011000111111100011000110001", "11110100011000111110100011000111110",
        "01111100001000010000100001000001111", "11110100011000110001100011000111110",
        "11111100001000011110100001000011111", "11111100001000011110100001000010000",
        "01111100001000010111100011000101111", "10001100011000111111100011000110001",
        "11111001000010000100001000010011111", "00111000100001000010100101001001100",
        "10001100101010011000101001001010001", "10000100001000010000100001000011111",
        "10001110111010110101100011000110001", "10001110011010110011100011000110001",
        "01110100011000110001100011000101110", "11110100011000111110100001000010000",
        "01110100011000110001101011001001101", "11110100011000111110101001001010001",
        "01111100001000001110000010000111110", "11111001000010000100001000010000100",
        "10001100011000110001100011000101110", "10001100011000110001100010101000100",
        "10001100011000110101101011010101010", "10001100010101000100010101000110001",
        "10001100010101000100001000010000100", "11111000010001000100010001000011111"};
    static const char* digits[] = {
        "01110100011001110101110011000101110", "00100011000010000100001000010001110",
        "01110100010000100010001000100011111", "11110000010000101110000010000111110",
        "00010001100101010010111110001000010", "11111100001000011110000010000111110",
        "01110100001000011110100011000101110", "11111000010001000100010000100001000",
        "01110100011000101110100011000101110", "01110100011000101111000010000101110"};
    if (c >= 'A' && c <= 'Z') return letters[c-'A'];
    if (c >= '0' && c <= '9') return digits[c-'0'];
    if (c == '-') return "00000000000000011111000000000000000";
    if (c == '/') return "00001000100001000100010000100010000";
    return "00000000000000000000000000000000000";
}

void Text(std::vector<unsigned char>& pixels, int size, int x, int y,
          const std::string& text, int scale, Vec3 color)
{
    for (char c : text) {
        const char* glyph = Glyph(c);
        for (int row=0; row<7; ++row) for (int col=0; col<5; ++col) {
            if (glyph[row*5+col] != '1') continue;
            for (int dy=0; dy<scale; ++dy) for (int dx=0; dx<scale; ++dx) {
                const int px=x+col*scale+dx, py=y+row*scale+dy;
                if (px<0 || py<0 || px>=size || py>=size) continue;
                const auto index=static_cast<size_t>((py*size+px)*3);
                pixels[index]=static_cast<unsigned char>(color.x);
                pixels[index+1]=static_cast<unsigned char>(color.y);
                pixels[index+2]=static_cast<unsigned char>(color.z);
            }
        }
        x += 6*scale;
    }
}

std::pair<int,int> Project(const Scene& scene, const RenderConfig& config, const Vec3& point)
{
    const Vec3 forward = Normalize(scene.camera.target-scene.camera.eye);
    const Vec3 right = Normalize(Cross(forward, scene.camera.up));
    const Vec3 up = Normalize(Cross(right,forward));
    const Vec3 delta = point-scene.camera.target;
    return {static_cast<int>((.5f+Dot(delta,right)/config.orthographic_height)*config.width),
            static_cast<int>((.5f-Dot(delta,up)/config.orthographic_height)*config.height)};
}

int Run(const std::vector<std::string>& args)
{
    fs::path archive, research, annotate, output = "documentation/rendus-scenes/derniere-partie.png";
    std::string selected;
    RenderConfig config;
    config.width = config.height = 1600; config.samples_per_pixel = 64;
    float azimuth=45, elevation=40;
    for (size_t i=1; i<args.size(); ++i) {
        const auto& arg=args[i];
        if (arg == "--help" || arg == "-h") {
            std::cout << "liminal_archive_render [--archive DIR | --research-dir ROOT] [--output FILE.png]\n"
                         "  [--size 256..4096] [--samples 1..4096] [--azimuth DEGREES] [--elevation 10..80]\n"
                         "  [--place PLACE_ID]  Default: all accepted places in the newest archived game.\n"
                         "  [--annotate OLD.png] Add titles to a matching legacy PNG without raytracing.\n"
                         "  Output is a square orthographic atlas; tile spacing/scales are presentation only.\n";
            return 0;
        }
        if (i+1 == args.size()) throw std::runtime_error("Missing value for " + arg);
        const auto value=args[++i];
        if (arg=="--archive") archive=fs::u8path(value);
        else if (arg=="--research-dir") research=fs::u8path(value);
        else if (arg=="--output") output=fs::u8path(value);
        else if (arg=="--annotate") annotate=fs::u8path(value);
        else if (arg=="--place") selected=value;
        else if (arg=="--size" || arg=="--samples") {
            size_t end=0; const int number=std::stoi(value,&end);
            if (end!=value.size() || number<(arg=="--size"?256:1) || number>4096)
                throw std::runtime_error("Invalid value for " + arg);
            if (arg=="--size") config.width=config.height=number;
            else config.samples_per_pixel=number;
        } else if (arg=="--azimuth" || arg=="--elevation") {
            size_t end=0; const float number=std::stof(value,&end);
            if (end!=value.size() || !std::isfinite(number) ||
                (arg=="--elevation" && (number<10 || number>80)))
                throw std::runtime_error("Invalid angle for " + arg);
            if (arg=="--azimuth") azimuth=std::fmod(number,360.0f); else elevation=number;
        } else throw std::runtime_error("Unknown option: " + arg);
    }
    if (!archive.empty() && !research.empty()) throw std::runtime_error("Choose --archive or --research-dir");
    if (output.extension() != ".png") throw std::runtime_error("Output must end in .png");
    if (archive.empty()) {
        if (research.empty()) {
#if defined(_WIN32)
            const auto local=_wgetenv(L"LOCALAPPDATA");
#else
            const auto local=std::getenv("LOCALAPPDATA");
#endif
            if (!local) throw std::runtime_error("Pass --research-dir or --archive");
            research=fs::path(local)/"WithinTheLatentWalls/research";
        }
        archive=LatestArchive(research);
    }
    archive=fs::canonical(archive);
    output=fs::weakly_canonical(fs::absolute(output));
    const auto relative=output.lexically_relative(archive);
    if (!relative.empty() && *relative.begin()!="..") throw std::runtime_error("Output must be outside the source archive");
    const auto state=ReadJson(archive/"state.json");
    auto places=ReadPlaces(archive,state);
    Scene scene;
    if (selected.empty()) { Layout(places,state); scene=Assemble(places,state); }
    else {
        const auto found=std::find_if(places.begin(),places.end(),[&](const Place& p){return p.id==selected;});
        if (found==places.end()) throw std::runtime_error("Place not found: "+selected);
        Place place=std::move(*found); places.clear(); places.push_back(std::move(place));
        scene=places.front().scene;
    }
    FrameArchiveScene(&scene,&config,azimuth,elevation);
    // Leave space for a title and footer outside the projected world bounds.
    config.orthographic_height *= 1.12f;
    std::vector<unsigned char> pixels;
    auto vector=[](const Vec3& v){return json::array({v.x,v.y,v.z});};
    if (!annotate.empty()) {
        auto metadata_path=annotate; metadata_path.replace_extension(".json");
        const auto previous=ReadJson(metadata_path);
        bool matches=!previous.contains("labels_version") &&
            previous.at("archive")==archive.filename().u8string() && previous.at("size")==config.width &&
            previous.at("samples")==config.samples_per_pixel && previous.at("seed")==config.seed &&
            previous.at("azimuth")==azimuth && previous.at("elevation")==elevation &&
            previous.at("orthographic_height")==config.orthographic_height &&
            previous.at("camera_eye")==vector(scene.camera.eye) &&
            previous.at("camera_target")==vector(scene.camera.target) && previous.at("links")==state.at("room_links") &&
            previous.at("mode")== (selected.empty()?"topological_atlas":"single_place") &&
            previous.at("places").size()==places.size();
        for (size_t i=0;matches && i<places.size();++i) {
            const auto& p=previous.at("places").at(i);
            matches=p.at("id")==places[i].id && p.at("source_sha256")==places[i].hash &&
                p.at("scale")==places[i].scale && p.at("yaw_degrees")==places[i].yaw*180/kPi &&
                p.at("grid")==json::array({places[i].grid_x,places[i].grid_z}) &&
                p.at("local_center")==vector(places[i].center);
        }
        if (!matches) throw std::runtime_error("Legacy PNG does not match this scene/configuration or is already labelled");
        int width=0,height=0,channels=0;
        auto* data=stbi_load(annotate.u8string().c_str(),&width,&height,&channels,3);
        if (!data) throw std::runtime_error("Cannot read legacy PNG");
        if (width!=config.width || height!=config.height) { stbi_image_free(data); throw std::runtime_error("Legacy PNG dimensions mismatch"); }
        pixels.assign(data,data+static_cast<size_t>(width)*height*3); stbi_image_free(data);
        std::cout<<"Adding archived titles without raytracing\n";
    } else {
    std::cout << "Archive: " << archive.filename().u8string() << "\nPlaces: " << places.size()
              << "\nRendering " << config.width << " x " << config.height << " / "
              << config.samples_per_pixel << " samples\n" << std::flush;
    if (!RenderSceneToPixels(scene,config,&pixels)) throw std::runtime_error("Render failed");
    const int font=std::max(1,config.width/400);
    const int margin=config.width/25;
    Text(pixels,config.width,margin,margin,"ATLAS DES LIEUX",font*2,Vec3(245,239,225));
    Text(pixels,config.width,margin,margin+font*19,"ENTRE LES MURS LATENTS",font,Vec3(216,210,197));
    if (selected.empty()) {
        for (size_t i=0;i<places.size();++i) {
            const auto point=Project(scene,config,places[i].anchor);
            const auto label=(i<9?"0":"")+std::to_string(i+1);
            Text(pixels,config.width,point.first+1,point.second+1,label,font*2,Vec3(20));
            Text(pixels,config.width,point.first,point.second,label,font*2,Vec3(250,242,216));
        }
    }
    Text(pixels,config.width,margin,config.height-margin-font*19,
         std::to_string(places.size())+" LIEUX / VUE ORTHOGRAPHIQUE",font,Vec3(235,227,210));
    Text(pixels,config.width,margin,config.height-margin-font*7,
         selected.empty()?(config.width<600?"LIENS ARCHIVES - ESPACEMENT LIBRE":
         "LIENS ARCHIVES - ECHELLES LOCALES - ESPACEMENT LIBRE"):"GEOMETRIE ARCHIVEE - VUE DE DESSUS",font,Vec3(216,210,197));
    }
    json captions=json::array();
#if defined(_WIN32)
    std::vector<CaptionBounds> occupied;
    if (selected.empty()) {
        const int number_scale=std::max(1,config.width/400)*2;
        for (const auto& place:places) {
            const auto number=Project(scene,config,place.anchor);
            occupied.push_back({number.first-2,number.second-2,number_scale*12+4,number_scale*7+4,0});
        }
    }
#endif
    for (const auto& place:places) {
        int x=config.width/2, y=static_cast<int>(config.height*.86f);
        int width=config.width*4/5, font_pixels=std::max(12,config.width/48);
        if (selected.empty()) {
            int left=config.width,right=0,bottom=0;
            for (float dx:{-24.5f,24.5f}) for (float dz:{-24.5f,24.5f}) {
                const auto point=Project(scene,config,Vec3(place.grid_x*64.0f+dx,-1.7f,place.grid_z*64.0f+dz));
                left=std::min(left,point.first); right=std::max(right,point.first); bottom=std::max(bottom,point.second);
            }
            x=(left+right)/2; y=bottom+std::max(2,config.width/512);
            width=std::max(48,static_cast<int>((right-left)*.70f));
            font_pixels=2*std::max(8,std::min(config.width/85,width/14));
            width=static_cast<int>(width*1.6f);
        }
#if defined(_WIN32)
        const auto bounds=DrawArchiveCaption(pixels,config.width,x,y,width,font_pixels,place.title,
            config.height-config.width/25-std::max(1,config.width/400)*25,
            selected.empty()?&occupied:nullptr);
        if (selected.empty()) {
            occupied.push_back(bounds);
            const int target_x=bounds.x+bounds.width/2, target_y=bounds.y-3;
            const int distance=std::max(std::abs(target_x-x),std::abs(target_y-y));
            if (distance>font_pixels) for (int step=0;step<=distance;++step) {
                const int px=x+(target_x-x)*step/distance, py=y+(target_y-y)*step/distance;
                if (px<0 || py<0 || px>=config.width || py>=config.height) continue;
                const size_t index=static_cast<size_t>((py*config.width+px)*3);
                for (int channel=0;channel<3;++channel)
                    pixels[index+channel]=static_cast<unsigned char>(pixels[index+channel]*.5f+110);
            }
        }
        captions.push_back({{"place",place.id},{"text",place.title},{"font_pixels",bounds.font_pixels},
            {"bounds",{bounds.x,bounds.y,bounds.width,bounds.height}}});
#else
        // Keep the standalone renderer usable without a platform text library.
        // The Windows build supplies full Unicode captions via GDI.
        std::string ascii=place.title;
        for (auto& c:ascii) if (c>='a'&&c<='z') c=static_cast<char>(c-'a'+'A');
        const int scale=std::max(1,font_pixels/8);
        Text(pixels,config.width,std::max(8,x-static_cast<int>(ascii.size())*3*scale),y,ascii,scale,Vec3(250,245,225));
        captions.push_back({{"place",place.id},{"text",place.title},{"font_pixels",scale*7}});
#endif
    }
    fs::create_directories(output.parent_path());
    if (!stbi_write_png(output.u8string().c_str(),config.width,config.height,3,pixels.data(),config.width*3))
        throw std::runtime_error("Cannot write PNG");
    json report={{"archive",archive.filename().u8string()},{"mode",selected.empty()?"topological_atlas":"single_place"},
        {"size",config.width},{"samples",config.samples_per_pixel},{"seed",config.seed},
        {"azimuth",azimuth},{"elevation",elevation},{"orthographic_height",config.orthographic_height},
        {"camera_eye",vector(scene.camera.eye)},{"camera_target",vector(scene.camera.target)},
        {"source_kind","accepted places/*.scene"},{"current_place",state.at("current_place_id")},
        {"presentation",selected.empty()?"Normalized local scales, cropped ground tiles, shared sun; links are not physical corridors":"Original geometry/sun/sky, player spotlight disabled"}};
    report["places"]=json::array();
    for (size_t i=0;i<places.size();++i) {
        const auto& p=places[i];
        report["places"].push_back({{"number",i+1},{"id",p.id},{"title",p.title},
            {"source","places/"+p.stem+".scene"},{"source_sha256",p.hash},
            {"grid",{p.grid_x,p.grid_z}},{"scale",p.scale},{"yaw_degrees",p.yaw*180/kPi},
            {"local_center",vector(p.center)}});
    }
    report["links"]=state.at("room_links");
    report["labels_version"]=selected.empty()?2:1;
    report["captions"]=captions;
    auto report_path=output; report_path.replace_extension(".json");
    std::ofstream file(report_path,std::ios::binary); file<<report.dump(2)<<'\n'; file.flush();
    if (!file) throw std::runtime_error("Cannot write render provenance");
    std::cout << "PNG: " << output.u8string() << '\n';
    return 0;
}
} // namespace

#if defined(_WIN32)
int wmain(int argc, wchar_t** argv)
#else
int main(int argc, char** argv)
#endif
{
    try {
        std::vector<std::string> args;
        for (int i=0;i<argc;++i) args.push_back(fs::path(argv[i]).u8string());
        return Run(args);
    } catch (const std::exception& e) { std::cerr<<"Archive render: "<<e.what()<<'\n'; return 1; }
}
