# Spatial Validation Plan

Derniere mise a jour : 2026-07-31

## Objet

Ce document formalise une premiere etape de validation du lien entre :

- la trame fictionnelle du projet
- la description textuelle d'un lieu
- la description spatiale encodee en scene v1
- le rendu 3D grayscale produit par le renderer

Le point a verifier n'est pas d'abord la qualite conversationnelle du LLM, mais la robustesse de la chaine :

```text
brief narratif
    ->
description textuelle courte
    ->
invariants spatiaux
    ->
scene v1
    ->
rendu 3D
```

## Principe

Avant de demander au LLM de generer librement des scenes, il faut d'abord construire des **fixtures canoniques** a la main.

L'objectif de cette etape est de repondre a trois questions :

1. Quels elements du monde du jeu sont effectivement representables avec le format `scene v1` actuel ?
2. Quels ecarts entre texte et image sont acceptables, fertiles ou au contraire destructeurs ?
3. Quelles extensions minimales du format sont necessaires avant l'integration narrative complete ?

## Contraintes techniques actuelles

Le format actuellement implemente dans le depot ne supporte que :

- `room`
- `camera`
- `spotlight`
- `plane`
- `box`

Le rendu actuel impose aussi plusieurs contraintes structurantes :

- grayscale uniquement
- pas de textures
- pas de ciel natif
- pas de gradient de fond natif
- pas de primitive dediee pour l'horizon, le desert ou les nuages
- un seul spot analytique attache a la camera

Consequence importante :

La phase 1 doit valider d'abord la **lisibilite spatiale** et la **coherence de traduction**, pas la richesse figurative complete du monde.

## Methode

Pour chaque lieu, on produira cinq artefacts :

1. un brief narratif court
2. une description textuelle cible de 2 a 5 phrases
3. une liste d'invariants spatiaux obligatoires
4. une scene `.scene` canonique ecrite a la main
5. un rendu PNG de reference

Chaque test devra permettre de comparer :

- ce que le texte nomme
- ce que la scene encode
- ce que l'image rend visible
- ce qui disparait entre les trois niveaux

## Regle de traduction

La scene 3D n'a pas a illustrer toute la prose.

En revanche, elle doit fixer quelques ancrages robustes :

- ou se tient le joueur
- ce qui barre ou ouvre le passage
- dans quelle direction porte le regard
- quelle masse architecturale domine la scene
- quelle relation le lieu entretient avec le dedans, le dehors, le seuil ou l'horizon

## Batterie de tests canoniques

### SV-001 - Portail d'entree du datacenter

Role fictionnel :

Lieu de seuil entre le desert et l'infrastructure. La scene doit faire sentir a la fois l'isolement, le controle et la fermeture.

Brief narratif :

Le joueur est devant l'entree principale du datacenter. Un portail ou une barriere filtre l'acces. Le batiment doit apparaitre comme une masse technique austere posee dans un environnement desertique.

Description textuelle cible :

Le portail d'entree coupe l'approche en deux. Derriere lui, la facade basse et opaque du datacenter absorbe la lumiere de fin de journee. Au-dela des murs lateraux, le desert reste visible mais vide.

Invariants spatiaux obligatoires :

- un obstacle frontal lisible : portail, grille, barriere ou sas
- une masse de facade du datacenter
- une direction d'entree claire
- une ouverture laterale ou un degagement qui suggere le desert
- une distinction nette entre dehors et dedans

Strategie scene v1 probable :

- `plane` pour le sol exterieur
- `box` pour la facade, les murs lateraux et le portail
- une ou plusieurs grandes surfaces lointaines pour suggerer le fond ou l'horizon

Critere d'acceptation :

- l'image doit etre lisible comme un exterieur de seuil, pas comme un couloir interieur
- on doit identifier sans ambiguite ce qui bloque l'entree
- la facade du datacenter doit dominer plus que le decor desertique
- le texte et l'image doivent s'accorder sur l'orientation de l'acces

Point de vigilance :

Le format v1 sait mal representer un vaste exterieur. Ce test servira a mesurer si quelques masses simples suffisent a faire exister le dehors.

### SV-002 - Interieur du datacenter avec travees de serveurs

Role fictionnel :

Lieu central de la machine. Il doit produire densite, repetition, chaleur, compression du passage et tension entre ordre technique et menace.

Brief narratif :

Le joueur circule dans une allee entre des travees de serveurs. Les baies doivent former une architecture repetitive, severe et presque abstraite. Le lieu doit etre lisible comme un datacenter, meme sans details photorealistes.

Description textuelle cible :

