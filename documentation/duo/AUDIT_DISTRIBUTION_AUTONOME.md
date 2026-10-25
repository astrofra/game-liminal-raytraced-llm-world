# Audit — distribution autonome (Gemma hors ligne)

Statut : **Relu et corrigé par Codex** ; livraison brute conservée hors dépôt. Voir [DUO-024](reviews/DUO-024.md). Étude documentaire seule, sur `aebcdb625f5847c3a860c724303b1f2ef1901a8c`.
Aucun test, build, téléchargement ni changement produit n'a été exécuté dans ce lot. « Fait » = lu
dans les fichiers listés ; « Reco » = proposition à arbitrer par Codex, non implémentée.

## 1. Faits observés : téléchargement, contrôle SHA, distribution

1. `scripts/download_gemma.py` : `REPO_ID = ggml-org/gemma-4-E4B-it-GGUF`, révision
   `51db8454…16ba4bf`, fichier `gemma-4-E4B-it-Q4_K_M.gguf`, `EXPECTED_SIZE = 5_335_289_824`,
   `EXPECTED_SHA256 = 90ce9812…e0313e9f`. `validate_gguf()` exige présence, taille exacte, signature
   `b"GGUF"` puis SHA-256 (`sha256_file()`). Défaut `models/gemma-4-e4b`, options `--force-download` et
   `--verify-only` (hors ligne, sans `huggingface_hub`) ; le téléchargement seul impose `huggingface_hub`.
