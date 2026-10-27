// DUO-047 LLM investigation distribution contract tests.
// The distribution API is implemented by Codex; this file only exercises the
// fixed public contract. No framework dependency and no main().
#include "investigation.h"
#include "object_interactions.h"
#include "turn_runner.h"
#include "nlohmann/json.hpp"

#include <cstddef>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using namespace liminal;

#define REQUIRE_DISTRIBUTION(condition)                                        \
    do {                                                                       \
        if (!(condition)) {                                                    \
            throw std::runtime_error(                                          \
                std::string("investigation distribution line ") +              \
                std::to_string(__LINE__) + ": " + #condition);                 \
        }                                                                      \
    } while (false)

namespace {

using json = nlohmann::json;

const char* const kRoomA = "generated:room_000001";
const char* const kRoomB = "generated:room_000002";
const char* const kRoomC = "generated:room_000003";
const char* const kRoomD = "generated:room_000004";

const char* const kCrystal = "crystal_sample";
const char* const kObstruction = "invisible_obstruction";
const char* const kEquipment = "vey_equipment";
const char* const kSurvey = "vey_survey";

const char* const kLlmOrigin = "llm";
const char* const kLegacyOrigin = "legacy_preserved";
const char* const kLlmPolicy = "eryx-clues-llm-v2";

// ---------------------------------------------------------------------------
// Session fixtures.
// ---------------------------------------------------------------------------

SessionState BaseFixture() {
    SessionState state;
    char error[512] = {};
    REQUIRE_DISTRIBUTION(InitializeSessionState(
        kLocationQuarryThreshold, &state, error, sizeof(error)));
    EnsureInvestigation(&state);
    EnsureObjectRegistry(&state);
    state.hard_state.turn_number = 7;
    return state;
}

void AddPlace(SessionState& state, const std::string& id, const std::string& title) {
    GeneratedRoom room;
    room.room_id = id;
    room.spatial_state.room_title = title;
    room.spatial_state.location_id = kLocationUnknown;
    room.scene_text = title;
    state.generated_rooms.push_back(room);
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

// A session whose current place (kRoomA) is a newly accepted place with no
// investigation decision yet.
SessionState DistributionFixture() {
    SessionState state = BaseFixture();
    AddPlace(state, kRoomA, "Seuil de la roche");
    SetCurrentPlace(state, kRoomA);
    return state;
}

// ---------------------------------------------------------------------------
// Sites and proposals.
// ---------------------------------------------------------------------------

InvestigationSite GroundSite(const std::string& id, float x, float z) {
    InvestigationSite site;
    site.id = id;
    site.kind = "ground";
    site.placement.x = x;
    site.placement.y = 0.0f;
    site.placement.z = z;
    site.direction = kDirectionUnknown;
    return site;
}

InvestigationSite ApproachSite(const std::string& id, CardinalDirection direction,
                               float x, float z) {
    InvestigationSite site;
    site.id = id;
    site.kind = "approach";
    site.placement.x = x;
    site.placement.y = 0.0f;
    site.placement.z = z;
    site.direction = direction;
    return site;
}

std::vector<InvestigationSite> GroundSites() {
    std::vector<InvestigationSite> sites;
    sites.push_back(GroundSite("ground_01", 1.0f, 1.0f));
    sites.push_back(GroundSite("ground_02", 2.0f, 1.0f));
    sites.push_back(GroundSite("ground_03", 3.0f, 1.0f));
    return sites;
}

std::vector<InvestigationSite> StandardSites() {
    std::vector<InvestigationSite> sites = GroundSites();
    sites.push_back(ApproachSite("approach_north", kDirectionNorth, 0.0f, 0.0f));
    return sites;
}

struct ProposalEntry {
    std::string opportunity_id, site_id;
};

std::string ProposalText(int revision, const std::vector<ProposalEntry>& placements) {
    json document;
    document["revision"] = revision;
    json list = json::array();
    for (const ProposalEntry& entry : placements) {
        json item;
        item["opportunity_id"] = entry.opportunity_id;
        item["site_id"] = entry.site_id;
        list.push_back(item);
    }
    document["placements"] = list;
    return document.dump();
}

// ---------------------------------------------------------------------------
// API drivers.
// ---------------------------------------------------------------------------

bool PrepareDistribution(const SessionState& source, const std::string& place_id,
                         const std::vector<InvestigationSite>& sites,
                         const std::string& proposal, SessionState* candidate,
                         std::string* error) {
    *candidate = source;
    return PrepareInvestigationDistribution(
        source, place_id, sites, proposal, candidate, error);
}

SessionState PrepareRequiredDistribution(const SessionState& source,
                                         const std::string& place_id,
                                         const std::vector<InvestigationSite>& sites,
                                         const std::string& proposal) {
    SessionState candidate;
    std::string error;
    REQUIRE_DISTRIBUTION(PrepareDistribution(
        source, place_id, sites, proposal, &candidate, &error));
    return candidate;
}

SessionState ObserveRequired(const SessionState& source, const std::string& id,
                             int revision) {
    SessionState candidate = source;
    std::string error;
    REQUIRE_DISTRIBUTION(PrepareInvestigationObservation(
        source, id, revision, &candidate, &error));
    return candidate;
}

SessionState TakeRequired(const SessionState& source, const std::string& object_id) {
    ObjectProposal proposal;
    proposal.action = "take";
    proposal.origin = "explicit";
    proposal.object_id = object_id;
    proposal.verdict = "allow";
    proposal.revision = source.objects.revision;
    SessionState candidate;
    std::string reason;
    REQUIRE_DISTRIBUTION(PrepareObjectTransfer(
        source, proposal, nullptr, &candidate, &reason));
    return candidate;
}

SessionState SessionRoundtrip(const SessionState& source, std::string* text = nullptr) {
    std::string serialized;
    REQUIRE_DISTRIBUTION(SerializeSessionStateToJsonString(source, &serialized));
    REQUIRE_DISTRIBUTION(!serialized.empty());
    if (text != nullptr) *text = serialized;
    SessionState parsed;
    char error[512] = {};
    REQUIRE_DISTRIBUTION(ParseSessionStateFromJson(
        serialized.c_str(), &parsed, error, sizeof(error)));
    return parsed;
}

// ---------------------------------------------------------------------------
// Inspection helpers.
// ---------------------------------------------------------------------------

bool ContainsString(const std::vector<std::string>& values, const std::string& value) {
    for (const std::string& entry : values) {
        if (entry == value) return true;
    }
    return false;
}

bool UniqueStrings(const std::vector<std::string>& values) {
    for (std::size_t i = 0; i < values.size(); ++i) {
        for (std::size_t j = i + 1; j < values.size(); ++j) {
            if (values[i] == values[j]) return false;
        }
    }
    return true;
}

// Set equality; the catalogue order is never imposed.
bool SameStringSet(const std::vector<std::string>& left,
                   const std::vector<std::string>& right) {
    if (left.size() != right.size()) return false;
    if (!UniqueStrings(left) || !UniqueStrings(right)) return false;
    for (const std::string& value : left) {
        if (!ContainsString(right, value)) return false;
    }
    return true;
}

const InvestigationDecision* FindDecision(const InvestigationState& investigation,
                                          const std::string& place_id) {
    for (const InvestigationDecision& decision : investigation.decisions) {
        if (decision.place_id == place_id) return &decision;
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

std::string OpportunityObject(const SessionState& state, const std::string& id) {
    const InvestigationOpportunity* opportunity = FindOpportunity(state.investigation, id);
    if (opportunity == nullptr) return std::string();
    return opportunity->object_id;
}

int FindBarrier(const SessionState& state, const std::string& place_id,
                CardinalDirection direction) {
    for (std::size_t i = 0; i < state.invisible_barriers.size(); ++i) {
        if (state.invisible_barriers[i].place_id == place_id &&
            state.invisible_barriers[i].direction == direction) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

// The canonical threshold place already accepted by the session.
std::string PreservedPlaceId(const SessionState& state) {
    for (const InvestigationDecision& decision : state.investigation.decisions) {
        if (decision.origin == kLegacyOrigin && IsCanonicalPlaceId(decision.place_id)) {
            return decision.place_id;
        }
    }
    return std::string();
}

bool DepositHas(const SessionState& state, const std::string& place_id,
                const std::string& object_id, float x, float z) {
    const auto found = state.objects.deposits.find(place_id);
    if (found == state.objects.deposits.end()) return false;
    for (const ObjectPlacement& placement : found->second) {
        if (placement.object_id != object_id) continue;
        const float dx = placement.x - x;
        const float dz = placement.z - z;
        if (dx * dx + dz * dz <= 0.0025f) return true;  // 5 cm tolerance
    }
    return false;
}

std::string DecisionDigest(const InvestigationState& investigation) {
    std::string digest;
    for (const InvestigationDecision& decision : investigation.decisions) {
        digest += decision.place_id;
        digest += "|";
        digest += decision.origin;
        digest += "|";
        digest += std::to_string(decision.committed_turn);
        digest += "|";
        for (const std::string& id : decision.opportunity_ids) {
            digest += id;
            digest += ",";
        }
        digest += "\n";
    }
    return digest;
}

struct Snapshot {
    std::string investigation;
    std::string objects;
    std::string decisions;
    std::vector<std::string> inventory;
    int revision = 0;
    std::size_t decision_count = 0;
    std::size_t opportunity_count = 0;
    std::size_t evidence_count = 0;
    std::size_t barrier_count = 0;

    explicit Snapshot(const SessionState& state)
        : investigation(SerializeInvestigation(state.investigation))
        , objects(SerializeObjectState(state.objects))
        , decisions(DecisionDigest(state.investigation))
        , inventory(state.hard_state.inventory_items)
        , revision(state.investigation.revision)
        , decision_count(state.investigation.decisions.size())
        , opportunity_count(state.investigation.opportunities.size())
        , evidence_count(state.investigation.evidence.size())
        , barrier_count(state.invisible_barriers.size())
    {
    }
};

void RequireUnchanged(const SessionState& state, const Snapshot& before) {
    REQUIRE_DISTRIBUTION(SerializeInvestigation(state.investigation) == before.investigation);
    REQUIRE_DISTRIBUTION(SerializeObjectState(state.objects) == before.objects);
    REQUIRE_DISTRIBUTION(state.hard_state.inventory_items == before.inventory);
    REQUIRE_DISTRIBUTION(state.investigation.revision == before.revision);
    REQUIRE_DISTRIBUTION(state.investigation.decisions.size() == before.decision_count);
    REQUIRE_DISTRIBUTION(DecisionDigest(state.investigation) == before.decisions);
    REQUIRE_DISTRIBUTION(state.investigation.opportunities.size() == before.opportunity_count);
    REQUIRE_DISTRIBUTION(state.investigation.evidence.size() == before.evidence_count);
    REQUIRE_DISTRIBUTION(state.invisible_barriers.size() == before.barrier_count);
}

void ExpectDistributionRefused(const SessionState& source, const std::string& place_id,
                               const std::vector<InvestigationSite>& sites,
                               const std::string& proposal) {
    const Snapshot source_before(source);
    SessionState candidate = source;
    const Snapshot candidate_before(candidate);
    std::string error;
    const bool accepted = PrepareInvestigationDistribution(
        source, place_id, sites, proposal, &candidate, &error);
    REQUIRE_DISTRIBUTION(!accepted);
    REQUIRE_DISTRIBUTION(!error.empty());
    RequireUnchanged(candidate, candidate_before);
    RequireUnchanged(source, source_before);
}

// ---------------------------------------------------------------------------
// 1. Accepted distributions follow the proposal exactly.
// ---------------------------------------------------------------------------

void TestDistributionFollowsProposal() {
    const SessionState base = DistributionFixture();

    REQUIRE_DISTRIBUTION(base.investigation.schema_version == 2);
    REQUIRE_DISTRIBUTION(base.investigation.policy == kLlmPolicy);
    REQUIRE_DISTRIBUTION(base.investigation.evidence.empty());
    REQUIRE_DISTRIBUTION(FindDecision(base.investigation, kRoomA) == nullptr);

    const std::string preserved_place = PreservedPlaceId(base);
    REQUIRE_DISTRIBUTION(!preserved_place.empty());
    const InvestigationDecision* preserved = FindDecision(base.investigation, preserved_place);
    REQUIRE_DISTRIBUTION(preserved != nullptr);
    REQUIRE_DISTRIBUTION(preserved->origin == kLegacyOrigin);
    REQUIRE_DISTRIBUTION(preserved->opportunity_ids.empty());
    REQUIRE_DISTRIBUTION(base.investigation.decisions.size() == 1);

    const std::vector<std::string> catalogue = {kCrystal, kObstruction, kEquipment, kSurvey};
    const std::vector<std::string> available_before =
        AvailableInvestigationOpportunities(base);
    REQUIRE_DISTRIBUTION(SameStringSet(available_before, catalogue));

    const int revision = base.investigation.revision;
    const int turn = base.hard_state.turn_number;

    // Zero placements: an empty decision is accepted and recorded.
    {
        SessionState candidate;
        std::string error;
        REQUIRE_DISTRIBUTION(PrepareDistribution(
            base, kRoomA, StandardSites(), ProposalText(revision, {}),
            &candidate, &error));
        const InvestigationDecision* decision = FindDecision(candidate.investigation, kRoomA);
        REQUIRE_DISTRIBUTION(decision != nullptr);
        REQUIRE_DISTRIBUTION(decision->origin == kLlmOrigin);
        REQUIRE_DISTRIBUTION(decision->opportunity_ids.empty());
        REQUIRE_DISTRIBUTION(decision->committed_turn == turn);
        REQUIRE_DISTRIBUTION(candidate.investigation.opportunities.empty());
        REQUIRE_DISTRIBUTION(candidate.investigation.evidence.empty());
        REQUIRE_DISTRIBUTION(candidate.objects.instances.size() ==
                             base.objects.instances.size());
        REQUIRE_DISTRIBUTION(candidate.invisible_barriers.size() ==
                             base.invisible_barriers.size());
        REQUIRE_DISTRIBUTION(candidate.investigation.revision > revision);
        REQUIRE_DISTRIBUTION(SameStringSet(
            AvailableInvestigationOpportunities(candidate), catalogue));
    }

    // The survey alone, on the first ground site.
    {
        SessionState candidate;
        std::string error;
        REQUIRE_DISTRIBUTION(PrepareDistribution(
            base, kRoomA, StandardSites(),
            ProposalText(revision, {{kSurvey, "ground_01"}}), &candidate, &error));
        const InvestigationDecision* decision = FindDecision(candidate.investigation, kRoomA);
        REQUIRE_DISTRIBUTION(decision != nullptr);
        REQUIRE_DISTRIBUTION(decision->origin == kLlmOrigin);
        REQUIRE_DISTRIBUTION(decision->opportunity_ids.size() == 1);
        REQUIRE_DISTRIBUTION(decision->opportunity_ids[0] == kSurvey);
        REQUIRE_DISTRIBUTION(candidate.investigation.opportunities.size() == 1);
        const InvestigationOpportunity* opportunity =
            FindOpportunity(candidate.investigation, kSurvey);
        REQUIRE_DISTRIBUTION(opportunity != nullptr);
        REQUIRE_DISTRIBUTION(opportunity->place_id == kRoomA);
        REQUIRE_DISTRIBUTION(!opportunity->object_id.empty());
        REQUIRE_DISTRIBUTION(candidate.objects.instances.count(opportunity->object_id) == 1);
        REQUIRE_DISTRIBUTION(
            candidate.objects.instances.at(opportunity->object_id).origin_place == kRoomA);
        REQUIRE_DISTRIBUTION(DepositHas(candidate, kRoomA, opportunity->object_id, 1.0f, 1.0f));
        REQUIRE_DISTRIBUTION(candidate.investigation.evidence.empty());
        REQUIRE_DISTRIBUTION(candidate.invisible_barriers.size() ==
                             base.invisible_barriers.size());
        REQUIRE_DISTRIBUTION(candidate.investigation.revision > revision);
        const std::vector<std::string> remaining =
            AvailableInvestigationOpportunities(candidate);
        REQUIRE_DISTRIBUTION(remaining.size() == 3);
        REQUIRE_DISTRIBUTION(!ContainsString(remaining, kSurvey));
    }

    // Crystal, tool and survey together, each on its proposed site.
    {
        SessionState candidate = PrepareRequiredDistribution(
            base, kRoomA, StandardSites(),
            ProposalText(revision, {{kSurvey, "ground_01"},
                                    {kEquipment, "ground_02"},
                                    {kCrystal, "ground_03"}}));
        const InvestigationDecision* decision = FindDecision(candidate.investigation, kRoomA);
        REQUIRE_DISTRIBUTION(decision != nullptr);
        REQUIRE_DISTRIBUTION(decision->origin == kLlmOrigin);
        REQUIRE_DISTRIBUTION(SameStringSet(decision->opportunity_ids,
                                           {kSurvey, kEquipment, kCrystal}));
        REQUIRE_DISTRIBUTION(candidate.investigation.opportunities.size() == 3);
        for (const char* id : {kSurvey, kEquipment, kCrystal}) {
            REQUIRE_DISTRIBUTION(FindOpportunity(candidate.investigation, id) != nullptr);
            const std::string object_id = OpportunityObject(candidate, id);
            REQUIRE_DISTRIBUTION(!object_id.empty());
            REQUIRE_DISTRIBUTION(candidate.objects.instances.count(object_id) == 1);
        }
        REQUIRE_DISTRIBUTION(DepositHas(candidate, kRoomA,
                                        OpportunityObject(candidate, kSurvey), 1.0f, 1.0f));
        REQUIRE_DISTRIBUTION(DepositHas(candidate, kRoomA,
                                        OpportunityObject(candidate, kEquipment), 2.0f, 1.0f));
        REQUIRE_DISTRIBUTION(DepositHas(candidate, kRoomA,
                                        OpportunityObject(candidate, kCrystal), 3.0f, 1.0f));
        REQUIRE_DISTRIBUTION(candidate.investigation.evidence.empty());
        REQUIRE_DISTRIBUTION(FindEvidence(candidate.investigation, kSurvey) == nullptr);
        REQUIRE_DISTRIBUTION(candidate.invisible_barriers.size() ==
                             base.invisible_barriers.size());
        REQUIRE_DISTRIBUTION(candidate.investigation.revision > revision);
        const std::vector<std::string> remaining =
            AvailableInvestigationOpportunities(candidate);
        REQUIRE_DISTRIBUTION(remaining.size() == 1);
        REQUIRE_DISTRIBUTION(ContainsString(remaining, kObstruction));
    }
}

// ---------------------------------------------------------------------------
// 2. Refusals are atomic: source and candidate stay untouched.
// ---------------------------------------------------------------------------

void TestDistributionRefusals() {
    const SessionState state = DistributionFixture();
    const int revision = state.investigation.revision;
    const std::vector<InvestigationSite> sites = StandardSites();
    const std::string valid = ProposalText(revision, {{kSurvey, "ground_01"}});

    // Stale, future and fractional revisions.
    ExpectDistributionRefused(state, kRoomA, sites,
                              ProposalText(revision - 1, {{kSurvey, "ground_01"}}));
    ExpectDistributionRefused(state, kRoomA, sites,
                              ProposalText(revision + 1, {{kSurvey, "ground_01"}}));
    {
        json document = json::parse(valid);
        document["revision"] = 0.5;
        ExpectDistributionRefused(state, kRoomA, sites, document.dump());
    }

    // Missing and extra fields.
    {
        json document = json::parse(valid);
        document.erase("revision");
        ExpectDistributionRefused(state, kRoomA, sites, document.dump());
    }
    {
        json document = json::parse(valid);
        document.erase("placements");
        ExpectDistributionRefused(state, kRoomA, sites, document.dump());
    }
    {
        json document = json::parse(valid);
        document["extra"] = 1;
        ExpectDistributionRefused(state, kRoomA, sites, document.dump());
    }
    {
        json document = json::parse(valid);
        document["placements"][0].erase("site_id");
        ExpectDistributionRefused(state, kRoomA, sites, document.dump());
    }
    {
        json document = json::parse(valid);
        document["placements"][0].erase("opportunity_id");
        ExpectDistributionRefused(state, kRoomA, sites, document.dump());
    }
    {
        json document = json::parse(valid);
        document["placements"][0]["note"] = "x";
        ExpectDistributionRefused(state, kRoomA, sites, document.dump());
    }

    // Unknown identity and invented site.
    ExpectDistributionRefused(state, kRoomA, sites,
                              ProposalText(revision, {{"unknown_clue", "ground_01"}}));
    ExpectDistributionRefused(state, kRoomA, sites,
                              ProposalText(revision, {{kSurvey, "ground_99"}}));

    // Site kinds: the obstruction needs an approach, the others a ground site.
    ExpectDistributionRefused(state, kRoomA, sites,
                              ProposalText(revision, {{kObstruction, "ground_01"}}));
    ExpectDistributionRefused(state, kRoomA, sites,
                              ProposalText(revision, {{kCrystal, "approach_north"}}));

    // Duplicate identities and duplicate sites inside one proposal.
    ExpectDistributionRefused(state, kRoomA, sites,
                              ProposalText(revision, {{kSurvey, "ground_01"},
                                                      {kSurvey, "ground_02"}}));
    ExpectDistributionRefused(state, kRoomA, sites,
                              ProposalText(revision, {{kSurvey, "ground_01"},
                                                      {kCrystal, "ground_01"}}));

    // Non-finite site coordinates.
    {
        std::vector<InvestigationSite> nan_sites = sites;
        nan_sites[0].placement.x = std::numeric_limits<float>::quiet_NaN();
        ExpectDistributionRefused(state, kRoomA, nan_sites,
                                  ProposalText(revision, {{kSurvey, "ground_01"}}));
    }
    {
        std::vector<InvestigationSite> infinite_sites = sites;
        infinite_sites[1].placement.z = std::numeric_limits<float>::infinity();
        ExpectDistributionRefused(state, kRoomA, infinite_sites,
                                  ProposalText(revision, {{kSurvey, "ground_02"}}));
    }

    // Two distinct sites closer than the minimum spacing (0.6 m).
    {
        std::vector<InvestigationSite> overlapping = GroundSites();
        overlapping.push_back(GroundSite("ground_04", 1.2f, 1.0f));
        ExpectDistributionRefused(state, kRoomA, overlapping,
                                  ProposalText(revision, {{kSurvey, "ground_01"}, {kCrystal, "ground_04"}}));
    }

    // Atomicity: a first valid placement followed by an invalid one.
    ExpectDistributionRefused(state, kRoomA, sites,
                              ProposalText(revision, {{kSurvey, "ground_01"},
                                                      {kCrystal, "ground_99"}}));
    ExpectDistributionRefused(state, kRoomA, sites,
                              ProposalText(revision, {{kSurvey, "ground_01"},
                                                      {kObstruction, "ground_02"}}));
    ExpectDistributionRefused(state, kRoomA, sites,
                              ProposalText(revision, {{kSurvey, "ground_01"},
                                                      {kSurvey, "ground_03"}}));

    // A candidate that differs from the source is left as provided.
    {
        const Snapshot source_before(state);
        SessionState candidate = state;
        candidate.hard_state.turn_number = 1234;
        const Snapshot candidate_before(candidate);
        std::string error;
        REQUIRE_DISTRIBUTION(!PrepareInvestigationDistribution(
            state, kRoomA, sites,
            ProposalText(revision, {{kSurvey, "ground_01"},
                                    {kCrystal, "approach_north"}}),
            &candidate, &error));
        REQUIRE_DISTRIBUTION(!error.empty());
        REQUIRE_DISTRIBUTION(candidate.hard_state.turn_number == 1234);
        RequireUnchanged(candidate, candidate_before);
        RequireUnchanged(state, source_before);
    }

    // The accepted path still works after all refusals.
    SessionState accepted;
    std::string accepted_error;
    REQUIRE_DISTRIBUTION(PrepareDistribution(
        state, kRoomA, sites, valid, &accepted, &accepted_error));
    REQUIRE_DISTRIBUTION(FindDecision(accepted.investigation, kRoomA) != nullptr);
    REQUIRE_DISTRIBUTION(accepted.investigation.evidence.empty());
}

// ---------------------------------------------------------------------------
// 3. Empty and grouped decisions are immutable and persistent.
// ---------------------------------------------------------------------------

void TestPersistentDecisions() {
    const SessionState base = DistributionFixture();
    const int revision = base.investigation.revision;

    // An empty decision is durable.
    SessionState empty = PrepareRequiredDistribution(
        base, kRoomA, StandardSites(), ProposalText(revision, {}));
    const InvestigationDecision* empty_decision = FindDecision(empty.investigation, kRoomA);
    REQUIRE_DISTRIBUTION(empty_decision != nullptr);
    REQUIRE_DISTRIBUTION(empty_decision->origin == kLlmOrigin);
    REQUIRE_DISTRIBUTION(empty_decision->opportunity_ids.empty());
    REQUIRE_DISTRIBUTION(empty.investigation.opportunities.empty());
    REQUIRE_DISTRIBUTION(AvailableInvestigationOpportunities(empty).size() == 4);

    ExpectDistributionRefused(empty, kRoomA, StandardSites(),
                              ProposalText(empty.investigation.revision, {}));
    ExpectDistributionRefused(empty, kRoomA, StandardSites(),
                              ProposalText(empty.investigation.revision,
                                           {{kSurvey, "ground_01"}}));

    // Session roundtrip keeps the empty decision.
    SessionState empty_round = SessionRoundtrip(empty);
    const InvestigationDecision* round_decision =
        FindDecision(empty_round.investigation, kRoomA);
    REQUIRE_DISTRIBUTION(round_decision != nullptr);
    REQUIRE_DISTRIBUTION(round_decision->origin == kLlmOrigin);
    REQUIRE_DISTRIBUTION(round_decision->opportunity_ids.empty());
    REQUIRE_DISTRIBUTION(AvailableInvestigationOpportunities(empty_round).size() == 4);
    ExpectDistributionRefused(empty_round, kRoomA, StandardSites(),
                              ProposalText(empty_round.investigation.revision, {}));

    // A group decision at another place: crystal on the ground, obstruction on
    // the approach.
    SessionState group_source = empty_round;
    AddPlace(group_source, kRoomB, "Repli ombrage");
    SetCurrentPlace(group_source, kRoomB);
    SessionState grouped = PrepareRequiredDistribution(
        group_source, kRoomB, StandardSites(),
        ProposalText(group_source.investigation.revision,
                     {{kCrystal, "ground_01"}, {kObstruction, "approach_north"}}));
    const InvestigationDecision* group_decision =
        FindDecision(grouped.investigation, kRoomB);
    REQUIRE_DISTRIBUTION(group_decision != nullptr);
    REQUIRE_DISTRIBUTION(group_decision->origin == kLlmOrigin);
    REQUIRE_DISTRIBUTION(SameStringSet(group_decision->opportunity_ids,
                                       {kCrystal, kObstruction}));
    REQUIRE_DISTRIBUTION(FindOpportunity(grouped.investigation, kCrystal) != nullptr);
    REQUIRE_DISTRIBUTION(FindOpportunity(grouped.investigation, kObstruction) != nullptr);
    const int barrier_index = FindBarrier(grouped, kRoomB, kDirectionNorth);
    REQUIRE_DISTRIBUTION(barrier_index >= 0);
    REQUIRE_DISTRIBUTION(
        grouped.invisible_barriers[static_cast<std::size_t>(barrier_index)].discovered == false);
    REQUIRE_DISTRIBUTION(grouped.investigation.evidence.empty());
    ExpectDistributionRefused(grouped, kRoomB, StandardSites(),
                              ProposalText(grouped.investigation.revision,
                                           {{kEquipment, "ground_02"}}));

    // Session roundtrip keeps the grouped decision and its barrier.
    SessionState grouped_round = SessionRoundtrip(grouped);
    const InvestigationDecision* kept = FindDecision(grouped_round.investigation, kRoomB);
    REQUIRE_DISTRIBUTION(kept != nullptr);
    REQUIRE_DISTRIBUTION(SameStringSet(kept->opportunity_ids, {kCrystal, kObstruction}));
    REQUIRE_DISTRIBUTION(FindBarrier(grouped_round, kRoomB, kDirectionNorth) >= 0);
    REQUIRE_DISTRIBUTION(AvailableInvestigationOpportunities(grouped_round).size() == 2);

    // Publish the survey at a third place.
    AddPlace(grouped_round, kRoomC, "Corniche basse");
    SetCurrentPlace(grouped_round, kRoomC);
    SessionState surveyed = PrepareRequiredDistribution(
        grouped_round, kRoomC, StandardSites(),
        ProposalText(grouped_round.investigation.revision, {{kSurvey, "ground_03"}}));
    REQUIRE_DISTRIBUTION(FindDecision(surveyed.investigation, kRoomC) != nullptr);
    REQUIRE_DISTRIBUTION(AvailableInvestigationOpportunities(surveyed).size() == 1);

    // A further place cannot republish an identity already placed.
    AddPlace(surveyed, kRoomD, "Faille sèche");
    SetCurrentPlace(surveyed, kRoomD);
    ExpectDistributionRefused(surveyed, kRoomD, StandardSites(),
                              ProposalText(surveyed.investigation.revision,
                                           {{kSurvey, "ground_01"}}));
    ExpectDistributionRefused(surveyed, kRoomD, StandardSites(),
                              ProposalText(surveyed.investigation.revision,
                                           {{kCrystal, "ground_02"}}));
    ExpectDistributionRefused(surveyed, kRoomD, StandardSites(),
                              ProposalText(surveyed.investigation.revision,
                                           {{kObstruction, "approach_north"}}));
    REQUIRE_DISTRIBUTION(FindDecision(surveyed.investigation, kRoomD) == nullptr);
}

// ---------------------------------------------------------------------------
// 4. Migration preserves accepted worlds and existing opportunities.
// ---------------------------------------------------------------------------

void TestMigrationPreservesDecisions() {
    SessionState state = DistributionFixture();
    state = PrepareRequiredDistribution(
        state, kRoomA, StandardSites(),
        ProposalText(state.investigation.revision, {{kEquipment, "ground_01"}}));
    REQUIRE_DISTRIBUTION(state.investigation.evidence.empty());

    // Old export without any investigation block.
    {
        std::string text;
        SessionState serialized = SessionRoundtrip(state, &text);
        REQUIRE_DISTRIBUTION(serialized.investigation.decisions.size() == 2);

        json document = json::parse(text);
        REQUIRE_DISTRIBUTION(document.contains("investigation"));
        document.erase("investigation");

        SessionState migrated;
        char error[512] = {};
        REQUIRE_DISTRIBUTION(ParseSessionStateFromJson(
            document.dump().c_str(), &migrated, error, sizeof(error)));
        EnsureInvestigation(&migrated);
        REQUIRE_DISTRIBUTION(migrated.current_place_id == state.current_place_id);
        REQUIRE_DISTRIBUTION(migrated.generated_rooms.size() ==
                             state.generated_rooms.size());
        REQUIRE_DISTRIBUTION(migrated.objects.instances.size() ==
                             state.objects.instances.size());
        REQUIRE_DISTRIBUTION(migrated.investigation.schema_version == 2);
        REQUIRE_DISTRIBUTION(migrated.investigation.policy == kLlmPolicy);
        REQUIRE_DISTRIBUTION(migrated.investigation.evidence.empty());
        REQUIRE_DISTRIBUTION(!migrated.investigation.decisions.empty());
        for (const InvestigationDecision& decision : migrated.investigation.decisions) {
            REQUIRE_DISTRIBUTION(decision.origin == kLegacyOrigin);
        }
        const std::string preserved_place = PreservedPlaceId(migrated);
        REQUIRE_DISTRIBUTION(!preserved_place.empty());
        const InvestigationDecision* preserved =
            FindDecision(migrated.investigation, preserved_place);
        REQUIRE_DISTRIBUTION(preserved != nullptr);
        REQUIRE_DISTRIBUTION(preserved->opportunity_ids.empty());

        const std::string ensured = SerializeInvestigation(migrated.investigation);
        EnsureInvestigation(&migrated);
        REQUIRE_DISTRIBUTION(SerializeInvestigation(migrated.investigation) == ensured);
    }

    // Version 1 export: decisions absent, previous policy and schema version.
    {
        std::string text;
        SessionRoundtrip(state, &text);
        json document = json::parse(text);
        REQUIRE_DISTRIBUTION(document.contains("investigation"));
        json& investigation = document["investigation"];
        investigation.erase("decisions");
        investigation["schema_version"] = 1;
        investigation["policy"] = "eryx-clues-v1";

        SessionState migrated;
        char error[512] = {};
        REQUIRE_DISTRIBUTION(ParseSessionStateFromJson(
            document.dump().c_str(), &migrated, error, sizeof(error)));
        EnsureInvestigation(&migrated);
        REQUIRE_DISTRIBUTION(migrated.investigation.schema_version == 2);
        REQUIRE_DISTRIBUTION(migrated.investigation.policy == kLlmPolicy);
        REQUIRE_DISTRIBUTION(migrated.investigation.evidence.empty());

        const InvestigationOpportunity* kept =
            FindOpportunity(migrated.investigation, kEquipment);
        REQUIRE_DISTRIBUTION(kept != nullptr);
        REQUIRE_DISTRIBUTION(kept->place_id == kRoomA);

        const InvestigationDecision* decision =
            FindDecision(migrated.investigation, kRoomA);
        REQUIRE_DISTRIBUTION(decision != nullptr);
        REQUIRE_DISTRIBUTION(decision->origin == kLegacyOrigin);
        REQUIRE_DISTRIBUTION(ContainsString(decision->opportunity_ids, kEquipment));

        const std::string preserved_place = PreservedPlaceId(migrated);
        REQUIRE_DISTRIBUTION(!preserved_place.empty());
        const InvestigationDecision* preserved =
            FindDecision(migrated.investigation, preserved_place);
        REQUIRE_DISTRIBUTION(preserved != nullptr);
        REQUIRE_DISTRIBUTION(preserved->origin == kLegacyOrigin);

        // The existing opportunity is preserved, hence no longer available.
        const std::vector<std::string> available =
            AvailableInvestigationOpportunities(migrated);
        REQUIRE_DISTRIBUTION(!ContainsString(available, kEquipment));
        REQUIRE_DISTRIBUTION(available.size() == 3);

        std::string validation_error;
        REQUIRE_DISTRIBUTION(ValidateInvestigation(migrated, &validation_error));
    }
}

// ---------------------------------------------------------------------------
// 5. Spatial proximity never supports the "Vey stayed at the sample" idea.
// ---------------------------------------------------------------------------

void RequireNoSpatialSupport(const InvestigationState& investigation) {
    const InvestigationHypothesis* stayed =
        FindHypothesis(investigation, "vey_stayed_at_sample");
    REQUIRE_DISTRIBUTION(stayed != nullptr);
    REQUIRE_DISTRIBUTION(stayed->status == "open");
    REQUIRE_DISTRIBUTION(stayed->supporting_evidence.empty());
    REQUIRE_DISTRIBUTION(!ContainsString(stayed->contradicting_evidence, kSurvey));
}

void TestSpatialProximityHypothesis() {
    const SessionState base = DistributionFixture();
    const int revision = base.investigation.revision;

    // Same place: the crystal and the tool share the room.
    {
        SessionState state = PrepareRequiredDistribution(
            base, kRoomA, StandardSites(),
            ProposalText(revision, {{kCrystal, "ground_01"}, {kEquipment, "ground_02"}}));
        const std::string crystal_object = OpportunityObject(state, kCrystal);
        REQUIRE_DISTRIBUTION(!crystal_object.empty());
        REQUIRE_DISTRIBUTION(!OpportunityObject(state, kEquipment).empty());

        state = ObserveRequired(state, kCrystal, state.investigation.revision);
        state = ObserveRequired(state, kEquipment, state.investigation.revision);
        SessionState carried = TakeRequired(state, crystal_object);
        RecordInvestigationTransfer(state, "take", crystal_object, &carried);
        REQUIRE_DISTRIBUTION(ContainsString(carried.hard_state.inventory_items, crystal_object));
        REQUIRE_DISTRIBUTION(CountEvidence(carried.investigation, "crystal_source") == 1);
        REQUIRE_DISTRIBUTION(CountEvidence(carried.investigation, "crystal_acquired") == 1);
        REQUIRE_DISTRIBUTION(CountEvidence(carried.investigation, kEquipment) == 1);
        RequireNoSpatialSupport(carried.investigation);
    }

    // Different places: the crystal at A, the tool and the survey at B.
    {
        SessionState state = PrepareRequiredDistribution(
            base, kRoomA, StandardSites(), ProposalText(revision, {{kCrystal, "ground_01"}}));
        const std::string crystal_object = OpportunityObject(state, kCrystal);
        REQUIRE_DISTRIBUTION(!crystal_object.empty());
        state = ObserveRequired(state, kCrystal, state.investigation.revision);
        SessionState carried = TakeRequired(state, crystal_object);
        RecordInvestigationTransfer(state, "take", crystal_object, &carried);
        REQUIRE_DISTRIBUTION(CountEvidence(carried.investigation, "crystal_acquired") == 1);

        AddPlace(carried, kRoomB, "Repli ombrage");
        SetCurrentPlace(carried, kRoomB);
        SessionState split = PrepareRequiredDistribution(
            carried, kRoomB, StandardSites(),
            ProposalText(carried.investigation.revision,
                         {{kEquipment, "ground_02"}, {kSurvey, "ground_03"}}));
        split = ObserveRequired(split, kEquipment, split.investigation.revision);
        REQUIRE_DISTRIBUTION(CountEvidence(split.investigation, "crystal_acquired") == 1);
        REQUIRE_DISTRIBUTION(CountEvidence(split.investigation, "crystal_source") == 1);
        REQUIRE_DISTRIBUTION(CountEvidence(split.investigation, kEquipment) == 1);
        REQUIRE_DISTRIBUTION(CountEvidence(split.investigation, kSurvey) == 0);
        RequireNoSpatialSupport(split.investigation);

        // The survey then contradicts the idea without erasing prior evidence.
        split = ObserveRequired(split, kSurvey, split.investigation.revision);
        const InvestigationHypothesis* stayed =
            FindHypothesis(split.investigation, "vey_stayed_at_sample");
        REQUIRE_DISTRIBUTION(stayed != nullptr);
        REQUIRE_DISTRIBUTION(stayed->status == "contradicted");
        REQUIRE_DISTRIBUTION(ContainsString(stayed->contradicting_evidence, kSurvey));
        REQUIRE_DISTRIBUTION(stayed->supporting_evidence.empty());
        REQUIRE_DISTRIBUTION(CountEvidence(split.investigation, "crystal_source") == 1);
        REQUIRE_DISTRIBUTION(CountEvidence(split.investigation, "crystal_acquired") == 1);
        REQUIRE_DISTRIBUTION(CountEvidence(split.investigation, kEquipment) == 1);
        REQUIRE_DISTRIBUTION(CountEvidence(split.investigation, kSurvey) == 1);
    }
}

// ---------------------------------------------------------------------------
// 6. Decisions and availability outlive the recent-history window.
// ---------------------------------------------------------------------------

void TestLongSessionRetentionAndContexts() {
    const SessionState base = DistributionFixture();
    SessionState state = PrepareRequiredDistribution(
        base, kRoomA, StandardSites(),
        ProposalText(base.investigation.revision, {{kSurvey, "ground_01"}}));

    AddPlace(state, kRoomC, "Puits effondré");
    SetCurrentPlace(state, kRoomC);
    state = PrepareRequiredDistribution(
        state, kRoomC, StandardSites(),
        ProposalText(state.investigation.revision,
                     {{kCrystal, "ground_02"}, {kObstruction, "approach_north"}}));
    REQUIRE_DISTRIBUTION(state.investigation.evidence.empty());
    REQUIRE_DISTRIBUTION(FindBarrier(state, kRoomC, kDirectionNorth) >= 0);

    for (int turn = 1; turn <= 45; ++turn) {
        SessionTurnRecord record;
        record.turn_number = turn;
        record.location_id = kLocationUnknown;
        record.origin_place_id = kRoomA;
        record.place_id = kRoomC;
        record.player_command = "observer le puits";
        record.intent = "perceive";
        record.narration = "Le puits reste stable.";
        state.history.push_back(record);
        state.hard_state.turn_number = turn;
    }
    REQUIRE_DISTRIBUTION(state.history.size() == 45);

    SessionState roundtrip = SessionRoundtrip(state);
    REQUIRE_DISTRIBUTION(
        SerializeInvestigation(roundtrip.investigation) ==
        SerializeInvestigation(state.investigation));

    const InvestigationDecision* at_a = FindDecision(roundtrip.investigation, kRoomA);
    REQUIRE_DISTRIBUTION(at_a != nullptr);
    REQUIRE_DISTRIBUTION(at_a->origin == kLlmOrigin);
    REQUIRE_DISTRIBUTION(at_a->opportunity_ids.size() == 1);
    REQUIRE_DISTRIBUTION(at_a->opportunity_ids[0] == kSurvey);
    const InvestigationDecision* at_c = FindDecision(roundtrip.investigation, kRoomC);
    REQUIRE_DISTRIBUTION(at_c != nullptr);
    REQUIRE_DISTRIBUTION(at_c->origin == kLlmOrigin);
    REQUIRE_DISTRIBUTION(SameStringSet(at_c->opportunity_ids, {kCrystal, kObstruction}));
    REQUIRE_DISTRIBUTION(FindBarrier(roundtrip, kRoomC, kDirectionNorth) >= 0);

    const std::vector<std::string> available =
        AvailableInvestigationOpportunities(roundtrip);
    REQUIRE_DISTRIBUTION(available.size() == 1);
    REQUIRE_DISTRIBUTION(ContainsString(available, kEquipment));

    // The authoring context keeps published decisions and remaining options.
    const std::vector<InvestigationSite> sites = StandardSites();
    const std::string authoring = InvestigationAuthoringContext(roundtrip, sites);
    REQUIRE_DISTRIBUTION(!authoring.empty());
    for (const char* published : {kSurvey, kCrystal, kObstruction}) {
        REQUIRE_DISTRIBUTION(authoring.find(published) != std::string::npos);
    }
    for (const std::string& id : available) {
        REQUIRE_DISTRIBUTION(authoring.find(id) != std::string::npos);
    }

    // The player-facing context never exposes the still-available options.
    const std::string player = InvestigationContext(roundtrip);
    for (const std::string& id : available) {
        REQUIRE_DISTRIBUTION(player.find(id) == std::string::npos);
    }
}

// Codex regression: a retained literary mention of a relocated registered clue
// must resolve to that identity, including imports predating mention bindings.
void TestRelocatedMentionCannotDuplicate() {
    auto state=DistributionFixture();
    state=PrepareRequiredDistribution(state,kRoomA,StandardSites(),
        ProposalText(state.investigation.revision,{{kCrystal,"ground_01"}}));
    const std::string quote="A crystalline specimen rests on the ground.";
    state.reference_descriptions[kRoomA]=quote;
    const auto id=OpportunityObject(state,kCrystal);
    auto carried=TakeRequired(state,id);
    REQUIRE_DISTRIBUTION(!carried.objects.mentions.empty());
    AddPlace(carried,kRoomB,"Other floor");SetCurrentPlace(carried,kRoomB);
    carried=PrepareRequiredDistribution(carried,kRoomB,StandardSites(),ProposalText(carried.investigation.revision,{}));
    ObjectProposal drop;drop.action="drop";drop.origin="explicit";drop.object_id=id;drop.verdict="allow";drop.revision=carried.objects.revision;
    ObjectPlacement placement;placement.x=2;placement.z=1;
    SessionState deposited;std::string error;
    REQUIRE_DISTRIBUTION(PrepareObjectTransfer(carried,drop,&placement,&deposited,&error));
    deposited=SessionRoundtrip(deposited);SetCurrentPlace(deposited,kRoomA);
    for(bool imported:{false,true}) {
        auto before=deposited;if(imported)before.objects.mentions.clear();
        ObjectProposal take;take.action="take";take.origin="implicit";take.quote=quote;take.label="crystalline specimen";
        take.verdict="allow";take.revision=before.objects.revision;
        auto candidate=before;const Snapshot snapshot(before);
        REQUIRE_DISTRIBUTION(!PrepareObjectTransfer(before,take,nullptr,&candidate,&error));
        REQUIRE_DISTRIBUTION(error=="not_here");RequireUnchanged(before,snapshot);RequireUnchanged(candidate,snapshot);
    }
}
}  // namespace

void RunInvestigationDistributionTests() {
    TestRelocatedMentionCannotDuplicate();
    TestDistributionFollowsProposal();
    TestDistributionRefusals();
    TestPersistentDecisions();
    TestMigrationPreservesDecisions();
    TestSpatialProximityHypothesis();
    TestLongSessionRetentionAndContexts();
}
