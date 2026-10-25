#include "place_proposal.h"

#include <cmath>
#include <iostream>
#include <limits>
#include <map>
#include <string>
#include <vector>

#include "nlohmann/json.hpp"

namespace {

int g_failures = 0;

void Check(bool condition, int line, const char* expression) {
    if (!condition) {
        ++g_failures;
        std::cerr << "FAIL line " << line << ": " << expression << '\n';
    }
}

#define CHECK(expression) Check((expression), __LINE__, #expression)

const std::map<std::string, bool> kNoFixedExits;
const std::vector<std::string> kBasePossibleExits{"north", "west"};

bool Run(const std::string& text, PlaceProposal* output, std::string* error,
         int current_external = 464, float current_body = 37.0f,
         const std::map<std::string, bool>& fixed_exits = kNoFixedExits,
         const std::vector<std::string>& possible_exits = kBasePossibleExits) {
    return ParsePlaceProposal(text, current_external, current_body, fixed_exits,
                              possible_exits, output, error);
}

void ExpectFailure(const char* label, const std::string& text,
                   int current_external = 464, float current_body = 37.0f,
                   const std::map<std::string, bool>& fixed_exits = kNoFixedExits,
                   const std::vector<std::string>& possible_exits = kBasePossibleExits) {
    PlaceProposal output;
    std::string error = "stale error";
    const bool parsed = Run(text, &output, &error, current_external, current_body,
                            fixed_exits, possible_exits);
    if (parsed) {
        ++g_failures;
        std::cerr << "FAIL expected rejection: " << label << '\n';
    }
    if (error.empty()) {
        ++g_failures;
        std::cerr << "FAIL missing error for: " << label << '\n';
    }
}

bool Near(float left, float right) {
    return std::fabs(left - right) < 1e-5f;
}

bool ExitIs(const PlaceProposal& proposal, const std::string& direction, bool value) {
    const auto it = proposal.exits.find(direction);
    return it != proposal.exits.end() && it->second == value;
}

bool SameProposal(const PlaceProposal& left, const PlaceProposal& right) {
    return left.title == right.title && left.summary == right.summary &&
           left.narration == right.narration &&
           left.thermal_reason == right.thermal_reason &&
           left.external_temperature_c == right.external_temperature_c &&
           left.body_temperature_c == right.body_temperature_c &&
           left.exits == right.exits;
}

const char* const kValidJson = R"json({
  "title": "La salle des passages",
  "summary": "Un lieu stable avec deux issues connues.",
  "narration": "La chaleur reste stable. Une porte au nord et un retour a l'ouest.",
  "thermal": {
    "external_temperature_c": 464,
    "body_temperature_c": 37.0,
    "reason": "La salle conserve une temperature stable."
  },
  "exits": {
    "north": true,
    "east": false,
    "south": false,
    "west": true
  }
})json";

std::string Dump(const nlohmann::json& value) {
    return value.dump();
}

void TestValidProposal() {
    const std::map<std::string, bool> fixed_north{{"north", true}};
    PlaceProposal output;
    std::string error = "stale error";
    CHECK(Run(kValidJson, &output, &error, 464, 37.0f, fixed_north,
              kBasePossibleExits));
    CHECK(error.empty());
    CHECK(output.title == "La salle des passages");
    CHECK(output.summary == "Un lieu stable avec deux issues connues.");
    CHECK(output.exits.size() == 4);
    CHECK(ExitIs(output, "north", true));
    CHECK(ExitIs(output, "east", false));
    CHECK(ExitIs(output, "south", false));
    CHECK(ExitIs(output, "west", true));
    CHECK(output.external_temperature_c == 464);
    CHECK(Near(output.body_temperature_c, 37.0f));
    CHECK(!output.thermal_reason.empty());
}