2. `scripts/model_profiles.py` : `load_profiles()` valide `schema_version == 1`, `default_profile`,
   `repo_id`, `revision` (40 hex), `filename` nu, `size_bytes > 0`, `sha256` (64 hex), `relative_path`
   portable (sans `..`/`\`/`:`), `reasoning`, `temperature`, `top_k`, `top_p` ; aucun réseau.
   `DEFAULT_PROFILE = load_profiles()["default_profile"]` à l'import (manifeste requis) ; `resolve_model_path()` concatène
   `root / relative_path`. Gemma non couvert, pas d'indicateur de livraison embarquée.
3. `scripts/ministral_models.json` : trois profils Ministral (Q4_K_M, Q5_K_M, Reasoning Q4), défaut
   `ministral-8b-q4`. Aucun profil Gemma, aucun indicateur de livraison embarquée.
4. `scripts/build_installer.py` : `MODEL_PROFILE = get_profile()` fige Ministral Q4_K_M et alimente
   `build_inno_installer()` en `/DModelDir|Name|Url|Hash|Size` (URL
   `https://huggingface.co/{repo}/resolve/{rev}/{file}?download=true`). `copy_distribution_payload()`
   copie notices/README, crée le dossier parent de `MODEL_RELATIVE_PATH` vide, **refuse tout `.gguf`
   dans le stage**, écrit `distribution-manifest.json` (`model_embedded: false`,
   `downloaded_during_installation: true`) et vérifie la provenance (SHA exe, manifeste ministral,
   commit llama.cpp). `build_inno_installer()` rejette un setup ≥ 1 000 000 000 octets ;
   `iter_source_files()` embarque `scripts/`, `packaging/` et `documentation/*.md` dans l'archive source.
5. `packaging/within_the_latent_walls.iss` : `#ifndef ModelDir|ModelName|ModelUrl|ModelHash|ModelSize`
   → `#error`. `[Files]` : `Source: {#ModelUrl}` en `external download` + `Hash` + `onlyifdoesntexist`.
   `[Dirs]` crée `{app}\{#ModelDir}`. Raccourcis et `[Run]` : `--sdl --model "{app}\{#ModelDir}\{#ModelName}"`
   `--location quarry_threshold --save-state {localappdata}\WithinTheLatentWalls\sdl_session_state.json`,
   `WorkingDir: "{app}"`. `[UninstallDelete]` : `{app}\models`. `LicenseFile` et `InfoBeforeFile`
   pointent vers `LICENSE.txt` / `INSTALLATION_README.txt` du stage.
6. `packaging/MODEL_NOTICE.txt`, `packaging/INSTALLATION_README.txt`, `documentation/WINDOWS_DISTRIBUTION.md` :
   annoncent Ministral Q4_K_M, licence Apache 2.0 et **connexion Internet obligatoire pendant setup** ;
   aucune mention de Gemma ni d'installation hors ligne.
7. `scripts/build_native.py` : `runtime-build.json` enregistre commit llama.cpp, SHA du manifeste
   ministral et SHA des binaires ; CMake/Ninja + CUDA. `CMakeLists.txt` compile `src/ollama_runtime.cpp`
   dans l'exécutable livré, construit le test `ollama_protocol`, génère l'en-tête de profils via
   `Python3` à partir de `model_profiles.py` + `ministral_models.json`, lie llama.cpp statiquement
   (`BUILD_SHARED_LIBS OFF`, `GGML_CUDA`) et récupère SDL3/SDL3_ttf par `FetchContent` (réseau au build).

## 2. Points à modifier ou supprimer pour un paquet hors ligne complet

- Manifeste : ajouter un profil Gemma reprenant à l'identique les constantes de `download_gemma.py`
  (repo, révision, fichier, taille, SHA, `relative_path: models/gemma-4-e4b/…`) et en faire le défaut ;
  sans cela le module, CMake et l'installeur restent Ministral.
- `build_installer.py` : retirer l'URL `/resolve/`, `MODEL_*` d'externalisation et les contrôles
  « pas de `.gguf` dans le stage » / « setup < 1 Go » pour le paquet embarqué ; copier le GGUF vérifié
  dans le stage et l'inscrire dans `payload_files` ; mettre `model_embedded: true` et
  `downloaded_during_installation: false`.
- `.iss` : remplacer la ligne `Source: {#ModelUrl}` external par une source locale du stage ; vérifier le SHA-256 au staging et à la livraison, supprimer `onlyifdoesntexist`/`download` ; vérifier le comportement de
  `[UninstallDelete] {app}\models` pour des poids livrés.
- Notices : réécrire `MODEL_NOTICE.txt` et `INSTALLATION_README.txt` pour Gemma (installation hors ligne et identité Gemma ; licence exacte à relever à la révision livrée) ; `WINDOWS_DISTRIBUTION.md` doit décrire l'installation hors ligne.
- Runtime/packaging : décider du sort de `src/ollama_runtime.cpp` et du test `ollama_protocol` (Codex,
  hors périmètre de ce lot) ; aucune référence active à Ministral ne doit subsister dans le paquet.
- `download_gemma.py` : reste un outil de préparation ; sa sortie doit devenir l'entrée du stage, et
  `--verify-only` sert de contrôle local du GGUF livré.

## 3. Pièges possibles (faits + recos)

- Chemins : `MODEL_RELATIVE_PATH` doit rester portable et cohérent entre manifeste, dossier du stage,
  `[Dirs]` et paramètre `--model` des raccourcis. Reco : tester aussi un lancement sans `WorkingDir`.
- DLL : `copy_runtime_dependencies()` exige `dumpbin`, `CUDA_PATH`, les redistribuables MSVC et un
  `.dll` résoluble sur `PATH` ; toute DLL manquante fait échouer le packaging. Reco : contrôler sur la
  machine cible l'absence d'installation de CUDA Toolkit.
- Modèle absent : sans GGUF embarqué ni téléchargement, le jeu démarrerait sans modèle. Reco : refuser
  le packaging si le GGUF attendu n'est pas vérifié.
- Taille/compression : `lzma2/ultra64` + `SolidCompression=yes` sur ~5,34 Go allongent fortement la
  compilation et l'espace disque temporaire. Reco : mesurer (aucun chiffre avancé ici).
- Licences/NOTICE : les fiches officielles [Google](https://huggingface.co/google/gemma-4-E4B-it) et
  [ggml-org](https://huggingface.co/ggml-org/gemma-4-E4B-it-GGUF) affichent Apache 2.0 pour Gemma 4.
  La livraison brute affirmait à tort qu'Apache devait disparaître ; correction Codex après vérification.
  Reprendre l'identité, l'origine et les notices exactes de l'artefact livré, pas celles de l'ancien modèle.
- Dépendance réseau résiduelle : `urllib`/Inno bootstrap et `FetchContent` sont des dépendances de
  build ; l'ancien `Source: {#ModelUrl}` est une dépendance réseau de l'installation ; le runtime du jeu
  utilise aussi actuellement le service HTTP Ollama. Retirer les deux et vérifier l'exécution sans réseau.
- Traçabilité : provenance `model_manifest_sha256` et smoke test `--llama-info` ne chargent pas le
  modèle ; ils ne prouvent ni Gemma ni l'inférence. Reco : test d'un tour LLM réel hors ligne.

## 4. Recette Windows propre sans Ollama/Python/CMake/Internet

1. Préparer hors de la machine le setup et son SHA-256 publié ; copier les deux fichiers.
2. Sur une machine Windows 10 1809+/11 x64 vierge : aucun Python, CMake, Visual Studio, CUDA Toolkit,
   Ollama. Couper l'interface réseau ; vérifier que le jeu ne lance ni service ni connexion locale.
3. `certutil -hashfile <setup>.exe SHA256` et comparer au hash publié.
4. Installer en per-user, puis vérifier `%LOCALAPPDATA%\Programs\Within the Latent Walls\models\…` :
   présence du GGUF, taille attendue, `certutil -hashfile … SHA256` égal au manifeste, présence de
   `distribution-manifest.json`, des DLL runtime et des notices.
5. Lancer par le raccourci du menu Démarrer, puis **hors du dossier du jeu** (ex. `%USERPROFILE%`) : le
   chemin absolu de `WithinTheLatentWalls.exe`, sans paramètres techniques, doit charger assets, police et modèle.
   Tester séparément les options de diagnostic ; elles ne doivent pas être nécessaires au lancement normal.
6. Vérifier l'absence de service/port Ollama, l'absence de trafic réseau, un tour LLM complet, une
   sauvegarde puis un rechargement de session.
7. Désinstaller : `{app}` supprimé, `%LOCALAPPDATA%\WithinTheLatentWalls` conservé ; relancer une
   installation pour vérifier l'idempotence.

Aucun de ces points n'est validé à ce stade : la recette est une proposition, pas un résultat observé.
