# Known Issues

Derniere mise a jour : 2026-07-29

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

### 2026-07-29 - Le renderer courant ne suit pas encore le modele d'eclairage cible de la spec

Statut :

Ouvert.

Description :

La spec vise une lumiere attachee a la camera. Le bootstrap Cornell Box utilise une aire lumineuse de plafond.

Impact :

- la scene de test valide la radiosite
- mais elle n'exprime pas encore l'esthetique found-footage / brutalist visee

Piste :

Introduire un mode d'eclairage camera pour la future scene proprietaire.

### 2026-07-29 - Format de scene encore absent

Statut :

Ouvert.

Description :

Le depot sait charger un OBJ de reference, mais pas encore le futur langage de scene proprietaire base sur primitives.

Impact :

- impossible pour l'instant de connecter proprement un futur LLM a une scene validable
- le pipeline de jeu complet n'existe pas encore

Piste :

Definir un parseur et un validateur pour le format de scene minimal decrit dans `SPEC.md`.

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
