# Project Journal

Journal chronologique des iterations de travail.

## 2026-07-29 - Iteration 0001 - Bootstrap du renderer Cornell Box

Objectif :

Demarrer le module de rendu du projet en s'appuyant sur l'ancien raytracer `toy-cpp-raytracer-2003`, sans en reprendre aveuglement la base.

Travail effectue :

- inspection du depot principal, alors quasi vide cote code
- inspection du vieux projet `C:\works\projects\toy-cpp-raytracer-2003`
- constat que les briques reutilisables sont surtout :
  - l'intersection rayon/AABB
  - l'intersection rayon/triangle
  - l'idee d'un loader OBJ minimal
- creation d'une nouvelle base C++11 avec CMake
- vendorisation locale de la Cornell Box OBJ/MTL
- ecriture d'un loader OBJ/MTL minimal
- reduction des materiaux Cornell en niveaux de gris
- ajout d'une BVH simple
- implementation d'un path tracer diffus simple avec lumiere directe et rebonds
- export de l'image en `PGM`
- documentation explicite de la provenance du code hybride

Resultat :

Le depot produit desormais un executable `liminal_cornell_renderer` capable de rendre une Cornell Box grayscale avec un peu de radiosite.

Observations :

- le rendu est visuellement lisible
- la structure du volume et les deux blocs sont bien percus
- l'image conserve un grain fort, conforme a l'intention generale
- quelques `fireflies` persistent, meme apres un clamp simple des contributions

Mesures relevees :

- `256x256`, `16 spp` : environ `1575.66 ms`
- `256x256`, `32 spp` : environ `4327.92 ms`
- `256x256`, `64 spp` : environ `6224.51 ms`

Ce qui a bien marche :

- repartir d'un noyau propre a ete plus rentable que reparer le code 2003
- la Cornell Box est un tres bon test de base
- le vendoring explicite de la provenance repond bien a l'objectif politique de code hybride humain/IA

Ce qui reste fragile :

- l'estimateur de lumiere directe est tres simple
- le bruit contient encore des points aberrants
- la scene n'est pas encore issue du futur format proprietaire a primitives
- l'esthetique finale du jeu demandera une lumiere camera et une spatialite plus pauvre ou plus etrange

Prochaine etape recommandee :

Faire converger ce bootstrap vers le vrai langage de scene du projet, puis adapter le modele d'eclairage vers la lumiere portee par la camera.

## 2026-07-29 - Iteration 0002 - Helper de build Windows

Objectif :

Ajouter un script simple a la racine du projet pour generer l'executable final via CMake et laisser la sortie terminal visible apres execution.

Travail effectue :

- ajout de `build_release.bat` a la racine
- configuration CMake via le generateur `Visual Studio 17 2022`
- compilation `Release` via `cmake --build`
- affichage explicite du chemin de l'executable genere
- terminaison par `pause`
- mise a jour de la documentation de build

Resultat :

Le projet peut maintenant etre compile sous Windows en double-cliquant le script ou en le lancant depuis un terminal.

Observation :

Le script reste volontairement simple et depend de la disponibilite de `cmake` dans le `PATH`.

## 2026-07-29 - Iteration 0003 - Helper Windows pour le test Cornell Box

Objectif :

Ajouter un script de test a la racine qui recompile le projet puis lance immediatement le rendu de verification de la Cornell Box.

Travail effectue :

- ajout de `run_cornell_test.bat`
- invocation de `build_release.bat` en mode chainable
- ajout d'un mode `NO_PAUSE` dans `build_release.bat` pour eviter une pause intermediaire quand il est appele par un autre batch
- lancement automatique de l'executable fraichement compile
- sortie du test vers `output\cornell_box_test.pgm`
- pause finale pour lire la sortie du terminal

Resultat :

Le projet peut maintenant etre rebuild puis teste via un seul batch a la racine.

Observation :

Le script cible un rendu de verification raisonnable en `256x256` et `32 spp`, pas un rendu final haute qualite.

## 2026-07-29 - Iteration 0004 - Scene format v1 et premiere scene liminale

Objectif :

Sortir du simple test OBJ de reference en implementant un premier format de scene proprietaire et une scene liminale handcraftee.

Travail effectue :

- ajout d'un chargeur generique `--scene` pour `.scene` et `.obj`
- implementation d'un parseur scene v1
- support des directives :
  - `room`
  - `camera`
  - `plane`
  - `box`
- support de `emit` pour les surfaces emissives
- support de `rot` sur les boites
- conversion des primitives vers le backend triangles/BVH existant
- ajout de `assets/scenes/liminal_service_corridor.scene`
- bascule du rendu par defaut vers la scene liminale
- adaptation de `run_cornell_test.bat` pour pointer explicitement vers l'OBJ Cornell
- verification du rendu Cornell apres refactorisation
- verification du rendu de la scene liminale

Resultat :

Le depot ne depend plus exclusivement d'un OBJ de reference pour exister visuellement. Il possede maintenant un pipeline scene v1 minimal et une premiere image proprietaire.

Observations :

- la scene liminale est lisible et deja plus specifique au projet que la Cornell Box
- le backend triangle/BVH existant se reutilise bien
- l'eclairage reste encore emissif dans la scene, pas attache a la camera
- le format de scene reste volontairement restreint

Mesures relevees :

- scene liminale, `256x256`, `24 spp`, `3 bounces` : environ `4295.14 ms`
- Cornell Box, `256x256`, `32 spp`, `3 bounces` : environ `4166.69 ms` apres refactorisation

Ce qui a bien marche :

- le passage `.scene` -> triangles ne demande pas de changer le renderer
- le test Cornell reste utile comme garde-fou de regression
- la scene liminale suffit deja a valider l'etape architecturale la plus importante

Ce qui reste fragile :

- pas encore de lumiere portee par la camera
- pas encore de schema contraint pour un futur LLM
- pas encore de support `sphere`, `cylinder`, `cone` ou `mesh reference`

Prochaine etape recommandee :

