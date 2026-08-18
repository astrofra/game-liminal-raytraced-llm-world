# Known Issues

Derniere mise a jour : 2026-08-18

## Ouverts

### 2026-08-18 - L'installeur Windows n'est pas encore signe ni valide sur une machine vierge

Statut :

Ouvert, non bloquant pour les builds internes.

Description :

Le setup `0.1.0` compile, son payload sans modele demarre et expose correctement `llama.cpp` avec CUDA. L'installation complete n'a volontairement pas ete executee localement, car elle declenche le telechargement de 5.20 Go. Le binaire de setup n'a pas encore de signature Authenticode.

Impact :

- Windows SmartScreen peut afficher un avertissement lors d'une distribution publique
- il reste a confirmer le parcours complet sur un Windows 10/11 x64 sans Visual Studio ni CUDA Toolkit
- un pilote NVIDIA compatible reste un prerequis systeme et n'est pas embarque

Piste :

Signer le setup et le binaire, publier leurs SHA-256, puis effectuer un test d'installation/desinstallation sur une machine virtuelle propre avec reprise d'un telechargement interrompu et lancement d'une premiere session.

### 2026-08-18 - Le labyrinthe Eryx n'accepte pas encore de mutations proposées par le LLM

Statut :

Ouvert.

Description :

La première route Eryx, une barrière invisible typée et un retour impossible sont implémentés. Cette topologie est cependant auteurisée et installée de manière déterministe. Le contrat de tour ne possède pas encore de proposition topologique séparée avec :

- relation visée et ancienne relation
- type de mutation
- preuves joueur requises
- décision `accepted`, `adjusted`, `rejected` ou `deferred`
- raison de validation et historique persistant

Impact :

- la tranche actuelle valide l'incident de référence, pas un labyrinthe générativement mutable
- la fréquence, la récupération et la provenance des mutations ne peuvent pas encore être testées
- le LLM reste interprète et narrateur, sans autorité topologique effective

Piste :

Ajouter un `TopologyProposal` borné, un validateur déterministe et un journal de décisions avant d'autoriser les mutations live.

### 2026-08-18 - La boucle thermique LLM reste une calibration artistique

Statut :

Ouvert, non bloquant.

Description :

La température corporelle fournit une composante d'échantillonnage comprise entre `0.10` et `0.90`. Elle est utilisée directement par défaut ou ajoutée comme dérive à une température CLI explicite, avec un plafond effectif de `1.50`. Les déplacements déterministes déclenchent aussi un court appel LLM supplémentaire pour décider de l'évolution thermique. Ces valeurs servent le rythme génératif du prototype ; elles ne constituent ni une simulation physiologique ni un modèle atmosphérique de Vénus.

Impact :

- un déplacement canonique peut désormais payer une petite latence d'inférence même sans génération de lieu
- si l'appel thermique échoue ou renvoie un JSON invalide, les deux températures restent inchangées
- un transcript déjà sauvegardé conserve en interne la langue dans laquelle chaque ancien tour a été écrit ; en interface française, une ancienne narration détectée comme anglophone est masquée par une notice française, mais elle n'est pas réellement retraduite

Piste :

Mesurer la latence et la dérive sur une session longue, puis ajuster la courbe corporelle, les bornes par tour et éventuellement la fréquence des évaluations thermiques sans donner au modèle d'autorité topologique.

### 2026-08-18 - Certains noms membres de ressources restent liés à la phase datacenter/désert

Statut :

Partiellement résolu.

Description :

`spatial_entropy`, `external_temperature_c` et `body_temperature_c` ont maintenant des membres C++ distincts et des clamps adaptés. Le loader accepte encore l'ancienne clé `datacenter_temperature_c` comme alias de sauvegarde pour l'entropie. Les membres suivants restent toutefois nommés selon les phases précédentes :

- `cooling_state`
- `water_state`
- `power_state`
- `desert_state`

Impact :

