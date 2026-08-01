# Prefab Catalog

Genere le 2026-08-01 19:00:01.

Ce document rassemble le premier catalogue visuel des prefabs `scene v1` dans un seul Markdown.

Notes :
- chaque visuel est rendu par `liminal_cornell_renderer` a partir du `.scene` adjacent
- rendu carre sans changement d'architecture du raytraceur : seul le buffer de sortie change
- studio ouvert avec `sky` visible et sans sol : les prefabs flottent volontairement dans le vide
- deux vues par prefab : 3/4 gauche et 3/4 droite
- parametres de rendu utilises : `768x768`, `8` samples par pixel

## prefab_gate

Portail de seuil controle, utile pour les entrees, grilles et sas.

Scene gauche : `generated/prefab_catalog/scenes/prefab_gate_left.scene`
Scene droite : `generated/prefab_catalog/scenes/prefab_gate_right.scene`

Vue 3/4 gauche : ![prefab_gate left](generated/prefab_catalog/images/prefab_gate_left.png)

Vue 3/4 droite : ![prefab_gate right](generated/prefab_catalog/images/prefab_gate_right.png)

Directive rendue :

```text
prefab_gate "catalog_gate" pos(0.0,1.55,0.0) size(6.15,3.10,0.40) gray(0.46) detail(0.58) bars(5)
```

## prefab_rack

Baie serveur ou rack technique. Le renderer y injecte automatiquement des LEDs rouges.

Scene gauche : `generated/prefab_catalog/scenes/prefab_rack_left.scene`
Scene droite : `generated/prefab_catalog/scenes/prefab_rack_right.scene`

Vue 3/4 gauche : ![prefab_rack left](generated/prefab_catalog/images/prefab_rack_left.png)

Vue 3/4 droite : ![prefab_rack right](generated/prefab_catalog/images/prefab_rack_right.png)

Directive rendue :

```text
prefab_rack "catalog_rack" pos(0.0,1.25,0.0) size(1.30,2.50,1.60) gray(0.19) detail(0.35)
```

## prefab_crate

Caisse, bloc de service ou console basse pour meubler une salle et ajouter des prises d'action.

Scene gauche : `generated/prefab_catalog/scenes/prefab_crate_left.scene`
Scene droite : `generated/prefab_catalog/scenes/prefab_crate_right.scene`

Vue 3/4 gauche : ![prefab_crate left](generated/prefab_catalog/images/prefab_crate_left.png)

Vue 3/4 droite : ![prefab_crate right](generated/prefab_catalog/images/prefab_crate_right.png)

Directive rendue :

```text
prefab_crate "catalog_crate" pos(0.0,0.55,0.0) size(1.70,1.10,1.40) gray(0.20) detail(0.31)
```

## prefab_cooling_unit

Bloc de climatisation ou masse technique pour fixer une silhouette industrielle stable.

Scene gauche : `generated/prefab_catalog/scenes/prefab_cooling_unit_left.scene`
Scene droite : `generated/prefab_catalog/scenes/prefab_cooling_unit_right.scene`

Vue 3/4 gauche : ![prefab_cooling_unit left](generated/prefab_catalog/images/prefab_cooling_unit_left.png)

Vue 3/4 droite : ![prefab_cooling_unit right](generated/prefab_catalog/images/prefab_cooling_unit_right.png)

Directive rendue :

```text
prefab_cooling_unit "catalog_cooling_unit" pos(0.0,1.35,0.0) size(1.20,2.70,3.10) gray(0.25) detail(0.37)
```
