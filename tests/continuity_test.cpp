#include "continuity.h"
#include "nlohmann/json.hpp"
#include <cstdio>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>
using namespace liminal;
using json = nlohmann::json;
#define CHECK(c) do { if (!(c)) throw std::runtime_error(std::string("Check failed at line ") + std::to_string(__LINE__) + ": " + #c); } while (false)
// FAUX compteur deterministe (octets des messages + 8 par message) : simule une capacite de contexte ;
// il ne represente ni Gemma ni un tokenizer reel.
static int TokenBytes(const std::vector<LlmPromptMessage>& messages) {
    int total = 0;
    for (const auto& message : messages) total += 8 + static_cast<int>(message.role.size() + message.content.size());
    return total;
}
static std::string PromptAll(const std::vector<LlmPromptMessage>& messages) {
    std::string text; for (const auto& message : messages) text += message.content; return text;
}
static bool PromptContains(const std::vector<LlmPromptMessage>& messages, const std::string& needle) {
    for (const auto& message : messages) if (message.content.find(needle) != std::string::npos) return true;
    return false;
}
static bool ValidUtf8(const std::string& text) {
    for (size_t i = 0; i < text.size();) {
        const unsigned char lead = static_cast<unsigned char>(text[i]);
        const size_t extra = lead < 0x80 ? 0 : (lead >= 0xC2 && lead <= 0xDF) ? 1
            : (lead >= 0xE0 && lead <= 0xEF) ? 2 : (lead >= 0xF0 && lead <= 0xF4) ? 3 : 4;
        if (extra == 4) return false;
        if (extra > 0 && i + extra >= text.size()) return false;
        for (size_t offset = 1; offset <= extra; ++offset)
            if ((static_cast<unsigned char>(text[i + offset]) & 0xC0) != 0x80) return false;
        i += extra + 1;
    }
    return true;
}
static const char* kRepairMarker = "REPAIR-MARKER";
static const char* kDiagnostic =
    "Diagnostic de reparation: le tour precedent a produit un noyau incoherent. Verifie l'origine, "
    "la transition, l'inventaire et les sorties bloquees avant de repondre. Cette charge est obligatoire "
    "et ne doit jamais etre coupee ni retiree par la reduction du budget de continuite.";
