# Spatial Validation Plan

Dernière mise à jour : 2026-08-18

## Objet

Ce document définit le protocole principal de validation du labyrinthe Eryx. Il ne suffit plus de vérifier qu'un brief produit une scène reconnaissable. Il faut tester ensemble :

- la commande du joueur
- l'interprétation du LLM
- l'état autoritatif
- la topologie mutable
- le feedback de traversal
- la description textuelle
- la scène compilée et l'image
- la carte mentale ou dessinée par le joueur

La question centrale est :

> Qu'est-ce qui doit rester stable pour qu'une contradiction spatiale soit vécue comme le comportement du labyrinthe plutôt que comme un jeu cassé ?

## Statut et périmètre

Les fixtures historiques du datacenter ont validé trois régimes visuels utiles : seuil extérieur, intérieur dense et point d'observation ouvert. Les fichiers `assets/scenes/datacenter_*.scene` et leurs mesures restent des baselines techniques, pas des lieux canoniques de la fiction active.

Le runtime possède maintenant une barrière invisible typée et une contradiction auteurisée, mais pas encore de validateur de propositions topologiques live. EV-001, EV-002 et le retour impossible d'EV-005 ont une première recette déterministe ; les tests de mutation pilotée par LLM restent un plan.

Le format rendu réellement supporté reste celui de [`SCENE_FORMAT_V1.md`](./SCENE_FORMAT_V1.md). Le test d'une barrière invisible ne demande pas l'ajout d'une directive visuelle tant que la sémantique de traversal et le feedback joueur suffisent.

Recette observée le 2026-08-18 : parcours de huit commandes depuis `quarry_threshold`, sept mouvements effectifs, contact nord refusé à `labyrinth_threshold`, barrière marquée `discovered`, détour par `prospect_shelter`, retour ouest direct vers `scanner_station`, puis sauvegarde/rechargement réussis. Les sept fixtures ont été compilées et rendues à `320x180`, `2 spp`.

## Hypothèses à tester

### H1 — Une barrière invisible peut rester actionnable

Le joueur peut comprendre qu'un mouvement a rencontré une surface et choisir une autre action, même si le viewport paraît ouvert.

### H2 — Contradiction intentionnelle et panne sont distinguables

Le feedback permet de distinguer au moins :

- commande mal interprétée
- sortie absente
- obstacle visible
- barrière invisible
- mutation topologique acceptée
- erreur technique ou fallback

### H3 — La reconnaissance locale supporte la dérive globale

Un lieu revisité peut conserver son identité grâce à quelques repères, même si son orientation, ses sorties ou ses connexions ont changé.

### H4 — La fréquence de mutation a un seuil de frustration

Des mutations rares mais significatives peuvent produire de l'incertitude. Des mutations continues risquent de rendre le marquage et le raisonnement inutiles.

### H5 — Un chemin impossible n'implique pas un état corrompu

Le joueur peut revenir à un lieu depuis une direction incompatible avec la carte sans perdre inventaire, découvertes, identité locale ou position autoritative.

### H6 — La cartographie devient une action de recherche

Les tentatives de carte peuvent révéler des motifs, enregistrer des contradictions ou échouer de manière intelligible. Elles ne doivent pas être rendues triviales par une carte omnisciente ni totalement inutiles par un changement arbitraire permanent.

## Ce qui reste dur et ce qui peut varier

### Invariants durs de tous les tests

- identité de session et numéro de tour cohérents
- position courante représentable dans le graphe autoritatif
- inventaire, échantillons et outils non perdus
- découvertes majeures persistantes
- au moins une action utile ou un mécanisme de récupération
- scène locale compilable ou fallback valide
- provenance et décision de mutation journalisées

### Variables topologiques autorisées

- adjacency d'une sortie
- statut d'une barrière invisible
- direction d'arrivée ou de retour
- longueur apparente d'un trajet
- compatibilité entre deux chemins connus
- géométrie inférée hors du champ local

### Repères locaux à préserver de préférence

Chaque lieu de test doit choisir deux ou trois repères parmi :

- forme de coque
- beacon ou pylône nommé
- scanner ou rig principal
- ouverture sur le ciel
- tranchée, rampe ou dalle caractéristique
- abri ou seuil
- objet persistant manipulable

Une mutation peut modifier leur disposition secondaire, mais ne doit pas effacer simultanément tous les signes d'identité du lieu.

## Artefacts par test

Chaque cas doit produire :

1. un état initial sérialisé ;
2. la séquence exacte de commandes ;
3. les sorties structurées brutes et réparées ;
4. les propositions et décisions topologiques ;
5. les états avant/après ;
6. les scènes compilées et leur provenance ;
7. les rendus PNG ;
8. un journal de traversal ;
9. la carte ou les annotations produites par le joueur/testeur ;
10. une fiche qualitative courte.

## Batterie de tests Eryx

