# Roadmap — Soutiens de verre sous les blocs flottants

Projet : **Within the Latent Walls / Entre les Murs Latents**  
Date : **26 octobre 2026**  
Base examinée : `main`, commit `0057fc4`  
Statut : **proposition à implémenter ; aucun résultat de cette nouvelle passe n’est encore mesuré**.

## 1. Intention et périmètre

Les scènes improvisées comportent parfois des masses qui semblent flotter. Conserver ces compositions et combler leur vide inférieur avec des **parois verticales transparentes**, descendant jusqu’au sol et reprenant le contour de leur emprise vue du ciel.

Le verre fournit un appui perceptible sans imposer au LLM un nouveau raisonnement physique. Le dispositif prolonge le motif du verre et des parois transparentes associé à Eryx. Ces soutiens sont du verre visible, distinct des barrières topologiques invisibles du jeu.

### Demande à respecter

1. Détecter les blocs sans appui avec une règle géométrique simple et explicite.
2. Reprendre la petite épaisseur de la paroi du seuil : **0,25 m**.
3. Construire les parois entre le sol et la sous-face des masses concernées.
4. Faire d’abord un essai visuel sur quelques scènes existantes avant de généraliser.

La première version n’ajoute ni simulation mécanique, ni contraintes physiques au prompt, ni appel LLM. Les volumes opaques produits par le modèle restent inchangés. La passe s’exécute après leur validation et après les suppressions locales d’occultants.

## 2. Références et possibilités existantes