void TestThermalBoundsAndVariations() {
    nlohmann::json value = nlohmann::json::parse(kValidJson);

    value["thermal"]["body_temperature_c"] = 37.0;
    PlaceProposal output;
    std::string error;
    CHECK(Run(Dump(value), &output, &error, 464, 37.0f));
    CHECK(Near(output.body_temperature_c, 37.0f));

    value["thermal"]["body_temperature_c"] = 37.4;
    CHECK(Run(Dump(value), &output, &error, 464, 37.0f));
    CHECK(Near(output.body_temperature_c, 37.4f));

    value["thermal"]["body_temperature_c"] = 36.6;
    CHECK(Run(Dump(value), &output, &error, 464, 37.0f));
    CHECK(Near(output.body_temperature_c, 36.6f));

    value["thermal"]["body_temperature_c"] = 37.4001;
    CHECK(Run(Dump(value), &output, &error, 464, 37.0f));

    value["thermal"]["body_temperature_c"] = 36.5999;
    CHECK(Run(Dump(value), &output, &error, 464, 37.0f));

    value["thermal"]["body_temperature_c"] = 37.4002;
    ExpectFailure("body increase over the variation limit", Dump(value));

    value["thermal"]["body_temperature_c"] = 36.5998;
    ExpectFailure("body decrease over the variation limit", Dump(value));

    value = nlohmann::json::parse(kValidJson);
    value["thermal"]["external_temperature_c"] = 300;
    value["thermal"]["body_temperature_c"] = 34.0;
    CHECK(Run(Dump(value), &output, &error, 300, 34.0f));
    CHECK(output.external_temperature_c == 300);
    CHECK(Near(output.body_temperature_c, 34.0f));

    value = nlohmann::json::parse(kValidJson);
    value["thermal"]["external_temperature_c"] = 520;
    value["thermal"]["body_temperature_c"] = 43.0;
    CHECK(Run(Dump(value), &output, &error, 520, 43.0f));
    CHECK(output.external_temperature_c == 520);
    CHECK(Near(output.body_temperature_c, 43.0f));

    value = nlohmann::json::parse(kValidJson);
    value["thermal"]["external_temperature_c"] = 299;
    value["thermal"]["body_temperature_c"] = 34.0;
    ExpectFailure("external temperature below the absolute bound", Dump(value), 300,
                  34.0f);

    value = nlohmann::json::parse(kValidJson);
    value["thermal"]["external_temperature_c"] = 521;
    value["thermal"]["body_temperature_c"] = 43.0;
    ExpectFailure("external temperature above the absolute bound", Dump(value), 520,
                  43.0f);

    value = nlohmann::json::parse(kValidJson);
    value["thermal"]["external_temperature_c"] = 300;
    value["thermal"]["body_temperature_c"] = 33.999;
    ExpectFailure("body temperature below the absolute bound", Dump(value), 300,
                  34.0f);

    value = nlohmann::json::parse(kValidJson);
    value["thermal"]["external_temperature_c"] = 520;
    value["thermal"]["body_temperature_c"] = 43.001;
    ExpectFailure("body temperature above the absolute bound", Dump(value), 520,
                  43.0f);

    value = nlohmann::json::parse(kValidJson);
    value["thermal"]["external_temperature_c"] = 484;
    CHECK(Run(Dump(value), &output, &error, 464, 37.0f));

    value["thermal"]["external_temperature_c"] = 485;
    ExpectFailure("external increase over the variation limit", Dump(value));

    value["thermal"]["external_temperature_c"] = 444;
    CHECK(Run(Dump(value), &output, &error, 464, 37.0f));

    value["thermal"]["external_temperature_c"] = 443;
    ExpectFailure("external decrease over the variation limit", Dump(value));

    value = nlohmann::json::parse(kValidJson);
    value["thermal"]["external_temperature_c"] = 464.0;
    ExpectFailure("external temperature must be an integer", Dump(value));

    value["thermal"]["external_temperature_c"] = "464";
    ExpectFailure("external temperature must not be a string", Dump(value));

    value = nlohmann::json::parse(kValidJson);
    value["thermal"]["body_temperature_c"] = "37.0";
    ExpectFailure("body temperature must not be a string", Dump(value));

    PlaceProposal unchanged;
    std::string bad_current_error;
    CHECK(!Run(kValidJson, &unchanged, &bad_current_error, 464,
               std::numeric_limits<float>::quiet_NaN()));
    CHECK(!bad_current_error.empty());
    CHECK(!Run(kValidJson, &unchanged, &bad_current_error, 464,
               std::numeric_limits<float>::infinity()));
}

