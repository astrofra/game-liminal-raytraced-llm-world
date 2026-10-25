// Standalone C++17 checks for the narrative language policy.
// Source encoding: UTF-8. Checks stay active in Release builds.

#include "narrative_language.h"

#include <cstddef>
#include <iostream>
#include <string>
#include <vector>

using namespace liminal;

namespace {

int g_failures = 0;

void Check(bool condition, int line, const char* expression) {
    if (!condition) {
        ++g_failures;
        std::cerr << "FAIL line " << line << ": " << expression << '\n';
    }
}

#define CHECK(expression) Check((expression), __LINE__, #expression)

const GameLanguage kAllLanguages[] = {
    kGameLanguageEnglish,
    kGameLanguageFrench,
    kGameLanguageNorwegian,
    kGameLanguageDanish,
    kGameLanguageGerman,
    kGameLanguageItalian,
};

void ExpectAccepted(const std::string& text, GameLanguage language,
                    const char* label, int line) {
    if (IsClearlyWrongNarrativeLanguage(text, language)) {
        ++g_failures;
        std::cerr << "FAIL line " << line << ": expected acceptance for " << label
                  << " (language " << static_cast<int>(language) << ")\n";
    }
}

void ExpectRejected(const std::string& text, GameLanguage language,
                    const char* label, int line) {
    if (!IsClearlyWrongNarrativeLanguage(text, language)) {
        ++g_failures;
        std::cerr << "FAIL line " << line << ": expected rejection for " << label
                  << " (language " << static_cast<int>(language) << ")\n";
    }
}

#define EXPECT_ACCEPTED(text, language, label) \
    ExpectAccepted((text), (language), (label), __LINE__)
#define EXPECT_REJECTED(text, language, label) \
    ExpectRejected((text), (language), (label), __LINE__)

void ExpectDirectionLabel(GameLanguage language, CardinalDirection direction,
                          const char* expected, int line) {
    const char* actual = LocalizedDirectionShortcut(direction, language);
    if (actual == nullptr) {
        ++g_failures;
        std::cerr << "FAIL line " << line << ": null direction label (language "
                  << static_cast<int>(language) << ", direction "
                  << static_cast<int>(direction) << ")\n";
        return;
    }
    if (std::string(actual) != expected) {
        ++g_failures;
        std::cerr << "FAIL line " << line << ": direction label mismatch (language "
                  << static_cast<int>(language) << ", direction "
                  << static_cast<int>(direction) << "): expected \"" << expected
                  << "\", got \"" << actual << "\"\n";
    }
}

#define EXPECT_DIRECTION_LABEL(language, direction, expected) \
    ExpectDirectionLabel((language), (direction), (expected), __LINE__)

// Neutral, natural paragraphs used only to check the conservative lexical guard.
const std::string kEnglishParagraph =
    "The corridor narrows as you advance, and the pale dust settles slowly on the "
    "metal floor. A low hum comes from the walls, steady enough to feel through your "
    "gloves. You keep your breathing calm, and you listen for any movement ahead in "
    "the dark.";  // 45 words

const std::string kFrenchParagraph =
    "Le couloir se resserre à mesure que vous avancez, et la poussière pâle se "
    "dépose lentement sur le sol métallique. Un bourdonnement sourd traverse les "
    "parois, assez régulier pour être senti à travers vos gants. Vous gardez une "
    "respiration calme et vous écoutez chaque mouvement devant vous dans "
    "l'obscurité.";  // 49 words

const std::string kNorwegianParagraph =
    "Korridoren smalner mens du beveger deg fremover, og det bleke støvet legger seg "
    "langsomt på det metalliske gulvet. En dyp summing kommer fra veggene, jevn nok "
    "til å kjennes gjennom hanskene dine. Du holder pusten rolig og lytter etter "
    "bevegelse foran deg i mørket.";  // 44 words

const std::string kDanishParagraph =
    "Korridoren bliver smalere, mens du bevæger dig fremad, og det blege støv lægger "
    "sig langsomt på det metalliske gulv. En dyb summen kommer fra væggene, jævn nok "
    "til at kunne mærkes gennem dine handsker. Du holder vejret roligt og lytter "
    "efter bevægelse foran dig i mørket.";  // 46 words

const std::string kGermanParagraph =
    "Der Gang wird enger, während Sie weitergehen, und der bleiche Staub legt sich "
    "langsam auf den metallischen Boden. Ein tiefes Summen kommt aus den Wänden, "
    "gleichmäßig genug, um es durch Ihre Handschuhe zu spüren. Sie halten den Atem "
    "ruhig und lauschen auf jede Bewegung vor Ihnen im Dunkeln.";  // 48 words

const std::string kItalianParagraph =
    "Il corridoio si restringe mentre avanzate, e la polvere pallida si deposita "
    "lentamente sul pavimento metallico. Un ronzio profondo proviene dalle pareti, "
    "abbastanza regolare da essere percepito attraverso i guanti. Mantenete il "
    "respiro calmo e ascoltate ogni movimento davanti a voi nell'oscurità.";  // 42 words

void TestNarrativeLanguageInstructions() {
    std::vector<std::string> instructions;
    for (GameLanguage language : kAllLanguages) {
        const std::string instruction = NarrativeLanguageInstruction(language);
        CHECK(!instruction.empty());
        instructions.push_back(instruction);
    }

    CHECK(instructions.size() == 6);
    for (std::size_t left = 0; left < instructions.size(); ++left) {
        for (std::size_t right = left + 1; right < instructions.size(); ++right) {
            CHECK(instructions[left] != instructions[right]);
        }
    }
}

void TestLocalizedDirectionShortcuts() {
    EXPECT_DIRECTION_LABEL(kGameLanguageEnglish, kDirectionNorth, "N");
    EXPECT_DIRECTION_LABEL(kGameLanguageEnglish, kDirectionEast, "E");
    EXPECT_DIRECTION_LABEL(kGameLanguageEnglish, kDirectionSouth, "S");
    EXPECT_DIRECTION_LABEL(kGameLanguageEnglish, kDirectionWest, "W");

    EXPECT_DIRECTION_LABEL(kGameLanguageFrench, kDirectionNorth, "N");
    EXPECT_DIRECTION_LABEL(kGameLanguageFrench, kDirectionEast, "E");
    EXPECT_DIRECTION_LABEL(kGameLanguageFrench, kDirectionSouth, "S");
    EXPECT_DIRECTION_LABEL(kGameLanguageFrench, kDirectionWest, "O");

    EXPECT_DIRECTION_LABEL(kGameLanguageNorwegian, kDirectionNorth, "N");
    EXPECT_DIRECTION_LABEL(kGameLanguageNorwegian, kDirectionEast, "Ø");
    EXPECT_DIRECTION_LABEL(kGameLanguageNorwegian, kDirectionSouth, "S");
    EXPECT_DIRECTION_LABEL(kGameLanguageNorwegian, kDirectionWest, "V");

    EXPECT_DIRECTION_LABEL(kGameLanguageDanish, kDirectionNorth, "N");
    EXPECT_DIRECTION_LABEL(kGameLanguageDanish, kDirectionEast, "Ø");
    EXPECT_DIRECTION_LABEL(kGameLanguageDanish, kDirectionSouth, "S");
    EXPECT_DIRECTION_LABEL(kGameLanguageDanish, kDirectionWest, "V");

    EXPECT_DIRECTION_LABEL(kGameLanguageGerman, kDirectionNorth, "N");
    EXPECT_DIRECTION_LABEL(kGameLanguageGerman, kDirectionEast, "O");
    EXPECT_DIRECTION_LABEL(kGameLanguageGerman, kDirectionSouth, "S");
    EXPECT_DIRECTION_LABEL(kGameLanguageGerman, kDirectionWest, "W");

    EXPECT_DIRECTION_LABEL(kGameLanguageItalian, kDirectionNorth, "N");
    EXPECT_DIRECTION_LABEL(kGameLanguageItalian, kDirectionEast, "E");
    EXPECT_DIRECTION_LABEL(kGameLanguageItalian, kDirectionSouth, "S");
    EXPECT_DIRECTION_LABEL(kGameLanguageItalian, kDirectionWest, "O");
}

void TestNaturalParagraphsInTheirOwnLanguage() {
    EXPECT_ACCEPTED(kEnglishParagraph, kGameLanguageEnglish, "English paragraph");
    EXPECT_ACCEPTED(kFrenchParagraph, kGameLanguageFrench, "French paragraph");
    EXPECT_ACCEPTED(kNorwegianParagraph, kGameLanguageNorwegian, "Norwegian paragraph");
    EXPECT_ACCEPTED(kDanishParagraph, kGameLanguageDanish, "Danish paragraph");
    EXPECT_ACCEPTED(kGermanParagraph, kGameLanguageGerman, "German paragraph");
    EXPECT_ACCEPTED(kItalianParagraph, kGameLanguageItalian, "Italian paragraph");
}

void TestClearlyWrongParagraphs() {
    EXPECT_REJECTED(kFrenchParagraph, kGameLanguageItalian,
                    "French paragraph for Italian");
    EXPECT_REJECTED(kFrenchParagraph, kGameLanguageEnglish,
                    "French paragraph for English");
    EXPECT_REJECTED(kItalianParagraph, kGameLanguageFrench,
                    "Italian paragraph for French");
    EXPECT_REJECTED(kEnglishParagraph, kGameLanguageItalian,
                    "English paragraph for Italian");
    EXPECT_REJECTED(kGermanParagraph, kGameLanguageFrench,
                    "German paragraph for French");
}

void TestConservativeAcceptanceForAmbiguousText() {
    const std::string samples[] = {
        "",
        "42",
        "464",
        "3.14",
        "Eryx",
        "Nyx",
        "K7",
    };
    for (GameLanguage language : kAllLanguages) {
        for (const std::string& sample : samples) {
            EXPECT_ACCEPTED(sample, language, "empty, numeric or ambiguous short text");
        }
    }
}

void TestItalianSessionRegression() {
    const std::string narration =
        "Vous tentez de progresser vers le sud, mais la masse angulaire de quartz vous "
        "arrête. Le silence de ce chantier planétaire est absolu, une pression lourde "
        "contre la coque de votre combinaison. Vous restez là, face à la froide "
        "immobilité de la roche.";
    EXPECT_REJECTED(narration, kGameLanguageItalian, "Italian session regression");
    EXPECT_ACCEPTED(narration, kGameLanguageFrench,
                    "Italian session regression in French");
}

}  // namespace

int main() {
    TestNarrativeLanguageInstructions();
    TestLocalizedDirectionShortcuts();
    TestNaturalParagraphsInTheirOwnLanguage();
    TestClearlyWrongParagraphs();
    TestConservativeAcceptanceForAmbiguousText();
    TestItalianSessionRegression();

    if (g_failures != 0) {
        std::cerr << g_failures << " narrative language check(s) failed\n";
        return 1;
    }
    std::cout << "All narrative language checks passed\n";
    return 0;
}