// Builder de test : systeme fixe + contexte serialise, plus la charge de reparation (assistant et
// diagnostic) quand le noyau porte le marqueur ; le compteur mesure tous les messages du builder.
static std::vector<LlmPromptMessage> BuildPrompt(const ContinuityContextV1& context) {
    std::vector<LlmPromptMessage> messages;
    messages.push_back({"system", "Tu maintiens la continuite: noyau, evenements recents et notes subjectives."});
    messages.push_back({"user", SerializeContinuityContext(context)});
    if (context.core_json.find(kRepairMarker) != std::string::npos) {
        messages.push_back({"assistant", kDiagnostic});
        messages.push_back({"user", "Repare le contexte sans perdre inventaire, contraintes ni dernier resultat."});
    }
    return messages;
}
static ContinuityTokenCounter Counter() {
    return [](const std::vector<LlmPromptMessage>& messages, int* tokens, std::string* error) {
        if (!tokens) { if (error) *error = "sortie de comptage absente"; return false; }
        *tokens = TokenBytes(messages); return true;
    };
}
static ContinuityContextV1 CoreOnly(const ContinuityContextV1& context) {
    ContinuityContextV1 core = context; core.events_json.clear(); core.notes.clear(); return core;
}
static std::string CoreJson(bool repair = false,
    const std::string& origin_description = "Une verriere fendue laisse tomber une lumiere pale sur les dalles.") {
    json core;
    core["origin"] = {{"id", "atrium"}, {"title", repair ? std::string("Atrium REPAIR-MARKER") : std::string("Atrium de verre")},
        {"description", origin_description}};
    core["transition"] = {{"direction", "north"}, {"return_direction", "south"}, {"outcome", "entered"}};
    core["engine"] = {{"inventory", {"cle de cuivre", "lampe tempete"}}, {"blocked_exits", {"grille nord"}}};
    return core.dump();
}
static std::string EventJson(int index, const std::string& place, const std::string& command,
    const std::string& outcome, const std::string& description) {
    json event; event["id"] = "ev" + std::to_string(index); event["place_id"] = place; event["command"] = command;
    event["intent"] = (command == "read_inventory" || command == "inspect_inventory_item" || command == "read_field_notes") ? command : "explorer"; event["outcome"] = outcome; event["description"] = description; return event.dump();
}
static ContinuityContextV1 TwentyEvents(bool repair = false) {
    static const char* places[20] = {"atrium", "serre", "atrium", "cave", "couloir", "atrium", "serre", "couloir",
        "cave", "atrium", "serre", "couloir", "atrium", "serre", "couloir", "atrium", "couloir", "atrium",
        "couloir", "atrium"};
    ContinuityContextV1 context; context.core_json = CoreJson(repair);
    for (int i = 1; i <= 20; ++i) context.events_json.push_back(EventJson(i, places[i - 1],
        "aller_" + std::to_string(i), "moved", "Traversee " + std::to_string(i) + " sans obstacle."));
    return context;
}
static std::vector<std::string> RetainedIds(const json& metrics) {
    std::vector<std::string> ids;
    for (const auto& id : metrics["retained_event_ids"]) ids.push_back(id.get<std::string>());
    return ids;
}
static int CountPresentNotes(const std::vector<LlmPromptMessage>& messages, const std::vector<std::string>& notes) {
    int present = 0; for (const auto& note : notes) if (PromptContains(messages, note)) ++present; return present;
}
static void CheckMetricsKeys(const json& metrics) {
    for (const char* key : {"policy", "context_tokens", "output_reserved", "technical_margin", "memory_cap",
             "memory_available", "mandatory_prompt_tokens", "prompt_tokens_before", "prompt_tokens_after", "memory_tokens",
             "retained_event_ids", "omitted_events", "omitted_notes", "shortened_fields", "success", "error"})
        CHECK(metrics.find(key) != metrics.end());
    CHECK(metrics["policy"].is_string() && !metrics["policy"].get<std::string>().empty());
    CHECK(metrics["retained_event_ids"].is_array() && metrics["shortened_fields"].is_array());
}
static json RunFit(const ContinuityContextV1& context, int context_tokens, int output, int memory_cap,
    bool expected, std::vector<LlmPromptMessage>* messages, std::string* error) {
    std::string metrics_text;
    const bool fitted = FitContinuityPrompt(context, context_tokens, output, memory_cap, BuildPrompt, Counter(),
        messages, &metrics_text, error);
    CHECK(!metrics_text.empty());
    const json metrics = json::parse(metrics_text); CheckMetricsKeys(metrics);
    CHECK(fitted == expected && metrics["success"].get<bool>() == expected);
    return metrics;
}

static void SerializationContract() {
    ContinuityContextV1 context = TwentyEvents(); context.notes = {"NOTE-UN", "NOTE-DEUX"};
    const std::string core = context.core_json, serialized = SerializeContinuityContext(context);
    const std::vector<std::string> events = context.events_json; const json document = json::parse(serialized);
    CHECK(document["version"] == 1 && document["recent_events"].size() == 20 && document["recent_events"][0].is_object());
    CHECK(document["recent_events"][0]["id"] == "ev1" && document["subjective_notes"].size() == 2 && document["subjective_notes"][1] == "NOTE-DEUX");
    CHECK(document["origin"]["id"] == "atrium" && document["transition"]["direction"] == "north" && document["engine"]["inventory"].size() == 2 && document["engine"]["blocked_exits"][0] == "grille nord");
    CHECK(context.core_json == core && context.events_json == events && context.notes[1] == "NOTE-DEUX");
    CHECK(SerializeContinuityContext(context) == serialized);
}
static void SelectionNotesAndBudget() {
    ContinuityContextV1 context = TwentyEvents();
    for (int i = 1; i <= 6; ++i) context.notes.push_back("NOTE-" + std::to_string(i) + " observation");
    const std::string core_before = context.core_json; const std::vector<std::string> events_before = context.events_json;
    const int output = 64, context_tokens = TokenBytes(BuildPrompt(context)) + output + 32;
    std::vector<LlmPromptMessage> messages; std::string error;
    const json metrics = RunFit(context, context_tokens, output, 4096, true, &messages, &error);
    CHECK(error.empty()); const int after = TokenBytes(messages), mandatory = TokenBytes(BuildPrompt(CoreOnly(context)));
    CHECK(metrics["context_tokens"] == context_tokens && metrics["output_reserved"] == output);
    CHECK(metrics["technical_margin"] == 32 && metrics["memory_cap"] == 4096 && metrics["prompt_tokens_after"] == after);
    CHECK(metrics["prompt_tokens_before"].get<int>() >= after && metrics["prompt_tokens_before"].get<int>() <= TokenBytes(BuildPrompt(context)));
    CHECK(after + output + 32 <= context_tokens && metrics["mandatory_prompt_tokens"] == mandatory);
    CHECK(metrics["memory_tokens"] == after - mandatory && metrics["memory_available"].get<int>() >= after - mandatory);
    const std::vector<std::string> expected = {"ev15", "ev16", "ev17", "ev18", "ev19", "ev20"};
    CHECK(RetainedIds(metrics) == expected && metrics["omitted_events"] == 14 && metrics["omitted_notes"] == 2);
    CHECK(CountPresentNotes(messages, context.notes) == 4);
    CHECK(context.core_json == core_before && context.events_json == events_before && context.notes.size() == 6);
    for (const auto& id : expected) CHECK(PromptContains(messages, "\"" + id + "\""));
    for (const char* older : {"\"ev1\"", "\"ev9\"", "\"ev13\"", "\"ev14\""}) CHECK(!PromptContains(messages, older));
    for (const char* core : {"cle de cuivre", "grille nord", "entered", "Atrium de verre"}) CHECK(PromptContains(messages, core));
}

