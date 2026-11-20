# Roadmap du déplacement à l’intérieur des scènes 3D

Proposition du 20 novembre 2026, à réaliser plus tard.

## Expérience visée

Le joueur voit du verre sous un surplomb et écrit « avance vers le cube en verre ».
Si le volume est identifié et accessible, il s’en approche dans la même pièce.
L’image change de point de vue et la narration décrit ce que cette approche permet
de distinguer. « Avance un peu » demande un pas dans l’axe du regard. « Explore un
peu par ici » peut laisser au moteur le choix d’un petit trajet. Une question comme
« Est-ce qu’on peut s’en approcher ? » reçoit une réponse depuis la position courante.

Le jeu dispose déjà de scènes compilées et persistantes, d’une caméra de référence
par scène, de contrôles d’approche et d’une animation de respiration. Un trajet
local demande un état de position et la validation de tout le chemin parcouru.
Les passages cardinaux continuent de s’appuyer sur les liens entre lieux. Voir les
[contrôles de visibilité](SPEC_VISIBILITE_ET_FICTION.md), la [vue animée](ANIMATED_VIEW_SPEC.md)
et les [parois repérées](PAROIS_REPEREES.md).

## 1. Prototyper un pas sur une scène existante

- Commencer au seuil de la carrière, puis essayer une pièce générée contenant un
  surplomb et un volume de verre. Charger le décor accepté et composer ses couches
  effectives pour le calcul et le rendu.
- Représenter la pose locale en mètres dans le repère de la pièce. Initialiser la
  pose à la caméra d’arrivée et garder les coordonnées du réseau de salles à leur
  échelle actuelle. Pour le pilote, réutiliser la pose d’arrivée lors d’un retour
  dans une pièce. Évaluer ensuite l’intérêt de retrouver la dernière pose locale.
- Essayer un segment court sur sol peu pentu, de l’ordre d’un à deux mètres pour
  commencer. Vérifier l’appui sous tout le trajet, la pente, le dégagement autour
  du scaphandre et au-dessus de lui, et le volume balayé jusqu’au point d’arrêt.
  Les tests actuels de la caméra et des approches servent de référence. Mesurer
  leurs limites avant de choisir les primitives de collision du pilote.
- Traiter les volumes de verre solides, les éléments opaques et les obstacles
  mobiles pertinents selon leur géométrie effective. Rester dans une zone intérieure
  bornée par les approches de sortie. Toute extension vers un seuil devra utiliser
  le contrôle autoritatif de franchissement et préserver la découverte au contact
  des parois invisibles, y compris sous un angle oblique. Le repère visuel d'une
  paroi révélée doit rester ancré dans la pièce lorsque la caméra se déplace.
- Rendre une image depuis la pose candidate, puis publier la pose et l’image
  ensemble. Une annulation ou un échec de rendu conserve la pose publiée. Établir
  le coût du calcul avant d’ajouter un plan animé de déplacement. La respiration se
  recalcule autour de la pose d’arrivée du pas, avec un dégagement vérifié.

### Recast et Detour dans le pilote

