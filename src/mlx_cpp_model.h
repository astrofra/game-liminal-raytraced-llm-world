#pragma once
#include <memory>
#include <string>
#include <vector>

namespace liminal {
// Native implementation of the single pinned Gemma 4 E4B affine 4-bit profile.
// The runtime verifies all artifacts before construction. Calls are serialized.
// MLX headers stay behind this interface; the game remains C++17.
class MlxCppModel {
  public:
    MlxCppModel(const std::string &weights, const std::string &metallib);
    ~MlxCppModel();
    void reset();
    // Prefill requests evaluate only KV state; decode also returns float logits.
    std::vector<float> step(const std::vector<int> &tokens, bool logits);

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace liminal