- dette de lecture et risque d'erreur lors des prochains changements de ressources
- la compatibilité de lecture des anciennes sauvegardes dépend encore de quelques alias explicites

Piste :

Renommer les quatre membres restants lors d'une migration de schéma versionnée et conserver des tests explicites de chargement des sauvegardes legacy.

### 2026-08-18 - Les verbes d'arpentage ne produisent pas encore de marques persistantes

Statut :

Ouvert.

Description :

Le parcours fournit une ardoise, de la craie, des pylônes et des scanners, mais les actions `mark`, `survey`, `compare` ou `scan` ne créent pas encore un objet de preuve structuré et persistant. La découverte de la barrière est actuellement déclenchée par le mouvement nord.

Impact :

- le joueur peut vivre la contradiction mais pas encore construire méthodiquement sa propre démonstration
- la boucle `mesurer -> marquer -> retracer -> comparer` reste surtout narrative

Piste :

Ajouter un petit registre de mesures et de marques relié aux lieux, aux directions et au tour d'observation.

### 2026-08-03 - La derive cachee du monde reste locale et ne reconcilie pas encore les boucles spatiales

Statut :

Ouvert.

Description :

Le runtime maintient une pose cachee `world_x/world_z` et des bandes qualitatives Eryx (`survey camp`, `inner quarry`, `quarry seam`, `outer shelf`, `open venus`) pour guider la generation de salles et la transition vers le champ ouvert.

Cette derive reste volontairement simple :

- elle se propage de proche en proche via les liens cardinaux
- elle permet deja de distinguer le cote de l'entree du cote oppose
- elle ne tente pas encore de fusionner proprement deux chemins differents qui devraient retomber sur une meme zone physique

Impact :

- l'impression spatiale est meilleure qu'un simple arbre de salles abstrait
- mais la coherence globale reste seulement "vaguement euclidienne"
- a moyen terme, deux branches eloignees pourraient encore decrire des voisinages incompatibles sans mecanisme de reconciliation

Piste :

Ne pas chercher automatiquement à rétablir un monde globalement euclidien. Pour Eryx :

- rendre les contradictions explicites dans un état topologique et un historique de mutation
- préserver la cohérence locale et les repères d'un lieu revisité
- benchmarker le seuil auquel les boucles incompatibles deviennent frustrantes
- réserver la relaxation ou fusion globale aux relations que le hard state exige réellement

### 2026-08-03 - Le vocabulaire de `scene_constraints` reste trop libre

Statut :

Ouvert.

Description :

La chaine hybride `scenario -> metadata LLM -> compilateur de scene` fonctionne mieux avec `scene_constraints`, mais le champ accepte encore des libelles quasi libres.

Aujourd'hui, cela aide l'exploration rapide, mais laisse aussi passer :

- des variantes redondantes pour une meme intention de decor
- des cues trop vagues pour produire un placement stable
- des nuances lexicales que le compilateur traite de facon opportuniste plutot que strictement semantique

Impact :

- stabilite visuelle encore variable d'une salle a l'autre
- difficultes a benchmarker proprement la relation entre metadata et rendu
- risque que le prompt "apprenne" de mauvais synonymes plus vite que le compilateur n'apprend a les interpreter

Piste :

Resserrer plus tard `scene_constraints` autour d'un petit vocabulaire controle, versionne et documente, par exemple :

- masses dominantes (`rack_bank`, `hero_ai_server`, `central_console`, `cooling_flank`)
- ouvertures / seuils (`rear_hatch`, `checkpoint_gate`, `service_door`)
- biais de composition (`keep_corridor_clear`, `open_horizon`, `perimeter_crates`)

L'objectif serait de garder un champ expressif, mais nettement moins libre, afin d'ameliorer a la fois la stabilite esthetique et l'audit du pipeline.

### 2026-08-02 - Un futur passage a un rendu spectral risque de casser l'economie CPU du prototype

