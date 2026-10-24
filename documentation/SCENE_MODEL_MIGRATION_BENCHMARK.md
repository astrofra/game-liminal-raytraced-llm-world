# Première étape de migration : benchmark direct `.scene`

Cette étape applique l'[étude Ministral / Gemma](ETUDE_MIGRATION_MINISTRAL_GEMMA_E4B.md) au lanceur `run_scene_generation_benchmark.bat`. Elle utilise le CLI local existant, avec Jinja et son parseur de conversation. Le runtime C++ du jeu, l'installeur et le modèle par défaut du jeu ne sont pas migrés par cette étape.

## Utilisation

Pour essayer Gemma avec la consigne de caméra validée sur les dix cas, lancer `run_scene_generation_benchmark.bat --profile gemma-e4b --camera-guidance`. Gemma reste expérimental : le contrôle géométrique ne garantit pas la qualité de chaque image.

Depuis la racine du projet, pour reproduire les variantes initiales :

```bat
download_gemma.bat
run_scene_generation_benchmark.bat --profile ministral
run_scene_generation_benchmark.bat --profile gemma-e4b
run_scene_generation_benchmark.bat --profile gemma-e4b --reasoning on --reasoning-budget 256
```

Chaque invocation crée un dossier distinct sous `documentation/generated/scene_generation_benchmark_runs/`, contenant `report.md`, les états, prompts, réponses brutes, réponses finales, scènes, images, journaux et `results.json`. Le rapport historique `SCENE_GENERATION_BENCHMARK.md` est préservé. Un dossier de sortie non vide ou un rapport déjà existant est refusé.

Pour un essai court ou une destination explicite :

```bat
run_scene_generation_benchmark.bat --profile gemma-e4b --case entry_gate_dusk --output-root documentation/generated/mon-essai-gemma
```

`--case` est répétable. Un identifiant inconnu est une erreur. `--model` permet de fournir un GGUF différent du chemin associé au profil ; le chemin et l'empreinte réellement employés figurent dans les preuves. `NO_PAUSE=1` permet l'exécution automatisée des lanceurs Windows.

## Poids et runtime

Le téléchargement Gemma est fixé au dépôt `ggml-org/gemma-4-E4B-it-GGUF`, révision `51db8454ac67b58ed848e54d4111be2ad16ba4bf`, fichier `gemma-4-E4B-it-Q4_K_M.gguf`. Le helper contrôle la taille de 5 335 289 824 octets, la signature GGUF et le SHA-256 `90ce98129eb3e8cc57e62433d500c97c624b1e3af1fcc85dd3b55ad7e0313e9f`. `python scripts/download_gemma.py --verify-only` refait ces contrôles sans réseau. Aucun poids Ministral n'est supprimé.

Le CLI testé annonce `version: 28 (cd60aba)`. Le fichier `VENDORED_COMMIT.txt` déclare séparément `6b36c2305644fd30db7cce3f4840c74574f31ce9` pour les sources. Ces deux références ne sont pas assimilées : les preuves conservent version et empreinte de l'exécutable ainsi que la référence des sources. Aucune mise à jour vendor n'a été nécessaire pour ces premiers chargements Gemma dans le CLI.

Sources de l'artefact : [révision GGUF](https://huggingface.co/ggml-org/gemma-4-E4B-it-GGUF/tree/51db8454ac67b58ed848e54d4111be2ad16ba4bf), [format des canaux Gemma 4](https://ai.google.dev/gemma/docs/core/prompt-formatting-gemma4).

## Contrat de génération et de mesure

- Profil par défaut : Ministral ; réflexion désactivée explicitement.
- Réflexion : `--reasoning on` exige un `--reasoning-budget` positif. Les budgets 256, 512 et 1024 sont sélectionnables ; leur comparaison complète reste une étape d'évaluation.
- `--n-predict` réserve par défaut 1024 tokens à la réponse. Le plafond transmis au CLI est cette valeur plus le budget de réflexion et, en mode réflexion, une marge de 64 tokens pour les transitions. Le sampler du CLI impose le budget de réflexion ; le contrôle final refuse une génération arrêtée sur la limite globale.
- Contexte par défaut 4096, batch 512, micro-batch 128 ; aucune croissance automatique du contexte côté script. Augmenter explicitement `--ctx-size` si le prompt et les deux budgets ne tiennent pas.
- Comparaison contrôlée : température 0, graine 42, top-k 40, top-p 0,95. Un profil stochastique conseillé pour Gemma peut être testé séparément avec `--temperature 1 --top-k 64 --top-p 0.95` et plusieurs graines ; ne pas mélanger ses résultats à la comparaison gloutonne.
- Délai maximal de génération : 300 secondes, configurable par `--generation-timeout`. Le délai expiré est un échec enregistré, pas une scène récupérable.
- Seule une réponse finale avec fin EOS observée peut être transmise à l'audit. Les pensées sont exclues avant l'extraction `.scene`. Une fin inconnue, un budget épuisé, un canal incomplet ou un format CLI inattendu échouent explicitement.
- Le CLI journalise ses événements en mode debug afin d'observer l'arrêt, les compteurs et les allocations. Ces journaux sont volumineux et peuvent contenir le raisonnement brut ; le rapport de lecture expose la réponse finale et les métriques.
- Les temps de génération comprennent le chargement et la fermeture du processus, mais pas le hachage initial du modèle. Les temps de prompt et de rendu sont séparés. Les allocations à la fermeture ne sont pas des mesures de pic mémoire.

