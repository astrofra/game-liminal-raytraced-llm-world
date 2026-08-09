# Functional Pipeline V1

Dernière mise à jour : 2026-08-09

## Rôle du document

Ce document formalise la chaîne entre commande, modèle de langage, état spatial et image. L'architecture reste celle qui a été validée avant la réorientation Eryx :

```text
état de jeu autoritatif
    -> résultat structuré du LLM
    -> état spatial intermédiaire
    -> compilation déterministe / hybride
    -> scène v1
    -> rendu
```

La réorientation ne justifie pas un retour à la génération libre d'une scène `.scene`. Elle précise le rôle du modèle : en plus d'interpréter et de raconter, il pourra proposer des mutations topologiques limitées représentant le labyrinthe invisible.

## Statut d'implémentation

La chaîne suivante existe déjà :

- structures `HardState`, `SoftState`, `SpatialState` et `TurnResult`
- appel local à `Ministral` via `llama.cpp`
- résultat JSON, tentative de réparation et fallback no-op
- application de deltas aux états
- compilation déterministe des lieux canoniques
- génération de salles par metadata JSON et compilateur hybride
- cache de scènes et graphe cardinal persistant
- rendu headless et interface SDL3
- audit `.scene` séparé

La chaîne actuelle emploie encore des prompts, états, archétypes, prefabs et identifiants liés au datacenter et au désert. Elle ne représente pas encore explicitement :

- une barrière invisible distincte d'une sortie simplement bloquée
- une proposition de mutation topologique avec décision de validation
- un historique des mutations acceptées
- les archétypes et objets de prospection Eryx
- des règles de préservation de repères locaux lors d'un changement de connectivité

Les sections Eryx ci-dessous décrivent donc la cible fonctionnelle, pas l'état actuel du binaire. [`TECHNICAL_STATE.md`](./TECHNICAL_STATE.md) reste la référence factuelle.

## Hypothèse directrice

`Ministral` ne doit pas posséder seul la carte, l'inventaire ou la géométrie finale. Une sortie stable en apparence peut rester fausse spatialement, et `temperature 0` ne garantit ni continuité ni validité.

La voie de production est :

1. conserver l'état autoritatif hors du modèle ;
2. projeter uniquement le contexte nécessaire au tour ;
3. demander au LLM un résultat structuré et borné ;
4. valider séparément les deltas de hard state et les propositions topologiques ;
5. mettre à jour un état spatial sémantique ;
6. compiler cet état vers `scene v1` de manière déterministe ;
7. garder la génération `.scene` libre comme audit expérimental.

Cette séparation transforme l'instabilité spatiale en décision inspectable plutôt qu'en corruption implicite.

## Niveaux d'état

### 1. Hard state

État fiable, persistant et actionnable :

- `turn_number`
- position physique autoritative du joueur dans le graphe
- inventaire, outils, cristaux ou échantillons collectés
- ressources de survie si elles sont retenues
- entités nommées et découvertes majeures
- engagements et conséquences mécaniques déjà établis
- métadonnées de session, du modèle et de validation

Le hard state ne dépend jamais uniquement de la prose du dernier tour. Une mutation topologique ne peut pas faire disparaître un objet collecté ni déplacer arbitrairement la position autoritative.

### 2. Soft state

État compressible et partiellement instable :

- résumé récent
- atmosphère et interprétations locales
- hypothèses du joueur ou du système
- confiance dans la carte
- explications historiques non actionnables
- contradictions tolérées qui ne modifient pas encore la navigation

Le soft state donne une couleur narrative sans devenir la seule source de vérité.

### 3. Spatial state

État sémantique entre fiction et rendu :

- `location_id` stable
- archétype local
- repères visuels persistants
- sorties et adjacences courantes
- obstacles visibles
- barrières invisibles connues ou supposées
- orientation et longueur de route inférées
- ouvertures vers le ciel ou le terrain
- objets visibles et actionnables
- contraintes de composition
- anomalies et preuves indirectes
- provenance et historique des mutations acceptées

