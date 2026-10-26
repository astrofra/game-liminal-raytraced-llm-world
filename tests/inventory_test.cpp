#include <cstdio>
#include <string>
#include <vector>

#include "../src/inventory.h"

namespace {

int g_checks = 0;
int g_failures = 0;

void Fail(int line, const std::string& message) {
    ++g_failures;
    std::printf("FAIL line %d: %s\n", line, message.c_str());
}

void ExpectTrue(bool value, const char* expression, int line) {
    ++g_checks;
    if (!value) {
        Fail(line, std::string("expected true: ") + expression);
    }
}

void ExpectFalse(bool value, const char* expression, int line) {
    ++g_checks;
    if (value) {
        Fail(line, std::string("expected false: ") + expression);
    }
}

void ExpectString(
    const std::string& actual,
    const std::string& expected,
    const char* expression,
    int line) {
    ++g_checks;
    if (actual != expected) {
        Fail(line,
             std::string(expression) + " = \"" + actual + "\", expected \"" + expected + "\"");
    }
}

void ExpectParse(
    const char* text,
    liminal::InventoryCommand command,
    const char* item_id,
    int line) {
    ++g_checks;
    const liminal::InventoryRequest request = liminal::ParseInventoryRequest(text);
    if (request.command != command || request.item_id != item_id) {
        Fail(line,
             std::string("ParseInventoryRequest(\"") + text + "\") = command " +
                 std::to_string(static_cast<int>(request.command)) + ", id \"" + request.item_id +
                 "\"; expected command " + std::to_string(static_cast<int>(command)) + ", id \"" +
                 item_id + "\"");
    }
}

#define EXPECT_TRUE(expression) ExpectTrue((expression), #expression, __LINE__)
#define EXPECT_FALSE(expression) ExpectFalse((expression), #expression, __LINE__)
#define EXPECT_STRING(actual, expected) ExpectString((actual), (expected), #actual, __LINE__)
#define EXPECT_PARSE(text, command, item_id) \
    ExpectParse((text), liminal::InventoryCommand::command, (item_id), __LINE__)

const int kLanguageCount = 6;

const liminal::GameLanguage kLanguages[kLanguageCount] = {
    liminal::kGameLanguageEnglish,
    liminal::kGameLanguageFrench,
    liminal::kGameLanguageNorwegian,
    liminal::kGameLanguageDanish,
    liminal::kGameLanguageGerman,
    liminal::kGameLanguageItalian,
};

struct ItemCase {
    const char* id;
    const char* names[kLanguageCount];
};

const ItemCase kItemCases[] = {
    {
        "survey map slate",
        {
            "survey map slate",
            "tablette cartographique",
            "kartbrett",
            "kort",
            "Kartentafel",
            "tavoletta cartografica",
        },
    },
    {
        "boundary chalk",
        {
            "boundary chalk",
            "craie de délimitation",
            "kritt",
            "kridt",
            "Kreide",
            "gesso",
        },
    },
    {
        "sample tongs",
        {
            "sample tongs",
            "pinces à échantillons",
            "prøvetang",
            "prøvetang",
            "Probenzange",
            "pinze per campioni",
        },
    },
};

void TestCatalog() {
    for (const ItemCase& item : kItemCases) {
        EXPECT_TRUE(liminal::IsKnownInventoryItem(item.id));
    }
    EXPECT_FALSE(liminal::IsKnownInventoryItem("craie"));
    EXPECT_FALSE(liminal::IsKnownInventoryItem("map"));
    EXPECT_FALSE(liminal::IsKnownInventoryItem(""));
    EXPECT_FALSE(liminal::IsKnownInventoryItem("survey map slate "));
    EXPECT_FALSE(liminal::IsKnownInventoryItem("Survey Map Slate"));
    EXPECT_FALSE(liminal::IsKnownInventoryItem("lantern"));
}

void TestNames() {
    for (const ItemCase& item : kItemCases) {
        for (int language = 0; language < kLanguageCount; ++language) {
            EXPECT_STRING(liminal::InventoryItemName(item.id, kLanguages[language]), item.names[language]);
        }
    }
    EXPECT_STRING(
        liminal::InventoryItemName("unknown artifact", liminal::kGameLanguageFrench),
        "unknown artifact");
    EXPECT_STRING(
        liminal::InventoryItemName("survey map slate", static_cast<liminal::GameLanguage>(-1)),
        "survey map slate");
    EXPECT_STRING(
        liminal::InventoryItemName("survey map slate", static_cast<liminal::GameLanguage>(42)),
        "survey map slate");
}

void TestDescriptions() {
    for (const ItemCase& item : kItemCases) {
        for (int language = 0; language < kLanguageCount; ++language) {
            EXPECT_FALSE(
                liminal::InventoryItemDescription(item.id, kLanguages[language]).empty());
        }
    }
    EXPECT_TRUE(
        liminal::InventoryItemDescription("survey map slate", liminal::kGameLanguageFrench)
            .find("cartographie") != std::string::npos);
    EXPECT_TRUE(
        liminal::InventoryItemDescription("boundary chalk", liminal::kGameLanguageFrench)
            .find("marque") != std::string::npos);
    EXPECT_TRUE(
        liminal::InventoryItemDescription("sample tongs", liminal::kGameLanguageFrench)
            .find("échantillons") != std::string::npos);
    EXPECT_STRING(
        liminal::InventoryItemDescription("unknown artifact", liminal::kGameLanguageFrench),
        "Aucune description établie pour cet objet.");
    EXPECT_STRING(
        liminal::InventoryItemDescription("unknown artifact", liminal::kGameLanguageEnglish),
        "No description has been established for this item.");
    EXPECT_STRING(
        liminal::InventoryItemDescription(
            "unknown artifact", static_cast<liminal::GameLanguage>(-4)),
        "No description has been established for this item.");
}

void TestFormatting() {
    const std::vector<std::string> no_items;
    EXPECT_STRING(
        liminal::FormatInventory(no_items, liminal::kGameLanguageFrench),
        "Vous ne portez rien.");
    EXPECT_STRING(
        liminal::FormatInventory(no_items, liminal::kGameLanguageEnglish),
        "You are not carrying anything.");
    EXPECT_STRING(
        liminal::FormatInventory(no_items, liminal::kGameLanguageGerman),
        "Sie tragen nichts bei sich.");
    EXPECT_STRING(
        liminal::FormatInventory(no_items, liminal::kGameLanguageNorwegian),
        "Du bærer ingenting.");
    EXPECT_STRING(
        liminal::FormatInventory(no_items, liminal::kGameLanguageDanish),
        "Du bærer ingenting.");
    EXPECT_STRING(
        liminal::FormatInventory(no_items, liminal::kGameLanguageItalian),
        "Non avete nulla con voi.");

    const std::vector<std::string> blank_items = {"", "", ""};
    EXPECT_STRING(
        liminal::FormatInventory(blank_items, liminal::kGameLanguageFrench),
        "Vous ne portez rien.");

    std::vector<std::string> items = {
        "survey map slate",
        "boundary chalk",
        "survey map slate",
        "",
        "unknown artifact",
    };
    const std::vector<std::string> original = items;
    EXPECT_STRING(
        liminal::FormatInventory(items, liminal::kGameLanguageFrench),
        "Objets portés :\n"
        "- tablette cartographique\n"
        "- craie de délimitation\n"
        "- unknown artifact");
    EXPECT_TRUE(items == original);
    EXPECT_STRING(
        liminal::FormatInventory(items, liminal::kGameLanguageEnglish),
        "Carried items:\n"
        "- survey map slate\n"
        "- boundary chalk\n"
        "- unknown artifact");
    EXPECT_STRING(
        liminal::FormatInventory(items, static_cast<liminal::GameLanguage>(99)),
        "Carried items:\n"
        "- survey map slate\n"
        "- boundary chalk\n"
        "- unknown artifact");

    const std::vector<std::string> reordered = {
        "sample tongs",
        "survey map slate",
        "boundary chalk",
    };
    EXPECT_STRING(
        liminal::FormatInventory(reordered, liminal::kGameLanguageItalian),
        "Oggetti trasportati:\n"
        "- pinze per campioni\n"
        "- tavoletta cartografica\n"
        "- gesso");
    const std::vector<std::string> norwegian_item = {"kritt"};
    const std::vector<std::string> danish_item = {"kridt"};
    EXPECT_STRING(
        liminal::FormatInventory(norwegian_item, liminal::kGameLanguageNorwegian),
        "Du har med deg:\n"
        "- kritt");
    EXPECT_STRING(
        liminal::FormatInventory(danish_item, liminal::kGameLanguageDanish),
        "Du har med dig:\n"
        "- kridt");
}

void TestListCommands() {
    const char* const phrases[] = {
        "i",
        "inventaire",
        "inventory",
        "inventar",
        "inventario",
        "mon inventaire",
        "consulter inventaire",
        "consulter l inventaire",
        "afficher inventaire",
        "afficher l inventaire",
        "qu est ce que je porte",
        "que porte je",
        "que transportez vous",
        "que transporte je",
        "what am i carrying",
        "what do i carry",
        "hva har jeg med meg",
        "hvad har jeg med mig",
        "was trage ich bei mir",
        "cosa porto con me",
    };
    for (const char* phrase : phrases) {
        EXPECT_PARSE(phrase, List, "");
    }
    EXPECT_PARSE("utiliser inventaire", Use, "");
    EXPECT_PARSE("inventaire secret du lieu", None, "");
    EXPECT_PARSE("inspecter inventaire", None, "");
    EXPECT_PARSE("mon inventaire secret", None, "");
}

void TestInspect() {
    EXPECT_PARSE("examiner la craie", Inspect, "boundary chalk");
    EXPECT_PARSE("regarder la carte", Inspect, "survey map slate");
    EXPECT_PARSE("inspecter les pinces", Inspect, "sample tongs");
    EXPECT_PARSE("inspect the chalk", Inspect, "boundary chalk");
    EXPECT_PARSE("examine my map", Inspect, "survey map slate");
    EXPECT_PARSE("look at the slate", Inspect, "survey map slate");
    EXPECT_PARSE("untersuche die kreide", Inspect, "boundary chalk");
    EXPECT_PARSE("betrachten die karte", Inspect, "survey map slate");
    EXPECT_PARSE("se pa la craie", Inspect, "boundary chalk");
    EXPECT_PARSE("undersok kritt", Inspect, "boundary chalk");
    EXPECT_PARSE("undersog kridt", Inspect, "boundary chalk");
    EXPECT_PARSE("esamina il gesso", Inspect, "boundary chalk");
    EXPECT_PARSE("guarda la mappa", Inspect, "survey map slate");
    EXPECT_PARSE("guarda la tavoletta", Inspect, "survey map slate");
    EXPECT_PARSE("examiner la tablette cartographique", Inspect, "survey map slate");
}

void TestTakeDropUse() {
    EXPECT_PARSE("prendre la craie", Take, "boundary chalk");
    EXPECT_PARSE("ramasser les pinces", Take, "sample tongs");
    EXPECT_PARSE("take the chalk", Take, "boundary chalk");
    EXPECT_PARSE("pick up the tongs", Take, "sample tongs");
    EXPECT_PARSE("nehmen die kreide", Take, "boundary chalk");
    EXPECT_PARSE("ta kritt", Take, "boundary chalk");
    EXPECT_PARSE("tag kridt", Take, "boundary chalk");
    EXPECT_PARSE("prendi il gesso", Take, "boundary chalk");
    EXPECT_PARSE("raccogli le pinze", Take, "sample tongs");

    EXPECT_PARSE("poser la craie", Drop, "boundary chalk");
    EXPECT_PARSE("deposer la carte", Drop, "survey map slate");
    EXPECT_PARSE("jeter les pinces", Unsupported, "sample tongs");
    EXPECT_PARSE("donner la craie", Unsupported, "boundary chalk");
    EXPECT_PARSE("drop the chalk", Drop, "boundary chalk");
    EXPECT_PARSE("discard my map", Unsupported, "survey map slate");
    EXPECT_PARSE("give the tongs", Unsupported, "sample tongs");
    EXPECT_PARSE("ablegen die kreide", Drop, "boundary chalk");
    EXPECT_PARSE("wegwerfen die karte", Unsupported, "survey map slate");
    EXPECT_PARSE("legg fra deg kritt", Drop, "boundary chalk");
    EXPECT_PARSE("laeg kridt", Drop, "boundary chalk");
    EXPECT_PARSE("slip gesso", Drop, "boundary chalk");
    EXPECT_PARSE("posa la mappa", Drop, "survey map slate");
    EXPECT_PARSE("lascia le pinze", Drop, "sample tongs");
    EXPECT_PARSE("getta il gesso", Unsupported, "boundary chalk");

    EXPECT_PARSE("utiliser la craie", Use, "boundary chalk");
    EXPECT_PARSE("use the map", Use, "survey map slate");
    EXPECT_PARSE("benutzen die karte", Use, "survey map slate");
    EXPECT_PARSE("bruk kritt", Use, "boundary chalk");
    EXPECT_PARSE("brug kridt", Use, "boundary chalk");
    EXPECT_PARSE("usa il gesso", Use, "boundary chalk");
    EXPECT_PARSE("utilizza le pinze", Use, "sample tongs");
}

void TestArticlesAndPoliteness() {
    EXPECT_PARSE("je veux prendre la craie", Take, "boundary chalk");
    EXPECT_PARSE("je voudrais examiner la carte", Inspect, "survey map slate");
    EXPECT_PARSE("i want to take the chalk", Take, "boundary chalk");
    EXPECT_PARSE("i would like to drop the tongs", Drop, "sample tongs");
    EXPECT_PARSE("je veux mon inventaire", List, "");
    EXPECT_PARSE("examiner la craie", Inspect, "boundary chalk");
    EXPECT_PARSE("prendre les pinces", Take, "sample tongs");
    EXPECT_PARSE("regarder la tablette", Inspect, "survey map slate");
    EXPECT_PARSE("examiner le gesso", Inspect, "boundary chalk");
    EXPECT_PARSE("prendre l carte", Take, "survey map slate");
    EXPECT_PARSE("prendre ma craie", Take, "boundary chalk");
    EXPECT_PARSE("examiner mes pinces", Inspect, "sample tongs");
    EXPECT_PARSE("prendre mon kritt", Take, "boundary chalk");
    EXPECT_PARSE("nehmen meine kreide", Take, "boundary chalk");
    EXPECT_PARSE("examiner lo gesso", Inspect, "boundary chalk");
    EXPECT_PARSE("utiliser ma tablette", Use, "survey map slate");
    EXPECT_PARSE("drop my chalk", Drop, "boundary chalk");
    EXPECT_PARSE("look at the map slate", Inspect, "survey map slate");
    EXPECT_PARSE("examiner der karte", Inspect, "survey map slate");
    EXPECT_PARSE("examiner das kartentafel", Inspect, "survey map slate");
    EXPECT_PARSE("examiner den karte", Inspect, "survey map slate");
    EXPECT_PARSE("examiner det kritt", Inspect, "boundary chalk");
    EXPECT_PARSE("examiner min kritt", Inspect, "boundary chalk");
    EXPECT_PARSE("examiner mit karte", Inspect, "survey map slate");
    EXPECT_PARSE("esamina gli gesso", Inspect, "boundary chalk");
    EXPECT_PARSE("esamina mio gesso", Inspect, "boundary chalk");
    EXPECT_PARSE("esamina mia mappa", Inspect, "survey map slate");
}

void TestAliases() {
    struct AliasCase {
        const char* alias;
        const char* id;
    };
    const AliasCase aliases[] = {
        {"tablette", "survey map slate"},
        {"carte", "survey map slate"},
        {"tablette cartographique", "survey map slate"},
        {"carte topographique", "survey map slate"},
        {"slate", "survey map slate"},
        {"map", "survey map slate"},
        {"map slate", "survey map slate"},
        {"survey map slate", "survey map slate"},
        {"kartbrett", "survey map slate"},
        {"kart", "survey map slate"},
        {"kort", "survey map slate"},
        {"kartentafel", "survey map slate"},
        {"karte", "survey map slate"},
        {"tavoletta", "survey map slate"},
        {"mappa", "survey map slate"},
        {"tavoletta cartografica", "survey map slate"},
        {"craie", "boundary chalk"},
        {"craie de delimitation", "boundary chalk"},
        {"chalk", "boundary chalk"},
        {"boundary chalk", "boundary chalk"},
        {"kritt", "boundary chalk"},
        {"kridt", "boundary chalk"},
        {"kreide", "boundary chalk"},
        {"gesso", "boundary chalk"},
        {"pinces", "sample tongs"},
        {"pince", "sample tongs"},
        {"pinces a echantillons", "sample tongs"},
        {"tongs", "sample tongs"},
        {"sample tongs", "sample tongs"},
        {"provetang", "sample tongs"},
        {"probenzange", "sample tongs"},
        {"pinze", "sample tongs"},
        {"pinze per campioni", "sample tongs"},
    };
    for (const AliasCase& alias : aliases) {
        EXPECT_PARSE(alias.alias, Inspect, alias.id);
    }
    EXPECT_PARSE("la craie", Inspect, "boundary chalk");
    EXPECT_PARSE("les pinces", Inspect, "sample tongs");
    EXPECT_PARSE("the map", Inspect, "survey map slate");
    EXPECT_PARSE("my tongs", Inspect, "sample tongs");
}

void TestAmbiguity() {
    EXPECT_PARSE("examiner", Ambiguous, "");
    EXPECT_PARSE("examiner la", Ambiguous, "");
    EXPECT_PARSE("regarder", None, "");
    EXPECT_PARSE("inspect the chalk and the tongs", Ambiguous, "");
    EXPECT_PARSE("examiner la craie et les pinces", Ambiguous, "");
    EXPECT_PARSE("examiner le gesso et la mappa", Ambiguous, "");
    EXPECT_PARSE("examine the chalk, the tongs", Ambiguous, "");
    EXPECT_PARSE("examiner la craie et le ciel", None, "");
    EXPECT_PARSE("examiner le ciel et la craie", None, "");
}

void TestNonCommands() {
    EXPECT_PARSE("regarder le ciel", None, "");
    EXPECT_PARSE("regarder la craie dans le ciel", None, "");
    EXPECT_PARSE("examiner le ciel", None, "");
    EXPECT_PARSE("examine the sky", None, "");
    EXPECT_PARSE("aller est", None, "");
    EXPECT_PARSE("nord", None, "");
    EXPECT_PARSE("j ai trouve une cle", None, "");
    EXPECT_PARSE("j ai une craie", None, "");
    EXPECT_PARSE("je possede une craie", None, "");
    EXPECT_PARSE("je prends la craie", Take, "boundary chalk");
    EXPECT_PARSE("prends la craie", Take, "boundary chalk");
    EXPECT_PARSE("regarde la craie", Inspect, "boundary chalk");
    EXPECT_PARSE("regarde", None, "");
    EXPECT_PARSE("guarda", None, "");
    EXPECT_PARSE("utilise les pinces", Use, "sample tongs");
    EXPECT_PARSE("marquer ici a la craie", Use, "");
    EXPECT_PARSE("qu est ce que je transporte", List, "");
    EXPECT_PARSE("prendre le portail", Take, "");
    EXPECT_PARSE("prendre la craie avec les pinces", Take, "");
    EXPECT_PARSE("prendre la craie poser la craie", Take, "");
    EXPECT_PARSE("prendre la craie et poser la craie", Take, "");
    EXPECT_PARSE("utiliser la craie et prendre les pinces", Use, "");
    EXPECT_PARSE("prendre", Take, "");
    EXPECT_PARSE("prendre la", Take, "");
    EXPECT_PARSE("poser", Drop, "");
    EXPECT_PARSE("jeter", Unsupported, "");
    EXPECT_PARSE("utiliser", Use, "");
    EXPECT_PARSE("use", Use, "");
    EXPECT_PARSE("tag", Take, "");
    EXPECT_PARSE("ta", Take, "");
    EXPECT_PARSE("", None, "");
    EXPECT_PARSE("   ", None, "");
}

}  // namespace

int main() {
    TestCatalog();
    TestNames();
    TestDescriptions();
    TestFormatting();
    TestListCommands();
    TestInspect();
    TestTakeDropUse();
    TestArticlesAndPoliteness();
    TestAliases();
    TestAmbiguity();
    TestNonCommands();

    std::printf("%d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
