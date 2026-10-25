#ifndef LIMINAL_SCENE_NORMALIZATION_H
#define LIMINAL_SCENE_NORMALIZATION_H

#include <string>
#include <vector>

namespace liminal {

// Conservative syntax-only repair of a scene program. No parsing, no geometry
// edit: each applied transformation is reported once in `changes`.
// Stable change codes, in transformation order:
// "bom_removed", "newlines_normalized", "markdown_envelope_removed",
// "terminal_semicolon_removed", "trailing_list_comma_removed".
struct SceneNormalization {
    std::string text;
    std::vector<std::string> changes;
};

// Inputs above the 65536-byte scene budget are returned unchanged. The caller
// verifies EOS before calling; this utility has no stream knowledge.
SceneNormalization NormalizeSceneSyntax(const std::string& raw);

} // namespace liminal
#endif
