#pragma once
#include "game_state.h"

namespace liminal {
// Missing legacy investigation data starts empty; prose never becomes evidence.
void EnsureInvestigation(SessionState* state);
bool ValidateInvestigation(const SessionState& state, std::string* error);
std::string SerializeInvestigation(const InvestigationState& state);
bool ParseInvestigation(const std::string& text, InvestigationState* state, std::string* error);
void PreserveLegacyInvestigationPlaces(SessionState* state);
bool InvestigationDecisionsPreserved(const SessionState& before, const SessionState& after);
struct InvestigationSite {
    std::string id, kind;
    ObjectPlacement placement;
    CardinalDirection direction = kDirectionUnknown;
};
std::vector<std::string> AvailableInvestigationOpportunities(const SessionState& state);
std::string InvestigationAuthoringContext(const SessionState& state, const std::vector<InvestigationSite>& sites);
std::string InvestigationPlacementSchema(const SessionState& state, const std::vector<InvestigationSite>& sites);
bool PrepareInvestigationDistribution(const SessionState& state, const std::string& new_place_id,
    const std::vector<InvestigationSite>& sites, const std::string& proposal_json,
    SessionState* candidate, std::string* error);
// Caller supplies a newly accepted place and a validated mobile placement or
// clear unlinked direction. This is candidate preparation, never publication.
bool PrepareInvestigationOpportunity(const SessionState& state, const std::string& id,
    const std::string& place, CardinalDirection direction, const ObjectPlacement* placement,
    SessionState* candidate, std::string* error);
bool PrepareInvestigationObservation(const SessionState& state, const std::string& id,
    int revision, SessionState* candidate, std::string* error);
// Facts follow verified engine transitions, never narrative declarations.
void RecordInvestigationTransfer(const SessionState& before, const std::string& action,
    const std::string& object_id, SessionState* candidate);
void RecordInvestigationContact(const SessionState& before, CardinalDirection direction,
    SessionState* candidate);
std::string InvestigationContext(const SessionState& state);
std::string InvestigationJournal(const SessionState& state);
std::string InvestigationMandate(GameLanguage language);
std::string InvestigationHint(const std::string& id, GameLanguage language);
std::string InvestigationFact(const std::string& id, GameLanguage language);
std::string InvestigationHypothesisText(const std::string& id, GameLanguage language);
}
