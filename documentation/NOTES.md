# Research Notes

_Thinking out loud. Historical formulations are retained even when a later section supersedes their positioning._

## Editorial status

The long synthesis below records the research state reached during the first technical and conceptual phase. Its general work on computational inversion, situated visual education, hard and soft state, image/text translation, and the prototype as a research instrument remains active.

Some formulations are now historical, in particular the description of the work as merely peripheral or adjacent to the demoscene. The dated section `2026-08-09 - Eryx reorientation` documents the active direction without rewriting this earlier reasoning as if it had never occurred.

> Ce rapprochement entre jeu vidéo et démoscène ne relève pas uniquement d’une parenté technique reconstruite rétrospectivement. Il procède également d’un point de vue situé et d’une formation esthétique personnelle. Sur les mêmes micro-ordinateurs, à la même époque, je découvrais les espaces tridimensionnels en faces pleines de jeux comme Driller et les mondes géométriques des démos, notamment Enigma de Phenomena. Ces productions partageaient les mêmes contraintes matérielles et des stratégies voisines de contournement esthétique : faible nombre de polygones, abstraction, discontinuité du mouvement et forte sollicitation de l’imagination. Elles appartenaient aussi à une même expérience domestique de l’informatique, faite de jeux, de copies, de crack intros, de démos et d’après-midi passés entre amis devant un Atari ST ou un Amiga. Si l’histoire culturelle a progressivement distingué ces pratiques, elles ont conjointement formé mon regard sur l’image calculée. Le prototype proposé cherche notamment à réactiver cette continuité sensible, en réunissant interaction, génération en temps réel et pauvreté graphique assumée.

## Previous research synthesis: inverting the computational load of interactive fiction

## Synthèse du cheminement artistique, scientifique et méthodologique

## 1. Point de départ

Le projet part d’une hypothèse contrefactuelle appliquée à l’histoire du jeu vidéo :

> **Et si les jeux vidéo avaient consacré leur puissance de calcul à la plasticité du récit plutôt qu’à la sophistication de l’image ?**

Depuis les années 1990 et surtout 2000, l’évolution dominante du jeu vidéo s’est largement structurée autour de l’accélération du rendu graphique : augmentation du nombre de polygones, amélioration des textures, simulation de la lumière, animation, effets visuels et recherche d’un hyperréalisme temps réel.

Par un retournement historique, les architectures graphiques développées pour cette course à l’image rendent aujourd’hui possible l’inférence locale de grands modèles de langage. Le projet propose donc de réaffecter la puissance de calcul : réduire volontairement la complexité du rendu visuel et consacrer l’essentiel de la dépense computationnelle à l’interprétation des actions, à la génération du récit et à la transformation du monde fictionnel.

Le prototype prendrait la forme d’une fiction interactive inspirée des jeux à *parser* de l’époque d’Infocom. Le joueur saisirait librement ses commandes, tandis qu’un modèle de langage local agirait comme un maître du jeu instable, capable d’interpréter les intentions, de produire des réponses narratives et de faire évoluer un espace relativement cohérent à court terme, mais globalement fragile, contradictoire ou labyrinthique.

## 2. Le projet comme contrefactualité technique

La contrefactualité ne porte pas seulement sur le contenu du récit. Elle se situe à un niveau plus profond : celui de l’histoire possible des techniques et des formes médiatiques.

Il ne s’agit pas simplement de générer des mondes alternatifs avec une IA, mais de fabriquer une branche fictive de l’histoire du jeu vidéo dans laquelle :

- le texte serait devenu le principal lieu d’innovation computationnelle ;
- l’image serait restée géométrique, pauvre, suggestive et peu coûteuse ;
- l’interaction en langage naturel aurait remplacé le perfectionnement continu des interfaces graphiques ;
- la cohérence du monde serait produite dynamiquement plutôt que fixée par une arborescence narrative préécrite.

Le prototype devient ainsi une **machine contrefactuelle exécutable**. Il ne représente pas seulement une histoire alternative : il tente d’en faire fonctionner un fragment.

## 3. Première formulation du dispositif

Le projet repose sur l’opposition volontaire entre deux régimes techniques.

### 3.1. Le langage comme centre du calcul

Le modèle de langage :

- interprète les commandes ouvertes du joueur ;
- produit une réponse narrative concise ;
- maintient un état local du monde ;
- introduit ou transforme des objets, des lieux et des personnages ;
- fournit une description structurée de la scène à rendre.

