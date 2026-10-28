# Notices tierces du moteur LLM

Le moteur `engine/` est écrit pour Gemma 4 E4B et n'embarque plus llama.cpp,
mais une partie de son algorithmique en dérive ; sa licence MIT reste donc
livrée (`licenses/llama.cpp-LICENSE.txt`, copiée de llama.cpp `620d2225`).

| Composant | Licence | Usage |
|---|---|---|
| llama.cpp / ggml (`620d2225`) | MIT | Déquantification Q4_K/Q6_K (`ggml-quants.c`), GEMV `kernel_mul_mv_q4_K/q6_K` de ggml-metal portées en Metal (MLX) et en GLSL (Vulkan), algorithme du tokenizer, graphe Gemma 4 et règle d'espaces de `json-schema-to-grammar` reproduits. |
| xgrammar (`1c9556dd`, v0.2.5) | Apache-2.0 (`LICENSE`, `NOTICE` de la source) | Contrainte JSON Schema, liée statiquement. |
| picojson (dans xgrammar) | BSD-2-Clause (`licenses/picojson-LICENSE.txt`) | Analyse JSON interne d'xgrammar. |
| dlpack (dans xgrammar) | Apache-2.0 | En-têtes de tenseurs d'xgrammar. |
| MLX (`v0.32.0`, macOS seulement) | MIT | Calcul Metal du backend macOS. |
| nlohmann/json | MIT (`licenses/nlohmann-json-LICENSE.txt`) | JSON du runtime et des outils. |
| libinfer | — | Projet de l'auteur, source d'inspiration du backend MLX ; non lié. |

Les en-têtes Vulkan et `glslc` (Vulkan SDK) ne servent qu'à la compilation ;
le jeu charge à l'exécution le `vulkan-1.dll` du pilote graphique.
