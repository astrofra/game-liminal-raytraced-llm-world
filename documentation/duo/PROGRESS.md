# Progression — Duo sur Within the Latent Walls

Mise à jour : 24 octobre 2026. Coordinateur : Codex. Exécutant : DeepSeek via Copilot CLI/Ollama.

| ID | Statut | Preuve | Suite |
|---|---|---|---|
| Installation Windows | Validé | [Configuration et versions](SETUP.md) ; doctor positif ; 45 tests réussis ; 14 empreintes du kit conformes | Environnement prêt |
| DUO-001 | Bloqué lors du run, clos sans intégration | [Revue](reviews/DUO-001.md) : HTTP 401, aucune écriture | Connexion Ollama effectuée par l'utilisateur |
| DUO-001-R1 | Rejeté, clos sans intégration | [Revue](reviews/DUO-001-R1.md) : dossier parent absent, tentative hors contrat | Contrat révisé par Codex après deux tentatives |
| DUO-002 | Validé | [Revue](reviews/DUO-002.md) : un create, un view, aucun écart ; [livrable](DUO_DEMO.md) intégré | Recette terminée ; appliquer ce circuit aux prochains lots bornés |
| DUO-003 | Validé | [Revue](reviews/DUO-003.md) : téléchargement Gemma fixé et contrôlé réellement | Intégré au prototype benchmark |
| DUO-004 | Validé après correction Codex | [Revue](reviews/DUO-004.md) : contrôle caméra, 28 tests ; écart de lecture et faux positif corrigé documentés | Intégré au bilan visuel |
| DUO-005 | Validé après revue Codex | [Revue](reviews/DUO-005.md) : manifeste, révision de téléchargement fixée, 54 tests Python ; Q5 vérifié sur disque | Qualification terminée, 57 tests finaux |
| DUO-006 | Intégré après corrections Codex | [Revue](reviews/DUO-006.md) : 40 cas, langues réelles, stress et trois sessions | 611 tours exécutés, revue mécanique et narrative documentée |
| DUO-007 | Intégré après revue Codex | [Revue](reviews/DUO-007.md) : packaging relié au manifeste | Installeur 2.2.0 construit et testé avec téléchargement réel |
| DUO-008 / R1 | Revue terminée, aucun correctif généré intégré | [Revue](reviews/DUO-008.md) : accès externe refusé puis audit d'extraits ; constats vérifiés, contrat des callbacks clarifié | Limites coopératives documentées |
| Migration — benchmark direct | Prototype validé, bascule du jeu non décidée | [Guide et résultats](../SCENE_MODEL_MIGRATION_BENCHMARK.md) : matrice de 10 cas par variante ; correction du prompt Gemma 10/10 syntaxe et caméra, limites visuelles restantes ; 51 tests | Évaluation artistique et corpus Eryx avant intégration native |
| Roadmap Ministral | Terminée | [Bilan](../MINISTRAL_QUALIFICATION_REPORT.md) : 70 scènes, 611 tours, protocole, builds, installation et retour arrière ; v0.7 intégré | Q4 retenu ; Q5/Reasoning expérimentaux ; limites métier et diffusion publique documentées |

Les contrats et revues sont conservés pour rendre les échecs et les corrections vérifiables. Les worktrees et les journaux du duo restent hors du dépôt principal. Aucun commit ni push effectué pendant ces lots.

24 octobre 2026 — Exploration artistique 01 : « La chambre du ciel », image et prompt déposés dans `documentation/concept-art/`. Génération raster par Codex avec l'outil intégré image_gen, hors lots de développement Copilot/DeepSeek. PNG vérifié en 1774 × 887 (2:1), revue visuelle effectuée ; transposition en boîtes/plans envisagée, textures à simplifier, aucun rendu moteur validé. Les quatre autres concepts attendent la discussion avec l'utilisateur.

24 octobre 2026 — Exploration artistique 02–05 : après approbation du premier concept, quatre images supplémentaires générées avec image_gen et déposées dans `documentation/concept-art/`, avec prompts exacts et notes de transposition. Série complète de cinq PNG vérifiés en 1774 × 887, revue visuelle effectuée. Volumes prévus en boîtes/plans, rotations disponibles via `box rot(...)` ; détails de matière et de sol à simplifier. Aucune délégation DeepSeek ni scène moteur produite pour ce lot artistique.

24 octobre 2026 — Exploration artistique 06–10 : lecture de `IN_THE_WALLS_OF_ERYX.md`, puis cinq concepts inspirés du cristal lointain, des six issues, des plantes à mirages, des torches nocturnes et de la sortie toute proche. Images créées avec image_gen, prompts exacts et libertés d'adaptation documentés dans `documentation/concept-art/`. Dix PNG de la série décodés et vérifiés en 1774 × 887 ; revue visuelle des cinq nouveaux concepts effectuée. Géométrie envisagée en boîtes/plans et prefabs cristallins existants ; aucun mur invisible rendu comme du verre. Aucun rendu moteur ni livraison DeepSeek revendiqué. Ajouts indexés préexistants préservés.
