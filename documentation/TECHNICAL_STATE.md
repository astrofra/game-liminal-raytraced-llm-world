# Technical State

Derniere mise a jour : 2026-07-31

## Resume

Le depot contient actuellement un premier module de rendu natif en C++11 centre sur une Cornell Box en niveaux de gris.

Ce module constitue un bootstrap du futur sous-systeme de rendu. Il ne s'agit pas encore du renderer final du jeu, mais d'une premiere base executable, compilable et documentee.

Sur le plan architectural, la cible d'inference retenue est maintenant `llama.cpp` avec acceleration CUDA autour de `Ministral 3 8B`.

Le depot contient maintenant une premiere boucle reelle `commande -> LLM -> etat -> scene -> rendu`, disponible en mode headless et dans une premiere HMI `SDL3`.

## Verrous d'architecture actes

- runtime narratif local : `llama.cpp`
- acceleration principale de developpement : `CUDA`
- modele cible v1 : `Ministral 3 8B Instruct 2512`
- future couche multimedia multi-OS : `SDL3`

## Fonctionnalites presentes

- Build natif via CMake et Visual Studio 2022.
- Options CMake `LIMINAL_ENABLE_LLAMA_CPP`, `LIMINAL_ENABLE_LLAMA_CUDA`, `LIMINAL_ENABLE_OPENMP` et `LIMINAL_ENABLE_SDL3_FRONTEND` pour raccorder un `llama.cpp` vendorise, activer le parallelisme CPU si disponible et construire la boucle desktop interactive.
- Fallback `find_package(SDL3)` puis `FetchContent` automatique sur `SDL 3.4.12` si la bibliotheque n'est pas deja installee localement.
- Helper Windows `build_release.bat` a la racine pour configurer et compiler la version `Release`.
- Helper Windows `run_cornell_test.bat` a la racine pour compiler puis lancer le rendu de verification Cornell Box.
- Helper Windows `download_ministral.bat` pour telecharger le modele cible.
- Helper Windows `ask_ministral.bat` pour lancer une question libre contre le modele local via `llama-cli`.
- Helper Windows `play_desert_des_tokens.bat` pour lancer directement la premiere boucle SDL3.
- Executable CLI `liminal_cornell_renderer`.
- Option CLI `--llama-info` pour verifier la presence du runtime `llama.cpp`, le commit vendorise et la disponibilite de l'offload GPU.
- Option CLI `--sdl` pour lancer la premiere boucle desktop `SDL3`.
- Options CLI pour le noyau fonctionnel en preparation :
  - `--dump-turn-contract`
  - `--dump-scene-audit-prompt`
  - `--compile-location`
  - `--audit-scene-text`
- Preset de rendu par defaut en `800x400` pour la scene liminale, avec cadrage panorama.
- Chargement de scene generique via `--scene <path>` pour `.scene` ou `.obj`.
- Parseur de format de scene proprietaire v1.
- Chargement equivalent d'une scene `v1` depuis un bloc texte en memoire, sans passer obligatoirement par un fichier comme interface interne.
- Validation syntaxique de base pour `room`, `camera`, `spotlight`, `sky`, `plane`, `box` et les premiers `prefab_*`.
- Spot analytique attache a la camera, avec panneau parametrique, portee limitee et cone progressif.
- Fond proceduriel `sky` optionnel pour les rayons sans intersection :
  - zenith sombre
  - horizon plus clair
  - nadir sombre
  - grain fort
  - etoiles deterministes
- Premiere couche de prefabs expands en primitives simples :
  - `prefab_gate`
  - `prefab_rack`
  - `prefab_crate`
  - `prefab_cooling_unit`
- Conversion des primitives `plane` et `box` vers le backend triangle/BVH existant.
- Premiere scene liminale handcraftee dans `assets/scenes/liminal_service_corridor.scene`.
- Trois scenes canoniques de validation spatiale handcraftees :
  - `assets/scenes/datacenter_entry_gate.scene`
  - `assets/scenes/datacenter_server_aisles.scene`
  - `assets/scenes/datacenter_roof_watch.scene`
- Chargement d'un fichier OBJ triangule simple.
- Chargement d'un fichier MTL reduit en materiaux grayscale.
- Reconstruction locale de la lumiere de la Cornell Box a partir des metadonnees de la scene.
- Structure d'acceleration BVH sur triangles.
- Intersections rayon/AABB et rayon/triangle.
- Path tracing diffus simple avec :
  - echantillonnage direct de la lumiere
  - un petit nombre de rebonds diffus
  - roulette russe
  - clamp simple des contributions extremes
