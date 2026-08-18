# Scene Format V1

Dernière mise à jour : 2026-08-09

## Statut

Ce document decrit le format de scene proprietaire v1 actuellement implemente dans le depot.

Le format cible plus large de la spec n'est pas encore entièrement supporté. L'implémentation actuelle couvre un sous-ensemble volontairement petit, suffisant pour le pipeline scène -> validation -> rendu. Le vocabulaire de prefabs actif est désormais orienté vers la carrière de cristal vénusienne ; les prefabs datacenter restent lisibles pour la compatibilité des scènes historiques.

## Intentions

Le format v1 doit rester :

- lisible par un humain
- facile a produire ou corriger
- compact
- defensif a parser
- plus stable qu'un `.OBJ` genere brut

Le format reste a deux niveaux :

- un niveau bas avec primitives explicites (`plane`, `box`)
- un niveau intermediaire avec quelques prefabs deterministes expands en primitives simples

## Directives actuellement supportees

### `room`

Definit un nom de scene.

Exemple :

```text
room "liminal service corridor"
```

### `camera`

Definit la camera de rendu.

Proprietes supportees :

- `eye(x,y,z)` obligatoire
- `target(x,y,z)` obligatoire
- `up(x,y,z)` optionnel
- `fov(value)` optionnel

Exemple :

```text
camera eye(0.0,1.45,-7.4) target(0.4,1.30,4.0) up(0.0,1.0,0.0) fov(50.0)
```

### `spotlight`

Definit un spot analytique attache a la camera.

Proprietes supportees :

- `panel(width,height)` obligatoire
- `offset(x,y,z)` obligatoire, exprime en espace camera
- `range(value)` obligatoire
- `cone(inner,outer)` obligatoire, en degres
- `intensity(value)` obligatoire

Le spot est rendu comme un petit panneau lumineux 1x1m fixe a la camera, oriente vers l'avant, avec attenuation par distance et cone progressif.

Exemple :

```text
spotlight panel(1.0,1.0) offset(0.0,0.0,0.35) range(12.0) cone(16.0,38.0) intensity(180.0)
```

### `sky`

Active un fond proceduriel palette-limitee pour les rayons qui ne rencontrent aucune geometrie.

Proprietes supportees :

- `zenith(value)` obligatoire
- `horizon(value)` obligatoire
- `nadir(value)` obligatoire
- `band(value)` obligatoire
- `curve(value)` obligatoire
- `noise(value)` obligatoire
- `stars(density,intensity,radius)` optionnel
- `seed(value)` optionnel

Le ciel est pense pour les exterieurs vénusiens du projet : dégradé vert au zénith vers jaune clair à l'horizon, nadir sombre, grain fort et étoiles déterministes possibles. Le renderer multiplie sa radiance par deux lorsqu'un chemin ne rencontre aucune géométrie ; les luminances de la directive restent donc des contrôles relatifs de forme et d'exposition.

Exemple :

```text
sky zenith(0.01) horizon(0.24) nadir(0.00) band(0.32) curve(1.95) noise(0.12) stars(0.0036,1.55,0.100) seed(77)
```

### `prefab_gate`

Prefab de seuil de carrière asymétrique, composé de piles massives, d'un linteau en porte-à-faux, d'un contrefort et de barres de contrôle. Il représente une infrastructure humaine visible, jamais un mur alien invisible.

Proprietes supportees :

- nom entre guillemets obligatoire
- `pos(x,y,z)` obligatoire
- `size(x,y,z)` obligatoire
- `gray(value)` obligatoire
- `detail(value)` optionnel
- `bars(count)` optionnel

Exemple :

```text
prefab_gate "entry_gate" pos(0.0,1.55,4.8) size(6.15,3.10,0.40) gray(0.46) detail(0.58) bars(5)
```

### `prefab_rack` — legacy

Prefab historique de baie ou rack technique, conservé pour les scènes et benchmarks datacenter.

Le renderer injecte automatiquement de petites LEDs rouges sur la facade du rack. Elles n'ont pas besoin d'etre decrites dans le fichier `.scene`.

Proprietes supportees :

- nom entre guillemets obligatoire
- `pos(x,y,z)` obligatoire
- `size(x,y,z)` obligatoire
- `gray(value)` obligatoire
- `detail(value)` optionnel

Exemple :

```text
prefab_rack "rack_left_01" pos(-2.9,1.25,-5.8) size(1.2,2.5,1.5) gray(0.19) detail(0.35)
```

### `prefab_crate`

Prefab de caisse d'échantillons, container d'oxygène ou bloc de service. Un bandeau frontal et un sceau géométrique renforcent sa lisibilité.

