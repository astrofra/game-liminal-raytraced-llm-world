#include "scene_normalization.h"

#include <cstdio>
#include <string>
#include <vector>

namespace {

int g_checks = 0;
int g_failures = 0;

void Check(bool condition, const char* name) {
    ++g_checks;
    if (condition) return;
    ++g_failures;
    std::fprintf(stderr, "FAIL: %s\n", name);
}

void CheckText(const liminal::SceneNormalization& result, const std::string& expected,
               const char* name) {
    ++g_checks;
    if (result.text == expected) return;
    ++g_failures;
    std::fprintf(stderr,
                 "FAIL: %s\n--- expected (%zu bytes) ---\n%s\n--- actual (%zu bytes) ---\n%s\n--- end ---\n",
                 name, expected.size(), expected.c_str(), result.text.size(), result.text.c_str());
}

void CheckChanges(const liminal::SceneNormalization& result,
                  const std::vector<std::string>& expected, const char* name) {
    ++g_checks;
    if (result.changes == expected) return;
    ++g_failures;
    std::fprintf(stderr, "FAIL: %s (changes %zu, expected %zu)\n", name, result.changes.size(),
                 expected.size());
    for (const std::string& change : result.changes) std::fprintf(stderr, "  got: %s\n", change.c_str());
    for (const std::string& change : expected) std::fprintf(stderr, "  expected: %s\n", change.c_str());
}

const std::string kBom = "\xEF\xBB\xBF";
const std::string kBomRemoved = "bom_removed";
const std::string kNewlines = "newlines_normalized";
const std::string kEnvelope = "markdown_envelope_removed";
const std::string kSemicolon = "terminal_semicolon_removed";
const std::string kComma = "trailing_list_comma_removed";

void CheckCase(const std::string& raw, const std::string& expected,
               const std::vector<std::string>& expected_changes, const char* name) {
    const liminal::SceneNormalization result = liminal::NormalizeSceneSyntax(raw);
    CheckText(result, expected, name);
    CheckChanges(result, expected_changes, name);
    const liminal::SceneNormalization again = liminal::NormalizeSceneSyntax(result.text);
    const std::string idempotence = std::string(name) + " (idempotence)";
    CheckText(again, result.text, idempotence.c_str());
    Check(again.changes.empty(), idempotence.c_str());
}

void TestFullPipeline() {
    const std::string raw = kBom +
        "```scene\r\n"
        "box \"wall\" pos(0,0,0) size(1,2,3,)\r\n"
        "camera eye(0,1.6,-6) target(0,1,5) up(0,1,0) fov(50);\r\n"
        "# keep pos(1,2,3,)\r\n"
        "```\r\n";
    const std::string expected =
        "box \"wall\" pos(0,0,0) size(1,2,3)\n"
        "camera eye(0,1.6,-6) target(0,1,5) up(0,1,0) fov(50)\n"
        "# keep pos(1,2,3,)\n";
    CheckCase(raw, expected, {kBomRemoved, kNewlines, kEnvelope, kSemicolon, kComma},
              "bom + crlf + envelope + semicolon + list comma");
    const liminal::SceneNormalization result = liminal::NormalizeSceneSyntax(raw);
    Check(result.changes.size() == 5, "one change entry per transformation type");
}

void TestWhitespaceOnlyEnvelope() {
    CheckCase("  \n```\nroom \"a\"\n```\n  ", "room \"a\"\n", {kEnvelope},
              "envelope surrounded by whitespace");
    CheckCase("```.scene\nbox \"a\" pos(1,2,3)\n```\n", "box \"a\" pos(1,2,3)\n", {kEnvelope},
              "dot-scene label");
    CheckCase("```scene \nbox \"a\"\n```\n", "box \"a\"\n", {kEnvelope},
              "label with trailing spaces");
}

void TestEnvelopeRefusals() {
    const std::vector<std::string> unchanged = {
        "```scene\nbox \"a\" pos(1,2,3)\n",
        "Here is the scene:\n```\nbox \"a\"\n```\n",
        "```scenex\nbox \"a\"\n```\n",
        "```scene box\nbox \"a\"\n```\n",
        "```\nbox \"a\"\n```\ntrailing prose\n",
        "~~~\nbox \"a\"\n~~~\n",
        "```\n```\n```\n",
        "```scene\n```\n```\n",
        "The fence ``` is mentioned in prose only\n",
    };
    for (size_t i = 0; i < unchanged.size(); ++i) {
        const std::string name = "envelope refusal " + std::to_string(i);
        CheckCase(unchanged[i], unchanged[i], {}, name.c_str());
    }
}

void TestQuotesAndComments() {
    const std::string text =
        "room \"a;b\"\n"
        "box \"c(d\" pos(1,2,3)\n"
        "# keep pos(1,2,3,);\n"
        "sky zenith(0.5) # comment ;\n"
        "room \"(1,2,3,)\"\n";
    CheckCase(text, text, {}, "quotes and comments untouched");
    CheckCase("room \"a;b\";", "room \"a;b\"", {kSemicolon}, "semicolon outside quotes only");
    CheckCase("box \"a\" pos;", "box \"a\" pos", {kSemicolon}, "semicolon at end of code");
    CheckCase("box \"a#b\" pos(1,2,3,) # trailing", "box \"a#b\" pos(1,2,3) # trailing", {kComma},
              "hash inside quoted name and comment preserved");
    CheckCase("box \"a\" pos(1,2,3,) # note pos(4,5,6,)",
              "box \"a\" pos(1,2,3) # note pos(4,5,6,)", {kComma},
              "comment keeps its own trailing comma");
}

void TestNumbers() {
    CheckCase("spotlight \"lamp\" panel(2) offset(1e-3,-2.5,3.) range(4e2,) cone(20,30)\n",
              "spotlight \"lamp\" panel(2) offset(1e-3,-2.5,3.) range(4e2) cone(20,30)\n",
              {kComma}, "scientific and negative values");
    CheckCase("box \"b\" pos(-1.5e-2, 2, 3, ) rot(0,-90,5)\n",
              "box \"b\" pos(-1.5e-2, 2, 3 ) rot(0,-90,5)\n", {kComma},
              "spaces before closing parenthesis");
    CheckCase("box \"a\" pos(1,2,3, ) size(4,5,6, ) rot(0,0,0)\n",
              "box \"a\" pos(1,2,3 ) size(4,5,6 ) rot(0,0,0)\n", {kComma},
              "two lists on one line, one change entry");
    CheckCase("box \"a\" pos(1,2,3,4,)", "box \"a\" pos(1,2,3,4)", {kComma},
              "arity is left to the downstream parser");
    CheckCase("box \"a\" size(0,0,0,)", "box \"a\" size(0,0,0)", {kComma},
              "values preserved, only the comma removed");
}

void TestCommaRefusals() {
    const std::vector<std::string> unchanged = {
        "box \"a\" pos(,)",
        "box \"a\" pos()",
        "box \"a\" pos(1,,2,)",
        "box \"a\" pos(1,2,3,,)",
        "box \"a\" pos(1,2,inf,)",
        "box \"a\" pos(1,2,-inf,)",
        "box \"a\" pos(1,2,nan,)",
        "box \"a\" pos(1,2,x,)",
        "box \"a\" pos(1 2,3,)",
        "box \"a\" pos((1,2,3,))",
        "box \"a\" pos(1,2,3,",
        "box \"a\" pos(1,2,3,) extra(",
        "box \"a\" pos(1,2,3)",
        "box \"a\" pos, 1,2,3,)",
    };
    for (size_t i = 0; i < unchanged.size(); ++i) {
        const std::string name = "comma refusal " + std::to_string(i);
        CheckCase(unchanged[i], unchanged[i], {}, name.c_str());
    }
}

void TestDimensions() {
    const std::string raw =
        "box \"a\" pos(0,0,0) size(0,-1,0)\n"
        "box \"b\" pos(0,0,0) size(1,2)\n"
        "box \"c\" pos(0,0,0) size(1,2,3,4,)\n";
    const std::string expected =
        "box \"a\" pos(0,0,0) size(0,-1,0)\n"
        "box \"b\" pos(0,0,0) size(1,2)\n"
        "box \"c\" pos(0,0,0) size(1,2,3,4)\n";
    CheckCase(raw, expected, {kComma}, "invalid dimensions are not repaired");
}

void TestSemicolons() {
    CheckCase("box \"a\" pos(1,2,3);  # done\n", "box \"a\" pos(1,2,3)  # done\n", {kSemicolon},
              "semicolon before comment");
    CheckCase("box \"a\" pos(1,2,3);;", "box \"a\" pos(1,2,3);;", {},
              "repeated terminators collapse in one pass");
    CheckCase("room \"a\"\n;\nbox \"b\"\n", "room \"a\"\n;\nbox \"b\"\n", {},
              "isolated terminator line");
}

void TestNewlinesAndBom() {
    CheckCase("room \"a\"\r\nbox \"b\"\r\n", "room \"a\"\nbox \"b\"\n", {kNewlines},
              "crlf to lf");
    CheckCase("room \"a\"\rbox \"b\"", "room \"a\"\nbox \"b\"", {kNewlines}, "lone cr to lf");
    CheckCase(kBom + "room \"a\"", "room \"a\"", {kBomRemoved}, "leading bom removed");
    CheckCase(kBom + kBom + "room \"a\"", kBom + kBom + "room \"a\"", {},
              "repeated bom removed in one pass");
    CheckCase(kBom, "", {kBomRemoved}, "bom only");
    CheckCase("", "", {}, "empty input");
}

void TestBudget() {
    const std::string exact = kBom + std::string(65533, ' ');
    Check(exact.size() == 65536u, "exact budget fixture size");
    CheckCase(exact, std::string(65533, ' '), {kBomRemoved}, "exactly 65536 bytes processed");

    const std::string oversize = kBom + "```\nroom \"a\";\r\n```\n" + std::string(65536, 'x');
    Check(oversize.size() > 65536u, "oversize fixture size");
    CheckCase(oversize, oversize, {}, "input above the budget is returned unchanged");
}

} // namespace

int main() {
    TestFullPipeline();
    TestWhitespaceOnlyEnvelope();
    TestEnvelopeRefusals();
    TestQuotesAndComments();
    TestNumbers();
    TestCommaRefusals();
    TestDimensions();
    TestSemicolons();
    TestNewlinesAndBom();
    TestBudget();
    if (g_failures != 0) {
        std::fprintf(stderr, "scene_normalization_test: %d of %d checks failed\n", g_failures, g_checks);
        return 1;
    }
    std::fprintf(stdout, "scene_normalization_test: %d checks passed\n", g_checks);
    return 0;
}
