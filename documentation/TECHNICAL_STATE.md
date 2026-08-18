# Technical State — Within the Latent Walls / Entre les Murs Latents

Derniere mise a jour : 2026-08-18

## Resume

Le depot contient actuellement un runtime natif C++11 avec renderer a scenes proprietaires, boucle locale `llama.cpp`, et frontend SDL3. La Cornell Box en niveaux de gris reste une scene de reference du sous-systeme de rendu, mais elle n'est plus le centre fonctionnel du depot.

Le renderer et l'interface constituent encore une base de prototype plutot qu'un produit final, mais la chaine executable est maintenant suffisamment complete pour des sessions headless et interactives.

Sur le plan architectural, la cible d'inference retenue est maintenant `llama.cpp` avec acceleration CUDA autour de `Ministral 3 8B`.

Le depot contient maintenant une premiere boucle reelle `commande -> LLM -> etat -> scene -> rendu`, disponible en mode headless et dans une premiere HMI `SDL3`.

## Frontiere entre reorientation et implementation

Une premiere tranche Eryx est implementee dans le runtime :

- sept lieux canoniques actifs, de `quarry_threshold` a `prospect_shelter`
- sept fixtures `.scene` source dans `assets/scenes/eryx_*.scene`
- prompts de tour, generation de salle et audit visuel migres vers la carriere venusienne
- etat serialise expose comme `spatial_entropy`, `external_temperature_c`, `body_temperature_c`, `suit_state`, `oxygen_state` et `instrument_power_state`
- HUD thermique `EXT. TEMPERATURE` / `BODY TEMPERATURE`, traduit en français après sélection `F`
- température d'échantillonnage effective dérivée de la température corporelle pour les tours et l'imagination des lieux
- sélection de langue `English (E)` / `Français (F)` au lancement SDL ; champs et mécaniques internes du LLM conservés en anglais
- écran de langue titré `Entre les Murs Latents` en grand, puis `Within the Latent Walls` en plus petit
- masque de visière 2D symétrique et procédural devant le viewport 3D
- douze liens diriges constituant une route d'arpentage
- type persistant `InvisibleBarrier`, separe de `blocked_exits`
- contact invisible au nord de `labyrinth_threshold`, avec preuve et statut `discovered`
- retour non reciproque `prospect_shelter --west--> scanner_station`
- huit archetypes Eryx dans le compilateur hybride et integration des neuf prefabs actifs

La frontiere encore ouverte est la mutation **live** : le LLM ne produit pas encore de proposition topologique separee, et le moteur n'a pas encore de decision `accepted`, `adjusted`, `rejected` ou `deferred` ni d'historique de mutation. La contradiction actuelle est auteurisee et deterministe.

L'entropie spatiale et les deux températures possèdent maintenant des membres C++ distincts. La lecture de l'ancienne clé `datacenter_temperature_c` reste acceptée comme alias de sauvegarde pour `spatial_entropy`. Les noms membres `cooling_state`, `water_state`, `power_state` et `desert_state` restent legacy. Les lieux, prefabs et fixtures datacenter sont préservés comme baselines historiques.

## Verrous d'architecture actes

- runtime narratif local : `llama.cpp`
- acceleration principale de developpement : `CUDA`
- modele cible v1 : `Ministral 3 8B Instruct 2512`
- couche multimedia multi-OS : `SDL3`
- distribution Windows : installeur Inno Setup natif, poids GGUF externe telecharge et verifie pendant l'installation

## Fonctionnalites presentes

- Tranche canonique Eryx documentee dans `ERYX_PLAYABLE_SPATIAL_ROADMAP.md` : seuil, extraction, coupe cristalline, scanner, plateau, seuil du labyrinthe et abri de Vey.
- Topologie de traversal Eryx installee sans appel LLM pour garantir un parcours de reference reproductible.
- Une barriere invisible est serialisee avec `place_id`, direction, preuve et statut de decouverte ; elle refuse le mouvement sans incrementer `move_count`.
- Les liens diriges peuvent etre volontairement non reciproques ; le premier retour impossible contourne deux lieux connus tout en conservant l'identite locale du scanner.
- Les bandes cachees actives sont `survey camp`, `inner quarry`, `quarry seam`, `outer shelf` et `open venus`.
- Les archetypes hybrides actifs incluent `quarry_threshold`, `extraction_field`, `quarry_cut`, `scanner_station`, `venus_plateau`, `labyrinth_threshold`, `prospecting_shelter` et `industrial_service_zone`.

