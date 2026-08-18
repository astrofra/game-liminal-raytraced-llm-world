# Documentation

Dernière mise à jour : 2026-08-18

Ce dossier sert de mémoire de travail artistique, scientifique et technique du projet.

La direction active est une œuvre interactive fondée sur la lignée d'Eryx : un monde d'extraction vénusien localement cohérent, traversé par un labyrinthe invisible dont le LLM peut proposer des mutations topologiques contrôlées. *Le Désert des tokens* est l'ancien titre de travail et la phase narrative immédiatement précédente.

La première tranche jouable Eryx est maintenant implémentée : sept lieux canoniques, neuf prefabs de carrière, prompts et états actifs, une barrière invisible typée et un retour non réciproque volontaire. Les anciens objets et fixtures datacenter restent disponibles comme compatibilité historique. [`TECHNICAL_STATE.md`](./TECHNICAL_STATE.md) sépare cette tranche réelle des mutations topologiques encore planifiées.

## Point d'entrée

- [`STORY.md`](./STORY.md) : direction fictionnelle active, rôle du joueur, labyrinthe invisible et lignée en quatre étapes.
- [`SPEC.md`](./SPEC.md) : spécification artistique, scientifique et technique active.
- [`ERYX_PROJECT_REORIENTATION_CODEX_BRIEF.md`](./ERYX_PROJECT_REORIENTATION_CODEX_BRIEF.md) : brief source de la migration documentaire du 2026-08-09.
- [`AMBIANCE_MOODBOARD.md`](./AMBIANCE_MOODBOARD.md) : lecture du corpus visuel comme espace incomplet mais opératoire, de la micro-informatique au labyrinthe génératif.
- [`ERYX_PLAYABLE_SPATIAL_ROADMAP.md`](./ERYX_PLAYABLE_SPATIAL_ROADMAP.md) : traduction opérationnelle des trois moodboards, carte des sept lieux et parcours dramatique en huit actions.

## Source littéraire

- [`IN_THE_WALLS_OF_ERYX.md`](./IN_THE_WALLS_OF_ERYX.md) : transcription Markdown attribuée du texte publié en 1939, avec provenance, statut de reproduction et note de contenu historique.

## Architecture et plans actifs

- [`FUNCTIONAL_PIPELINE_V1.md`](./FUNCTIONAL_PIPELINE_V1.md) : chaîne autoritative `commande -> résultat structuré -> état spatial -> compilation -> rendu` et cible de mutation topologique.
- [`SPATIAL_VALIDATION_PLAN.md`](./SPATIAL_VALIDATION_PLAN.md) : protocole Eryx pour barrières invisibles, revisites, chemins impossibles, fréquence des mutations et cartographie.
- [`HYBRID_SCENE_LAYOUT_PLAN.md`](./HYBRID_SCENE_LAYOUT_PLAN.md) : migration du compositeur hybride vers carrière, plateau, abri, scanner et seuil invisible.
- [`SCENE_FORMAT_V1.md`](./SCENE_FORMAT_V1.md) : format de scène réellement implémenté, y compris les prefabs de prospection/extraction. Les murs invisibles restent volontairement dans l'état de traversal, pas dans la géométrie visible.
- [`LLAMA_CUDA_SPECS.md`](./LLAMA_CUDA_SPECS.md) : procédure de build et de validation de `Ministral 3 8B` avec `llama.cpp` et CUDA.
- [`MULTIPROCESSING_FEASIBILITY.md`](./MULTIPROCESSING_FEASIBILITY.md) : étude et base de validation du rendu CPU parallèle, concrétisée par OpenMP.

## État, décisions et histoire

- [`TECHNICAL_STATE.md`](./TECHNICAL_STATE.md) : photographie factuelle du dépôt et frontière entre implémentation legacy et cible Eryx.
- [`KNOWN_ISSUES.md`](./KNOWN_ISSUES.md) : bugs, limites, dettes de migration et risques ouverts.
- [`DECISIONS.md`](./DECISIONS.md) : décisions techniques et conceptuelles stabilisées.
- [`PROJECT_JOURNAL.md`](./PROJECT_JOURNAL.md) : historique chronologique ; les étapes datacenter y restent volontairement intactes.
- [`NOTES.md`](./NOTES.md) : recherches et formulations, y compris la phase *Le Désert des tokens*, la généalogie Eryx et la médiation DMA WinUAE.

## Documents générés et baselines historiques

- [`PREFAB_CATALOG.md`](./PREFAB_CATALOG.md) : catalogue généré depuis les géométries C++ actives de la carrière d'Eryx, avec deux vues et une scène d'audit par objet.
- [`SCENE_GENERATION_BENCHMARK.md`](./SCENE_GENERATION_BENCHMARK.md) : benchmark historique de génération `.scene` directe sur des briefs datacenter.
- [`HYBRID_SCENE_GENERATION_BENCHMARK.md`](./HYBRID_SCENE_GENERATION_BENCHMARK.md) : benchmark historique de la chaîne hybride datacenter.
- `generated/` : prompts, réponses, états, scènes, logs et images soutenant ces résultats.

Ne pas renommer ni réinterpréter les résultats historiques comme des benchmarks Eryx. Une nouvelle série doit être générée depuis des prompts, assets et cas sources réellement migrés.

## Règle de maintenance

À chaque itération significative :

1. ajouter une entrée datée dans `PROJECT_JOURNAL.md` ;
2. mettre à jour `TECHNICAL_STATE.md` si des fonctionnalités, fichiers, commandes ou performances changent ;
3. ajouter une décision dans `DECISIONS.md` lorsqu'un choix devient structurant ;
4. mettre à jour `KNOWN_ISSUES.md` lorsqu'un problème apparaît, disparaît ou change de statut ;
5. mettre à jour les documents de conception concernés sans transformer une intention en fait ;
6. régénérer catalogues et benchmarks depuis leurs sources plutôt que d'inventer des sorties Markdown ou PNG.

## Convention d'écriture

- utiliser des dates absolues `YYYY-MM-DD` ;
- distinguer observations, décisions, hypothèses et intentions ;
- donner le contexte de toute mesure de performance ;
- conserver les problèmes non résolus visibles ;
- préserver les anciennes branches dans les notes et le journal ;
- employer l'ancien récit datacenter uniquement comme histoire, baseline ou dette d'implémentation ;
- ne pas inventer de titre final ;
- préférer l'anglais technique concis pour les nouveaux documents, sauf lorsqu'un fichier existant est volontairement maintenu en français.
