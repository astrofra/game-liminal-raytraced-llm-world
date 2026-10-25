# Livraison du runtime autonome

25 octobre 2026 — implémentation intégrée et qualification locale réalisée.

## Implémentation

Gemma 4 E4B Q4_K_M est chargé par llama.cpp dans le processus du jeu.
Un manifeste unique alimente téléchargement développeur, table C++ et distribution.
Les poids sont vérifiés puis conservés ; appels sérialisés et contexte neuf à chaque
requête. Fermeture après arrêt des workers, annulation coopérative au chargement et au décodage.
Les sorties passent par le template et le parseur Gemma ; seuls les canaux finaux
sont admis. Les JSON Schemas contraignent effectivement l'échantillonnage.

Le sampler commun amont vérifie d'abord le token candidat et ne filtre tout le
vocabulaire qu'en cas de rejet. Le premier essai utilisait un filtrage systématique,
qui coûtait environ 100 ms par token et causait des délais narratifs. Cet essai a
été arrêté et ses preuves gardées ; il ne fait pas partie de la recette finale.

Le lancement normal garde la fiction, les issues, la chaleur, les six langues,
l'inventaire, le HUD et la composition locale. Aucune lecture des archives au démarrage.
Le modèle, les scènes et les polices sont résolus depuis l'exécutable.

Le client HTTP du jeu, ses paramètres, tests de transport, anciens profils et
téléchargements ont été supprimés. Les bancs natifs utiles restent disponibles.
Les anciennes qualifications et sorties volumineuses ont été retirées du checkout
après copie dans l'[archive de recherche](HISTORIQUE_RECHERCHE.md).

## Preuves et suivi

Racine des preuves : `C:/works/duo-runs/game-liminal-raytraced-llm-world/native-migration/`.
Le lot DeepSeek [DUO-025](duo/reviews/DUO-025.md) a fourni l'audit des dépendances
entre scripts ; Codex a revu les recommandations, implémenté, testé et intégré.

- Huit CTest et 61 tests Python passent sur la version finale (57 scripts, 4 bancs).
- Sondes GPU : texte italien, JSON, modèle absent/invalide, EOS, limite de tokens,
  annulation avant/pendant chargement et génération, délai, contexte trop petit.
- Vingt réponses contraintes malgré une consigne contradictoire : exactement
  `{"ok":true}`, un chargement puis dix-neuf réutilisations, contexte neuf par appel.
  Un conflit de noms entre fichier de contrainte et résultat dans le premier banc
  a été corrigé ; `schema-final.json` est la preuve adversariale finale.
- Partie anglaise de vingt commandes, trois lieux générés, neuf déplacements :
  21 appels réussis. Narrations médianes de 4,27 s ; compositions à chaud autour
  de 10 s. Premier chargement avec vérification : environ 16,5 s supplémentaires.
  Ces mesures sont propres à la machine de développement et aux prompts testés.
- Parcours de quatre commandes en français, italien, allemand, norvégien et danois,
  avec découverte, observation, inventaire et retour. Un retour déjà réussi pouvait
  être raconté comme une seconde tentative : le narrateur reçoit maintenant une
  arrivée accomplie. Retests italien et norvégien, dont le dernier sur le binaire
  final, confirment la correction ; cela ne garantit pas toute formulation future.
- Deux parties SDL finales, française et italienne, avec découverte puis observation :
  sorties normales, captures `sdl-fr.png` et `sdl-it.png`, poids réutilisés entre workers.
  Les deux nouveaux lieux ont des `.scene` différents, avec descriptions, images,
  prompts, profondeur/ID, état et marqueur de fermeture dans leurs archives propres.
  Le premier lieu reste identique. Aucun accès réseau du processus n'a été observé
  par interrogation périodique des connexions ; le réseau système n'était pas coupé.
- Fermeture SDL pendant le premier chargement : sortie normale, archive fermée,
  historique vide ; aucune arrivée fictive n'est enregistrée.
- Distribution installée dans un chemin avec espaces et accent ; démarrage depuis
  un autre dossier avec PATH limité à Windows, sans environnement développeur.
  Consultation de l'inventaire SDL dans les six langues, sans appel au modèle.
  Un fichier corrompu au chemin attendu est refusé avec `model_corrupt` avant inférence.

- Sonde CPU sur le dossier portable final, PATH réduit : JSON correct, zéro couche
  déportée sur GPU, sortie normale. Ce test court ne qualifie pas les latences d'une
  partie CPU complète (`cpu-final.json`).

## Nettoyage et livraison

L'exécutable publié dans `build/Release/` est celui appelé par
`play_within_the_latent_walls.bat`. La distribution 3.0.0 embarque le GGUF, les DLL,
les ressources et les notices ; garder les trois volumes `.bin` avec le Setup.
Le dossier `output/installer/stage/` constitue aussi une version portable complète.
Les sources correspondantes sont fournies séparément, sans poids.

Le nettoyage retire le client de service, ses tests, anciens profils, scripts
orphelins et qualifications obsolètes. Les données de recherche retirées ont été
archivées avant suppression (ZIP de 275 593 381 octets, voir historique).
Les contrats historiques du duo restent datés. Le kit du duo et l'outil personnel
`commit_with_llm.py` sont des outils de développement encore utiles, distincts du
jeu et exclus de sa distribution ; ils ne sont pas nécessaires pour jouer.
Les chemins d'anciens caches locaux de compilation ne constituent pas une dépendance
produit et ne sont pas livrés.

## Limites

La qualification sur Windows vierge reste distincte des essais locaux. Le CPU
utilise le même modèle mais ne promet pas la latence GPU. Les performances
à froid incluent vérification et chargement ; elles ne doivent pas être mélangées
aux réponses à chaud. Aucun commit ni push effectué par Codex.
