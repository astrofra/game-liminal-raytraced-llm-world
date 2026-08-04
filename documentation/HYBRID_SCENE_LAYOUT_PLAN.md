# Hybrid Scene Layout Plan

Derniere mise a jour : 2026-08-03

## Role du document

Ce document formalise une voie hybride pour la generation des salles :

- le LLM choisit **quoi** mettre dans la piece
- le moteur decide **ou** et **comment** disposer ces objets

L'objectif n'est pas de faire un solveur 3D general, mais un pipeline stable, debuggable et suffisamment expressif pour produire des salles lisibles, actionnables et esthetiquement defendables.

Le plan de travail valide est :

1. documentation technique
2. implementation
3. benchmark
4. diagnostic

## Hypothese directrice

La generation libre d'une `.scene` complete par le LLM a montre deux limites distinctes :

- scenes tronquees
- scenes completes mais hors contrat `scene v1`

Le meilleur compromis v1 est donc :

- **LLM** pour les decisions semantiques
- **procedural** pour le layout spatial
- **compilateur deterministe** pour la scene finale

La nuance ajoutee depuis le 3 aout 2026 est la suivante :

- la geographie cachee du monde (`distance + angle` autour du datacenter) ne doit pas piloter la geometrie directement
- elle doit d'abord etre **verbalisee** en un guide de derive cache
- ce guide pousse ensuite le LLM a choisir une famille lexicale, des objets et des asymetries differentes d'une room a l'autre

Le LLM ne doit pas choisir des coordonnees exactes. Il doit choisir :

- le type de salle
- les masses principales
- les objets actionnables
- les ouvertures
- quelques contraintes spatiales qualitatives

Le moteur doit ensuite transformer cela en implantation concrete.

## Cadrage

Le probleme a resoudre n'est pas un moteur de physique.

Le probleme a resoudre est :

- obtenir une salle spatialement plausible
- sans interpenetrations grossieres
- avec une circulation minimale
- avec quelques silhouettes fortes
- en restant deterministe a seed fixee

La bonne granularite pour le v1 est un solveur **2.5D** :

- le placement se fait surtout dans le plan `XZ`
- `Y` est derive de regles simples (`floor`, `wall`, `ceiling`, `stack`)
- les collisions sont traitees via des AABB de layout

## Principe de separation des responsabilites

### Ce que decide le LLM

- archetype de salle
- ambiance locale
- liste d'objets
- importance relative
- affordances actionnables
- orientation ou zone approximative
- exceptions semantiques voulues

### Ce que decide le moteur

- dimensions de la salle
- enveloppe spatiale
- ancres et surfaces disponibles
- position exacte des objets
- avoidance des collisions
- couloir de circulation
- camera finale
- lumiere finale
- compilation vers `scene v1`

## Contrat de donnees recommande

Le LLM ne doit plus sortir directement une `.scene` complete pour la voie principale.

Il doit sortir un contrat de layout plus petit, du type :

```text
room_layout
  room_archetype
  room_scale
  mood
  openings
  dominant_axis
  object_specs[]
```

Chaque `object_spec` devrait contenir au minimum :

```text
id
kind
semantic_role
size_class
priority
mount
zone
facing
against_wall
allow_overlap
stack_on
notes
```

## Champs recommandes

### `room_archetype`

Exemples :

- `entry_threshold`
- `checkpoint`
- `server_aisles`
- `control_hub`
- `backup_vault`
- `cooling_bay`
- `roof_parapet`
- `loading_dock`

### `room_scale`

Valeurs simples :

- `small`
- `medium`
- `large`
- `long`

### `mount`

Valeurs v1 :

- `floor`
- `wall`
- `ceiling`
- `stack`

### `zone`

Valeurs v1 :

- `center`
- `near_entry`
- `far_end`
- `left_band`
- `right_band`
- `north_wall`
- `south_wall`
- `east_wall`
- `west_wall`
- `perimeter`

### `size_class`

Valeurs v1 :

- `small`
- `medium`
- `large`
- `hero`

### `priority`

Entier simple, par exemple :

- `100` pour masse heroique
- `60` pour objet de structure
- `30` pour accessoire important
- `10` pour decor supprimable

### `allow_overlap`

Valeur booleenne a eviter autant que possible.

Mieux vaut la remplacer plus tard par des exceptions explicites :

- `flush_to_wall`
- `embedded_in_wall`
- `under_object`
- `stack_on:<id>`
- `blocks_exit`

## Bibliotheque de dimensions procedurales

Le moteur doit posseder une table deterministe `kind -> dimensions`.

Exemples v1 :

- `prefab_gate`
- `prefab_rack`
- `prefab_crate`
- `prefab_cooling_unit`
- `box_console`
- `box_hatch`
- `box_placard`
- `box_cabinet`
- `box_switch`

Chaque type doit fournir :

