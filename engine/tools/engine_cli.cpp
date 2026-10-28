// Developer CLI for the dedicated E4B engine.
//   engine_cli parity --model <gguf> --golden <record.json> [--steps <n=16>]
//     Runs the CPU oracle on the golden's prompt tokens, compares last-position
//     hidden states per layer and the top logits, then teacher-forces the golden
//     greedy tokens and reports argmax agreement.
#include "core/e4b_spec.h"
#include "core/gguf.h"
#include "ref/forward_ref.h"
#if defined(LIMINAL_ENGINE_HAVE_MLX)
#include "mlx/mlx_backend.h"
#endif
#include "nlohmann/json.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <string>

using json = nlohmann::json;
using namespace liminal::engine;

namespace {

struct Similarity {
    double cosine;
    double rel_l2;
};

static Similarity Compare(const std::vector<float>& a, const std::vector<float>& b)
{
    double dot = 0, na = 0, nb = 0, diff = 0;
    for (size_t i = 0; i < a.size() && i < b.size(); ++i) {
        dot += static_cast<double>(a[i]) * b[i];
        na += static_cast<double>(a[i]) * a[i];
        nb += static_cast<double>(b[i]) * b[i];
        diff += (static_cast<double>(a[i]) - b[i]) * (static_cast<double>(a[i]) - b[i]);
    }
    return {dot / std::sqrt(na * nb + 1e-30), std::sqrt(diff / (nb + 1e-30))};
}

static int Argmax(const std::vector<float>& logits)
{
    return static_cast<int>(std::max_element(logits.begin(), logits.end()) - logits.begin());
}

static double Seconds(std::chrono::steady_clock::time_point start)
{
    return std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
}

// Same comparison on the MLX device: logits and greedy agreement plus throughput.
static int ParityMlx(const std::string& model_path, const std::string& golden_path, int steps)
{
#if defined(LIMINAL_ENGINE_HAVE_MLX)
    GgufFile file;
    std::string error;
    E4bModel model;
    if (!GgufOpen(model_path, &file, &error) || !E4bBind(file, &model, &error)) { fprintf(stderr, "%s\n", error.c_str()); return 1; }
    auto start = std::chrono::steady_clock::now();
    MlxModel* mlx = MlxLoad(model, &error);
    if (!mlx) { fprintf(stderr, "%s\n", error.c_str()); return 1; }
    printf("upload: %.2f s\n", Seconds(start));
    std::ifstream in(golden_path, std::ios::binary);
    const json golden = json::parse(in);
    const auto tokens = golden["tokens"].get<std::vector<int32_t>>();
    const auto& greedy = golden["greedy"]["tokens"];
    const auto& tops = golden["greedy"]["top"];
    std::vector<float> logits;
    int agree = 0, compared = 0, worst_step = -1;
    double worst_gap = 0.0, decode_seconds = 0.0;
    for (int pass = 0; pass < 2; ++pass) {  // the first pass warms Metal pipelines
        MlxState* state = MlxStateCreate(mlx, static_cast<int>(tokens.size()) + steps + 8);
        start = std::chrono::steady_clock::now();
        for (size_t offset = 0; offset < tokens.size(); offset += 512) {
            const int count = static_cast<int>(std::min<size_t>(512, tokens.size() - offset));
            if (!MlxForward(mlx, state, tokens.data() + offset, count, &logits, &error)) { fprintf(stderr, "%s\n", error.c_str()); return 1; }
        }
        const double prefill = Seconds(start);
        agree = compared = 0;
        decode_seconds = 0.0;
        worst_gap = 0.0;
        for (int step = 0; step < steps && step < static_cast<int>(greedy.size()); ++step) {
            const auto& top = tops[static_cast<size_t>(step)];
            const int native = Argmax(logits);
            ++compared;
            if (native == top[0][0].get<int>()) ++agree;
            else if (pass == 1) printf("  step %d: native %d vs llama %d (llama margin %.3f)\n", step, native, top[0][0].get<int>(),
                top[0][1].get<double>() - top[1][1].get<double>());
            for (const auto& entry : top) {
                const double gap = std::fabs(logits[static_cast<size_t>(entry[0].get<int>())] - entry[1].get<double>());
                if (gap > worst_gap) { worst_gap = gap; worst_step = step; }
            }
            const int32_t forced = greedy[static_cast<size_t>(step)].get<int>();
            start = std::chrono::steady_clock::now();
            if (!MlxForward(mlx, state, &forced, 1, &logits, &error)) { fprintf(stderr, "%s\n", error.c_str()); return 1; }
            decode_seconds += Seconds(start);
        }
        if (pass == 1) printf("prefill %zu tokens: %.3f s (%.0f tok/s), decode %.1f tok/s\n", tokens.size(), prefill,
            tokens.size() / prefill, compared / decode_seconds);
        MlxStateFree(state);
    }
    printf("argmax agreement %d/%d, worst top-logit gap %.3f (step %d)\n", agree, compared, worst_gap, worst_step);
    MlxFree(mlx);
    GgufClose(&file);
    return agree * 1000 >= compared * 995 ? 0 : 2;
#else
    (void)model_path; (void)golden_path; (void)steps;
    fprintf(stderr, "MLX is not compiled in\n");
    return 1;
#endif
}

static int Parity(const std::string& model_path, const std::string& golden_path, int steps)
{
    GgufFile file;
    std::string error;
    E4bModel model;
    if (!GgufOpen(model_path, &file, &error) || !E4bBind(file, &model, &error)) { fprintf(stderr, "%s\n", error.c_str()); return 1; }
    std::ifstream in(golden_path, std::ios::binary);
    const json golden = json::parse(in);
    const auto tokens = golden["tokens"].get<std::vector<int32_t>>();
    RefState state;
    RefDump dump;
    std::vector<float> logits;
    auto start = std::chrono::steady_clock::now();
    RefForward(model, &state, tokens.data(), static_cast<int>(tokens.size()), &logits, &dump);
    printf("prefill %zu tokens: %.1f s\n", tokens.size(), Seconds(start));

    double worst = 1.0;
    std::string worst_name;
    if (golden.contains("layers")) {
        for (auto it = golden["layers"].begin(); it != golden["layers"].end(); ++it) {
            const auto found = dump.find(it.key());
            if (found == dump.end()) continue;
            const Similarity s = Compare(found->second, it.value().get<std::vector<float>>());
            // llama.cpp keeps an f16 KV cache, the oracle f32: the bar is set on layer outputs.
            if (it.key().rfind("l_out-", 0) == 0 && s.cosine < worst) { worst = s.cosine; worst_name = it.key(); }
            if (it.key().rfind("l_out-", 0) == 0 || it.key() == "inp_scaled" || it.key() == "result_norm" || s.cosine < 0.9999)
                printf("  %-24s cos %.6f  rel_l2 %.2e\n", it.key().c_str(), s.cosine, s.rel_l2);
        }
        printf("worst layer output: %s cos %.6f\n", worst_name.c_str(), worst);
    }

    const auto& greedy = golden["greedy"]["tokens"];
    const auto& tops = golden["greedy"]["top"];
    int agree = 0, compared = 0;
    for (int step = 0; step < steps && step < static_cast<int>(greedy.size()); ++step) {
        const int expected = greedy[static_cast<size_t>(step)].get<int>();
        const int native = Argmax(logits);
        const auto& top = tops[static_cast<size_t>(step)];
        const double llama_top = top[0][1].get<double>();
        const double llama_second = top.size() > 1 ? top[1][1].get<double>() : llama_top;
        ++compared;
        if (native == top[0][0].get<int>()) ++agree;
        else printf("  step %d: native %d (%.3f) vs llama %d (%.3f), llama margin %.3f\n", step, native,
            logits[static_cast<size_t>(native)], top[0][0].get<int>(), llama_top, llama_top - llama_second);
        if (step == 0) printf("  step 0 logit[%d]: native %.4f llama %.4f\n", top[0][0].get<int>(),
            logits[static_cast<size_t>(top[0][0].get<int>())], llama_top);
        const int32_t forced = expected;
        start = std::chrono::steady_clock::now();
        RefForward(model, &state, &forced, 1, &logits);
        if (step == 0) printf("  decode step: %.2f s\n", Seconds(start));
    }
    printf("argmax agreement %d/%d\n", agree, compared);
    GgufClose(&file);
    return worst >= 0.999 && agree == compared ? 0 : 2;
}

}  // namespace

int main(int argc, char** argv)
{
    if (argc < 2) { fprintf(stderr, "usage: engine_cli parity --model <gguf> --golden <json> [--steps n] [--device cpu-ref|mlx]\n"); return 2; }
    const std::string command = argv[1];
    std::string model, golden, device = "cpu-ref";
    int steps = 16;
    for (int i = 2; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--model" && i + 1 < argc) model = argv[++i];
        else if (arg == "--golden" && i + 1 < argc) golden = argv[++i];
        else if (arg == "--steps" && i + 1 < argc) steps = atoi(argv[++i]);
        else if (arg == "--device" && i + 1 < argc) device = argv[++i];
    }
    if (command == "parity" && !model.empty() && !golden.empty())
        return device == "mlx" ? ParityMlx(model, golden, steps) : Parity(model, golden, steps);
    fprintf(stderr, "unknown command or missing arguments\n");
    return 2;
}
