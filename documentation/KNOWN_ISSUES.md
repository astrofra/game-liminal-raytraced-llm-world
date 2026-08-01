# Known Issues

Derniere mise a jour : 2026-08-01

## Ouverts

### 2026-08-01 - Un dossier litteral `%TEMP%` peut apparaitre a la racine du depot sous Windows

Statut :

Ouvert, mais cause racine probable identifiee.

Description :

Un dossier litteral `%TEMP%` a ete observe a la racine du depot, avec un sous-dossier `%TEMP%\\liminal_sdl3_inspect`.

Les verifications locales montrent que ce sous-dossier contient un clone Git superficiel du depot `https://github.com/libsdl-org/SDL.git`, positionne directement sur le tag `release-3.4.12`.

Les points suivants ont ete confirmes :

- le helper tracked `build_release.bat` n'utilise pas `%TEMP%`
- le `CMakeLists.txt` tracked ne cree pas ce chemin
- `.gitignore` ignore deja `/%TEMP%`, ce qui montre qu'il s'agit d'un artefact local deja rencontre
- le clone parasite a ete cree le `2026-07-31 16:30:52 +0200`

Cause probable :

Mauvais dialecte de variable d'environnement sur Windows.

Inference la plus probable :

Une commande de type PowerShell a utilise une cible comme `%TEMP%\\liminal_sdl3_inspect` au lieu de `$env:TEMP\\liminal_sdl3_inspect`.

Dans ce cas, PowerShell n'expanse pas `%TEMP%` et cree un vrai dossier relatif nomme litteralement `%TEMP%` dans le dossier courant.

Commande probable, inferee a partir du contenu du clone :

```powershell
git clone --branch release-3.4.12 --depth 1 https://github.com/libsdl-org/SDL.git %TEMP%\liminal_sdl3_inspect
```

Impact :

- pollution du workspace avec un depot parasite
- confusion possible sur l'origine reelle des artefacts de build
- risque de croire a tort que `FetchContent` ou le build principal ecrivent hors de `build/`

Correctifs / prevention :

- PowerShell : utiliser `$env:TEMP`
- `cmd` / `.bat` : utiliser `%TEMP%`
- CMake : utiliser `$ENV{TEMP}`
- Python : utiliser `tempfile.gettempdir()`

Nettoyage local si le dossier parasite est inutile :

```powershell
Remove-Item -Recurse -Force '.\%TEMP%'
```

### 2026-07-29 - Fireflies et bruit encore trop agressifs

Statut :

Ouvert.

Description :

Le renderer produit encore des contributions extremes visibles sous forme de points tres clairs.

Impact :

- nuit a la lisibilite
- complique l'evaluation esthetique reelle du grain souhaite
- masque partiellement la radiosite simple

Causes probables :

- estimateur de lumiere directe tres rudimentaire
- absence de MIS
- clamp de contribution trop simple

Pistes :

- ameliorer l'echantillonnage de lumiere
- separer mieux bruit voulu et valeurs aberrantes
- tester un clamp plus fin ou une strategie de normalisation plus defendable

### 2026-07-30 - Le spotlight camera demande encore une calibration esthetique

Statut :

Ouvert.

Description :

Le spotlight camera est implemente, mais son cone, sa portee et son intensite restent a calibrer finement pour obtenir l'esthetique oppressive visee sans eclairer la scene de maniere trop uniforme.

Impact :

- l'intention architecturale est maintenant respectee
- mais le rendu final peut encore sembler trop plat ou trop large selon la scene et le cadrage

Piste :

Iterer sur les parametres du spotlight et sur la composition des scenes proprietaires.

### 2026-07-29 - Format de scene v1 encore partiel

Statut :

Ouvert.

Description :

Le depot sait maintenant charger un format de scene proprietaire v1, mais seulement pour un sous-ensemble restreint.

Impact :

- le pipeline scene proprietaire existe
- mais il ne couvre pas encore tout le vocabulaire prevu par la spec
- la connexion future a un LLM resterait fragile sans validation plus forte

Piste :

Etendre progressivement le support a d'autres primitives et renforcer la validation.

### 2026-07-31 - Les exterieurs vastes restent fragiles malgre le nouveau fond `sky`

Statut :

Ouvert.

Description :

Le fond proceduriel `sky` ameliore nettement le ciel, l'horizon et la lecture des scenes exterieures. En revanche, le format `scene v1` reste encore pauvre pour decrire un exterieur desertique vaste avec peu de primitives semantiques.

Impact :

- le ciel lit mieux qu'avant, mais le desert repose encore sur peu de masses
- le toit reste credible comme poste d'observation, mais le dehors est encore tres abstrait
- l'absence de prefabs ou d'objets stables limite la reconnaissance immediate de certains lieux

Piste :

Executer la batterie de tests de `SPATIAL_VALIDATION_PLAN.md`, puis ajouter soit des prefabs stables, soit un petit vocabulaire spatial supplementaire si la composition simple reste insuffisante.

### 2026-07-31 - Les prefabs actuels augmentent trop vite le cout geometrique

Statut :

Ouvert.

Description :

La premiere couche `prefab_*` stabilise bien la lecture semantique du decor, mais chaque objet s'expanse aujourd'hui en plusieurs `box` avec ses propres materiaux. Cela augmente rapidement le nombre de triangles et ralentit fortement certaines scenes, surtout les travees de racks.

Impact :

- temps de rendu en hausse sur les scenes denses, surtout en mono-thread
- inflation du nombre de materiaux
- OpenMP masque partiellement le probleme sur machine multi-coeur, sans le resoudre sur le fond
- risque de perdre l'avantage de contrainte simple si la bibliotheque grossit sans discipline

Piste :

Introduire ensuite soit des materiaux partages, soit une repetition modulaire plus compacte, soit une couche d'instanciation plus sobre.

### 2026-07-31 - Le contrat de tour et le pont entre etat du monde, narration et scene sont formalises mais pas encore executes de bout en bout

Statut :

Ouvert.

Description :

Le projet sait maintenant decrire ce maillon et en poser les premiers structs et prompts, mais il ne dispose pas encore d'une boucle runtime complete qui relie de facon robuste :

- l'etat actionnable du monde
- la prose retournee au joueur
- la description spatiale utile
- la scene effectivement rendue

Impact :

- la generation directe de scene complete par le LLM reste trop fragile comme voie principale
- la continuite inter-tour n'est pas encore executee ni testee
- la validation du schema structure et l'application des deltas restent a implementer

Piste :

Brancher la premiere boucle headless `commande -> prompt -> resultat structure -> mise a jour d'etat -> compilation de scene -> rendu`, puis auditer la qualite des scenes `.scene` candidates generees en memoire.

### 2026-07-29 - Telemetrie encore insuffisante

Statut :

Ouvert.

Description :

Les temps de chargement et de rendu sont affiches, mais les mesures demandees par la spec ne sont pas encore presentes.

Impact :

- impossible de demontrer serieusement l'inversion de charge computationnelle

Manques :

- memoire
- CPU/GPU
- temps detailles par etape
- erreurs de validation de scene

### 2026-07-29 - `documentation/NOTES.md` apparait mal encode dans le terminal actuel

Statut :

Ouvert.

Description :

Le contenu semble correct sur le fond mais s'affiche avec du mojibake dans l'environnement terminal actuel.

Impact :

- gene la lecture
- peut creer de fausses alertes sur le contenu

Piste :

Verifier et normaliser explicitement l'encodage du fichier si cela devient genant pour le travail quotidien.
