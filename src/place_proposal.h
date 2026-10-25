#pragma once

#include <map>
#include <string>
#include <vector>

struct PlaceProposal {
    std::string title;
    std::string summary;
    std::string narration;
    std::string thermal_reason;
    int external_temperature_c = 464;
    float body_temperature_c = 37.0f;
    std::map<std::string, bool> exits;
};

bool ParsePlaceProposal(const std::string& text, int current_external, float current_body,
    const std::map<std::string, bool>& fixed_exits,
    const std::vector<std::string>& possible_exits,
    PlaceProposal* output, std::string* error);
