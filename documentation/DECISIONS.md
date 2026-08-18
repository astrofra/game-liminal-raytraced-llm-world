# Decisions

Derniere mise a jour : 2026-08-18

## 2026-07-29 - Base du renderer en C++11

Decision :

Le bootstrap du renderer est implemente en C++11 avec CMake.

Raison :

- coherent avec la direction generale du projet
- compatible avec une integration future a `llama.cpp`
- simple a distribuer en natif
- approprie pour un sous-systeme de rendu bas niveau

Consequence :

Le depot prend des maintenant une forme adaptee a un executable natif unique.

## 2026-07-29 - Reprise selective du projet `toy-cpp-raytracer-2003`

Decision :

Le vieux projet n'est pas porte tel quel. Seules quelques idees algorithmiques sont reprises avec reecriture propre.

Raison :

- la base historique contient trop de dette structurelle
- la reparation couterait plus cher qu'une reecriture minimale
- il faut pouvoir montrer un code hybride humain/IA lisible et defendable

Consequence :

Le depot actuel conserve une provenance explicite sans heriter de l'architecture ancienne.

## 2026-07-29 - Vendoriser la Cornell Box comme scene de reference

Decision :

La Cornell Box est integree localement dans `assets/cornell/`.

Raison :

- scene canonique pour verifier geometrie, lumiere et rebonds
- reproductibilite locale
- pas de dependance reseau au moment du rendu

Consequence :

Le renderer dispose d'une baseline visuelle stable pour les premieres iterations.

## 2026-07-29 - Commencer par un renderer grayscale CPU-only

Decision :

La premiere implementation vise un rendu CPU simple, grayscale, bruite, sans dependance graphique externe.

Raison :

- valider d'abord la logique du transport de lumiere
- eviter un detour premature par SDL, OpenGL ou une UI
- preserver la lisibilite du code et la rapidite d'iteration

Consequence :

Le resultat est austere mais deja utile pour la recherche, le profiling et la suite de l'architecture.

## 2026-07-29 - Sortie image en `PGM`

Decision :

Le renderer ecrit des fichiers `PGM` binaires.

Raison :

- format trivial a ecrire sans dependance
- suffisant pour un rendu grayscale
- bon compromis pour un bootstrap technique

Consequence :

Le pipeline d'image reste rudimentaire et devra etre remplace ou complete plus tard pour la presentation publique.

## 2026-07-29 - Utiliser une lumiere de plafond pour la premiere Cornell Box

Decision :

Le bootstrap Cornell Box utilise une aire lumineuse reconstruite au plafond, au lieu de la lumiere attachee a la camera prevue par la spec du jeu.

Raison :

- la Cornell Box est d'abord un test de reference du renderer
- cela simplifie la verification des rebonds diffus
- le modele de lumiere portee par la camera sera plus pertinent sur la scene proprietaire du jeu

Consequence :

La base actuelle valide la radiosite simple, mais ne represente pas encore l'esthetique finale du projet.

## 2026-07-29 - Commencer le format de scene v1 avec un sous-ensemble restreint

Decision :

Le format de scene v1 est implemente d'abord avec `room`, `camera`, `plane` et `box`.

Raison :

- c'est le plus petit sous-ensemble utile pour sortir du test pur OBJ
- cela suffit a construire une premiere scene liminale credible
- cela garde le parseur simple et defensible
- cela permet de reutiliser sans rupture le backend triangle/BVH deja en place

Consequence :

Le pipeline scene proprietaire existe maintenant, mais il reste incomplet vis-a-vis de la spec longue.

## 2026-07-29 - Basculer le rendu par defaut vers une scene liminale proprietaire

Decision :

L'executable rend par defaut `assets/scenes/liminal_service_corridor.scene` au lieu de la Cornell Box.

Raison :

- le projet doit maintenant montrer sa propre image, pas seulement une scene canonique de reference
- cela force le pipeline `.scene` a rester vivant
- la Cornell Box reste disponible pour les tests de regression via `run_cornell_test.bat`

Consequence :

Le depot commence a produire une image plus proche de son identite future, tout en conservant un test de reference stable.

