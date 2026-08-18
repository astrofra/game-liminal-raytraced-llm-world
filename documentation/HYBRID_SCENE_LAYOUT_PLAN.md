# Hybrid Scene Layout Plan

Dernière mise à jour : 2026-08-18

## Rôle du document

Ce document formalise le compositeur hybride des scènes locales :

- le LLM choisit **quoi** doit être signifiant dans le lieu ;
- l'état spatial validé détermine **ce qui est autorisé ou traversable** ;
- le moteur décide **où** et **comment** disposer la géométrie visible.

L'objectif reste un pipeline stable, déterministe, auditable et assez expressif pour produire des lieux lisibles. La réorientation Eryx change les archétypes, objets et tensions spatiales, pas le principe du pipeline.

## Statut

La voie hybride active possède les huit archétypes Eryx proposés dans ce document, les neuf prefabs de prospection/extraction, des coques dédiées et des fallbacks de carrière. Les sept lieux canoniques utilisent des fixtures source afin de garantir la composition de la première tranche.

La barrière invisible est maintenant représentée dans l'état de traversal et reste absente de la géométrie visible. Les benchmarks datacenter restent une baseline historique ; ils ne décrivent pas les prompts actifs. Le validateur de propositions topologiques live reste à implémenter.

## Hypothèse directrice

La génération libre d'une scène complète par le LLM a montré des troncatures et des sorties hors grammaire. Le compromis v1 reste :

- **LLM** pour les décisions sémantiques et les contraintes qualitatives ;
- **validateur de topologie** pour les changements de traversal ;
- **procédural** pour le layout visible ;
- **compilateur déterministe** pour la scène finale.

Le modèle ne choisit pas les coordonnées exactes. Il choisit un type de lieu, des masses, des objets actionnables, des repères et des asymétries. Le moteur incarne ces choix en protégeant les sorties, la lisibilité, les repères persistants et le budget géométrique.

## Principe Eryx : local visible, topologie invisible

Le layout visible représente l'infrastructure humaine et le terrain : carrière, plateau, abri, station, rig, pylon, cargo, tranchée ou rampe.

Le labyrinthe alien appartient principalement à :

- l'état d'adjacence
- la collision et la traversal
- les barrières invisibles
- les changements de route
- le feedback narratif ou instrumental

Le solveur ne doit pas matérialiser toutes les relations de graphe sous forme de corridors et de murs. Une zone ouverte peut être divisée topologiquement tout en restant visuellement ouverte.

## Séparation des responsabilités

### Le LLM décide

- l'archétype local et l'ambiance concise
- les objets et repères sémantiques
- leur importance relative
- les affordances actionnables
- une orientation ou zone qualitative
- des contraintes de composition
- une proposition topologique séparée, si le contrat de tour l'autorise

### Le validateur topologique décide

- si une sortie ou relation peut changer
- si une barrière invisible peut apparaître ou disparaître
- si la fréquence de mutation reste acceptable
- si hard state, progression et récupération restent valides
- quels indices joueur doivent accompagner le changement

### Le moteur de layout décide

- dimensions et coque visible
- ancres et surfaces utilisables
- position et orientation exactes
- évitement des collisions visibles
- circulation locale
- persistance des repères d'un lieu revisité
- camera, lumière et compilation `scene v1`

## Contrat de layout recommandé

La voie principale reçoit un contrat de ce type :

```text
room_layout
  location_id
  room_archetype
  room_scale
  mood
  visible_openings
  dominant_axis
  persistent_anchor_ids[]
  object_specs[]
  scene_constraints[]
```