Statut :

Ouvert.

Description :

Le renderer actuel travaille sur une palette RGB tres contrainte. Le verre des cristaux simule une dispersion à trois bandes par canal héroïque et une absorption volumique RGB selon Beer–Lambert, mais il ne calcule ni spectre continu, ni diffusion interne, ni caustiques focalisées. Cette approximation peut produire du bruit chromatique aux faibles nombres de samples. Un passage à un rendu spectral ferait surtout monter le cout des materiaux, du shading, des milieux et de la bande passante memoire, meme si les intersections geometriques et le BVH resteraient globalement du meme ordre.

Ordres de grandeur a garder en tete :

- rendu spectral naif avec `12` longueurs d'onde : environ `4x` le cout du calcul couleur par rapport a `RGB`
- rendu spectral naif avec `30` longueurs d'onde : environ `10x` le cout du calcul couleur
- sur un path tracer complet, le surcout total reel serait souvent plutot de l'ordre de `2x` a `8x`, selon la part prise par les intersections par rapport au shading
- une approche type `hero wavelength` ou quelques longueurs d'onde par chemin pourrait rester vers `1.2x` a `3x`, mais avec plus de bruit et parfois plus de samples par pixel pour converger

Approximations actuelles à conserver comme limites explicites : les rayons caméra suivent Snell, Fresnel et la réflexion totale interne avec trois IOR RGB jitterés et un filtre d'épaisseur. Les rayons d'ombre reçoivent le même filtre RGB, mais traversent les interfaces en ligne droite. Cette solution donne des franges réfractives et une ombre transmissive stable sans prétendre résoudre un spectre continu ou les caustiques d'un prisme.

Impact :

- risque de contredire l'hypothese centrale du projet selon laquelle l'image doit rester nettement moins couteuse que l'inference
- risque de rendre le budget CPU du raytracing trop visible dans la boucle interactive
- risque d'ouvrir une complexite supplementaire sur les textures, materiaux et futures mesures de performance

Piste :

Si un besoin visuel reel apparait plus tard (dispersion continue, absorption spectrale plus juste, iridescence, caustiques colorees), preferer d'abord une approche spectrale parcimonieuse et benchmarker separement :

- intersections
- shading / BSDF
- bande passante memoire
- convergence en samples par pixel

### 2026-08-02 - La generation directe `.scene` peut inventer des proprietes hors grammaire

Statut :

Ouvert.

Description :

Le benchmark direct `SCENE_GENERATION_BENCHMARK.md` montre que `Ministral` ne se contente pas de parfois tronquer une scene : il peut aussi produire une scene complete mais hors contrat `scene v1`.

Exemple observe :

Cas `loading_dock_dust` du `2026-08-02` :

- tentative de `prefab_gate "shutter"` avec une `size(...)` a seulement deux dimensions
- invention de proprietes non supportees comme `open(0.5)` sur `prefab_gate`
- invention de `bent(0.7)` sur un `box`
- tentative de `noise(0.25)` sur une primitive `plane`

Impact :

- la scene n'est pas tronquee, mais elle reste irrecevable par le parseur
- ce type d'echec ne se corrige pas par simple augmentation du budget de tokens
- il revele une derive semantique du modele vers un format plus riche que le sous-ensemble reellement implemente

Comportement actuel :

- le parseur refuse la scene des qu'une directive ou une signature sort de la grammaire supportee
- aucun mode de repli ne reecrit aujourd'hui ces proprietes vers une forme admissible
- le benchmark les classe donc a juste titre comme `invalid_scene`

Piste :

Plus tard, choisir explicitement entre :

- enrichir progressivement `scene v1` pour absorber quelques affordances frequentes
- resserrer encore le prompt pour interdire plus brutalement les extensions implicites
- ajouter un petit reparateur syntaxique / semantique qui supprime ou projette les proprietes inconnues
- garder ce comportement strict si l'objectif principal reste la lisibilite et l'audit du contrat

