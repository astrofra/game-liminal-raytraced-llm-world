#include "inventory.h"

#include <cstddef>
#include <string>
#include <vector>

namespace liminal {
namespace {

const int kLanguageCount = 6;

int LanguageIndex(GameLanguage language) {
    const int index = static_cast<int>(language);
    if (index < 0 || index >= kLanguageCount) {
        return 0;
    }
    return index;
}

struct InventoryEntry {
    const char* id;
    const char* names[kLanguageCount];
    const char* descriptions[kLanguageCount];
};

const InventoryEntry kInventoryCatalog[] = {
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
        {
            "A flat slate used as a support for consulting cartography notes.",
            "Une tablette destinée à vos relevés de cartographie.",
            "Et kartbrett som støtte for oppmålingsnotater.",
            "En korttavle som støtte for opmålingsnotater.",
            "Eine Kartentafel als Unterlage für Kartografie und Vermessungsnotizen.",
            "Una tavoletta cartografica come supporto per consultare appunti di rilevamento.",
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
        {
            "A piece of chalk intended for marking survey boundaries.",
            "Une craie destinée à marquer des repères pendant la prospection.",
            "Et krittstykke beregnet på å merke grenser under kartlegging.",
            "Et stykke kridt beregnet til at markere grænser under opmåling.",
            "Kreide zum Markieren von Grenzen bei der Erkundung.",
            "Un gesso destinato a segnare punti di riferimento durante la ricognizione.",
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
        {
            "Tongs intended for handling small samples at a distance from your gloves.",
            "Des pinces pour manipuler de petits échantillons en gardant vos gants à distance.",
            "En prøvetang beregnet på å håndtere små prøver uten direkte kontakt med hanskene.",
            "En prøvetang beregnet til at håndtere små prøver uden direkte kontakt med handskerne.",
            "Eine Zange zum Handhaben kleiner Proben mit Abstand zu Ihren Handschuhen.",
            "Pinze destinate a manipolare piccoli campioni a distanza dai guanti.",
        },
    },
};

const char* const kInventoryHeaders[kLanguageCount] = {
    "Carried items:",
    "Objets portés :",
    "Du har med deg:",
    "Du har med dig:",
    "Getragene Gegenstände:",
    "Oggetti trasportati:",
};

const char* const kInventoryEmptyPhrases[kLanguageCount] = {
    "You are not carrying anything.",
    "Vous ne portez rien.",
    "Du bærer ingenting.",
    "Du bærer ingenting.",
    "Sie tragen nichts bei sich.",
    "Non avete nulla con voi.",
};

const char* const kUnknownDescriptions[kLanguageCount] = {
    "No description has been established for this item.",
    "Aucune description établie pour cet objet.",
    "Ingen beskrivelse er etablert for denne gjenstanden.",
    "Der er endnu ingen beskrivelse af denne genstand.",
    "Für diesen Gegenstand ist keine Beschreibung festgelegt.",
    "Nessuna descrizione è stata stabilita per questo oggetto.",
};

struct InventoryAlias {
    const char* alias;
    const char* id;
};

const InventoryAlias kInventoryAliases[] = {
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

const char* const kListPhrases[] = {
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
    "qu est ce que je transporte",
    "qu ai je sur moi",
    "what am i carrying",
    "what do i carry",
    "hva har jeg med meg",
    "hvad har jeg med mig",
    "was trage ich bei mir",
    "cosa porto con me",
};

struct InventoryVerb {
    const char* prefix;
    InventoryCommand kind;
};

const InventoryVerb kInventoryVerbs[] = {
    {"examiner", InventoryCommand::Inspect},
    {"regarder", InventoryCommand::Inspect},
    {"regarde", InventoryCommand::Inspect},
    {"inspecter", InventoryCommand::Inspect},
    {"inspect", InventoryCommand::Inspect},
    {"examine", InventoryCommand::Inspect},
    {"look at", InventoryCommand::Inspect},
    {"untersuche", InventoryCommand::Inspect},
    {"betrachten", InventoryCommand::Inspect},
    {"se pa", InventoryCommand::Inspect},
    {"undersok", InventoryCommand::Inspect},
    {"undersog", InventoryCommand::Inspect},
    {"esamina", InventoryCommand::Inspect},
    {"guarda", InventoryCommand::Inspect},
    {"reprendre", InventoryCommand::Take},
    {"reprends", InventoryCommand::Take},
    {"emporter", InventoryCommand::Take},
    {"ramene", InventoryCommand::Take},
    {"collect", InventoryCommand::Take},
    {"retrieve", InventoryCommand::Take},
    {"set down", InventoryCommand::Drop},
    {"put down", InventoryCommand::Drop},
    {"prends", InventoryCommand::Take},
    {"ramasse", InventoryCommand::Take},
    {"prendre", InventoryCommand::Take},
    {"ramasser", InventoryCommand::Take},
    {"take", InventoryCommand::Take},
    {"pick up", InventoryCommand::Take},
    {"nehmen", InventoryCommand::Take},
    {"ta", InventoryCommand::Take},
    {"tag", InventoryCommand::Take},
    {"prendi", InventoryCommand::Take},
    {"raccogli", InventoryCommand::Take},
    {"pose", InventoryCommand::Drop},
    {"depose", InventoryCommand::Drop},
    {"jette", InventoryCommand::Unsupported},
    {"donne", InventoryCommand::Unsupported},
    {"poser", InventoryCommand::Drop},
    {"deposer", InventoryCommand::Drop},
    {"jeter", InventoryCommand::Unsupported},
    {"donner", InventoryCommand::Unsupported},
    {"drop", InventoryCommand::Drop},
    {"discard", InventoryCommand::Unsupported},
    {"give", InventoryCommand::Unsupported},
    {"ablegen", InventoryCommand::Drop},
    {"wegwerfen", InventoryCommand::Unsupported},
    {"legg fra deg", InventoryCommand::Drop},
    {"laeg", InventoryCommand::Drop},
    {"slip", InventoryCommand::Drop},
    {"posa", InventoryCommand::Drop},
    {"lascia", InventoryCommand::Drop},
    {"getta", InventoryCommand::Unsupported},
    {"utilise", InventoryCommand::Use},
    {"marquer", InventoryCommand::Use},
    {"tracer", InventoryCommand::Use},
    {"prelever", InventoryCommand::Use},
    {"utiliser", InventoryCommand::Use},
    {"use", InventoryCommand::Use},
    {"benutzen", InventoryCommand::Use},
    {"bruk", InventoryCommand::Use},
    {"brug", InventoryCommand::Use},
    {"usa", InventoryCommand::Use},
    {"utilizza", InventoryCommand::Use},
};

const char* const kArticles[] = {
    "la", "le", "les", "l", "ma", "mon", "mes",
    "the", "my",
    "der", "die", "das", "meine", "den", "det", "min", "mit",
    "il", "lo", "gli", "le", "mio", "mia",
};

const char* const kConjunctions[] = {
    " et ",
    " and ",
    " und ",
    " og ",
    " e ",
    ",",
};

const char* const kPolitenessPrefixes[] = {
    "i would like to",
    "je voudrais",
    "i want to",
    "je veux",
    "je",
};

std::string Trim(const std::string& text) {
    std::size_t begin = 0;
    while (begin < text.size() && text[begin] == ' ') {
        ++begin;
    }
    std::size_t end = text.size();
    while (end > begin && text[end - 1] == ' ') {
        --end;
    }
    return text.substr(begin, end - begin);
}

bool StartsWithWord(const std::string& text, const char* prefix) {
    const std::string word(prefix);
    if (text.compare(0, word.size(), word) != 0) {
        return false;
    }
    return text.size() == word.size() || text[word.size()] == ' ';
}

std::string StripLeadingArticle(const std::string& text) {
    const std::size_t space = text.find(' ');
    const std::string first =
        text.substr(0, space == std::string::npos ? text.size() : space);
    for (const char* article : kArticles) {
        if (first == article) {
            return space == std::string::npos ? std::string() : text.substr(space + 1);
        }
    }
    return text;
}

std::string StripPoliteness(const std::string& text) {
    for (const char* prefix : kPolitenessPrefixes) {
        if (StartsWithWord(text, prefix)) {
            return Trim(text.substr(std::string(prefix).size()));
        }
    }
    return text;
}

bool IsListPhrase(const std::string& text) {
    for (const char* phrase : kListPhrases) {
        if (text == phrase) {
            return true;
        }
    }
    return false;
}

bool ResolveInventoryItem(const std::string& text, std::string* id) {
    if (text.empty()) {
        return false;
    }
    for (const InventoryEntry& entry : kInventoryCatalog) {
        if (text == entry.id) {
            *id = entry.id;
            return true;
        }
    }
    for (const InventoryAlias& alias : kInventoryAliases) {
        if (text == alias.alias) {
            *id = alias.id;
            return true;
        }
    }
    return false;
}

bool AllSegmentsAreKnownItems(const std::string& text) {
    std::vector<std::string> segments;
    std::string current;
    std::size_t index = 0;
    while (index < text.size()) {
        bool split = false;
        for (const char* conjunction : kConjunctions) {
            const std::string separator(conjunction);
            if (text.compare(index, separator.size(), separator) == 0) {
                segments.push_back(current);
                current.clear();
                index += separator.size();
                split = true;
                break;
            }
        }
        if (!split) {
            current.push_back(text[index]);
            ++index;
        }
    }
    segments.push_back(current);
    if (segments.size() < 2) {
        return false;
    }
    for (const std::string& segment : segments) {
        const std::string candidate = Trim(StripLeadingArticle(Trim(segment)));
        std::string id;
        if (candidate.empty() || !ResolveInventoryItem(candidate, &id)) {
            return false;
        }
    }
    return true;
}

bool MatchVerb(const std::string& text, InventoryCommand* command, std::string* target) {
    bool matched = false;
    std::size_t best_length = 0;
    InventoryCommand best_command = InventoryCommand::None;
    for (const InventoryVerb& verb : kInventoryVerbs) {
        if (!StartsWithWord(text, verb.prefix)) {
            continue;
        }
        const std::size_t length = std::string(verb.prefix).size();
        if (matched && length <= best_length) {
            continue;
        }
        matched = true;
        best_length = length;
        best_command = verb.kind;
    }
    if (!matched) {
        return false;
    }
    *command = best_command;
    *target = Trim(text.substr(best_length));
    return true;
}

}  // namespace

bool IsKnownInventoryItem(const std::string& id) {
    for (const InventoryEntry& entry : kInventoryCatalog) {
        if (id == entry.id) {
            return true;
        }
    }
    return false;
}

std::string InventoryItemName(const std::string& id, GameLanguage language) {
    const int index = LanguageIndex(language);
    for (const InventoryEntry& entry : kInventoryCatalog) {
        if (id == entry.id) {
            return entry.names[index];
        }
    }
    return id;
}

std::string InventoryItemDescription(const std::string& id, GameLanguage language) {
    const int index = LanguageIndex(language);
    for (const InventoryEntry& entry : kInventoryCatalog) {
        if (id == entry.id) {
            return entry.descriptions[index];
        }
    }
    return kUnknownDescriptions[index];
}

std::string FormatInventory(const std::vector<std::string>& items, GameLanguage language) {
    const int index = LanguageIndex(language);
    std::vector<std::string> listed;
    std::string lines;
    for (const std::string& item : items) {
        if (Trim(item).empty()) {
            continue;
        }
        bool duplicate = false;
        for (const std::string& previous : listed) {
            if (previous == item) {
                duplicate = true;
                break;
            }
        }
        if (duplicate) {
            continue;
        }
        listed.push_back(item);
        if (!lines.empty()) {
            lines += "\n";
        }
        lines += "- " + InventoryItemName(item, language);
    }
    if (lines.empty()) {
        return kInventoryEmptyPhrases[index];
    }
    return std::string(kInventoryHeaders[index]) + "\n" + lines;
}

InventoryRequest ParseInventoryRequest(const std::string& normalized) {
    InventoryRequest request;
    const std::string text = StripPoliteness(Trim(normalized));
    if (text.empty()) {
        return request;
    }

    // Bare observation still belongs to the scene, not to the inventory.
    if(text=="regarder"||text=="regarde"||text=="guarda"||text=="se pa")return request;
    if (IsListPhrase(text)) {
        request.command = InventoryCommand::List;
        return request;
    }

    InventoryCommand verb_command = InventoryCommand::None;
    std::string remainder;
    if (MatchVerb(text, &verb_command, &remainder)) {
        const std::string target = StripLeadingArticle(remainder);
        request.command = verb_command;
        request.target = target;
        if (verb_command == InventoryCommand::Inspect) {
            std::string id;
            if (target.empty()) {
                request.command = InventoryCommand::Ambiguous;
            } else if (ResolveInventoryItem(target, &id)) {
                request.item_id = id;
            } else if (AllSegmentsAreKnownItems(target)) {
                request.command = InventoryCommand::Ambiguous;
            } else {
                request.command = InventoryCommand::None;
            }
            return request;
        }
        std::string id;
        if (ResolveInventoryItem(target, &id)) {
            request.item_id = id;
        }
        return request;
    }

    std::string id;
    if (ResolveInventoryItem(StripLeadingArticle(text), &id)) {
        request.command = InventoryCommand::Inspect;
        request.item_id = id;
    }
    return request;
}

}  // namespace liminal
