#ifndef LIMINAL_NARRATIVE_LANGUAGE_H
#define LIMINAL_NARRATIVE_LANGUAGE_H

#include "game_state.h"
#include <string>

namespace liminal {
// System-level instruction, independent of the language of commands or memories.
std::string NarrativeLanguageInstruction(GameLanguage language);
// Conservative lexical guard: uncertain/short text is accepted, not certified.
bool IsClearlyWrongNarrativeLanguage(const std::string& text, GameLanguage expected);
const char* LocalizedDirectionShortcut(CardinalDirection direction, GameLanguage language);
}
#endif