- `layout_half_extents_xz`
- `render_size_xyz`
- `mount`
- `wall_clearance`
- `preferred_orientation`

Important :

La bounding box de layout n'est pas obligee d'etre identique a la geometrie rendue. Elle peut etre volontairement plus conservative pour proteger la lisibilite.

## Pipeline recommande

```text
brief spatial
    ->
guide textuel cache de derive (distance + angle -> motifs, dissymetrie, vocabulaire)
    ->
LLM layout contract
    ->
validation semantique
    ->
construction d'une coque de salle
    ->
placement initial par ancres et zones
    ->
solveur iteratif anti-chevauchement
    ->
reserve de circulation
    ->
pruning des objets non essentiels
    ->
camera + spotlight
    ->
scene v1 finale
```

## Algorithme sur papier

### Etape 0 - Guide textuel cache

Avant meme que le LLM ne choisisse les objets, le moteur derive un petit guide textuel a partir de la pose cachee du monde :

- bande radiale : `central core`, `inner technical ring`, `perimeter seam`, `outer parapet`, `open desert`
- relation au portail : `entry-facing side`, `east flank`, `west flank`, `far side opposite the entry gate`
- ouverture probable : interieur ferme, ouvertures partielles, ciel ouvert, desert ouvert
- quelques motifs preferes
- quelques motifs a eviter
- une pression de dissymetrie verbale : `east hatch`, `rear beacon`, `west crate`, etc.

Ce guide n'est pas un contrat geometrique.

Il sert a faire diverger les sorties JSON du LLM sur le plan :

- du titre
- du `location_archetype`
- des `anchors`
- des `visible_objects`
- des `scene_constraints`

Autrement dit :

- la variation doit naitre d'abord du langage
- le solveur spatial ne fait qu'incarner ce langage de facon stable

### Etape 1 - Coque de salle

Le solveur commence par choisir une coque simple selon `room_archetype`.

Exemples :

- `entry_threshold` : rectangle plutot large et peu profond
- `server_aisles` : rectangle long avec axe principal fort
- `control_hub` : petite salle presque carree
- `cooling_bay` : salle moyenne a grande avec masses laterales
- `roof_parapet` : dalle exterieure avec bord et horizon

La coque produit :

- largeur
- profondeur
- hauteur
- surfaces murales utilisables
- point d'entree
- point(s) de sortie
- bandes reservees a la circulation

### Etape 2 - Ancres

Le moteur derive des ancres stables :

- centre
- murs
- coins
- proche entree
- fond de salle
- ligne mediane
- bande gauche / droite

Les objets ne sont pas places directement dans l'espace libre. Ils sont d'abord associes a une ancre compatible.

### Etape 3 - Placement initial

Ordre recommande :

1. masses hero
2. structure secondaire
3. affordances actionnables
4. petits accessoires

Chaque objet recoit :

- une orientation initiale
- une position initiale derivee de `zone`
- une cote d'ancrage si `against_wall = true`

Exemples :

- un `prefab_rack` en `server_aisles` va d'abord sur une bande laterale
- un `crate` va plutot en bord de circulation ou contre un mur
- une `placard` va plutot sur un mur
- un `hatch` va plutot au sol, hors couloir principal

### Etape 4 - Solveur iteratif anti-chevauchement

Le solveur ne doit pas chercher un optimum global. Il doit juste eliminer les recouvrements evidents.

Representer chaque objet par une AABB de layout dans le plan `XZ`.

Pour `N` iterations :

1. parcourir toutes les paires d'objets
2. tester le chevauchement `XZ`
3. si chevauchement :
   - calculer la penetration sur `x`
   - calculer la penetration sur `z`
   - choisir l'axe de separation minimal
   - pousser les deux objets en sens inverse
4. reappliquer les contraintes de murs et de salle
5. resnapper certains objets au mur si necessaire

Schema de poussee :

```text
push_a = penetration * weight_b / (weight_a + weight_b)
push_b = penetration * weight_a / (weight_a + weight_b)
```

Ou :

- `weight` eleve = objet peu mobile
- `weight` faible = objet facilement deplacable

Bon choix v1 :

- masse heroique : peu mobile
- rack / cooling unit : peu mobile
- crate / console basse : mobile
- petit accessoire : tres mobile ou supprimable

### Etape 5 - Couloir de circulation

Apres relaxation, verifier qu'un chemin simple reste libre entre :

- entree
- centre de salle
- sorties ouvertes
- eventuel objet heroique ou affordance majeure

Version v1 tres simple :

- reserver a l'avance une bande de circulation
- interdire aux objets volumineux d'y entrer

Version v2 :

- grille 2D grossiere
- cellules bloquees par AABB
- BFS entre entree et sorties

Si la circulation echoue :

1. deplacer les objets les moins prioritaires
2. sinon supprimer les objets decoratifs
3. sinon reduire la taille de certains objets secondaires

### Etape 6 - Exceptions semantiques