### 3.2. L’image comme matérialisation pauvre

Le rendu visuel est volontairement limité :

- géométrie constituée de primitives simples ;
- image monochrome ou en niveaux de gris ;
- éclairage frontal lié à la caméra ;
- faible résolution ;
- bruit de lancer de rayons conservé comme matière esthétique ;
- topologie incertaine et continuité spatiale partielle.

L’image n’est plus le centre spectaculaire du système. Elle devient l’empreinte, la traduction ou parfois le résidu imparfait de ce que le modèle affirme dans le texte.

## 4. Les réserves à l'aune du sujet de thèse

La confrontation au sujet de thèse soulève plusieurs difficultés légitimes.

### 4.1. Le risque de la démonstration technique

Une grande partie du public du colloque ne sera probablement pas en mesure d’évaluer les choix de programmation, l’intégration du modèle local, la structure du moteur ou la nature du raytracer. Une communication trop centrée sur l’implémentation risquerait donc de rendre l’enjeu scientifique invisible.

### 4.2. La question du champ d’expertise

Le projet semble d’abord déplacer la recherche vers :

- la littérature électronique ;
- la narratologie ;
- la fiction interactive ;
- le jeu vidéo.

Or le champ d’expertise revendiqué est principalement celui de l’image numérique, de la démoscène, des œuvres exécutables et des rapports entre technique, création et préservation.

Il serait fragile de se présenter soudainement comme spécialiste de la qualité littéraire des productions d’un LLM ou de l’histoire générale du récit interactif.

### 4.3. Le rapport à la thèse

Le projet doctoral porte sur la démoscène, son histoire, ses formes contemporaines et sa postérité à l’ère de l’intelligence artificielle. Une fiction interactive générative n’est pas automatiquement un objet démoscène.

La simple présence de jeux dans les bases de données de la scène ne suffit pas à établir un lien scientifique solide.

Ce biais devient problématique si l’œuvre est conçue uniquement pour illustrer une conclusion déjà fixée.

## 5. Recentrage sur le champ de l’image

La réponse à ces réserves consiste à ne pas revendiquer une expertise principale sur le récit, mais à reformuler la question depuis l’image :

> **Que devient l’image vidéoludique lorsque la dépense computationnelle, l’innovation technique et le prestige culturel ne se concentrent plus sur elle ?**

Le prototype ne cherche pas à démontrer qu’un LLM écrit mieux qu’un auteur. Il cherche à observer :

- ce que devient l’image lorsqu’elle cesse d’être le principal lieu du calcul ;
- comment un texte généré se transforme en espace visible ;
- quelles pertes apparaissent entre description, état du monde, géométrie et rendu ;
- comment une représentation pauvre agit sur l’imagination ;
- comment le joueur réconcilie ou non les contradictions entre texte et image.

La contribution se situe donc dans l’analyse et la création d’une **image calculée sous un régime d’infériorité volontaire**.

Le langage devient intensif et coûteux. L’image devient minimale, partielle et tardive.

## 6. Une position artistique située

Le lien entre jeu vidéo et démoscène ne repose pas uniquement sur une parenté technique reconstruite après coup. Il procède d’une formation esthétique personnelle dans laquelle ces deux cultures n’ont jamais été complètement séparées.

La découverte de jeux comme *Driller* et celle de démos comme *Enigma* de Phenomena appartiennent à une même expérience de la micro-informatique :

- même époque ;
- mêmes machines ;
- mêmes limitations matérielles ;
- même fascination pour les mondes en faces pleines ;
- même économie de formes ;
- même capacité à suggérer des espaces beaucoup plus vastes que ce que la machine pouvait réellement représenter ;
- même émotion intime devant une image calculée en temps réel.

Le jeu proposait l’exploration et l’action. La démo proposait la contemplation, la surprise et la démonstration technique. Mais les deux participaient d’une même découverte sensible de l’espace numérique.

Cette proximité n’est donc pas seulement formelle. Elle est biographique, matérielle et affective.

## 7. Une culture domestique commune

Avant que les catégories ne se stabilisent, jeux vidéo, cracking et démoscène partageaient un même environnement domestique.

La démoscène pouvait occuper quelques dizaines ou centaines d’octets placés au seuil d’un jeu, sous la forme d’un écran, d’un logo, d’un message ou d’une animation ajoutée par un groupe de crackers.

