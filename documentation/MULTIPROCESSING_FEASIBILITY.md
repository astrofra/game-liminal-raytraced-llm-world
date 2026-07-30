# Etude De Faisabilite - Multiprocessing Du Renderer

Derniere mise a jour : 2026-07-30

## Objet

Evaluer s'il est raisonnable d'activer un rendu CPU parallele dans le renderer actuel sans alourdir la logique metier, idealement avec un mecanisme compile-time base sur des macros.

## Conclusion courte

Oui pour du parallelisme CPU multi-coeur a faible intrusion.

Non, ou en tout cas pas proprement, pour du vrai multi-processus OS "juste avec des macros".

La voie la plus simple et la plus defendable est :

- garder le renderer mono-thread comme baseline
- ajouter un mode optionnel OpenMP au build
- encapsuler la directive parallele derriere une macro unique
- paralleliser uniquement la boucle externe de rendu des lignes

Le code actuel s'y prete bien.

## Important : vocabulaire

Le terme "multiprocessing" peut vouloir dire deux choses differentes :

- parallelisme CPU sur plusieurs coeurs, souvent implemente avec plusieurs threads dans un meme processus
- vrai multi-processus avec plusieurs executables/processus cooperants

Sous la contrainte "sans alourdir la logique du code, juste avec des macros", il faut viser le premier sens.

Le second sens demanderait :

- duplication ou partage explicite de la scene et de la BVH
- decoupage en tuiles ou bandes
- IPC ou fichiers intermediaires
- fusion des buffers
- gestion plus lourde des erreurs et du cycle de vie

Ce n'est pas coherent avec l'objectif de simplicite.

## Etat du renderer actuel

Le point chaud principal est dans `RenderSceneToImage` de `src/renderer.cpp`.

Observation structurante :

- la scene est en lecture seule pendant le rendu
- la camera et le spotlight sont calcules une fois puis relus
- chaque pixel ecrit une seule case distincte du buffer `pixels`
- le RNG est local a chaque pixel, derive de `x`, `y` et du `seed` global
- il n'existe pas d'accumulation partagee entre pixels

Cela fait du coeur du rendu un cas presque ideal de parallelisme "embarrassingly parallel".

## Ce qui est parallele sans douleur

### Boucle par ligne

La boucle externe sur `y` est le meilleur point d'entree :

- tres peu intrusif
- chaque thread traite une ligne complete
- aucune synchronisation sur le buffer image si chaque thread ecrit sa propre ligne
- determinisme conserve car le seed par pixel ne depend pas de l'ordre d'execution

### Boucle par pixel aplatie

Une autre option est d'aplatir l'image en une seule boucle `for (index = 0; index < width * height; ++index)`.

Avantage :

- encore plus simple pour certains ordonnanceurs

Inconvenient :

- leger cout de conversion `index -> x, y`
- moins lisible que la double boucle actuelle

Sous la contrainte de lisibilite, la boucle par ligne reste preferable.

## Ce qui bloque ou demande un traitement special

### Affichage de progression

Le `printf("  line %d / %d\\n", ...)` actuel devient problematique en parallele :

- ordre non deterministe
- sorties intercalees
- bruit terminal plus fort

Options minimales :

- desactiver le log par ligne quand le mode parallele est actif
- ou garder un compteur atomique leger et n'imprimer qu'occasionnellement

Pour rester minimal, la meilleure solution est de ne pas journaliser chaque ligne en mode parallele.

### Ecriture PNG

La sortie PNG se fait apres le rendu complet, sur un buffer deja rempli.

Donc :

- pas de risque particulier
- pas besoin de verrou autour de `stb_image_write`

### BVH et intersections

Le code d'intersection ne modifie pas la scene.

Donc :

- pas de verrou necessaire
- bonne candidate pour etre relue concurremment

### RNG

Le RNG est instancie localement dans la boucle pixel.

Donc :

- pas d'etat global partage
- pas de contention
- pas de perte de determinisme liee a l'ordonnancement

## Options techniques comparees

| Option | Faisabilite | Intrusion code | Observations |
| --- | --- | --- | --- |
| OpenMP optionnel avec macro | Elevee | Faible | Meilleur compromis |
| `std::thread` / pool maison | Elevee | Moyenne a forte | Plus de plomberie que voulu |
| `std::async` | Moyenne | Moyenne | Peu controlable, pas ideal pour un renderer |
| Vrai multi-processus | Faible sous cette contrainte | Forte | Hors cible |

## Recommandation

### Recommandation principale

Activer un mode OpenMP optionnel, compile-time, avec une seule macro visible dans `renderer.cpp`.

### Pourquoi OpenMP ici

- le coeur du rendu est deja une boucle `for` propre
- le partage de donnees est quasi exclusivement en lecture seule
- l'integration peut rester locale au build et a une petite zone du code
- on garde un fallback mono-thread sans bifurquer l'architecture

## Forme minimale recommandee

### 1. Une option CMake

Exemple de direction technique :