Faire converger le renderer vers le modele d'eclairage cible de la spec : une seule lumiere attachee a la camera, avec la scene proprietaire comme support principal.

## 2026-07-30 - Iteration 0005 - Spotlight camera et sortie PNG

Objectif :

Faire converger le renderer vers l'esthetique cible en ajoutant une lumiere attachee a la camera, puis sortir les images en PNG.

Travail effectue :

- vendorisation de `stb_image_write.h`
- ajout du support `PNG` dans le renderer
- conservation du `PGM` selon l'extension du fichier de sortie
- ajout d'une structure `CameraSpotlight`
- ajout du support `spotlight` dans le format `.scene`
- implementation d'un spot analytique attache a la camera :
  - petit panneau lumineux parametrique
  - orientation vers l'avant
  - portee limitee
  - cone progressif
- adaptation de la scene liminale pour supprimer l'emission du plafond et activer le spotlight camera
- adaptation de `run_cornell_test.bat` pour sortir en PNG
- bascule du preset de rendu par defaut vers `800x400` en panorama
- verification du rendu Cornell apres la refonte
- verification du rendu liminal avec le nouveau spot
- rendu panorama `800x400` de la scene liminale

Resultat :

Le renderer produit maintenant des PNG en natif et la scene proprietaire est eclairee par un spot fixe a la camera, conforme a l'orientation generale de la spec.

Observations :

- la Cornell Box reste stable comme scene de regression
- la scene liminale ne depend plus d'une surface emissive de plafond
- le spotlight camera fonctionne, mais demande encore une calibration artistique
- le panorama `800x400` ouvre mieux la scene lateralement
- le preset panorama devient maintenant le comportement par defaut du binaire

Mesures relevees :

- scene liminale, `256x256`, `32 spp`, `3 bounces` : environ `5430.59 ms`
- Cornell Box, `256x256`, `32 spp`, `3 bounces` : environ `4061.97 ms`
- scene liminale panorama, `800x400`, `32 spp`, `3 bounces` : environ `35748.60 ms`

Ce qui a bien marche :

- la sortie PNG simplifie immediatement l'usage
- le spotlight camera rapproche nettement l'architecture du renderer de la spec
- la separation entre Cornell de regression et scene proprietaire s'est renforcee

Ce qui reste fragile :

- le spotlight demande encore du tuning
- le format de scene v1 reste volontairement incomplet
- les `fireflies` n'ont pas disparu

Prochaine etape recommandee :

Calibrer le spotlight camera et la composition de la scene liminale avant d'elargir encore le vocabulaire des primitives.

## 2026-07-30 - Iteration 0006 - Etude de faisabilite pour le parallelisme CPU

Objectif :

Evaluer s'il est raisonnable d'activer un rendu multi-coeur dans le renderer sans alourdir la logique du code, en privilegiant une solution a base de macros compile-time.

Travail effectue :

- inspection du coeur de rendu dans `src/renderer.cpp`
- verification des points de partage memoire :
  - scene en lecture seule
  - BVH en lecture seule
  - RNG local par pixel
  - buffer image ecrit par cases disjointes
- comparaison entre plusieurs approches :
  - OpenMP
  - `std::thread`
  - `std::async`
  - vrai multi-processus
- redaction d'une note de faisabilite dediee dans `documentation/MULTIPROCESSING_FEASIBILITY.md`

Resultat :

Le renderer actuel est un bon candidat pour une parallelisation CPU a faible intrusion, mais pas pour un vrai multi-processus sous la contrainte "juste avec des macros".

Observation :

La recommandation de travail est d'utiliser un mode OpenMP optionnel, borne a la boucle externe sur les lignes de rendu, avec une macro unique et un fallback mono-thread.

## 2026-07-30 - Iteration 0007 - Validation de la cible d'inference locale

Objectif :

Lever l'ambiguite restante sur le couple modele/runtime afin de stabiliser la suite de l'implementation.

Travail effectue :

- validation du choix `llama.cpp` comme runtime d'inference
- validation du mode CUDA comme profil d'acceleration principal sur la machine de developpement actuelle
- validation de `Ministral 3 8B Instruct 2512` comme cible modele v1
- alignement de la documentation de reference sur ce choix

Resultat :

Le projet dispose maintenant d'une cible runtime explicite pour la couche narrative locale. La spec longue n'est plus en tension avec la note technique CUDA sur le point du modele.

Observations :

- ce choix clarifie le budget memoire et le profil de performance a viser
- il ne supprime pas les questions de redistribution, de taille de package et de fallback CPU
- il rend maintenant prioritaire l'integration effective de `llama.cpp` dans le binaire ou dans un chemin de validation local reproductible

Prochaine etape recommandee :

Construire une premiere boucle verticale headless qui enchaine chargement du modele, prompt structure, validation du tour, mise a jour d'etat, scene v1 et rendu.

## 2026-07-30 - Iteration 0008 - Vendorisation de `llama.cpp` et verification CUDA locale

Objectif :

Transformer la decision runtime en integration de depot verifiable, tout en confirmant l'etat reel du toolchain CUDA sur cette machine.

Travail effectue :

- verification locale de `nvidia-smi`
- verification locale de `nvcc --version`
- constat que la machine expose une `RTX 4060` et un toolkit CUDA `13.0`
- ajout d'une copie vendorisee de `llama.cpp` dans `vendor/llama.cpp`
- ajout d'un fichier `VENDORED_COMMIT.txt` pour tracer le commit exact
- extension du `CMakeLists.txt` principal avec :
  - `LIMINAL_ENABLE_LLAMA_CPP`
  - `LIMINAL_ENABLE_LLAMA_CUDA`
- ajout d'une couche minimale `src/llm_runtime.h/.cpp`
- ajout d'une option CLI `--llama-info`
- ajout du script `scripts/download_ministral.py`
- extension de `.gitignore` pour les poids et les builds `llama.cpp`
- verification CMake configure + build `Release`
- verification du binaire final avec `--llama-info`

Resultat :

Le depot peut maintenant compiler un executable `liminal_cornell_renderer` qui integre `llama.cpp` et confirme localement la disponibilite de l'offload GPU via CUDA.

