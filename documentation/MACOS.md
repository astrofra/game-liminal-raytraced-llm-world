# macOS Apple Silicon / MLX

Portage sur la branche `macos-port`, réalisé le 28 octobre 2026. Le moteur reste
C++17/SDL3 ; MLX-LM exécute Gemma 4 E4B 4 bits sur le GPU Apple. Le raytracer reste
CPU, avec OpenMP si libomp est disponible. Le backend Windows llama.cpp/CUDA reste
sélectionnable et ses dépendances vendored ne sont pas modifiées.

## Installation et lancement

Prérequis : Mac Apple Silicon en mode natif (pas Rosetta), macOS 14 minimum,
Xcode Command Line Tools, Python 3.12, CMake et Ninja. Avec Homebrew déjà installé :

```sh
xcode-select --install  # seulement si les outils Xcode sont absents
brew install python@3.12 cmake ninja libomp
python3.12 scripts/setup_macos.py
python3 scripts/build_macos.py --test --publish
./play_within_the_latent_walls.command
```

Le setup crée `.venv-mlx/`, installe les versions de `requirements_mlx.txt`,
puis télécharge environ 5,2 Go dans `models/mlx/gemma-4-e4b-it-4bit/`. Ces fichiers
sont ignorés par Git. Prévoir environ 10 Go libres pour environnement, poids et
build, en plus du dépôt ; la machine de recette possède 18 Go de mémoire unifiée.
`--skip-model` prépare seulement l'environnement. Le téléchargement peut être
relancé : chaque fichier déjà valide est conservé. Un fichier incomplet est remplacé
seulement après contrôle de taille et de SHA-256 ; aucune authentification nécessaire.

Double-cliquer le lanceur `.command`, ou ouvrir
`output/macos/Within the Latent Walls.app`. Le build publie un **bundle local**,
signé ad hoc : les ressources et le worker sont copiés dans `Contents/Resources`,
mais Python, le modèle et les bibliothèques Homebrew restent sur ce Mac. Ce n'est
pas un paquet autonome à distribuer. Après déplacement du dépôt, refaire le build.
Aucune signature Developer ID, notarisation, publication ou installation globale
n'est effectuée par les scripts.

Une fois la préparation achevée, le jeu fonctionne hors ligne. Il n'utilise ni
Ollama, ni Copilot, ni un serveur HTTP. Le duo de développement est indépendant.

## Architecture et contrats

`LIMINAL_ENABLE_MLX=ON` est le défaut sur Apple ; le script de build impose arm64.
MLX et llama.cpp sont exclusifs. Pour un build llama.cpp explicite sur Mac :
`-DLIMINAL_ENABLE_MLX=OFF -DLIMINAL_ENABLE_LLAMA_CPP=ON -DLIMINAL_ENABLE_LLAMA_CUDA=OFF`.
Cette variante n'est pas la cible qualifiée de ce portage.

Le processus C++ démarre son worker Python par `posix_spawn`, avec un canal Unix
privé hérité via stdin/stdout, sans shell ni port réseau. Les appels sont sérialisés.
Le modèle vérifié et le tokenizer restent résidents ; cache KV, sampler et état de
grammaire sont recréés à chaque appel. Le jeu tue et récolte le worker en cas de
délai dépassé, annulation, rupture de protocole ou erreur ; le prochain appel peut
le relancer. La fermeture du jeu le récolte aussi. Une annulation pendant un long
prefill n'attend donc pas le prochain token, mais impose un rechargement ensuite.

Le tokenizer et le template proviennent de la même révision MLX que les poids.
Le comptage de contexte utilise le prompt exact, y compris le schéma JSON ajouté
aux messages et la désactivation de la réflexion. Un dépassement du budget est
refusé avant le décodage. Aucun historique implicite ne passe entre les requêtes.

Outlines Core masque les logits pour le JSON ; le masque utilise float32 pour
éviter une erreur Metal bfloat16 sur macOS 14. La table de tokens conserve tous
les alias et les octets de fallback de Gemma : l'adaptateur générique d'Outlines
écrasait des chiffres/punctuations partageant la même représentation. Les contrôles
de canaux privés C++ restent actifs avant toute publication de texte.

Les bornes numériques flottantes ne sont pas toutes imposées par le masque
Outlines : le schéma complet est aussi donné au modèle et **validé intégralement**
avec jsonschema avant qu'une réponse soit déclarée réussie. Les validations
mécaniques du moteur restent ensuite autoritatives. Une proposition invalide est
rejetée, jamais corrigée silencieusement ou exécutée comme si elle était valide.
Aucun parseur de langage naturel C++ ni modification des règles de jeu n'est ajouté.

Le manifeste `scripts/models_mlx.json` épingle
[`mlx-community/gemma-4-e4b-it-4bit`](https://huggingface.co/mlx-community/gemma-4-e4b-it-4bit/tree/475b9088d29754a3379866cf5aeb6b41acd313c2)
à `475b9088d29754a3379866cf5aeb6b41acd313c2`. Chaque fichier est vérifié avant
chargement. Si la taille/date d'un fichier résident change, une nouvelle
vérification précède son rechargement. Le runtime force les modes hors ligne
Hugging Face/Transformers et n'exécute aucun code distant du modèle.

## Chemins et diagnostics

Les archives se trouvent dans
`~/Library/Application Support/WithinTheLatentWalls/research/`.
Chaque lancement crée une nouvelle partie, sans recharger ces archives.
Les ressources sont trouvées depuis l'exécutable, même en lançant depuis `/tmp`
ou Finder ; les sorties diagnostiques explicites restent relatives au cwd.

```sh
python3 scripts/download_mlx.py --verify-only
ctest --test-dir build_macos --output-on-failure
python3 -m unittest discover -s scripts -p 'test_*.py'
python3 -m unittest discover -s tests -p 'test_*.py'
build_macos/liminal_cornell_renderer --llama-info
python3 scripts/verify_mlx_runtime.py --out /tmp/liminal-mlx-recette-nouvelle
```

`--llama-info` est le nom historique du diagnostic ; il affiche aussi MLX.
`--model` accepte le répertoire MLX de l'artefact épinglé ; il ne sélectionne pas
un autre modèle arbitraire. Les variables `LIMINAL_MLX_PYTHON` et
`LIMINAL_MLX_MODEL` permettent de déplacer les dépendances locales. Les chemins
compilés se règlent aussi par `LIMINAL_MLX_PYTHON` et `LIMINAL_MLX_MODEL_DIR` dans CMake.
`--llm-gpu-layers` est refusé par MLX : c'est une option llama.cpp.

Les poids MLX et GGUF sont deux quantifications différentes ; un même seed ne
promet pas la même prose ni le même labyrinthe entre plateformes.

## Qualification

Voir la [revue MAC-002](duo/reviews/MAC-002.md) pour les essais réellement observés,
les versions, les limites et les résultats. Les journaux complets restent hors
Git. L'audit et les tests bornés du duo ont leur propre revue :
[MAC-001](duo/reviews/MAC-001.md) et [MAC-003R1](duo/reviews/MAC-003R1.md).

Références d'implémentation : [MLX-LM](https://github.com/ml-explore/mlx-lm),
[Outlines Core](https://github.com/dottxt-ai/outlines-core).