Elle occupait aussi une partie des après-midi passés chez des amis :

- on échangeait des disquettes ;
- on lançait des jeux ;
- on regardait des intros et des démos ;
- on comparait les machines ;
- on découvrait des musiques, des graphismes et des effets ;
- on passait d’une activité à l’autre sans nécessairement les considérer comme des domaines distincts.

Sur Atari ST, Amiga ou autres micro-ordinateurs, le jeu et la démoscène appartenaient au même monde matériel : mêmes écrans, mêmes lecteurs de disquettes, mêmes temps de chargement, mêmes architectures et souvent les mêmes personnes.

La distinction ultérieure entre « joueur » et « scener » est en partie une construction identitaire et historique. Elle ne résume pas toute l’expérience vécue de la micro-informatique.

## 8. L’émotion comme matériau de recherche

L’émotion personnelle n’est pas présentée comme une preuve universelle. Elle constitue un point de vue situé.

La position peut être formulée ainsi :

> Je ne prétends pas restituer l’expérience de tous les joueurs ou de tous les sceners. Je pars de la manière dont les jeux et les démos ont conjointement formé mon regard, mon rapport à l’image calculée et ma pratique artistique.

Cette position permet d’assumer que la généalogie proposée est à la fois :

- documentée historiquement ;
- vécue personnellement ;
- reconstruite artistiquement ;
- utilisée comme moteur d’une expérience contemporaine.

Le projet ne cherche donc pas à effacer la subjectivité, mais à la rendre explicite et critiquable.

## 9. La démoscène comme culture de l’allocation du calcul

Le lien le plus fort avec la démoscène ne réside pas dans une définition générique du type « la scène accepte aussi les jeux ».

Il réside dans une conception plus fondamentale :

> **La démoscène est une culture de l’allocation expressive des ressources computationnelles.**

Une production démoscène rend visibles des choix de calcul :

- que faut-il calculer ?
- que faut-il pré-calculer ?
- quelles limites faut-il accepter ?
- quelles limites faut-il contourner ?
- où placer l’effort technique ?
- comment transformer une contrainte matérielle en signature esthétique ?

Le projet reprend précisément cette logique. Il impose une contrainte conceptuelle : dépenser la puissance sur le langage et maintenir l’image dans une économie volontaire.

Il partage ainsi plusieurs traits de l’habitus démoscène :

- moteur spécifique ou minimal ;
- maîtrise directe de la chaîne technique ;
- œuvre exécutable ;
- production en temps réel ;
- limitation volontaire ;
- relation indissociable entre architecture et esthétique ;
- détournement des usages dominants de la machine ;
- importance accordée à la matérialité du calcul.

Le prototype peut être considéré comme une œuvre périphérique ou adjacente à la démoscène, sans qu’il soit nécessaire de décider immédiatement s’il constitue une « véritable démo ».

## 10. Une double contrefactualité

Le projet fait fonctionner deux hypothèses parallèles.

### 10.1. Contrefactualité de l’histoire du jeu vidéo

Et si le jeu vidéo avait investi sa puissance de calcul dans la narration, l’interprétation et la plasticité textuelle plutôt que dans l’image réaliste ?

### 10.2. Contrefactualité des frontières culturelles

Et si les liens entre jeu, cracking, fiction interactive et démoscène n’avaient pas été progressivement séparés par des catégories, des hiérarchies et des identités distinctes ?

Le prototype devient alors le descendant fictif d’une branche dans laquelle :

- les jeux à parser ;
- les mondes 3D primitifs ;
- la culture technique de la démoscène ;
- la génération procédurale ;
- et l’inférence locale des LLM

auraient continué à évoluer ensemble.

## 11. Valoriser la création au sein du colloque

Le colloque peut être compris comme le catalyseur du projet.

Le prototype prolonge des préoccupations déjà présentes :

- pratique ancienne de l’image temps réel ;
- expérience du raytracing ;
- intérêt pour les mondes géométriques pauvres ;
- travail sur les espaces extrapolés par IA dans *Le Restaurant* ;
- réflexion sur la continuité et l’incohérence des espaces générés ;
- recherche sur la démoscène ;
- interrogation sur les formes contemporaines de création avec l’IA.

L’acceptation au colloque fournit une échéance et permet de formaliser ces éléments dans une œuvre cohérente.

