# Catalogue des prefabs — carrière d’Eryx

Généré le 2026-08-09 18:49:46.

Ce document est généré depuis les géométries C++ réellement prises en charge par le format `scene v1`. Il fixe le vocabulaire visuel actif de la carrière de cristal vénusienne : infrastructure humaine brutaliste, instruments de prospection lisibles, cristaux facettés et repères de navigation.

La structure alien reste volontairement absente du catalogue : ses murs sont une propriété de collision et de topologie, pas un prefab visible. Les anciennes directives de rack, refroidissement et serveur IA restent acceptées pour la compatibilité des scènes historiques, mais ne font plus partie de ce vocabulaire actif.

Sources de direction : [brief de réorientation](ERYX_PROJECT_REORIENTATION_CODEX_BRIEF.md), [nouvelle](IN_THE_WALLS_OF_ERYX.md), [origines visuelles](moodboard/origins/) et [brutalisme](moodboard/brutalism/).

Notes :
- chaque visuel est rendu par `liminal_cornell_renderer` à partir du `.scene` adjacent
- rendu carré sans changement d’architecture du raytraceur : seul le buffer de sortie change
- chaque objet repose sur un plan de carrière neutre sous un ciel brumeux sans étoiles
- deux vues par prefab : 3/4 gauche et 3/4 droite
- paramètres de rendu utilisés : `1024x1024`, `24` samples par pixel

## prefab_gate

Seuil de carrière asymétrique : deux piles massives, un linteau en porte-à-faux et une grille de contrôle légère. Il marque une limite humaine sans représenter les murs invisibles du labyrinthe.

Scene gauche : `generated/prefab_catalog/scenes/prefab_gate_left.scene`
Scene droite : `generated/prefab_catalog/scenes/prefab_gate_right.scene`

Vue 3/4 gauche : ![prefab_gate left](generated/prefab_catalog/images/prefab_gate_left.png)

Vue 3/4 droite : ![prefab_gate right](generated/prefab_catalog/images/prefab_gate_right.png)

Directive rendue :

```text
prefab_gate "catalog_quarry_gate" pos(0.0,1.90,0.0) size(6.60,3.80,0.65) gray(0.31) detail(0.48) bars(6)
```

## prefab_crate

Container de prélèvement, réserve d'oxygène ou caisse d'outillage. Le bandeau frontal et le sceau clair en font une prise d'action lisible dans une scène pauvre.

Scene gauche : `generated/prefab_catalog/scenes/prefab_crate_left.scene`
Scene droite : `generated/prefab_catalog/scenes/prefab_crate_right.scene`

Vue 3/4 gauche : ![prefab_crate left](generated/prefab_catalog/images/prefab_crate_left.png)

Vue 3/4 droite : ![prefab_crate right](generated/prefab_catalog/images/prefab_crate_right.png)

Directive rendue :

```text
prefab_crate "catalog_sample_case" pos(0.0,0.60,0.0) size(1.90,1.20,1.50) gray(0.20) detail(0.36)
```

## prefab_survey_beacon

Balise de route et mât de relèvement. Sa fourche ouverte, sa barre de visée et son unique lampe fournissent un repère fragile lorsque les relations spatiales dérivent.

Scene gauche : `generated/prefab_catalog/scenes/prefab_survey_beacon_left.scene`
Scene droite : `generated/prefab_catalog/scenes/prefab_survey_beacon_right.scene`

Vue 3/4 gauche : ![prefab_survey_beacon left](generated/prefab_catalog/images/prefab_survey_beacon_left.png)

Vue 3/4 droite : ![prefab_survey_beacon right](generated/prefab_catalog/images/prefab_survey_beacon_right.png)

Directive rendue :

```text
prefab_survey_beacon "catalog_route_datum" pos(0.0,1.90,0.0) size(1.20,3.80,1.10) gray(0.24) detail(0.44)
```

## prefab_crystal_scanner

Instrument d'affinité pour localiser et examiner un échantillon. Deux piles épaisses encadrent un petit cristal et une ligne de mesure suspendue.

Scene gauche : `generated/prefab_catalog/scenes/prefab_crystal_scanner_left.scene`
Scene droite : `generated/prefab_catalog/scenes/prefab_crystal_scanner_right.scene`

Vue 3/4 gauche : ![prefab_crystal_scanner left](generated/prefab_catalog/images/prefab_crystal_scanner_left.png)

Vue 3/4 droite : ![prefab_crystal_scanner right](generated/prefab_catalog/images/prefab_crystal_scanner_right.png)

Directive rendue :