- Parallelisation optionnelle de la boucle de rendu par lignes via OpenMP, avec fallback mono-thread.
- Sortie image en `PNG` via `stb_image_write`.
- Sortie `PGM` legacy encore supportee selon l'extension du fichier.
- Mesure du temps de chargement et du temps de rendu.
- Arborescence `vendor/llama.cpp` ajoutee au depot comme base d'integration locale du runtime LLM.
- Script Python `scripts/download_ministral.py` pour telecharger et valider `Ministral 3 8B Instruct 2512` en GGUF `Q4_K_M`.
- Smoke test local `llama-cli` valide sur `Ministral 3 8B`, avec reponse effective a une question libre.
- Premier noyau fonctionnel pour la future boucle de tour :
  - structs `HardState`, `SoftState`, `SpatialState`, `TurnResult`
  - contrat de tour structure et prompt builder v1
  - prompt d'audit direct pour une sortie `.scene`
  - compilateur deterministe des trois lieux canoniques depuis `SpatialState`
- Premier runtime headless `Ministral` branche au moteur :
  - `GenerateChatCompletion()` via `llama.cpp`
  - callback de streaming token par token exploitable par l'HMI
  - parsing JSON tolerant aux fences Markdown
  - mode repair d'une sortie JSON mal formee
  - fallback no-op si la sortie reste inexploitable apres repair
  - application des deltas sur `HardState`, `SoftState` et `SpatialState`
  - deuxieme appel LLM separe pour l'audit `.scene`
  - rendu final depuis la voie deterministe `SpatialState -> Scene`
- Premier graphe de salles improvisees :
  - interception de `NORTH`, `EAST`, `SOUTH`, `WEST`
  - generation d'une nouvelle salle seulement a la premiere entree
  - cache persistant du `scene_text` et des liens cardinaux en session
  - retour possible vers une salle deja decouverte sans repasser par le LLM
  - coexistence entre lieux canoniques et lieux generes dans la meme session
- Premiere HMI `SDL3` desktop :
  - event loop non bloquante
  - worker thread dedie a l'inference et au raytracing
  - affichage du flux brut du modele pendant la generation du JSON
  - panneau de transcript
  - ligne de commande avec edition clavier et historique haut/bas
  - annulation best-effort via `Escape`
- Nouveau chemin de rendu memoire `RenderSceneToPixels()` pour alimenter directement une texture `SDL3`.
- Options CLI de boucle fonctionnelle :
  - `--run-turn`
  - `--run-session`
  - `--sdl`
  - `--model`
  - `--llm-temperature`
  - `--llm-predict`
  - `--use-json-grammar`
  - `--no-json-grammar`
  - `--command-file`
  - `--load-state`
  - `--save-state`
  - `--dump-session-state`
  - `--dump-session-history`
  - `--prefer-candidate-scene`
  - `--dump-raw-turn`

## Fichiers importants

