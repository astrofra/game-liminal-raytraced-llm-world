#pragma once

#include <map>
#include <string>
#include <vector>

namespace liminal {

struct ExitBalancePolicy {
    int target_open_exits = 0; // 0: legacy policy; otherwise 2 or 3, including return.
    bool allow_dead_end = false;
    unsigned int selection_seed = 0;
};

int DrawExplorationExitTarget(unsigned int world_seed, int room_index);

// Inputs are validated cardinal maps. Preserve fixed routes and geometric limits.
// Keep a proposed one-exit dead end only when allowed; otherwise meet the target
// as closely as those constraints permit. Prefer the model's existing openings.
std::map<std::string, bool> BalanceExplorationExits(
    const std::map<std::string, bool>& proposed,
    const std::map<std::string, bool>& fixed,
    const std::vector<std::string>& possible,
    const ExitBalancePolicy& policy);

} // namespace liminal