Pour éviter que le prototype ne soit qu’une illustration, il doit être traité comme un instrument d’enquête. Sa réalisation doit pouvoir contredire, compliquer ou déplacer l’hypothèse initiale.

Il faut donc observer ce qu’il produit réellement :

- les incohérences qui deviennent fécondes ;
- celles qui détruisent la jouabilité ;
- les écarts entre texte et image ;
- le rapport du joueur à l’attente ;
- les formes de mémoire maintenues ou perdues ;
- la manière dont le joueur interprète les contradictions ;
- le rôle de l’image pauvre dans la construction mentale du monde.

## 12. Le prototype comme appareil de recherche

La spécification du projet introduit une distinction importante entre **état dur** et **état souple**.

### 12.1. État dur

Les éléments nécessaires à l’action doivent rester relativement stables :

- lieu actuel ;
- inventaire ;
- entités nommées ;
- objets manipulables ;
- engagements ;
- menaces ;
- faits récents nécessaires à l’interaction.

### 12.2. État souple

D’autres éléments peuvent dériver :

- géographie générale ;
- continuité architecturale ;
- ambiance ;
- détails secondaires ;
- histoire du monde ;
- explications non nécessaires à l’action.

Cette distinction produit une question de recherche centrale :

> **Un monde interactif peut-il rester jouable lorsque sa cohérence opératoire est stable, mais que sa cohérence représentative dérive ?**

Le modèle ne doit donc pas seulement générer du texte. Il doit fournir une réponse structurée contenant :

- l’interprétation de l’action ;
- la narration ;
- les modifications de l’état ;
- les objets visibles ;
- les entités présentes ;
- la description de la scène ;
- éventuellement une demande de clarification.

## 13. Le passage du langage à l’image

Le système peut être décrit comme une chaîne de transformations :

```text
commande du joueur
        ↓
interprétation de l’intention
        ↓
mise à jour de l’état fictionnel
        ↓
description géométrique contrainte
        ↓
validation de la scène
        ↓
rendu raytracé
        ↓
image présentée au joueur
```

Cette chaîne est un lieu essentiel de la recherche.

Chaque passage introduit des pertes, des simplifications et des interprétations :

- le modèle reformule l’intention du joueur ;
- l’état du monde sélectionne ce qui doit persister ;
- le langage de scène réduit le récit à quelques primitives ;
- le raytracer transforme ces primitives en lumière et en ombre ;
- le joueur réinterprète l’image à partir du texte.

L’image n’illustre donc pas directement le récit. Elle résulte d’une succession de traductions et de contraintes.

## 14. Une esthétique qui ne doit pas devenir un simple habillage

Le brutaliste, le monochrome, le liminal et l’esthétique de *found footage* sont cohérents avec le projet, mais ils ne doivent pas se réduire à un style plaqué sur le système.

Ils doivent apparaître comme les conséquences du régime computationnel choisi :

- peu de primitives ;
- rendu direct ;
- faible nombre d’échantillons ;
- éclairage unique ;
- absence de textures complexes ;
- spatialité incertaine ;
- image incomplète.

Le bruit du raytracing peut devenir la manifestation visible d’un monde encore en cours de calcul.

Les faces pleines, les espaces vides et les volumes abrupts réactivent également le vocabulaire visuel des jeux et démos qui ont formé le regard de l’artiste.

## 15. Mesurer réellement l’inversion de la charge

La notion de « charge computationnelle » ne doit pas rester une métaphore.

Le prototype devrait mesurer :

- le temps d’inférence du modèle ;
- le temps de génération et de validation de la scène ;
- le temps de rendu ;
- la consommation mémoire ;
- l’utilisation du CPU ;
- l’utilisation du GPU ;
- la quantité de données produites à chaque tour.

Une contradiction serait possible si le raytracer devenait plus coûteux que le modèle de langage. Le projet doit donc conserver une image effectivement économique, ou au moins rendre visible la distribution réelle du calcul.

Une piste esthétique forte consisterait à utiliser le temps d’inférence pour raffiner progressivement l’image. Pendant que le modèle prépare sa réponse, le rendu accumulerait des échantillons. L’image deviendrait alors une matérialisation du temps consacré au récit.

## 16. Réduire l’ambition technique pour préserver l’hypothèse

La spécification complète inclut :

