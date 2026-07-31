# Scene Format V1

Derniere mise a jour : 2026-07-31

## Statut

Ce document decrit le format de scene proprietaire v1 actuellement implemente dans le depot.

Le format cible plus large de la spec n'est pas encore entierement supporte. L'implementation actuelle couvre un sous-ensemble volontairement petit, suffisant pour amorcer le pipeline scene -> validation -> rendu.

## Intentions

Le format v1 doit rester :

- lisible par un humain
- facile a produire ou corriger
- compact
- defensif a parser
- plus stable qu'un `.OBJ` genere brut

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

Active un fond proceduriel grayscale pour les rayons qui ne rencontrent aucune geometrie.

Proprietes supportees :

- `zenith(value)` obligatoire
- `horizon(value)` obligatoire
- `nadir(value)` obligatoire
- `band(value)` obligatoire
- `curve(value)` obligatoire
- `noise(value)` obligatoire
- `stars(density,intensity,radius)` optionnel
- `seed(value)` optionnel

Le ciel est pense pour les exterieurs pauvres du projet : zenith sombre, horizon plus clair, nadir sombre, grain fort et etoiles deterministes possibles.

Exemple :

```text
sky zenith(0.01) horizon(0.24) nadir(0.00) band(0.32) curve(1.95) noise(0.12) stars(0.0036,1.55,0.100) seed(77)
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
- simplification automatique d'une scene invalide
- schema contraint pour un futur LLM

## Limites importantes pour la validation spatiale

Pour la premiere batterie de tests exterieurs autour du datacenter et du desert, il faut garder a l'esprit que :

- un exterieur vaste devra etre suggere avec tres peu de masses
- le desert ne peut pas encore exister comme type de surface dedie
- le ciel de fin de journee passe maintenant par un fond proceduriel optionnel, pas par une primitive riche de scene
- la lisibilite devra venir surtout du cadrage, du parapet, du seuil, des grandes surfaces et de la ligne d'horizon

## Exemple complet

Voir [../assets/scenes/liminal_service_corridor.scene](/C:/works/projects/game-liminal-raytraced-llm-world/assets/scenes/liminal_service_corridor.scene:1).

Pour la prochaine phase de travail, voir aussi [SPATIAL_VALIDATION_PLAN.md](./SPATIAL_VALIDATION_PLAN.md).
