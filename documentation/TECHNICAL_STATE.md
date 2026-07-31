# Technical State

Derniere mise a jour : 2026-07-31

## Resume

Le depot contient actuellement un premier module de rendu natif en C++11 centre sur une Cornell Box en niveaux de gris.

Ce module constitue un bootstrap du futur sous-systeme de rendu. Il ne s'agit pas encore du renderer final du jeu, mais d'une premiere base executable, compilable et documentee.

Sur le plan architectural, la cible d'inference retenue est maintenant `llama.cpp` avec acceleration CUDA autour de `Ministral 3 8B`, et le depot contient deja une couche minimale d'introspection runtime.

En revanche, la boucle de jeu, l'inference de tour et la future couche multimedia `SDL3` ne sont pas encore branchees.

## Verrous d'architecture actes

- runtime narratif local : `llama.cpp`
- acceleration principale de developpement : `CUDA`
- modele cible v1 : `Ministral 3 8B Instruct 2512`
- future couche multimedia multi-OS : `SDL3`

## Fonctionnalites presentes

- Build natif via CMake et Visual Studio 2022.
- Options CMake `LIMINAL_ENABLE_LLAMA_CPP` et `LIMINAL_ENABLE_LLAMA_CUDA` pour raccorder un `llama.cpp` vendorise.
- Helper Windows `build_release.bat` a la racine pour configurer et compiler la version `Release`.
- Helper Windows `run_cornell_test.bat` a la racine pour compiler puis lancer le rendu de verification Cornell Box.
- Helper Windows `download_ministral.bat` pour telecharger le modele cible.
- Helper Windows `ask_ministral.bat` pour lancer une question libre contre le modele local via `llama-cli`.
- Executable CLI `liminal_cornell_renderer`.
- Option CLI `--llama-info` pour verifier la presence du runtime `llama.cpp`, le commit vendorise et la disponibilite de l'offload GPU.
- Preset de rendu par defaut en `800x400` pour la scene liminale, avec cadrage panorama.
- Chargement de scene generique via `--scene <path>` pour `.scene` ou `.obj`.
- Parseur de format de scene proprietaire v1.
- Validation syntaxique de base pour `room`, `camera`, `spotlight`, `sky`, `plane` et `box`.
- Spot analytique attache a la camera, avec panneau parametrique, portee limitee et cone progressif.
- Fond proceduriel `sky` optionnel pour les rayons sans intersection :
  - zenith sombre
  - horizon plus clair
  - nadir sombre
  - grain fort
  - etoiles deterministes
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
- Sortie image en `PNG` via `stb_image_write`.
- Sortie `PGM` legacy encore supportee selon l'extension du fichier.
- Mesure du temps de chargement et du temps de rendu.
- Arborescence `vendor/llama.cpp` ajoutee au depot comme base d'integration locale du runtime LLM.
- Script Python `scripts/download_ministral.py` pour telecharger et valider `Ministral 3 8B Instruct 2512` en GGUF `Q4_K_M`.
- Smoke test local `llama-cli` valide sur `Ministral 3 8B`, avec reponse effective a une question libre.

## Fichiers importants