- application native ;
- intégration directe de `llama.cpp` ;
- fonctionnement hors ligne ;
- gestion du modèle ;
- sauvegarde et chargement ;
- interface complète ;
- langage de scène ;
- validation ;
- raytracing ;
- support multiplateforme ;
- distribution autonome.

Cet ensemble est trop vaste pour constituer le premier objectif.

Pour le colloque, un **vertical slice** suffit :

1. une session jouable de cinq à dix minutes ;
2. un modèle local ;
3. une boucle de commandes ouvertes ;
4. un état dur minimal ;
5. un langage de scène à primitives ;
6. un raytracer monochrome identifiable ;
7. une journalisation des réponses, états, erreurs et temps de calcul.

La priorité n’est pas de livrer immédiatement un produit fini, mais de produire une expérience suffisamment stable pour interroger l’hypothèse.

## 17. Questions de recherche possibles

Le projet pourrait être structuré autour des questions suivantes :

1. Que devient l’image lorsqu’elle cesse d’être le principal lieu de dépense computationnelle du jeu vidéo ?
2. Une fiction interactive peut-elle rester jouable lorsque sa cohérence locale est maintenue mais que sa géographie globale dérive ?
3. Comment les joueurs interprètent-ils les contradictions entre narration, état du monde et représentation visuelle ?
4. Une image géométrique pauvre stimule-t-elle davantage la projection mentale qu’une image détaillée ?
5. Comment la latence de l’inférence transforme-t-elle le rythme et l’attente propres à la fiction interactive ?
6. Dans quelle mesure le projet réactive-t-il une continuité historique et sensible entre jeu vidéo, cracking et démoscène ?
7. Peut-on considérer l’allocation du calcul comme un geste esthétique en soi ?
8. Le recours à un modèle de langage produit-il une ouverture narrative ou seulement une autre forme de détermination statistique ?
9. Quels éléments d’un monde doivent rester stables pour que son instabilité soit vécue comme esthétique plutôt que comme erreur ?
10. Comment une position autoethnographique peut-elle contribuer à une histoire située de l’image calculée ?

## 18. Structure possible de la communication

### 18.1. Le contrefactuel technique

Présenter l’hypothèse : que serait devenu le jeu vidéo si le calcul avait été consacré au récit plutôt qu’à l’image ?

### 18.2. La formation esthétique située

Relier *Driller*, *Enigma*, les faces pleines, l’Amiga ou l’Atari ST, les disquettes, les jeux, les crack intros et les après-midi entre amis.

### 18.3. Une histoire commune avant la séparation

Montrer que jeu, cracking et démoscène partageaient machines, réseaux, pratiques et acteurs avant la stabilisation de leurs frontières.

### 18.4. L’appareil

Décrire le modèle local, l’état dur et souple, le langage de scène, le raytracer et la boucle d’interaction.

### 18.5. L’expérience

Présenter des séquences de jeu, des contradictions, des mutations de lieux, des écarts entre texte et image et des mesures de temps de calcul.

### 18.6. Les résultats

Analyser ce que le prototype révèle de notre présent :

- naturalisation de l’hyperréalisme ;
- hiérarchie entre texte et image ;
- rôle esthétique de la contrainte ;
- fragilité de la mémoire générative ;
- valeur culturelle accordée à certains types de calcul.

## 19. Formulation synthétique du positionnement

> Ce projet ne cherche pas à démontrer qu’une intelligence artificielle peut écrire une meilleure fiction qu’un auteur. Il construit une machine contrefactuelle qui redistribue la puissance de calcul entre langage et image afin d’observer comment cette redistribution transforme la représentation, l’interaction et la cohérence d’un monde exécutable. Cette expérience procède d’une position artistique située : les jeux vidéo et les démos ont conjointement formé mon regard sur l’image calculée, dans une même culture domestique de la micro-informatique où les mondes en faces pleines de *Driller* répondaient à ceux d’*Enigma*, sur les mêmes machines, sous les mêmes contraintes et avec une même capacité à produire de l’émotion à partir de très peu.

## 20. Paragraphe réutilisable dans un article ou une communication