### 2026-08-01 - Les scenes `.scene` candidates du LLM peuvent arriver tronquees et sont rejetees en bloc

Statut :

Ouvert.

Description :

Les sorties `.scene` generees par le LLM peuvent parfois se terminer abruptement, y compris au milieu d'une directive.

Exemples observes :

- fin de ligne coupee en plein identifiant ou en pleine propriete, par exemple `box "met`
- fin de ligne coupee au milieu d'une directive valide, par exemple `spotlight panel(0.5,0.5) offset(0.0,0`

Impact :

- la scene candidate peut sembler "presque correcte" visuellement dans le terminal, mais rester inexploitable pour le moteur
- le parseur `scene v1` n'essaie pas aujourd'hui de conserver un prefixe sain puis d'ignorer la queue corrompue
- la scene candidate est donc invalidee en bloc des qu'une directive est syntaxiquement incomplete ou inconnue

Comportement actuel :

- `LoadSceneFromSceneText(...)` parse ligne par ligne
- `ParseSceneV1Directive(...)` echoue a la premiere directive invalide
- il n'y a pas de mode "best effort" qui ignorerait le reliquat tronque

Consequence runtime :

- pour une nouvelle salle improvisee, le runtime ne depend plus d'un `.scene` libre :
  - `turn_runner.cpp` demande au LLM seulement la metadata JSON de la salle
  - `scene_compiler.cpp` reconstruit ensuite une scene `hybrid` deterministe a partir du `SpatialState`
  - si ce compilateur hybride echoue, le pipeline bascule sur `BuildFallbackGeneratedRoomSceneText(...)`
- le raytraceur ne rend donc pas une scene tronquee en runtime : il rend soit une scene hybride valide, soit une scene fallback deterministe
- pour un tour standard avec `candidate_scene_text`, une candidate invalide est simplement ignoree et le rendu retombe sur `LoadSceneForPlace(...)`, sauf si une candidate valide a explicitement ete retenue

Nuance :

Si la troncature tombe exactement apres une ligne complete, la scene peut encore charger correctement. Le probleme apparait surtout quand la coupure tombe au milieu d'une directive.

Piste :

Plus tard, choisir explicitement entre :

- garder le comportement actuel fail-closed, simple et robuste
- tenter un mode de recuperation qui conserve seulement le prefixe syntaxiquement valide
- imposer des garde-fous de generation plus stricts pour les `.scene` longs
- separer encore mieux la scene debug brute et la scene effectivement admissible par le moteur

### 2026-08-01 - Les narrations peuvent etre tronquees de maniere abrupte

Statut :

Ouvert.

Description :

La boucle de tour impose aujourd'hui des descriptions courtes par deux mecanismes cumules :

- le prompt demande explicitement une prose breve
- un garde-fou runtime retaille ensuite certaines sorties et ajoute `...` si la narration depasse les bornes locales

Impact :

- certaines descriptions finissent abruptement au milieu d'une observation
- l'effet est visible surtout sur les salles generees riches en details actionnables
- la coupe peut sembler artificielle meme quand le contenu LLM etait encore coherent

Cause actuelle :

La contrainte de concision ne repose pas seulement sur le modele. Elle est aussi imposee proceduralement dans `src/turn_runner.cpp` via `ConstrainNarrationText(...)`.

Piste :

Revenir plus tard sur l'equilibre entre :

- concision demandee au prompt
- limite dure appliquee au runtime
- futur scroll de transcript cote SDL3
- eventuelle distinction entre narration joueur et log debug plus complet

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

### 2026-07-31 - La boucle de tour existe mais son contrat reste fragile et incomplet

Statut :

Ouvert.

Description :

La boucle runtime `commande -> prompt -> resultat structure -> etat -> compilation de scene -> rendu` est maintenant executee en headless et dans SDL3. Elle possede une reparation JSON et un fallback no-op, mais le contrat ne relie pas encore de facon assez robuste :

