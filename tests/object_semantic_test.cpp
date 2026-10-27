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

#define REQUIRE_SEMANTIC(c)                                                     \
    do {                                                                        \
        if (!(c)) {                                                             \
            throw std::runtime_error(std::string("Object semantic line ") +     \
                std::to_string(__LINE__) + ": " + #c);                          \
        }                                                                       \
    } while (false)

namespace {

using json = nlohmann::json;

const char* kChalk = "boundary chalk";
const char* kElsewhere = "generated:room_000002";

SessionState Fixture() {
    SessionState state;
    char error[512] = {};
    REQUIRE_SEMANTIC(InitializeSessionState(kLocationQuarryThreshold, &state,
        error, sizeof(error)));
    state.language = kGameLanguageFrench;
    EnsureObjectRegistry(&state);
    state.reference_descriptions[state.current_place_id] = "Le sol ocre reste nu.";
    return state;
}

void DepositChalkHere(SessionState& state) {
    auto& items = state.hard_state.inventory_items;
    const auto found = std::find(items.begin(), items.end(), std::string(kChalk));
    REQUIRE_SEMANTIC(found != items.end());
    items.erase(found);
    ObjectPlacement placement;
    placement.object_id = kChalk;
    state.objects.deposits[state.current_place_id].push_back(placement);
    std::string why;
    REQUIRE_SEMANTIC(ValidateObjectState(state, &why));
}

struct Snapshot {
    std::string objects;
    std::vector<std::string> inventory;

    explicit Snapshot(const SessionState& state)
        : objects(SerializeObjectState(state.objects))
        , inventory(state.hard_state.inventory_items)
    {
    }
};

void RequireUnchanged(const SessionState& state, const Snapshot& before) {
    REQUIRE_SEMANTIC(SerializeObjectState(state.objects) == before.objects);
    REQUIRE_SEMANTIC(state.hard_state.inventory_items == before.inventory);
}

int CountId(const std::vector<std::string>& values, const std::string& id) {
    return static_cast<int>(std::count(values.begin(), values.end(), id));
}

std::size_t DepositCount(const SessionState& state, const std::string& place) {
    const auto found = state.objects.deposits.find(place);
    return found == state.objects.deposits.end() ? 0 : found->second.size();
}

struct Harness {
    HeadlessTurnConfig config;
    int calls = 0;
    std::string expected_command;
    std::string action, origin, object_id, quote, label, visual_id, verdict, reason;
    bool durable = false;
    bool material_source = false;

    Harness() {
        config.interaction_completion =
            [this](const LlmGenerationConfig&, const std::vector<LlmPromptMessage>& messages,
                LlmStreamCallback, void*, LlmGenerationResult* result) -> bool {
                ++calls;
                REQUIRE_SEMANTIC(messages.size() >= 2);
                REQUIRE_SEMANTIC(messages[1].role == "user");
                const json context = json::parse(messages[1].content);
                REQUIRE_SEMANTIC(context.at("command").get<std::string>() == expected_command);
                const json proposal = {
                    {"action", action},
                    {"origin", origin},
                    {"object_id", object_id},
                    {"quote", quote},
                    {"label", label},
                    {"visual_id", visual_id},
                    {"verdict", verdict},
                    {"durable", durable},
                    {"material_source", material_source},
                    {"reason", reason},
                    {"revision", context.at("revision").get<int>()},
                };
                result->success = true;
                result->reached_eog = true;
                result->response_text = FixtureWireAction(proposal,context).dump();
                return true;
            };
    }
};

void TestFreeFormExplicitTake(const std::string& command) {
    SessionState source = Fixture();
    DepositChalkHere(source);
    const Snapshot before(source);
    const std::size_t instances_before = source.objects.instances.size();

    Harness harness;
    harness.expected_command = command;
    harness.action = "take";
    harness.origin = "explicit";
    harness.object_id = kChalk;
    harness.verdict = "allow";
    harness.reason = "Vous rangez la craie avec vos affaires.";

    Scene scene;
    SessionState candidate = source;
    const ObjectResolution resolution =
        ResolveObjectAction(source, command, harness.config, scene, &candidate);

    REQUIRE_SEMANTIC(harness.calls == 1);
    REQUIRE_SEMANTIC(resolution.handled);
    REQUIRE_SEMANTIC(resolution.changed);
    REQUIRE_SEMANTIC(resolution.action == "take");
    REQUIRE_SEMANTIC(resolution.object_id == kChalk);
    REQUIRE_SEMANTIC(resolution.reason.empty());

    RequireUnchanged(source, before);
    REQUIRE_SEMANTIC(DepositCount(source, source.current_place_id) == 1);
    REQUIRE_SEMANTIC(CountId(candidate.hard_state.inventory_items, kChalk) == 1);
    REQUIRE_SEMANTIC(DepositCount(candidate, source.current_place_id) == 0);
    REQUIRE_SEMANTIC(candidate.objects.instances.size() == instances_before);
    REQUIRE_SEMANTIC(candidate.objects.instances.count(kChalk) == 1);
    REQUIRE_SEMANTIC(candidate.objects.revision == source.objects.revision + 1);
    std::string why;
    REQUIRE_SEMANTIC(ValidateObjectState(candidate, &why));
}

void TestAbsentObjectIdIsRefused() {
    SessionState source = Fixture();
    DepositChalkHere(source);
    const Snapshot before(source);

    Harness harness;
    harness.expected_command = "ramasser la craie au sol";
    harness.action = "take";
    harness.origin = "explicit";
    harness.object_id = "object:absent";
    harness.verdict = "allow";
    harness.reason = "Une craie se trouverait ici.";

    Scene scene;
    SessionState candidate = source;
    const ObjectResolution resolution =
        ResolveObjectAction(source, harness.expected_command, harness.config, scene, &candidate);

    REQUIRE_SEMANTIC(harness.calls == 1);
    REQUIRE_SEMANTIC(resolution.handled);
    REQUIRE_SEMANTIC(!resolution.changed);
    REQUIRE_SEMANTIC(!resolution.reason.empty());
    REQUIRE_SEMANTIC(candidate.objects.instances.count("object:absent") == 0);

    RequireUnchanged(source, before);
    RequireUnchanged(candidate, before);
    REQUIRE_SEMANTIC(DepositCount(candidate, source.current_place_id) == 1);
    std::string why;
    REQUIRE_SEMANTIC(ValidateObjectState(candidate, &why));
}

void TestObjectDepositedElsewhereIsRefused() {
    SessionState source = Fixture();
    DepositChalkHere(source);
    source.objects.deposits.at(source.current_place_id).clear();
    ObjectPlacement placement;
    placement.object_id = kChalk;
    source.objects.deposits[kElsewhere].push_back(placement);
    std::string valid;
    REQUIRE_SEMANTIC(ValidateObjectState(source, &valid));
    const Snapshot before(source);

    Harness harness;
    harness.expected_command = "ramasser la craie au sol";
    harness.action = "take";
    harness.origin = "explicit";
    harness.object_id = kChalk;
    harness.verdict = "allow";
    harness.reason = "La craie serait à portée.";

    Scene scene;
    SessionState candidate = source;
    const ObjectResolution resolution =
        ResolveObjectAction(source, harness.expected_command, harness.config, scene, &candidate);

    REQUIRE_SEMANTIC(harness.calls == 1);
    REQUIRE_SEMANTIC(resolution.handled);
    REQUIRE_SEMANTIC(!resolution.changed);
    REQUIRE_SEMANTIC(!resolution.reason.empty());

    RequireUnchanged(source, before);
    RequireUnchanged(candidate, before);
    REQUIRE_SEMANTIC(CountId(candidate.hard_state.inventory_items, kChalk) == 0);
    REQUIRE_SEMANTIC(DepositCount(candidate, candidate.current_place_id) == 0);
    REQUIRE_SEMANTIC(DepositCount(candidate, kElsewhere) == 1);
    std::string why;
    REQUIRE_SEMANTIC(ValidateObjectState(candidate, &why));
}

void TestInspectionIsPerceiveWithoutChange() {
    SessionState source = Fixture();
    DepositChalkHere(source);
    const Snapshot before(source);

    Harness harness;
    harness.expected_command = "examiner la craie au sol";
    harness.action = "perceive";
    harness.origin = "none";
    harness.verdict = "allow";

    Scene scene;
    SessionState candidate = source;
    const ObjectResolution resolution =
        ResolveObjectAction(source, harness.expected_command, harness.config, scene, &candidate);

    REQUIRE_SEMANTIC(harness.calls == 1);
    REQUIRE_SEMANTIC(!resolution.handled);
    REQUIRE_SEMANTIC(resolution.action == "perceive");
    REQUIRE_SEMANTIC(resolution.object_id.empty());
    REQUIRE_SEMANTIC(resolution.reason.empty());
    REQUIRE_SEMANTIC(!resolution.changed);

    RequireUnchanged(source, before);
    RequireUnchanged(candidate, before);
    REQUIRE_SEMANTIC(DepositCount(candidate, candidate.current_place_id) == 1);
    std::string why;
    REQUIRE_SEMANTIC(ValidateObjectState(candidate, &why));
}

void TestInventedImplicitQuoteIsRefused() {
    SessionState source = Fixture();
    DepositChalkHere(source);
    const Snapshot before(source);
    const std::size_t instances_before = source.objects.instances.size();

    Harness harness;
    harness.expected_command = "ramasser la poussière de craie";
    harness.action = "take";
    harness.origin = "implicit";
    harness.quote = "Une fine poussière de craie repose sur le sol.";
    harness.label = "poussière de craie";
    harness.verdict = "allow";
    harness.material_source = true;
    harness.reason = "Un petit prélèvement de cette poussière est possible.";

    Scene scene;
    SessionState candidate = source;
    const ObjectResolution resolution =
        ResolveObjectAction(source, harness.expected_command, harness.config, scene, &candidate);

    REQUIRE_SEMANTIC(harness.calls == 1);
    REQUIRE_SEMANTIC(resolution.handled);
    REQUIRE_SEMANTIC(!resolution.changed);
    REQUIRE_SEMANTIC(!resolution.reason.empty());

    RequireUnchanged(source, before);
    RequireUnchanged(candidate, before);
    REQUIRE_SEMANTIC(candidate.objects.instances.size() == instances_before);
    REQUIRE_SEMANTIC(candidate.objects.sources.empty());
    REQUIRE_SEMANTIC(CountId(candidate.hard_state.inventory_items, kChalk) == 0);
    REQUIRE_SEMANTIC(DepositCount(candidate, candidate.current_place_id) == 1);
    std::string why;
    REQUIRE_SEMANTIC(ValidateObjectState(candidate, &why));
}

}  // namespace

void RunObjectSemanticTests() {
    TestFreeFormExplicitTake("ramasser la craie au sol");
    TestFreeFormExplicitTake("je voudrais reprendre ce que je viens de poser");
    TestFreeFormExplicitTake("celle qui est à mes pieds, je la remets avec mes affaires");
    TestAbsentObjectIdIsRefused();
    TestObjectDepositedElsewhereIsRefused();
    TestInspectionIsPerceiveWithoutChange();
    TestInventedImplicitQuoteIsRefused();
    // Exercise the complete router too: no verb/prefix recognized in C++.
    auto source=Fixture();DepositChalkHere(source);Harness harness;
    REQUIRE_SEMANTIC(ObjectNarrationAllowed(source,"Vous vous trouvez dans un espace vaste, baigné d'une lumière diffuse.","","",nullptr));
    harness.expected_command="celle qui est à mes pieds, je la remets avec mes affaires";
    harness.action="take";harness.origin="explicit";harness.object_id=kChalk;harness.verdict="allow";
    harness.config.local_scenes=true;harness.config.investigation_opportunities=false;
    harness.config.generation_config.prompt_token_counter=[](const std::vector<LlmPromptMessage>&,int* count,std::string*){*count=1000;return true;};
    int narrations=0;
    harness.config.narrative_completion=[&](const LlmGenerationConfig&,const std::vector<LlmPromptMessage>&,LlmStreamCallback,void*,LlmGenerationResult*){++narrations;return false;};
    HeadlessTurnResult turn;char error[512]={};
    REQUIRE_SEMANTIC(RunHeadlessTurnFromState(source,harness.expected_command.c_str(),harness.config,&turn,error,sizeof(error)));
    REQUIRE_SEMANTIC(harness.calls==1&&narrations==1&&turn.authorized_object_id==kChalk);
    REQUIRE_SEMANTIC(turn.updated_hard_state.inventory_items.size()==3);
    // A failed provenance check is final: no narrator invents an obstacle.
    harness.expected_command="un petit prélèvement, si possible";
    harness.origin="implicit";harness.object_id="";harness.quote="De la poussière repose ici.";harness.label="poussière";
    REQUIRE_SEMANTIC(RunHeadlessTurnFromState(source,harness.expected_command.c_str(),harness.config,&turn,error,sizeof(error)));
    REQUIRE_SEMANTIC(harness.calls==2&&narrations==1&&turn.authorized_object_id.empty());
    REQUIRE_SEMANTIC(turn.updated_objects.revision==source.objects.revision);
    harness.config.reuse_unchanged_view=true;
    REQUIRE_SEMANTIC(RunHeadlessTurnFromState(source,"inventaire",harness.config,&turn,error,sizeof(error)));
    REQUIRE_SEMANTIC(harness.calls==2&&turn.view_unchanged&&turn.rendered_scene.triangles.empty());
}
