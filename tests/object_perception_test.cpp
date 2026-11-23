#include "object_interactions.h"
#include "turn_runner.h"
#include "nlohmann/json.hpp"
#include <stdexcept>
#include <string>
#include <vector>

using namespace liminal;

#define REQUIRE_PERCEPTION(c)                                                   \
    do {                                                                        \
        if (!(c)) {                                                             \
            throw std::runtime_error(std::string("Object perception line ") +   \
                std::to_string(__LINE__) + ": " + #c);                          \
        }                                                                       \
    } while (false)

namespace {

using json = nlohmann::json;

const char* kDalle = "Une dalle transparente se dresse devant vous.";
const char* kMap = "survey map slate";
const char* kChalk = "boundary chalk";

SessionState Fixture() {
    SessionState state;
    char error[512] = {};
    REQUIRE_PERCEPTION(InitializeSessionState(kLocationQuarryThreshold, &state,
        error, sizeof(error)));
    state.language = kGameLanguageFrench;
    EnsureObjectRegistry(&state);
    state.reference_descriptions[state.current_place_id] = kDalle;
    return state;
}

struct Snapshot {
    std::string objects, place;
    std::vector<std::string> inventory;
    int move_count;
    explicit Snapshot(const SessionState& state)
        : objects(SerializeObjectState(state.objects))
        , inventory(state.hard_state.inventory_items)
        , place(state.current_place_id)
        , move_count(state.hard_state.move_count) {}
};

void RequireUnchanged(const SessionState& state, const Snapshot& before) {
    REQUIRE_PERCEPTION(SerializeObjectState(state.objects) == before.objects);
    REQUIRE_PERCEPTION(state.hard_state.inventory_items == before.inventory);
    REQUIRE_PERCEPTION(state.current_place_id == before.place);
    REQUIRE_PERCEPTION(state.hard_state.move_count == before.move_count);
}

struct Harness {
    HeadlessTurnConfig config;
    int calls = 0, source = 0, revision_delta = 0;
    std::string action = "perceive", object_id, label, reason = "Vous observez la dalle transparente.";
    std::vector<std::string> clarification_ids;
    bool material_source = false, durable = false;

    Harness() {
        config.interaction_completion =
            [this](const LlmGenerationConfig&, const std::vector<LlmPromptMessage>& messages,
                LlmStreamCallback, void*, LlmGenerationResult* result) -> bool {
                ++calls;
                REQUIRE_PERCEPTION(messages.size() >= 2);
                REQUIRE_PERCEPTION(messages[1].role == "user");
                const json context = json::parse(messages[1].content);
                REQUIRE_PERCEPTION(!context.at("source_references").empty());
                REQUIRE_PERCEPTION(context.at("source_references").at(0).at("source").get<int>() == 1);
                const json proposal = {
                    {"reason", reason}, {"action", action}, {"object_id", object_id},
                    {"clarification_ids", clarification_ids}, {"source", source}, {"label", label},
                    {"visual_id", ""}, {"material_source", material_source}, {"durable", durable},
                    {"revision", context.at("revision").get<int>() + revision_delta},
                    {"investigation_id", ""},
                    {"investigation_revision", context.at("investigation").at("revision").get<int>()},
                    {"reply_topic", "general"},
                };
                REQUIRE_PERCEPTION(proposal.size() == 13);
                result->success = true;
                result->reached_eog = true;
                result->response_text = proposal.dump();
                return true;
            };
    }
};

void Run(const SessionState& source, const std::string& command, Harness& harness,
    SessionState* candidate, ObjectResolution* resolution) {
    Scene scene; *candidate = source;
    *resolution = ResolveObjectAction(source, command, harness.config, scene, candidate);
}

void RequireNarrated(const ObjectResolution& resolution, const SessionState& source,
    const Snapshot& before, const SessionState& candidate) {
    REQUIRE_PERCEPTION(!resolution.handled); REQUIRE_PERCEPTION(!resolution.changed);
    REQUIRE_PERCEPTION(resolution.action == "perceive"); REQUIRE_PERCEPTION(resolution.reason.empty());
    RequireUnchanged(source, before); RequireUnchanged(candidate, before);
    std::string why; REQUIRE_PERCEPTION(ValidateObjectState(candidate, &why));
}

void RequireRejected(const ObjectResolution& resolution, const SessionState& source,
    const Snapshot& before, const SessionState& candidate) {
    REQUIRE_PERCEPTION(resolution.handled); REQUIRE_PERCEPTION(!resolution.changed);
    REQUIRE_PERCEPTION(!resolution.reason.empty());
    RequireUnchanged(source, before); RequireUnchanged(candidate, before);
    std::string why; REQUIRE_PERCEPTION(ValidateObjectState(candidate, &why));
}

void TestSourceGroundedPerceptionNarrates() {
    SessionState source = Fixture(); const Snapshot before(source); Harness harness;
    harness.source = 1; harness.label = "dalle transparente";
    SessionState candidate; ObjectResolution resolution;
    Run(source, "Je regarde la dalle transparente.", harness, &candidate, &resolution);
    REQUIRE_PERCEPTION(harness.calls == 1);
    RequireNarrated(resolution, source, before, candidate);
    REQUIRE_PERCEPTION(resolution.object_id.empty());
}

void TestHandOnSlateRemainsPerception() {
    SessionState source = Fixture(); const Snapshot before(source); Harness harness;
    harness.source = 1; harness.label = "dalle transparente";
    harness.object_id = kMap; harness.clarification_ids = {kMap};
    SessionState candidate; ObjectResolution resolution;
    Run(source, "Je pose ma main sur la dalle transparente.", harness, &candidate, &resolution);
    REQUIRE_PERCEPTION(harness.calls == 1);
    RequireNarrated(resolution, source, before, candidate);
    REQUIRE_PERCEPTION(resolution.object_id.empty());
    REQUIRE_PERCEPTION(resolution.clarification_candidates.empty());
    REQUIRE_PERCEPTION(!resolution.clarified);
}

void TestNeutralPerceptionNarrates() {
    SessionState source = Fixture(); const Snapshot before(source); Harness harness;
    SessionState candidate; ObjectResolution resolution;
    Run(source, "Je regarde autour de moi.", harness, &candidate, &resolution);
    REQUIRE_PERCEPTION(harness.calls == 1);
    RequireNarrated(resolution, source, before, candidate);
}

void TestStaleRevisionIsRefused() {
    SessionState source = Fixture(); const Snapshot before(source); Harness harness;
    harness.revision_delta = 1;
    SessionState candidate; ObjectResolution resolution;
    Run(source, "Je regarde autour de moi.", harness, &candidate, &resolution);
    REQUIRE_PERCEPTION(harness.calls == 1);
    RequireRejected(resolution, source, before, candidate);
}

void TestInvalidReferencesAreRefused() {
    for (int variant = 0; variant < 3; ++variant) {
        SessionState source = Fixture(); const Snapshot before(source); Harness harness;
        if (variant == 0) { harness.source = 99; harness.label = "dalle transparente"; }
        if (variant == 1) harness.object_id = "object:inconnu";
        if (variant == 2) harness.clarification_ids = {"object:inconnu"};
        SessionState candidate; ObjectResolution resolution;
        Run(source, "Je regarde la dalle transparente.", harness, &candidate, &resolution);
        REQUIRE_PERCEPTION(harness.calls == 1);
        RequireRejected(resolution, source, before, candidate);
    }
}

void TestTwoCandidatesAreNotSilentPerception() {
    SessionState source = Fixture(); const Snapshot before(source); Harness harness;
    harness.clarification_ids = {kMap, kChalk};
    SessionState candidate; ObjectResolution resolution;
    Run(source, "Je regarde ce que je porte.", harness, &candidate, &resolution);
    REQUIRE_PERCEPTION(harness.calls == 1);
    RequireRejected(resolution, source, before, candidate);
}

void TestMaterialOrDurablePerceptionIsRefused() {
    for (int variant = 0; variant < 2; ++variant) {
        SessionState source = Fixture(); const Snapshot before(source); Harness harness;
        harness.material_source = variant == 0; harness.durable = variant == 1;
        SessionState candidate; ObjectResolution resolution;
        Run(source, "Je regarde la dalle transparente.", harness, &candidate, &resolution);
        REQUIRE_PERCEPTION(harness.calls == 1);
        RequireRejected(resolution, source, before, candidate);
    }
}

void TestGenuineClarificationKeepsCandidates() {
    SessionState source = Fixture(); const Snapshot before(source); Harness harness;
    harness.action = "clarify"; harness.clarification_ids = {kMap, kChalk};
    SessionState candidate; ObjectResolution resolution;
    Run(source, "Je regarde l'un de mes objets.", harness, &candidate, &resolution);
    REQUIRE_PERCEPTION(harness.calls == 1); REQUIRE_PERCEPTION(resolution.handled);
    REQUIRE_PERCEPTION(!resolution.changed); REQUIRE_PERCEPTION(resolution.reason == "clarify");
    REQUIRE_PERCEPTION(resolution.clarified);
    REQUIRE_PERCEPTION(resolution.clarification_candidates.size() == 2);
    REQUIRE_PERCEPTION(resolution.clarification_candidates[0] == kMap);
    REQUIRE_PERCEPTION(resolution.clarification_candidates[1] == kChalk);
    RequireUnchanged(source, before); RequireUnchanged(candidate, before);
    std::string why; REQUIRE_PERCEPTION(ValidateObjectState(candidate, &why));
}

void TestIncompleteDropIsNotObservation() {
    SessionState source = Fixture(); const Snapshot before(source); Harness harness;
    harness.action = "drop";
    SessionState candidate; ObjectResolution resolution;
    Run(source, "Je pose l'objet.", harness, &candidate, &resolution);
    REQUIRE_PERCEPTION(harness.calls == 1);
    RequireRejected(resolution, source, before, candidate);
    REQUIRE_PERCEPTION(resolution.action != "perceive");
}

// Codex: exercise the entire turn, not only the resolver's dispatch flag.
void TestGestureReachesNarrator() {
    SessionState state = Fixture(); const Snapshot before(state); Harness harness;
    harness.source = 1; harness.label = "dalle transparente";
    harness.object_id = kMap; harness.clarification_ids = {kMap};
    harness.config.local_scenes = true;
    harness.config.generation_config.n_ctx = 16384;
    harness.config.generation_config.prompt_token_counter = [](const auto& messages, int* count, std::string*) {
        size_t bytes = 0; for(const auto& message : messages) bytes += message.content.size();
        *count = static_cast<int>(bytes / 4); return true;
    };
    int narrations = 0;
    harness.config.dynamic_scene_config.completion = [](const auto&, const auto&, auto, void*, auto*) -> bool {
        throw std::runtime_error("A sensory gesture must not compose a new place");
    };
    const std::string prose = "Sous votre gant, la surface de la dalle oppose une résistance lisse. Votre souffle résonne derrière la visière.";
    harness.config.narrative_completion = [&](const auto&, const auto& messages, auto, void*, auto* result) {
        ++narrations;
        const auto request = json::parse(messages.back().content);
        result->response_text = json{{"title", "Seuil de la carrière"},
            {"summary", "Une dalle transparente se dresse devant les parois de la carrière."}, {"narration", prose},
            {"thermal", {{"external_temperature_c", request["temperature_external_c"]},
                {"body_temperature_c", request["temperature_body_c"]}, {"reason", "Protection stable."}}},
            {"exits", request["exit_proposal"]["fixed"]}}.dump();
        result->success = result->reached_eog = true;
        return true;
    };
    HeadlessTurnResult turn; char error[512] = {};
    const char* command = "Je pose ma main sur la dalle transparente.";
    REQUIRE_PERCEPTION(RunHeadlessTurnFromState(state, command, harness.config, &turn, error, sizeof(error)));
    if(!turn.accepted || turn.used_turn_fallback)
        throw std::runtime_error("Perception narration rejected: " + turn.narrative_metrics_json + " / " + turn.interaction_metrics_json);
    REQUIRE_PERCEPTION(turn.turn_result.intent == "perceive_accepted_scene");
    REQUIRE_PERCEPTION(turn.turn_result.narration == prose && narrations == 1);
    REQUIRE_PERCEPTION(turn.scene_attempts.empty());
    RequireUnchanged(state, before);
    UpdateSessionStateFromTurn(command, turn, &state);
    RequireUnchanged(state, before);
    REQUIRE_PERCEPTION(state.investigation.evidence.empty());
    REQUIRE_PERCEPTION(state.hard_state.body_temperature_c == 37.f);
}

}  // namespace

void RunObjectPerceptionTests() {
    TestSourceGroundedPerceptionNarrates(); TestHandOnSlateRemainsPerception();
    TestNeutralPerceptionNarrates(); TestStaleRevisionIsRefused();
    TestInvalidReferencesAreRefused(); TestTwoCandidatesAreNotSilentPerception();
    TestMaterialOrDurablePerceptionIsRefused(); TestGenuineClarificationKeepsCandidates();
    TestIncompleteDropIsNotObservation();
    TestGestureReachesNarrator();
}