static void ConsultationsAndDuplicates() {
    ContinuityContextV1 context; context.core_json = CoreJson();
    context.events_json = {
        EventJson(1, "atrium", "read_inventory", "listed", "Inventaire lu."),
        EventJson(2, "atrium", "go north", "moved", "Pas vers le nord."),
        EventJson(3, "couloir", "inspect_inventory_item", "examined", "Cle examinee."),
        EventJson(4, "couloir", "go east", "blocked", "Porte est verrouillee."),
        EventJson(5, "couloir", "go east", "blocked", "Porte est verrouillee."),
        EventJson(6, "atrium", "take key", "taken", "Cle de cuivre prise."),
        EventJson(7, "atrium", "read_field_notes", "read", "Notes relues."),
        EventJson(8, "couloir", "push door", "opened", "Porte poussee.")};
    const int output = 64, context_tokens = TokenBytes(BuildPrompt(context)) + output + 32;
    std::vector<LlmPromptMessage> messages; std::string error;
    const json metrics = RunFit(context, context_tokens, output, 4096, true, &messages, &error);
    CHECK(error.empty() && TokenBytes(messages) + output + 32 <= context_tokens);
    CHECK(!PromptContains(messages, "read_inventory") && !PromptContains(messages, "inspect_inventory_item"));
    CHECK(!PromptContains(messages, "read_field_notes") && PromptContains(messages, "take key"));
    CHECK(PromptContains(messages, "push door") && PromptContains(messages, "\"ev6\"") && PromptContains(messages, "\"ev8\""));
    CHECK(PromptContains(messages, "\"ev4\"") != PromptContains(messages, "\"ev5\""));
    const std::vector<std::string> retained = RetainedIds(metrics);
    CHECK(!retained.empty() && retained.size() <= 6 && metrics["omitted_events"] == 8 - static_cast<int>(retained.size()));
    std::set<std::string> places; for (const auto& event_text : context.events_json) {
        const json event = json::parse(event_text);
        for (const auto& id : retained) if (event["id"] == id) places.insert(event["place_id"].get<std::string>());
    }
    CHECK(places.size() <= 2);
    ContinuityContextV1 only; only.core_json = CoreJson();
    only.events_json = {EventJson(1, "atrium", "read_inventory", "listed", "Inventaire lu."),
        EventJson(2, "atrium", "inspect_inventory_item", "examined", "Cle examinee."),
        EventJson(3, "atrium", "read_field_notes", "read", "Notes relues.")};
    const int only_tokens = TokenBytes(BuildPrompt(only)) + output + 32;
    std::vector<LlmPromptMessage> only_messages; std::string only_error;
    const json only_metrics = RunFit(only, only_tokens, output, 4096, true, &only_messages, &only_error);
    const std::vector<std::string> only_retained = RetainedIds(only_metrics);
    CHECK(only_error.empty() && !only_retained.empty() && only_retained.size() <= 3);
    CHECK(PromptContains(only_messages, "\"ev3\"") && PromptContains(only_messages, "read_field_notes"));
    CHECK(only_metrics["omitted_events"] == 3 - static_cast<int>(only_retained.size()));
}
static void BudgetWithRepairAndNotes() {
    ContinuityContextV1 context = TwentyEvents(true);
    for (int i = 1; i <= 6; ++i) context.notes.push_back("NOTE-" + std::to_string(i) + " observation");
    const int output = 48, mandatory = TokenBytes(BuildPrompt(CoreOnly(context)));
    const int context_tokens = mandatory + 320 + output + 32, memory_cap = 250;
    std::vector<LlmPromptMessage> messages; std::string error;
    const json metrics = RunFit(context, context_tokens, output, memory_cap, true, &messages, &error);
    const int after = TokenBytes(messages), before = metrics["prompt_tokens_before"].get<int>();
    CHECK(error.empty() && metrics["prompt_tokens_after"] == after && metrics["mandatory_prompt_tokens"] == mandatory);
    CHECK(before > after && before <= TokenBytes(BuildPrompt(context)));
    CHECK(after + output + 32 <= context_tokens && after - mandatory <= memory_cap);
    CHECK(metrics["memory_tokens"] == after - mandatory && metrics["memory_cap"] == memory_cap);
    CHECK(metrics["memory_available"].get<int>() <= memory_cap && metrics["memory_available"].get<int>() >= after - mandatory);
    CHECK(PromptContains(messages, "Diagnostic de reparation") && PromptContains(messages, "Repare le contexte"));
    const int present = CountPresentNotes(messages, context.notes); CHECK(present <= 4 && metrics["omitted_notes"] == 6 - present);
    const std::vector<std::string> retained = RetainedIds(metrics);
    CHECK(!retained.empty() && retained.size() <= 6 && metrics["omitted_events"] == 20 - static_cast<int>(retained.size()));
    for (size_t i = 0; i < retained.size(); ++i)
        CHECK(retained[i] == "ev" + std::to_string(21 - static_cast<int>(retained.size()) + static_cast<int>(i)));
    for (const auto& id : retained) CHECK(PromptContains(messages, "\"" + id + "\""));
}

