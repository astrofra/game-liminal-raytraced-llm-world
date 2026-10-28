# Audit statique — obstacles au portage Apple Silicon / MLX (MAC-001)

- Date : 2026-10-28 · Branche `codex/macos-audit` · Base `c6b0467281f13606a1376f5024c2b0a51585a16d`
- Portée : lecture seule des fichiers autorisés. **Aucun test, build ni shell exécuté.**
- Statut livraison : **À vérifier** (Codex tranche interfaces, build et implémentation).
- Convention : **[F]** fait constatable dans le fichier cité, **[H]** hypothèse à valider par build/run.

## Obstacles

1. **[F] `CMakeLists.txt` — options backend.** `LIMINAL_ENABLE_LLAMA_CUDA` vaut `ON` par défaut et force `GGML_CUDA` `ON` ; aucune option `GGML_METAL`, `GGML_ACCELERATE` ni MLX. Aucune branche Apple de liens/frameworks (seul `if(WIN32)` lie `shell32 bcrypt`).
   **[H]** Un configure macOS échouera faute de CUDA Toolkit, ou retombera silencieusement en CPU. Préserver la granularité des options et le repli CPU explicite (`--llm-gpu-layers 0`).

2. **[F] `scripts/build_native.py` — outillage Windows.** `compiler_environment()` code en dur `cl.exe`, `Hostx64/x64`, `Windows Kits/10`, `ProgramFiles` ; `main()` impose `Ninja Multi-Config`, `CMAKE_CUDA_ARCHITECTURES`, `CMAKE_CUDA_FLAGS=--use-local-env`, et des artefacts `.exe`.
   **[H]** Le script est inutilisable sur macOS ; un chemin de build Apple distinct (Codex) est requis, pas une adaptation de celui-ci.

3. **[F] `src/resource_paths.h` — `ResourceRoot()`.** Branche `__APPLE__` présente via `_NSGetExecutablePath`, puis recherche ascendante bornée à 4 niveaux du marqueur `assets/scenes/eryx_quarry_threshold.scene`.
   **[H]** Dans un bundle `.app`, l'exécutable est dans `Contents/MacOS/` et les ressources iraient dans `Contents/Resources/` : aucun candidat ne teste ce dossier avant la borne, donc `ResourceRoot()` retomberait sur `Contents/MacOS/`. **[H]** `_NSGetExecutablePath` n'est pas résolu (`realpath`), un lancement par lien symbolique peut fausser la base. Contrat à garder : résolution indépendante du cwd.

4. **[F] `src/research_archive.cpp` — `DefaultRoot()`.** Ordre : `LOCALAPPDATA` → `XDG_DATA_HOME` → `HOME/.local/share`. Aucune branche `__APPLE__`.
   **[H]** Sur macOS les archives atterrissent dans `~/.local/share/within-the-latent-walls/research`, hors convention `~/Library/Application Support`. Non bloquant fonctionnellement ; à décider avant signature/notarisation. Contrat à garder : l'archive n'est jamais rechargée automatiquement.

5. **[F] `src/llm_runtime.cpp` — vérification du modèle.** `ChatCompletionInternal` : `BCrypt*` sous `_WIN32`, sinon `sha256/sha256.c` vendoré (`sha256_t`, `SHA256_DIGEST_SIZE`), comparaison au `model_sha256` du manifeste.
   **[H]** La branche non-Windows compile sous Apple clang (`sha256.c` inclus dans un namespace anonyme) avec l'include `${LIMINAL_LLAMA_CPP_DIR}/vendor` déjà posé par la cible. Contrat à garder : SHA-256 exact et refus `model_corrupt`.