- Build natif via CMake et Visual Studio 2022.
- Options CMake `LIMINAL_ENABLE_LLAMA_CPP`, `LIMINAL_ENABLE_LLAMA_CUDA`, `LIMINAL_ENABLE_OPENMP` et `LIMINAL_ENABLE_SDL3_FRONTEND` pour raccorder un `llama.cpp` vendorise, activer le parallelisme CPU si disponible et construire la boucle desktop interactive.
- Fallback `find_package(SDL3)` puis `FetchContent` automatique sur `SDL 3.4.12` si la bibliotheque n'est pas deja installee localement.
- Helper Windows `build_release.bat` a la racine pour configurer et compiler la version `Release`.
- Helper Windows `build_installer.bat` pour fabriquer le setup, le manifeste de distribution et l'archive des sources correspondantes.
- Helper Windows `run_cornell_test.bat` a la racine pour compiler puis lancer le rendu de verification Cornell Box.
- Helper Windows `download_ministral.bat` pour telecharger le modele cible.
- Helper Windows `ask_ministral.bat` pour lancer une question libre contre le modele local via `llama-cli`.
- Helper Windows `play_within_the_latent_walls.bat` pour lancer directement la boucle SDL3 sous le titre actif.
- Helper Windows `generate_prefab_catalog.bat` pour regenerer le catalogue visuel des prefabs.
- Helper Windows `run_scene_generation_benchmark.bat` pour executer une batterie fixe de generations `.scene` via `Ministral`, puis auditer et rendre chaque cas.
- Helper Windows `run_hybrid_scene_generation_benchmark.bat` pour executer la chaine runtime `room JSON -> compilateur hybride -> rendu` sur une batterie fixe de briefs source.
- Executable CLI `liminal_cornell_renderer`.
- Option CLI `--llama-info` pour verifier la presence du runtime `llama.cpp`, le commit vendorise et la disponibilite de l'offload GPU.
- Option CLI `--sdl` pour lancer la premiere boucle desktop `SDL3`.
- Options CLI pour le noyau fonctionnel en preparation :
  - `--dump-turn-contract`
  - `--dump-generated-room-prompt`
  - `--dump-scene-audit-prompt`
  - `--compile-location`
  - `--audit-scene-text`
- Options de profondeur de transport séparées : `--bounces` pour les événements diffus et `--glass-bounces` pour les interfaces diélectriques (`9` par défaut).
- Preset de rendu par defaut en `800x400` pour la scene liminale, avec cadrage panorama.
- Chargement de scene generique via `--scene <path>` pour `.scene` ou `.obj`.
- Parseur de format de scene proprietaire v1.
- Chargement equivalent d'une scene `v1` depuis un bloc texte en memoire, sans passer obligatoirement par un fichier comme interface interne.
- Validation syntaxique de base pour `room`, `camera`, `spotlight`, `sky`, `plane`, `box` et les premiers `prefab_*`.
- Spot analytique attache a la camera, avec panneau parametrique, portee limitee et cone progressif.
- Fond proceduriel `sky` optionnel pour les rayons sans intersection :
  - zenith vert
  - horizon jaune clair
  - nadir sombre
  - grain fort
  - etoiles deterministes
  - radiance d'environnement multipliée par `2` uniquement sur les rayons sans intersection
- Premiere couche de prefabs expands en primitives simples :
  - `prefab_gate`
  - `prefab_rack`
  - `prefab_crate`
  - `prefab_cooling_unit`
  - `prefab_ai_server`
  - `prefab_survey_beacon`
  - `prefab_crystal_scanner`
  - `prefab_crystal_cluster`
  - `prefab_extraction_rig`
  - `prefab_prospect_shelter`
  - `prefab_quarry_pylon`
  - `prefab_atmospheric_processor`
  - `prefab_cactus_sentinel`
  - `prefab_cactus_fork`
  - `prefab_cactus_cluster`
  - `prefab_rock_low`
  - `prefab_rock_wide`
  - `prefab_rock_tall`
  - `prefab_rock_spire`
- Conversion des primitives `plane` et `box` vers le backend triangle/BVH existant.
- Premiere scene liminale handcraftee dans `assets/scenes/liminal_service_corridor.scene`.
- Sept scenes canoniques Eryx handcraftees :
  - `assets/scenes/eryx_quarry_threshold.scene`
  - `assets/scenes/eryx_extraction_field.scene`
  - `assets/scenes/eryx_crystal_cut.scene`
  - `assets/scenes/eryx_scanner_station.scene`
  - `assets/scenes/eryx_survey_plateau.scene`
  - `assets/scenes/eryx_labyrinth_threshold.scene`
  - `assets/scenes/eryx_prospect_shelter.scene`