static void OriginShortenedToSentence() {
    const std::string japanese =
        "\xE6\x89\x89\xE3\x81\xAF\xE9\x96\x89\xE3\x81\x96\xE3\x81\x95\xE3\x82\x8C\xE3\x81\xA6\xE3\x81\x84\xE3\x82\x8B\xE3\x80\x82";
    const std::string first = "Une verriere fendue laisse tomber une lumiere pale sur les dalles.";
    const std::string long_description = first
        + " Les arches basses gardent un air froid et des traces de pas humides."
        + " Le sol garde des reflets ternes." + " La poussiere dort sur chaque marche. " + japanese;
    ContinuityContextV1 context; context.core_json = CoreJson(false, long_description);
    context.events_json = {EventJson(90, "atrium", "aller_90", "moved", "Traversee calme du seuil.")};
    const int output = 48, mandatory = TokenBytes(BuildPrompt(CoreOnly(context)));
    const int context_tokens = mandatory + 60 + output + 32;
    std::vector<LlmPromptMessage> messages; std::string error;
    const json metrics = RunFit(context, context_tokens, output, 100000, true, &messages, &error);
    const int after = TokenBytes(messages);
    CHECK(error.empty() && metrics["prompt_tokens_after"] == after && after + output + 32 <= context_tokens);
    CHECK(metrics["memory_tokens"].get<int>() <= metrics["memory_available"].get<int>() && !metrics["shortened_fields"].empty());
    CHECK(PromptContains(messages, first));
    CHECK(!PromptContains(messages, "Les arches basses") && !PromptContains(messages, "Le sol garde"));
    CHECK(!PromptContains(messages, "La poussiere") && PromptContains(messages, "\"ev90\""));
    CHECK(PromptContains(messages, "cle de cuivre") && PromptContains(messages, "grille nord") && ValidUtf8(PromptAll(messages)));
}

