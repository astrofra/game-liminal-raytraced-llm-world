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