void TestExitRules() {
    nlohmann::json value = nlohmann::json::parse(kValidJson);
    for (const char* direction : {"north", "east", "south", "west"}) {
        value["exits"][direction] = false;
    }

    PlaceProposal output;
    std::string error;
    CHECK(Run(Dump(value), &output, &error, 464, 37.0f));
    CHECK(ExitIs(output, "north", false));
    CHECK(ExitIs(output, "east", false));
    CHECK(ExitIs(output, "south", false));
    CHECK(ExitIs(output, "west", false));

    const std::map<std::string, bool> fixed_all_false{
        {"north", false}, {"east", false}, {"south", false}, {"west", false}};
    CHECK(Run(Dump(value), &output, &error, 464, 37.0f, fixed_all_false,
              kBasePossibleExits));
    const std::vector<std::string> no_possible_exits;
    CHECK(Run(Dump(value), &output, &error, 464, 37.0f, kNoFixedExits,
              no_possible_exits));

    const std::map<std::string, bool> fixed_north_true{{"north", true}};
    const std::map<std::string, bool> fixed_north_false{{"north", false}};

    ExpectFailure("fixed open exit conflicts with a closed proposal", Dump(value), 464,
                  37.0f, fixed_north_true, kBasePossibleExits);
    ExpectFailure("fixed closed exit conflicts with an open proposal", kValidJson, 464,
                  37.0f, fixed_north_false, kBasePossibleExits);

    const std::vector<std::string> only_north{"north"};
    ExpectFailure("west opening is geometrically forbidden", kValidJson, 464, 37.0f,
                  kNoFixedExits, only_north);

    const std::vector<std::string> only_west{"west"};
    ExpectFailure("north opening is geometrically forbidden", kValidJson, 464, 37.0f,
                  kNoFixedExits, only_west);

    ExpectFailure("fixed open exit is not geometrically possible", kValidJson, 464,
                  37.0f, fixed_north_true, only_west);

    const std::map<std::string, bool> unknown_fixed{{"up", false}};
    ExpectFailure("unknown fixed exit direction", kValidJson, 464, 37.0f, unknown_fixed,
                  kBasePossibleExits);

    const std::vector<std::string> unknown_possible{"north", "west", "up"};
    ExpectFailure("unknown possible exit direction", kValidJson, 464, 37.0f,
                  kNoFixedExits, unknown_possible);

    value = nlohmann::json::parse(kValidJson);
    value["exits"]["north"] = 1;
    ExpectFailure("exit boolean must reject an integer", Dump(value));

    value = nlohmann::json::parse(kValidJson);
    value["exits"]["north"] = "true";
    ExpectFailure("exit boolean must reject a string", Dump(value));

    value = nlohmann::json::parse(kValidJson);
    value["exits"]["east"] = 0;
    ExpectFailure("closed exit must be a boolean", Dump(value));

    value = nlohmann::json::parse(kValidJson);
    value["exits"]["south"] = nullptr;
    ExpectFailure("exit boolean must reject null", Dump(value));

    value = nlohmann::json::parse(kValidJson);
    value["exits"]["up"] = false;
    ExpectFailure("extra exit direction", Dump(value));

    value = nlohmann::json::parse(kValidJson);
    value["exits"].erase("west");
    ExpectFailure("missing exit direction", Dump(value));

    value = nlohmann::json::parse(kValidJson);
    value["exits"] = "closed";
    ExpectFailure("exits must be an object", Dump(value));
}

