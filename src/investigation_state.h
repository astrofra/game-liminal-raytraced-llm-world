#pragma once
#include <string>
#include <vector>

namespace liminal {
struct InvestigationThread {
    std::string id, status = "open";
    std::vector<std::string> evidence_ids, hypothesis_ids;
};
struct InvestigationEvidence {
    std::string id, kind, place_id, object_id, direction, source_event, fact;
    std::vector<std::string> thread_ids;
    int turn = 0;
};
struct InvestigationHypothesis {
    std::string id, proposition, status = "open";
    std::vector<std::string> supporting_evidence, contradicting_evidence;
};
struct InvestigationOpportunity {
    std::string id, kind, place_id, object_id, direction, status = "published";
    int turn = 0;
};
struct InvestigationDecision {
    std::string place_id, origin;
    std::vector<std::string> opportunity_ids;
    int committed_turn = 0;
};
struct InvestigationState {
    int schema_version = 2, revision = 0;
    std::string policy = "eryx-clues-llm-v2";
    std::vector<InvestigationThread> threads;
    std::vector<InvestigationEvidence> evidence;
    std::vector<InvestigationHypothesis> hypotheses;
    std::vector<InvestigationOpportunity> opportunities;
    std::vector<InvestigationDecision> decisions;
};
}
