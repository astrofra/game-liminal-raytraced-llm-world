#pragma once
// MLX/Metal implementation of the E4B forward pass. Plain C++ interface: no MLX
// types leak to the engine core, which is built without MLX headers.
#include "core/e4b_spec.h"

#include <string>
#include <vector>

namespace liminal::engine {

struct MlxModel;
struct MlxState;

// Uploads every weight of the bound GGUF to the GPU (K-quant blocks unchanged).
MlxModel* MlxLoad(const E4bModel& model, std::string* error);
void MlxFree(MlxModel* model);
const char* MlxDeviceName(const MlxModel* model);

MlxState* MlxStateCreate(MlxModel* model, int capacity);
void MlxStateFree(MlxState* state);
int MlxStatePast(const MlxState* state);

// Evaluates n tokens after the cached ones; `logits` receives the last token's
// softcapped logits (vocab floats).
bool MlxForward(MlxModel* model, MlxState* state, const int32_t* tokens, int n,
    std::vector<float>* logits, std::string* error);

}  // namespace liminal::engine