## 2026-07-30 - Utiliser un spotlight analytique attache a la camera pour les scenes proprietaires

Decision :

Les scenes `.scene` proprietaires peuvent activer un spotlight analytique fixe a la camera, parametre par panneau, offset, portee, cone et intensite.

Raison :

- rapproche le renderer du modele d'eclairage cible de la spec
- donne une image plus found-footage et plus oppressante
- evite de dependre uniquement d'une surface emissive dans la scene
- reste plus simple a controler qu'un systeme d'eclairage plus general

Consequence :

La scene liminale est maintenant eclairee par la camera, tandis que la Cornell Box conserve son eclairage emissif de reference.

## 2026-07-30 - Vendoriser `stb_image_write.h` pour la sortie PNG

Decision :

Le projet vendorise `stb_image_write.h` et utilise le PNG comme format de sortie par defaut.

Raison :

- format plus pratique a ouvrir et partager que le `PGM`
- dependance legere et facile a embarquer
- pas besoin d'introduire une bibliotheque image plus lourde

Consequence :

Le chemin de sortie par defaut passe en `.png`, tout en gardant le `PGM` en mode legacy selon l'extension demandee.

## 2026-07-30 - Adopter un preset panorama `800x400` comme resolution par defaut

Decision :

Le renderer utilise desormais `800x400` comme resolution de rendu par defaut pour la scene liminale.

Raison :

- le cadrage panorama expose mieux la longueur laterale du corridor
- la composition gagne en lecture architecturale
- cela rapproche davantage l'image par defaut de l'intention visuelle du projet

Consequence :

Un lancement sans `--width` ni `--height` produit maintenant un rendu plus couteux, mais plus representatif du projet.

## 2026-08-01 - Passer le catalogue de prefabs en mode HQ par defaut

Decision :

Le script `scripts/generate_prefab_catalog.py` utilise desormais par defaut `1536x1536` et `32` samples par pixel.

Raison :

- le catalogue sert surtout au diagnostic visuel des prefabs
- les artefacts geometriques subtils se voient mal a `768x768` et `8 spp`
- cette charge supplementaire n'impacte pas le rendu interactif du jeu, seulement les exports de controle

Consequence :

Un lancement sans options du generateur de catalogue produit maintenant des PNG plus lents a calculer, mais nettement plus utiles pour l'audit.

## 2026-07-30 - Valider `llama.cpp` avec CUDA et `Ministral 3 8B` comme cible d'inference v1

Decision :

Le runtime narratif cible du projet est desormais acte comme etant `llama.cpp` avec acceleration CUDA, autour de `Ministral 3 8B Instruct 2512` en GGUF quantifie.

Raison :

- supprime l'ambiguite restante entre la spec longue et la spec technique CUDA
- cadre plus proprement les contraintes de VRAM, de latence et de packaging
- reste coherent avec une distribution locale native sans Python ni Ollama
- correspond a la machine de developpement actuelle et au document `LLAMA_CUDA_SPECS.md`

Consequence :

Les prochaines iterations doivent maintenant viser une vraie integration locale de l'inference, la definition du contrat de tour structure, puis la boucle verticale texte -> etat -> scene -> rendu.

## 2026-07-30 - Vendoriser `llama.cpp` dans `vendor/llama.cpp`

Decision :

La base `llama.cpp` retenue par le projet est copiee dans `vendor/llama.cpp` et raccordee au `CMakeLists.txt` principal derriere des options explicites.

Raison :

- evite une dependance reseau au moment du build du runtime narratif
- garde un commit exact de provenance dans le depot
- rend l'integration future plus concrete qu'une simple note de faisabilite
- reste coherent avec la logique de vendoring deja utilisee pour d'autres assets et dependances simples

Consequence :

Le projet peut maintenant compiler un binaire `Release` qui linke `llama.cpp`, detecte le backend CUDA et expose un point de verification minimal via `--llama-info`.

## 2026-07-31 - Retenir `SDL3` comme couche multimedia multi-OS

Decision :

La future couche fenetre, evenements, saisie texte, presentation bitmap et audio simple du projet reposera sur `SDL3`.

Raison :