- Trois scenes canoniques historiques de validation spatiale :
  - `assets/scenes/datacenter_entry_gate.scene`
  - `assets/scenes/datacenter_server_aisles.scene`
  - `assets/scenes/datacenter_roof_watch.scene`
- Chargement d'un fichier OBJ triangule simple.
- Chargement d'un fichier MTL reduit en materiaux simples, convertis vers la palette RGB semantique interne.
- Reconstruction locale de la lumiere de la Cornell Box a partir des metadonnees de la scene.
- Structure d'acceleration BVH sur triangles.
- Intersections rayon/AABB et rayon/triangle.
- Les AABB de feuilles coplanaires, donc d'epaisseur nulle sur un axe, sont acceptees par le parcours BVH. Cette correction evite la disparition de triangles entiers ou de demi-facettes sur les primitives `box`.
- Path tracing diffus simple avec :
  - echantillonnage direct de la lumiere
  - un petit nombre de rebonds diffus
  - roulette russe
  - clamp simple des contributions extremes
- Modèle diélectrique verrouillé pour les prismes de `prefab_crystal_cluster` et l'échantillon de `prefab_crystal_scanner` :
  - indice de réfraction fixe `1.52`
  - loi de Snell
  - Fresnel diélectrique exact, sans approximation de Schlick
  - réflexion totale interne
  - tirage stochastique réflexion / transmission conservant l'énergie
  - limite indépendante de neuf interfaces par chemin
  - absorption volumique RGB selon Beer–Lambert et distance réellement parcourue dans chaque prisme
  - suivi borné des milieux actifs pour les cristaux qui se chevauchent
  - cutoff énergétique interne `0.02`
  - transmission approximative des rayons d'ombre avec le même filtre volumique
  - dispersion RGB « poor man's » : un canal héroïque stratifié par sample, IOR central `1.52`, écart inter-bandes nominal `0.035` et jitter d'IOR
  - recomposition Monte-Carlo pondérée des trois canaux sans tripler le nombre de chemins par impact
- Palette visuelle verrouillee :
  - ciel vénusien vert vers jaune clair
  - desert ocre
  - LEDs de racks rouges
  - reste du decor en gris
- Parallelisation optionnelle de la boucle de rendu par lignes via OpenMP, avec fallback mono-thread.
- Sortie image en `PNG` RGB via `stb_image_write`.
- Sortie `PGM` legacy encore supportee selon l'extension du fichier.
- Mesure du temps de chargement et du temps de rendu.
- Arborescence `vendor/llama.cpp` ajoutee au depot comme base d'integration locale du runtime LLM.
- Script Python `scripts/download_ministral.py` pour telecharger et valider `Ministral 3 8B Instruct 2512` en GGUF `Q4_K_M`.
- Script Python `scripts/build_installer.py` pour assembler un payload sans `.gguf`, resoudre les DLL non-systeme requises, tester `--llama-info`, amorcer un compilateur Inno Setup epingle si necessaire et produire les artefacts de distribution.
- Installeur Windows `0.1.0` valide localement : payload stage de `576,120,309` octets, setup compresse de `406,824,953` octets, modele externe de `5,198,911,904` octets.
- Smoke test local `llama-cli` valide sur `Ministral 3 8B`, avec reponse effective a une question libre.
- Premier noyau fonctionnel pour la future boucle de tour :
  - structs `HardState`, `SoftState`, `SpatialState`, `TurnResult`
  - contrat de tour structure et prompt builder v1
  - prompt d'audit direct pour une sortie `.scene`
  - compilateur deterministe des sept lieux Eryx et des trois baselines datacenter depuis `SpatialState`
- Premier runtime headless `Ministral` branche au moteur :
  - `GenerateChatCompletion()` via `llama.cpp`
  - callback de streaming token par token exploitable par l'HMI
  - parsing JSON tolerant aux fences Markdown
  - mode repair d'une sortie JSON mal formee
  - fallback no-op si la sortie reste inexploitable apres repair
  - application des deltas sur `HardState`, `SoftState` et `SpatialState`
  - `scene_constraints` persiste maintenant dans `SpatialState`, remonte dans les prompts et peut piloter la composition du decor compile
  - deuxieme appel LLM separe pour l'audit `.scene`
  - rendu final depuis la voie deterministe `SpatialState -> Scene`
