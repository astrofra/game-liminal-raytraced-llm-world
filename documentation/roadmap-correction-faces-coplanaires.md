# Roadmap — Corriger les recouvrements coplanaires avant les soutiens de verre

Projet : **Within the Latent Walls / Entre les Murs Latents**  
Date : **26 octobre 2026**  
Statut : **spécification proposée ; aucune correction ni mesure nouvelle effectuée par ce document**.

## 1. Objectif

Repérer les faces de blocs opaques orientées vers la caméra qui occupent pratiquement le même plan et se recouvrent sur une surface non nulle. Déplacer très légèrement **le bloc entier auquel appartient une face**, puis vérifier que le recouvrement coplanaire est résolu.

Cette passe déterministe intervient **avant la détection des blocs flottants et l’ajout des soutiens de verre**. Le verre doit être construit à partir des positions opaques définitives.

Le modèle continue d’improviser librement. Aucun nouveau prompt, appel LLM ou solveur physique n’est nécessaire. Il s’agit d’une correction géométrique locale, bornée et tracée.

## 2. Définir précisément le défaut

Un clash ciblé réunit trois conditions :

1. Au moins une des deux faces est suffisamment orientée vers la caméra.
2. Leurs plans sont confondus ou séparés par une distance inférieure à une petite tolérance.
3. Leurs polygones possèdent une intersection de surface positive dans ce plan.

Deux faces seulement voisines ou jointives par une arête ne constituent pas ce défaut. Deux triangles appartenant à la même face d’une même boîte ne doivent jamais le déclencher.

« Même Z » est le cas particulier de faces parallèles au plan de l’image. Pour des boîtes tournées, comparer les **plans réels en coordonnées monde** : deux faces à profondeur caméra identique peuvent appartenir à des plans différents ; deux faces coplanaires obliques peuvent avoir des sommets de profondeurs différentes.

Le défaut peut rendre ambigu le choix d’une intersection, d’une normale ou d’un matériau. Cette passe ne prétend pas traiter tous les artefacts du raytracer : auto-intersections, bruit, biais des rayons secondaires et interfaces diélectriques exigent des diagnostics distincts.

## 3. Périmètre initial

- Traiter les `box` opaques générées, y compris leurs rotations X/Y/Z.
- Garder caméra, sol de référence et éléments fixes immobiles.
- Exclure le verre préexistant et les futurs soutiens de cette première passe.
- Examiner les six faces quadrilatérales originales des boîtes, avant ou indépendamment de leur triangulation.
- Conserver pour chaque face : identifiant du bloc, indice stable de face, quatre sommets monde, normale unitaire et centre.
- Réutiliser les transformations du moteur, dans le même ordre de rotations. Ne pas reconstruire un second système de coordonnées approximatif.

Les faces hors champ peuvent être retenues par le préfiltre : l’orientation vers la caméra ne prouve pas leur visibilité. Une passe de visibilité supplémentaire pourra être envisagée si les mesures montrent des modifications inutiles, sans devenir un prérequis du pilote.

## 4. Lot COP-01 — Détection sans déplacement

### 4.1 Sélection « face caméra »

Pour une face de centre `c`, normale extérieure unitaire `n` et caméra en `eye` :

```text
v = normalize(eye - c)
facing = dot(n, v)
```

Retenir une face si `facing > facing_min`. Valeur initiale proposée : `facing_min = 0.05`, correspondant à une incidence encore frontale mais presque rasante. Le seuil est configurable ; il exprime une orientation, pas une marge de profondeur.

Au moins une face de la paire doit satisfaire ce filtre. Comparer aussi sa face partenaire si celle-ci est opposée : une normale inverse n’empêche pas deux surfaces d’être confondues. Distinguer dans le rapport les normales de même sens et de sens opposé.

**Préserver les contacts constructifs :** deux faces opposées de boîtes adjacentes peuvent simplement constituer leur interface intérieure commune. Dans ce cas, ne pas ouvrir un joint ni séparer une dalle de son pilier. Pour le pilote, diagnostiquer les normales opposées mais réserver la correction automatique aux recouvrements de faces de même sens ; une extension aux interfaces opposées réellement exposées nécessitera une qualification distincte. La liste des conflits à réduire exclut donc les joints constructifs classés hors correction.