- [Blocs flottants dans une scène générée](https://github.com/astrofra/game-liminal-raytraced-llm-world/blob/main/img/2026-10-26_084250.png).
- [Paroi transparente du seuil](https://github.com/astrofra/game-liminal-raytraced-llm-world/blob/main/img/2026-10-26_084147.png).
- Dans `assets/scenes/eryx_quarry_threshold.scene`, la paroi existante est :

```text
glass_box "threshold_pane" pos(-0.76,2.0,-2.34) size(7.0,4.0,0.25) rot(0.0,38.0,0.0) gray(0.66)
```

`glass_box` existe déjà : volume fermé, épaisseur réelle, matériau diélectrique. Reprendre son matériau et ses paramètres optiques sans inventer une transparence alpha ni un nouveau réglage d’opacité.

Les boîtes acceptent des rotations Euler X, puis Y, puis Z. Le sol du contrat local est actuellement plat à `y = 0`. Ces conventions doivent être réutilisées directement depuis les représentations du moteur.

## 3. Définition géométrique du résultat

### Emprise vue du ciel

Pour chaque boîte, transformer ses huit sommets selon sa rotation et sa position, puis les projeter sur le plan XZ. Leur enveloppe convexe est le contour de référence.

- Boîte sans inclinaison, éventuellement tournée autour de Y : emprise rectangulaire.
- Boîte inclinée autour de X ou Z : projection pouvant former un polygone à six côtés.

**Ne pas employer l’AABB mondiale comme contour final.** Elle convient au préfiltrage, mais créerait des murs dépassant l’emprise d’une boîte tournée.

### Forme des soutiens

Les murs suivent le périmètre de cette emprise : une coque périphérique verticale, sans dalle de fond ni plafond de verre. Le vide intérieur reste vide ; il ne devient pas un gros bloc plein de verre.

- Base : sol de référence `y = 0`.
- Sommet : sous-face réelle du bloc, à la position horizontale concernée.
- Épaisseur : **0,25 m**, mesurée perpendiculairement à la paroi.
- Implantation préférée : épaisseur vers l’intérieur de l’emprise, afin de ne pas déborder artificiellement autour du bloc.

Les volumes de verre doivent être fermés, avec normales cohérentes et épaisseur non nulle, pour réutiliser correctement le rendu diélectrique.

### Boîtes inclinées

Une boîte inclinée ne possède pas une base horizontale. Remplir jusqu’à son seul `min_y` laisserait un vide sous les parties relevées ; remplir uniformément jusqu’à son centre créerait des intersections excessives.

La construction doit donc suivre l’**enveloppe inférieure réelle** de la boîte. Une méthode bornée, sans CSG générale :

1. Extraire les faces de la boîte dont la normale a une composante verticale négative ; elles définissent la sous-face vue depuis le dessous.
2. Projeter leurs triangles sur XZ. Sur chaque morceau non dégénéré, la hauteur de la sous-face est affine.
3. Intersecter les bandes périphériques d’épaisseur 0,25 m avec ces morceaux projetés.
4. Extruder chaque morceau obtenu du sol jusqu’à cette hauteur, puis trianguler en volumes fermés.
5. Écarter ou découper les morceaux dont la hauteur n’est pas strictement positive.

Les faces presque verticales ne doivent pas produire de division numérique instable. Employer des tolérances communes et une construction déterministe.

Les bandes peuvent être réunies en une coque fermée unique par bloc, ou partitionnées en éléments jointifs sans recouvrement volumique. **Éviter les panneaux superposés dans les angles**, les faces internes dupliquées et les interfaces diélectriques coïncidentes.

## 4. Détecter les blocs flottants sans solveur physique

### Règle du pilote

Un bloc opaque est considéré sans appui lorsqu’il reste séparé du sol et n’est relié à aucun bloc déjà relié au sol par une chaîne de contacts géométriques.

Cette règle mesure une connexion au sol, pas une stabilité mécanique : elle ne calcule ni gravité, ni charge, ni centre de masse. Un contact latéral peut suffire à classer un assemblage comme connecté. C’est un choix conservateur pour éviter de remplir de verre les ouvertures d’un portique déjà assemblé.

### Algorithme

1. Conserver les identifiants et les boîtes orientées des seules masses opaques originales. Exclure les futurs soutiens de cette liste de candidats.
2. Calculer le minimum vertical à partir des sommets transformés.
3. Marquer comme ancrées les boîtes touchant ou pénétrant le sol, selon une tolérance de contact.
4. Construire un graphe des contacts/intersections entre boîtes originales.
5. Propager l’ancrage depuis les boîtes au sol.
6. Les boîtes restantes sont les candidates flottantes.

Pour les contacts, employer un préfiltre AABB puis un test OBB/OBB sur les axes séparateurs, incluant les cas de contact et d’intersection avec tolérance. Ne pas conclure à un appui sur la seule intersection de deux AABB. Factoriser une représentation géométrique existante plutôt que réinterpréter le texte par expressions régulières.

Valeurs initiales proposées : tolérance de contact de l’ordre de **1 cm**, avec un seuil de vide significatif distinct, par exemple **5 cm**, pour éviter des murs dus à un bruit numérique. Ces valeurs sont à vérifier sur les fixtures ; elles ne reprennent pas automatiquement la tolérance plus large d’un autre audit.

### Assemblages suspendus

Traiter les candidats dans un ordre stable : altitude minimale puis identifiant. Après ajout d’un soutien valide sous un bloc, considérer sa composante de contact comme reliée au sol. Les blocs déjà portés par cet assemblage n’exigent plus chacun une coque redondante.

Exemple : une dalle flottante surmontée de trois autres blocs reçoit du verre sous sa partie basse ; les trois blocs qui la touchent ne reçoivent pas trois nouveaux socles traversant la dalle.

Si la première proposition de soutien d’une composante est impossible ou écartée, poursuivre les candidats suivants. Archiver les cas sans solution ; ne pas déplacer ou supprimer une masse opaque pour forcer l’ajout de verre.

## 5. Lot VERRE-01 — Détection et visualisation des candidats

Implémenter d’abord une analyse autonome des scènes acceptées, sans modifier le rendu normal.

Produire un rapport par bloc : identifiant, sommets ou emprise calculée, altitude minimale, contact au sol, voisins, composante et décision. Ajouter une vue de diagnostic colorée des candidats si utile.

### Fixtures minimales

- Boîte au sol : aucun soutien.
- Boîte isolée au-dessus du sol : candidate.
- Dalle posée sur deux piliers : aucun soutien supplémentaire.
- Pile entière flottante : composante détectée, traitement depuis le bas.
- Deux boîtes dont les AABB se recouvrent mais qui ne se touchent pas : aucune fausse connexion.
- Boîte tournée autour de Y, puis inclinée autour de X/Z.
- Vide inférieur proche du seuil de tolérance : décision stable et documentée.

**Livrable :** liste vérifiable des blocs auxquels la passe proposerait un soutien. Ne pas assimiler ce rapport à une preuve de stabilité physique.

## 6. Lot VERRE-02 — Premier essai de construction

### Étape rapide

Commencer avec une boîte horizontale, puis une boîte tournée seulement autour de Y. Construire une coque rectangulaire de quatre murs, épaisseur 0,25 m, du sol à la base. Les primitives `glass_box` existantes peuvent suffire pour ce cas, avec des jonctions sans double volume dans les angles.

### Étape nécessaire pour les scènes montrées

Ajouter ensuite les boîtes inclinées, avec l’emprise projetée et le sommet ajusté à leur sous-face. Cette étape exige probablement un petit générateur de géométrie diélectrique ; une simple `glass_box` rectangulaire ne représente pas toutes les parois à sommet oblique.

Conserver le générateur dans un module dédié, par exemple `glass_supports.*`, sans généraliser tout le langage de scène. Si une directive de sérialisation supplémentaire est nécessaire, la réserver aux soutiens produits par le moteur ; le LLM continue d’écrire son contrat opaque actuel.

### Petites masses et cas dégénérés

Si une emprise est trop étroite pour conserver une paroi de 25 cm avec un intérieur non dégénéré, ne pas réduire silencieusement l’épaisseur ni créer une coque invalide. Écarter ce candidat avec une raison explicite pour le pilote. Revoir cette politique après les premiers rendus si ces cas sont fréquents.

**Livrable :** trois paires avant/après sur la même géométrie opaque : boîte simple, boîte tournée en Y, boîte inclinée X/Z. Utiliser les scènes générées originales si elles sont disponibles dans les archives ; les captures seules ne fournissent pas leurs coordonnées exactes.

## 7. Lot VERRE-03 — Comparaison visuelle avant intégration

Rendre les mêmes scènes, avec la même caméra, les mêmes éclairages, graines et paramètres :

1. Sans soutien.
2. Avec soutiens de 0,25 m et matériau identique au verre du seuil.

Examiner d’abord les images sans HUD, puis la vue complète dans le scaphandre. Vérifier :

- Jonction au sol et contact visuel avec la sous-face, y compris en inclinaison.
- Respect du contour vu du ciel.
- Lecture des arêtes et des reflets, sans transformer le verre en paroi opaque.
- Réfraction et coloration après plusieurs interfaces ; coût du bruit et du nombre de rebonds diélectriques.
- Absence de fissures, de z-fighting et de faces dupliquées aux angles.
- Absence de dégradation majeure du repère ou du parcours visible.
- Coût de construction, triangles ajoutés et temps de rendu avant/après.

Un résultat techniquement correct peut rester visuellement trop chargé. La décision de généraliser revient à cette comparaison, sans ajouter d’emblée plusieurs styles de soutien ou de nouveaux modèles physiques.

**Jalon :** valider le principe visuel sur ces trois cas avant de l’activer sur toutes les nouvelles pièces.

## 8. Lot VERRE-04 — Intégration dans la génération et la persistance

### Ordre des passes

1. Génération de la scène opaque par le LLM.
2. Normalisation, audit et corrections existantes, dont suppressions d’occultants.
3. Détection des masses sans appui sur cette version opaque finale.
4. Construction déterministe des soutiens de verre.
5. Contrôles spécifiques de la géométrie augmentée et rendu de vérification.
6. Extraction des faits narratifs, puis publication et archivage du lieu final.

L’ajout de verre ne déclenche pas un appel LLM supplémentaire. Les corrections du générateur portent toujours sur le programme opaque original ; ne pas renvoyer les soutiens au modèle comme s’il les avait écrits.

### Contrats à préserver

- Les limites de 8 à 20 boîtes du contrat LLM concernent toujours sa sortie opaque. Comptabiliser séparément la géométrie ajoutée par le moteur, avec un plafond fini.
- Les parois de verre ne créent ni nouvelle issue ni barrière topologique invisible. Elles restent une décoration géométrique gérée par le moteur.
- Vérifier l’intérieur de la caméra et les volumes réservés aux approches/retours. Une paroi transparente reste un solide géométrique, même si l’audit d’occultation opaque l’ignore.
- Si un soutien coupe une zone réservée, écarter l’élément concerné ou le soutien entier et consigner la décision. Ne pas modifier la topologie ni imposer au LLM une nouvelle génération pour cet essai visuel.
- Si la passe de soutien échoue, conserver la scène opaque validée pour le pilote avec un statut explicite ; ne pas perdre un lieu fonctionnel à cause de ce complément.
- Préserver les contrôles opaques existants et ajouter une vérification visuelle après réfraction : la visibilité opaque seule ne garantit pas la lisibilité à travers le verre.
- Les faits envoyés au narrateur doivent identifier les soutiens effectivement ajoutés comme verre visible, sans les confondre avec le labyrinthe invisible ni en déduire des objets manipulables.

### Conservation et traçabilité

Archiver séparément le programme opaque accepté et la géométrie finale augmentée. Pour chaque ajout, conserver : bloc source, contour, paramètres, épaisseur, version de l’algorithme, pièces de verre produites et éventuelle raison d’omission.

La scène effectivement rendue possède sa propre empreinte et est réutilisée à l’identique lors des revisites. Ne pas recalculer les soutiens avec une nouvelle version de l’algorithme à chaque retour. La passe doit être idempotente : elle remplace ou reconnaît ses propres ajouts, sans empiler de nouveaux soutiens.

Si la géométrie n’est pas exprimable avec les seules directives actuelles, choisir une représentation sérialisable et déterministe avant l’intégration. Un ajout présent uniquement en mémoire et absent des archives ne satisfait pas la persistance du lieu.

## 9. Tests et critères de livraison

| Cas | Résultat attendu |
|---|---|
| Bloc au sol ou porté par un assemblage connecté | Aucun soutien ajouté. |
| Bloc isolé flottant | Parois au contour de son emprise, du sol à sa sous-face. |
| Rotation Y | Soutien tourné avec le bloc, pas avec son AABB mondiale. |
| Inclinaison X/Z | Sommet du verre raccordé à la sous-face réelle. |
| Pile flottante | Soutiens utiles sans coques traversant les blocs déjà reliés. |
| Contact à la limite de tolérance | Décision stable et reproductible. |
| Petite emprise ou contour dégénéré | Omission expliquée, aucune primitive invalide. |
| Caméra ou retour dans un soutien proposé | Élément écarté et signalé, état du jeu préservé. |
| Deux exécutions de la passe | Aucun doublon ; même résultat géométrique. |
| Revisite et lecture d’archive diagnostique | Même géométrie finale et même empreinte. |

### Définition de terminé pour le pilote

- Détection documentée des connexions au sol, sans prétention de simulation mécanique.
- Épaisseur fixée à 0,25 m et matériau optique du seuil réutilisé.
- Contour projeté correct et jonction à la sous-face, y compris pour les blocs inclinés.
- Quelques comparaisons avant/après examinées et coûts de rendu mesurés.
- Aucun changement des volumes opaques, du prompt d’improvisation ou du nombre d’appels LLM.
- Ajouts et omissions archivés, scène finale persistante, limites explicites.

## 10. Points d’entrée et ordre recommandé

| Fichier ou module | Rôle |
|---|---|
| `src/scene.*` | Réutiliser les transformations et le matériau diélectrique ; représenter les soutiens fermés. |
| Nouveau `glass_supports.*` proposé | Détection, contour projeté, construction et rapport déterministes. |
| `src/dynamic_scene.cpp` | Placer la nouvelle passe après les corrections opaques. |
| `src/scene_validation.cpp` | Réutiliser les contrôles pertinents ; distinguer contrat LLM et géométrie augmentée. |
| `src/turn_runner.cpp` | Faits narratifs, publication et conservation de la scène finale. |
| `src/research_archive.*` | Source opaque, ajouts, version, paramètres et images avant/après. |
| `tests/` | Cas de contact, rotation, jonction, idempotence et persistance. |

Ordre : **VERRE-01 → VERRE-02 → VERRE-03, revue visuelle → VERRE-04**.

L’objectif immédiat reste celui proposé : **ajouter le verre, regarder ce que cela produit et décider ensuite de la suite**.