```text
prefab_crystal_scanner "catalog_affinity_scanner" pos(0.0,1.25,0.0) size(2.60,2.50,1.80) gray(0.25) detail(0.45) glow(0.28)
```

## prefab_crystal_cluster

Veine cristalline affleurante, à la fois ressource, balise lumineuse et appât. Les cinq prismes sont facettés en C++ et non assemblés comme de simples boîtes.

Scene gauche : `generated/prefab_catalog/scenes/prefab_crystal_cluster_left.scene`
Scene droite : `generated/prefab_catalog/scenes/prefab_crystal_cluster_right.scene`

Vue 3/4 gauche : ![prefab_crystal_cluster left](generated/prefab_catalog/images/prefab_crystal_cluster_left.png)

Vue 3/4 droite : ![prefab_crystal_cluster right](generated/prefab_catalog/images/prefab_crystal_cluster_right.png)

Directive rendue :

```text
prefab_crystal_cluster "catalog_exposed_vein" pos(0.0,1.30,0.0) size(2.00,2.60,1.80) gray(0.66) glow(0.34)
```

## prefab_extraction_rig

Portique de forage compact : jambes inclinées, tête suspendue, colonne et pointe de coupe. Il exprime l'extraction sans devenir une machine décorative complexe.

Scene gauche : `generated/prefab_catalog/scenes/prefab_extraction_rig_left.scene`
Scene droite : `generated/prefab_catalog/scenes/prefab_extraction_rig_right.scene`

Vue 3/4 gauche : ![prefab_extraction_rig left](generated/prefab_catalog/images/prefab_extraction_rig_left.png)

Vue 3/4 droite : ![prefab_extraction_rig right](generated/prefab_catalog/images/prefab_extraction_rig_right.png)

Directive rendue :

```text
prefab_extraction_rig "catalog_diamond_drill" pos(0.0,1.80,0.0) size(3.00,3.60,2.40) gray(0.23) detail(0.43)
```

## prefab_prospect_shelter

Abri de prospection brutaliste : socle, masses décalées, entrée profondément en retrait et toiture en porte-à-faux. Une architecture humaine, visible et lourde.

Scene gauche : `generated/prefab_catalog/scenes/prefab_prospect_shelter_left.scene`
Scene droite : `generated/prefab_catalog/scenes/prefab_prospect_shelter_right.scene`

Vue 3/4 gauche : ![prefab_prospect_shelter left](generated/prefab_catalog/images/prefab_prospect_shelter_left.png)

Vue 3/4 droite : ![prefab_prospect_shelter right](generated/prefab_catalog/images/prefab_prospect_shelter_right.png)

Directive rendue :

```text
prefab_prospect_shelter "catalog_field_shelter" pos(0.0,1.70,0.0) size(5.60,3.40,4.20) gray(0.28) detail(0.41)
```

## prefab_quarry_pylon

Pylône de cote implanté sur un gradin de carrière. Sa couronne fendue reprend les volumes brutalistes emboîtés et distingue un repère industriel d'une simple lampe.

Scene gauche : `generated/prefab_catalog/scenes/prefab_quarry_pylon_left.scene`
Scene droite : `generated/prefab_catalog/scenes/prefab_quarry_pylon_right.scene`

Vue 3/4 gauche : ![prefab_quarry_pylon left](generated/prefab_catalog/images/prefab_quarry_pylon_left.png)

Vue 3/4 droite : ![prefab_quarry_pylon right](generated/prefab_catalog/images/prefab_quarry_pylon_right.png)

Directive rendue :

```text
prefab_quarry_pylon "catalog_quarry_datum" pos(0.0,2.00,0.0) size(1.60,4.00,1.40) gray(0.27) detail(0.45)
```

## prefab_atmospheric_processor

Unité de service atmosphérique et d'oxygène : masse basse, double cheminée et prises d'air frontales. Elle remplace la climatisation de datacenter par un besoin propre à Vénus.

Scene gauche : `generated/prefab_catalog/scenes/prefab_atmospheric_processor_left.scene`
Scene droite : `generated/prefab_catalog/scenes/prefab_atmospheric_processor_right.scene`

Vue 3/4 gauche : ![prefab_atmospheric_processor left](generated/prefab_catalog/images/prefab_atmospheric_processor_left.png)

Vue 3/4 droite : ![prefab_atmospheric_processor right](generated/prefab_catalog/images/prefab_atmospheric_processor_right.png)

Directive rendue :

```text
prefab_atmospheric_processor "catalog_oxygen_service" pos(0.0,1.60,0.0) size(2.50,3.20,2.00) gray(0.24) detail(0.41)
```