Chaque `object_spec` peut contenir :

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
stack_on
notes
```

Les barrières invisibles et adjacences ne doivent pas être cachées dans `object_specs`. Elles appartiennent à un contrat topologique distinct.

## Archétypes Eryx implémentés

Le premier vocabulaire doit rester petit :

- `quarry_threshold` : seuil large entre terrain et infrastructure
- `extraction_field` : extérieur ouvert ponctué de machines et pylônes
- `venus_plateau` : terrain vaste, peu d'ancres, forte importance du hors-champ
- `industrial_service_zone` : passage technique entre modules de prospection
- `prospecting_shelter` : intérieur compact, localement stable
- `scanner_station` : instrument principal et zone apparemment ouverte à tester
- `labyrinth_threshold` : lieu où la traversal cesse de correspondre au visible
- `quarry_cut` : tranchée, rampe ou excavation guidant localement le mouvement

Ces huit valeurs sont reconnues par le runtime et possèdent des coques, proportions et prefabs par défaut distincts.

## Échelles, montages et zones

### `room_scale`

- `small`
- `medium`
- `large`
- `long`
- `open`

### `mount`

- `floor`
- `wall`
- `ceiling`
- `stack`

### `zone`

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
- `horizon_marker`

### `size_class`

- `small`
- `medium`
- `large`
- `hero`

### `priority`

- `100` : repère héroïque ou affordance critique
- `60` : masse de structure
- `30` : objet actionnable secondaire
- `10` : décor supprimable

Les exceptions de chevauchement doivent être typées (`flush_to_wall`, `embedded_in_wall`, `stack_on`) plutôt que résumées par un `allow_overlap` général.

## Bibliothèque sémantique active

### Géométrie conservée ou réinterprétée

- `prefab_gate` : seuil de périmètre, pression ou survey checkpoint
- `prefab_crate` : cargo, échantillons, oxygène ou outils
- `prefab_rack`, `prefab_cooling_unit` et `prefab_ai_server` : compatibilité legacy uniquement ; ils sont absents du catalogue actif tant qu'une scène historique ne les demande pas

La géométrie du seuil et de la caisse a été refondue. Les autres noms runtime historiques n'ont pas été maquillés en objets Eryx.

### Géométrie Eryx implémentée

- `prefab_survey_beacon` : survey beacon / navigation mast
- `prefab_crystal_scanner` : crystal scanner / sample instrument
- `prefab_crystal_cluster` : crystal cluster / exposed specimen
- `prefab_extraction_rig` : mining drill / extraction rig
- `prefab_prospect_shelter` : shelter / prospecting station module
- `prefab_quarry_pylon` : quarry marker / industrial pylon
- `prefab_atmospheric_processor` : oxygen and atmospheric service

Chaque type doit fournir :

- dimensions de layout conservatrices
- dimensions de rendu
- montage préféré
- dégagement nécessaire
- orientation préférée
- coût géométrique approximatif
- rôle sémantique et actionnable

Le catalogue doit maximiser la valeur sémantique par élément géométrique. Aucun grand ensemble décoratif n'est nécessaire.

## Coques visibles

Le solveur choisit une coque simple selon l'archétype.

### `quarry_threshold`

- rectangle large et peu profond
- un bord construit, une ouverture vers le terrain
- gate, marker ou shelter comme repère
- ciel et espace négatif importants

### `extraction_field`

- plan ouvert sans plafond
- quelques masses éloignées
- rig ou beacon comme repère principal
- circulation visible non assimilée à un couloir

### `venus_plateau`

- horizon et terrain dominant
- très peu d'objets
- repères assez distincts pour tester les déplacements
- forte place laissée à la topologie invisible

### `prospecting_shelter`

- petit intérieur ou volume semi-ouvert
- une sortie principale lisible
- équipement persistant
- identité locale forte pour les revisites

### `scanner_station`

- instrument ou console héroïque
- champ de test apparent devant ou autour
- pylon ou beacon secondaire
- cadrage permettant de voir un espace que la traversal pourra contredire

### `labyrinth_threshold`

- géométrie humaine minimale
- espace apparemment ouvert
- au moins deux trajectoires testables
- aucun corridor alien visible par défaut

## Pipeline recommandé

```text
état spatial validé
    -> guide textuel de variation locale
    -> contrat sémantique LLM
    -> validation du vocabulaire
    -> choix de coque
    -> restauration des repères persistants
    -> ancres et placement initial
    -> relaxation anti-chevauchement
    -> réserve de circulation visible
    -> pruning
    -> camera + spotlight
    -> scene v1
    -> audit / fallback