Observations :

- la compilation CUDA de `llama.cpp` est lourde mais aboutit
- le binaire final signale correctement :
  - `llama.cpp compiled in: yes`
  - `GPU offload available: yes`
  - la detection de la `RTX 4060`
- l'inference reelle du modele n'est pas encore branchee a la boucle du jeu

Prochaine etape recommandee :

Ajouter un premier chargement de modele et une interface headless minimale de type `prompt -> completion structuree`, avant de lier cela a l'etat de jeu et au renderer.

## 2026-07-30 - Iteration 0009 - Helper Windows pour interroger le modele local

Objectif :

Ajouter un point d'entree simple pour verifier rapidement le modele telecharge sans retaper toute la ligne `llama-cli`.

Travail effectue :

- ajout de `ask_ministral.bat` a la racine
- verification du chemin par defaut vers :
  - `vendor\llama.cpp\build-cuda\bin\Release\llama-cli.exe`
  - `models\ministral-3-8b\Ministral-3-8B-Instruct-2512-Q4_K_M.gguf`
- support d'une question passee en argument ou saisie interactivement
- ajout d'un preset de lancement raisonnable :
  - `--n-gpu-layers auto`
  - `--ctx-size 4096`
  - `--flash-attn on`
  - `--single-turn`
- mise a jour de la documentation d'usage
- verification pratique avec une question libre

Resultat :

Le depot dispose maintenant d'un helper minimal pour faire un smoke test du LLM local en une seule commande.

Observation :

Le chemin `ask_ministral.bat "Quel est le sens de la vie ?"` suffit maintenant a valider rapidement que le modele, `llama-cli` et le backend CUDA cooperent bien.

## 2026-07-31 - Iteration 0010 - Verrouillage de la couche multimedia cible

Objectif :

Fermer le dernier grand choix d'infrastructure cote I/O et HMI avant d'attaquer la boucle de jeu interactive.

Travail effectue :

- comparaison des options `SDL3`, `libretro`, `GLFW` et `sokol`
- evaluation du besoin reel du projet :
  - fenetre native multi-OS
  - saisie texte clavier de type parser
  - historique de commandes
  - affichage d'un bitmap produit par le renderer CPU
  - audio simple possible plus tard
- validation de `SDL3` comme couche multimedia principale
- rejet de `libretro` comme architecture primaire, tout en le laissant comme piste de port secondaire eventuelle
- mise a jour de la documentation d'architecture et d'etat technique

Resultat :

Le projet dispose maintenant d'un choix explicite et stable pour la future couche de fenetre, d'evenements, de texte et de presentation : `SDL3`.

Observations :

- `SDL3` colle bien a une fiction interactive desktop avec viewport image + transcript + ligne de commande
- `libretro` reste interessant pour une diffusion ecosystemique, mais pas comme coeur d'application a ce stade
- ce verrou retire une ambiguite importante avant l'implementation de la premiere tranche verticale jouable

Prochaine etape recommandee :

Commencer la boucle verticale `GameState -> input texte -> appel LLM -> resultat structure -> scene -> rendu -> presentation SDL3`.

## 2026-07-31 - Iteration 0011 - Plan de validation spatiale des lieux canoniques

Objectif :

Preparer la premiere vraie validation du lien entre fiction du datacenter, texte de lieu, scene v1 et rendu 3D.

Travail effectue :

- relecture du format actuel dans `assets/scenes/liminal_service_corridor.scene`
- relecture de `SCENE_FORMAT_V1.md`
- recoupement avec `STORY.md` pour identifier les lieux les plus structurants
- formalisation d'un plan de validation dedie
- choix de trois lieux canoniques :
  - portail d'entree
  - travees de serveurs
  - toit / tour de ronde
- explicitation des limites actuelles du format pour le ciel, l'horizon et les exterieurs
- mise a jour de l'index documentaire et des issues connues

Resultat :

Le projet dispose maintenant d'un protocole explicite pour tester la chaine `scenario -> texte -> scene -> rendu` avant de brancher la generation spatiale du LLM.

Observations :

- la scene de corridor existante reste un bon point de controle interieur
- le portail et surtout le toit vont servir de tests de stress du format v1
- la question du ciel sombre et du degrade est probablement le meilleur revelateur des limites actuelles

Prochaine etape recommandee :

Ecrire les trois scenes canoniques a la main, produire leurs rendus de reference, puis comparer ce qui tient ou casse entre brief narratif et image.

## 2026-07-31 - Iteration 0012 - Fixtures spatiales canoniques et rendus de reference

Objectif :

Passer du plan de validation a une premiere implementation concrete des trois lieux canoniques.

Travail effectue :

- ecriture de `assets/scenes/datacenter_entry_gate.scene`
- ecriture de `assets/scenes/datacenter_server_aisles.scene`
- ecriture de `assets/scenes/datacenter_roof_watch.scene`
- rendu smoke test des trois scenes en basse resolution
- rendu de reference des trois scenes en `800x400`, `16 spp`
- inspection visuelle des PNG produits

Resultat :

Le depot contient maintenant trois fixtures spatiales explicites qui couvrent :

- le seuil exterieur
- l'interieur technique dense
- le point haut d'observation vers le desert

Observations :

- le portail d'entree lit correctement comme un exterieur ferme et controle
- les travees de serveurs produisent une interiorite dense et repetitive, mais restent encore plus architecturales que semantiquement "serveurs"
- le toit fonctionne comme poste d'observation, mais le ciel sombre est obtenu par empilement de plans emissifs plutot que par un vrai fond ou degrade
- les trois scenes sont distinguables sans aide textuelle, ce qui valide la methode de fixtures manuelles

Mesures relevees :

- portail d'entree, `800x400`, `16 spp`, `3 bounces` : environ `16207.52 ms`
- travees de serveurs, `800x400`, `16 spp`, `3 bounces` : environ `11063.99 ms`
- toit / tour de ronde, `800x400`, `16 spp`, `3 bounces` : environ `9253.65 ms`

