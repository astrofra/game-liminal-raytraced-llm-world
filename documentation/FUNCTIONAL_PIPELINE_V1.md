# Functional Pipeline V1

Derniere mise a jour : 2026-07-31

## Role du document

Ce document formalise la prochaine grande inconnue du projet :

- comment l'espace du jeu est maintenu en memoire
- comment cet etat est projete vers le LLM
- comment le LLM produit a la fois du recit et des changements de monde
- comment ces changements deviennent une scene 3D rendable

Le point difficile n'est pas seulement de faire parler `Ministral`, mais de garder une chaine de transformation debuggable entre :

```text
etat de jeu
    ->
texte de tour
    ->
etat spatial
    ->
scene v1
    ->
rendu
```

## Cadrage pour la preuve de concept

Pour la phase actuelle, les priorites de performance sont :

- la latence d'inference du LLM
- le temps de rendu CPU par tour

En revanche, les points suivants restent secondaires tant qu'ils ne degradent pas visiblement la cadence de jeu :

- le temps de fabrication runtime des scenes
- l'occupation RAM de la scene
- les optimisations de structure memoire tres fines

Autrement dit : il est premature d'investir lourdement dans une architecture sophistiquee d'instanciation ou de compaction memoire si le vrai blocage se situe d'abord dans la boucle fonctionnelle.

## Hypothese directrice

La generation libre d'une scene `.scene` complete par `Ministral` a `temperature 0` est trop fragile comme voie principale.

`temperature 0` peut aider a regulariser une sortie structuree, mais ne garantit ni :

- la coherence spatiale
- la continuite inter-tour
- la stabilite d'echelle
- la validite semantique d'une geometrie complete

La voie recommandee pour le v1 est donc :

1. conserver un etat de jeu autoritatif hors du modele
2. demander au LLM un resultat structure tres contraint
3. maintenir un **etat spatial intermediaire** distinct de la geometrie finale
4. compiler cet etat spatial vers une scene `v1` de facon deterministe

## Trois niveaux d'etat

### 1. Hard state

Etat fiable, persistant, actionnable.

Il devrait contenir au minimum :

- `turn_number`
- `current_location_id`
- inventaire
- objets manipulables immediatement pertinents
- entites nommees persistantes
- alarmes, promesses ou menaces non resolues
- etat materiel du datacenter
  - alimentation
  - refroidissement
  - eau
  - niveau d'alerte
- metadonnees de session et du modele

Cet etat ne doit pas dependre de la formulation prose du dernier tour.

### 2. Soft state

Etat interpretable, compressible, partiellement instable.

Il peut contenir :

- resume recent
- atmosphere
- hypotheses en cours
- incoherences tolerees
- topologie large du monde
- explications historiques non directement actionnables

Cet etat sert surtout a nourrir la couleur narrative sans devenir la seule source de verite.

### 3. Spatial state

Etat intermediaire entre fiction et rendu.

C'est la piece manquante la plus importante pour le v1.

Il devrait decrire un lieu par elements symboliques et visuels, sans encore tomber dans les primitives finales du renderer.

Exemples de champs utiles :

- `location_id`
- `location_archetype`
- `canonical_fixture`
- `camera_mode`
- `time_of_day`
- `visibility_level`
- `desert_state`
- `interior_density`
- `alert_level`
- `lighting_mood`
- `anchors`
  - portail
  - travees
  - parapet
  - horizon
- `visible_objects`
  - racks
  - caisse
  - groupe froid
  - console
- `blocked_exits`
- `spatial_anomalies`

Le `spatial_state` ne doit pas essayer d'etre une mini-scene 3D generale. Son role est de faire le pont entre :

- ce que le LLM comprend
- ce que le moteur peut valider
- ce que le compilateur de scene sait effectivement rendre

## Chaine fonctionnelle recommandee pour le v1

```text
commande joueur
    ->
projection de contexte utile
    ->
appel LLM contraint
    ->
resultat de tour structure
    ->
validation / reparation
    ->
mise a jour du hard state
    ->
mise a jour du soft state
    ->
mise a jour du spatial state
    ->
compilateur de scene deterministe
    ->
scene v1
    ->
renderer
```

## Contrat de tour recommande

Le LLM ne devrait pas etre sollicite pour renvoyer seulement du texte libre.

