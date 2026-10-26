# État technique — 26 octobre 2026

## Runtime actuel

C++17, SDL3/SDL_ttf, raytracer CPU et llama.cpp lié dans le processus.
Gemma 4 E4B Instruct Q4_K_M, artefact unique défini par `scripts/models.json`.
Vendor épinglé : `620d2225cc436ab0b001e3200e590e3b286ef9ab`.
Aucun service d'inférence n'est nécessaire au jeu.

Les poids sont vérifiés par SHA-256 au premier chargement, puis conservés
jusqu'à la fermeture. Les appels sont sérialisés ; contexte, cache KV, sampler
et parseur sont recréés à chaque requête. Il n'y a donc pas de reprise implicite
d'historique entre requêtes ou parties. Le template Gemma demande une réponse
sans réflexion ; le parseur amont et un garde empêchent d'afficher des canaux privés.
Les propositions structurées utilisent un JSON Schema converti en grammaire.
Annulation coopérative, délais, budgets de tokens et de contexte restent bornés.

Le démarrage sans argument ouvre SDL, demande la langue puis lance directement
le parcours local. Les ressources sont résolues depuis l'exécutable ; les sorties
diagnostiques explicites restent relatives au dossier courant de l'appelant.
Le [bilan de migration](BILAN_RUNTIME_AUTONOME.md) distingue les vérifications
réelles des limites de qualification restantes.

## Jeu et recherche

- Un seuil de carrière prédéfini, inspiré du concept 12 ; tous les autres lieux sont inventés pendant la partie.
- Trois compositions au maximum, normalisation syntaxique conservatrice et audit strict de caméra.
- Visibilité 256 × 128, profondeur et ID d'objet ; suppression locale bornée des occultants.
- Géométrie acceptée immuable, liens réciproques et retours garantis par le moteur.
- Narration, propositions d'issues et chaleur par le modèle, puis validation autoritative.
- Température corporelle couplée à la température d'échantillonnage des prochains appels.
- Inventaire déterministe : trois outils conservés, consultation et examen sans inférence ni évolution thermique.
- Six langues sélectionnées au lancement ; consigne répétée à chaque appel et secours localisés.
- HUD compilé et traduit : initiales cardinales, températures alignées, cadres ouverts épais et lumineux, fermés fins et additifs.
- Nouvelle graine et exploration à chaque lancement ; aucun chargement d'archive dans SDL.
- Archives de recherche automatiques : descriptions, `.scene`, images, essais rejetés, profondeur/ID, prompts, métriques et états.

La visière utilise `assets/masque-scaphandre-interieur-v3.png` : texture RGBA
chargée une fois par fenêtre, alpha classique et mise à l'échelle linéaire,
superposée à la scène avant le HUD. Les températures et la boussole sont remontées
pour dégager le joint inférieur. Le dessin procédural du masque est supprimé.
Le chargement PNG requiert SDL 3.4 minimum (version fournie : 3.4.12).
Le post-traitement périphérique, les huit vues de respiration, les matériaux
diélectriques et la lumière distante restent inchangés.
Les parcours hybrides et en deux passes sont des diagnostics explicites ;
ils ne remplacent pas le parcours local normal.

## Distribution et construction

Build développeur : `scripts/build_native.py --publish`. Modèle préparé par
`download_gemma.bat`, puis contrôlé par `--verify-only`. Le joueur reçoit le modèle.
Le [paquet Windows](WINDOWS_DISTRIBUTION.md) comprend le GGUF, les ressources,
les DLL redistribuables et les notices. Dossier portable puis Setup accompagné
de ses volumes locaux ; aucune récupération réseau pendant l'installation.

Les anciens transports, manifestes et téléchargements du produit ont été supprimés.
Les qualifications dépassées ont été retirées de la documentation courante et
conservées dans l'[archive de recherche](HISTORIQUE_RECHERCHE.md), ainsi que dans Git.
Le kit externe du duo reste distinct du runtime livré et n'est pas distribué.
