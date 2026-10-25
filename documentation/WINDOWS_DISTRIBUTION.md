# Distribution Windows autonome

La distribution contient le jeu, Gemma 4 E4B Q4_K_M (5 335 289 824 octets),
les ressources, les polices et les DLL nécessaires. Le joueur n'installe aucun
moteur, interpréteur ou outil de compilation. L'installation fonctionne hors ligne.

## Préparer le paquet sur la machine de développement

```powershell
python scripts/build_native.py --publish
python scripts/download_gemma.py --verify-only
python scripts/build_installer.py --skip-build --portable-only
python scripts/build_installer.py --skip-build --version 3.0.0
```

Si le modèle manque, `download_gemma.bat` le récupère depuis une révision épinglée ;
cette étape est réservée à la fabrication. Le packaging vérifie SHA du binaire,
identité du manifeste et du vendor, taille et SHA du GGUF, puis copie les dépendances.

- `output/installer/stage/` : dossier portable complet ; lancer `WithinTheLatentWalls.exe`.
- `output/installer/dist/*Setup-3.0.0.exe` et `*.bin` : installeur et volumes à distribuer ensemble.
- `output/installer/dist/*Source-3.0.0.zip` : sources correspondantes, dépendances et scripts de construction.
- `distribution-manifest.json` : inventaire, tailles et SHA des fichiers livrés ; `model_embedded=true`.

Le modèle est inclus sans compression supplémentaire et réparti dans des volumes
de 2 Go, pour éviter les limites de taille d'un unique exécutable. Les raccourcis
lancent le jeu directement, sans ancien fichier de reprise ni argument de modèle.

## Conditions de fonctionnement

Windows x64 10 version 1809 ou plus récent, suffisamment de RAM, pilote NVIDIA
pour l'accélération CUDA. Les DLL CUDA et Microsoft requises sont copiées auprès
du jeu ; CUDA Toolkit n'est pas nécessaire chez le joueur. Prévoir 16 Go pour
conserver le paquet et son installation. Le diagnostic CPU utilise `--llm-gpu-layers 0`.
Les performances CPU et GPU se qualifient séparément.

Les ressources sont recherchées près de l'exécutable, indépendamment du dossier
courant. Archives écrites dans Local AppData, conservées à la désinstallation.
Chaque lancement commence une nouvelle partie.

## Notices et qualification

`MODEL_NOTICE.txt` identifie le GGUF, sa révision et son SHA. La fiche épinglée
du quantificateur ne contient pas de fichier de licence distinct ; le modèle
source renvoie à [la licence Apache 2.0 publiée par Google](https://ai.google.dev/gemma/docs/gemma_4_license).
Le texte de cette licence et les notices des dépendances sont inclus dans le paquet.

Voir le [bilan observé](BILAN_RUNTIME_AUTONOME.md) pour les tests effectués.
Ne pas assimiler une recette sur la machine de développement à une qualification
sur un Windows vierge ; cette dernière doit être identifiée séparément.