- Construire avec Recast un maillage de navigation à partir des triangles de
  collision statiques du décor accepté, verre solide compris. Définir le rayon
  et la hauteur du scaphandre, la pente et la marche maximales. Calculer un seul
  maillage par pièce à son premier usage, puis le réutiliser tant que la géométrie
  source et ces paramètres correspondent. [Configuration Recast](https://recastnav.com/structrcConfig.html)
- Quand Gemma désigne un repère visible, le moteur cherche une position d’arrêt
  sur le sol praticable, à distance de la surface. Detour calcule le chemin vers
  cette position et peut contourner un obstacle. Si la requête renvoie un chemin
  partiel, le scénario reste une approche incomplète. Tout pas effectué et le
  point d’arrêt réel sont transmis à la narration. [Requêtes Detour](https://recastnav.com/classdtNavMeshQuery.html)
- Le moteur vérifie le trajet contre la géométrie et les obstacles mobiles dans
  leur état courant, puis applique les règles de franchissement et de découverte
  au contact. Le maillage reste une approximation du sol praticable. Une paroi
  invisible et son repère graphique relèvent de l’état du jeu. [Guide Recast](https://github.com/recastnavigation/recastnavigation/blob/main/Docs/_1_Introduction.md)

## 2. Relier les mots aux éléments visibles

- Construire un petit ensemble de repères depuis la vue courante et le décor
  accepté. Pour chaque repère, donner au modèle un identifiant temporaire, une
  description courte, le matériau connu et sa position relative dans l’image.
  Vérifier séparément la visibilité des surfaces transparentes. La passe
  d’occultation opaque seule ne suffit pas pour identifier ce que le joueur voit
  dans le verre.
- Laisser le LLM choisir l’intention et la cible par identifiant. Le moteur calcule
  la trajectoire, la distance d’arrêt à la surface et la pose issue du trajet.
  À cible incertaine, poser une question qui distingue les repères possibles.
  À trajet barré, indiquer l’obstacle observable en restant sur place si aucun pas
  utile et valide n’est possible. Les relances utilisent la proposition en cours
  et la revalident depuis la pose courante avant exécution.
- Transmettre au maître du jeu le résultat validé, le changement de point de vue et
  les faits désormais observables. Il peut librement décrire le changement de
  perspective, les reflets et les sensations dans le scaphandre. Une observation
  et une prise d’objet restent des intentions distinctes d’une approche.

## 3. Tester le choix d’un pas par le moteur

- Pour une exploration sans cible, construire quelques segments courts dans un
  secteur cohérent avec le regard ou la consigne. Valider les segments, puis les
  classer selon le dégagement et le changement de point de vue. Tester un tirage
  pondéré parmi les options utiles, avec une graine rejouable. Si un seul trajet
  convient, le prendre. Si aucun ne convient, expliquer ce qui bloque. Avec Detour,
  les candidats peuvent aussi venir des zones accessibles autour du joueur. La
  distance est bornée sur le trajet parcouru.
- Garder « avance » prévisible avec un pas dans l’axe. Réserver la variation aux
  demandes qui délèguent vraiment le choix, comme « explore un peu ». Un repère
  nommé reste la cible de l’approche. Comparer les mêmes commandes avec le pas
  déterministe et le tirage pondéré avant de retenir ce dernier.

## 4. Recette avant activation dans le jeu

- Tester la dalle du seuil, deux repères difficiles à distinguer, une approche
  bouchée, un sol sans appui, un surplomb bas et un passage latéral. Vérifier les
  vues, les collisions du trajet et le champ de vision après déplacement. Rejouer
  la même graine à état identique. Au seuil de sortie, vérifier que le pilote ne
  permet aucun passage direct. Quand l'approche des sorties devient jouable,
  tester les contacts de face et en biais avec les parois invisibles.
- Comparer le prototype de pas direct et Recast + Detour dans la même pièce et
  depuis la même pose. Partir de la pièce du cube en verre. Vérifier qu’un trajet peut contourner
  un obstacle en gardant le cube comme cible, qu’un cube inaccessible est signalé
  comme tel et qu’un objet déplacé peut bloquer un trajet déjà calculé. Tester aussi
  un passage trop bas ou trop étroit pour le scaphandre. Mesurer le coût de
  construction, la mémoire par pièce, la latence des requêtes et la longueur des
  trajets proposés. Retenir Recast si le contournement apporte une approche utile
  et que ces coûts restent compatibles avec le délai entre commande et première image.
- Enchaîner question, approche ciblée, « vas-y », annulation, commande cardinale et
  retour. Vérifier que la pose locale et la vue évoluent ensemble, que le fichier
  de décor accepté garde son empreinte et que les liens, les objets et les effets
  de simulation suivent les événements validés. Comparer le rythme avec la partie
  actuelle avant de décider comment un pas local compte dans la simulation.
- Archiver commande, appels LLM, graine, poses, repères, candidats, motifs de rejet,
  trajet accepté, référence du décor, ambiance et capture SDL finale. Mesurer le
  délai entre commande et première image. Évaluer sur Gemma et SDL si le mouvement
  apporte un repère utile et si les réponses restent naturelles en cas de blocage.

Le pilote doit permettre de décider de l’amplitude du pas et de l’intérêt du
maillage de navigation. Le choix aléatoire et l’animation du trajet s’évaluent une
fois ce déplacement court jouable.