- Premier graphe de salles improvisees :
  - interception de `NORTH`, `EAST`, `SOUTH`, `WEST`
  - generation d'une nouvelle salle seulement a la premiere entree
  - cache persistant du `scene_text` et des liens cardinaux en session
  - retour possible vers une salle deja decouverte sans repasser par le LLM
  - coexistence entre lieux canoniques et lieux generes dans la meme session
- Premier etat cache de monde "vaguement euclidien" pour les salles improvisees :
  - pose cachee `world_x/world_z` stockee dans `SpatialState`
  - derives publics `world_band`, `survey_base_relation` et `sky_exposure`
  - bandes qualitatives `survey camp`, `inner quarry`, `quarry seam`, `outer shelf`, `open venus`
  - propagation de la pose a travers les liens cardinaux et conservation en session JSON
- Generation de salles maintenant guidee par cette derive cachee :
  - le prompt de metadata ne recoit plus un simple seuil de distance
  - il recoit des cues qualitatifs sur la bande du monde, le cote de la base d'arpentage et l'ouverture vers le ciel
  - un **guide de derive textuel cache** traduit maintenant distance + angle en pression narrative, asymetries laterales, motifs preferes et vocabulaire a eviter
  - le LLM est explicitement pousse a laisser ces cues contaminer `title`, `location_archetype`, `anchors`, `visible_objects` et `scene_constraints`
  - les `scene_constraints` peuvent porter une dissymetrie verbale (`east pylon`, `deep-field beacon`, `west sample case`, etc.)
  - les lieux lointains tendent vers le shelf puis le champ venusien sans plafond, plutot que rester des interieurs fermes
- Compilateur hybride `SpatialState -> .scene` etendu :
  - bascule entre service pressurise, carriere, plateau et champ ouvert selon l'etat cache et les cues semantiques
  - shells Eryx distincts : couronne fendue, work slab, cut walls, cantilever, ligne d'horizon et abri
  - injection procedurale de rigs, scanners, cristaux, pylones, balises, cargo et traitement atmospherique
  - les cactus, racks et masses datacenter ne restent accessibles que par les archetypes et fixtures legacy
  - reconnaissance dédiée des cues `crystal`, `scanner`, `drill`, `prospect shelter`, `survey beacon`, `quarry pylon` et `atmospheric processor` vers les nouvelles primitives Eryx
- Premiere HMI `SDL3` desktop :
  - event loop non bloquante
  - worker de tour dedie a l'inference et a l'image `0`, plus worker de vue separe pour les images `1` a `7`
  - buffer `AnimatedView` borne a huit images RGB contigues, publiees une fois terminees
  - mouvement de respiration deterministe entre pose camera A et pose B : `+0.025 m` vertical, `+0.012 m` avant, `+0.35 deg` pitch et `+0.15 deg` roll
  - interpolation ease-in/ease-out cosinus et lecture ping-pong dynamique a `6 images/s`
  - generation identifiee par compteur monotone, rejet des publications obsoletes et annulation entre deux images
  - image courante chargee dans une texture SDL streaming unique ; HUD, visiere, transcript et saisie recomposes ensuite a chaque passe
  - flux brut du modele et `.scene` dans le terminal
  - ligne terminal de provenance apres chaque tour pour distinguer scene canonique, salle generee, fallback metadata et fallback scene
  - panneau de transcript joueur avec narration finale seulement
  - rendu TTF joueur via `SDL3_ttf` et fontes `Zilla Slab`
  - écran de sélection de langue `E`/`F` avant la boucle de commande
  - interface exposée, narrations déterministes et instructions LLM joueur localisées ; contrats internes conservés en anglais
  - validation lexicale des sorties joueur françaises, suivie si nécessaire d'une passe de localisation LLM à température nulle puis d'un fallback français déterministe
  - raccourcis et directions affichés selon la langue (`NORTH`/`NORD`, `WEST`/`OUEST`, etc.)
  - garde d'affichage française pour les titres et narrations anglophones déjà présents dans une ancienne sauvegarde, sans mutation des données archivées
  - masque de visière symétrique calculé par scanlines rectangulaires et frontières elliptiques ; extérieur de fenêtre noir et encoche centrale limitée au tiers inférieur
  - deux panneaux thermiques affichant température extérieure entière et température corporelle décimale
  - segments `*highlightes*` rendus avec `Zilla Slab Highlight`
  - ligne de commande avec edition clavier et historique haut/bas
  - ligne de statut localisée avec spinner ASCII pour distinguer les phases d'imagination et de raytracing
  - annulation best-effort via `Escape`