> Le rapprochement entre jeu vidéo et démoscène ne relève pas uniquement d’une parenté technique reconstruite rétrospectivement. Il procède également d’un point de vue situé et d’une formation esthétique personnelle. Sur les mêmes micro-ordinateurs, à la même époque, je découvrais les espaces tridimensionnels en faces pleines de jeux comme *Driller* et les mondes géométriques de démos telles qu’*Enigma* de Phenomena. Ces productions partageaient les mêmes contraintes matérielles et des stratégies voisines de contournement esthétique : faible nombre de polygones, abstraction, discontinuité du mouvement et forte sollicitation de l’imagination. Elles appartenaient aussi à une même expérience domestique de l’informatique, faite de jeux, de copies, de crack intros, de démos et d’après-midi passés entre amis devant un Atari ST ou un Amiga. Si l’histoire culturelle a progressivement distingué ces pratiques, elles ont conjointement formé mon regard sur l’image calculée. Le prototype proposé cherche à réactiver cette continuité sensible, en réunissant interaction, génération en temps réel, langage naturel et pauvreté graphique assumée.

## 21. Repères théoriques et historiques mobilisables

- Daniel Botz, *Kunst, Code und Maschine. Die Ästhetik der Computer-Demoszene*, 2011.
- Markku Reunanen, *Computer Demos—What Makes Them Tick?*, 2010.
- Markku Reunanen, « How Those Crackers Became Us Demosceners », 2014.
- Markku Reunanen, *Times of Change in the Demoscene*, 2017.
- Piotr Marecki, Yerzmyey et Robert Straka, *ZX Spectrum Demoscene*, 2020.
- Brenda Laurel, *Computers as Theatre*, 1991.
- Michael Mateas et Andrew Stern, « Writing Façade: A Case Study in Procedural Authorship », 2008.
- Michel Bret, *Images de synthèse : méthodes et algorithmes pour la réalisation d’images numériques*, 1988.
- *Driller*, Incentive Software, 1987.
- *The Sentinel*, Geoff Crammond, 1986.
- *Enigma*, Phenomena, 1991.
- *Le Restaurant*, François Gutherz, 2026.

## 22. Conclusion

Le projet devient plus cohérent dès lors qu’il n’est plus présenté comme une incursion opportuniste dans la littérature électronique, mais comme l’aboutissement provisoire de plusieurs lignes déjà présentes dans la pratique et la recherche :

- une histoire personnelle de l’image calculée ;
- une culture vécue de la micro-informatique ;
- une appartenance à la démoscène ;
- une pratique du rendu temps réel ;
- un intérêt pour les mondes génératifs et incohérents ;
- une réflexion sur l’intelligence artificielle comme opérateur de création, de mémoire et de médiation.

Le colloque ne crée pas artificiellement cette trajectoire. Il fournit le cadre dans lequel elle devient explicite, formulable et expérimentable.

La réalisation du prototype doit toutefois rester ouverte : l’œuvre ne doit pas confirmer automatiquement le projet théorique. Elle doit produire des résistances, des erreurs et des écarts capables de faire évoluer la recherche.

C’est précisément à cette condition que le dispositif pourra devenir à la fois une œuvre, un prototype technique et un véritable objet de recherche-création.

## 2026-08-09 - Eryx reorientation

### Why the narrative focus changed

The immediately preceding narrative branch, developed under the working title *Le Désert des tokens*, placed an officer in an autonomous desert datacenter waiting for a massive cyberattack that might not exist. Server heat, water consumption, environmental damage, institutional interpretation, and LLM sampling temperature formed a feedback loop.

That branch remains intellectually useful. It made computation, latency, heat, ecological cost, and self-amplifying interpretation part of the fiction. It also clarified the inversion at the center of the project: GPU resources were spent on local language inference while the CPU produced a deliberately poor raytraced image.

Its weakness for the PhD research was structural. The relationship to the demoscene depended mainly on an aesthetic of constraint and on an analogy of resource allocation. The active Eryx direction gives the work a direct historical and personal genealogy while preserving the engine architecture that the earlier phase helped define.

The datacenter, the officer waiting for an attack, cyber-threat interpretation, heat/water feedback, and the direct equation `desert temperature -> LLM temperature` are therefore superseded as the main fiction. They remain documented here and in the chronological journal as a previous conceptual stage.

### Four-stage genealogy

```text
Lovecraft and Kenneth J. Sterling, *In the Walls of Eryx*
        ↓
ASD, *Beyond the Walls of Eryx* (2007)
        ↓
Mandarine, *Within the Mesh* (2013)
        ↓
current interactive LLM work
```

