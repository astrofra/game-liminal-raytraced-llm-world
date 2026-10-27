# Scénario et qualité des compositions `.scene`

Note du 27 octobre 2026, fondée sur le code à `765e066` et les archives de
parties examinées à cette date. Analyse documentaire ; aucune modification
du runtime ni expérience comparative contrôlée effectuée pour cette note.

L'ajout de la gestion du scénario peut influencer la qualité des compositions
3D. Un canal de transmission est confirmé dans le code : le générateur reçoit
le dossier d'enquête dans son contexte de continuité. Une dégradation artistique
causée par cet ajout reste toutefois une hypothèse à tester.

## Ce qui est établi

Le contrat architectural de base, dans
[`assets/scene_generation/local_v1.json`](../assets/scene_generation/local_v1.json),
n'a pas changé entre l'état précédant l'enquête (`f87b230`) et l'état examiné.
Il demande toujours une composition de 8 à 20 boîtes, avec une caméra, un sol
et un éclairage fixés, des contraintes de visibilité et de support.

La création d'un lieu comporte deux étapes distinctes :

1. Gemma compose le programme `.scene`, puis le moteur valide sa géométrie.
2. Un appel narratif choisit les indices parmi les sites compatibles, propose
   les issues et les températures, et décrit l'arrivée.

Le catalogue de répartition des indices n'est donc pas une tâche supplémentaire
à exécuter pendant la composition 3D. Il intervient après celle-ci.

En revanche, `BuildContinuityContext`, dans
[`turn_runner.cpp`](../src/turn_runner.cpp), place le résultat
d'`InvestigationContext(state)` dans `engine.investigation`. Ce contexte est
transmis à `BuildLocalSceneProgramPrompt`, dans
[`scene_design.cpp`](../src/scene_design.cpp), puis au générateur géométrique.
Il contient les questions d'enquête, les faits connus, les hypothèses soutenues
ou contredites, les indices accessibles et les règles d'interprétation.
Les conclusions encore cachées sont filtrées, mais le dossier connu reste
présent même lorsqu'il ne fournit aucune contrainte architecturale utile.

Les descriptions des lieux et les événements récents offrent un autre chemin
d'influence : une prose façonnée par le scénario peut orienter la composition
suivante. Ce mécanisme de continuité existait déjà avant l'enquête.

Dans [`llm_runtime.cpp`](../src/llm_runtime.cpp), chaque requête recrée son
contexte d'inférence et son échantillonneur. Les poids restent chargés, mais
les appels précédents ne laissent pas une conversation implicite dans Gemma.
L'influence à étudier passe par les données effectivement retransmises.

## Mesures relevées dans les journaux

Les archives sources restent hors dépôt, sous
`%LOCALAPPDATA%/WithinTheLatentWalls/research/`.

| Observation | Avant l'enquête | Après l'enquête |
|---|---:|---:|
| Première génération géométrique examinée | 2 494 tokens de prompt | 2 665 tokens de prompt |
| Réserve de sortie géométrique | 2 048 tokens | 2 048 tokens |
| Fenêtre de contexte | 8 192 tokens | 8 192 tokens |

La comparaison porte sur `turns/turn-00001.json` des archives
`run-20261027T072857Z-1411155189` et
`run-20261027T151358Z-3533984756`, dans `scene_metrics.calls`.
Le message système et le préambule architectural sont identiques. Le contexte
diffère par le dossier d'enquête et un champ vide `objects.bound_mentions`.
Le surcroît total est de 171 tokens, soit environ 7 %. Les graines diffèrent :
cette observation mesure l'évolution du prompt, pas un effet causal sur l'image.

Dans la partie `run-20261027T125416Z-1132572880`, un prompt géométrique atteint
4 637 tokens après plusieurs interactions. L'historique, les objets et le dossier
d'enquête ont tous évolué ; cet accroissement ne peut pas être attribué au seul
scénario.

Dans l'instantané examiné de la partie `run-20261027T151358Z-3533984756`
(10 commandes enregistrées, 7 lieux générés), les sept lieux sont acceptés.
L'un nécessite trois essais ; leurs sorties se terminent normalement par EOS.
Aucune troncature géométrique par épuisement du budget de sortie n'a été relevée
dans cette série. Ces mesures concernent la génération 3D, pas les prompts
narratifs, qui ont leur propre charge et leur propre réserve de sortie.

Les validations actuelles vérifient notamment syntaxe, visibilité, placement
et navigation. Elles ne mesurent pas l'intérêt visuel, la variété des masses
ou la qualité d'une composition. Une scène valide peut rester peu convaincante.

## Hypothèse et autres explications possibles

Le dossier d'enquête pourrait détourner une partie du conditionnement vers les
thèmes du récit, renforcer des motifs déjà décrits ou rendre les consignes
architecturales moins saillantes. Cela pourrait produire des lieux plus
convenus ou répétitifs sans provoquer d'erreur technique.

Le dossier fait aussi partie du contexte obligatoire. Dans
[`continuity.cpp`](../src/continuity.cpp), son coût peut réduire la place
disponible pour les événements et descriptions récents lorsque le budget se
resserre. Ce mécanisme existe ; son effet sur la qualité n'est pas mesuré ici.

D'autres facteurs restent à contrôler : graines différentes, température
d'échantillonnage liée à la chaleur corporelle, géométrie et prose du lieu
précédent, réparations de visibilité et contraintes architecturales déjà fortes.
La comparaison de parties différentes ne permet pas de départager ces causes.

## Expérience proposée

Commencer par une comparaison isolant une seule variable :

- **A — contexte actuel** : requête géométrique complète, telle qu'archivée.
- **B — dossier retiré** : même requête, avec seulement
  `engine.investigation` retiré de la copie destinée à la géométrie.

Conserver les mêmes scènes de départ, commandes, modèle et quantification,
graines, températures, paramètres d'échantillonnage, budgets, moteur et réglages
de rendu. Utiliser plusieurs états sources, avant et après des découvertes,
et plusieurs graines par paire. Les états de partie originaux restent intacts.

Pour cette première comparaison, figer également les événements retenus afin
que la suppression du dossier ne réintroduise pas automatiquement davantage
d'historique. Une seconde série pourra laisser le sélecteur de budget fonctionner
normalement pour mesurer l'effet global de la séparation.

Comparer les premières propositions avant réparation, puis les scènes finales
acceptées. Présenter les images par paires anonymisées, avec des réglages de
rendu identiques et sans indices mobiles ni repères de parois. Évaluer la
lisibilité des volumes, les vides, l'échelle, la variété et la continuité avec
le lieu de départ. Relever séparément les refus, réparations, tokens, durées
et motifs d'arrêt. Le nombre de boîtes ne suffit pas à mesurer la qualité.

Si le résultat est favorable, envisager un contexte dédié à la génération 3D :
géométrie acceptée, ambiance, contraintes de passage et événements physiques
pertinents. Le narrateur conserverait le dossier d'enquête complet. Cette
séparation devrait préserver l'autorité du moteur et les faits spatiaux utiles,
sans parser C++ de langage naturel ni appel de résumé supplémentaire.

Liens : [continuité des scènes](CONTINUITE_SCENES.md),
[répartition des indices](REPARTITION_INDICES_LLM.md),
[état technique](TECHNICAL_STATE.md).