- Script Python `scripts/generate_prefab_catalog.py` pour rendre le vocabulaire de prefabs actif, stocker les `.scene`/`.png` d'audit et assembler un unique `documentation/PREFAB_CATALOG.md`.
  - Le catalogue Eryx rend neuf objets à `1024x1024`, `24` samples par pixel, en deux vues chacun, sur un plan de carrière commun.
  - Le générateur supprime les anciens artefacts `.scene`/`.png` qui ne font plus partie de la liste source.
- Script Python `scripts/run_scene_generation_benchmark.py` pour :
  - fabriquer des prompts d'audit `.scene` depuis le moteur lui-meme
  - interroger `Ministral` via `llama-cli`
  - normaliser la sortie brute en `.scene`
  - auditer chaque scene avec le parseur runtime
  - rendre les scenes valides et assembler `documentation/SCENE_GENERATION_BENCHMARK.md`
- Script Python `scripts/run_hybrid_scene_generation_benchmark.py` pour :
  - fabriquer le prompt exact de generation de salle JSON via le moteur
  - lancer `--run-turn` sur les memes briefs source
  - capter la metadata brute, la scene compilee et l'etat final
  - rendre les scenes generees par la chaine hybride
  - assembler `documentation/HYBRID_SCENE_GENERATION_BENCHMARK.md`