```

La topologie invisible est évaluée en parallèle pour la traversal. Elle n'entre dans la géométrie que si une preuve indirecte explicitement validée doit être rendue.

## Guide textuel de variation

Le système actuel dérive un guide de monde depuis une pose cachée autour du datacenter. La migration Eryx doit remplacer cette sémantique par des cues comme :

- relation au camp ou à l'abri de prospection
- profondeur dans la carrière ou distance sur le plateau
- exposition au ciel et au terrain
- densité d'infrastructure humaine
- proximité supposée du labyrinthe
- confiance de navigation
- asymétrie locale : beacon à l'est, rampe arrière, rig sur le flanc, etc.

Le guide influence titre, archétype, repères, objets et contraintes de scène. Il ne définit pas une géométrie secrète exacte et ne doit pas annuler une mutation topologique validée.

## Placement 2.5D

Le placement visible reste principalement dans le plan `XZ`. `Y` est dérivé de règles simples (`floor`, `wall`, `ceiling`, `stack`). Les collisions utilisent des AABB de layout volontairement conservatrices.

### Ordre de placement

1. repères persistants
2. masse héroïque
3. structure secondaire
4. affordances actionnables
5. petits accessoires

Une revisite doit réutiliser la seed et les repères du lieu. Une mutation de connectivité ne doit pas relancer arbitrairement toute la composition.

### Ancres

- centre
- murs ou bord construit
- coins
- proximité de l'entrée
- fond visible
- ligne médiane
- bandes gauche et droite
- horizon marker

### Relaxation anti-chevauchement

Pour chaque paire d'AABB en conflit :

1. calculer la pénétration sur `x` et `z` ;
2. choisir l'axe de séparation minimal ;
3. pousser selon la mobilité relative ;
4. réappliquer les limites de coque et les contraintes de montage ;
5. supprimer un objet faible priorité si le conflit persiste.

Forme de poussée :

```text
push_a = penetration * weight_b / (weight_a + weight_b)
push_b = penetration * weight_a / (weight_a + weight_b)
```

Les repères persistants et masses héroïques sont peu mobiles. Les accessoires sont mobiles ou supprimables.

## Circulation visible versus traversal réelle

Le solveur doit conserver une lecture locale : entrée, centre, sorties visibles et affordance majeure ne doivent pas être obstrués accidentellement par le décor.

Cette circulation visible ne garantit pas la traversal réelle. Une barrière invisible validée peut refuser une trajectoire apparemment libre. La différence doit provenir de l'état topologique, pas d'un chevauchement géométrique involontaire.

Le diagnostic doit donc distinguer :

- collision de layout visible, à corriger
- obstacle visible intentionnel
- barrière invisible intentionnelle
- sortie sans relation topologique

## Camera

La camera reste contrôlée par le moteur. Gabarits planifiés :

- seuil : frontal légèrement décentré
- carrière : axe de rampe ou de tranchée
- plateau : horizon et repère vertical
- abri : affordance et sortie dans le même cadre si possible
- scanner : instrument et champ ouvert
- seuil invisible : espace ouvert et repères permettant de comparer les trajectoires

La camera doit montrer au moins une masse principale, une affordance et une relation spatiale utile. Elle ne doit pas révéler automatiquement les surfaces invisibles.

## Pruning et budget géométrique

Si le layout ne converge pas, supprimer dans cet ordre :

1. accessoires
2. doublons sémantiques
3. décor redondant
4. objets secondaires sans fonction

Ne jamais supprimer :

- repère persistant requis
- affordance critique
- sortie visible nécessaire à la composition
- masse héroïque définissant l'identité du lieu

Le coût des prefabs doit rester compatible avec l'inversion computationnelle. Un nouveau prefab doit justifier son nombre de primitives par une forte valeur de lecture ou d'action.

## Déterminisme et revisite

Chaque lieu doit être stabilisé par :

- identité et seed persistantes
- même archétype de base
- repères persistants
- mêmes règles de placement
- historique des mutations séparé de la composition locale

Une revisite peut modifier les sorties, des détails secondaires ou les preuves du labyrinthe. Elle doit conserver un fingerprint local suffisant pour que la contradiction globale soit perceptible.

## Fallbacks

En cas d'échec :

- garder les repères persistants
- supprimer les objets de faible priorité
- simplifier la coque
- produire une scène de fallback Eryx sobre après son implémentation
- tant que cette migration n'est pas codée, conserver explicitement le fallback legacy actuel

Un fallback technique ne doit pas introduire une mutation topologique ni être présenté au joueur comme un phénomène du labyrinthe.

## Plan d'implémentation

### A — Vocabulaire sémantique

- ajouter les archétypes Eryx
- borner `scene_constraints`
- documenter les repères persistants
- supprimer les instructions actives de prompt datacenter

### B — Prefabs — première passe réalisée le 2026-08-09

- géométries convaincantes réinterprétées sans casser la lecture des scènes historiques
- sept nouveaux éléments à forte valeur sémantique implémentés
- dimensions, zone de placement et blocage de corridor ajoutés au compilateur hybride
- [`PREFAB_CATALOG.md`](./PREFAB_CATALOG.md) régénéré depuis les sources ; le suivi du coût géométrique par objet reste à formaliser

### C — Persistance locale

- associer seed et repères au `location_id`
- séparer les détails secondaires des ancres d'identité
- tester les revisites avant les mutations

### D — Topologie invisible

- implémenter barrière invisible et feedback de traversal
- ajouter proposition et validateur de mutation
- ne transmettre au layout que les preuves indirectes à rendre

### E — Diagnostic et benchmark

- dump du contrat LLM
- dump du placement initial/final
- rapport de collisions et pruning
- état topologique et décision du validateur
- scène finale et provenance
- vue top-down de layout visible optionnelle
- vue topologique debug séparée

## Plan de benchmark

Conserver les rapports datacenter du 2026-08-02 comme baseline historique. Une nouvelle batterie Eryx ne doit être générée qu'après implémentation des archétypes et prefabs réels.

Cas recommandés :

- quarry threshold
- scanner station
- prospecting shelter
- extraction field
- Venusian plateau
- labyrinth threshold

Mesures :

- taux de scènes valides
- collisions visibles restantes
- objets supprimés
- conservation des repères sur revisite
- temps LLM, layout et rendu
- triangles et matériaux
- stabilité à seed fixe
- absence de géométrie opaque créée pour une barrière invisible

## Critères de réussite

La migration hybride Eryx est validée si :

- les lieux lisent comme extraction/prospection plutôt que datacenter
- les extérieurs ouverts restent composés avec peu d'éléments
- les revisites préservent des repères locaux
- une mutation topologique n'oblige pas à reconstruire toute la scène
- barrière invisible et collision géométrique accidentelle sont distinctes
- aucun résultat de benchmark n'est inventé ou extrapolé depuis les fixtures legacy
- le solveur reste petit, déterministe et auditable

## Conclusion

La bonne ambition n'est pas un générateur général de mondes 3D ni un dédale visible.

Elle est un **compositeur de lieux de prospection pauvres**, capable de préserver l'identité locale pendant qu'une couche topologique distincte négocie le labyrinthe invisible.