Les travees de serveurs se suivent comme des steles noires. L'air semble plus chaud entre les rangs, et le faisceau de la lampe glisse sur des surfaces plates, des angles durs, des couloirs presque interchangeables. Rien ne bouge, sinon la sensation que la machine ecoute.

Invariants spatiaux obligatoires :

- repetition de volumes verticaux
- au moins une allee de circulation clairement lisible
- profondeur frontale ou diagonale forte
- plafond ou masse superieure qui ecrase un peu l'espace
- densite plus grande que dans la scene de corridor actuelle

Strategie scene v1 probable :

- `plane` pour sol et plafond
- suites de `box` hautes et etroites pour les baies
- quelques `box` secondaires pour gaines, blocs de refroidissement ou armoires

Critere d'acceptation :

- l'image doit etre reconnue comme un interieur technique dense
- le joueur doit pouvoir lire une allee principale et au moins une bifurcation ou une travee adjacente
- la repetition architecturale doit rester interpretable et ne pas devenir un bruit informe
- le texte et l'image doivent converger sur l'idee de rangees / travees

Point de vigilance :

Le format actuel ne sait pas encore exprimer proprement les details fins, les cables ou les voyants. Il faut donc verifier si la typologie spatiale seule suffit.

### SV-003 - Toit du datacenter comme tour de ronde

Role fictionnel :

Lieu d'observation. Il doit articuler la reference au fort du *Desert des Tartares*, l'attente, le regard militaire et l'ouverture vers un dehors immense.

Brief narratif :

Le joueur est sur le toit du datacenter, au bord d'un parapet ou d'un chemin de ronde technique. Il regarde le desert, l'horizon et un ciel de fin de journee majoritairement sombre.

Description textuelle cible :

Le toit sert de tour de ronde au-dessus des machines. Le parapet decoupe l'horizon en une ligne dure, et le desert s'etale au-dela comme une surface presque vide. Le ciel est deja sombre, avec un reste de gradation encore perceptible vers le lointain.

Invariants spatiaux obligatoires :

- un plan de toit ou une plate-forme lisible
- un bord protege : parapet, muret ou garde-corps massif
- une ouverture vers un lointain
- une ligne d'horizon perceptible
- une domination forte du vide au-dessus et au-dela de l'architecture

Strategie scene v1 probable :

- `plane` pour le toit
- `box` pour le parapet et quelques emergences techniques
- grandes `plane` lointaines ou superposees pour suggere le desert et le ciel

Critere d'acceptation :

- l'image doit etre lisible comme un point haut d'observation exterieur
- le joueur doit percevoir le bord, puis l'au-dela
- l'horizon doit exister visuellement, meme de maniere tres pauvre
- le texte et l'image doivent converger sur l'idee de veille, d'attente et d'exposition au dehors

Point de vigilance majeur :

Le ciel sombre avec degrade est probablement le meilleur test de rupture du format actuel. Si la scene n'arrive pas a le rendre sans bricolage excessif, cela justifiera une extension v1.1 du format.

## Ordre d'execution recommande

1. garder `liminal_service_corridor.scene` comme scene de controle interieure deja existante
2. produire la fixture canonique du portail d'entree
3. produire la fixture canonique des travees de serveurs
4. produire la fixture canonique du toit / tour de ronde
5. seulement ensuite confronter ces lieux a une generation assistee par LLM

## Sorties attendues

Fichiers cibles recommandes :

- `assets/scenes/datacenter_entry_gate.scene`
- `assets/scenes/datacenter_server_aisles.scene`
- `assets/scenes/datacenter_roof_watch.scene`

Rendus cibles recommandes :

- `output/datacenter_entry_gate.png`
- `output/datacenter_server_aisles.png`
- `output/datacenter_roof_watch.png`

## Critere de sortie de cette premiere etape

Cette etape pourra etre consideree comme validee si :

- les trois lieux sont clairement distinguables sans aide textuelle
- chaque rendu preserve les ancrages spatiaux du brief
- la prose courte et l'image ne se contredisent pas sur les faits spatiaux essentiels
- les pertes entre texte et image sont documentees
- les limites du format v1 sont suffisamment identifiees pour decider d'extensions minimales

## Extensions probables a evaluer apres cette phase

Si les trois tests montrent des limites trop fortes, les extensions les plus plausibles a etudier ensuite seront :

- un fond ou ciel explicite
- un degrade vertical simple pour le ciel
- une notion de materiau ou de tag semantique plus riche
- un moyen compact de repeter des modules identiques
- un eclairage exterieur plus adapte qu'un unique spot camera
