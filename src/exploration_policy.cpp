#include "exploration_policy.h"
#include <algorithm>
#include <array>
#include <cstdint>

namespace liminal {
namespace {
uint32_t Mix(uint32_t value) {
    value ^= value >> 16; value *= 0x7feb352du;
    value ^= value >> 15; value *= 0x846ca68bu;
    return value ^ (value >> 16);
}
}

int DrawExplorationExitTarget(unsigned int world_seed, int room_index) {
    return 2 + static_cast<int>(Mix(world_seed ^ Mix(static_cast<uint32_t>(room_index))) & 1u);
}

std::map<std::string, bool> BalanceExplorationExits(
    const std::map<std::string, bool>& proposed,
    const std::map<std::string, bool>& fixed,
    const std::vector<std::string>& possible,
    const ExitBalancePolicy& policy) {
    if(policy.target_open_exits==0)return proposed;
    std::array<std::string,4> directions={"north","east","south","west"};
    std::map<std::string,bool> exits;
    std::vector<std::string> adjustable;
    int fixed_open=0,open=0;
    for(const auto& direction:directions) {
        const auto required=fixed.find(direction),proposal=proposed.find(direction);
        const bool available=std::find(possible.begin(),possible.end(),direction)!=possible.end();
        if(required!=fixed.end()) {
            exits[direction]=required->second;
            fixed_open+=required->second?1:0;
        } else {
            exits[direction]=available&&proposal!=proposed.end()&&proposal->second;
            if(available)adjustable.push_back(direction);
        }
        open+=exits[direction]?1:0;
    }
    if(policy.allow_dead_end&&open==1)return exits;
    const int target=std::clamp(policy.target_open_exits,fixed_open,fixed_open+static_cast<int>(adjustable.size()));
    // Stable per-room ordering, independent of container order and platform RNG.
    std::sort(adjustable.begin(),adjustable.end(),[&](const std::string& a,const std::string& b) {
        const auto rank=[&](const std::string& d) {
            return Mix(policy.selection_seed ^ (0x9e3779b9u * static_cast<uint32_t>(1+std::find(directions.begin(),directions.end(),d)-directions.begin())));
        };
        return rank(a)==rank(b)?a<b:rank(a)<rank(b);
    });
    for(const auto& direction:adjustable) {
        if(open<target&&!exits[direction]){exits[direction]=true;++open;}
        else if(open>target&&exits[direction]){exits[direction]=false;--open;}
    }
    return exits;
}
} // namespace liminal