Proprietes supportees :

- nom entre guillemets obligatoire
- `pos(x,y,z)` obligatoire
- `size(x,y,z)` obligatoire
- `gray(value)` obligatoire
- `detail(value)` optionnel

Exemple :

```text
prefab_crate "service_console" pos(0.8,0.65,-6.9) size(1.3,1.3,0.8) gray(0.19) detail(0.31)
```

### Prefabs actifs de la carrière d'Eryx

Les sept directives suivantes partagent les propriétés obligatoires `pos(x,y,z)`, `size(x,y,z)` et `gray(value)`. `detail(value)` est optionnel sur les objets industriels. Les deux objets cristallins acceptent aussi `glow(value)`, qui règle une émission blanche contrainte.

Le matériau `glass` des prismes cristallins est verrouillé dans le moteur, et non exposé comme une directive libre. Il utilise un indice de réfraction central de `1.52`, la réfraction de Snell, une réflexion de Fresnel exacte et la réflexion totale interne. Une dispersion RGB volontairement approximative sépare les IOR rouge, vert et bleu avec un écart nominal de `0.035` et un léger jitter par échantillon. Les trois canaux sont stratifiés puis recomposés au fil des samples ; aucun lancer spectral complet n'est effectué.

La matière intérieure applique aussi un filtre de Beer–Lambert dépendant de la distance parcourue, avec des coefficients RGB verrouillés à `(0.45,0.22,0.08)`. Les grandes épaisseurs absorbent donc davantage le rouge et le vert que le bleu. Un cutoff de transport à `0.02` élimine les chemins devenus négligeables. `gray(value)` module la transmission aux interfaces ; il ne donne pas accès à une couleur ou à un BSDF arbitraire. Le renderer autorise par défaut au plus neuf événements diélectriques par chemin, indépendamment des rebonds diffus.

#### `prefab_survey_beacon`

Balise de navigation à mât fourchu, barre de relèvement et lampe unique.

```text
prefab_survey_beacon "route_datum" pos(-4.8,1.7,6.4) size(1.1,3.4,1.0) gray(0.24) detail(0.43)
```

#### `prefab_crystal_scanner`

Instrument d'affinité brutaliste encadrant un petit échantillon en verre cristallin.

```text
prefab_crystal_scanner "affinity_scanner" pos(-2.8,1.2,1.4) size(2.2,2.4,1.5) gray(0.25) detail(0.44) glow(0.28)
```

#### `prefab_crystal_cluster`

Veine affleurante formée de cinq prismes hexagonaux pointus en verre diélectrique et d'un lit fracturé.

```text
prefab_crystal_cluster "exposed_vein" pos(3.4,1.15,4.2) size(1.8,2.3,1.6) gray(0.66) glow(0.34)
```

#### `prefab_extraction_rig`

Portique de forage compact avec jambes inclinées, tête suspendue, colonne et pointe de coupe.

```text
prefab_extraction_rig "diamond_drill" pos(0.0,1.6,5.0) size(2.8,3.2,2.2) gray(0.23) detail(0.42)
```

#### `prefab_prospect_shelter`

Abri modulaire à entrée en retrait, masses décalées et toiture en porte-à-faux.

```text
prefab_prospect_shelter "field_shelter" pos(0.0,1.55,8.0) size(4.8,3.1,3.5) gray(0.28) detail(0.40)
```

#### `prefab_quarry_pylon`

Pylône de cote massif à couronne fendue, distinct d'une balise légère.

```text
prefab_quarry_pylon "quarry_datum" pos(4.8,1.75,6.8) size(1.4,3.5,1.2) gray(0.27) detail(0.44)
```

#### `prefab_atmospheric_processor`

Unité de service d'air et d'oxygène avec double cheminée et prises frontales.

```text
prefab_atmospheric_processor "oxygen_service" pos(-4.4,1.5,3.2) size(2.2,3.0,1.7) gray(0.24) detail(0.40)
```

### `prefab_cooling_unit` — legacy

Prefab historique de bloc de climatisation, conservé pour les scènes datacenter. Utiliser `prefab_atmospheric_processor` pour les nouvelles scènes d'Eryx.

Proprietes supportees :

- nom entre guillemets obligatoire
- `pos(x,y,z)` obligatoire
- `size(x,y,z)` obligatoire
- `gray(value)` obligatoire
- `detail(value)` optionnel

Exemple :

```text
prefab_cooling_unit "cooling_block_left" pos(-4.6,1.35,2.4) size(1.2,2.7,3.1) gray(0.25) detail(0.37)
```

### `prefab_ai_server` — legacy

