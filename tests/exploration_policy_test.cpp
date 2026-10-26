#include "exploration_policy.h"

#include <algorithm>
#include <initializer_list>
#include <iostream>
#include <map>
#include <string>
#include <vector>

namespace {

using ExitMap = std::map<std::string, bool>;

int g_failures = 0;

void Check(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++g_failures;
    }
}

ExitMap ProposedExits(bool north, bool east, bool south, bool west) {
    return {{"north", north}, {"east", east}, {"south", south}, {"west", west}};
}

std::vector<std::string> AllDirections() {
    return {"north", "east", "south", "west"};
}

bool IsOpen(const ExitMap& exits, const std::string& direction) {
    const auto it = exits.find(direction);
    return it != exits.end() && it->second;
}

int CountOpen(const ExitMap& exits) {
    int count = 0;
    for (const auto& entry : exits) {
        if (entry.second) {
            ++count;
        }
    }
    return count;
}

void CheckFixed(const ExitMap& result, const ExitMap& fixed, const std::string& label) {
    for (const auto& entry : fixed) {
        if (entry.second) {
            Check(IsOpen(result, entry.first), label + ": fixed-open exit was closed: " + entry.first);
        } else {
            Check(!IsOpen(result, entry.first), label + ": fixed-closed exit was opened: " + entry.first);
        }
    }
}

void CheckOnlyPossible(const ExitMap& result, const std::vector<std::string>& possible,
                       const std::string& label) {
    for (const auto& entry : result) {
        if (entry.second &&
            std::find(possible.begin(), possible.end(), entry.first) == possible.end()) {
            Check(false, label + ": opened a non-possible exit: " + entry.first);
        }
    }
}

void TestDeterministicTargets() {
    const unsigned int seeds[] = {0u, 1u, 42u, 1234u};
    for (const unsigned int seed : seeds) {
        int count2 = 0;
        int count3 = 0;
        int invalid = 0;
        int nondeterministic = 0;
        for (int room = 0; room < 10000; ++room) {
            const int target = liminal::DrawExplorationExitTarget(seed, room);
            if (target == 2) {
                ++count2;
            } else if (target == 3) {
                ++count3;
            } else {
                ++invalid;
            }
            if (target != liminal::DrawExplorationExitTarget(seed, room)) {
                ++nondeterministic;
            }
        }
        const std::string label = "seed " + std::to_string(seed);
        Check(invalid == 0, label + ": target must be 2 or 3");
        Check(nondeterministic == 0, label + ": target must be deterministic");
        Check(count2 >= 4500 && count2 <= 5500, label + ": target 2 share outside 45..55%");
        Check(count3 >= 4500 && count3 <= 5500, label + ": target 3 share outside 45..55%");
    }
}

void TestZeroTargetPreservesProposed() {
    liminal::ExitBalancePolicy policy;
    policy.target_open_exits = 0;
    const ExitMap proposed = ProposedExits(true, false, true, false);
    const ExitMap result = liminal::BalanceExplorationExits(
        proposed, ExitMap{}, AllDirections(), policy);
    Check(result == proposed, "target 0 must preserve proposed exactly");
}

void TestOpensOneExitWhenBelowTarget() {
    const ExitMap fixed{{"north", true}};
    const std::vector<std::string> possible = AllDirections();
    liminal::ExitBalancePolicy policy;
    policy.target_open_exits = 3;
    const ExitMap result = liminal::BalanceExplorationExits(
        ProposedExits(true, true, false, false), fixed, possible, policy);

    Check(CountOpen(result) == 3, "target 3 must open exactly one additional exit");
    Check(IsOpen(result, "north"), "north must remain open");
    Check(IsOpen(result, "east"), "east must remain open");
    const int extra = (IsOpen(result, "south") ? 1 : 0) + (IsOpen(result, "west") ? 1 : 0);
    Check(extra == 1, "exactly one of south/west must be opened");
    CheckFixed(result, fixed, "one additional exit");
    CheckOnlyPossible(result, possible, "one additional exit");
}

void TestTargetAndFixedForPartialProposals() {
    const ExitMap fixed{{"north", true}};
    const std::vector<std::string> possible = AllDirections();
    liminal::ExitBalancePolicy policy;
    policy.target_open_exits = 2;

    ExitMap result = liminal::BalanceExplorationExits(
        ProposedExits(true, true, true, false), fixed, possible, policy);
    Check(CountOpen(result) == 2, "target 2 must keep exactly two exits from N/E/S");
    Check(IsOpen(result, "north"), "north must remain open in the N/E/S proposal");
    const int retained = (IsOpen(result, "east") ? 1 : 0) + (IsOpen(result, "south") ? 1 : 0);
    Check(retained == 1, "exactly one of east/south must remain open");
    Check(!IsOpen(result, "west"), "west must not open for the N/E/S proposal");
    CheckOnlyPossible(result, possible, "N/E/S proposal");

    for (const int target : {2, 3}) {
        policy.target_open_exits = target;
        result = liminal::BalanceExplorationExits(
            ProposedExits(true, true, true, true), fixed, possible, policy);
        Check(CountOpen(result) == target, "full proposal must be reduced to the target");
        Check(IsOpen(result, "north"), "north must remain open for the full proposal");
        CheckOnlyPossible(result, possible, "full proposal");
    }
}