- [../CMakeLists.txt](/C:/works/projects/game-liminal-raytraced-llm-world/CMakeLists.txt:1) : configuration du build.
- [../src/core.h](/C:/works/projects/game-liminal-raytraced-llm-world/src/core.h:1) : types de base, math, RNG, configuration de rendu.
- [../src/scene.h](/C:/works/projects/game-liminal-raytraced-llm-world/src/scene.h:1) : structures de scene et BVH.
- [../src/scene.cpp](/C:/works/projects/game-liminal-raytraced-llm-world/src/scene.cpp:1) : chargement `.scene` et `.obj`, parseur scene v1 fichier ou memoire, conversion des primitives, materiaux, lumiere Cornell, construction BVH.
- [../src/renderer.cpp](/C:/works/projects/game-liminal-raytraced-llm-world/src/renderer.cpp:1) : camera, spotlight analytique, intersections, visibilite, integrateur, export PNG/PGM.
- [../src/sdl_frontend.h](/C:/works/projects/game-liminal-raytraced-llm-world/src/sdl_frontend.h:1) : interface de la premiere boucle interactive `SDL3`.
- [../src/sdl_frontend.cpp](/C:/works/projects/game-liminal-raytraced-llm-world/src/sdl_frontend.cpp:1) : fenetre, transcript, saisie texte, worker thread et presentation streaming.
- [../src/main.cpp](/C:/works/projects/game-liminal-raytraced-llm-world/src/main.cpp:76) : point d'entree CLI, parsing des options, telemetrie basique et premieres commandes de debug fonctionnel.
- [../src/game_state.h](/C:/works/projects/game-liminal-raytraced-llm-world/src/game_state.h:1) : structures du monde, des deltas et du contrat de tour.
- [../src/game_state.cpp](/C:/works/projects/game-liminal-raytraced-llm-world/src/game_state.cpp:1) : enums, etats initiaux et resumes de debug.
- [../src/turn_contract.h](/C:/works/projects/game-liminal-raytraced-llm-world/src/turn_contract.h:1) : interface de generation des prompts structure et audit.
- [../src/turn_contract.cpp](/C:/works/projects/game-liminal-raytraced-llm-world/src/turn_contract.cpp:1) : texte du schema de sortie, brief spatial et prompts v1.
- [../src/scene_compiler.h](/C:/works/projects/game-liminal-raytraced-llm-world/src/scene_compiler.h:1) : interface de compilation d'un `SpatialState` vers une scene rendable.
- [../src/scene_compiler.cpp](/C:/works/projects/game-liminal-raytraced-llm-world/src/scene_compiler.cpp:1) : mapping des lieux canoniques et audit memoire d'une scene candidate.
- [../src/llm_runtime.h](/C:/works/projects/game-liminal-raytraced-llm-world/src/llm_runtime.h:1) : interface minimale du runtime `llama.cpp`.
- [../src/llm_runtime.cpp](/C:/works/projects/game-liminal-raytraced-llm-world/src/llm_runtime.cpp:1) : initialisation backend, generation texte `llama.cpp`, introspection GPU et configuration de l'inference.
- [../src/turn_runner.h](/C:/works/projects/game-liminal-raytraced-llm-world/src/turn_runner.h:1) : interface de la boucle headless d'un tour.
- [../src/turn_runner.cpp](/C:/works/projects/game-liminal-raytraced-llm-world/src/turn_runner.cpp:1) : parsing de la sortie LLM, application des deltas et audit scene separe.
- [../assets/cornell/cornell_box.obj](/C:/works/projects/game-liminal-raytraced-llm-world/assets/cornell/cornell_box.obj:1) : scene de reference vendorisee.
- [../assets/cornell/cornell_box.mtl](/C:/works/projects/game-liminal-raytraced-llm-world/assets/cornell/cornell_box.mtl:1) : materiaux de reference.
- [../assets/scenes/liminal_service_corridor.scene](/C:/works/projects/game-liminal-raytraced-llm-world/assets/scenes/liminal_service_corridor.scene:1) : premiere scene proprietaire liminale.
- [../assets/scenes/datacenter_entry_gate.scene](/C:/works/projects/game-liminal-raytraced-llm-world/assets/scenes/datacenter_entry_gate.scene:1) : fixture canonique du portail d'entree.
- [../assets/scenes/datacenter_server_aisles.scene](/C:/works/projects/game-liminal-raytraced-llm-world/assets/scenes/datacenter_server_aisles.scene:1) : fixture canonique des travees de serveurs.
- [../assets/scenes/datacenter_roof_watch.scene](/C:/works/projects/game-liminal-raytraced-llm-world/assets/scenes/datacenter_roof_watch.scene:1) : fixture canonique du toit / tour de ronde.
- [../scripts/download_ministral.py](/C:/works/projects/game-liminal-raytraced-llm-world/scripts/download_ministral.py:1) : telechargement et validation du modele cible.
- [SCENE_FORMAT_V1.md](./SCENE_FORMAT_V1.md) : description du format de scene implemente.
- [SPATIAL_VALIDATION_PLAN.md](./SPATIAL_VALIDATION_PLAN.md) : protocole de validation du lien entre brief narratif, texte, scene v1 et rendu.
- [FUNCTIONAL_PIPELINE_V1.md](./FUNCTIONAL_PIPELINE_V1.md) : cadrage de la future boucle fonctionnelle et de la generation de scene assistee par LLM.
- [LLAMA_CUDA_SPECS.md](./LLAMA_CUDA_SPECS.md) : procedure de build et de validation du runtime `llama.cpp` cible.
- [../vendor/llama.cpp/VENDORED_COMMIT.txt](/C:/works/projects/game-liminal-raytraced-llm-world/vendor/llama.cpp/VENDORED_COMMIT.txt:1) : commit exact de `llama.cpp` vendorise.
- [../vendor/stb/stb_image_write.h](/C:/works/projects/game-liminal-raytraced-llm-world/vendor/stb/stb_image_write.h:1) : sortie PNG vendorisee.
- [../vendor/legacy_rt2003/README.md](/C:/works/projects/game-liminal-raytraced-llm-world/vendor/legacy_rt2003/README.md:1) : provenance des idees reutilisees depuis le vieux projet.

