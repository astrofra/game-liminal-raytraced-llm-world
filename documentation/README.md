# Within the Latent Walls / Entre les Murs Latents — Documentation

Dernière mise à jour : 2026-10-24

Ce dossier sert de mémoire de travail artistique, scientifique et technique du projet.

*Within the Latent Walls* / *Entre les Murs Latents* est une œuvre interactive fondée sur la lignée d'Eryx : un monde d'extraction vénusien localement cohérent, traversé par un labyrinthe invisible dont le LLM peut proposer des mutations topologiques contrôlées.

La première tranche jouable Eryx est maintenant implémentée : sept lieux canoniques, neuf prefabs de carrière, prompts et états actifs, une barrière invisible typée et un retour non réciproque volontaire. Les anciens objets et fixtures restent disponibles comme compatibilité technique. [`TECHNICAL_STATE.md`](./TECHNICAL_STATE.md) sépare cette tranche réelle des mutations topologiques encore planifiées.

## Point d'entrée

- [`STORY.md`](./STORY.md) : direction fictionnelle active, rôle du joueur, labyrinthe invisible et lignée en quatre étapes.
- [`SPEC.md`](./SPEC.md) : spécification artistique, scientifique et technique active.
- [`ERYX_PROJECT_REORIENTATION_CODEX_BRIEF.md`](./ERYX_PROJECT_REORIENTATION_CODEX_BRIEF.md) : brief source de la migration documentaire du 2026-08-09.
- [`AMBIANCE_MOODBOARD.md`](./AMBIANCE_MOODBOARD.md) : lecture du corpus visuel comme espace incomplet mais opératoire, de la micro-informatique au labyrinthe génératif.
- [`ERYX_PLAYABLE_SPATIAL_ROADMAP.md`](./ERYX_PLAYABLE_SPATIAL_ROADMAP.md) : traduction opérationnelle des trois moodboards, carte des sept lieux et parcours dramatique en huit actions.

## Source littéraire

- [`IN_THE_WALLS_OF_ERYX.md`](./IN_THE_WALLS_OF_ERYX.md) : transcription Markdown attribuée du texte publié en 1939, avec provenance, statut de reproduction et note de contenu historique.

## Architecture et plans actifs

