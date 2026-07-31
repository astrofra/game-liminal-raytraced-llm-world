# Known Issues

Derniere mise a jour : 2026-07-31

## Ouverts

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

### 2026-07-31 - Les exterieurs vastes et le ciel sombre sont mal representes par `scene v1`

Statut :

Ouvert.

Description :

Le format `scene v1` actuel sait bien decrire des interieurs simples a base de `plane` et `box`, mais il n'offre pas encore de mecanisme propre pour representer un ciel, un degrade de fond, un horizon stable ou un exterieur desertique vaste.

Impact :

- risque de rendre le portail d'entree trop interieur ou trop abstrait
- risque de rendre le toit du datacenter peu lisible comme poste d'observation
- difficulte a faire exister la reference au desert sans bricolages de composition

Piste :

Executer d'abord la batterie de tests de `SPATIAL_VALIDATION_PLAN.md`, puis decider si une extension minimale du format est necessaire.

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
