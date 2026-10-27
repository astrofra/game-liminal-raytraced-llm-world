# Progression du duo — 27 octobre 2026

Codex coordonne les interfaces, le runtime, la validation et l'intégration.
DeepSeek intervient via le kit externe ; le runtime du jeu reste indépendant de ce kit.

| Lot | État | Preuves et suite |
|---|---|---|
| DUO-044 / AUDIO-05–07 | Roadmap intégrée ; prochaine itération à réaliser | [Revue](reviews/DUO-044.md) : bilan des huit premiers tours, fiches/écoute, choix expressifs et recette comparative ; lecture et audit Codex, une clarification à l'intégration. |
| Démarrage audio | Correctif Codex livré et publié | [Bilan](../AUDIO_AMBIENCE.md) : mix du seuil dès le choix de langue, rampe de cinq secondes sans appel LLM ; 15 CTest passent, SDL réel sans commande vérifié, silence explicite et sauvegarde préservés. |
| DUO-042 / AUDIO-01–02 | Livraison brute rejetée ; contenu repris et corrigé par Codex | [Revue](reviews/DUO-042.md) : timeout, lectures hors contrat et poursuite après échec d'outil ; deux défauts numériques constatés par tests. Extraction corrigée et reproductible, OGG inchangés. |
| DUO-043 | Validé et intégré | [Revue](reviews/DUO-043.md) : un fichier, cinq régressions physiques ; outils/chemins conformes, 5/5 tests exécutés par Codex passent. |
| AUDIO-03–04 | Binaire publié, recette technique effectuée | [Bilan](../AUDIO_AMBIENCE.md) : décodeur léger intégré, gains dans l'appel narratif, rampes et marges, annulation et erreurs ; 15 CTest et 114 Python passent, Gemma/SDL réels vérifiés. Écoute et validation artistique des trims/jonctions encore à effectuer par l'auteur. |
| DUO-040 / DUO-041 | Correctif livré et publié | [Revue DUO-040](reviews/DUO-040.md), [revue DUO-041](reviews/DUO-041.md), [bilan](../RETOUR_JEU_INTERACTIONS_2.md) ; premier contrat retiré après clarification ; tests DeepSeek repris avec écart de lecture documenté, formulations libres interprétées par le LLM, raccourcis directs, inventaire sans nouveau rendu ; 14 CTest, recettes Gemma et nouvelle partie SDL de douze commandes sans secours narratif |
| DUO-039 | Correctif après partie livré et publié | [Contrat](tasks/DUO-039.md), [revue](reviews/DUO-039.md), [bilan](../RETOUR_JEU_INTERACTIONS.md) ; six tests DeepSeek intégrés, observations persistantes et résolution/narration Codex, 14 CTest et 64 Python passent ; nouvelle partie SDL de dix commandes sans remplacement narratif, prises libres Gemma et dépôt avancé de 20 cm vérifiés |
| DUO-037 / DUO-038 | Interactions livrées, binaire publié | [Revue](reviews/DUO-038.md), [bilan](../INTERACTIONS_ENVIRONNEMENT.md) ; tests DeepSeek repris après deux runs non conformes, moteur et intégration Codex ; 14 CTest + 64 Python passent, Gemma et SDL vérifiés, dépôt visible à travers le masque |
| DUO-035 / DUO-036 | Livré — cibles 50/50, binaire publié | [Revue initiale](reviews/DUO-035.md), [revue finale](reviews/DUO-036.md) ; tests DeepSeek corrigés et intégrés, politique logique Codex, impasses préservées, géométrie inchangée, 14 CTest passent et essai réel Gemma accepté |
| DUO-034 | Correctif Codex livré ; livraison DeepSeek rejetée | [Contrat](tasks/DUO-034.md), [revue](reviews/DUO-034.md) ; seuil N/E/S, continuation réservée sur la dernière branche, huit pièces en test adversarial, 13 CTest passent, essai réel nord/retour ; binaire publié |
| DUO-033 / COP-01 à 04 | Pilote livré, binaire publié | [Contrat](tasks/DUO-033.md), [revue](reviews/DUO-033.md), [bilan](../CORRECTION_COPLANAIRE.md) ; quinze cas DeepSeek, géométrie et intégration Codex, quatre scènes × huit vues examinées, 13 CTest et 64 Python passent ; COP précède le verre |
| DUO-032 / VERRE-01 à 04 | Pilote livré, binaire publié | [Contrat](tasks/DUO-032.md), [revue](reviews/DUO-032.md), [bilan](../SOUTIENS_VERRE.md) ; tests DeepSeek corrigés, maillage et intégration Codex, cinq paires et deux vues SDL examinées ; 11 CTest et 64 Python passent |
| DUO-031 | HUD repositionné, rendu vérifié | [Contrat](tasks/DUO-031.md), [revue](reviews/DUO-031.md) ; températures centrées à hauteur inchangée, boussole dégagée du joint droit et centres alignés ; binaire publié sans recompilation CUDA |
| DUO-030 | Audit DeepSeek non intégré ; correctif Codex livré | [Contrat](tasks/DUO-030.md), [revue](reviews/DUO-030.md) ; arrêt après échec d'outil non respecté par l'audit. Setup sans modèle de 414 Mo, téléchargement final vérifié, aucun rebuild CUDA |
| DUO-028 | Audit relu et intégré avec réserves | [Contrat](tasks/DUO-028.md), [revue](reviews/DUO-028.md) ; écarts des prompts et archives confirmés, oracles de navigation corrigés par Codex |
| DUO-029 | Tests de budget intégrés sélectivement | [Contrat](tasks/DUO-029.md), [revue](reviews/DUO-029.md) ; fixture corrigée, tests C++ exécutés par Codex |
| CONT-01 à CONT-05 | Pilote livré et recette exécutée | [Bilan](../CONTINUITE_SCENES.md) : neuf tests CTest, 61 Python, parcours Gemma A → B → A → C, tokens vérifiés, binaire publié ; réserves artistiques documentées |
| DUO-027 | Validé — documentation uniquement | [Contrat](tasks/DUO-027.md), [revue](reviews/DUO-027.md) ; matrice relue et précisée dans la [roadmap des interactions](../ROADMAP_INTERACTIONS_ENVIRONNEMENT.md), gameplay à implémenter |
| DUO-026 | Masque bitmap livré et vérifié | [Contrat](tasks/DUO-026.md), [revue](reviews/DUO-026.md) ; v3 transparente, dessin procédural supprimé |
| Runtime autonome | Livré, qualification locale effectuée | [Bilan](../BILAN_RUNTIME_AUTONOME.md) : parcours 20 commandes, tests des langues, protocole et poids résidents |
| DUO-025 | Audit DeepSeek relu, nettoyage intégré | [Contrat](tasks/DUO-025.md), [revue](reviews/DUO-025.md) ; outils HTTP orphelins et anciens profils supprimés, bancs natifs conservés |
| DUO-023 | HUD livré | [Revue](reviews/DUO-023.md) ; positions et contraste conservés dans le runtime actuel |
| DUO-022 | Langues livrées | [Revue](reviews/DUO-022.md) ; garde linguistique et traductions compilées conservés |
| DUO-021 | Inventaire livré | [Revue](reviews/DUO-021.md) ; trois outils, six langues, sans inférence |
| DUO-020 | Propositions autoritatives livrées | [Revue](reviews/DUO-020.md) ; issues et chaleur validées par le moteur |

Les étapes antérieures et leur progression détaillée sont conservées dans
l'[archive de recherche](../HISTORIQUE_RECHERCHE.md) et dans Git. Les contrats et
revues restent des traces datées, pas des instructions de lancement actuelles.
Aucun commit ni push effectué par Codex.

Précision produit du 26 octobre 2026, traitée directement par Codex car elle modifie la sémantique : la roadmap distingue désormais les cibles implicites issues de la description littéraire et les objets explicitement déposés. Une reprise retire l'entrée de la liste de la pièce ; une première prise implicite matérialise l'objet et mémorise son retrait. Lots et recette adaptés ; aucun code changé ni nouveau run DeepSeek.