void TestSchemaAndStrings() {
    nlohmann::json value = nlohmann::json::parse(kValidJson);

    value.erase("title");
    ExpectFailure("missing root title", Dump(value));
    value = nlohmann::json::parse(kValidJson);
    value.erase("summary");
    ExpectFailure("missing root summary", Dump(value));
    value = nlohmann::json::parse(kValidJson);
    value.erase("narration");
    ExpectFailure("missing root narration", Dump(value));
    value = nlohmann::json::parse(kValidJson);
    value.erase("thermal");
    ExpectFailure("missing root thermal", Dump(value));
    value = nlohmann::json::parse(kValidJson);
    value.erase("exits");
    ExpectFailure("missing root exits", Dump(value));

    value = nlohmann::json::parse(kValidJson);
    value["extra"] = true;
    ExpectFailure("extra root field", Dump(value));

    value = nlohmann::json::parse(kValidJson);
    value["thermal"].erase("reason");
    ExpectFailure("missing thermal reason", Dump(value));
    value = nlohmann::json::parse(kValidJson);
    value["thermal"].erase("body_temperature_c");
    ExpectFailure("missing thermal body", Dump(value));
    value = nlohmann::json::parse(kValidJson);
    value["thermal"]["extra"] = 1;
    ExpectFailure("extra thermal field", Dump(value));

    value = nlohmann::json::parse(kValidJson);
    value["thermal"] = "hot";
    ExpectFailure("thermal must be an object", Dump(value));
    value = nlohmann::json::parse(kValidJson);
    value["title"] = 12;
    ExpectFailure("title must be a string", Dump(value));
    value = nlohmann::json::parse(kValidJson);
    value["summary"] = false;
    ExpectFailure("summary must be a string", Dump(value));
    value = nlohmann::json::parse(kValidJson);
    value["narration"] = nullptr;
    ExpectFailure("narration must be a string", Dump(value));
    value = nlohmann::json::parse(kValidJson);
    value["thermal"]["reason"] = 7;
    ExpectFailure("thermal reason must be a string", Dump(value));
    ExpectFailure("root must be an object", "[]");

    value = nlohmann::json::parse(kValidJson);
    value["title"] = "";
    ExpectFailure("empty title", Dump(value));
    value = nlohmann::json::parse(kValidJson);
    value["summary"] = "";
    ExpectFailure("empty summary", Dump(value));
    value = nlohmann::json::parse(kValidJson);
    value["narration"] = "";
    ExpectFailure("empty narration", Dump(value));
    value = nlohmann::json::parse(kValidJson);
    value["thermal"]["reason"] = "";
    ExpectFailure("empty thermal reason", Dump(value));

    value = nlohmann::json::parse(kValidJson);
    value["title"] = std::string(301, 'x');
    ExpectFailure("title over byte limit", Dump(value));
    value["title"] = std::string(300, 'x');
    value["summary"] = std::string(2400, 's');
    value["narration"] = std::string(6000, 'n');
    value["thermal"]["reason"] = std::string(1200, 'r');
    PlaceProposal output;
    std::string error;
    CHECK(Run(Dump(value), &output, &error));
    CHECK(output.title.size() == 300);
    CHECK(output.summary.size() == 2400);
    CHECK(output.narration.size() == 6000);
    CHECK(output.thermal_reason.size() == 1200);

    value["summary"] = std::string(2401, 's');
    ExpectFailure("summary over byte limit", Dump(value));
    value = nlohmann::json::parse(kValidJson);
    value["narration"] = std::string(6001, 'n');
    ExpectFailure("narration over byte limit", Dump(value));
    value = nlohmann::json::parse(kValidJson);
    value["thermal"]["reason"] = std::string(1201, 'r');
    ExpectFailure("thermal reason over byte limit", Dump(value));
}

