# Within the Latent Walls / Entre les Murs Latents

![Carrière vénusienne](img/liminal-room.png)

Fiction interactive et recherche-création : un labyrinthe vénusien inspiré de
*In the Walls of Eryx*, avec narration générée et décors calculés par un raytracer.
Le moteur C++17 garde l'autorité sur les déplacements, l'inventaire et les températures.

## Jouer

Lancer `play_within_the_latent_walls.bat` dans le dépôt, ou
`WithinTheLatentWalls.exe` dans le dossier portable. Choisir une des six langues,
puis jouer directement. Au seuil, **nord, est et sud** ouvrent l'exploration ; suivre ensuite
les issues de la boussole. `inventaire` / `inventory` / `i` consulte les possessions. `déposer la craie`,
`reprendre la craie` et `regarder tout autour` permettent de laisser et retrouver
un objet. Une petite cible décrite dans le lieu peut être ramassée après validation
par le moteur ; les objets et leurs retraits persistent pendant la partie.

Gemma 4 E4B Q4_K_M tourne dans le processus via **llama.cpp intégré**. Aucun
service, serveur, compte ou accès Internet n'est nécessaire pour jouer après installation.
L'installeur télécharge le modèle en dernière phase, puis le jeu fonctionne hors ligne.
Le modèle est vérifié au premier appel et reste chargé jusqu'à la fermeture.
Chaque requête possède son contexte propre ; la température d'échantillonnage
suit l'évolution thermique du scaphandre.

Un seul lieu est prédéfini. Les autres sont inventés pendant la partie et gardent
leur géométrie lors des retours. Chaque lancement recommence une exploration.
Le moteur conserve une issue vers l'inconnu : les impasses restent possibles tant
qu'un autre chemin inexploré est accessible. Pour les autres pièces, il tire une
cible de deux ou trois sorties, retour compris, avec une probabilité de 50 % pour
chacune. Le LLM choisit les directions ; le moteur ajuste leur nombre si nécessaire,
dans les limites du décor existant, sans modifier la géométrie 3D.
Le modèle compose librement à partir du lieu quitté et des événements validés.
Tous les lieux restent en mémoire pendant la partie ; seul l'extrait d'historique
envoyé au modèle est réduit selon son budget de tokens.
Descriptions, `.scene`, images et résultats sont archivés sous
`%LOCALAPPDATA%/WithinTheLatentWalls/research/`, sans réutilisation au lancement suivant.

## Construire

Windows : C++17, CMake, Ninja, MSVC compatible CUDA, Python pour les outils,
CUDA Toolkit pour construire la version GPU. SDL3/SDL_ttf sont liés statiquement ;
llama.cpp est épinglé au commit `620d2225cc436ab0b001e3200e590e3b286ef9ab`.

```powershell
python scripts/build_native.py --publish
# Préparation développeur seulement, si le GGUF manque :
python scripts/download_gemma.py
python scripts/download_gemma.py --verify-only
```

Le manifeste unique est `scripts/models.json`. Les modèles ne sont pas suivis dans Git.
Le dossier de build par défaut est `build_native/` ; le lanceur utilise le binaire
publié dans `build/Release/`. `--with-cli` construit aussi l'outil de diagnostic amont.

## Distribution Windows

```powershell
python scripts/build_installer.py --skip-build --version 3.0.1
```

L'installeur unique et l'archive des sources sont dans `output/installer/dist/`.
Le Setup contient le jeu, ses ressources et ses DLL, sans modèle ni volumes `.bin`.
Après leur copie, il télécharge les 5,34 Go de Gemma dans le dossier du jeu et
vérifie son SHA-256. Un modèle déjà présent et valide est conservé. Aucune
installation de Python, CUDA Toolkit ou d'un service d'inférence chez le joueur.
La fabrication standard n'exige pas de GGUF local et `--skip-build` réutilise le
binaire publié sans recompiler CUDA.

Pour préparer explicitement un dossier portable complet hors ligne, utiliser
`python scripts/build_installer.py --skip-build --portable-only` avec le GGUF local
vérifié. Cette option remplace `output/installer/stage/` et ne produit pas de Setup.
Voir le [guide de distribution](documentation/WINDOWS_DISTRIBUTION.md).

## Vérifications et documentation

```powershell
ctest --test-dir build_native -C Release --output-on-failure
python -m unittest discover -s scripts -p "test_*.py"
python -m unittest discover -s tests -p "test_*.py"
python scripts/verify_native_llm.py --exe build/Release/liminal_cornell_renderer.exe --out C:/preuves/nouvelle-recette
```

`--help` expose les diagnostics : rendu, génération de scène, sessions headless,
annulation et sondes du modèle. `--llm-gpu-layers 0` sélectionne le CPU pour une recette.
Le jeu normal n'affiche aucun sélecteur de backend ou de mode de scène.

- [État technique](documentation/TECHNICAL_STATE.md)
- [Prendre, déposer et retrouver les objets](documentation/INTERACTIONS_ENVIRONNEMENT.md)
- [Continuité des scènes et recette](documentation/CONTINUITE_SCENES.md)
- [Correction des faces coplanaires avant le verre](documentation/CORRECTION_COPLANAIRE.md)
- [Soutiens de verre : pilote, comparaisons et limites](documentation/SOUTIENS_VERRE.md)
- [Guide de partie et archives](documentation/SINGLE_START_RESEARCH.md)
- [Bilan du runtime autonome](documentation/BILAN_RUNTIME_AUTONOME.md)
- [Index documentaire](documentation/README.md)