void TestDeadEndHandling() {
    const ExitMap fixed{{"north", true}};
    const std::vector<std::string> possible = AllDirections();
    const ExitMap proposed = ProposedExits(true, false, false, false);

    for (const int target : {2, 3}) {
        liminal::ExitBalancePolicy policy;
        policy.target_open_exits = target;
        policy.allow_dead_end = true;
        ExitMap result = liminal::BalanceExplorationExits(proposed, fixed, possible, policy);
        Check(CountOpen(result) == 1, "allow_dead_end=true must retain the single exit");
        Check(IsOpen(result, "north"), "allow_dead_end=true must retain north");

        policy.allow_dead_end = false;
        result = liminal::BalanceExplorationExits(proposed, fixed, possible, policy);
        Check(CountOpen(result) == target, "allow_dead_end=false must open up to the target");
        Check(IsOpen(result, "north"), "north must remain open when opening exits");
    }
}

void TestPartialFixedClosuresAndOpenFixed() {
    const std::vector<std::string> possible = AllDirections();
    liminal::ExitBalancePolicy policy;
    policy.target_open_exits = 3;

    const ExitMap fixedWithClosure{{"north", true}, {"west", false}};
    ExitMap result = liminal::BalanceExplorationExits(
        ProposedExits(true, false, false, true), fixedWithClosure, possible, policy);
    Check(CountOpen(result) == 3, "fixed north/open and west/closed must yield three exits");
    Check(IsOpen(result, "north"), "fixed north must remain open");
    Check(IsOpen(result, "east"), "east must open to reach target 3");
    Check(IsOpen(result, "south"), "south must open to reach target 3");
    Check(!IsOpen(result, "west"), "fixed-closed west must remain closed");
    CheckFixed(result, fixedWithClosure, "partial fixed map");

    const ExitMap fixedOpen{{"north", true}, {"east", true}, {"south", true}};
    policy.target_open_exits = 2;
    result = liminal::BalanceExplorationExits(
        ProposedExits(false, false, false, false), fixedOpen, possible, policy);
    Check(CountOpen(result) == 3, "three fixed-open exits must all be preserved");
    Check(IsOpen(result, "north") && IsOpen(result, "east") && IsOpen(result, "south"),
          "all fixed-open exits must be preserved");
    CheckFixed(result, fixedOpen, "three fixed-open exits");
}

void TestFeasibilityAndPossibleSet() {
    const ExitMap fixed{{"north", true}};
    liminal::ExitBalancePolicy policy;
    policy.target_open_exits = 3;

    const std::vector<std::string> northEast{"north", "east"};
    ExitMap result = liminal::BalanceExplorationExits(
        ProposedExits(true, false, false, false), fixed, northEast, policy);
    Check(CountOpen(result) == 2, "target 3 with possible N/E must yield only two exits");
    Check(IsOpen(result, "north") && IsOpen(result, "east"), "possible N/E must both be open");
    CheckOnlyPossible(result, northEast, "possible N/E");

    const std::vector<std::string> northOnly{"north"};
    result = liminal::BalanceExplorationExits(
        ProposedExits(true, false, false, false), fixed, northOnly, policy);
    Check(CountOpen(result) == 1, "target 3 with possible N only must yield one exit");
    Check(IsOpen(result, "north"), "possible N only must keep north open");
    CheckOnlyPossible(result, northOnly, "possible N only");
}

void TestOrderIndependenceAndStability() {
    const ExitMap fixed{{"north", true}, {"west", true}};
    const std::vector<std::string> firstOrder = AllDirections();
    const std::vector<std::string> secondOrder{"west", "south", "east", "north"};
    liminal::ExitBalancePolicy policy;
    policy.target_open_exits = 3;

    const ExitMap first = liminal::BalanceExplorationExits(
        ProposedExits(false, false, false, false), fixed, firstOrder, policy);
    Check(CountOpen(first) == 3, "fixed N/W with target 3 must open one third exit");
    Check(IsOpen(first, "north") && IsOpen(first, "west"), "fixed N/W must remain open");
    const int third = (IsOpen(first, "east") ? 1 : 0) + (IsOpen(first, "south") ? 1 : 0);
    Check(third == 1, "exactly one of east/south must open");
    CheckFixed(first, fixed, "order test");

    const ExitMap second = liminal::BalanceExplorationExits(
        ProposedExits(false, false, false, false), fixed, secondOrder, policy);
    Check(first == second, "result must not depend on the order of possible exits");

    const ExitMap rebalanced = liminal::BalanceExplorationExits(first, fixed, firstOrder, policy);
    Check(rebalanced == first, "rebalancing with the same constraints must be stable");
}

}  // namespace

int main() {
    TestDeterministicTargets();
    TestZeroTargetPreservesProposed();
    TestOpensOneExitWhenBelowTarget();
    TestTargetAndFixedForPartialProposals();
    TestDeadEndHandling();
    TestPartialFixedClosuresAndOpenFixed();
    TestFeasibilityAndPossibleSet();
    TestOrderIndependenceAndStability();

    if (g_failures != 0) {
        std::cerr << g_failures << " assertion(s) failed\n";
        return 1;
    }
    std::cout << "All exploration policy tests passed\n";
    return 0;
}
