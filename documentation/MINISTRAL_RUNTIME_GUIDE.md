# Runtime Ministral : construction et qualification

La qualification terminée est résumée dans [MINISTRAL_ROADMAP_PROGRESS.md](MINISTRAL_ROADMAP_PROGRESS.md). Les profils Q5 et Reasoning restent expérimentaux ; le défaut du manifeste reste `ministral-8b-q4`.

## Construction Windows

Le runtime utilisant les templates Jinja de `llama-common` exige C++17. Le helper utilise CMake, Ninja, le SDK Windows et un toolset MSVC compatible avec CUDA. Sur la machine de qualification, CUDA 13.0 fonctionne avec MSVC 19.44 ; MSVC 19.51 est refusé par CUDA.

```powershell
python scripts/build_native.py --with-cli --publish
```

La construction se fait dans `build_ministral_v070/`. `--publish` copie les exécutables réussis vers les chemins utilisés par les lanceurs : `build/Release/liminal_cornell_renderer.exe` et `build/bin/Release/llama-cli.exe`. Le manifeste `runtime-build.json` lie les empreintes des exécutables au commit source. `build_release.bat --with-cli` appelle ce même helper.

Pour choisir un toolset installé ou extrait localement, utiliser `--msvc-root <dossier VC/Tools/MSVC/version>`, la variable `LIMINAL_MSVC_ROOT`, ou le fichier ignoré `build_settings.local.json` contenant `{"msvc_root":"..."}`. Aucun contournement `allow-unsupported-compiler` n'est utilisé. Une autre copie de llama.cpp peut être sélectionnée avec `--llama-dir` pendant une qualification.

## Profils reproductibles

La qualification conserve `ministral-8b-q4` par défaut. Q5 et Reasoning restent des profils d'essai : leurs compromis de complétude, de latence et de cohérence métier sont détaillés dans le [bilan](MINISTRAL_QUALIFICATION_REPORT.md). Le runtime vendorisé est `v0.7.0`, commit `620d2225cc436ab0b001e3200e590e3b286ef9ab`.

`scripts/ministral_models.json` contient dépôt, révision, fichier, taille, SHA-256 et réglages. Le téléchargement, la table C++ générée, le benchmark et le packaging lisent cette source. Aucun projecteur multimodal n'est nécessaire.

```powershell
python scripts/download_ministral.py --profile ministral-8b-q4 --verify-only
python scripts/download_ministral.py --profile ministral-8b-q5
python scripts/download_ministral.py --profile ministral-8b-reasoning-q4
```

Le téléchargement fixe la révision et contrôle les octets. `--verify-only` n'utilise pas le réseau. Les profils coexistent sur disque ; un modèle est chargé puis libéré à chaque appel, sans résidence simultanée volontaire.

```powershell
.\build\Release\liminal_cornell_renderer.exe --run-turn --location quarry_threshold --command "regarde autour de toi" --llm-profile ministral-8b-q4 --save-state output\qualification.json --output output\qualification.png
```

Pour tester le checkpoint Reasoning, sélectionner `ministral-8b-reasoning-q4`. La réflexion est propre à ce checkpoint. Les appels auxiliaires de thermique, localisation et réparation sont routés vers le Q4 Instruct, qui doit donc aussi être présent. La modulation de température corporelle reste active ; la température réellement utilisée figure dans chaque mesure.

## Canaux et limites

Le runtime applique le template Jinja embarqué et utilise le parseur de llama.cpp. Une garde supplémentaire reconnaît les marqueurs Ministral `[THINK]` et `[/THINK]`, y compris lorsqu'ils sont fragmentés ou présents dans le préfixe. Comme dans le parseur amont, le bloc de réflexion est optionnel : le checkpoint peut produire directement sa réponse finale. S'il ouvre une réflexion, elle doit être fermée. Seule la réponse finale atteint les parseurs métier et le flux affiché. La réflexion reste dans les diagnostics explicites ; elle n'entre pas dans les sauvegardes ni l'historique narratif.

Le budget final est `--llm-predict` (768 par défaut). Le budget de réflexion est distinct : `--llm-reasoning-budget` (512 par défaut). Une réflexion qui dépasse son budget provoque un échec explicite ; le runtime ne récupère pas un JSON de brouillon. Un arrêt EOG est requis. Les dépassements, annulations, erreurs de protocole et délais sont distingués.

Le contexte est fixe et borné (`--llm-context`, 8192 par défaut), avec batch 512 et micro-batch 128. Le délai par appel inclut le chargement (`--llm-timeout-ms`, 180000 par défaut). Avec `--use-json-grammar`, Instruct utilise une grammaire pendant l'échantillonnage ; Reasoning garde sa réflexion libre puis valide strictement le JSON final.

L'annulation est coopérative, vérifiée avant et pendant le chargement du modèle, entre les lots de prompt et les tokens, y compris pendant la réflexion, puis avant de publier la réponse. Les callbacks peuvent recevoir un delta vide comme signal de vie. Le délai ne constitue pas une garantie de préemption instantanée d'un transfert CUDA ni du nettoyage des allocations.

Chaque appel écrit une ligne JSON `LIMINAL_LLM_METRIC` sur stderr : profil, rôle de l'appel, température effective, budgets, tokens par canal, chargement, inférence, durée totale, offload et motif d'arrêt. Le total d'un tour inclut aussi les appels auxiliaires et réparations. Les mesures mémoire du runner sont des pics échantillonnés, pas des maxima matériels garantis ; la VRAM mesurée couvre tout le GPU.

## Recette

```powershell
ctest --test-dir build_ministral_v070 -C Release --output-on-failure
python -m unittest discover -s scripts -p "test_*.py"
python scripts/verify_native_llm.py --exe build_ministral_v070/Release/liminal_cornell_renderer.exe --out output/protocol-new-run
run_scene_generation_benchmark.bat --profile ministral-8b-q5
```

`verify_native_llm.py` charge réellement les trois modèles. Les sorties de probe comprennent volontairement le brut, la réflexion, la réponse finale et le flux visible pour inspection. Le benchmark de scènes refuse les dossiers de résultats déjà remplis. Les rapports historiques restent distincts du corpus Eryx.

Le corpus `scripts/eryx_qualification_cases.json` contient 40 cas et trois sessions de 15, 30 et 28 tours. `--write-initial-state <json>` exporte une fixture canonique sans inférence. Le runner `run_eryx_qualification.py --fixtures <dossier>` applique les états de stress et les langues déclarés, passe les commandes via des fichiers UTF-8, et sauvegarde/recharge chaque tour dans un processus neuf. Les assertions narratives nécessitent une lecture des résultats ; une scène valide ou un processus réussi ne suffit pas à les valider.

## Retour arrière

Les poids Q4 et les sauvegardes restent compatibles avec l'ancien exécutable. La campagne conserve une copie historique, une référence reconstruite avec le toolset commun et le binaire v0.7 avant adaptation. Les chemins et empreintes sont consignés dans le bilan de qualification. Pour un essai de retour arrière, lancer la référence depuis la racine du projet avec une copie de sauvegarde et le profil Q4 ; ne pas régénérer les lieux déjà enregistrés. Aucun changement de format de sauvegarde, de `.scene` ou de path tracer n'est prévu par cette migration.