Certaines collisions doivent rester possibles, mais seulement si elles sont explicites.

Cas permis plus tard :

- caisse partiellement sous une console
- panneau encastre dans un mur
- objet pose sur une autre masse
- obstruction volontaire d'une sortie

Le solveur doit donc distinguer :

- collision interdite
- collision toleree
- collision requise

Mais le v1 peut commencer avec seulement :

- `forbid overlap`
- `allow wall embedding`
- `allow stack_on`

### Etape 7 - Camera

La camera ne doit pas etre laissee au LLM.

Le moteur choisit un gabarit de camera par archetype :

- seuil : frontal legerement decentre
- travees : axe de fuite
- controle : legere plongee ou face centrale
- toit : profondeur vers l'horizon
- cooling bay : masse centrale ou laterale dans un grand noir

La camera peut ensuite ajuster :

- distance
- yaw
- pitch

pour garder au moins :

- une masse principale lisible
- une affordance visible
- une ouverture ou un bord de salle

## Strategie de pruning

Si le layout n'arrive pas a converger, ne pas raffiner le solveur a l'infini.

Il faut supprimer.

Ordre recommande :

1. petits accessoires
2. doublons semantiques
3. objets decoratifs redondants
4. objets secondaires hors gameplay

Ne jamais supprimer :

- ouverture principale
- masse heroique
- affordance critique

## Determinisme

Le layout doit etre reproductible.

Chaque salle doit etre stabilisee par :

- seed derivee de l'identite de salle
- meme archetype
- meme liste d'objets
- memes regles de placement

Cela permet :

- comparaison visuelle
- debug
- benchmark
- reprise de sauvegarde

## Pourquoi cette approche n'est pas trop risquee

Elle reste defendable si :

- le solveur reste petit
- les archetypes restent peu nombreux
- les objets restent dans une bibliotheque fermee
- le LLM ne manipule pas des coordonnees libres

Elle devient risquee si :

- on laisse le LLM inventer des primitives et leurs positions exactes
- on attend du solveur qu'il sauve n'importe quelle scene
- on veut gerer de vraies geometries 3D generales

Le risque principal n'est donc pas l'idee du solveur. Le risque principal serait de lui donner un probleme trop libre.

## Plan d'implementation

### Etape A - Structures

Ajouter des structures du type :

```text
RoomLayoutDraft
RoomShell
LayoutObjectSpec
PlacedLayoutObject
LayoutSolveReport
```

### Etape B - Nouveau contrat LLM

Le LLM doit sortir un bloc structure pour les objets, pas une `.scene` complete, par exemple :

```text
room_archetype
room_scale
openings
object_specs[]
```

Le chemin `.scene` direct doit rester disponible comme benchmark et mode d'audit, pas comme chemin principal.

### Etape C - Table des prefabs et boites semantiques

Ajouter une table interne :

- dimensions de layout
- dimensions de rendu
- type de montage
- poids de deplacement
- regles d'ancrage

### Etape D - Solveur

Implementer dans cet ordre :

1. coque
2. ancres
3. placement initial
4. relaxation anti-chevauchement
5. reserve de circulation
6. pruning
7. compilation `.scene`

### Etape E - Camera

Sortir la camera du LLM et la rendre derivee de :

- archetype
- masse dominante
- ouverture
- horizon

## Plan de benchmark

Le benchmark doit comparer :

1. `.scene` libre directe par le LLM
2. layout hybride `LLM -> solveur -> scene`

Mesures minimales :

- taux de scenes valides
- nombre d'interpenetrations restantes
- nombre d'objets supprimes
- temps de generation LLM
- temps de solveur
- temps de rendu
- triangles produits
- stabilite visuelle sur reruns

Jeux de cas recommandes :

- seuil d'entree
- checkpoint
- salle de controle
- travees de serveurs
- baie de refroidissement
- toit / parapet
- dock de chargement

## Plan de diagnostic

Le solveur doit pouvoir sortir des artefacts lisibles.

Minimum utile :

- dump JSON du contrat LLM
- dump du placement initial
- dump du placement final
- rapport de collisions resolues
- rapport de pruning
- image finale
- eventuellement une vue debug 2D `top-down`

Le diagnostic doit rendre possible la question :

> est-ce que l'erreur vient du choix du LLM, du solveur, de la camera, ou du renderer ?

## Criteres de reussite

Cette approche sera validee si elle permet :

- des salles plus coherentes spatialement que la `.scene` libre
- moins d'echecs de syntaxe
- moins de substitutions absurdes de prefabs
- des images plus lisibles ou plus fortes
- une stabilite suffisante sur plusieurs runs

## Conclusion

La bonne ambition pour le v1 n'est pas :

- un generateur de scene general

La bonne ambition est :

- un **compositeur de salles pauvres**, contraint, hybride, stable et auditable

Le LLM doit choisir les ingredients.

Le moteur doit faire la mise en place.