Employer la caméra de référence du lieu et, si nécessaire, l’union des candidats des huit vues de respiration. Ne pas faire dépendre les déplacements de la frame affichée : une correction unique doit rester stable sur toute la séquence.

### 4.2 Coplanarité géométrique

Pour des faces A et B appartenant à deux blocs distincts :

1. Préfiltrer par leurs AABB élargies de la tolérance de plan.
2. Vérifier que les normales sont presque parallèles : `abs(dot(nA, nB)) >= cos(angle_tol)`.
3. Vérifier les distances signées de tous les sommets de B au plan de A, puis de A au plan de B.
4. Retenir seulement si leurs valeurs absolues restent sous `plane_tol`.

Formule de distance, avec `a0` un sommet de A :

```text
distance(p, plan_A) = dot(nA, p - a0)
```

Valeurs initiales de pilote : `angle_tol = 0.1 degré`, `plane_tol = 0.001 m` (1 mm). Les deux tests sont nécessaires : sur une très grande face, une faible différence angulaire peut produire un écart notable aux extrémités.

Ces tolérances servent à classer les faces ; elles sont distinctes du déplacement appliqué. Les vérifier à l’échelle réelle des scènes et des biais de rayons existants. Le `kEpsilon` lu dans la version précédemment examinée vaut `0.0001f`, mais ne constitue pas à lui seul un réglage universel pour cette nouvelle passe.

### 4.3 Surface commune

- Construire une base orthonormée `(u, v)` dans le plan de A.
- Projeter les deux quadrilatères dans cette base, avec une origine proche des objets pour limiter les erreurs numériques.
- Calculer leur intersection convexe en 2D, par clipping polygonal.
- Déclencher seulement si l’aire commune dépasse un petit seuil, par exemple `1e-6 m²` pour le pilote.

Le recouvrement d’AABB n’est qu’un préfiltre. Une arête ou un sommet communs ont une aire nulle et ne nécessitent pas de correction. Les cas très fins dépendent de la précision numérique et doivent avoir des fixtures dédiées.

### Livrable

Un rapport lisible de chaque paire : blocs, faces, orientation caméra, angle des normales, distances aux plans, aire commune et raison de classement. Aucun changement de scène à cette étape.

## 5. Lot COP-02 — Décaler un bloc de quelques centimètres

### 5.1 Choisir quoi déplacer

Traiter les paires dans un ordre stable, défini par les identifiants de blocs et indices de faces.

- Par défaut, conserver le premier bloc et essayer de déplacer le second.
- Si ce mouvement est impossible, essayer l’autre bloc mobile.
- Ne jamais déplacer le sol, la caméra ou une ressource fixe.
- Le bloc doit rester entier : modifier sa position, conserver dimensions, rotations, matériaux et identifiant.

Ne pas créer une règle spéciale fondée sur le nom poétique d’un volume. Un repère peut être déplacé si les audits confirment ensuite sa visibilité.

### 5.2 Direction et amplitude

Essayer d’abord une translation suivant la normale du plan problématique, dans les deux sens. Une translation tangentielle ne sépare pas les plans et peut conserver tout le défaut.

Premier ensemble de candidats proposé : **±1 cm, puis ±2 cm, puis ±4 cm**, suivant la normale. Employer la plus petite translation satisfaisant les contrôles, avec un ordre déterministe entre solutions équivalentes.

Ne pas déplacer systématiquement selon Z monde : cette direction peut être tangentielle à la face. Vérifier la séparation obtenue sur tous les sommets après transformation ; ne pas considérer le défaut résolu parce qu’une position a simplement changé.

Pour des plans quasi parallèles, le déplacement nécessaire dépend aussi de leur écart maximal initial. Si les candidats ne fournissent pas une marge suffisante au-delà de `plane_tol`, classer le cas non résolu plutôt que forcer un mouvement excessif.

### 5.3 Vérifier le candidat globalement

Après chaque translation proposée :