Ce qui a bien marche :

- le vocabulaire `plane` + `box` suffit deja a distinguer trois regimes spatiaux nets
- la scene du toit prouve qu'un horizon pauvre peut exister visuellement, meme de facon tres contrainte
- la batterie de tests rend maintenant visible ce que le format sait ou ne sait pas traduire

Ce qui reste fragile :

- le desert et le ciel reposent encore sur des approximations de composition
- l'interieur du datacenter reste typologiquement juste, mais pas encore assez signe comme "baies de serveurs"
- le lien entre description textuelle et scene n'est pas encore automatise par un contrat de donnees

Prochaine etape recommandee :

Comparer chaque fixture a un brief narratif court, documenter les pertes de traduction, puis decider des extensions minimales du format `scene v1`.

## 2026-07-31 - Iteration 0013 - Fond proceduriel de ciel pour les exterieurs

Objectif :

Remplacer les faux plans de ciel des scenes exterieures par un vrai fond proceduriel mieux adapte au toit du datacenter et a l'entree.

Travail effectue :

- ajout d'une structure `SkyBackground` au runtime
- ajout d'une directive `sky` dans le parseur `.scene`
- ajout d'un fond procedural grayscale dans le renderer pour les rayons sans intersection
- support d'un horizon plus clair entre un zenith et un nadir sombres
- ajout de grain fort et d'etoiles deterministes
- adaptation de `datacenter_entry_gate.scene` et `datacenter_roof_watch.scene` pour utiliser `sky` au lieu de panneaux emissifs de fond
- validation renderer via un build dedie `build_skytest` avec `LIMINAL_ENABLE_LLAMA_CPP=OFF`

Resultat :

Le ciel n'est plus simule par empilement de plans geometriques. Les scenes exterieures disposent maintenant d'un fond procedural plus souple, plus doux dans son degrade, et mieux raccorde au grain general du renderer.

Observations :

- la lecture du toit s'ameliore nettement grace a un horizon plus plausible
- le portail gagne en ambiance exterieure avec un ciel plus vivant
- les etoiles restent discretes, ce qui est preferable a ce stade a un effet demonstratif trop visible
- l'interieur des travees n'est pas affecte, ce qui confirme que la directive reste bien optionnelle

Mesures relevees :

- portail d'entree, `800x400`, `16 spp`, `3 bounces` : environ `8295.89 ms`
- toit / tour de ronde, `800x400`, `16 spp`, `3 bounces` : environ `6325.08 ms`

Ce qui a bien marche :

- le renderer gere mieux les scenes ouvertes sans geometie de fond artificielle
- le degrade du ciel est plus proche de l'intention de fin de journee
- la stabilite visuelle des exterieurs augmente

Ce qui reste fragile :

- le desert lui-meme reste peu semantique
- les etoiles pourraient encore etre affinees artistiquement
- le projet ne dispose pas encore d'une bibliotheque de prefabs stables pour les objets recurrents

Prochaine etape recommandee :

Commencer une petite couche de prefabs stables pour `rack`, `gate`, `crate`, `cooling_unit` et autres objets recurrents du datacenter.

## 2026-07-31 - Iteration 0014 - Premiere couche de prefabs stables

Objectif :

Ajouter un niveau intermediaire plus semantique entre la scene ecrite et les primitives brutes du renderer.

Travail effectue :

- ajout de prefabs expands en `box` dans le parseur `.scene`
- premiere bibliotheque introduite :
  - `prefab_gate`
  - `prefab_rack`
  - `prefab_crate`
  - `prefab_cooling_unit`
- conversion partielle des scenes canoniques pour utiliser ces prefabs
- validation par build dedie `build_skytest`
- rerendus des trois scenes de reference

Resultat :

Le projet dispose maintenant d'une premiere couche de stabilisation semantique du decor. Les scenes parlent un peu moins en geometrie brute et un peu plus en objets recurrents.

Observations :

- les travees du datacenter lisent mieux comme rangees de racks
- le portail beneficie d'une definition plus stable et plus reusable
- le toit gagne quelques objets techniques recurrents plus convaincants
- le cout geometrique augmente fortement, surtout sur les travées

Mesures relevees :

- portail d'entree avec prefab gate, `800x400`, `16 spp`, `3 bounces` : environ `8331.42 ms`
- travees avec prefabs racks / cooling / crate, `800x400`, `16 spp`, `3 bounces` : environ `23903.92 ms`
- toit avec prefabs crate / cooling, `800x400`, `16 spp`, `3 bounces` : environ `7797.88 ms`

Ce qui a bien marche :

- la semantique spatiale devient plus lisible
- le decor peut maintenant rester instable sans perdre tous ses repères
- la couche prefab donne une meilleure base pour un futur LLM que des dizaines de `box` isolement anonymes

Ce qui reste fragile :

- les prefabs sont encore axis-alignes
- le nombre de triangles et de materiaux grimpe vite
- il n'existe pas encore de couche de repetition ou d'instanciation compacte

Prochaine etape recommandee :

Factoriser la bibliotheque prefab, puis reduire son cout en introduisant soit des materiaux partages, soit une notion de repetition modulaire plus compacte.

## 2026-07-31 - Iteration 0015 - OpenMP optionnel et preset de travail plus rapide

Objectif :

Reduire le temps de rendu des scenes denses sans engager tout de suite une refonte architecturale du renderer.

Travail effectue :

- ajout de l'option CMake `LIMINAL_ENABLE_OPENMP`, activee par defaut
- integration conditionnelle de `find_package(OpenMP)` avec fallback mono-thread si indisponible
- parallelisation de la boucle externe de rendu par lignes dans `src/renderer.cpp`
- ajustement du logging de progression pour le mode OpenMP
- abaissement du preset par defaut de `32 spp` a `16 spp`
- generation de deux builds de comparaison sans `llama.cpp` :
  - `build_noomp`
  - `build_omp`
- benchmark comparatif sur la scene canonique la plus lourde
- verification de l'identite binaire des PNG produits avec la meme seed

Resultat :

