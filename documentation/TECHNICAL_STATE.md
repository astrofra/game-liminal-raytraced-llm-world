# Technical State

Derniere mise a jour : 2026-07-30

## Resume

Le depot contient actuellement un premier module de rendu natif en C++11 centre sur une Cornell Box en niveaux de gris.

Ce module constitue un bootstrap du futur sous-systeme de rendu. Il ne s'agit pas encore du renderer final du jeu, mais d'une premiere base executable, compilable et documentee.

Sur le plan architectural, la cible d'inference retenue est maintenant `llama.cpp` avec acceleration CUDA autour de `Ministral 3 8B`, mais cette partie n'est pas encore integree dans le code du depot.

## Fonctionnalites presentes

- Build natif via CMake et Visual Studio 2022.
- Helper Windows `build_release.bat` a la racine pour configurer et compiler la version `Release`.
- Helper Windows `run_cornell_test.bat` a la racine pour compiler puis lancer le rendu de verification Cornell Box.
- Executable CLI `liminal_cornell_renderer`.
- Preset de rendu par defaut en `800x400` pour la scene liminale, avec cadrage panorama.
- Chargement de scene generique via `--scene <path>` pour `.scene` ou `.obj`.
- Parseur de format de scene proprietaire v1.
- Validation syntaxique de base pour `room`, `camera`, `spotlight`, `plane` et `box`.
- Spot analytique attache a la camera, avec panneau parametrique, portee limitee et cone progressif.
- Conversion des primitives `plane` et `box` vers le backend triangle/BVH existant.
- Premiere scene liminale handcraftee dans `assets/scenes/liminal_service_corridor.scene`.
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

## Fichiers importants

- [../CMakeLists.txt](/C:/works/projects/game-liminal-raytraced-llm-world/CMakeLists.txt:1) : configuration du build.
- [../src/core.h](/C:/works/projects/game-liminal-raytraced-llm-world/src/core.h:1) : types de base, math, RNG, configuration de rendu.
- [../src/scene.h](/C:/works/projects/game-liminal-raytraced-llm-world/src/scene.h:1) : structures de scene et BVH.
- [../src/scene.cpp](/C:/works/projects/game-liminal-raytraced-llm-world/src/scene.cpp:1) : chargement `.scene` et `.obj`, conversion des primitives, materiaux, lumiere Cornell, construction BVH.
- [../src/renderer.cpp](/C:/works/projects/game-liminal-raytraced-llm-world/src/renderer.cpp:1) : camera, spotlight analytique, intersections, visibilite, integrateur, export PNG/PGM.
- [../src/main.cpp](/C:/works/projects/game-liminal-raytraced-llm-world/src/main.cpp:76) : point d'entree CLI, parsing des options, telemetrie basique.
- [../assets/cornell/cornell_box.obj](/C:/works/projects/game-liminal-raytraced-llm-world/assets/cornell/cornell_box.obj:1) : scene de reference vendorisee.
- [../assets/cornell/cornell_box.mtl](/C:/works/projects/game-liminal-raytraced-llm-world/assets/cornell/cornell_box.mtl:1) : materiaux de reference.
- [../assets/scenes/liminal_service_corridor.scene](/C:/works/projects/game-liminal-raytraced-llm-world/assets/scenes/liminal_service_corridor.scene:1) : premiere scene proprietaire liminale.
- [SCENE_FORMAT_V1.md](./SCENE_FORMAT_V1.md) : description du format de scene implemente.
- [../vendor/stb/stb_image_write.h](/C:/works/projects/game-liminal-raytraced-llm-world/vendor/stb/stb_image_write.h:1) : sortie PNG vendorisee.
- [../vendor/legacy_rt2003/README.md](/C:/works/projects/game-liminal-raytraced-llm-world/vendor/legacy_rt2003/README.md:1) : provenance des idees reutilisees depuis le vieux projet.

## Commandes de build et d'execution

```powershell
cmake -S . -B build -G "Visual Studio 17 2022"
cmake --build build --config Release
.\build\Release\liminal_cornell_renderer.exe
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

Ces chiffres sont seulement des reperes de travail. Ils ne constituent pas encore un benchmark stable.

## Ce que ce module ne fait pas encore

- pas de scene v1 complete : seulement `plane` et `box` sont supportes
- pas de validation defensive ou simplification automatique d'une scene generee par LLM
- pas encore d'integration `llama.cpp` ni de chargement du modele `Ministral 3 8B`
- pas d'API integrable propre pour un futur runtime de jeu
- pas d'accumulation progressive pendant l'inference
- pas d'UI, pas de transcript, pas de boucle narrative
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