1. Reconstruire toutes les faces du bloc déplacé.
2. Recalculer ses paires avec tous les blocs concernés, y compris ses autres faces.
3. Vérifier la caméra hors des solides, les approches/retours réservés et la visibilité du repère.
4. Vérifier les limites spatiales et l’absence d’enfoncement significatif sous le sol.
5. Refuser tout nouveau clash coplanaire entrant dans le périmètre surveillé.
6. Accepter seulement si le nombre total de conflits ciblés diminue et si la marge de séparation du conflit traité est suffisante.

Les intersections constructives entre volumes existent déjà dans le projet : cette passe n’exige pas un monde sans intersections. Elle doit en revanche éviter d’introduire une régression mesurable dans les invariants du lieu. Préférer, entre solutions équivalentes, celle qui préserve les appuis et modifie le moins la composition.

Ne pas déplacer automatiquement toute une composante d’objets ni lancer un solveur de soutien. Réévaluer les connexions au sol après les déplacements ; la passe de verre traitera les positions finales selon son propre contrat.

### 5.4 Terminaison et rollback

Prévoir des plafonds explicites : par exemple 16 translations acceptées par scène et au plus 5 cm de longueur cumulée de déplacement par bloc, à qualifier. Conserver aussi un plafond d’essais de candidats.

Après chaque acceptation, remettre à jour la liste des conflits. La diminution stricte de leur nombre, l’ordre stable et les plafonds empêchent une oscillation infinie entre deux objets.

Un essai rejeté est annulé intégralement. S’il ne reste aucune correction admissible, conserver la meilleure scène validée et signaler les paires non résolues. Ne pas demander automatiquement au LLM de réécrire la scène ni déclarer un succès silencieux.

## 6. Lot COP-03 — Placement exact avant le verre

Ordre cible :

1. Écriture du programme opaque par le LLM.
2. Normalisation, audits et corrections existantes, dont les suppressions d’occultants.
3. **Détection et correction des recouvrements coplanaires opaques.**
4. Nouvel audit de la scène opaque corrigée.
5. Détection des blocs flottants sur ces positions corrigées.
6. Construction des soutiens de verre sous les masses définitives.
7. Vérification et rendu de la scène augmentée.
8. Extraction des faits, narration, publication et conservation.

Cette demande complète la roadmap des soutiens de verre : l’invariance des blocs opaques demandée à la passe de verre reste valable **à partir de la sortie de COP**, où les microtranslations sont déjà terminées.

Ne pas rejouer une correction opaque après construction du verre en laissant les soutiens à leur ancienne position. Si l’opaque est de nouveau modifié, invalider puis reconstruire ses soutiens avant publication.

Les interfaces entre verre et masses opaques, et entre panneaux de verre, ne sont pas traitées par cette première passe face caméra. Leur construction doit conserver les règles de jonction de la roadmap verre.

## 7. Lot COP-04 — Rendu et séquence animée

Sur quelques scènes de diagnostic :

- Produire des paires avant/après avec caméra, éclairage, graines et réglages identiques.
- Inclure un cas avec deux matériaux distincts sur les surfaces concurrentes, pour rendre l’ambiguïté observable.
- Comparer les huit points de vue de respiration, pas seulement l’image centrale.
- Vérifier que les ombres et reflets restent acceptables et que le petit déplacement ne devient pas un changement majeur de silhouette.
- Mesurer temps de détection, nombre de tests précis, candidats essayés, translations acceptées et conflits résiduels.

Une absence de pixel différent n’est pas un échec si les faces étaient masquées ; une différence d’image ne suffit pas non plus à prouver que le défaut est corrigé. Associer les images aux mesures géométriques.

## 8. Traçabilité, idempotence et persistance

Archiver au minimum :

- Programme brut du modèle et programme opaque avant la passe COP.
- Programme opaque corrigé et scène finale avec le verre.
- Version de l’algorithme, caméra de référence et paramètres de tolérance/déplacement.
- Paires détectées, translations testées et acceptées, raisons de rejet et cas non résolus.
- Position initiale, position finale et longueur cumulée déplacée pour chaque bloc modifié.
- Empreintes des différentes étapes et paramètres du rendu comparatif.