static void MandatoryDiagnosticTooLarge() {
    ContinuityContextV1 context; context.core_json = CoreJson(true);
    const std::string core_before = context.core_json;
    ContinuityContextV1 bare = context; bare.core_json = CoreJson(false);
    const int output = 48, without_repair = TokenBytes(BuildPrompt(CoreOnly(bare)));
    const int context_tokens = without_repair + 40 + output + 32;
    CHECK(TokenBytes(BuildPrompt(CoreOnly(context))) + output + 32 > context_tokens);
    CHECK(without_repair + output + 32 <= context_tokens);
    std::vector<LlmPromptMessage> messages; std::string error;
    const json metrics = RunFit(context, context_tokens, output, 100000, false, &messages, &error);
    CHECK(error.find("context_limit") != std::string::npos);
    CHECK(metrics["error"].is_string() && metrics["error"].get<std::string>().find("context_limit") != std::string::npos);
    CHECK(context.core_json == core_before); const json core = json::parse(context.core_json);
    CHECK(core["engine"]["inventory"].size() == 2 && core["engine"]["blocked_exits"][0] == "grille nord");
    CHECK(core["transition"]["outcome"] == "entered" && core["origin"]["id"] == "atrium");
}
// Codex integration: exercise non-ASCII first sentences, not just a discarded suffix.
static void UnicodeSentences() {
    for(const std::string first:{std::string(u8"La lumière éclaire l’étrange carrière. "),std::string(u8"扉は閉じられている。")}) {
        ContinuityContextV1 original;original.core_json=CoreJson();
        std::string long_description=first;
        for(int i=0;i<40;++i)long_description+=u8"Les arêtes éclairées révèlent les échelles. ";
        original.events_json={EventJson(1,"atrium","look","No movement",long_description)};
        auto compact=original;
        compact.events_json={EventJson(1,"atrium","look","No movement",first)};
        const int budget=TokenBytes(BuildPrompt(compact))+64+32;
        std::vector<LlmPromptMessage> messages;std::string error;
        const auto metrics=RunFit(original,budget,64,1024,true,&messages,&error);
        const auto selected=json::parse(messages[1].content);
        CHECK(selected["recent_events"][0]["description"].get<std::string>().find(first.substr(0,first.size()-1))==0);
        CHECK(ValidUtf8(messages[1].content)&&!metrics["shortened_fields"].empty());
        CHECK(original.events_json[0].find("chelles")!=std::string::npos);
    }
}
// DUO-075: variante cloud de FitContinuityPrompt (preserve_latest_within_context).
static json RunFitPreserve(const ContinuityContextV1& context, int context_tokens, int output, int memory_cap,
    bool allow_empty, bool preserve, bool expected, std::vector<LlmPromptMessage>* messages, std::string* error) {
    std::string metrics_text;
    const bool fitted = FitContinuityPrompt(context, context_tokens, output, memory_cap, BuildPrompt, Counter(),
        messages, &metrics_text, error, allow_empty, preserve);
    CHECK(!metrics_text.empty());
    const json metrics = json::parse(metrics_text); CheckMetricsKeys(metrics);
    CHECK(fitted == expected && metrics["success"].get<bool>() == expected);
    return metrics;
}
// Le mode cloud tolere un dernier evenement utile au-dela de memory_tokens si le prompt complet,
// la sortie reservee et la marge de 32 tiennent dans context_tokens ; son outcome n'est pas coupe.
static void PreserveLatestEventWithinContext() {
    std::string long_outcome;
    while (long_outcome.size() < 1200)
        long_outcome += "La verriere garde des eclats ternes et une trace humide sur toute la longueur du montant. ";
    CHECK(long_outcome.size() > 1100);
    const std::string note = "NOTE-ANCIENNE a oublier avant le dernier resultat.";
    const std::string old_event = EventJson(1, "atrium", "aller_1", "moved", "Traversee 1 sans obstacle.");
    const std::string latest_event = EventJson(2, "serre", "examiner_verriere", long_outcome, "Verriere pale examinee.");
    ContinuityContextV1 context; context.core_json = CoreJson();
    context.events_json = {old_event, latest_event}; context.notes = {note};
    const std::string core_before = context.core_json;
    const std::vector<std::string> events_before = context.events_json;

    const int output = 64, context_tokens = 32768, memory_cap = 1024;
    const int mandatory = TokenBytes(BuildPrompt(CoreOnly(context)));
    CHECK(TokenBytes(BuildPrompt(context)) - mandatory > memory_cap);

    // Comportement strict existant : apres retrait de la note et de l'ancien evenement, le dernier
    // evenement depasse memory_tokens et l'outcome n'etant pas coupe, l'ajustement echoue.
    std::vector<LlmPromptMessage> strict_messages; std::string strict_error;
    const json strict_metrics = RunFitPreserve(context, context_tokens, output, memory_cap, false, false, false,
        &strict_messages, &strict_error);
    CHECK(strict_error.find("context_limit") != std::string::npos);
    CHECK(strict_metrics["error"].get<std::string>().find("context_limit") != std::string::npos);
    CHECK(strict_metrics["omitted_notes"].get<int>() == 1 && strict_metrics["omitted_events"].get<int>() >= 1);
    CHECK(context.core_json == core_before && context.events_json == events_before && context.notes.size() == 1);

    // Mode cloud : le dernier evenement utile et son outcome sont conserves entiers.
    std::vector<LlmPromptMessage> messages; std::string error;
    const json metrics = RunFitPreserve(context, context_tokens, output, memory_cap, false, true, true, &messages, &error);
    const int after = TokenBytes(messages);
    CHECK(error.empty() && metrics["prompt_tokens_after"] == after && metrics["mandatory_prompt_tokens"] == mandatory);
    CHECK(after + output + 32 <= context_tokens);
    CHECK(metrics["memory_tokens"] == after - mandatory && metrics["memory_tokens"].get<int>() > memory_cap);
    CHECK(metrics.find("memory_soft_limit_exceeded") != metrics.end());
    CHECK(metrics["memory_soft_limit_exceeded"].is_boolean() && metrics["memory_soft_limit_exceeded"].get<bool>());
    CHECK(PromptContains(messages, long_outcome));
    CHECK(PromptContains(messages, "\"ev2\"") && !PromptContains(messages, "\"ev1\""));
    CHECK(!PromptContains(messages, note));
    CHECK(RetainedIds(metrics) == std::vector<std::string>{"ev2"});
    CHECK(metrics["omitted_events"] == 1 && metrics["omitted_notes"] == 1);
    CHECK(context.core_json == core_before && context.events_json == events_before && context.notes.size() == 1);

    // Limite totale trop petite : la tolerance memoire ne doit pas contourner context_tokens.
    const int tiny_tokens = mandatory + 300 + output + 32;
    std::vector<LlmPromptMessage> tiny_messages; std::string tiny_error;
    const json tiny_metrics = RunFitPreserve(context, tiny_tokens, output, memory_cap, false, true, false,
        &tiny_messages, &tiny_error);
    CHECK(tiny_error.find("context_limit") != std::string::npos);
    CHECK(tiny_metrics["error"].get<std::string>().find("context_limit") != std::string::npos);
    CHECK(context.core_json == core_before && context.events_json == events_before && context.notes.size() == 1);

    // allow_empty_memory=true ne doit pas perdre le dernier evenement utile quand il tient.
    std::vector<LlmPromptMessage> empty_messages; std::string empty_error;
    const json empty_metrics = RunFitPreserve(context, context_tokens, output, memory_cap, true, true, true,
        &empty_messages, &empty_error);
    CHECK(empty_error.empty() && TokenBytes(empty_messages) + output + 32 <= context_tokens);
    CHECK(PromptContains(empty_messages, long_outcome) && PromptContains(empty_messages, "\"ev2\""));
    CHECK(!PromptContains(empty_messages, "\"ev1\"") && !PromptContains(empty_messages, note));
    CHECK(RetainedIds(empty_metrics) == std::vector<std::string>{"ev2"});
    CHECK(empty_metrics.find("memory_soft_limit_exceeded") != empty_metrics.end());
    CHECK(empty_metrics["memory_soft_limit_exceeded"].is_boolean() && empty_metrics["memory_soft_limit_exceeded"].get<bool>());
    CHECK(context.core_json == core_before && context.events_json == events_before && context.notes.size() == 1);
}
int main() {
    try {
        SerializationContract(); SelectionNotesAndBudget(); ConsultationsAndDuplicates();
        BudgetWithRepairAndNotes(); OriginShortenedToSentence(); MandatoryDiagnosticTooLarge(); UnicodeSentences();
        PreserveLatestEventWithinContext();
        std::puts("Continuity budget: selection 6/2, order, dedup, consultations, notes, repair load, UTF-8 sentence, context_limit and cloud latest-event preservation passed"); return 0;
    } catch (const std::exception& e) { std::fprintf(stderr, "%s\n", e.what()); return 1; }
}