Le renderer dispose maintenant d'un mode multi-coeur optionnel a faible intrusion. Sur la scene des travees de serveurs, le gain observe est tres important sans changement d'image detecte sur le test mene.

Observations :

- la baisse a `16 spp` renforce le grain mais reste compatible avec l'intention visuelle actuelle
- OpenMP soulage fortement le cout des prefabs denses
- la cause structurelle du cout geometrique n'est pas supprimee pour autant
- le fallback mono-thread reste utile pour conserver un comportement simple et portable

Mesures relevees :

- travees de serveurs, `800x400`, `16 spp`, `3 bounces`, mono-thread : environ `24376.07 ms`
- travees de serveurs, `800x400`, `16 spp`, `3 bounces`, OpenMP, `16` threads annonces : environ `2708.34 ms`
- hash SHA-256 identique entre `output\datacenter_server_aisles_noomp.png` et `output\datacenter_server_aisles_omp.png`

Ce qui a bien marche :

- le parallelisme par lignes se branche proprement sur l'architecture existante
- le speed-up observe est largement suffisant pour justifier l'option
- le determinisme du rendu reste bon sur le cas teste

Ce qui reste fragile :

- OpenMP depend encore du compilateur et de la configuration locale
- les prefabs restent trop couteux en triangles et en materiaux
- la progression par ligne est moins fine en mode parallele

Prochaine etape recommandee :

Attaquer la reduction du cout structurel des prefabs, par exemple via materiaux partages, repetition modulaire ou instanciation plus compacte.

## 2026-07-31 - Iteration 0017 - Palette RGB semantique verrouillee

Objectif :

Quitter le grayscale strict sans ouvrir un renderer couleur libre, afin de donner au monde une signature visuelle plus stable et plus lisible.

Travail effectue :

- passage du path tracer interne de la luminance scalaire a une radiance `Vec3`
- sortie `PNG` et buffer memoire `RenderSceneToPixels()` convertis en RGB
- conservation du `PGM` legacy par projection en luminance
- ajout d'une palette semantique verrouillee :
  - ciel bleu degrade
  - desert ocre via nommage `ground` / `desert_*` / `ridge_*` / `outcrop_*`
  - LEDs rouges injectees automatiquement dans `prefab_rack`
  - reste du decor en gris
- retrait des LEDs du sampling de lumiere directe pour eviter qu'elles ne degradent tout le rendu interieur par un bruit rouge excessif
- mise a jour du prompt de regles `.scene` pour expliquer que `gray()` pilote la luminance, pas une palette libre

Resultat :

Le renderer garde son austerite et son grain, mais il sort maintenant du monochrome pur. Les exterieurs lisent mieux, les racks gagnent des points d'ancrage visuels, et le LLM reste cadre par une palette que le moteur applique lui-meme.

Observations :

- le changement le plus couteux n'etait pas la texture SDL mais bien le passage du chemin de lumiere en `Vec3`
- les LEDs rouges fonctionnent mieux comme accents visibles que comme vraies lampes de scene
- la contrainte semantique reste preferable a une syntaxe `color()` ouverte pour l'etape actuelle

## 2026-07-31 - Iteration 0016 - Cadrage fonctionnel de la boucle de tour et de la generation de scene

Objectif :

Clarifier la prochaine grande inconnue du projet : l'articulation entre memoire du monde, texte genere, continuite du recit et generation de scenes rendables.

Travail effectue :

- clarification documentaire sur les vraies priorites de la preuve de concept
  - latence d'inference
  - temps de rendu CPU par tour
- notation explicite que le temps de fabrication runtime des scenes et l'occupation RAM ne sont pas encore les priorites critiques
- decision de ne pas prendre comme voie principale une generation libre de scene complete par le LLM
- formalisation d'une strategie `hard state -> soft state -> spatial state -> scene v1`
- ajout d'un document dedie `FUNCTIONAL_PIPELINE_V1.md`
- mise a jour de la documentation d'etat, des decisions et des risques ouverts

Resultat :

Le projet dispose maintenant d'un cadrage explicite pour attaquer la boucle verticale fonctionnelle sans confondre d'emblee recit, memoire, geographie et geometrie brute.

Observations :

- la vraie difficulte n'est pas seulement de faire produire du texte au modele
- le point sensible est la traduction stable entre ce qui doit rester actionnable et ce qui peut rester flottant
- `Ministral` a `temperature 0` parait mieux adapte a des deltas structures qu'a une scene complete libre

Ce qui a bien marche :

- la distinction `hard state` / `soft state` / `spatial state` rend le probleme beaucoup plus lisible
- le recentrage sur le volet fonctionnel evite de surinvestir trop tot dans les optimisations profondes du renderer
- la voie deterministe `spatial state -> scene v1` offre une meilleure surface de validation

Ce qui reste fragile :

- aucun contrat de tour n'est encore implemente
- aucun compilateur de scene depuis l'etat spatial n'existe encore
- la generation de nouveaux lieux hors fixtures canoniques reste entierement ouverte

Prochaine etape recommandee :

Definir les structures C++ du monde, le schema de sortie structuree de `Ministral`, puis brancher une premiere boucle headless `commande -> tour -> scene -> rendu`.

## 2026-07-31 - Iteration 0017 - Noyau fonctionnel en C++ et audit memoire des scenes candidates

Objectif :

Traduire le cadrage fonctionnel en premieres briques executables cote moteur, avant l'integration complete de l'inference.

Travail effectue :

- ajout de `src/game_state.h/.cpp`
  - `HardState`
  - `SoftState`
  - `SpatialState`
  - `HardStateDelta`
  - `SpatialStateDelta`
  - `TurnResult`
- ajout de `src/turn_contract.h/.cpp`
  - schema de sortie structuree v1
  - prompt de tour structure
  - prompt d'audit direct `.scene`
- ajout de `src/scene_compiler.h/.cpp`
  - etats spatiaux canoniques pour `gate`, `server_aisles`, `roof_watch`
  - compilation deterministe de ces lieux vers une `Scene`
  - point d'audit d'une scene candidate fournie comme texte
