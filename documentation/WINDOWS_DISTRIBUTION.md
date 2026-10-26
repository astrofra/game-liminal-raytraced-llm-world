# Distribution Windows

Le Setup standard contient le jeu, les ressources, les polices et les DLL,
sans les poids du modèle. **Après la copie des fichiers du jeu**, il télécharge
Gemma 4 E4B Q4_K_M (5 335 289 824 octets, soit 5,34 Go), puis vérifie taille et SHA-256.
Le joueur n'installe aucun moteur, interpréteur ou outil de compilation.
Une connexion Internet est nécessaire pour télécharger Gemma ; le jeu fonctionne
ensuite entièrement hors ligne, sans compte ni service d'inférence.

## Préparer l'installeur

Avec un binaire publié à jour :

```powershell
python scripts/build_installer.py --skip-build --version 3.0.1
```

`--skip-build` évite toute compilation C++/CUDA. Le packaging contrôle la provenance
du binaire, l'identité du manifeste et du vendor, puis copie les dépendances.
Le GGUF local n'est ni requis, ni lu, ni copié pour le Setup standard.

- `output/installer/stage/` : jeu et DLL sans modèle, zone de fabrication du Setup.
- `output/installer/dist/Within-the-Latent-Walls-Setup-3.0.1.exe` : unique fichier d'installation à distribuer, sans `.bin`.
- `output/installer/dist/Within-the-Latent-Walls-Source-3.0.1.zip` : sources correspondantes, dépendances et scripts de construction, sans poids.
- `distribution-manifest.json` : inventaire du payload ; `model_embedded=false`, `downloaded_during_installation=true`, URL/révision/taille/SHA du modèle.

Les anciens artefacts 3.0.0 avec modèle embarqué ne sont pas le Setup courant.
Mesure du 26 octobre : Setup 3.0.1 de 414 184 450 octets (414 Mo), contre
5 918 840 219 octets pour l'EXE et les trois volumes 3.0.0, soit 93 % de moins.
Le stage sans modèle occupe 584 534 645 octets.

## Dernière phase : Gemma

L'URL, la révision, le nom du fichier, la taille et le SHA proviennent de
`scripts/models.json`. La révision reste épinglée ; aucune recherche de « latest ».
Inno Setup télécharge dans `{app}/models/gemma-4-e4b/`, où le runtime résout déjà
ses ressources, indépendamment du dossier courant du raccourci.

Un fichier existant est réutilisé seulement après vérification de sa taille
64 bits et de son SHA-256. Sinon, le téléchargement natif utilise un fichier
temporaire et vérifie le SHA avant publication du nom final : voir la
[documentation Inno Setup, paramètres `Hash` et `download`](https://jrsoftware.org/ishelp/topic_filessection.htm).
La taille est aussi contrôlée après téléchargement.

Une panne réseau ou un mauvais SHA déclenche l'erreur native avec possibilité de
réessayer ou d'annuler. Si l'utilisateur choisit « Ignorer », la fin d'installation
est indiquée comme incomplète, le code de sortie est non nul et le lancement du
jeu n'est pas proposé. Relancer le Setup permet de retenter ; la reprise à l'octet
près après interruption n'est pas garantie. Le volume total téléchargé reste
celui du jeu et du modèle : l'allègement concerne le fichier Setup à distribuer.

## Option développeur : dossier portable complet

```powershell
python scripts/download_gemma.py --verify-only
python scripts/build_installer.py --skip-build --portable-only
```

Si le modèle manque, `download_gemma.bat` le récupère. Cette option exige le GGUF
local vérifié et le copie dans `stage/` (`model_embedded=true`). Elle ne compile
pas de Setup. Le dossier complet se lance par `WithinTheLatentWalls.exe` hors ligne.
Chaque fabrication remplace `stage/` ; conserver une copie du portable si nécessaire.
Le compilateur refuse de produire un Setup à partir d'un stage contenant des GGUF.

## Conditions et qualification

Windows x64 10 version 1809 ou plus récent, suffisamment de RAM et un pilote NVIDIA
pour CUDA. Les DLL CUDA et Microsoft requises sont copiées auprès du jeu ; CUDA
Toolkit n'est pas nécessaire chez le joueur. Prévoir au moins 8 Go libres pour
une installation neuve, puis de la place pour les archives de recherche. Le
diagnostic CPU utilise `--llm-gpu-layers 0` ; ses performances se qualifient séparément.

Les archives sont écrites dans Local AppData et conservées à la désinstallation.
Chaque lancement commence une nouvelle partie. `MODEL_NOTICE.txt` identifie le
modèle et sa licence ; les notices des dépendances sont incluses dans le paquet.

La recette du téléchargement se relance avec Inno Setup installé :

```powershell
python scripts/verify_installer_download.py --out C:/preuves/nouvelle-recette-installeur
```

Ajouter `--existing-model models/gemma-4-e4b/gemma-4-E4B-it-Q4_K_M.gguf`
pour vérifier aussi la réutilisation du vrai fichier de plus de 4 Go, sans copie
ni téléchargement, par lien physique dans le dossier de preuves sur le même volume.

Ce banc utilise le script Inno de production et un petit fichier HTTP local, sans
inscription de désinstallation, raccourci ni lancement de jeu. Il vérifie l'ordre
copie/téléchargement, la réutilisation, la corruption, les erreurs réseau/SHA,
l'interruption, la relance et le refus de succès après une étape ignorée.
Il ne remplace pas une installation complète sur un Windows vierge.
Le [bilan du runtime](BILAN_RUNTIME_AUTONOME.md) conserve les preuves de la version
3.0.0 ; la [revue du retour au Setup léger](duo/reviews/DUO-030.md) décrit cette correction.
