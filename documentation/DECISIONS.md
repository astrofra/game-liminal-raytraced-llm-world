# Decisions

Derniere mise a jour : 2026-07-29

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