- extension du parseur scene pour accepter aussi une scene `v1` en memoire, pas seulement depuis un fichier
- extension de la CLI avec :
  - `--dump-turn-contract`
  - `--dump-scene-audit-prompt`
  - `--compile-location`
  - `--audit-scene-text`
- build de verification sans `llama.cpp`
- test de compilation du lieu `gate` depuis `SpatialState`
- test d'audit memoire de `datacenter_roof_watch.scene`

Resultat :

Le projet dispose maintenant d'un premier noyau fonctionnel executable pour preparer la vraie boucle verticale. Le pipeline final n'a plus besoin d'etre pense comme un echange par fichiers entre LLM et raytracer : les scenes candidates peuvent etre validatees directement depuis un bloc texte en memoire.

Observations :

- la distinction entre chemin principal structure et chemin d'audit `.scene` devient nette
- le compilateur canonique repose encore sur les fixtures existantes, ce qui est suffisant pour un v1 defensif
- la scene libre generee par le modele peut maintenant etre auditee sans devenir le bus interne obligatoire du moteur

Mesures relevees :

- `--compile-location gate`, `800x400`, `16 spp`, `3 bounces` : environ `1387.04 ms`
- `--audit-scene-text assets\scenes\datacenter_roof_watch.scene`, `800x400`, `16 spp`, `3 bounces` : environ `1481.94 ms`

Ce qui a bien marche :

- le passage scene texte -> parseur memoire -> rendu fonctionne
- les prompts v1 deviennent inspectables et versionnables
- les trois lieux canoniques ont maintenant une representation exploitable cote etat spatial

Ce qui reste fragile :

- aucun appel `Ministral` n'est encore branche sur ce contrat
- aucun parseur JSON de `TurnResult` n'est encore implemente
- l'application des deltas de monde reste a coder

Prochaine etape recommandee :

Brancher un premier tour headless reel avec `Ministral`, parser la sortie structuree, puis appliquer les deltas sur `HardState` et `SpatialState`.

## 2026-07-31 - Iteration 0018 - Premier tour headless `Ministral` et audit `.scene` separe

Objectif :

Fermer la premiere boucle verticale reelle entre commande texte, inference locale, etat du monde, compilation de scene et rendu.

Travail effectue :

- extension de `src/llm_runtime.h/.cpp` pour charger le modele GGUF et generer une completion locale via `llama.cpp`
- ajout de `src/turn_runner.h/.cpp`
- parsing JSON de `TurnResult` avec extraction tolerante d'un objet meme si le modele ajoute des fences Markdown
- application des deltas sur `HardState`, `SoftState` et `SpatialState`
- ajout des options CLI :
  - `--run-turn`
  - `--model`
  - `--llm-temperature`
  - `--llm-predict`
  - `--use-json-grammar`
  - `--no-json-grammar`
  - `--prefer-candidate-scene`
  - `--dump-raw-turn`
- premier essai d'inclusion d'une `.scene` candidate directement dans le JSON de tour
- constat que cette voie etait trop fragile
- refonte immediate vers une strategie a deux appels :
  - appel 1 : JSON structure du tour
  - appel 2 : `.scene` candidate pour audit seulement
- validation complete locale sur `roof_watch`

Resultat :

Le projet sait maintenant executer un vrai tour headless avec `Ministral`, produire une narration structuree, mettre a jour l'etat spatial, compiler une scene canonique et rendre l'image correspondante. Il sait aussi demander au modele une `.scene` candidate separee puis l'auditer en memoire.

Observations :

- la voie `JSON` seule est relativement stable a `temperature 0`
- l'encapsulation d'un mini-programme `.scene` dans ce meme JSON degradait fortement la robustesse
- un deuxieme appel scene-audit dedie est beaucoup plus defendable pour le v1
- la scene candidate n'est pas encore la source d'autorite du rendu final

Mesures relevees :

- `--run-turn --location roof_watch --command "observe the horizon"` :
  - `828` tokens de prompt
  - `365` tokens generes pour le JSON
  - environ `8797.61 ms` d'inference pour le tour structure
  - `.scene` candidate validee a part : `338` triangles, `29` materiaux
  - rendu final compile depuis `SpatialState` : environ `829.42 ms`

Ce qui a bien marche :

- la chaine memoire `LLM -> etat -> scene -> rendu` fonctionne reellement
- l'audit `.scene` separe donne un bon point de controle pour juger la qualite spatiale du modele
- la distinction entre voie principale deterministe et voie experimentale libre devient concrete

Ce qui reste fragile :

- le runtime reste mono-tour et sans persistance
- le modele continue parfois a renvoyer des fences Markdown autour du JSON
- la grammaire JSON `llama.cpp` n'est pas encore exploitee de facon fiable dans cette integration Windows/CUDA

Prochaine etape recommandee :

Enchainer sur une vraie boucle multi-tour avec historique, et introduire un validateur / reparateur plus riche pour les deltas spatiaux et les scenes candidates.

## 2026-07-31 - Iteration 0019 - Session multi-tour persistante et fallback no-op

Objectif :

Passer du simple tour headless a une vraie session chargeable/sauvegardable, capable de survivre a une sortie LLM mal formee.

Travail effectue :

- ajout de `SessionState` et `SessionTurnRecord`
- serialisation / deserialisation JSON de la session
- chargement / sauvegarde via CLI
- historique recent injecte dans le prompt de tour
- ajout de `RunHeadlessTurnFromState()` et `UpdateSessionStateFromTurn()`
- ajout des options CLI :
  - `--run-session`
  - `--command-file`
  - `--load-state`
  - `--save-state`
  - `--dump-session-state`
  - `--dump-session-history`
- ajout d'un repair LLM pour tenter de recuperer un JSON de tour mal forme
- ajout d'un fallback no-op si parse initial + repair echouent
- validation pratique d'une session a deux tours puis d'une reprise depuis JSON

Resultat :

Le projet sait maintenant enchaîner plusieurs commandes sur un meme etat de monde, sauvegarder cet etat, le recharger plus tard, et continuer sans repartir du lieu canonique initial.