Le minimum utile pour un tour v1 est plutot :

- interpretation d'intention
- narration courte
- deltas de hard state
- deltas de spatial state
- demande de clarification optionnelle
- notes de continuite optionnelles

Schema indicatif :

```text
intent
narration
clarification
hard_state_delta
spatial_delta
continuity_notes
```

Le point important est que la narration et la scene ne doivent pas etre la seule memoire du monde.

## Pourquoi ne pas demander une scene complete a chaque tour

Demander au LLM de produire directement une scene `v1` complete a chaque tour pose plusieurs problemes :

- la syntaxe peut etre correcte mais la scene spatialement absurde
- une petite derive de texte peut provoquer une grosse derive geometrique
- la continuite entre deux tours devient difficile a inspecter
- le debug est mauvais : on ne sait plus si l'erreur vient du recit, du monde, du format ou du rendu
- `temperature 0` aide la regularite de forme, pas la verite spatiale

La generation directe de scene complete peut rester :

- un mode experimental
- un outil de recherche
- un test de stress

Mais elle ne devrait pas etre la voie de production du v1.

## Role recommande de `Ministral` a temperature zero

`Ministral` a `temperature 0` est plus defendable pour :

- interpreter une commande
- choisir parmi des etiquettes, archetypes et deltas
- produire une narration courte et stable
- decider quels objets ou signes doivent etre visibles

Il est moins defendable, en premiere intention, pour :

- inventer toute la geometrie d'un lieu a chaque tour
- gerer seul la continuite spatiale globale
- placer librement des primitives nombreuses sans compilateur ni reparateur

## Strategie scene v1 recommandee

Le compilateur de scene doit rester cote moteur, pas cote modele.

Approche recommandee :

1. le `spatial_state` choisit une base de lieu
2. cette base pointe vers une fixture canonique ou un archetype stable
3. des modificateurs deterministes ajoutent ou retirent certains objets
4. le compilateur produit une scene `v1` finale

Exemples :

- `location_id = gate` + `alert_level = high` + `desert_state = dusty`
- `location_id = server_aisles` + `interior_density = dense` + `cooling_state = degraded`
- `location_id = roof_watch` + `time_of_day = dusk` + `visibility_level = low`

Autrement dit, le LLM ne pose pas chaque mur. Il selectionne et inflechit un lieu.

## Premier perimetre fonctionnel conseille

Pour la premiere boucle verticale headless, il est raisonnable de limiter le monde a trois lieux canoniques :

- `gate`
- `server_aisles`
- `roof_watch`

Avec ces contraintes :

- pas de generation libre de nouveaux lieux au debut
- pas de topologie ouverte du datacenter entier
- pas de scene complete libre par tour
- temperature tres basse pour la partie structuree
- narration courte
- listes et enums bornes

## Fallbacks a prevoir

Si la sortie de tour n'est pas exploitable, le moteur doit pouvoir :

- conserver le dernier `hard_state` stable
- conserver le dernier `spatial_state` stable
- regenerer la derniere scene valide
- produire une narration de doute, d'obscurite ou d'ambiguite plutot qu'un crash

Le but n'est pas de prouver une omnipotence du modele, mais de garder la machine jouable.

## Etapes concretes recommandees

1. Definir les structs C++ du `hard_state`, `soft_state`, `spatial_state` et `turn_result`.
2. Definir un premier schema de sortie contraint pour `Ministral`.
3. Ecrire un validateur / reparateur minimal du `turn_result`.
4. Ecrire un compilateur de scene deterministe a partir du `spatial_state`.
5. Brancher une boucle headless `commande -> tour -> scene -> rendu`.
6. Seulement ensuite ouvrir la generation assistee de nouveaux lieux au-dela des trois fixtures canoniques.

## Critere de reussite du v1

Le v1 sera deja convaincant si :

- le joueur peut se deplacer entre quelques lieux
- le texte reste court et lisible
- l'etat actionnable reste coherent sur quelques tours
- l'image change de facon interpretable
- les derives du monde restent locales et poetiques, pas purement cassantes

Ce n'est qu'apres cette validation qu'il sera rationnel de reouvrir :

- les optimisations profondes de scene / RAM
- l'instanciation sophistiquee
- la generation spatiale plus libre
- les lieux non canoniques