void TestJsonParsingStrictness() {
    const char* duplicate_root = R"json({
      "title": "A",
      "title": "B",
      "summary": "S",
      "narration": "N",
      "thermal": {
        "external_temperature_c": 464,
        "body_temperature_c": 37.0,
        "reason": "R"
      },
      "exits": {
        "north": true,
        "east": false,
        "south": false,
        "west": true
      }
    })json";
    const char* duplicate_thermal = R"json({
      "title": "A",
      "summary": "S",
      "narration": "N",
      "thermal": {
        "external_temperature_c": 464,
        "external_temperature_c": 465,
        "body_temperature_c": 37.0,
        "reason": "R"
      },
      "exits": {
        "north": true,
        "east": false,
        "south": false,
        "west": true
      }
    })json";
    const char* duplicate_exits = R"json({
      "title": "A",
      "summary": "S",
      "narration": "N",
      "thermal": {
        "external_temperature_c": 464,
        "body_temperature_c": 37.0,
        "reason": "R"
      },
      "exits": {
        "north": true,
        "north": false,
        "east": false,
        "south": false,
        "west": true
      }
    })json";

    ExpectFailure("duplicate root key", duplicate_root);
    ExpectFailure("duplicate thermal key", duplicate_thermal);
    ExpectFailure("duplicate exits key", duplicate_exits);

    ExpectFailure("empty input", "");
    ExpectFailure("malformed input", "{");
    ExpectFailure("fenced JSON", std::string("```json\n") + kValidJson + "\n```");
    ExpectFailure("leading prose", std::string("Here is the place: ") + kValidJson);
    ExpectFailure("trailing prose", std::string(kValidJson) + " trailing text");
    std::string truncated(kValidJson);
    truncated.pop_back();
    ExpectFailure("truncated JSON", truncated);
}

void TestOutputAndErrorContract() {
    PlaceProposal sentinel;
    sentinel.title = "sentinel title";
    sentinel.summary = "sentinel summary";
    sentinel.narration = "sentinel narration";
    sentinel.thermal_reason = "sentinel reason";
    sentinel.external_temperature_c = 321;
    sentinel.body_temperature_c = 7.5f;
    sentinel.exits = {{"north", false}, {"east", true}, {"south", false}, {"west", false}};

    PlaceProposal output = sentinel;
    std::string error = "stale error";
    nlohmann::json value = nlohmann::json::parse(kValidJson);
    value["exits"]["north"] = 1;
    CHECK(!Run(Dump(value), &output, &error));
    CHECK(!error.empty());
    CHECK(SameProposal(output, sentinel));

    output = sentinel;
    error = "stale error";
    CHECK(!Run(std::string("```\n") + kValidJson + "\n```", &output, &error));
    CHECK(!error.empty());
    CHECK(SameProposal(output, sentinel));

    output = sentinel;
    error = "stale error";
    const char* duplicate = R"json({
      "title": "A",
      "summary": "S",
      "narration": "N",
      "thermal": {
        "external_temperature_c": 464,
        "body_temperature_c": 37.0,
        "reason": "R"
      },
      "exits": {
        "north": true,
        "east": false,
        "south": false,
        "west": false,
        "west": true
      }
    })json";
    CHECK(!Run(duplicate, &output, &error));
    CHECK(!error.empty());
    CHECK(SameProposal(output, sentinel));

    output = sentinel;
    error = "stale error";
    CHECK(!Run(kValidJson, &output, &error, 464,
               std::numeric_limits<float>::infinity()));
    CHECK(!error.empty());
    CHECK(SameProposal(output, sentinel));

    std::string null_output_error;
    CHECK(!Run(kValidJson, nullptr, &null_output_error));
    CHECK(!null_output_error.empty());
    CHECK(!Run(kValidJson, nullptr, nullptr));

    output = sentinel;
    CHECK(Run(kValidJson, &output, nullptr));
    CHECK(!SameProposal(output, sentinel));

    output = sentinel;
    CHECK(!Run(std::string("not json"), &output, nullptr));
    CHECK(SameProposal(output, sentinel));

    output = sentinel;
    error = "stale error";
    CHECK(Run(kValidJson, &output, &error));
    CHECK(error.empty());
}

}  // namespace

int main() {
    TestValidProposal();
    TestThermalBoundsAndVariations();
    TestExitRules();
    TestSchemaAndStrings();
    TestJsonParsingStrictness();
    TestOutputAndErrorContract();

    if (g_failures != 0) {
        std::cerr << g_failures << " place proposal check(s) failed\n";
        return 1;
    }
    std::cout << "All place proposal checks passed\n";
    return 0;
}