The literary source provides Venusian extraction, crystals, invisible architecture, failed mapping, and uncertainty about spatial stability.

ASD transformed the motif into demoscene audiovisual language: lines, linked worlds, shifting projection, transformation, and collapse. [*Beyond the Walls of Eryx*](https://www.pouet.net/prod.php?which=31088) demonstrates that Eryx had already become a computational aesthetic within demoscene history.

Mandarine's [*Within the Mesh*](https://www.pouet.net/prod.php?which=61730), co-created by the present project's author, explicitly continued this lineage in 2013. The reference is consequently not imported into the project after the fact; it passed through the author's own practice more than a decade earlier.

The current work adds an interactive transformation. The player inhabits a local, actionable world in which a model may propose selected changes to topology. A deterministic engine validates those proposals and preserves hard state. The model's imperfect long-range spatial continuity is not simply renamed “hallucination”; it becomes a bounded capacity to renegotiate the labyrinth.

### Sharpened research proposition

The central question becomes:

> How can an AI-driven interactive work extend a demoscene lineage by turning the spatial inconsistency of generative models into an expressive, navigable constraint?

A broader PhD-facing formulation is:

> How can artificial intelligence mediate and reactivate a demoscene aesthetic founded on real-time computation, platform awareness, technical constraint, and incomplete but suggestive images?

The crucial playability question is:

> What has to remain stable for a generatively changing world to be experienced as an uncanny labyrinth rather than as a broken game?

### Hard state and soft topology

The earlier hard/soft distinction becomes more precise.

Hard state should protect inventory, collected specimens, survival resources if used, named discoveries, major commitments, and the player's authoritative current graph position.

Soft topology may include adjacency, orientation, route length, unseen barriers, corridor continuity, and inferred geometry beyond the current local view. Local visual anchors should usually persist even when connections change.

The LLM may propose a blocked exit, a vanished barrier, an impossible return path, or a reconnection to another known place. The engine decides whether the mutation is accepted. This division gives the model genuine spatial authorship without making prose the sole source of truth.

### Image and invisible architecture

The new setting aligns the renderer's primitive vocabulary with the fiction:

- boxes and slabs become extraction infrastructure
- planes become quarry floors, cuts, and plateaus
- repeated modules become prospecting or atmospheric equipment
- constrained color and grayscale become instrumental perception
- visible raytracing noise becomes incomplete sensing
- sparse scenes make unseen space active

The labyrinth must not become an ordinary visible maze. Text, traversal, and the image should remain asymmetric. The text may know that a wall occupies apparently empty space; the viewport may reveal only indirect evidence such as a scanner trace, halted dust, a shadow, or an object pressed against nothing.

### Temperature retained only as an optional mechanism

The earlier coupling among environmental, machine, and sampling temperature remains an interesting research mechanism but is no longer the narrative center. If reused, it should serve the Eryx setting through suit stress, oxygen depletion, exposure, proximity to the labyrinth, or loss of spatial confidence.

Temperature must not be the sole source of topology change. Randomness does not by itself produce a meaningful labyrinth.

### Constraint mediation and WinUAE DMA visualization

The Amiga DMA mediation remains fully relevant. Saying that older computers were simply “limited” hides the organization of their resources. The WinUAE DMA visualizer can show CPU, Copper, Blitter, bitplane DMA, sprites, audio, and other accesses occupying memory-bus time.

A useful public comparison remains:

1. a relatively idle Workbench;
2. a simple window operation;
3. a carefully selected demo sequence using several hardware subsystems.

This makes constraint visible as allocation and orchestration. The comparison with the current project must remain conceptual rather than architectural:

```text
Amiga-era demo:
hardware resources -> coordinated audiovisual output

current project:
GPU compute -> LLM inference, interpretation, and topological proposals
CPU compute -> deliberately constrained raytraced image
```

In both cases, the form of the work depends on where the machine spends its computation.

### Mediation chain

The work can now be presented as two complementary chains:

```text
literary invisible labyrinth
        -> demoscene audiovisual interpretation
        -> personal demoscene reinterpretation
        -> interactive generative labyrinth
```

```text
historical hardware allocation
        -> visible through DMA mediation

contemporary inference/render allocation
        -> visible through runtime telemetry and the resulting image
```

This creates a coherent triangle among demoscene lineage, AI-driven spatial mediation, and public understanding of computational constraint.


