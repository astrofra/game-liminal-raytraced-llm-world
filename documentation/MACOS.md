# macOS Apple Silicon — application C++ autonome

Le backend par défaut est **llama.cpp / Metal**, directement lié au jeu C++17.
Le `.app` embarque Gemma 4 E4B Q4_K_M, les ressources, SDL3, libomp et les shaders
Metal. Il fonctionne hors ligne, sans Python, Homebrew, dépôt, Ollama ou serveur
sur le Mac du joueur. Le raytracer reste CPU/OpenMP. macOS 14 et Apple Silicon
sont requis ; la recette locale est effectuée sous macOS 14.1 avec 18 Go de RAM.

## Construire et lancer

Les outils ci-dessous ne sont nécessaires que sur le Mac de développement :
Xcode Command Line Tools, Python 3.10+, CMake et Ninja. libomp est optionnel
pour le rendu parallèle. Avec Homebrew déjà installé :

```sh
brew install cmake ninja libomp
python3 scripts/setup_macos.py
python3 scripts/build_macos.py --test --publish
./play_within_the_latent_walls.command
```

Le setup natif utilise la bibliothèque standard Python, sans venv ni pip. Il
vérifie le modèle existant ou télécharge la révision épinglée dans
`scripts/models.json` : `ggml-org/gemma-4-E4B-it-GGUF`, révision
`51db8454ac67b58ed848e54d4111be2ad16ba4bf`, fichier Q4_K_M de 5 335 289 824 octets.
Le téléchargement temporaire est validé (taille, signature GGUF, SHA-256) avant
renommage. Un téléchargement interrompu doit être relancé. `--skip-model` évite
ce téléchargement ; la publication exige toujours un modèle complet et vérifié.
Prévoir au moins 20 Go libres, en plus du dépôt, pour poids, build et assemblage.

Ouvrir **`output/macos-native/Within the Latent Walls.app`**, ou copier ce dossier
`.app` complet ailleurs, par exemple dans Applications. Le lanceur `.command`
préfère ce bundle puis le binaire `build_macos_native` s'il n'a pas été publié.
Le bundle historique `output/macos/` n'est pas remplacé.

`--skip-build --publish` permet de reconditionner un binaire natif déjà compilé.
La publication vérifie le backend, copie récursivement les dylibs non système,
réécrit leurs chemins et supprime les rpaths de développement. Elle refuse les
bibliothèques manquantes et les binaires sans arm64. Notices et provenance sont
incluses ; le modèle copié est vérifié à nouveau. L'application est assemblée
à part, signée ad hoc et vérifiée avant de remplacer une publication antérieure.
Il n'y a ni signature Developer ID, ni notarisation, ni distribution automatique.

## Runtime et archives

`LIMINAL_ENABLE_MLX=OFF` est le défaut. Le script active llama.cpp, Metal,
Accelerate et l'incorporation des shaders, désactive CUDA et impose arm64.
Windows conserve son backend llama.cpp/CUDA et ses scripts ; aucun changement
n'est apporté au vendor. Windows n'a pas été recompilé sur ce Mac.

Tout le runtime est dans le processus C++ : poids résidents vérifiés par SHA-256,
contexte neuf par requête, template Gemma, grammaire JSON, streaming et annulation
coopérative. CommonCrypto accélère la vérification SHA-256 sur macOS. Les règles
de navigation, inventaire, enquête et validation des propositions restent celles
du moteur. Aucun parseur C++ de langage naturel n'est ajouté.

Les archives restent dans
`~/Library/Application Support/WithinTheLatentWalls/research/`.
Chaque lancement crée une nouvelle partie. Les ressources sont trouvées depuis
l'exécutable, indépendamment du dossier courant ; les chemins de sortie explicites
restent relatifs au dossier de lancement.

```sh
build_macos_native/liminal_cornell_renderer --llama-info
ctest --test-dir build_macos_native --output-on-failure
python3 -m unittest discover -s scripts -p 'test_*.py'
python3 -m unittest discover -s tests -p 'test_*.py'
python3 scripts/verify_native_llm.py --exe build_macos_native/liminal_cornell_renderer --out /tmp/liminal-native-recette
```

## Référence MLX historique

La version Python/MLX reste disponible pour les comparaisons de développement :

```sh
python3.12 scripts/setup_macos.py --backend mlx-python
python3 scripts/build_macos.py --backend mlx-python --test --publish
```

Elle utilise `.venv-mlx`, `models/mlx/` et le bundle local `output/macos/`, qui
reste dépendant du dépôt et de Homebrew. Ce n'est pas la livraison autonome.
MLX et llama.cpp sont exclusifs dans un build. Aucun backend MLX C++ n'est annoncé
comme implémenté ou mesuré. Les poids Safetensors 4 bits et GGUF Q4_K_M sont des
quantifications distinctes : le même seed ne garantit pas la même prose.

Les essais historiques MLX sont conservés dans la [revue MAC-002](duo/reviews/MAC-002.md).
La [revue MAC-005](duo/reviews/MAC-005.md) décrit la recette native, les mesures
comparatives et leurs limites. Les journaux complets et les poids restent hors Git.