- l'etat actionnable du monde
- la prose retournee au joueur
- la description spatiale et topologique utile
- la scene effectivement rendue

Impact :

- la generation directe de scene complete par le LLM reste trop fragile comme voie principale
- la continuite inter-tour fonctionne sur une premiere tranche, mais les JSON mal formes declenchent encore des fallbacks
- `scene_constraints` reste trop libre
- le contrat ne sait pas encore exprimer ni valider les mutations topologiques Eryx

Piste :

Resserrer le schema et ses vocabulaires, conserver la compilation hybride comme voie principale, puis ajouter un contrat topologique separe avec validation et provenance explicites.

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

## Résolus

### 2026-08-18 - Le projet et son script de lancement conservaient des titres provisoires

Statut :

Résolu le 2026-08-18 après sélection collective du titre bilingue.

Résolution :

- titre anglais actif : *Within the Latent Walls*
- titre français actif : *Entre les Murs Latents*
- fenêtre SDL et écran de langue raccordés à la nouvelle identité
- helper renommé `play_within_the_latent_walls.bat`
- exemples de lancement et documents actifs mis à jour
- *Le Désert des tokens* maintenu uniquement dans les passages qui documentent explicitement la branche narrative historique

### 2026-08-18 - Une salle générée en mode français pouvait encore exposer de l'anglais

Statut :

Résolu le 2026-08-18 pour les nouveaux tours et protégé à l'affichage pour les sauvegardes existantes.

Résolution :

- règle française obligatoire renforcée dans les messages système de génération de salle et de tour
- détection lexicale d'une réponse joueur restée majoritairement anglaise
- seconde passe de localisation LLM à température nulle, avec fallback français si cette passe échoue
- directions développées dans la langue sélectionnée, par exemple `N` vers `NORD`
- ancien titre anglais remplacé à l'affichage par un libellé français neutre et ancienne narration anglaise masquée par une notice française

### 2026-08-18 - Les prompts, lieux canoniques et fallbacks restaient liés au datacenter

Statut :

Résolu le 2026-08-18 pour la première tranche jouable Eryx.

Résolution :

- sept identifiants et fixtures canoniques Eryx
- prompts, schémas JSON, états initiaux, HUD, bandes cachées et fallbacks migrés
- huit archétypes de layout et intégration contextuelle des neuf prefabs actifs
- conservation explicite des fixtures et directives datacenter comme baselines historiques

### 2026-08-18 - Les barrières invisibles étaient confondues avec `blocked_exits`

Statut :

Résolu le 2026-08-18 pour le contact déterministe de référence.

Résolution :

`InvisibleBarrier` possède un lieu, une direction, une preuve et un statut de découverte persistants. Le mouvement reconnu peut être refusé sans géométrie visible, sans changement de lieu et sans incrément de `move_count`. La narration distingue explicitement ce cas d'un échec de parser ou d'un obstacle visible.

### 2026-08-09 - Les prefabs et leur catalogue restaient sémantiquement liés au datacenter

Statut :

Résolu le 2026-08-09 pour le vocabulaire visuel et le catalogue. La migration globale du runtime reste suivie séparément.

Résolution :

- refonte source de `prefab_gate` et `prefab_crate` pour la carrière et les containers d'échantillons
- ajout de `prefab_survey_beacon`, `prefab_crystal_scanner`, `prefab_crystal_cluster`, `prefab_extraction_rig`, `prefab_prospect_shelter`, `prefab_quarry_pylon` et `prefab_atmospheric_processor`
- raccordement de ces directives au parseur `scene v1`, au prompt d'audit et à la reconnaissance lexicale du compilateur hybride
- régénération de `PREFAB_CATALOG.md`, des scènes sources et de dix-huit vues depuis le renderer réel
- maintien des racks, unités de refroidissement et serveurs IA comme directives legacy hors du catalogue actif