- [../CMakeLists.txt](/C:/works/projects/game-liminal-raytraced-llm-world/CMakeLists.txt:1) : configuration du build.
- [../src/core.h](/C:/works/projects/game-liminal-raytraced-llm-world/src/core.h:1) : types de base, math, RNG, configuration de rendu.
- [../src/scene.h](/C:/works/projects/game-liminal-raytraced-llm-world/src/scene.h:1) : structures de scene et BVH.
- [../src/scene.cpp](/C:/works/projects/game-liminal-raytraced-llm-world/src/scene.cpp:1) : chargement `.scene` et `.obj`, conversion des primitives, materiaux, lumiere Cornell, construction BVH.
- [../src/renderer.cpp](/C:/works/projects/game-liminal-raytraced-llm-world/src/renderer.cpp:1) : camera, spotlight analytique, intersections, visibilite, integrateur, export PNG/PGM.
- [../src/main.cpp](/C:/works/projects/game-liminal-raytraced-llm-world/src/main.cpp:76) : point d'entree CLI, parsing des options, telemetrie basique.
- [../src/llm_runtime.h](/C:/works/projects/game-liminal-raytraced-llm-world/src/llm_runtime.h:1) : interface minimale du runtime `llama.cpp`.
- [../src/llm_runtime.cpp](/C:/works/projects/game-liminal-raytraced-llm-world/src/llm_runtime.cpp:1) : initialisation backend, introspection GPU et impression de l'etat `llama.cpp`.
- [../assets/cornell/cornell_box.obj](/C:/works/projects/game-liminal-raytraced-llm-world/assets/cornell/cornell_box.obj:1) : scene de reference vendorisee.
- [../assets/cornell/cornell_box.mtl](/C:/works/projects/game-liminal-raytraced-llm-world/assets/cornell/cornell_box.mtl:1) : materiaux de reference.
- [../assets/scenes/liminal_service_corridor.scene](/C:/works/projects/game-liminal-raytraced-llm-world/assets/scenes/liminal_service_corridor.scene:1) : premiere scene proprietaire liminale.
- [../assets/scenes/datacenter_entry_gate.scene](/C:/works/projects/game-liminal-raytraced-llm-world/assets/scenes/datacenter_entry_gate.scene:1) : fixture canonique du portail d'entree.
- [../assets/scenes/datacenter_server_aisles.scene](/C:/works/projects/game-liminal-raytraced-llm-world/assets/scenes/datacenter_server_aisles.scene:1) : fixture canonique des travees de serveurs.
- [../assets/scenes/datacenter_roof_watch.scene](/C:/works/projects/game-liminal-raytraced-llm-world/assets/scenes/datacenter_roof_watch.scene:1) : fixture canonique du toit / tour de ronde.
- [../scripts/download_ministral.py](/C:/works/projects/game-liminal-raytraced-llm-world/scripts/download_ministral.py:1) : telechargement et validation du modele cible.
- [SCENE_FORMAT_V1.md](./SCENE_FORMAT_V1.md) : description du format de scene implemente.
- [SPATIAL_VALIDATION_PLAN.md](./SPATIAL_VALIDATION_PLAN.md) : protocole de validation du lien entre brief narratif, texte, scene v1 et rendu.
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

Exemple avec scene explicite :

```powershell
.\build\Release\liminal_cornell_renderer.exe --scene assets\scenes\liminal_service_corridor.scene
.\build\Release\liminal_cornell_renderer.exe --scene assets\cornell\cornell_box.obj
```

Helper Windows :

```bat
build_release.bat
run_cornell_test.bat
download_ministral.bat
ask_ministral.bat
```

Exemple de rendu :

```powershell
.\build\Release\liminal_cornell_renderer.exe --samples 32 --width 256 --height 256 --output output\cornell_box_32spp.png
```

## Parametres par defaut

- largeur : `800`
- hauteur : `400`
- echantillons par pixel : `32`
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

- portail d'entree du datacenter : environ `8295.89 ms`
- travees de serveurs : environ `11063.99 ms`
- toit / tour de ronde : environ `6325.08 ms`

Ces chiffres sont seulement des reperes de travail. Ils ne constituent pas encore un benchmark stable.

## Ce que ce module ne fait pas encore

- pas de scene v1 complete : seulement `plane` et `box` sont supportes
- pas de validation defensive ou simplification automatique d'une scene generee par LLM
- pas encore de chargement effectif du modele `Ministral 3 8B` dans la boucle du jeu
- pas encore d'inference de tour, ni de prompt assembly, ni de schema de sortie structuree
- pas d'API integrable propre pour un futur runtime de jeu
- pas d'accumulation progressive pendant l'inference
- pas encore de couche `SDL3` pour fenetre, transcript, ligne de commande parser et presentation temps reel du bitmap
- pas d'UI jouable, pas de transcript integre, pas de boucle narrative
- pas encore de bibliotheque de prefabs stables pour rack, portail, caisse, bloc de climatisation, etc.
- pas de sauvegarde/chargement
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
