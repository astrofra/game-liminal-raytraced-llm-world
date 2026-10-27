// DUO-045 investigation conservation contract tests.
// The investigation module is implemented by Codex; this file only exercises
// the fixed public API. No framework dependency and no main().
#include "investigation.h"
#include "object_interactions.h"
#include "turn_runner.h"
#include "nlohmann/json.hpp"

#include <cstddef>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

using namespace liminal;

#define REQUIRE_INVESTIGATION(condition)                                       \
    do {                                                                       \
        if (!(condition)) {                                                    \
            throw std::runtime_error(                                          \
                std::string("investigation contract line ") +                  \
                std::to_string(__LINE__) + ": " + #condition);                 \
        }                                                                      \
    } while (false)

namespace {

using json = nlohmann::json;

const char* const kRoomA = "generated:room_000001";
const char* const kRoomB = "generated:room_000002";
const char* const kCrystalClue = "clue:crystal_sample";
const char* const kEquipmentClue = "clue:vey_equipment";
const char* const kSurveyClue = "clue:vey_survey";
const char* const kObstruction = "invisible_obstruction";

SessionState BaseFixture() {
    SessionState state;
    char error[512] = {};
    REQUIRE_INVESTIGATION(InitializeSessionState(
        kLocationQuarryThreshold, &state, error, sizeof(error)));
    EnsureInvestigation(&state);
    EnsureObjectRegistry(&state);
    return state;
}

void AddPlace(SessionState& state, const std::string& id, const std::string& title) {
    GeneratedRoom room;
    room.room_id = id;
    room.spatial_state.room_title = title;
    room.spatial_state.location_id = kLocationUnknown;
    room.scene_text = title;
    state.generated_rooms.push_back(room);
    PreserveLegacyInvestigationPlaces(&state);
}

void SetCurrentPlace(SessionState& state, const std::string& id) {
    state.current_place_id = id;
    state.hard_state.current_location_id = kLocationUnknown;
    for (const GeneratedRoom& room : state.generated_rooms) {
        if (room.room_id == id) {
            state.spatial_state = room.spatial_state;
            return;
        }
    }
}

SessionState PlaceFixture() {
    SessionState state = BaseFixture();
    AddPlace(state, kRoomA, "Seuil de la roche");
    AddPlace(state, kRoomB, "Repli ombrage");
    SetCurrentPlace(state, kRoomA);
    return state;
}

ObjectPlacement PlacementFor(const std::string& object_id, float x, float y, float z) {
    ObjectPlacement placement;
    placement.object_id = object_id;
    placement.x = x;
    placement.y = y;
    placement.z = z;
    return placement;
}

ObjectProposal TakeProposal(const std::string& id, int revision) {
    ObjectProposal proposal;
    proposal.action = "take";
    proposal.origin = "explicit";
    proposal.object_id = id;
    proposal.verdict = "allow";
    proposal.revision = revision;
    return proposal;
}

ObjectProposal DropProposal(const std::string& id, int revision) {
    ObjectProposal proposal;
    proposal.action = "drop";
    proposal.origin = "explicit";
    proposal.object_id = id;
    proposal.verdict = "allow";
    proposal.revision = revision;
    return proposal;
}

bool ContainsString(const std::vector<std::string>& values, const std::string& value) {
    for (const std::string& entry : values) {
        if (entry == value) return true;
    }
    return false;
}

const InvestigationThread* FindThread(const InvestigationState& investigation,
                                      const std::string& id) {
    for (const InvestigationThread& thread : investigation.threads) {
        if (thread.id == id) return &thread;
    }
    return nullptr;
}

const InvestigationEvidence* FindEvidence(const InvestigationState& investigation,
                                          const std::string& id) {
    for (const InvestigationEvidence& evidence : investigation.evidence) {
        if (evidence.id == id) return &evidence;
    }
    return nullptr;
}

int CountEvidence(const InvestigationState& investigation, const std::string& id) {
    int count = 0;
    for (const InvestigationEvidence& evidence : investigation.evidence) {
        if (evidence.id == id) ++count;
    }
    return count;
}

const InvestigationHypothesis* FindHypothesis(const InvestigationState& investigation,
                                              const std::string& id) {
    for (const InvestigationHypothesis& hypothesis : investigation.hypotheses) {
        if (hypothesis.id == id) return &hypothesis;
    }
    return nullptr;
}

const InvestigationOpportunity* FindOpportunity(const InvestigationState& investigation,
                                                const std::string& id) {
    for (const InvestigationOpportunity& opportunity : investigation.opportunities) {
        if (opportunity.id == id) return &opportunity;
    }
    return nullptr;
}

bool ContextMentionsEvidence(const std::string& context,
                             const InvestigationEvidence& evidence) {
    if (context.find(evidence.id) != std::string::npos) return true;
    return !evidence.fact.empty() && context.find(evidence.fact) != std::string::npos;
}

struct Snapshot {
    std::string investigation;
    std::string objects;
    std::vector<std::string> inventory;

    explicit Snapshot(const SessionState& state)
        : investigation(SerializeInvestigation(state.investigation))
        , objects(SerializeObjectState(state.objects))
        , inventory(state.hard_state.inventory_items)
    {
    }
};

void RequireUnchanged(const SessionState& state, const Snapshot& before) {
    REQUIRE_INVESTIGATION(SerializeInvestigation(state.investigation) == before.investigation);
    REQUIRE_INVESTIGATION(SerializeObjectState(state.objects) == before.objects);
    REQUIRE_INVESTIGATION(state.hard_state.inventory_items == before.inventory);
}

bool PrepareOpportunity(const SessionState& source, const std::string& id,
                        const std::string& place, CardinalDirection direction,
                        const ObjectPlacement* placement, SessionState* candidate,
                        std::string* error) {
    *candidate = source;
    if(!PrepareInvestigationOpportunity(source,id,place,direction,placement,candidate,error))return false;
    // This historical fixture directly authors a preserved world, outside runtime distribution.
    for(auto& decision:candidate->investigation.decisions)if(decision.place_id==place) {
        decision.opportunity_ids.clear();
        for(const auto& o:candidate->investigation.opportunities)if(o.place_id==place)decision.opportunity_ids.push_back(o.id);
    }
    return true;
}

bool PrepareObservation(const SessionState& source, const std::string& id, int revision,
                        SessionState* candidate, std::string* error) {
    *candidate = source;
    return PrepareInvestigationObservation(source, id, revision, candidate, error);
}

SessionState PrepareRequiredOpportunity(const SessionState& source, const std::string& id,
                                        const std::string& place,
                                        CardinalDirection direction,
                                        const ObjectPlacement* placement) {
    SessionState candidate;
    std::string error;
    REQUIRE_INVESTIGATION(PrepareOpportunity(
        source, id, place, direction, placement, &candidate, &error));
    return candidate;
}

SessionState ObserveRequired(const SessionState& source, const std::string& id,
                             int revision) {
    SessionState candidate;
    std::string error;
    REQUIRE_INVESTIGATION(PrepareObservation(source, id, revision, &candidate, &error));
    return candidate;
}

SessionState AcquireRequired(const SessionState& source, const std::string& object_id) {
    SessionState candidate;
    std::string reason;
    REQUIRE_INVESTIGATION(PrepareObjectTransfer(
        source, TakeProposal(object_id, source.objects.revision), nullptr,
        &candidate, &reason));
    return candidate;
}

// Returns the index of the first matching invisible barrier, adding one at
// discovered=false when the preparation did not create it.
std::size_t EnsureBarrier(SessionState* state, const std::string& place,
                          CardinalDirection direction) {
    for (std::size_t i = 0; i < state->invisible_barriers.size(); ++i) {
        InvisibleBarrier& barrier = state->invisible_barriers[i];
        if (barrier.place_id == place && barrier.direction == direction) {
            barrier.discovered = false;
            return i;
        }
    }
    InvisibleBarrier barrier;
    barrier.place_id = place;
    barrier.direction = direction;
    barrier.discovered = false;
    state->invisible_barriers.push_back(barrier);
    return state->invisible_barriers.size() - 1;
}

void TestInitialisationMigrationAndVersions() {
    SessionState state = PlaceFixture();
    std::string valid_error;
    REQUIRE_INVESTIGATION(ValidateInvestigation(state, &valid_error));
    REQUIRE_INVESTIGATION(state.investigation.evidence.empty());
    REQUIRE_INVESTIGATION(state.investigation.threads.size() == 3);
    for (const char* id : {"vey", "obstruction", "crystal"}) {
        const InvestigationThread* thread = FindThread(state.investigation, id);
        REQUIRE_INVESTIGATION(thread != nullptr);
        REQUIRE_INVESTIGATION(thread->status == "open");
        REQUIRE_INVESTIGATION(thread->evidence_ids.empty());
    }

    const std::string ensured = SerializeInvestigation(state.investigation);
    EnsureInvestigation(&state);
    REQUIRE_INVESTIGATION(SerializeInvestigation(state.investigation) == ensured);

    const std::string serialized = SerializeInvestigation(state.investigation);
    REQUIRE_INVESTIGATION(!serialized.empty());
    InvestigationState parsed;
    std::string error;
    REQUIRE_INVESTIGATION(ParseInvestigation(serialized, &parsed, &error));
    REQUIRE_INVESTIGATION(SerializeInvestigation(parsed) == serialized);

    InvestigationState future = state.investigation;
    future.schema_version = 999;
    const std::string future_text = SerializeInvestigation(future);
    REQUIRE_INVESTIGATION(!future_text.empty());
    InvestigationState rejected;
    std::string future_error;
    REQUIRE_INVESTIGATION(!ParseInvestigation(future_text, &rejected, &future_error));
    REQUIRE_INVESTIGATION(!future_error.empty());
    auto fractional=json::parse(serialized);fractional["revision"]=0.5;
    REQUIRE_INVESTIGATION(!ParseInvestigation(fractional.dump(), &rejected, &future_error));
    fractional=json::parse(serialized);fractional["schema_version"]=1.5;
    REQUIRE_INVESTIGATION(!ParseInvestigation(fractional.dump(), &rejected, &future_error));

    SessionState observed = PrepareRequiredOpportunity(
        state, "vey_equipment", kRoomA, kDirectionUnknown,
        std::make_unique<ObjectPlacement>(PlacementFor(kEquipmentClue, 1.0f, 0.0f, 1.0f)).get());
    observed = ObserveRequired(observed, "vey_equipment",
                               observed.investigation.revision);
    REQUIRE_INVESTIGATION(!observed.investigation.evidence.empty());

    std::string session_text;
    REQUIRE_INVESTIGATION(SerializeSessionStateToJsonString(observed, &session_text));
    REQUIRE_INVESTIGATION(!session_text.empty());

    SessionState roundtrip;
    char roundtrip_error[512] = {};
    REQUIRE_INVESTIGATION(ParseSessionStateFromJson(
        session_text.c_str(), &roundtrip, roundtrip_error, sizeof(roundtrip_error)));
    REQUIRE_INVESTIGATION(
        SerializeInvestigation(roundtrip.investigation) ==
        SerializeInvestigation(observed.investigation));
    REQUIRE_INVESTIGATION(roundtrip.current_place_id == observed.current_place_id);
    REQUIRE_INVESTIGATION(roundtrip.objects.instances.size() == observed.objects.instances.size());

    json document = json::parse(session_text);
    REQUIRE_INVESTIGATION(document.contains("investigation"));
    document.erase("investigation");
    const std::string legacy_text = document.dump();

    SessionState migrated;
    char migration_error[512] = {};
    REQUIRE_INVESTIGATION(ParseSessionStateFromJson(
        legacy_text.c_str(), &migrated, migration_error, sizeof(migration_error)));
    REQUIRE_INVESTIGATION(migrated.investigation.evidence.empty());
    EnsureInvestigation(&migrated);
    REQUIRE_INVESTIGATION(migrated.investigation.evidence.empty());
    REQUIRE_INVESTIGATION(FindThread(migrated.investigation, "vey") != nullptr);
    REQUIRE_INVESTIGATION(FindThread(migrated.investigation, "obstruction") != nullptr);
    REQUIRE_INVESTIGATION(FindThread(migrated.investigation, "crystal") != nullptr);
    REQUIRE_INVESTIGATION(migrated.current_place_id == observed.current_place_id);
    REQUIRE_INVESTIGATION(migrated.generated_rooms.size() == observed.generated_rooms.size());
    REQUIRE_INVESTIGATION(migrated.objects.instances.size() == observed.objects.instances.size());
    REQUIRE_INVESTIGATION(migrated.hard_state.score == observed.hard_state.score);
    AddPlace(migrated, "generated:room_000003", "Future place");
    SetCurrentPlace(migrated, "generated:room_000003");
    const auto legacy_object = migrated.objects.instances.at(kEquipmentClue);
    const auto placement = PlacementFor("", 1, 0, 1);
    auto future_clue = PrepareRequiredOpportunity(migrated, "vey_equipment",
        migrated.current_place_id, kDirectionUnknown, &placement);
    REQUIRE_INVESTIGATION(future_clue.objects.instances.at(kEquipmentClue).origin_place == legacy_object.origin_place);
    REQUIRE_INVESTIGATION(future_clue.investigation.opportunities.front().object_id != kEquipmentClue);
    REQUIRE_INVESTIGATION(future_clue.investigation.evidence.empty());
    REQUIRE_INVESTIGATION(ValidateInvestigation(future_clue, &valid_error));

    json corrupted = json::parse(session_text);
    corrupted["investigation"]["schema_version"] = 999;
    SessionState versioned;
    char version_error[512] = {};
    REQUIRE_INVESTIGATION(!ParseSessionStateFromJson(
        corrupted.dump().c_str(), &versioned, version_error, sizeof(version_error)));
}

void TestObservationRefusalsAndIdempotence() {
    SessionState state = PlaceFixture();
    state = PrepareRequiredOpportunity(
        state, "vey_equipment", kRoomA, kDirectionUnknown,
        std::make_unique<ObjectPlacement>(PlacementFor(kEquipmentClue, 1.0f, 0.0f, 1.0f)).get());
    REQUIRE_INVESTIGATION(state.investigation.evidence.empty());
    REQUIRE_INVESTIGATION(state.objects.instances.count(kEquipmentClue) == 1);
    REQUIRE_INVESTIGATION(!state.objects.instances.at(kEquipmentClue).portable);
    REQUIRE_INVESTIGATION(state.objects.deposits.count(kRoomA) == 1);
    REQUIRE_INVESTIGATION(FindOpportunity(state.investigation, "vey_equipment") != nullptr);

    {
        const Snapshot source_before(state);
        SessionState candidate = state;
        const Snapshot candidate_before(candidate);
        std::string error;
        REQUIRE_INVESTIGATION(!PrepareInvestigationObservation(
            state, "unknown_opportunity", state.investigation.revision,
            &candidate, &error));
        REQUIRE_INVESTIGATION(!error.empty());
        RequireUnchanged(candidate, candidate_before);
        RequireUnchanged(state, source_before);
    }
    {
        const Snapshot source_before(state);
        SessionState candidate = state;
        const Snapshot candidate_before(candidate);
        std::string error;
        REQUIRE_INVESTIGATION(!PrepareInvestigationObservation(
            state, "vey_equipment", state.investigation.revision + 1,
            &candidate, &error));
        REQUIRE_INVESTIGATION(!error.empty());
        RequireUnchanged(candidate, candidate_before);
        RequireUnchanged(state, source_before);
    }
    {
        SessionState distant = state;
        SetCurrentPlace(distant, kRoomB);
        const Snapshot source_before(distant);
        SessionState candidate = distant;
        const Snapshot candidate_before(candidate);
        std::string error;
        REQUIRE_INVESTIGATION(!PrepareInvestigationObservation(
            distant, "vey_equipment", distant.investigation.revision,
            &candidate, &error));
        REQUIRE_INVESTIGATION(!error.empty());
        RequireUnchanged(candidate, candidate_before);
        RequireUnchanged(distant, source_before);
    }

    SessionState observed = ObserveRequired(state, "vey_equipment",
                                            state.investigation.revision);
    REQUIRE_INVESTIGATION(CountEvidence(observed.investigation, "vey_equipment") == 1);
    REQUIRE_INVESTIGATION(CountEvidence(observed.investigation, "vey_survey") == 0);
    REQUIRE_INVESTIGATION(FindEvidence(observed.investigation, "vey_equipment") != nullptr);
    const int observation_revision = observed.investigation.revision;
    const Snapshot observed_before(observed);
    SessionState repeated;
    std::string repeat_error;
    REQUIRE_INVESTIGATION(PrepareObservation(
        observed, "vey_equipment", observation_revision, &repeated, &repeat_error));
    RequireUnchanged(repeated, observed_before);
    REQUIRE_INVESTIGATION(repeated.investigation.revision == observation_revision);
    REQUIRE_INVESTIGATION(CountEvidence(repeated.investigation, "vey_equipment") == 1);
}

void TestCrystalAcquisitionAndPossession() {
    SessionState state = PlaceFixture();
    const int initial_score = state.hard_state.score;
    state.hard_state.turn_number = 3;
    state = PrepareRequiredOpportunity(
        state, "crystal_sample", kRoomA, kDirectionUnknown,
        std::make_unique<ObjectPlacement>(PlacementFor(kCrystalClue, 2.0f, 0.0f, 1.0f)).get());
    REQUIRE_INVESTIGATION(state.investigation.evidence.empty());
    REQUIRE_INVESTIGATION(state.objects.instances.count(kCrystalClue) == 1);
    REQUIRE_INVESTIGATION(state.objects.instances.at(kCrystalClue).portable);

    state = ObserveRequired(state, "crystal_sample", state.investigation.revision);
    REQUIRE_INVESTIGATION(CountEvidence(state.investigation, "crystal_source") == 1);
    REQUIRE_INVESTIGATION(CountEvidence(state.investigation, "crystal_acquired") == 0);

    {
        SessionState same = state;
        const Snapshot before(same);
        RecordInvestigationTransfer(state, "take", kCrystalClue, &same);
        RequireUnchanged(same, before);
        REQUIRE_INVESTIGATION(CountEvidence(same.investigation, "crystal_acquired") == 0);
    }
    {
        SessionState ordinary = state;
        ObjectInstance instance;
        instance.id = "object:ordinary_crystal";
        instance.label = "cristal de Vey";
        instance.description = "un galet ordinaire";
        instance.origin_place = kRoomA;
        instance.portable = true;
        ordinary.objects.instances[instance.id] = instance;
        ordinary.objects.deposits[kRoomA].push_back(
            PlacementFor(instance.id, 3.0f, 0.0f, 1.0f));
        std::string valid;
        REQUIRE_INVESTIGATION(ValidateObjectState(ordinary, &valid));
        SessionState taken;
        std::string reason;
        REQUIRE_INVESTIGATION(PrepareObjectTransfer(
            ordinary, TakeProposal(instance.id, ordinary.objects.revision), nullptr,
            &taken, &reason));
        const std::size_t acquired_before =
            taken.investigation.evidence.size();
        RecordInvestigationTransfer(ordinary, "take", instance.id, &taken);
        REQUIRE_INVESTIGATION(CountEvidence(taken.investigation, "crystal_acquired") == 0);
        REQUIRE_INVESTIGATION(taken.investigation.evidence.size() == acquired_before);
        REQUIRE_INVESTIGATION(taken.hard_state.score == initial_score);
    }

    SessionState carrying = AcquireRequired(state, kCrystalClue);
    REQUIRE_INVESTIGATION(ContainsString(carrying.hard_state.inventory_items, kCrystalClue));
    RecordInvestigationTransfer(state, "take", kCrystalClue, &carrying);
    REQUIRE_INVESTIGATION(CountEvidence(carrying.investigation, "crystal_acquired") == 1);
    REQUIRE_INVESTIGATION(CountEvidence(carrying.investigation, "crystal_source") == 1);
    REQUIRE_INVESTIGATION(carrying.hard_state.score == initial_score);
    const InvestigationThread* crystal_thread = FindThread(carrying.investigation, "crystal");
    REQUIRE_INVESTIGATION(crystal_thread != nullptr);
    REQUIRE_INVESTIGATION(crystal_thread->status == "in_progress");
    for(const auto& hypothesis:carrying.investigation.hypotheses)
        REQUIRE_INVESTIGATION(hypothesis.supporting_evidence.empty()); // No Vey association from a specimen alone.

    SessionState at_b = carrying;
    SetCurrentPlace(at_b, kRoomB);
    SessionState dropped;
    std::string drop_reason;
    REQUIRE_INVESTIGATION(PrepareObjectTransfer(
        at_b, DropProposal(kCrystalClue, at_b.objects.revision),
        std::make_unique<ObjectPlacement>(PlacementFor(kCrystalClue, 0.0f, 0.0f, 1.0f)).get(), &dropped, &drop_reason));
    REQUIRE_INVESTIGATION(!ContainsString(dropped.hard_state.inventory_items, kCrystalClue));
    REQUIRE_INVESTIGATION(
        dropped.objects.deposits.count(kRoomB) == 1 &&
        dropped.objects.deposits.at(kRoomB).size() == 1);
    REQUIRE_INVESTIGATION(CountEvidence(dropped.investigation, "crystal_acquired") == 1);
    REQUIRE_INVESTIGATION(dropped.hard_state.score == initial_score);

    SessionState retaken;
    std::string take_reason;
    REQUIRE_INVESTIGATION(PrepareObjectTransfer(
        dropped, TakeProposal(kCrystalClue, dropped.objects.revision), nullptr,
        &retaken, &take_reason));
    REQUIRE_INVESTIGATION(ContainsString(retaken.hard_state.inventory_items, kCrystalClue));
    REQUIRE_INVESTIGATION(retaken.objects.instances.count(kCrystalClue) == 1);
    REQUIRE_INVESTIGATION(retaken.objects.instances.size() == carrying.objects.instances.size());
    RecordInvestigationTransfer(dropped, "take", kCrystalClue, &retaken);
    REQUIRE_INVESTIGATION(CountEvidence(retaken.investigation, "crystal_acquired") == 1);
    REQUIRE_INVESTIGATION(CountEvidence(retaken.investigation, "crystal_source") == 1);
    REQUIRE_INVESTIGATION(retaken.hard_state.score == initial_score);
    REQUIRE_INVESTIGATION(FindEvidence(retaken.investigation, "crystal_acquired") != nullptr);
}

void TestVeyTracesSupportAndContradiction() {
    SessionState state = PlaceFixture();
    state = PrepareRequiredOpportunity(
        state, "vey_equipment", kRoomA, kDirectionUnknown,
        std::make_unique<ObjectPlacement>(PlacementFor(kEquipmentClue, 1.0f, 0.0f, 1.0f)).get());
    state = PrepareRequiredOpportunity(
        state, "vey_survey", kRoomA, kDirectionUnknown,
        std::make_unique<ObjectPlacement>(PlacementFor(kSurveyClue, 1.5f, 0.0f, 1.0f)).get());
    REQUIRE_INVESTIGATION(state.investigation.evidence.empty());
    REQUIRE_INVESTIGATION(state.objects.instances.at(kEquipmentClue).portable == false);
    REQUIRE_INVESTIGATION(state.objects.instances.at(kSurveyClue).portable == false);

    state = ObserveRequired(state, "vey_equipment", state.investigation.revision);
    REQUIRE_INVESTIGATION(CountEvidence(state.investigation, "vey_equipment") == 1);
    REQUIRE_INVESTIGATION(CountEvidence(state.investigation, "vey_survey") == 0);
    state = ObserveRequired(state, "vey_survey", state.investigation.revision);
    REQUIRE_INVESTIGATION(CountEvidence(state.investigation, "vey_equipment") == 1);
    REQUIRE_INVESTIGATION(CountEvidence(state.investigation, "vey_survey") == 1);

    const InvestigationHypothesis* prospecting =
        FindHypothesis(state.investigation, "vey_prospecting");
    REQUIRE_INVESTIGATION(prospecting != nullptr);
    REQUIRE_INVESTIGATION(prospecting->status == "supported");
    REQUIRE_INVESTIGATION(
        ContainsString(prospecting->supporting_evidence, "vey_equipment"));
    REQUIRE_INVESTIGATION(
        ContainsString(prospecting->supporting_evidence, "vey_survey"));

    const InvestigationHypothesis* stayed =
        FindHypothesis(state.investigation, "vey_stayed_at_sample");
    REQUIRE_INVESTIGATION(stayed != nullptr);
    REQUIRE_INVESTIGATION(stayed->status == "contradicted");
    REQUIRE_INVESTIGATION(
        ContainsString(stayed->contradicting_evidence, "vey_survey"));
    REQUIRE_INVESTIGATION(FindEvidence(state.investigation, "vey_equipment") != nullptr);
    REQUIRE_INVESTIGATION(FindEvidence(state.investigation, "vey_survey") != nullptr);

    const InvestigationThread* thread = FindThread(state.investigation, "vey");
    REQUIRE_INVESTIGATION(thread != nullptr);
    REQUIRE_INVESTIGATION(thread->status == "in_progress");
    REQUIRE_INVESTIGATION(thread->status != "resolved");
    REQUIRE_INVESTIGATION(ContainsString(thread->evidence_ids, "vey_equipment"));
    REQUIRE_INVESTIGATION(ContainsString(thread->evidence_ids, "vey_survey"));
}

void TestObstructionContactAndExamination() {
    SessionState state = PlaceFixture();
    state = PrepareRequiredOpportunity(
        state, kObstruction, kRoomA, kDirectionEast, nullptr);
    const std::size_t barrier_index = EnsureBarrier(&state, kRoomA, kDirectionEast);
    REQUIRE_INVESTIGATION(state.invisible_barriers[barrier_index].discovered == false);
    REQUIRE_INVESTIGATION(state.investigation.evidence.empty());
    REQUIRE_INVESTIGATION(FindOpportunity(state.investigation, kObstruction) != nullptr);

    {
        const Snapshot source_before(state);
        SessionState candidate = state;
        const Snapshot candidate_before(candidate);
        std::string error;
        REQUIRE_INVESTIGATION(!PrepareInvestigationObservation(
            state, kObstruction, state.investigation.revision, &candidate, &error));
        REQUIRE_INVESTIGATION(!error.empty());
        RequireUnchanged(candidate, candidate_before);
        RequireUnchanged(state, source_before);
    }
    {
        SessionState candidate = state;
        const Snapshot before(candidate);
        RecordInvestigationContact(state, kDirectionEast, &candidate);
        RequireUnchanged(candidate, before);
        REQUIRE_INVESTIGATION(
            CountEvidence(candidate.investigation, "obstruction_contact") == 0);
    }
    {
        SessionState blocked = state;
        blocked.invisible_barriers.clear();
        blocked.spatial_state.blocked_exits.push_back("east");
        const Snapshot before(blocked);
        SessionState candidate = blocked;
        RecordInvestigationContact(blocked, kDirectionEast, &candidate);
        REQUIRE_INVESTIGATION(
            CountEvidence(candidate.investigation, "obstruction_contact") == 0);
        REQUIRE_INVESTIGATION(
            CountEvidence(candidate.investigation, "obstruction_examined") == 0);
        RequireUnchanged(candidate, before);
    }

    SessionState contacted = state;
    contacted.invisible_barriers[barrier_index].discovered = true;
    RecordInvestigationContact(state, kDirectionEast, &contacted);
    REQUIRE_INVESTIGATION(
        CountEvidence(contacted.investigation, "obstruction_contact") == 1);
    REQUIRE_INVESTIGATION(
        CountEvidence(contacted.investigation, "obstruction_examined") == 0);

    {
        SessionState repeated = contacted;
        RecordInvestigationContact(contacted, kDirectionEast, &repeated);
        REQUIRE_INVESTIGATION(
            CountEvidence(repeated.investigation, "obstruction_contact") == 1);
    }

    SessionState examined = ObserveRequired(
        contacted, kObstruction, contacted.investigation.revision);
    REQUIRE_INVESTIGATION(
        CountEvidence(examined.investigation, "obstruction_contact") == 1);
    REQUIRE_INVESTIGATION(
        CountEvidence(examined.investigation, "obstruction_examined") == 1);

    const int examination_revision = examined.investigation.revision;
    const Snapshot examined_before(examined);
    SessionState repeated;
    std::string repeat_error;
    REQUIRE_INVESTIGATION(PrepareObservation(
        examined, kObstruction, examination_revision, &repeated, &repeat_error));
    RequireUnchanged(repeated, examined_before);
    REQUIRE_INVESTIGATION(
        CountEvidence(repeated.investigation, "obstruction_examined") == 1);
}

void TestFortyTurnRetentionAndHiddenTruth() {
    SessionState state = PlaceFixture();
    state = PrepareRequiredOpportunity(
        state, "crystal_sample", kRoomA, kDirectionUnknown,
        std::make_unique<ObjectPlacement>(PlacementFor(kCrystalClue, 2.0f, 0.0f, 1.0f)).get());
    state = PrepareRequiredOpportunity(
        state, "vey_equipment", kRoomA, kDirectionUnknown,
        std::make_unique<ObjectPlacement>(PlacementFor(kEquipmentClue, 1.0f, 0.0f, 1.0f)).get());
    state = PrepareRequiredOpportunity(
        state, "vey_survey", kRoomA, kDirectionUnknown,
        std::make_unique<ObjectPlacement>(PlacementFor(kSurveyClue, 1.5f, 0.0f, 1.0f)).get());
    state = ObserveRequired(state, "crystal_sample", state.investigation.revision);
    state = ObserveRequired(state, "vey_equipment", state.investigation.revision);
    const SessionState before_acquire = state;
    SessionState carrying = AcquireRequired(before_acquire, kCrystalClue);
    RecordInvestigationTransfer(before_acquire, "take", kCrystalClue, &carrying);
    REQUIRE_INVESTIGATION(
        CountEvidence(carrying.investigation, "crystal_acquired") == 1);
    state = carrying;

    for (int turn = 1; turn <= 40; ++turn) {
        SessionTurnRecord record;
        record.turn_number = turn;
        record.location_id = kLocationUnknown;
        record.origin_place_id = kRoomA;
        record.place_id = kRoomA;
        record.player_command = "observer le seuil";
        record.intent = "perceive";
        record.narration = "Le seuil reste stable.";
        state.history.push_back(record);
        state.hard_state.turn_number = turn;
    }

    const std::string context_before = InvestigationContext(state);
    REQUIRE_INVESTIGATION(!context_before.empty());
    for (const char* id : {"crystal_source", "crystal_acquired", "vey_equipment"}) {
        const InvestigationEvidence* evidence = FindEvidence(state.investigation, id);
        REQUIRE_INVESTIGATION(evidence != nullptr);
        REQUIRE_INVESTIGATION(ContextMentionsEvidence(context_before, *evidence));
    }
    REQUIRE_INVESTIGATION(CountEvidence(state.investigation, "vey_survey") == 0);
    const InvestigationHypothesis* stayed =
        FindHypothesis(state.investigation, "vey_stayed_at_sample");
    REQUIRE_INVESTIGATION(
        stayed == nullptr || stayed->contradicting_evidence.empty());
    REQUIRE_INVESTIGATION(FindOpportunity(state.investigation, "vey_survey") != nullptr);

    state = ObserveRequired(state, "vey_survey", state.investigation.revision);
    const InvestigationEvidence* survey =
        FindEvidence(state.investigation, "vey_survey");
    REQUIRE_INVESTIGATION(survey != nullptr);
    if (!survey->fact.empty()) {
        bool proposition_reused = false;
        for (const InvestigationHypothesis& hypothesis : state.investigation.hypotheses) {
            if (hypothesis.proposition == survey->fact) proposition_reused = true;
        }
        if (!proposition_reused) {
            REQUIRE_INVESTIGATION(
                context_before.find(survey->fact) == std::string::npos);
        }
    }
    const InvestigationHypothesis* contradicted =
        FindHypothesis(state.investigation, "vey_stayed_at_sample");
    REQUIRE_INVESTIGATION(contradicted != nullptr);
    REQUIRE_INVESTIGATION(
        ContainsString(contradicted->contradicting_evidence, "vey_survey"));
    const std::string context_after = InvestigationContext(state);
    for (const char* id : {"crystal_source", "crystal_acquired", "vey_equipment", "vey_survey"}) {
        const InvestigationEvidence* evidence = FindEvidence(state.investigation, id);
        REQUIRE_INVESTIGATION(evidence != nullptr);
        REQUIRE_INVESTIGATION(ContextMentionsEvidence(context_after, *evidence));
    }
    REQUIRE_INVESTIGATION(!InvestigationJournal(state).empty());
}

}  // namespace

void RunInvestigationContractTests() {
    TestInitialisationMigrationAndVersions();
    TestObservationRefusalsAndIdempotence();
    TestCrystalAcquisitionAndPossession();
    TestVeyTracesSupportAndContradiction();
    TestObstructionContactAndExamination();
    TestFortyTurnRetentionAndHiddenTruth();
}