### EV-001 — Seuil du champ d'extraction

Objectif :

Valider la lisibilité du nouveau monde visible avant d'introduire l'instabilité.

Situation :

Le joueur se tient devant un seuil de prospection sur un plateau ou au bord d'une carrière. Un portail, un pylône ou un abri marque l'infrastructure humaine. Le terrain ouvert et le ciel doivent rester perceptibles.

Invariants visuels :

- une direction d'entrée claire
- une masse humaine identifiable
- un repère vertical
- un espace extérieur plus vaste que la géométrie rendue
- aucune obligation de montrer le labyrinthe

Critères d'acceptation :

- le lieu n'est pas lu comme un datacenter remaquillé
- la hiérarchie seuil / infrastructure / terrain est claire
- le joueur peut identifier au moins deux actions spatiales
- la scène reste sobre et ne dépend pas de nombreux objets décoratifs

### EV-002 — Premier contact avec une barrière invisible

Objectif :

Vérifier qu'une surface invisible est compréhensible et actionnable.

Séquence minimale :

```text
observer la zone ouverte
    -> avancer
    -> contact / mouvement refusé
    -> examiner ou sonder
    -> contourner, marquer ou changer de direction
```

Conditions :

- le premier viewport ne montre pas de mur opaque
- le refus de mouvement indique un contact spatial, pas une incompréhension grammaticale
- une commande de suivi fournit au moins une information : orientation, étendue supposée, distance ou résultat instrumental
- le joueur conserve une autre action utile

Mesures qualitatives :

- première interprétation du joueur : mur, bug, limite de carte, commande invalide, autre
- nombre de commandes avant compréhension
- confiance dans le fait que la contradiction est intentionnelle
- capacité à proposer un contournement

Critère d'échec :

Le joueur répète la même commande parce qu'il pense que l'entrée n'a pas été reconnue, ou conclut que le jeu est bloqué sans autre piste.

### EV-003 — Distinguer parser, obstacle et mutation

Objectif :

Comparer trois événements avec des formulations cohérentes mais distinctes.

Cas :

1. commande réellement ambiguë, clarification demandée ;
2. sortie bloquée de manière stable par un obstacle visible ;
3. trajet connu maintenant refusé par une barrière invisible acceptée.

Critères :

- le joueur identifie correctement au moins deux catégories sur trois sans debug
- la narration ne fictionnalise pas une erreur de schéma ou un fallback
- les logs distinguent clairement les trois causes
- l'interface n'emploie pas le même signal pour clarification et collision

### EV-004 — Lieu revisité, repères locaux persistants

Objectif :

Tester la reconnaissance d'un lieu dont la connectivité a changé.

Situation :

Une station de scanner est visitée, quittée, puis retrouvée. Le scanner, un beacon et une découpe du terrain persistent ; une sortie mène désormais ailleurs ou une ancienne direction est bloquée.

Critères :

- le joueur reconnaît le lieu avant que son nom soit explicitement répété, si le protocole le permet
- deux repères au moins persistent
- l'état de l'objet manipulé au premier passage persiste
- le changement de connectivité est détectable par l'action
- la scène ne devient pas un duplicata entièrement différent sous le même identifiant

### EV-005 — Retour impossible sans corruption du hard state

Objectif :

Produire une contradiction forte mais mécaniquement sûre.

Séquence indicative :

```text
station A --north--> carrière B --east--> abri C
abri C --back/west--> station A depuis une direction incompatible
```

Contrôles automatiques :

- le joueur se trouve sur un nœud existant après chaque transition
- aucun objet n'est dupliqué ou perdu
- l'historique conserve les relations ancienne et nouvelle avec leur statut
- une sauvegarde/reprise après la contradiction reproduit l'état commis
- le compilateur rend une scène locale valide

Contrôles qualitatifs :

- le retour est perçu comme impossible, pas seulement oublié
- la contradiction arrive après suffisamment d'exposition à la carte initiale
- le texte reste concis et n'explique pas entièrement le phénomène

### EV-006 — Fréquence et budget de mutation

Objectif :

Trouver le seuil entre tension et arbitraire.

Comparer sur des sessions de longueur équivalente :

- profil A : aucune mutation
- profil B : une mutation significative
- profil C : mutation espacée avec période de stabilité
- profil D : mutations fréquentes

Mesures :

- capacité à dessiner ou raconter la route
- nombre de commandes de vérification
- sentiment d'agence
- frustration déclarée
- erreurs attribuées au parser ou au logiciel
- poursuite volontaire de l'exploration

Hypothèse de départ :

Le profil C devrait mieux soutenir l'incertitude qu'un monde fixe ou qu'un changement permanent. Cette hypothèse doit être testée, pas présentée comme un résultat.

### EV-007 — Cartographie et marques du joueur

Objectif :

Évaluer ce que la carte peut enregistrer quand l'espace change.