Écrire les nouvelles positions dans la représentation persistante des objets, puis reconstruire géométrie et BVH. Ne pas décaler seulement des triangles de rendu qui seraient recréés à leurs anciennes coordonnées au prochain chargement.

Les faits narratifs et le contexte de continuité des futurs lieux doivent dériver de la géométrie finale. Le programme brut reste une source de provenance, pas la scène présentée comme réellement visible.

Une revisite réutilise la scène acceptée. Elle ne rejoue pas les microtranslations. La passe doit être idempotente sur sa sortie résolue et reconnaître les états déjà traités, y compris les cas résiduels signalés, pour éviter une dérive de quelques centimètres à chaque retour.

## 9. Tests essentiels

| Fixture | Attendu |
|---|---|
| Deux faces frontales, même plan, surface commune | Clash détecté et translation minimale admissible. |
| Plans parallèles séparés au-delà de la tolérance | Aucun déplacement. |
| Même profondeur caméra, normales différentes | Pas de faux diagnostic fondé sur Z seul. |
| Deux faces obliques coplanaires avec Z variables | Détection correcte dans leur plan réel. |
| Arête commune ou sommet commun | Aucun déplacement. |
| Deux triangles d’une même face | Exclus par leur propriétaire et leur indice de face. |
| Normales opposées, surface commune | Diagnostic distinct ; joint constructif préservé, aucune correction automatique dans le pilote. |
| Faces dorsales uniquement | Ignorées dans le périmètre initial, avec cette limite documentée. |
| Blocs tournés, quasi parallèles ou très grands | Distances vérifiées sur tous les sommets ; aucune approximation AABB finale. |
| Trois blocs en conflit | Correction globale, pas de nouveau clash ni oscillation. |
| Boîtes exactement dupliquées | Plusieurs faces contrôlées ; résolution bornée ou résidu explicite. |
| Candidat bouchant le retour ou contenant la caméra | Mouvement refusé, rollback complet. |
| Sol et éléments fixes | Jamais déplacés. |
| Respiration caméra | Géométrie stable entre frames ; défaut vérifié sur la séquence. |
| Ajout ultérieur du verre | Soutiens calculés depuis les positions corrigées. |
| Revisite, sérialisation, seconde exécution | Mêmes positions et empreintes ; pas de dérive cumulative. |

## 10. Implantation proposée

| Point d’entrée | Travail |
|---|---|
| Nouveau module `coplanar_cleanup.*` | Extraction des faces, diagnostic, recherche bornée des translations et rapport. |
| `scene.*` | Réutiliser les transformations et conserver le lien face → bloc. |
| `dynamic_scene.cpp` | Orchestrer COP après les corrections opaques et avant les soutiens. |
| `scene_validation.cpp` | Réutiliser les audits caméra, passage et visibilité ; distinguer détection et validation. |
| Module des soutiens de verre | Consommer la version opaque définitive et invalider les ajouts si elle change. |
| `research_archive.*` | Conserver sources, corrections, paramètres et conflits résiduels. |
| `tests/` | Fixtures géométriques et tests d’intégration/persistance. |

Ces noms sont des propositions à raccorder à la version du dépôt au moment de l’implémentation. Cette roadmap se fonde sur les conventions précédemment examinées ; elle ne revendique pas un nouvel audit de la dernière branche distante.

## Définition de terminé pour le pilote

- Les recouvrements sont identifiés par orientation, plan réel et aire commune.
- Les blocs tournés et les faces opposées sont traités explicitement.
- Les corrections déplacent l’objet entier de quelques centimètres, avec budget et rollback.
- Les nouvelles positions passent les audits existants et ne créent pas de nouveau clash ciblé.
- La passe précède strictement la construction du verre.
- Des rendus avant/après et la séquence de respiration ont été examinés.
- Les changements sont persistants, reproductibles et archivés, sans appel LLM supplémentaire.

Ordre recommandé : **COP-01 détection → COP-02 déplacements → essai visuel COP-04 → intégration COP-03 et persistance**.