- [`GENERATION_DYNAMIQUE_DECORS_PLAN.md`](./GENERATION_DYNAMIQUE_DECORS_PLAN.md) : conception illustrée et plan approuvé pour des lieux imaginés et écrits en `.scene` par Ministral pendant la partie.
- [`GENERATION_DYNAMIQUE_DECORS_RUNTIME.md`](./GENERATION_DYNAMIQUE_DECORS_RUNTIME.md) : implémentation du mode explicite `dynamic`, contrôles, persistance, banc comparatif et limites de qualification.
- [`GENERATION_DYNAMIQUE_DECORS_BILAN.md`](./GENERATION_DYNAMIQUE_DECORS_BILAN.md) : tests exécutés, essais réels et décision de qualification, distincts des réponses injectées dans les tests.
- [`concept-art/ATLAS_COMPOSITION.md`](./concept-art/ATLAS_COMPOSITION.md) : les dix références visuelles, leurs principes transférables et les limites de transposition vers le raytracer.
- [`concept-art/raytracer-study/README.md`](./concept-art/raytracer-study/README.md) : reconstructions natives des concepts 01, 02, 03 et 09, comparateur illustré, mesures et ajout de `distant_light` pour les variantes diurnes.
- [`FUNCTIONAL_PIPELINE_V1.md`](./FUNCTIONAL_PIPELINE_V1.md) : chaîne autoritative `commande -> résultat structuré -> état spatial -> compilation -> rendu` et cible de mutation topologique.
- [`SPATIAL_VALIDATION_PLAN.md`](./SPATIAL_VALIDATION_PLAN.md) : protocole Eryx pour barrières invisibles, revisites, chemins impossibles, fréquence des mutations et cartographie.
- [`HYBRID_SCENE_LAYOUT_PLAN.md`](./HYBRID_SCENE_LAYOUT_PLAN.md) : migration du compositeur hybride vers carrière, plateau, abri, scanner et seuil invisible.
- [`SCENE_FORMAT_V1.md`](./SCENE_FORMAT_V1.md) : format de scène réellement implémenté, y compris les prefabs de prospection/extraction. Les murs invisibles restent volontairement dans l'état de traversal, pas dans la géométrie visible.
- [`ANIMATED_VIEW_SPEC.md`](./ANIMATED_VIEW_SPEC.md) : spécification maintenant implémentée des huit images raytracées progressives, du mouvement de respiration et du playback ping-pong.
- [`LLAMA_CUDA_SPECS.md`](./LLAMA_CUDA_SPECS.md) : procédure de build et de validation de `Ministral 3 8B` avec `llama.cpp` et CUDA.
- [`ETUDE_MIGRATION_MINISTRAL_GEMMA_E4B.md`](./ETUDE_MIGRATION_MINISTRAL_GEMMA_E4B.md) : étude en français du remplacement par Gemma 4 E4B, de la réflexion sélective, des adaptations logicielles et du protocole comparatif.
- [`SCENE_MODEL_MIGRATION_BENCHMARK.md`](./SCENE_MODEL_MIGRATION_BENCHMARK.md) : première étape appliquée au benchmark `.scene`, profils locaux, séparation des canaux et contrôle des caméras.
- [`SCENE_BENCHMARK_MODEL_COMPARISON.md`](./SCENE_BENCHMARK_MODEL_COMPARISON.md) : comparaison réelle des dix scènes avec Ministral et Gemma, temps mesurés, défauts de caméra et images.
- [`SCENE_BENCHMARK_GEMMA_GUIDANCE.md`](./SCENE_BENCHMARK_GEMMA_GUIDANCE.md) : essai correctif Gemma avec consigne explicite de caméra, comparé au même modèle sans cette consigne.
- [`ETUDE_MISE_A_JOUR_MINISTRAL_LLAMA_CPP.md`](./ETUDE_MISE_A_JOUR_MINISTRAL_LLAMA_CPP.md) : étude en français des variantes Ministral, de la mise à jour de `llama.cpp` et des modèles Mistral récents, à matériel cible inchangé.
- [`MINISTRAL_ROADMAP_PROGRESS.md`](./MINISTRAL_ROADMAP_PROGRESS.md) : état d'exécution de cette roadmap en duo.
- [`MINISTRAL_RUNTIME_GUIDE.md`](./MINISTRAL_RUNTIME_GUIDE.md) : C++17, construction Windows, profils, canaux, limites et recette.
- [`MINISTRAL_SCENE_COMPARISON.md`](./MINISTRAL_SCENE_COMPARISON.md) : mesures et images du benchmark direct Q4/Q5/Reasoning.
- [`MINISTRAL_SCENE_SEEDS.md`](./MINISTRAL_SCENE_SEEDS.md) : répétitions directes Reasoning sur trois graines.
- [`MINISTRAL_NATIVE_COMPARISON.md`](./MINISTRAL_NATIVE_COMPARISON.md) : comparaison native A/B/BJ/C/D, sessions, latences et métadonnées de salle.
- [`MINISTRAL_SEED_COMPARISON.md`](./MINISTRAL_SEED_COMPARISON.md) : reprises natives ciblées, avec limites du score automatique.
- [`MINISTRAL_QUALIFICATION_REPORT.md`](./MINISTRAL_QUALIFICATION_REPORT.md) : bilan de qualification, provenance, lecture visuelle et décision.
- [`ETUDE_ADAPTATION_LLM_IN_THE_WALLS_OF_ERYX.md`](./ETUDE_ADAPTATION_LLM_IN_THE_WALLS_OF_ERYX.md) : troisième étude, en français, sur l’adaptation du LLM à partir de la nouvelle, le corpus de recherche-création, LoRA/QLoRA et l’évaluation artistique à matériel inchangé.
- [`WINDOWS_DISTRIBUTION.md`](./WINDOWS_DISTRIBUTION.md) : construction, contenu et validation de l'installeur Windows sans poids IA embarqués.
- [`MULTIPROCESSING_FEASIBILITY.md`](./MULTIPROCESSING_FEASIBILITY.md) : étude et base de validation du rendu CPU parallèle, concrétisée par OpenMP.

## État, décisions et histoire

- [`TECHNICAL_STATE.md`](./TECHNICAL_STATE.md) : photographie factuelle du dépôt et frontière entre implémentation legacy et cible Eryx.
- [`KNOWN_ISSUES.md`](./KNOWN_ISSUES.md) : bugs, limites, dettes de migration et risques ouverts.
- [`DECISIONS.md`](./DECISIONS.md) : décisions techniques et conceptuelles stabilisées.
- [`PROJECT_JOURNAL.md`](./PROJECT_JOURNAL.md) : historique chronologique, conservé tel quel.
- [`NOTES.md`](./NOTES.md) : recherches et formulations, généalogie Eryx et médiation DMA WinUAE.

## Documents générés et baselines historiques

- [`PREFAB_CATALOG.md`](./PREFAB_CATALOG.md) : catalogue généré depuis les géométries C++ actives de la carrière d'Eryx, avec deux vues et une scène d'audit par objet.
- [`SCENE_GENERATION_BENCHMARK.md`](./SCENE_GENERATION_BENCHMARK.md) : benchmark historique de génération `.scene` directe.
- [`HYBRID_SCENE_GENERATION_BENCHMARK.md`](./HYBRID_SCENE_GENERATION_BENCHMARK.md) : benchmark historique de la chaîne hybride.
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
- employer la fiction Eryx comme seule direction narrative active, sans réintroduire le vocabulaire des phases antérieures ;
- employer *Within the Latent Walls* en anglais et *Entre les Murs Latents* en français pour désigner le projet actif ;
- préférer l'anglais technique concis pour les nouveaux documents, sauf lorsqu'un fichier existant est volontairement maintenu en français.