Variantes :

- carte dessinée librement hors du jeu
- commandes `mark`, `survey`, `compare`, `retrace`
- journal automatique limité aux lieux et traversées vécues

Questions :

- le joueur note-t-il les directions, les repères ou les échecs de traversal ?
- une contradiction efface-t-elle la valeur de la carte ou lui donne-t-elle un nouveau rôle ?
- faut-il afficher l'historique d'une relation ou seulement son état courant ?
- quelle information rendrait la carte omnisciente et détruirait l'expérience ?

Critère d'acceptation :

La carte reste utile comme archive d'observations même lorsqu'elle ne garantit pas la topologie actuelle.

### EV-008 — Ouverture apparente divisée par une sous-structure invisible

Objectif :

Tester le labyrinthe dans une carrière ou un plateau sans le transformer en couloirs visibles.

Situation :

Le viewport montre une zone ouverte avec un rig, un pylône et une rampe. Les trajectoires révèlent qu'une ou plusieurs surfaces invisibles divisent cet espace.

Critères :

- les repères visibles permettent de décrire et tester les trajectoires
- les barrières n'exigent pas l'ajout de murs opaques
- les surfaces invisibles ont une logique locale exploitable, même si le plan global dérive
- un scanner ou un effet indirect, s'il existe, reste temporaire et partiel

### EV-009 — Disparition d'une barrière connue

Objectif :

Vérifier qu'une ouverture nouvelle peut être reconnue comme mutation plutôt que comme oubli de collision.

Critères :

- la barrière a été rencontrée et marquée auparavant
- le nouveau passage est testé par le joueur ou signalé indirectement, pas seulement annoncé
- l'historique enregistre la disparition
- la traversée n'altère pas le hard state
- le monde offre une conséquence spatiale ou narrative réelle au-delà du plan libéré

### EV-010 — Session ghost-player

Objectif :

Valider une présentation autonome structurée.

Séquence recommandée :

```text
survey -> mark -> move -> retrace -> compare -> contradiction -> remap -> continue
```

Critères :

- la séquence fournit une progression lisible sans boucle de modèle auto-conversationnelle
- la topologie reste réellement générée ou négociée pendant l'exécution
- un spectateur peut comprendre le rôle respectif de la commande, du LLM, du validateur et de l'image
- la durée et le nombre maximal de mutations sont bornés

## Matrice de lecture text/image/traversal

Pour chaque événement spatial, documenter ce que chaque canal affirme :

| Événement | Texte | Image | Traversal | État/debug |
|---|---|---|---|---|
| obstacle visible | nomme ou décrit | masse visible | mouvement refusé | relation stable |
| barrière invisible | contact ou mesure | espace apparemment libre, indice optionnel | mouvement refusé | barrière typée |
| mutation acceptée | conséquence locale | scène locale éventuellement inchangée | relation nouvelle | proposition + décision |
| parser ambigu | clarification | inchangée | non tenté | aucun delta |
| fallback technique | message neutre | dernière scène valide | état inchangé | erreur explicite |

Cette matrice doit empêcher qu'une même apparence joueur recouvre des causes incompatibles.

## Indicateurs minimaux

### Quantitatifs

- taux de propositions valides / ajustées / rejetées
- nombre de mutations par session et par sortie
- commandes nécessaires après une collision invisible
- taux de scènes compilées sans fallback
- corruptions ou divergences de hard state : cible `0`
- sauvegardes/reprises réussies après mutation
- temps d'inférence, validation, compilation et rendu

### Qualitatifs

- reconnaissance des lieux revisités
- perception d'intention versus perception de bug
- valeur accordée à la carte
- sentiment d'agence
- tension, surprise et frustration
- compréhension du partage entre texte, image et mouvement

## Ordre d'exécution recommandé

1. implémenter un état de barrière invisible et un feedback de traversal sans mutation ;
2. exécuter EV-001 à EV-003 ;
3. ajouter la persistance des repères et l'historique topologique ;
4. exécuter EV-004 et EV-007 ;
5. ajouter une seule mutation typée et un validateur strict ;
6. exécuter EV-005, EV-008 et EV-009 ;
7. calibrer la fréquence avec EV-006 ;
8. préparer EV-010 uniquement après stabilisation de la tranche interactive.

## Critère de sortie

La première validation Eryx est réussie si :

- une barrière invisible est compréhensible et actionnable sans mur visible
- la majorité des testeurs distingue contradiction intentionnelle et panne
- un lieu revisité reste localement reconnaissable malgré un changement de relation
- un chemin impossible ne corrompt aucun hard state
- la cartographie conserve une utilité comme archive d'observations
- le budget de mutation produit plus de tension que de frustration
- les scènes restent localement rendables et les fallbacks sont explicitement séparés de la fiction
- tous les résultats négatifs restent documentés plutôt que requalifiés comme effets artistiques