- Nouveau chemin de rendu memoire `RenderSceneToPixels()` pour alimenter directement une texture `SDL3`, desormais en buffer RGB.
- Self-test `--animated-view-self-test` pour les sequences `1/2/4/8`, les endpoints camera, l'absence de derive et le rejet des generations obsoletes.
- Diagnostics `--animated-view-debug`, `--animated-view-fail-frame`, `--sdl-smoke-test-ms` et `--sdl-smoke-command` pour observer le worker, forcer un mode degrade et automatiser une session SDL courte.
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
- [../src/animated_view.h](/C:/works/projects/game-liminal-raytraced-llm-world/src/animated_view.h:1) : etat borne de la vue, configuration du mouvement et interface de playback.
- [../src/animated_view.cpp](/C:/works/projects/game-liminal-raytraced-llm-world/src/animated_view.cpp:1) : pose B, interpolation cosinus, ping-pong, garde de generation et self-test.
- [../src/sdl_frontend.h](/C:/works/projects/game-liminal-raytraced-llm-world/src/sdl_frontend.h:1) : interface de la premiere boucle interactive `SDL3`.
- [../src/sdl_frontend.cpp](/C:/works/projects/game-liminal-raytraced-llm-world/src/sdl_frontend.cpp:1) : fenetre, transcript, saisie texte, workers de tour et d'animation, publication progressive et presentation streaming.
- [../src/main.cpp](/C:/works/projects/game-liminal-raytraced-llm-world/src/main.cpp:76) : point d'entree CLI, parsing des options, telemetrie basique et premieres commandes de debug fonctionnel.
- [../src/game_state.h](/C:/works/projects/game-liminal-raytraced-llm-world/src/game_state.h:1) : structures du monde, des deltas et du contrat de tour.
- [../src/game_state.cpp](/C:/works/projects/game-liminal-raytraced-llm-world/src/game_state.cpp:1) : enums, etats initiaux, pose cachee du monde et resumes de debug.
- [../src/turn_contract.h](/C:/works/projects/game-liminal-raytraced-llm-world/src/turn_contract.h:1) : interface de generation des prompts structure et audit.
- [../src/turn_contract.cpp](/C:/works/projects/game-liminal-raytraced-llm-world/src/turn_contract.cpp:1) : texte du schema de sortie, brief spatial et prompts v1.
- [../src/scene_compiler.h](/C:/works/projects/game-liminal-raytraced-llm-world/src/scene_compiler.h:1) : interface de compilation d'un `SpatialState` vers une scene rendable.
- [../src/scene_compiler.cpp](/C:/works/projects/game-liminal-raytraced-llm-world/src/scene_compiler.cpp:1) : mapping des lieux canoniques, compilation hybride interior/parapet/desert et audit memoire d'une scene candidate.
- [../src/llm_runtime.h](/C:/works/projects/game-liminal-raytraced-llm-world/src/llm_runtime.h:1) : interface minimale du runtime `llama.cpp`.
- [../src/llm_runtime.cpp](/C:/works/projects/game-liminal-raytraced-llm-world/src/llm_runtime.cpp:1) : initialisation backend, generation texte `llama.cpp`, introspection GPU et configuration de l'inference.
- [../src/turn_runner.h](/C:/works/projects/game-liminal-raytraced-llm-world/src/turn_runner.h:1) : interface de la boucle headless d'un tour.
- [../src/turn_runner.cpp](/C:/works/projects/game-liminal-raytraced-llm-world/src/turn_runner.cpp:1) : parsing de la sortie LLM, application des deltas et audit scene separe.
- [../assets/cornell/cornell_box.obj](/C:/works/projects/game-liminal-raytraced-llm-world/assets/cornell/cornell_box.obj:1) : scene de reference vendorisee.
- [../assets/cornell/cornell_box.mtl](/C:/works/projects/game-liminal-raytraced-llm-world/assets/cornell/cornell_box.mtl:1) : materiaux de reference.
- [../assets/scenes/liminal_service_corridor.scene](/C:/works/projects/game-liminal-raytraced-llm-world/assets/scenes/liminal_service_corridor.scene:1) : premiere scene proprietaire liminale.
- [../assets/scenes/eryx_quarry_threshold.scene](/C:/works/projects/game-liminal-raytraced-llm-world/assets/scenes/eryx_quarry_threshold.scene:1) : premiere fixture de la route canonique Eryx.
- [ERYX_PLAYABLE_SPATIAL_ROADMAP.md](./ERYX_PLAYABLE_SPATIAL_ROADMAP.md) : carte, grammaire visuelle et recette du parcours Eryx.
- [../assets/scenes/datacenter_entry_gate.scene](/C:/works/projects/game-liminal-raytraced-llm-world/assets/scenes/datacenter_entry_gate.scene:1) : fixture historique du portail datacenter.
- [../assets/scenes/datacenter_server_aisles.scene](/C:/works/projects/game-liminal-raytraced-llm-world/assets/scenes/datacenter_server_aisles.scene:1) : fixture canonique des travees de serveurs.
- [../assets/scenes/datacenter_roof_watch.scene](/C:/works/projects/game-liminal-raytraced-llm-world/assets/scenes/datacenter_roof_watch.scene:1) : fixture canonique du toit / tour de ronde.
- [../scripts/download_ministral.py](/C:/works/projects/game-liminal-raytraced-llm-world/scripts/download_ministral.py:1) : telechargement et validation du modele cible.
- [../scripts/generate_prefab_catalog.py](/C:/works/projects/game-liminal-raytraced-llm-world/scripts/generate_prefab_catalog.py:1) : regeneration du catalogue visuel des prefabs.
- [../scripts/run_scene_generation_benchmark.py](/C:/works/projects/game-liminal-raytraced-llm-world/scripts/run_scene_generation_benchmark.py:1) : benchmark local de generation de scenes `.scene`.
- [../scripts/run_hybrid_scene_generation_benchmark.py](/C:/works/projects/game-liminal-raytraced-llm-world/scripts/run_hybrid_scene_generation_benchmark.py:1) : benchmark local de la chaine runtime hybride.
- [../scripts/scene_generation_benchmark_cases.json](/C:/works/projects/game-liminal-raytraced-llm-world/scripts/scene_generation_benchmark_cases.json:1) : batterie fixe des briefs spatiaux benchmarkes.
- [SCENE_FORMAT_V1.md](./SCENE_FORMAT_V1.md) : description du format de scene implemente.
- [HYBRID_SCENE_LAYOUT_PLAN.md](./HYBRID_SCENE_LAYOUT_PLAN.md) : plan technique de la future couche hybride LLM + placement procedural.
- [SCENE_GENERATION_BENCHMARK.md](./SCENE_GENERATION_BENCHMARK.md) : synthese visuelle et technique de la batterie de generation `.scene`.
- [HYBRID_SCENE_GENERATION_BENCHMARK.md](./HYBRID_SCENE_GENERATION_BENCHMARK.md) : synthese visuelle et technique de la chaine runtime hybride.
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
.\build\Release\liminal_cornell_renderer.exe --dump-turn-contract --location labyrinth_threshold --command "inspect the northern datum"
.\build\Release\liminal_cornell_renderer.exe --dump-scene-audit-prompt --location scanner_station
.\build\Release\liminal_cornell_renderer.exe --compile-location quarry_threshold --output output\compiled_eryx_threshold.png
.\build\Release\liminal_cornell_renderer.exe --audit-scene-text assets\scenes\eryx_labyrinth_threshold.scene --output output\audited_labyrinth_threshold.png
.\build\Release\liminal_cornell_renderer.exe --sdl --location quarry_threshold --save-state output\eryx_session_state.json
.\build\Release\liminal_cornell_renderer.exe --animated-view-self-test
.\build\Release\liminal_cornell_renderer.exe --sdl --animated-view-debug --view-animation-fps 6
.\build\Release\liminal_cornell_renderer.exe --run-session --location quarry_threshold --command north --command north --command east --command north --command east --command north --command east --command west --save-state output\eryx_route.json --output output\eryx_route.png
.\build\Release\liminal_cornell_renderer.exe --run-turn --load-state output\eryx_route.json --command west --save-state output\eryx_route_2.json --output output\eryx_route_2.png
```

Helper Windows :

```bat
build_release.bat
run_cornell_test.bat
download_ministral.bat
ask_ministral.bat
play_within_the_latent_walls.bat
generate_prefab_catalog.bat
run_scene_generation_benchmark.bat
```

Note d'hygiene Windows :

- en PowerShell, utiliser `$env:TEMP`
- en `cmd` / `.bat`, utiliser `%TEMP%`
- en CMake, utiliser `$ENV{TEMP}`

Observation locale du `2026-07-31` :

Un dossier parasite `/%TEMP%` a ete observe a la racine du depot. Les verifications locales indiquent qu'il s'agit d'un clone SDL temporaire cree via une syntaxe de variable d'environnement inadaptee au shell appelant, et non d'un artefact produit par les fichiers de build tracked du projet.

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

### 2026-08-02 - Benchmark direct de generation `.scene`

Contexte : build `Release` local avec `llama.cpp` et CUDA, `Ministral 3 8B Instruct 2512`, prompts fabriques via `--dump-scene-audit-prompt`, rendu benchmark en `800x400`, `8 spp`, `3 bounces`.

- batterie fixe : `10` briefs spatiaux
- scenes valides : `9 / 10`
- temps de generation LLM observes : environ `17.95 s` a `27.16 s` selon les cas
- temps d'audit + rendu observes : environ `0.46 s` a `1.00 s`

Observation importante :

- la plupart des briefs fixes passent maintenant de bout en bout jusqu'au PNG
- l'echec restant (`loading_dock_dust`) n'est pas une troncature mais une invention de vocabulaire hors grammaire `scene v1`
- le modele a tente un `prefab_gate` de type volet/shutter avec `size` incomplete et des proprietes non supportees comme `open()` ou `bent()`
- le benchmark devient donc un bon outil pour separer :
  - les scenes simplement valides
  - les scenes tronquees
  - les scenes syntaxiquement propres mais hors contrat

### 2026-08-02 - Premiere integration runtime du layout hybride

Contexte : build `Release` local avec `llama.cpp` et CUDA, `Ministral 3 8B Instruct 2512`, creation d'une salle improvisee depuis `gate`, puis second tour dans cette salle via etat JSON recharge.

- la generation de metadata de salle improvisee reste confiee au LLM local
- la generation de la scene de cette salle ne passe plus par un prompt `.scene` libre :
  - `scene_compiler.cpp` fabrique maintenant un `.scene` deterministe a partir du `SpatialState`
  - les scenes canoniques continuent a etre lues depuis leurs fichiers `.scene`
- la provenance de scene est maintenant stockee dans l'etat de session :
  - `hybrid`
  - `fallback`
  - `llm` pour les anciennes sauvegardes ou les anciennes salles deja serialisees
- un tour standard dans une salle generee recalcule maintenant le cache de scene de cette salle au lieu de reutiliser aveuglement un ancien `.scene`

Validation manuelle :

- `--run-turn --location gate --command north` :
  - salle `Control Hub` creee
  - `Generated room scene source: hybrid`
  - rendu : `output\\hybrid_generated_room_v2.png`
- `--run-turn --load-state output\\hybrid_generated_room_v2.session.json --command "examine status board"` :
  - meme salle rechargee
  - `Generated room cache refresh source: hybrid`
  - rendu : `output\\hybrid_generated_room_refresh.png`

Observation importante :

- le pipeline runtime `metadata JSON -> SpatialState -> layout hybride -> .scene -> audit -> rendu` fonctionne maintenant de bout en bout
- `scene_constraints` fournit maintenant un vrai canal runtime entre la sortie JSON du LLM et les prefabs / masses choisis par le compilateur de scene
- le benchmark de generation `.scene` libre reste utile pour auditer Ministral, mais il n'est plus le chemin principal de fabrication des nouvelles salles runtime

### 2026-08-02 - Benchmark runtime de la chaine hybride

Contexte : build `Release` local avec `llama.cpp` et CUDA, `Ministral 3 8B Instruct 2512`, meme batterie de `10` briefs source que le benchmark `.scene` direct, mais exploites ici comme salles d'origine pour la chaine `brief source -> room JSON -> compilateur hybride -> rendu`.

- scenes valides : `10 / 10`
- scenes avec fallback metadata ou scene : `0 / 10`
- temps d'inference observes : environ `7.08 s` a `9.68 s`
- temps de rendu observes : environ `0.56 s` a `1.33 s`

Observation importante :

- la voie runtime hybride elimine ici l'echec residuel observe sur `loading_dock_dust` dans le benchmark `.scene` direct
- la robustesse vient du fait que Ministral n'a plus a inventer la grammaire `.scene` complete : il n'invente plus qu'une metadata JSON de salle
- le benchmark historique reste donc utile pour auditer la "liberte syntaxique" de Ministral, tandis que le benchmark hybride mesure la voie effectivement retenue pour le jeu

## Ce que ce module ne fait pas encore

- pas de scene v1 complete : seulement un sous-ensemble centre sur `room`, `camera`, `spotlight`, `sky`, `plane`, `box` et les premiers `prefab_*` est supporte
- pas encore de validation semantique riche ou de simplification automatique d'une scene generee par LLM
- pas d'API integrable propre pour un futur runtime de jeu
- pas d'accumulation progressive du raytracing pendant qu'un tour s'execute
- le frontend `SDL3` reste minimaliste :
  - transcript et saisie via `SDL3_ttf` et les fontes Zilla Slab
  - titre et statut debug via `SDL_RenderDebugText`
  - pas encore de layout plus riche ou de widgets dedies
- le streaming actuel expose le flux brut du modele pendant la fabrication du JSON, pas encore une narration incrementalement parsee
- la generation de metadata de salle improvisee reste fragile :
  - fallback metadata utilise si le JSON de room generation est mal forme
  - fallback scene utilise si le compilateur hybride echoue a produire une scene admissible
- la generation directe de `.scene` reste partiellement hors contrat :
  - le modele peut encore inventer des proprietes non supportees comme `open()`, `bent()` ou des arites de `size(...)` invalides
  - le benchmark `SCENE_GENERATION_BENCHMARK.md` sert maintenant a objectiver ces derives
- la couche hybride reste encore heuristique :
  - classification texte -> objet encore sommaire
  - certains objets textuels sont encore ramenes a des `box` ou `prefab_*` approximatifs
  - pas encore de solveur de placement plus fin qu'une repartition 2.5D avec anti-chevauchement simple
- la grammaire JSON `llama.cpp` n'est pas activee par defaut car l'appel bas niveau s'est montre instable sur ce build Windows/CUDA
- pas encore de couche compacte d'instanciation ou de repetition pour les prefabs
- les prefabs actuels augmentent fortement le nombre de triangles et de materiaux
- pas de telemetrie CPU/GPU/memoire
- pas de suite globale de tests automatises ; la vue animee possede toutefois un self-test deterministe et des smoke tests SDL temporises
- semantique runtime Eryx encore partielle :
  - pas de proposition LLM de mutation topologique separee des deltas spatiaux ordinaires
  - pas de validateur de frequence, recuperation ou provenance des mutations live
  - pas de registre persistant pour les marques, scans et comparaisons produits par le joueur
- noms membres C++ legacy encore conserves pour le scaphandre, l'oxygène, l'alimentation des instruments et la météo de surface

## Ecart assume par rapport a la spec longue

La spec longue parlait initialement d'une image strictement grayscale. Le depot a maintenant bifurque vers un RGB tres contraint : ciel vénusien vert/jaune, desert ocre, LEDs rouges et tout le reste en gris, tout en conservant la lumiere portee par la camera et le grain brutaliste.

Le depot n'est plus limite a la Cornell Box : il sait maintenant rendre une premiere scene proprietaire a primitives et l'eclairer avec un spot analytique attache a la camera.

La Cornell Box reste preservee comme scene de reference, car cela permet de :

- verifier rapidement la chaine de rendu
- obtenir une scene de reference connue
- mesurer les performances
- documenter une premiere radiosite simple

L'eclairage camera existe, mais sa calibration est encore ouverte sur le plan esthetique.

Cet ecart est intentionnel et provisoire.
