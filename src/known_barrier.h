#pragma once
#include "game_state.h"
#include "scene.h"

namespace liminal {
enum class BarrierViewSide { unknown, front, right, back, left };
bool HasKnownBarrier(const SessionState& state, CardinalDirection direction);
// The accepted room's original orientation is stable across revisits.
BarrierViewSide KnownBarrierSide(const SessionState& state, CardinalDirection direction);
// A visual layer only: no change to the accepted architecture or world state.
bool BuildKnownBarrierScene(const std::string& base, const SessionState& state,
    std::string* composed, Scene* scene, std::string* error);
}
