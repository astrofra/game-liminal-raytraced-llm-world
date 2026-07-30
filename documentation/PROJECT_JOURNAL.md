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
