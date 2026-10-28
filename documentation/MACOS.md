# macOS Apple Silicon — MLX C++ autonome

Le backend retenu est **MLX C++ / Metal**, directement dans le jeu. llama.cpp/Metal
reste une alternative native. Aucun Python, worker, serveur, Homebrew ou dépôt
n'est nécessaire sur le Mac du joueur. Le bundle embarque modèle, tokenizer,
ressources, bibliothèques et shaders. macOS 14 et Apple Silicon sont requis.

## Construire et lancer

Sur le Mac de développement seulement : Xcode Command Line Tools, Python 3.11+,
CMake et Ninja ; libomp est optionnel pour le raytracer CPU parallèle.
Avec Homebrew déjà installé :

```sh
brew install cmake ninja libomp
python3 scripts/setup_macos.py
python3 scripts/build_macos.py --test --publish
./play_within_the_latent_walls.command
```

Ouvrir **`output/macos-mlx-cpp/Within the Latent Walls.app`** (environ 5 Go).
Le dossier `.app` complet peut être copié ailleurs, par exemple dans Applications.
Le lanceur préfère ce bundle, puis l'alternative llama.cpp, puis un binaire de build.
Chaque lancement crée une nouvelle partie. Les archives restent dans
`~/Library/Application Support/WithinTheLatentWalls/research/`.

Le setup crée `.venv-mlx-cpp` avec MLX **0.32.4**, uniquement pour obtenir le SDK
C++ lors du build. Cet environnement et Python ne sont pas embarqués. Le moteur
reste C++17 ; seul le module utilisant les en-têtes MLX est compilé en C++20.
Le modèle Safetensors 4 bits est épinglé dans `scripts/models_mlx_cpp.json` :
`mlx-community/gemma-4-e4b-it-4bit`, révision
`475b9088d29754a3379866cf5aeb6b41acd313c2`.

Un petit GGUF de vocabulaire est extrait à la préparation depuis l'artefact GGUF
épinglé de `scripts/models.json`. Seules ses métadonnées sont conservées, aucun
tenseur GGUF. Le tokenizer extrait a sa propre taille et son propre SHA-256
attendus. Le GGUF complet de 5,3 Go est donc une source de build ; il n'est pas
copié dans l'application MLX. Le premier setup peut télécharger les deux formats.
Prévoir **25 Go libres**, en plus du dépôt, pour modèles, SDK, build et assemblage.
Les téléchargements temporaires sont vérifiés avant renommage. `--skip-model`
prépare uniquement le SDK ; publier exige toujours les poids et le tokenizer vérifiés.

## Livraison autonome

La publication assemble un nouveau bundle à part, copie les dylibs non système
récursivement, réécrit leurs chemins et supprime les rpaths de développement.
Elle refuse les dépendances manquantes et les binaires sans arm64. Le modèle copié
est vérifié à nouveau ; notices et provenance sont incluses. `mlx.metallib` est
embarquée et son chemin configuré avant la première initialisation GPU.

Le bundle est signé **ad hoc**, puis vérifié avant remplacement d'une publication
antérieure. Il n'est ni signé Developer ID ni notarialisé. Aucune publication,
installation globale ou transmission des poids n'est effectuée par ces scripts.
`--skip-build --publish` reconditionne un binaire dont le hash et le backend
correspondent à la provenance enregistrée ; sinon un vrai build est exigé.

## Backend alternatif et référence historique

Pour construire l'alternative **llama.cpp / Metal**, également sans Python :

```sh
python3 scripts/setup_macos.py --backend native
python3 scripts/build_macos.py --backend native --test --publish
open 'output/macos-native/Within the Latent Walls.app'
```

Cette application contient Gemma 4 E4B Q4_K_M, vérifié par SHA-256 avec CommonCrypto.
Ses shaders Metal sont incorporés au binaire. Le guide et la recette intermédiaires
sont conservés dans la [revue MAC-005](duo/reviews/MAC-005.md).

La référence **Python/MLX**, dépendante du dépôt, reste disponible explicitement :

```sh
python3.12 scripts/setup_macos.py --backend mlx-python
python3 scripts/build_macos.py --backend mlx-python --test --publish
```

Elle utilise `.venv-mlx`, `models/mlx/` et `output/macos/`. Les trois variantes ont
des builds et des publications distincts. La version initiale n'est pas écrasée.
Un même seed ne garantit pas la même prose entre quantifications et samplers.

## Contrats et qualification

Le C++ charge les poids résidents, crée un cache KV neuf à chaque requête,
utilise le template Gemma sans réflexion, contraint le JSON et contrôle les canaux
publiés. Les bornes et la sémantique du jeu restent validées par le moteur.
L'annulation est coopérative entre blocs de prefill et entre tokens. Le modèle
n'est jamais téléchargé au runtime. Seul le profil Gemma E4B épinglé est supporté.

llama.cpp fournit au backend MLX natif le tokenizer, le template et la grammaire ;
les calculs du réseau passent par MLX. `--llama-info` affiche donc les deux
bibliothèques présentes et précise **MLX C++ / no Python runtime**. L'option
`--llm-gpu-layers` est réservée au backend d'inférence llama.cpp et refusée par MLX.
Le duo Copilot/Ollama sert uniquement au développement et reste indépendant.

```sh
build_macos_mlx_cpp/liminal_cornell_renderer --llama-info
ctest --test-dir build_macos_mlx_cpp --output-on-failure
python3 -m unittest discover -s scripts -p 'test_*.py'
python3 -m unittest discover -s tests -p 'test_*.py'
python3 scripts/verify_mlx_cpp_runtime.py --exe build_macos_mlx_cpp/liminal_cornell_renderer --out /tmp/liminal-mlx-cpp-recette
```

Sur la machine de recette (macOS 14.1, 18 Go), une narration réelle prend à chaud
**16,3 s en MLX C++**, contre **22,6 s en llama.cpp** et **15,1 s en Python/MLX**.
Les réponses font respectivement 279, 299 et 262 tokens : ce ne sont pas des
sorties identiques. Le JSON court prend 0,86 / 1,16 / 0,84 s. MLX C++ est le plus
rapide des deux backends C++ mesurés ; ces résultats ne garantissent pas une
égalité stricte de latence sur chaque demande. Voir la [revue MAC-006](duo/reviews/MAC-006.md).

Windows conserve llama.cpp/CUDA et ses scripts ; aucun vendor modifié. Il n'a pas
été recompilé sur ce Mac. Les [essais MLX historiques](duo/reviews/MAC-002.md)
et les [tests du duo](duo/reviews/MAC-007.md) restent consultables.