Le transcript `--output-file` sépare le message utilisateur et la réponse assistant. Le script vérifie le préfixe contre le prompt exact, en tolérant la suppression des sauts de ligne finaux propre au CLI. Il contrôle aussi le flux de réflexion live : le fichier transcript seul peut refermer artificiellement un bloc interrompu.

## Validité et cadrage

Le contrôle du renderer porte sur le chargement de la scène. Il ne suffit pas à garantir une image exploitable : les premiers essais Gemma ont produit une visée verticale parallèle à `up`, acceptée par le renderer mais presque uniforme à l'image.

Le contrôle complémentaire `audit_camera()` exige une seule caméra, des valeurs finies, un angle de champ dans ]0,180[, des vecteurs non nuls et une direction non parallèle au vecteur vertical. Il signale le défaut sans réécrire silencieusement la caméra ou la scène générée. Il ne juge pas l'éclairage, le placement des objets ou la fidélité artistique.

`up` et `fov` restent optionnels comme dans le moteur, avec les valeurs par défaut `(0,1,0)` et `40`. Le résultat conserve `syntax_valid_scene` séparément ; `valid_scene` exige désormais aussi la validité de la caméra. Le statut `invalid_camera` produit un code de sortie 2, tout en conservant l'image de diagnostic. Les premiers fichiers de la matrice A0/A1/B/C précèdent cette intégration : leurs statuts bruts sont préservés et le comparateur refait le contrôle de caméra sur les scènes sauvegardées.

L'option expérimentale `--camera-guidance` ajoute au prompt une contrainte explicite de base caméra et un exemple non dégénéré. Le prompt original du moteur et le prompt effectivement envoyé sont tous deux conservés. Cette variante change le prompt : elle doit être évaluée séparément de la comparaison à prompts identiques.

```bat
run_scene_generation_benchmark.bat --profile gemma-e4b --camera-guidance
```

Le comparateur peut réévaluer les anciennes sorties sans modifier leurs preuves :

```powershell
python scripts/compare_scene_benchmarks.py --run documentation/generated/scene_model_comparison/A0-ministral --run documentation/generated/scene_model_comparison/A1-ministral --run documentation/generated/scene_model_comparison/B-gemma --run documentation/generated/scene_model_comparison/C256-gemma --markdown documentation/SCENE_BENCHMARK_MODEL_COMPARISON.md --json documentation/generated/scene_model_comparison/comparison.json
```

Choisir de nouveaux chemins de synthèse pour une nouvelle comparaison.

## Duo et validation

Codex a conçu l'adaptation du banc, la séparation des canaux, les mesures et la revue des images. DeepSeek via Copilot CLI/Ollama a livré le helper de téléchargement ([DUO-003](duo/tasks/DUO-003.md)), puis le contrôle de caméra et ses tests ([DUO-004](duo/tasks/DUO-004.md)), dans deux worktrees isolés. Les revues consignent les contrôles et les écarts observés.

```powershell
python scripts/test_scene_benchmark_protocol.py
python scripts/test_scene_benchmark_quality.py
python scripts/test_scene_benchmark_runner.py
```

Cette batterie de dix briefs historiques constitue un diagnostic du banc direct, pas les 40 cas Eryx et les sessions longues proposés dans l'étude. Les appels natifs, la navigation, la sauvegarde, la localisation, la thermique et la distribution restent à évaluer dans les étapes suivantes. La décision de changer le modèle du jeu doit tenir compte des images et de la cohérence, pas seulement du temps de génération ou du taux de syntaxe valide.

## Résultat de la première matrice

Voir la [comparaison avec images et preuves](SCENE_BENCHMARK_MODEL_COMPARISON.md). Sur ces dix briefs : Ministral obtient 10/10 scènes à syntaxe et caméra valides ; Gemma sans réflexion 2/10 ; Gemma avec 256 tokens de réflexion 2/10, avec en plus une scène syntaxiquement invalide (`size()` incomplet). Les médianes de génération sont respectivement 22,17 s, 17,01 s et 17,80 s. Ces mesures ne démontrent pas de bénéfice visuel de la réflexion sur cette batterie.

Le contrôle de caméra reste un minimum géométrique. Certaines images qui le passent restent mal cadrées ou masquées par la géométrie ; la revue des images ne peut donc pas être remplacée par ce seul score. Le profil Ministral reste le choix par défaut pendant l'expérimentation Gemma.

## Essai correctif du prompt

Après trois cas ciblés réussis, la variante Gemma sans réflexion avec `--camera-guidance` a été étendue aux dix cas : **10/10 scènes à syntaxe et caméra valides**, EOS observé pour chaque réponse, médiane de génération **16,15 s**. Voir le [comparatif du prompt corrigé](SCENE_BENCHMARK_GEMMA_GUIDANCE.md) et le [rapport complet avec les dix images](generated/scene_model_comparison/D-gemma-guided-full/report.md).

Ce résultat corrige le défaut systématique de caméra, sans modifier a posteriori les scènes générées. Il ne suffit pas à recommander une bascule du jeu : `control_hub` reste visuellement presque uniforme et très bruité malgré une caméra valide. Le cadrage des objets, les occultations et l'éclairage demandent encore une évaluation artistique, puis un corpus Eryx adapté. Le gain mesuré sur cette variante comprend un changement de prompt ; il ne doit pas être attribué au modèle seul.

Validation logicielle : **51 tests réussis** (17 de protocole, 28 de caméra, 6 du pipeline avec processus simulés), complétés par les générations réelles et la revue des images. L'aide du batch fonctionne également depuis un autre répertoire de travail. Aucun changement du code C++, des sources vendor, de l'installeur ou des poids Ministral ; aucun commit ni push.