6. **[F] `src/llm_runtime.cpp` — placement GPU/CPU.** `model_params.n_gpu_layers = config.n_gpu_layers` (défaut `-1`), `offload_kqv = true`, `flash_attn_type` activé par défaut, threads via `ResolveThreadCount` (`hardware_concurrency`).
   **[H]** Sur Apple Silicon ces valeurs supposent un backend Metal compilé ; `hardware_concurrency` mélange cœurs performance et efficacité, et le support flash-attn/offload KQV en Metal n'est pas garanti à budget constant. Contrat à garder : repli CPU par configuration, aucune modification du modèle résident.

7. **[F] `src/llm_runtime.cpp` — `InitializeLlmRuntime()`.** Le callback `llama_log_set` remplit `offloaded_layers` / `total_layers` en analysant le texte `"offloaded %d/%d layers"`.
   **[H]** Un backend Metal/MLX formulera autrement : métriques à `-1`, sans effet sur la génération mais régression d'observabilité (`LIMINAL_LLM_METRIC`).

8. **[F] `src/llm_runtime.cpp` — contrat d'inférence.** `common_chat_templates_init(model, "")` avec `use_jinja = true`, `enable_thinking = false`, `reasoning_format = COMMON_REASONING_FORMAT_DEEPSEEK`, `common_tokenize` + `AppendTokenPiece`, `json_schema_to_grammar` + `common_sampler`, garde `GuardFinalChannel` et rejet des `tool_calls`.
   **[H]** Un portage MLX doit retrouver le **tokenizer et le template du GGUF** (pas une réimplémentation), une décodage contraint équivalent au JSON Schema, et le masquage des canaux privés. Aucun parseur de langage naturel en C++ ne doit être ajouté : le LLM interprète, le C++ valide et exécute.

9. **[F] `src/llm_runtime.cpp` — annulation et délais.** `GenerationTrace::expired()`/`loading_alive()` sont vérifiés entre les chunks de prompt, entre les tokens, et via `abort_callback` ; verrou pris par `try_lock_for(20 ms)` ; contexte, KV, sampler recréés à chaque appel (`n_seq_max = 1`).
   **[H]** L'annulation reste coopérative : un `llama_decode` long ou `llama_init_from_model` sur Metal peut dépasser le `timeout_ms` (180 s par défaut) avant le prochain point de contrôle. Contrats à garder : modèle résident, contexte neuf, appel sérialisé, annulation et délai bornés.

10. **[F] `src/main.cpp` — entrée et chemins.** La conversion UTF-8 de `argv` (CommandLineToArgvW) est réservée à `_WIN32` ; `run_sdl = argc == 1` ; `scene_path` et `output_path` par défaut restent relatifs au cwd pour le rendu diagnostique.
    **[H]** Sur APFS les noms sont normalisés NFD alors que les chaînes du dépôt sont NFC : un chemin accentué peut ne pas s'ouvrir. Contrat à garder : sorties diagnostiques relatives assumées, ressources du jeu indépendantes du cwd.

## Contrats à préserver (rappel)

- Tokenizer et template exacts du modèle livré ; budget compté avec `CountChatPromptTokens` sur le couple réellement chargé.
- JSON final contraint par schéma converti en grammaire, validé objet après nettoyage.
- Annulation coopérative et délai borné à chaque étape (attente de verrou, hachage, chargement, prompt, décodage, validation).
- Modèle résident entre les appels, contexte/KV/sampler neufs à chaque requête, appels sérialisés.
- Canaux privés (raisonnement, outils) jamais publiés ; préfixe du flux final immuable.
- Chemins de ressources indépendants du cwd ; archives de recherche hors runtime de jeu.
- État autoritatif et navigation en C++ ; le LLM ne fournit que des propositions structurées validées.

## Limites

Audit purement statique sur les dix lectures autorisées. Aucune conclusion sur `src/sdl_frontend.cpp`, `src/file_io.h`, `src/llm_channels.h`, le tree `vendor/llama.cpp` ou `scripts/models.json`, non lus ici. Toute mesure (build macOS, backend Metal/MLX, temps de chargement) reste à produire par Codex.