- couvre nativement la creation de fenetre, les evenements clavier, la saisie texte Unicode, la presentation d'une image CPU dans une texture GPU et l'audio basique
- reste portable entre Windows, macOS et Linux sans exiger de frontend externe
- correspond mieux a une fiction interactive desktop a interface parser qu'une cible `libretro` prise comme architecture principale
- laisse ouverte la possibilite d'un port ou backend secondaire plus tard, sans imposer cette contrainte maintenant

Consequence :

Les prochaines iterations de la boucle verticale devront viser un runtime natif avec `SDL3` pour la HMI, le transcript, la ligne de commande parser et l'affichage temps reel du viewport.

## 2026-07-31 - Valider d'abord la chaine scenario -> scene -> rendu sur trois lieux canoniques

Decision :

Avant d'ouvrir largement la generation de scenes par le LLM, la premiere validation spatiale du projet passera par trois lieux de reference :

- le portail d'entree du datacenter
- l'interieur du datacenter avec ses travees de serveurs
- le toit du datacenter comme tour de ronde ouvrant vers le desert

Raison :

- la difficulte principale du projet n'est pas d'abord conversationnelle, mais traductive
- il faut verifier ce que le format `scene v1` sait vraiment exprimer
- ces trois lieux couvrent trois regimes essentiels : seuil, interieur dense, exterieur de veille
- le test du toit forcera rapidement la question du ciel, de l'horizon et des limites du format actuel

Consequence :

La prochaine phase doit produire une batterie de tests fonctionnels et des scenes canoniques de reference avant toute generalisation de la generation spatiale.

## 2026-07-31 - Activer OpenMP en option de build et abaisser le preset par defaut a `16 spp`

Decision :

Le renderer active desormais un parallelisme CPU OpenMP quand le compilateur le permet, via l'option CMake `LIMINAL_ENABLE_OPENMP`, et le preset de rendu par defaut passe de `32` a `16` echantillons par pixel.

Raison :

- le cout des scenes canoniques les plus denses devient trop eleve en mono-thread, surtout depuis l'introduction des prefabs
- un grain plus fort reste compatible avec l'intention esthetique du projet
- OpenMP permet un gain de temps immediat sans rearchitecture lourde du renderer
- le fallback mono-thread reste disponible si OpenMP n'est pas present localement

Consequence :

Le depot privilegie maintenant un rendu de travail plus rapide, multi-coeur quand disponible, tout en conservant un mode deterministe et un comportement de repli simple.

## 2026-07-31 - Prioriser la chaine fonctionnelle et un etat spatial intermediaire avant les optimisations profondes

Decision :

Pour la preuve de concept, les prochaines iterations priorisent la chaine `etat -> texte -> scene -> rendu`. Le LLM ne doit pas generer librement toute la scene `v1` a chaque tour : il doit d'abord produire un resultat structure a tres basse temperature, qui alimente un etat spatial intermediaire compile ensuite de facon deterministe.

Raison :

- les vrais budgets de temps sont d'abord la latence d'inference et le rendu CPU par tour
- le temps de fabrication runtime des scenes et l'occupation RAM ne sont pas encore les priorites critiques du prototype
- `Ministral` a `temperature 0` est plus defendable sur des deltas structures que sur une geometrie libre complete
- separer hard state, soft state, etat spatial, narration et scene facilite la validation et le debug

Consequence :

La prochaine etape doit definir un contrat de tour structure, les structures de memoire du monde et un compilateur de scene deterministe pour les lieux canoniques avant d'ouvrir une generation spatiale plus libre.

## 2026-07-31 - Passer du grayscale strict a un RGB semantique verrouille

Decision :

Le renderer n'est plus strictement monochrome. Il produit maintenant une image RGB tres contrainte : ciel bleu, desert ocre, LEDs de racks rouges, et tout le reste en niveaux de gris.

Raison :

- la palette limitee renforce la lisibilite spatiale sans perdre l'austerite du projet
- le contraste bleu / ocre / rouge aide a distinguer exterieur, desert et repere technique
- laisser la couleur libre au format `.scene` ou au LLM serait trop instable pour la preuve de concept
- garder `gray()` comme seule entree materielle reste plus simple pour le parseur et pour le prompt

