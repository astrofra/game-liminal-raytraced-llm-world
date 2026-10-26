# Progression du duo — 26 octobre 2026

Codex coordonne les interfaces, le runtime, la validation et l'intégration.
DeepSeek intervient via le kit externe ; le runtime du jeu reste indépendant de ce kit.

| Lot | État | Preuves et suite |
|---|---|---|
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