Observations :

- le repair ne suffit pas toujours
- le fallback no-op remplit bien son role de garde-fou de jouabilite
- l'historique recent dans le prompt augmente le contexte disponible pour le modele

Mesures relevees :

- session `observe the horizon` puis `inspect the cooling unit` :
  - `2` tours executes
  - `19669.71 ms` d'inference totale
  - fallback no-op declenche au second tour
- reprise `check the crate` depuis le JSON sauvegarde :
  - prompt `1079` tokens
  - generation `448` tokens
  - environ `10942.42 ms` d'inference

Ce qui a bien marche :

- la persistance JSON est suffisante pour un v1 headless
- le moteur ne crash plus sur une sortie de tour inexploitable
- le dernier rendu reste bien derive de la voie compilee deterministe

Ce qui reste fragile :

- le fallback no-op reste esthetiquement brut
- la scene candidate libre du modele reste souvent invalide
- la session ne gere pas encore de boucle temps reel, d'entree interactive ou de presentation SDL3

Prochaine etape recommandee :

Attaquer une vraie boucle de jeu interactive locale, d'abord en terminal enrichi ou directement via SDL3, en reutilisant la session persistante comme noyau.

## 2026-07-31 - Iteration 0020 - Premiere boucle SDL3 avec streaming LLM

Objectif :

Passer de la session headless persistante a une premiere boucle desktop jouable, sans perdre le streaming de sortie du modele pendant l'attente.

Travail effectue :

- ajout de `src/sdl_frontend.h/.cpp`
- ajout de l'option CLI `--sdl`
- ajout d'un callback de streaming dans `src/llm_runtime.h/.cpp`
- propagation de ce streaming a travers `HeadlessTurnConfig` et `RunHeadlessTurnFromState()`
- ajout d'un chemin `RenderSceneToPixels()` pour produire directement un buffer memoire au lieu d'un fichier image
- integration `SDL3` dans `CMakeLists.txt` :
  - `find_package(SDL3)` si disponible
  - sinon `FetchContent` automatique sur `SDL 3.4.12`
- validation de build locale `Release`

Resultat :

Le projet dispose maintenant d'une premiere HMI desktop. Le haut de la fenetre affiche le rendu raytrace courant, le bas affiche le transcript de session et une ligne de commande, et le flux brut du modele reste visible pendant la generation du JSON de tour.

Observations :

- le streaming reduit bien l'impression d'attente des que la generation commence
- le cout principal restant est le chargement du modele et le raytracing CPU, pas le parse des scenes
- la premiere couche texte repose sur `SDL_RenderDebugText`, suffisante pour valider la boucle mais pas encore pour une finition esthetique

Mesures relevees :

- recompilation locale du vendredi 31 juillet 2026 :
  - detection explicite du toolkit CUDA pendant la configuration CMake (`CUDA Toolkit found`)
  - build `Release` complet avec `SDL3`, `llama.cpp`, `CUDA` et `OpenMP` valide

Ce qui a bien marche :

- la boucle UI reste responsive pendant l'inference et pendant le raytracing
- la session persistante existante a pu etre reutilisee comme noyau sans refonte lourde
- l'interface memoire `SpatialState -> Scene -> pixels -> texture SDL3` est maintenant concrete

Ce qui reste fragile :

- le streaming montre encore le JSON brut du modele, pas une narration deja interpretee
- l'UI est volontairement spartiate
- il n'y a pas encore d'accumulation progressive du rendu ni de cache modele resident entre les tours

Prochaine etape recommandee :

Stabiliser le frontend SDL3 autour d'un protocole de sortie a deux canaux :

- canal live pour l'attente perceptive
- canal structure pour l'etat du monde et le rendu final

## 2026-07-31 - Iteration 0021 - Salles improvisees et graphe cardinal persistant

Objectif :

Mettre a l'epreuve le concept central du jeu en laissant le LLM fabriquer une nouvelle salle a la volee sur un deplacement cardinal vers un espace encore inconnu.

Travail effectue :

- extension de `SessionState` avec :
  - `current_place_id`
  - `generated_rooms`
  - `room_links`
  - compteur `next_generated_room_index`
- ajout d'un branchement special dans `RunHeadlessTurnFromState()` pour `NORTH`, `EAST`, `SOUTH`, `WEST`
- generation d'un nouveau voisin uniquement si aucun lien n'existe encore dans cette direction
- cache persistant du `scene_text` pour recharger une salle deja inventee sans repasser par le modele
- retour possible d'une salle generee vers un lieu canonique via les liens reciproques
- ajout de fallbacks :
  - metadata de salle si le JSON de room generation est inutilisable
  - scene de salle si le `.scene` genere reste invalide

Resultat :

Le projet peut maintenant improviser une salle 3D "a partir de rien" lors d'un deplacement cardinal vers un espace inexplore, memoriser cette salle, puis y revenir plus tard dans la meme session.

Observations :

- le principe de "monde non predetermine" fonctionne maintenant sur une tranche v0 exploitable
- la topologie minimale locale est preservee grace au graphe de liens cardinaux
- la partie la plus fragile reste la generation du JSON de metadata de salle, plus instable que le simple rendu de scene fallback

Validation pratique :

- session `NORTH -> EAST -> WEST` validee localement le vendredi 31 juillet 2026
- resultat observe :
  - `2` salles generees persistantes
  - retour `WEST` vers la salle precedente sans inference LLM supplementaire
- reprise `SOUTH` depuis la salle nord validee vers `gate`

Prochaine etape recommandee :

Faire monter d'un cran la qualite des salles improvisees :

- repair dedie du JSON de room generation
- mise a jour optionnelle de la scene d'une salle existante apres certaines actions locales
- etiquette visuelle explicite des sorties disponibles dans le transcript ou l'HMI

## 2026-07-31 - Iteration 0018 - Lisibilite de la GUI SDL3 avec fontes TTF

Objectif :

Rendre la zone textuelle joueur plus lisible sans perdre la separation entre interface fictionnelle et instrumentation debug.

