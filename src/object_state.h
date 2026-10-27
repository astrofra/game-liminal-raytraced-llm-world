#pragma once
#include <map>
#include <string>
#include <vector>

namespace liminal {
struct ObjectInstance {
    std::string id, label, description;
    std::vector<std::string> aliases;
    std::string origin_place, source_quote, visual_id;
    int created_turn = 0;
    bool portable = false; // Unknown legacy possessions acquire no capabilities.
};
struct ObjectPlacement {
    std::string object_id;
    float x = 0, y = 0, z = 0;
};
struct ObjectSource {
    std::string place_id, quote, label, object_id, reason, visual_id;
    bool allowed = false;
    int remaining = 0; // One bounded extraction per literary source in this version.
    bool material_source = false; // Its parent dust/pile may remain after one sample.
};
struct ObjectState {
    int version = 1;
    int revision = 0;
    int next_id = 1;
    std::map<std::string, ObjectInstance> instances;
    std::map<std::string, std::vector<ObjectPlacement>> deposits;
    std::vector<ObjectSource> sources;
    // Attested noun spans tied to an explicit identity, even after relocation.
    struct Mention { std::string place_id, object_id, quote; };
    std::vector<Mention> mentions;
};
} // namespace liminal