## Commandes de build et d'execution

```powershell
cmake -S . -B build -G "Visual Studio 17 2022"
cmake --build build --config Release
.\build\Release\liminal_cornell_renderer.exe
```

Avec `llama.cpp` et CUDA :

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -DLIMINAL_ENABLE_LLAMA_CPP=ON -DLIMINAL_ENABLE_LLAMA_CUDA=ON
cmake --build build --config Release
.\build\Release\liminal_cornell_renderer.exe --llama-info
```

Le vendredi 31 juillet 2026, une recompilation locale avec ces options a confirme de nouveau la presence du toolkit CUDA pendant la configuration CMake (`CUDA Toolkit found`).

Avec OpenMP desactive explicitement :

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -DLIMINAL_ENABLE_OPENMP=OFF
cmake --build build --config Release
```

Exemple avec scene explicite :

```powershell
.\build\Release\liminal_cornell_renderer.exe --scene assets\scenes\liminal_service_corridor.scene
.\build\Release\liminal_cornell_renderer.exe --scene assets\cornell\cornell_box.obj
```

Exemples du noyau fonctionnel :

```powershell
.\build\Release\liminal_cornell_renderer.exe --dump-turn-contract --location roof_watch --command "observe the horizon"
.\build\Release\liminal_cornell_renderer.exe --dump-scene-audit-prompt --location server_aisles
.\build\Release\liminal_cornell_renderer.exe --compile-location gate --output output\compiled_gate.png
.\build\Release\liminal_cornell_renderer.exe --audit-scene-text assets\scenes\datacenter_roof_watch.scene --output output\audited_roof_watch.png
.\build\Release\liminal_cornell_renderer.exe --sdl --location gate --save-state output\sdl_session_state.json
.\build\Release\liminal_cornell_renderer.exe --run-turn --location roof_watch --command "observe the horizon" --dump-raw-turn --output output\turn_roof_watch.png
.\build\Release\liminal_cornell_renderer.exe --run-session --location roof_watch --command "observe the horizon" --command "inspect the cooling unit" --save-state output\session_state.json --output output\session.png
.\build\Release\liminal_cornell_renderer.exe --run-turn --load-state output\session_state.json --command "check the crate" --save-state output\session_state_2.json --output output\session_2.png
```

Helper Windows :

```bat
build_release.bat
run_cornell_test.bat
download_ministral.bat
ask_ministral.bat
play_desert_des_tokens.bat
```

Exemple de rendu :

```powershell
.\build\Release\liminal_cornell_renderer.exe --samples 32 --width 256 --height 256 --output output\cornell_box_32spp.png
```

## Parametres par defaut

- largeur : `800`
- hauteur : `400`
- echantillons par pixel : `16`
- rebonds diffus max : `3`
- echantillons de lumiere directe : `2`
- seed : `1337`
- exposition : `1.0`

## Resultats observes

### 2026-07-29

Contexte : build `Release` local sur la machine de travail actuelle.

- `256x256`, `16 spp`, `3 bounces` : environ `1575.66 ms`
- `256x256`, `32 spp`, `3 bounces` : environ `4327.92 ms`
- `256x256`, `64 spp`, `3 bounces` : environ `6224.51 ms`

### 2026-07-30

Contexte : build `Release` local sur la machine de travail actuelle, apres ajout du spotlight camera et de la sortie PNG.

- scene liminale, `256x256`, `32 spp`, `3 bounces` : environ `5430.59 ms`
- Cornell Box, `256x256`, `32 spp`, `3 bounces` : environ `4061.97 ms`
- scene liminale panorama, `800x400`, `32 spp`, `3 bounces` : environ `35748.60 ms`

### 2026-07-31

Contexte : build `Release` local sur la machine de travail actuelle, scenes canoniques de validation spatiale, `800x400`, `16 spp`, `3 bounces`.

- portail d'entree du datacenter avec prefab gate : environ `8331.42 ms`
- travees de serveurs avec prefabs : environ `23903.92 ms`
- toit / tour de ronde avec prefabs : environ `7797.88 ms`