Travail effectue :

- integration de `SDL3_ttf` dans la build SDL3
- chargement de `assets/fonts/Zilla_Slab/ZillaSlab-Regular.ttf` pour le transcript et la saisie
- chargement de `assets/fonts/Zilla_Slab_Highlight/ZillaSlabHighlight-Regular.ttf` pour les segments `*highlightes*`
- conservation de `SDL_RenderDebugText` pour le titre et la barre de statut
- remplacement du wrapping par nombre de caracteres par un wrapping fonde sur la largeur reelle du texte
- adaptation du champ de saisie a une largeur proportionnelle avec curseur mesure en pixels

Resultat :

La GUI joueur n'est plus rendue avec la petite fonte bitmap monospaced. Le texte narratif et la saisie utilisent maintenant une fonte bien plus lisible, tandis que la couche debug garde son caractere brut et instrumental.

Observations :

- la distinction entre zone fictionnelle et zone debug devient plus nette
- le support UTF-8 cote joueur est meilleur qu'avec `SDL_RenderDebugText`
- la convention `*...*` devient une facon simple d'obtenir des accents visuels sans ouvrir un systeme de style plus lourd

## 2026-08-09 - Iteration 0022 - Réorientation documentaire vers Eryx

Objectif :

Reconsidérer la direction artistique et de recherche du projet sans réécrire son histoire technique ni prétendre que le runtime a déjà migré.

Contexte immédiatement précédent :

La phase *Le Désert des tokens* mettait en scène un officier affecté à un datacenter autonome dans le désert, dans l'attente d'une cyberattaque indéterminée. Le calcul du centre chauffait et consommait ses ressources, tandis que la température fictionnelle devait influencer la température d'échantillonnage du modèle. Cette boucle reliait infrastructure numérique, écologie, attente institutionnelle, tokens et dérive interprétative.

Ce concept reste une étape légitime. Il a aidé à préciser :

- l'inversion de charge entre inférence GPU et image CPU contrainte
- la visibilité de la latence et de la computation
- la séparation entre état autoritatif et narration générée
- le potentiel esthétique de la cohérence locale et de la dérive globale
- l'intérêt d'une version autonome à joueur fantôme

Motif de la réorientation :

La relation du datacenter à la démoscène restait principalement analogique : contrainte, allocation du calcul et grammaire visuelle héritée. Le motif d'Eryx fournit un ancrage plus direct pour la thèse :

```text
Lovecraft et Kenneth J. Sterling, *In the Walls of Eryx*
    -> ASD, *Beyond the Walls of Eryx* (2007)
    -> Mandarine, *Within the Mesh* (2013)
    -> œuvre interactive générative actuelle
```

ASD a déjà transformé le labyrinthe invisible en espace calculé de démo. *Within the Mesh*, co-créé par l'auteur du projet, a poursuivi cette relation dans sa propre pratique. Le travail actuel peut donc ajouter une quatrième transformation : un espace calculé que le joueur habite et dont certaines connexions peuvent être renégociées pendant l'exécution.

Décisions documentaires prises :

- la fiction active devient une zone d'extraction vénusienne et un labyrinthe alien invisible
- le datacenter, l'officier et la cyberattaque sont conservés comme phase historique, pas comme fiction active
- aucun titre final n'est inventé ; *Le Désert des tokens* reste identifié comme ancien titre de travail
- le LLM est défini comme interprète et auteur de propositions topologiques contrôlées, pas comme source de vérité globale
- le hard state et le compilateur déterministe restent autoritatifs
- les barrières invisibles doivent exister d'abord dans la traversal et la sémantique, pas comme murs visibles ordinaires
- la validation spatiale devient un axe majeur de recherche et de playtest
- les benchmarks et catalogues existants restent des baselines factuelles datacenter
- les notes sur le visualiseur DMA WinUAE et l'allocation des ressources sont préservées

Travail effectué :

- réécriture de `STORY.md` et `SPEC.md`
- réorganisation du moodboard autour de trois couches : perception micro-informatique, transformation démoscène, transformation générative actuelle
- ajout d'une note datée à `NOTES.md` sans effacer la synthèse antérieure
- relecture Eryx de `FUNCTIONAL_PIPELINE_V1.md` et `HYBRID_SCENE_LAYOUT_PLAN.md`
- nouveau protocole principal dans `SPATIAL_VALIDATION_PLAN.md`
- ajout de statuts de migration aux documents générés concernés
- mise à jour des décisions, problèmes connus, état technique et index documentaires

Limite explicite :

Aucun code, prompt runtime, prefab, fixture ou résultat de benchmark n'a été modifié pendant cette passe. Les fonctionnalités Eryx restent planifiées jusqu'à une itération d'implémentation distincte.

Prochaine étape recommandée :

Définir puis implémenter le plus petit contrat de barrière invisible et de proposition topologique, tout en remplaçant progressivement les prompts et archétypes datacenter par la sémantique de prospection/extraction. Régénérer les catalogues et benchmarks seulement après ces changements source.

## 2026-08-09 - Iteration 0023 - Transcription Markdown de *In the Walls of Eryx*

Objectif :

Conserver dans le dossier documentaire une version de travail locale et attribuée de la source littéraire centrale du projet.

Travail effectué :

- conversion de l'e-text de The H. P. Lovecraft Archive vers `documentation/IN_THE_WALLS_OF_ERYX.md`
- suppression de la navigation, des ornements et des pixels d'indentation propres à la page web
- conservation du texte, des paragraphes, des italiques, des sections datées et de l'orthographe de la source électronique
- ajout des liens de provenance, de la publication originale et du statut public-domain signalé pour l'édition américaine de 1939
- ajout d'une note sur le langage colonial et racialisant du texte historique
- ajout du fichier à l'index documentaire et lien depuis `STORY.md`

Validation :

- début et fin du récit contrôlés après conversion
- rapport final de Wesley P. Miller présent
- aucun élément HTML ou pixel de mise en page résiduel
- liens Markdown relatifs vérifiés