Le `SpatialState` ne doit pas devenir une scène 3D générale. Il décrit ce que le moteur doit rendre, conserver, autoriser ou interdire.

### 4. Topologie souple

La topologie mutable est une partie contrôlée du spatial state. Elle peut porter sur :

- une relation d'adjacence
- l'état traversable d'une sortie
- une barrière invisible
- la direction par laquelle un lieu est rejoint
- la compatibilité de deux chemins connus
- la distance ou le nombre d'étapes supposé entre deux repères

La position physique courante du joueur reste autoritative, même si la relation qui l'a conduit au lieu devient impossible à réconcilier avec la carte précédente.

## Chaîne fonctionnelle cible

```text
commande du joueur
    ->
projection du contexte utile
    ->
appel LLM contraint
    ->
validation syntaxique / réparation
    ->
validation des deltas de hard state
    ->
validation de la proposition topologique
    ->
commit hard + soft + spatial
    ->
compilateur de scène déterministe / hybride
    ->
audit de scene v1 ou fallback sûr
    ->
renderer
    ->
texte, image et feedback de traversal
```

Chaque étape doit produire une provenance ou un diagnostic suffisant pour distinguer une erreur de parser, une sortie LLM invalide, un rejet de mutation, un fallback de scène et une contradiction intentionnelle.

## Contrat de tour cible

Le résultat minimal devrait contenir :

```text
intent
narration
clarification
hard_state_delta
soft_state_delta
spatial_delta
topology_proposal
continuity_notes
```

Une proposition topologique ne doit pas être dissimulée dans `narration` ou dans un champ libre de contraintes de scène. Elle doit être typée et validable.

Forme indicative :

```text
topology_proposal
  mutation_type
  source_location_id
  source_exit
  previous_relation
  proposed_relation
  player_evidence
  narrative_reason
  confidence
```

Les noms exacts restent un choix d'implémentation. Les propriétés importantes sont la séparation, l'inspectabilité et la possibilité de refuser la proposition sans perdre le reste du tour.

## Mutations que le LLM peut proposer

Le vocabulaire v1 peut rester très réduit :

- `block_exit_invisible`
- `unblock_exit_invisible`
- `reconnect_exit`
- `impossible_return_direction`
- `divide_open_area`
- `restore_previous_relation`

Le modèle ne propose pas des coordonnées de murs ni une nouvelle carte complète. Il désigne une relation locale et fournit les indices que le joueur peut recevoir.

## Validation des mutations

Le moteur décide si une proposition est acceptée, modifiée ou rejetée.

### Invariants durs

Une mutation ne doit pas :

- effacer ou dupliquer inventaire et découvertes
- invalider l'identité du lieu courant
- laisser le joueur sans action utile, sauf séquence explicitement conçue et récupérable
- rendre une affordance critique définitivement inaccessible sans règle narrative dédiée
- corrompre la sérialisation ou le graphe
- produire une scène locale non rendable

### Contrôles de lisibilité

Le validateur devrait aussi pouvoir limiter :

- le nombre de mutations sur une fenêtre de tours
- deux changements successifs portant sur la même sortie
- la disparition simultanée de tous les repères locaux
- une contradiction qui n'a aucun feedback textuel, traversal ou instrumental
- une accumulation de chemins impossibles sans possibilité de remapper

### Résultats possibles

- `accepted` : proposition commise telle quelle
- `adjusted` : type conservé mais cible, fréquence ou conséquence limitée
- `rejected` : monde inchangé, avec narration réparée si nécessaire
- `deferred` : proposition mémorisée comme possibilité mais non commise

Le résultat doit être journalisé.

## Barrières invisibles et rendu

Une barrière invisible est d'abord une contrainte de traversal. Elle ne doit pas devenir automatiquement un `box` opaque.

Le joueur doit recevoir une information actionnable quand un mouvement échoue : contact, distance estimée, orientation de la surface, résultat du scanner, ou changement dans la poussière et le faisceau.