Consequence :

Le coeur du renderer et les buffers memoire passent en RGB, mais le langage `.scene` reste base sur `gray()` et sur un nommage semantique simple. Le `PGM` legacy reste disponible via conversion en luminance.

## 2026-08-09 - Réorienter la fiction vers un labyrinthe génératif fondé sur Eryx

Decision :

Le datacenter autonome de *Le Désert des tokens* cesse d'être la fiction principale. La direction active devient une zone d'extraction vénusienne traversée par un labyrinthe invisible et potentiellement changeant.

La généalogie structurante est désormais explicite :

```text
*In the Walls of Eryx*
    -> ASD, *Beyond the Walls of Eryx*
    -> Mandarine, *Within the Mesh*
    -> œuvre interactive LLM actuelle
```

Le LLM pourra proposer des mutations topologiques limitées, mais l'état autoritatif, la validation et la compilation de scène restent contrôlés par le moteur.

Raison :

- le récit datacenter établissait une boucle forte entre calcul, chaleur, écologie et interprétation, mais sa relation à la démoscène restait indirecte
- Eryx fournit une lignée historique dans la démoscène à travers ASD
- *Within the Mesh*, co-créé par l'auteur du projet, rend cette lignée personnelle et antérieure au projet actuel
- le labyrinthe invisible donne une fonction fictionnelle précise à la cohérence locale et à la dérive globale déjà présentes dans l'architecture
- l'autorité contrôlée du LLM sur certaines relations spatiales transforme une faiblesse de continuité en question de recherche testable
- le décor d'extraction justifie les dalles, plans, répétitions industrielles, espaces ouverts et images instrumentales du renderer

Decisions associées :

- préserver l'inversion de charge computationnelle, `llama.cpp`, l'exécutable natif, SDL3, le hard/soft/spatial state, le compilateur hybride et le raytracer actuel
- ne pas revenir à la génération libre d'une `.scene` comme voie de production
- ne pas rendre les murs invisibles comme des murs opaques ordinaires
- valider toute proposition topologique avant commit
- réinterpréter les recherches et géométries antérieures plutôt que les effacer
- conserver *Le Désert des tokens* comme ancien titre de travail et phase historique
- traiter les noms runtime, prompts, fixtures, prefabs et benchmarks datacenter comme dette de migration ou baseline tant qu'aucun refactor code ne les remplace

Consequence :

La documentation artistique et les plans de validation adoptent Eryx immédiatement. Le binaire courant reste factuellement datacenter/désert jusqu'à une tâche de migration du code, des prompts et des assets. Aucune fonctionnalité Eryx ne doit être décrite comme implémentée avant cette étape.

## 2026-08-09 - Faire du catalogue de prefabs la première migration source vers Eryx

Decision :

Le catalogue actif devient un vocabulaire de carrière de cristal sur Vénus. Il contient neuf objets source-générés : seuil, container d'échantillons, balise de survey, scanner, veine cristalline, rig d'extraction, abri de prospection, pylône de carrière et processeur atmosphérique.

Les directives `prefab_rack`, `prefab_cooling_unit` et `prefab_ai_server` restent supportées pour les fixtures et benchmarks historiques, mais sortent du catalogue actif. Les murs aliens invisibles restent exclus des prefabs visibles.

Raison :

- le brief demande une infrastructure humaine brutaliste opposée à une topologie alien imperceptible
- la nouvelle fournit des fonctions concrètes à haute valeur sémantique : détecter, extraire, baliser, stocker, respirer et s'abriter
- les moodboards `origins` et `brutalism` convergent vers des masses asymétriques, porte-à-faux, couronnes fendues, instruments minces et fragments facettés
- conserver les anciens noms comme compatibilité évite de falsifier l'histoire et de casser les scènes existantes
- neuf types suffisent à former une grammaire sans ouvrir une bibliothèque décorative coûteuse

Consequence :

Le parseur `scene v1`, le compilateur hybride, le prompt d'audit et la documentation technique reconnaissent les nouveaux objets. `PREFAB_CATALOG.md` et ses dix-huit images sont régénérés depuis le renderer. Le preset passe à `1024x1024`, `24` samples par pixel : le nombre de vues augmente fortement tout en conservant un niveau d'audit supérieur au rendu de jeu.

