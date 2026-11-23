#ifndef LIMINAL_NARRATIVE_HEAT_H
#define LIMINAL_NARRATIVE_HEAT_H

#include <algorithm>
#include <cmath>

namespace liminal {

// The literary sampler is independent of the structured world proposal.
// At 38 C it joins the normal body-driven curve (0.26), then rises continuously.
inline float LiterarySamplingTemperature(float body_c, int ceiling = 5) {
    if (!std::isfinite(body_c)) body_c = 37.0f;
    const float body = std::clamp(body_c, 34.0f, 43.0f);
    if (body < 38.0f) return std::clamp(.10f + .16f * (body - 37.0f), .10f, .26f);
    const float heat = std::clamp((body - 38.0f) / 4.0f, 0.0f, 1.0f);
    return .26f + ((ceiling == 10 ? 10.0f : 5.0f) - .26f) * std::pow(heat, 1.5f);
}

inline const char* LiteraryHeatStage(float body_c) {
    if (body_c >= 42.0f) return "delirium";
    if (body_c >= 41.0f) return "fracture";
    if (body_c >= 40.0f) return "dislocation";
    if (body_c >= 39.0f) return "echo";
    if (body_c >= 38.0f) return "hesitation";
    return "clear";
}

inline const char* LiteraryHeatInstruction(float body_c) {
    if (body_c >= 42.0f)
        return "Thought is breaking apart. Echo and distort a concrete word from the perception, interrupt words, let a count lose its certainty. Use short broken clauses, not a long descriptive paragraph. Keep some words intact and recognizable.";
    if (body_c >= 41.0f)
        return "Thought repeatedly loses its thread. Use several incomplete clauses, an intrusive repetition and one uncertain count. At most one word may break off. Keep a recognizable sensory detail. Do not dissolve every word into noise.";
    if (body_c >= 40.0f)
        return "Attention falters repeatedly. Alternate short intact sentences with unfinished clauses. Return twice to the same concrete word with a small change of meaning. Break the rhythm, but keep words intact and the scene recognizable.";
    if (body_c >= 39.0f)
        return "A detail will not leave your attention. Repeat one concrete word or short phrase, then correct one small impression. Let a sentence hesitate. Keep complete, readable sentences and intact words; no broken syllables or delirious counting.";
    if (body_c >= 38.0f)
        return "A first doubt enters an otherwise lucid perception. Let one sentence pause and gently correct itself around a concrete detail. Keep the remaining prose fluid and precise, with intact sentences and words. No obsessive repetition, broken syllables or delirium yet.";
    return "Write clear, precise sensory prose with varied natural sentences.";
}

} // namespace liminal
#endif
