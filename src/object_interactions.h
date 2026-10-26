#pragma once
#include "game_state.h"
#include "scene.h"

namespace liminal {
struct HeadlessTurnConfig;
struct ObjectResolution {
    bool handled = false, changed = false, clarified = false;
    std::string action, object_id, reason, metrics;
};
ObjectResolution ResolveObjectAction(const SessionState& state, const std::string& command,
    const HeadlessTurnConfig& config, const Scene& scene, SessionState* candidate);
// Pure candidate preparation: neither input nor ownership changes on failure.
struct ObjectProposal {
    std::string action; // take, drop, none, clarify
    std::string origin; // explicit, implicit
    std::string object_id, quote, label, reason, visual_id;
    std::string verdict; // allow, forbid, uncertain
    bool durable = false;
    bool material_source = false;
    int revision = 0;
};
void EnsureObjectRegistry(SessionState* state);
bool ValidateObjectState(const SessionState& state, std::string* error);
std::string ObjectName(const SessionState& state, const std::string& id);
std::string ObjectVisualName(const std::string& id);
std::string NormalizeObjectText(const std::string& text);
bool MayRequestObjectTransfer(const std::string& command);
bool ObjectTargetGrounded(const std::string& command, const std::string& label);
std::vector<std::string> MatchLocalObjects(const SessionState& state, const std::string& target);
bool PrepareObjectTransfer(const SessionState& state, const ObjectProposal& proposal,
    const ObjectPlacement* placement, SessionState* candidate, std::string* reason);
std::string SerializeObjectState(const ObjectState& state);
bool ParseObjectState(const std::string& text, ObjectState* state, std::string* error);
std::string ObjectEvidence(const SessionState& state, const std::string& place);
void RetainObjectObservation(SessionState* state, const std::string& observation);
std::string ObjectContext(const SessionState& state, const std::string& focus = {});
bool ObjectNarrationAllowed(const SessionState& state, const std::string& text,
    const std::string& action, const std::string& id, bool* transfer_mentioned);
std::string ObjectPresenceText(const SessionState& state, bool include_sources = true);
std::string ObjectActionText(const SessionState& state, const std::string& action, const std::string& id);
std::string ObjectRefusalText(GameLanguage language, const std::string& reason);
bool FindObjectPlacement(const Scene& scene, const SessionState& state, ObjectPlacement* placement);
bool BuildObjectScene(const std::string& base, const SessionState& state,
    std::string* composed, Scene* scene, std::string* error);
} // namespace liminal
