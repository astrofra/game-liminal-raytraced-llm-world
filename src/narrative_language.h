#ifndef LIMINAL_NARRATIVE_LANGUAGE_H
#define LIMINAL_NARRATIVE_LANGUAGE_H

#include "game_state.h"
#include <string>
#include <map>

namespace liminal {
// System-level instruction, independent of the language of commands or memories.
std::string NarrativeLanguageInstruction(GameLanguage language);
// Conservative lexical guard: uncertain/short text is accepted, not certified.
bool IsClearlyWrongNarrativeLanguage(const std::string& text, GameLanguage expected);
const char* LocalizedDirectionShortcut(CardinalDirection direction, GameLanguage language);
// Remove free-prose compass claims and optionally append the engine's actual routes.
std::string ConstrainArrivalDirections(const std::string& text,GameLanguage language,
    const std::map<std::string,bool>& exits,CardinalDirection return_direction,bool append_routes);
}
#endif