Prefab de serveur IA ou de mainframe d'inference axis-aligne.

Le renderer l'expanse en cinq masses simples : un noyau central et quatre colonnes peripheriques, avec trois LEDs rouges verticales visibles dans l'entrecolonnement frontal.

Proprietes supportees :

- nom entre guillemets obligatoire
- `pos(x,y,z)` obligatoire
- `size(x,y,z)` obligatoire
- `gray(value)` obligatoire
- `detail(value)` optionnel

Exemple :

```text
prefab_ai_server "inference_mainframe" pos(4.1,1.40,-2.2) size(1.7,2.8,1.7) gray(0.16) detail(0.28)
```

### `plane`

Primitive plane finie, rendue comme un quad triangule.

Proprietes supportees :

- nom entre guillemets obligatoire
- `pos(x,y,z)` obligatoire
- `normal(x,y,z)` obligatoire
- `size(width,depth)` obligatoire
- `gray(value)` obligatoire
- `emit(value)` optionnel

Exemple :

```text
plane "floor" pos(0.0,0.0,0.0) normal(0.0,1.0,0.0) size(8.4,18.0) gray(0.14)
plane "light_panel" pos(0.2,2.79,-0.6) normal(0.0,-1.0,0.0) size(1.1,6.4) gray(0.0) emit(10.5)
```

### `box`

Primitive boite, rendue comme 12 triangles.

Proprietes supportees :

- nom entre guillemets obligatoire
- `pos(x,y,z)` obligatoire
- `size(x,y,z)` obligatoire
- `gray(value)` obligatoire
- `rot(x,y,z)` optionnel, en degres Euler
- `emit(value)` optionnel

Exemple :

```text
box "pillar" pos(2.6,1.4,2.2) size(0.55,2.8,0.55) gray(0.43)
box "tilted_panel" pos(1.45,1.35,-1.9) size(0.12,2.2,1.1) rot(0.0,18.0,0.0) gray(0.46)
```

## Commentaires

Les commentaires commencent par `#` et vont jusqu'a la fin de la ligne.

## Palette semantique actuelle

Le format reste volontairement simple : `gray(value)` est toujours obligatoire pour les primitives et prefabs, et continue de piloter la luminance de base.

La couleur finale est appliquée par le moteur selon une palette verrouillée :

- `sky` produit un ciel vénusien vert vers jaune clair
- les surfaces nommees `ground`, `desert_*`, `ridge_*`, `outcrop_*` sont teintees en ocre
- les cristaux et lampes des nouveaux prefabs émettent une lumière blanche dont l'intensité reste bornée par leurs directives
- `prefab_rack` ajoute des LEDs rouges discrètes dans les scènes legacy
- `prefab_ai_server` ajoute aussi trois LEDs rouges verticales
- tout le reste reste en niveaux de gris

Le LLM ne doit donc pas inventer de `color()` libre. La stabilite passe par le nommage et par `gray()`, pas par une palette ouverte dans le langage de scene.

## Validation actuelle

Le parseur valide actuellement :

- la presence des proprietes obligatoires
- les tailles strictement positives
- les normales non nulles pour les planes
- l'extension du fichier charge (`.scene` ou `.obj`)
- l'existence d'au moins une geometrie rendable

## Limitations actuelles

Le format implemente ne supporte pas encore :

- `sphere`
- `cylinder`
- `cone`
- `mesh reference`
- materiaux plus riches que `gray` et `emit`
- rotation explicite des prefabs
- prefabs parametriques plus riches que la petite bibliotheque actuelle
- simplification automatique d'une scene invalide
- schema contraint pour un futur LLM

## Limites importantes pour la validation spatiale

Pour la premiere batterie de tests exterieurs autour du datacenter et du desert, il faut garder a l'esprit que :

- un exterieur vaste devra etre suggere avec tres peu de masses
- le desert ne peut pas encore exister comme type de surface dedie
- le ciel de fin de journee passe maintenant par un fond proceduriel optionnel, pas par une primitive riche de scene
- la lisibilite devra venir surtout du cadrage, du parapet, du seuil, des grandes surfaces et de la ligne d'horizon
- les prefabs actuels sont volontairement simples et axis-alignes
- chaque prefab s'expanse aujourd'hui en plusieurs `box`, ce qui augmente vite le nombre de triangles

## Exemple complet

Voir [../assets/scenes/liminal_service_corridor.scene](/C:/works/projects/game-liminal-raytraced-llm-world/assets/scenes/liminal_service_corridor.scene:1).

Pour la prochaine phase de travail, voir aussi [SPATIAL_VALIDATION_PLAN.md](./SPATIAL_VALIDATION_PLAN.md).