```cmake
option(LIMINAL_ENABLE_OPENMP "Enable OpenMP render parallelism" ON)

if(LIMINAL_ENABLE_OPENMP)
    find_package(OpenMP)
    if(OpenMP_CXX_FOUND)
        target_link_libraries(liminal_cornell_renderer PRIVATE OpenMP::OpenMP_CXX)
        target_compile_definitions(liminal_cornell_renderer PRIVATE LIMINAL_ENABLE_OPENMP=1)
    endif()
endif()
```

Effet :

- aucun changement de logique metier
- build mono-thread si OpenMP n'est pas disponible
- build parallele si le toolchain sait faire

### 2. Une macro d'annotation de boucle

Deux variantes realistes existent :

- macro MSVC basee sur `__pragma`
- macro GCC/Clang basee sur `_Pragma`

Exemple d'encapsulation cross-compiler :

```cpp
#if defined(LIMINAL_ENABLE_OPENMP)
  #if defined(_MSC_VER)
    #define LIMINAL_OMP_PARALLEL_FOR __pragma(omp parallel for schedule(dynamic, 1))
  #else
    #define LIMINAL_OMP_PARALLEL_FOR _Pragma("omp parallel for schedule(dynamic, 1)")
  #endif
#else
  #define LIMINAL_OMP_PARALLEL_FOR
#endif
```

Puis dans le renderer :

```cpp
LIMINAL_OMP_PARALLEL_FOR
for (int y = 0; y < config.height; ++y) {
    ...
}
```

Cela reste tres proche du code actuel.

## Pourquoi `schedule(dynamic, 1)` est defendable

Le cout d'une ligne n'est pas parfaitement uniforme :

- geometrie variable
- nombre d'occlusions variable
- chemins aleatoires variables

Un ordonnancement dynamique limite le risque qu'un thread termine tres vite pendant qu'un autre garde les lignes plus couteuses.

Alternative plus simple :

- `schedule(static)` si on veut un comportement encore plus previsible

Verdict :

- `dynamic, 1` est probablement le meilleur premier essai
- `static` reste acceptable si on veut minimiser l'overhead et garder la lecture mentale la plus simple

## Determinisme et reproductibilite

Le point important est favorable :

- chaque pixel derive deja son seed de `config.seed`, `x` et `y`
- l'ordre d'execution des pixels n'influence donc pas les tirages aleatoires d'un autre pixel

Inference :

Le rendu devrait rester bit-a-bit stable entre executions paralleles et mono-thread, tant qu'on ne modifie pas la logique interne d'un pixel et qu'on ne fait pas de reductions flottantes partagees.

Cette affirmation est une inference raisonnable a partir du code actuel, pas une mesure deja verifiee.

## Gains attendus

Sans benchmark reel, on ne peut donner qu'un ordre de grandeur.

Le temps de rendu de la scene liminale panorama releve le 2026-07-30 est d'environ `35748.60 ms` en `800x400`, `32 spp`, `3 bounces`.

Comme la quasi-totalite du cout est dans la boucle pixel :

- sur 8 threads logiques, un gain plausible est de l'ordre de `x4` a `x7`
- sur 16 threads logiques, un gain plausible est de l'ordre de `x6` a `x10`

Cela dependra notamment :

- du CPU reel
- de la bande passante memoire
- de la qualite du runtime OpenMP
- du ratio entre cout de path tracing et cout des logs

Ces chiffres sont donc des hypotheses de travail, pas des resultats.

## Risques

### Risque faible

- bruit dans les logs de progression
- disponibilite variable d'OpenMP selon compilateur / configuration locale

### Risque moyen

- gain inferieur a l'attendu si l'ordonnancement choisi est mauvais
- overhead non negligeable sur les tres petits rendus

### Risque faible a moyen a plus long terme

- si le renderer ajoute plus tard une accumulation partagee, un denoiser progressif, ou un affichage live, la strategie devra etre re-verifiee

## Ce qui n'est pas recommande

Sous la contrainte actuelle, il ne faut pas :

- introduire un scheduler custom
- construire un thread pool manuel
- passer en multi-processus
- disperser des `#ifdef` partout dans le code de shading

Le parallelisme doit rester confine a :

- la configuration CMake
- une macro d'annotation de boucle
- un petit traitement specifique pour les logs

## Plan minimal si on veut l'implementer ensuite

1. Ajouter une option OpenMP au `CMakeLists.txt`.
2. Ajouter une macro d'annotation de boucle dans un petit header dedie ou en tete de `renderer.cpp`.
3. Paralleliser uniquement la boucle externe `for (y ...)`.
4. Desactiver ou simplifier le log par ligne en mode parallele.
5. Mesurer au moins :
   - Cornell `256x256`, `32 spp`
   - scene liminale `256x256`, `32 spp`
   - scene liminale `800x400`, `32 spp`
6. Verifier que l'image reste identique entre mode mono-thread et mode parallele avec meme `seed`.

## Verdict final

Faisabilite : elevee, si l'objectif reel est un renderer multi-coeur a faible intrusion.

Approche recommandee : OpenMP optionnel, active par macro compile-time, boucle externe sur les lignes, pas de refonte architecturale.

Faisabilite du vrai multi-processus "juste avec des macros" : faible et non recommandee.