## 2026-08-09 - Réserver un verre diélectrique borné aux cristaux

Decision :

Les prismes de `prefab_crystal_cluster` et l'échantillon de `prefab_crystal_scanner` utilisent un modèle `glass` interne. Son IOR central est fixé à `1.52` ; la réfraction, le Fresnel diélectrique exact et la réflexion totale interne sont calculés par le renderer. Une dispersion RGB « poor man's », inspirée du [shader RenderMan documenté par Astrofra](https://astrofra.com/wordpress/index.php/2005/07/12/poor-man-s-dispersion-shader-rman-version/), décale les IOR des trois canaux avec un écart nominal de `0.035` et un jitter borné. Un filtre Beer–Lambert RGB donne une densité croissante avec l'épaisseur. Les événements diélectriques ont un plafond séparé de neuf interfaces par chemin et un cutoff énergétique de `0.02`.

Le format `.scene` ne gagne ni directive `material`, ni IOR libre. La pointe du rig d'extraction, qui réutilise la même géométrie de prisme comme outil, reste diffuse et opaque.

Raison :

- dans la nouvelle, le cristal est un objet optiquement actif, précieux et attirant, distinct de l'architecture alien explicitement non réfractive et non réfléchissante
- dans le catalogue, la transparence et la déviation du fond différencient immédiatement la ressource vénusienne des masses brutalistes humaines
- un budget spéculaire séparé permet au rayon d'entrer, de sortir et de subir des réflexions internes sans augmenter le nombre de rebonds diffus de toute la scène
- un canal RGB héroïque stratifié par sample recompose les trois réfractions sans le coût systématique de trois chemins complets par impact
- verrouiller le modèle préserve la petite grammaire contrôlable par le compilateur et le LLM

Consequence :

Le renderer possède désormais deux familles de transport : diffus et diélectrique. Les ombres traversent approximativement le verre avec le même filtre d'épaisseur ; la dispersion continue, l'absorption spectrale et les caustiques restent hors périmètre afin de préserver le budget CPU.

## 2026-08-18 - Fonder la première tranche Eryx sur une boucle d'arpentage en sept lieux

Decision :

La première carte active suit une progression fixe : seuil de carrière, champ d'extraction, coupe cristalline, scanner, plateau de balises, seuil du labyrinthe et abri de Vey. Le retour ouest depuis l'abri mène directement au scanner et n'est volontairement pas réciproque.

Raison :

- une chaîne d'abord fiable donne au joueur des repères à perdre
- les moodboards `objects` demandent une navigation par lumières rares plutôt qu'un décor dense
- `brutalism` fournit les masses humaines lourdes qui stabilisent chaque lieu
- `origins` justifie une contradiction de projection et de connexion plutôt qu'un couloir alien visible
- huit commandes suffisent à établir le monde, rencontrer une barrière, contourner puis constater un retour impossible

Consequence :

Les sept fixtures source et douze liens dirigés constituent la recette canonique de référence. Cette contradiction auteurisée doit rester reproductible même lorsque les mutations live seront ajoutées.

## 2026-08-18 - Séparer la barrière invisible de la géométrie et des sorties bloquées

Decision :

Une barrière alien est un état de traversal persistant, distinct de `SpatialState.blocked_exits` et absent de la scène visible. Elle enregistre lieu, direction, preuve et découverte. Un contact reconnu consomme un tour, mais ne déplace pas le joueur et n'augmente pas `move_count`.

Raison :

- une sortie bloquée décrit une fermeture locale ordinaire ; la barrière doit rester apparemment traversable
- rendre un mur, une grille ou un champ de force contredirait la source littéraire et le brief visuel
- un type séparé permet au feedback de distinguer collision intentionnelle, commande incomprise et erreur technique
- la persistance rend la découverte testable après sauvegarde et rechargement

Consequence :

Le viewport montre le champ ouvert tandis que le moteur refuse le mouvement et produit une preuve instrumentale. Les futures mutations devront modifier cette couche topologique sans demander au LLM de dessiner la collision.