### 2026-07-31 - Validation OpenMP

Contexte : scene `datacenter_server_aisles.scene`, `800x400`, `16 spp`, `3 bounces`, meme seed, build sans `llama.cpp`.

- build mono-thread `LIMINAL_ENABLE_OPENMP=OFF` : environ `24376.07 ms`
- build OpenMP `LIMINAL_ENABLE_OPENMP=ON`, `16` threads annonces : environ `2708.34 ms`
- image de sortie bit-identique entre les deux builds sur ce test

Ces chiffres sont seulement des reperes de travail. Ils ne constituent pas encore un benchmark stable.

### 2026-07-31 - Validation boucle headless LLM

Contexte : build `Release` local avec `llama.cpp` et CUDA, `Ministral 3 8B Instruct 2512`, scene finale compilee depuis `SpatialState`.

- `--run-turn --location roof_watch --command "observe the horizon"` :
  - prompt structure : `828` tokens
  - generation JSON : `365` tokens
  - inference JSON : environ `8797.61 ms`
  - audit `.scene` separe : scene candidate validee (`338` triangles, `29` materiaux)
  - rendu final compile depuis `SpatialState` : environ `829.42 ms`

Observation importante :

- l'encapsulation d'une scene multi-ligne dans le JSON de tour s'est revelee fragile
- un deuxieme appel LLM dedie a l'audit `.scene` s'est montre nettement plus stable
- la voie principale recommandee reste donc `JSON structure -> etat -> compilateur deterministe`

### 2026-07-31 - Validation session multi-tour et persistance

Contexte : build `Release` local avec `llama.cpp` et CUDA, session `roof_watch`, deux commandes successives puis reprise depuis JSON sauvegarde.

- `--run-session --command "observe the horizon" --command "inspect the cooling unit"` :
  - `2` tours executes
  - JSON du second tour degrade, fallback no-op applique
  - etat final sauvegarde dans `output\session_state_smoke.json`
- `--run-turn --load-state output\session_state_smoke.json --command "check the crate"` :
  - reprise de session effective
  - historique conserve
  - nouvel etat sauvegarde dans `output\session_state_smoke_2.json`

Observation importante :

- la session survit maintenant a une sortie LLM mal formee sans perdre l'etat
- le fallback conserve la jouabilite, mais il reste un mode degrade
- la generation `.scene` libre reste plus fragile que la voie compilee deterministe

## Ce que ce module ne fait pas encore

- pas de scene v1 complete : seulement un sous-ensemble centre sur `room`, `camera`, `spotlight`, `sky`, `plane`, `box` et les premiers `prefab_*` est supporte
- pas encore de validation semantique riche ou de simplification automatique d'une scene generee par LLM
- pas d'API integrable propre pour un futur runtime de jeu
- pas d'accumulation progressive du raytracing pendant qu'un tour s'execute
- le frontend `SDL3` reste minimaliste :
  - texte via `SDL_RenderDebugText`
  - pas encore de police bitmap custom
  - pas encore de layout plus riche ou de widgets dedies
- le streaming actuel expose le flux brut du modele pendant la fabrication du JSON, pas encore une narration incrementalement parsee
- la generation de metadata de salle improvisee reste fragile :
  - fallback metadata utilise si le JSON de room generation est mal forme
  - fallback scene utilise si le `.scene` genere reste invalide
- la grammaire JSON `llama.cpp` n'est pas activee par defaut car l'appel bas niveau s'est montre instable sur ce build Windows/CUDA
- pas encore de couche compacte d'instanciation ou de repetition pour les prefabs
- les prefabs actuels augmentent fortement le nombre de triangles et de materiaux
- pas de telemetrie CPU/GPU/memoire
- pas de tests automatises

## Ecart assume par rapport a la spec longue

La spec cible a terme une image grayscale avec une lumiere portee par la camera, afin d'obtenir un rendu plus brutaliste et found-footage.

Le depot n'est plus limite a la Cornell Box : il sait maintenant rendre une premiere scene proprietaire a primitives et l'eclairer avec un spot analytique attache a la camera.

La Cornell Box reste preservee comme scene de reference, car cela permet de :

- verifier rapidement la chaine de rendu
- obtenir une scene de reference connue
- mesurer les performances
- documenter une premiere radiosite simple

L'eclairage camera existe, mais sa calibration est encore ouverte sur le plan esthetique.

Cet ecart est intentionnel et provisoire.