Le compilateur peut éventuellement produire des preuves indirectes :

- trace de scanner temporaire
- poussière ou vapeur arrêtée sur un plan
- objet semblant s'appuyer sur le vide
- discontinuité d'ombre ou de lumière

Ces effets sont incrémentaux et ne sont pas requis pour la première implémentation. Le texte et le feedback de déplacement restent suffisants pour un test initial, à condition de ne pas ressembler à un échec silencieux.

## Pourquoi ne pas demander une scène complète à chaque tour

Une scène générée librement peut être syntaxiquement valide mais spatialement absurde, dériver d'échelle, inventer des propriétés, contredire les sorties ou être tronquée. Elle mélange alors trop de responsabilités : narration, mémoire, topologie, placement et syntaxe de rendu.

La génération directe `.scene` doit rester :

- un benchmark de la liberté syntaxique du modèle
- un outil de comparaison avec la voie hybride
- un mode d'audit ou de recherche

Elle ne doit ni constituer l'état du monde ni décider seule de la collision.

## Stratégie de scène

Le compilateur de scène reste côté moteur :

1. choisir une coque ou une fixture locale ;
2. préserver les repères persistants du lieu ;
3. placer les masses et objets sémantiques ;
4. réserver la circulation visible ;
5. traduire les contraintes acceptées ;
6. produire et auditer une scène v1.

Exemples de cibles Eryx planifiées :

- `quarry_threshold` + portail de prospection + ciel ouvert
- `scanner_station` + beacon + spécimen + grande zone apparemment libre
- `prospecting_shelter` + unité atmosphérique + sortie de service
- `extraction_field` + rig + pylônes + horizon
- `venus_plateau` + repères locaux rares + sous-structure invisible

Ces archétypes et prefabs ne sont pas encore implémentés. Les identifiants courants `gate`, `server_aisles`, `roof_watch` et les salles datacenter restent des fixtures legacy utiles pour vérifier le pipeline.

## Fallbacks

Si la sortie de tour est inexploitable :

- conserver le dernier hard state valide
- conserver la dernière topologie validée
- conserver ou reconstruire la dernière scène locale valide
- appliquer un no-op explicite et journalisé
- produire un feedback joueur qui ne prétend pas qu'une mutation a eu lieu

Si seule la proposition topologique est invalide, le reste du tour peut être accepté après réparation de la narration. Une erreur technique ne doit pas être automatiquement fictionnalisée comme un pouvoir du labyrinthe.

## Température

Une température d'échantillonnage basse reste adaptée aux résultats structurés. Une variation liée à l'environnement, au stress ou à l'entropie spatiale peut être étudiée plus tard pour la narration.

La température ne doit jamais :

- être la seule source de changement topologique
- s'appliquer au validateur ou au compilateur déterministe
- permettre au modèle de contourner le hard state

## Première tranche Eryx recommandée

Limiter la première expérience à quelques lieux localement distincts et à un petit vocabulaire de mutation :

1. seuil du champ d'extraction
2. station de scanner
3. abri de prospection
4. plateau ou carrière ouverte
5. seuil du labyrinthe invisible

La tranche doit permettre :

- observation et déplacement
- marquage d'un trajet
- rencontre d'une barrière invisible
- détour
- retracement
- une contradiction topologique acceptée
- conservation des objets et découvertes
- remapping ou récupération

## Critère de réussite

Le pipeline sera convaincant si :

- l'état actionnable reste cohérent sur toute la tranche
- le joueur comprend quoi tenter après un contact invisible
- une mutation peut rendre la carte fausse sans rendre la session injouable
- un lieu revisité conserve assez de repères pour être reconnu
- le texte, la traversal et l'image peuvent diverger sans devenir indifférenciables d'un bug
- chaque mutation et fallback reste inspectable dans les logs
- la scène finale provient toujours d'un état validé et d'un compilateur déterministe ou hybride
