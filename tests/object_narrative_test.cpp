#include "player_action_fixture.h"
#include "object_interactions.h"
#include "turn_runner.h"
#include "nlohmann/json.hpp"
#include <algorithm>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <vector>

using namespace liminal;

#define REQUIRE_NARRATIVE(c)                                                    \
    do {                                                                        \
        if (!(c)) {                                                             \
            throw std::runtime_error(std::string("Object narrative line ") +    \
                std::to_string(__LINE__) + ": " + #c);                         \
        }                                                                       \
    } while (false)

namespace {

using json = nlohmann::json;

const char* kPlace = "generated:room_000001";
const char* kChalk = "boundary chalk";
const char* kDropProse =
    "Vous déposez la craie de délimitation sur le sol ocre.\n"
    "La lumière souligne sa surface claire.";
const char* kTakeProse =
    "Vous avez pris la craie de délimitation.\n"
    "Sa surface claire accroche la lumière.";
const char* kLookProse =
    "La lumière découpe les arêtes du passage.\n"
    "Votre souffle résonne derrière la visière.";

const char* SceneText() {
    return R"(room "Narration fixture"
camera eye(0,1.6,-6) target(0,0.6,4) up(0,1,0) fov(50)
sky zenith(0.1) horizon(0.4) nadir(0.05) band(0.35) curve(1.6) noise(0)
distant_light direction(-0.6,-1,0.35) intensity(3.5) color(1,0.88,0.55) angular_radius(0.65)
box "floor" pos(0,-0.1,0) size(20,0.2,20) gray(0.6)
box "wall" pos(-4,2,1) size(1,4,8) gray(0.6)
)";
}

SessionState Fixture() {
    SessionState state;
    char error[512] = {};
    REQUIRE_NARRATIVE(InitializeSessionState(kLocationQuarryThreshold, &state,
        error, sizeof(error)));
    state.language = kGameLanguageFrench;
    state.current_place_id = kPlace;
    state.origin_place_id = kPlace;
    state.hard_state.current_location_id = kLocationUnknown;
    state.spatial_state = SpatialState();
    state.spatial_state.room_title = "Le seuil ocre";
    state.spatial_state.room_summary =
        "Un sol ocre soutient ce passage. Les murs se perdent dans la lumière.";
    state.spatial_state.blocked_exits = {"east", "west"};
    state.generated_rooms.clear();
    state.room_links.clear();
    state.invisible_barriers.clear();
    GeneratedRoom room;
    room.room_id = kPlace;
    room.spatial_state = state.spatial_state;
    room.scene_text = SceneText();
    room.scene_contract_version = 0;
    room.arrival_narration = room.spatial_state.room_summary;
    state.generated_rooms.push_back(room);
    state.reference_descriptions[kPlace] = room.arrival_narration;
    return state;
}

bool Has(const SessionState& state, const std::string& id) {
    return std::find(state.hard_state.inventory_items.begin(),
                 state.hard_state.inventory_items.end(),
                 id) != state.hard_state.inventory_items.end();
}

std::size_t DepositCount(const SessionState& state, const std::string& place) {
    const auto found = state.objects.deposits.find(place);
    return found == state.objects.deposits.end() ? 0 : found->second.size();
}

std::string CollapseWhitespace(const std::string& text) {
    std::string collapsed;
    bool pending_space = false;
    for (char character : text) {
        if (character == ' ' || character == '\n' || character == '\t' ||
            character == '\r') {
            pending_space = true;
            continue;
        }
        if (pending_space && !collapsed.empty()) {
            collapsed += ' ';
        }
        pending_space = false;
        collapsed += character;
    }
    return collapsed;
}

bool ContainsProse(const std::string& narration, const std::string& prose) {
    return CollapseWhitespace(narration).find(CollapseWhitespace(prose)) !=
        std::string::npos;
}

bool StateMentions(const SessionState& state, const std::string& needle) {
    for (const auto& item : state.hard_state.inventory_items) {
        if (item.find(needle) != std::string::npos) {
            return true;
        }
    }
    for (const auto& entry : state.objects.instances) {
        if (entry.first.find(needle) != std::string::npos ||
            entry.second.label.find(needle) != std::string::npos) {
            return true;
        }
    }
    return false;
}

struct Harness {
    HeadlessTurnConfig config;
    int interpretations = 0;
    int narrations = 0;
    int compositions = 0;
    std::string prose;
    std::vector<std::string> extra_deposited_ids;
    std::vector<std::string> last_deposited_ids;

    Harness() {
        config.local_scenes = true;config.investigation_opportunities=false;
        config.generation_config.prompt_token_counter =
            [](const std::vector<LlmPromptMessage>& messages, int* count,
                std::string*) {
                std::size_t characters = 0;
                for (const auto& message : messages) {
                    characters += message.content.size();
                }
                *count = static_cast<int>(characters / 4 + 1);
                return true;
            };
        config.dynamic_scene_config.completion =
            [this](const LlmGenerationConfig&, const std::vector<LlmPromptMessage>&,
                LlmStreamCallback, void*, LlmGenerationResult*) -> bool {
            ++compositions;
            return false;
        };
        config.interaction_completion =
            [this](const LlmGenerationConfig& interpretation_config,
                const std::vector<LlmPromptMessage>& messages, LlmStreamCallback,
                void*, LlmGenerationResult* result) {
                ++interpretations;
                REQUIRE_NARRATIVE(interpretation_config.purpose ==
                    "player_interpretation_v2");
                REQUIRE_NARRATIVE(messages.size() > 1);
                const json context = json::parse(messages[1].content);
                json proposal = FixturePlayerAction(context);
                result->success = true;
                result->reached_eog = true;
                result->response_text = FixtureWireAction(proposal,context).dump();
                return true;
            };
        config.narrative_completion =
            [this](const LlmGenerationConfig& narrative_config,
                const std::vector<LlmPromptMessage>& messages, LlmStreamCallback,
                void*, LlmGenerationResult* result) {
                ++narrations;
                REQUIRE_NARRATIVE(messages.size() > 1);
                const json request = json::parse(messages[1].content);
                REQUIRE_NARRATIVE(request.contains("objects"));
                REQUIRE_NARRATIVE(request["objects"].contains("deposited"));
                json deposited_ids = json::array();
                last_deposited_ids.clear();
                for (const auto& item : request["objects"]["deposited"]) {
                    const std::string id = item.at("id").get<std::string>();
                    deposited_ids.push_back(id);
                    last_deposited_ids.push_back(id);
                }
                for (const auto& id : extra_deposited_ids) {
                    deposited_ids.push_back(id);
                }
                REQUIRE_NARRATIVE(request.contains("exit_proposal"));
                REQUIRE_NARRATIVE(request["exit_proposal"].contains("fixed"));
                json response = {
                    {"title", "Le seuil ocre"},
                    {"summary", "Les masses claires bordent ce passage ouvert."},
                    {"narration", prose},
                    {"thermal",
                        {{"external_temperature_c", 464},
                            {"body_temperature_c",
                                request.value("temperature_body_c", 37.0f) + 0.1f},
                            {"reason", "Une brève exposition."}}},
                    {"exits", request["exit_proposal"]["fixed"]},
                };
                if (!narrative_config.json_schema.empty()) {
                    const json schema = json::parse(narrative_config.json_schema);
                    if (schema.contains("properties") &&
                        schema["properties"].contains("deposited_ids")) {
                        response["deposited_ids"] = deposited_ids;
                    }
                }
                result->success = true;
                result->reached_eog = true;
                result->response_text = response.dump();
                return true;
            };
    }

    HeadlessTurnResult Run(SessionState& state, const char* command,
        bool publish = true) {
        HeadlessTurnResult turn;
        char error[512] = {};
        if (!RunHeadlessTurnFromState(state, command, config, &turn, error,
                sizeof(error))) {
            throw std::runtime_error(std::string("Object narrative turn ") +
                command + ": " + error);
        }
        if (publish) {
            UpdateSessionStateFromTurn(command, turn, &state);
        }
        return turn;
    }
};

void DropChalk(SessionState& state, Harness& harness) {
    harness.prose = kDropProse;
    harness.Run(state, "poser craie");
    REQUIRE_NARRATIVE(!Has(state, kChalk));
    REQUIRE_NARRATIVE(DepositCount(state, kPlace) == 1);
}

// 1. A supported explicit drop keeps the model prose and changes only ownership.
void TestExplicitDropConservesProse() {
    SessionState state = Fixture();
    Harness harness;
    harness.prose = kDropProse;
    const HeadlessTurnResult turn = harness.Run(state, "poser craie");
    REQUIRE_NARRATIVE(ContainsProse(turn.turn_result.narration, kDropProse));
    REQUIRE_NARRATIVE(!turn.used_turn_fallback);
    REQUIRE_NARRATIVE(!Has(state, kChalk));
    REQUIRE_NARRATIVE(state.hard_state.inventory_items.size() == 2);
    REQUIRE_NARRATIVE(DepositCount(state, kPlace) == 1);
    REQUIRE_NARRATIVE(state.objects.deposits.at(kPlace)[0].object_id == kChalk);
    REQUIRE_NARRATIVE(harness.interpretations == 1);
    REQUIRE_NARRATIVE(harness.compositions == 0);
    REQUIRE_NARRATIVE(harness.narrations == 1);
    std::string why;
    REQUIRE_NARRATIVE(ValidateObjectState(state, &why));
}

// 2. Taking the deposited chalk back preserves its identity and prose.
void TestExplicitTakeConservesProseAndIdentity() {
    SessionState state = Fixture();
    Harness harness;
    DropChalk(state, harness);
    const std::string deposited_id = state.objects.deposits.at(kPlace)[0].object_id;
    harness.prose = kTakeProse;
    const HeadlessTurnResult turn = harness.Run(state, "prendre craie");
    REQUIRE_NARRATIVE(ContainsProse(turn.turn_result.narration, kTakeProse));
    REQUIRE_NARRATIVE(!turn.used_turn_fallback);
    REQUIRE_NARRATIVE(Has(state, deposited_id));
    REQUIRE_NARRATIVE(state.hard_state.inventory_items.size() == 3);
    REQUIRE_NARRATIVE(DepositCount(state, kPlace) == 0);
    REQUIRE_NARRATIVE(state.objects.instances.count(deposited_id) == 1);
    REQUIRE_NARRATIVE(harness.narrations == 2);
    std::string why;
    REQUIRE_NARRATIVE(ValidateObjectState(state, &why));
}

// 3. Observation accepts correct coverage and completes the missing presence.
void TestObservationCompletesDepositedPresence() {
    SessionState state = Fixture();
    Harness harness;
    DropChalk(state, harness);
    const int interpretations_before = harness.interpretations;
    const int compositions_before = harness.compositions;
    const int narrations_before = harness.narrations;
    const std::vector<std::string> inventory_before =
        state.hard_state.inventory_items;
    const std::size_t instances_before = state.objects.instances.size();
    harness.prose = kLookProse;
    const HeadlessTurnResult turn = harness.Run(state, "regarder");
    REQUIRE_NARRATIVE(ContainsProse(turn.turn_result.narration, kLookProse));
    REQUIRE_NARRATIVE(!turn.used_turn_fallback);
    REQUIRE_NARRATIVE(std::find(harness.last_deposited_ids.begin(),
                harness.last_deposited_ids.end(),
                kChalk) != harness.last_deposited_ids.end());
    const std::string narration = CollapseWhitespace(turn.turn_result.narration);
    const std::size_t prose_end = narration.find("derrière la visière.");
    const std::size_t presence = narration.find("craie");
    REQUIRE_NARRATIVE(prose_end != std::string::npos);
    REQUIRE_NARRATIVE(presence != std::string::npos);
    REQUIRE_NARRATIVE(presence > prose_end);
    REQUIRE_NARRATIVE(harness.interpretations == interpretations_before + 1);
    REQUIRE_NARRATIVE(harness.compositions == compositions_before);
    REQUIRE_NARRATIVE(harness.narrations == narrations_before + 1);
    REQUIRE_NARRATIVE(state.hard_state.inventory_items == inventory_before);
    REQUIRE_NARRATIVE(state.objects.instances.size() == instances_before);
    REQUIRE_NARRATIVE(!Has(state, kChalk));
    REQUIRE_NARRATIVE(DepositCount(state, kPlace) == 1);
}

// 4. An invented acquisition during the drop never passes and creates nothing.
void TestInventedAcquisitionDuringDropIsRejected() {
    SessionState state = Fixture();
    Harness harness;
    const std::size_t instances_before = state.objects.instances.size();
    harness.prose =
        "Vous ramassez une clé dorée et la rangez dans votre équipement.";
    const HeadlessTurnResult turn = harness.Run(state, "poser craie");
    REQUIRE_NARRATIVE(turn.turn_result.narration.find("clé dorée") ==
        std::string::npos);
    REQUIRE_NARRATIVE(turn.turn_result.narration.find(
                "rangez dans votre équipement") == std::string::npos);
    REQUIRE_NARRATIVE(turn.used_turn_fallback);
    REQUIRE_NARRATIVE(state.objects.instances.size() == instances_before);
    REQUIRE_NARRATIVE(!StateMentions(state, "clé"));
    REQUIRE_NARRATIVE(!StateMentions(state, "dorée"));
    REQUIRE_NARRATIVE(!Has(state, kChalk));
    REQUIRE_NARRATIVE(state.hard_state.inventory_items.size() == 2);
    REQUIRE_NARRATIVE(DepositCount(state, kPlace) == 1);
    REQUIRE_NARRATIVE(state.objects.deposits.at(kPlace)[0].object_id == kChalk);
    std::string why;
    REQUIRE_NARRATIVE(ValidateObjectState(state, &why));
}

// 5. An invented acquisition during observation is rejected; deposit is intact.
void TestInventedAcquisitionDuringObservationIsRejected() {
    SessionState state = Fixture();
    Harness harness;
    DropChalk(state, harness);
    const std::vector<std::string> inventory_before =
        state.hard_state.inventory_items;
    const std::size_t instances_before = state.objects.instances.size();
    harness.prose = "Vous ramassez la craie de délimitation.";
    const HeadlessTurnResult turn = harness.Run(state, "regarder");
    REQUIRE_NARRATIVE(turn.turn_result.narration.find(
                "Vous ramassez la craie de délimitation.") == std::string::npos);
    REQUIRE_NARRATIVE(turn.used_turn_fallback);
    REQUIRE_NARRATIVE(state.hard_state.inventory_items == inventory_before);
    REQUIRE_NARRATIVE(state.objects.instances.size() == instances_before);
    REQUIRE_NARRATIVE(!Has(state, kChalk));
    REQUIRE_NARRATIVE(state.hard_state.inventory_items.size() == 2);
    REQUIRE_NARRATIVE(DepositCount(state, kPlace) == 1);
    std::string why;
    REQUIRE_NARRATIVE(ValidateObjectState(state, &why));
}

// 6. Coverage naming an absent object is contradictory and changes nothing.
void TestContradictoryDepositedCoverageIsRejected() {
    SessionState state = Fixture();
    Harness harness;
    DropChalk(state, harness);
    const std::vector<std::string> inventory_before =
        state.hard_state.inventory_items;
    const std::size_t instances_before = state.objects.instances.size();
    harness.extra_deposited_ids = {"invented-relic"};
    harness.prose = kLookProse;
    const HeadlessTurnResult turn = harness.Run(state, "regarder");
    REQUIRE_NARRATIVE(turn.turn_result.narration.find("invented-relic") ==
        std::string::npos);
    REQUIRE_NARRATIVE(turn.used_turn_fallback);
    REQUIRE_NARRATIVE(state.hard_state.inventory_items == inventory_before);
    REQUIRE_NARRATIVE(state.objects.instances.size() == instances_before);
    REQUIRE_NARRATIVE(state.objects.instances.count("invented-relic") == 0);
    REQUIRE_NARRATIVE(DepositCount(state, kPlace) == 1);
    REQUIRE_NARRATIVE(state.objects.deposits.at(kPlace)[0].object_id == kChalk);
    std::string why;
    REQUIRE_NARRATIVE(ValidateObjectState(state, &why));
}

}  // namespace

void RunObjectNarrativeTests() {
    TestExplicitDropConservesProse();
    TestExplicitTakeConservesProseAndIdentity();
    TestObservationCompletesDepositedPresence();
    TestInventedAcquisitionDuringDropIsRejected();
    TestInventedAcquisitionDuringObservationIsRejected();
    TestContradictoryDepositedCoverageIsRejected();
}
