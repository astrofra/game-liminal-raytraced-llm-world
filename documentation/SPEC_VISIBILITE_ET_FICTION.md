# Visibilité, réparations locales et fiction — spécification

25 octobre 2026. Spécification d'implémentation du [document historique archivé](HISTORIQUE_RECHERCHE.md), ajustée aux décisions du joueur. Ce document décrit la cible ; les résultats observés sont consignés dans le [document historique archivé](HISTORIQUE_RECHERCHE.md).

## 1. Contrat de visibilité

Une passe native de **256 × 128 rayons**, au centre des pixels, produit une profondeur en mètres et un identifiant d'objet sur **16 bits**. Identifiant 0 : fond sans impact opaque ; profondeur infinie. Les identifiants désignent les directives géométriques, pas les matériaux : toutes les pièces d'un prefab appartiennent au même objet. La table ID/nom accompagne les bitmaps. Aucun éclairage, rebond ni appel LLM. La caméra, le rapport 2:1 et le champ vertical sont ceux de la scène acceptée.

La grille reste fixe : **aucun raffinement, suréchantillonnage ou test de visibilité plus fin**. Elle détecte les occultations significatives, pas chaque petit détail. L'absence d'un petit repère sur cette grille n'est pas à elle seule un motif de rejet. Un repère contractuel doit néanmoins exister dans le programme.

Le verre est traversé par cette passe d'occultation opaque. Cela n'enlève ni son volume solide aux contrôles de caméra/collision ni son matériau au rendu physique. La passe ne simule pas la réfraction.

Une deuxième profondeur, calculée sur la même grille et limitée au repère, permet de distinguer un repère sous la résolution d'un repère réellement masqué : rejet seulement s'il occuperait au moins 8 pixels seul et que plus de 95 % de ces pixels sont cachés par des surfaces opaques plus proches. Les objets responsables sont comptés par ID. Ces seuils sont des paramètres initiaux à éprouver, pas une garantie perceptive universelle.

Le premier contrôle conserve la limite actuelle : plus de 90 % de l'image occupée par une surface opaque à moins de 3 mètres constitue une obstruction proche en mode local. Les sols et les objets désirés dans le champ ne sont pas supprimés simplement parce qu'ils sont visibles.

## 2. Géométrie et réparation locale

La caméra dans un solide, les dimensions invalides, un retour bloqué et les défauts de support restent des erreurs géométriques strictes. La passe bitmap ne remplace jamais ces vérifications. Aucune tolérance syntaxique ne corrige une coordonnée ou une dimension.

Avant une nouvelle composition LLM, une réparation native peut supprimer l'objet opaque responsable d'une occultation avérée. Première portée : boîtes du contrat local, jamais le sol, la caméra, l'éclairage ou le repère. Pas de déplacement ni de réduction improvisés. Une boîte servant de support à une autre est protégée ; une géométrie dont le rôle ne permet pas cette vérification est laissée au mécanisme de correction LLM. Maximum deux suppressions par candidat, avec un audit complet après chacune. Toute réparation non concluante est abandonnée. Aucun changement d'une scène déjà acceptée ou revisitée.

Les bitmaps et le programme avant/après sont conservés pour la recherche. Un échec de réparation locale ne transforme pas un échec géométrique en succès.

## 3. Syntaxe sans invention

Normaliser BOM UTF-8 initial, fins de ligne, une unique enveloppe Markdown complète, point-virgule terminal et virgule terminale d'une liste numérique complète. Les nombres, noms, propriétés et directives restent identiques. Le parseur accepte aussi les majuscules ASCII dans les identifiants, sans renommer les objets. La comparaison de l’en-tête fixe accepte les espaces, représentations numériques équivalentes et permutations des propriétés. Le parseur strict vérifie ensuite l'arité, les doublons, les bornes et les propriétés inconnues. Pas de suppression de ligne invalide, de récupération d'un programme noyé dans du texte, de coordonnée ajoutée ou de réparation d'une sortie tronquée. L'EOS normal reste requis avant acceptation.

Chaque transformation est nommée et archivée avec le texte brut et le texte transmis à l'audit. Une normalisation ne consomme pas une tentative LLM.

## 4. Trois compositions au plus

En mode local : composition initiale, correction guidée par diagnostic, puis recomposition avec nouvelle graine et sans recopier le candidat fautif. Maximum trois appels de composition dans le budget global existant de 120 secondes ; annulation coopérative à chaque phase. Pas de relance en cas d'annulation ou de backend indisponible. Les appels de narration sont mesurés séparément et leur échec ne remet pas en cause une géométrie déjà valide.

## 5. Autorité et fiction

Le moteur décide déplacements et effets avant la narration. Les scènes acceptées restent immuables. Une passe dédiée crée un titre, une arrivée et une courte revisite à partir des faits de la géométrie réellement acceptée, de l'audit et des sorties autorisées. Le brief n'est pas une preuve de réalisation. Les identifiants techniques ne sont pas des appellations publiques ni des objets automatiquement manipulables.

Les commandes libres d'observation, examen, écoute, attente, comparaison, rappel et orientation reçoivent une narration située. Pour une manipulation non prise en charge, le narrateur reçoit explicitement une absence d'effet : pas d'objet gagné, de paroi déplacée ou de chemin créé par le texte. Les mesures/instruments ne sont décrits comme ayant produit un résultat que si le moteur fournit ce résultat. L'état existant fait autorité.

Les descriptions ont des budgets distincts : arrivée plus ample, observation intermédiaire, revisite courte. Une panne narrative utilise un repli propre au lieu ; elle ne déclenche pas une nouvelle génération 3D. Les historiques de commandes, observations et refus restent dans la partie, avec les relations entre lieux. Aucun fichier d'une partie antérieure n'est relu au lancement.

## 6. Recherche et recette

Conserver chaque candidat, graine, texte brut, normalisations, programme final, audit, réparations, profondeur et ID ; conserver aussi les réponses narratives brutes et affichées et tout motif de repli. Les images de diagnostic sont des artefacts sans HUD ; l'archive existante conserve le rendu artistique.

Vérifier : repère mince de la partie du 25 octobre accepté ; obstacle opaque couvrant réellement le repère détecté ; caméra enfermée toujours rejetée ; verre non opaque ; ID prefab indépendant des matériaux ; suppression protégée et réaudit ; normalisations sans changement numérique ; sortie tronquée rejetée ; trois appels au plus et annulation ; deux lieux textuellement distincts ; commandes libres et revisite française ; empreinte de scène inchangée après observation/manipulation refusée ; nouveau lancement sans réutilisation d'archive.

Les campagnes comparatives de qualité littéraire et de gain statistique du troisième appel devront distinguer les tests automatisés, les sessions réellement jouées et les évaluations humaines encore à faire. Aucun gain artistique ou taux de succès n'est présumé.
