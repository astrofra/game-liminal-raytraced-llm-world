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
- [Correction coplanaire](CORRECTION_COPLANAIRE.md) avant le verre : faces originales,
  intersection plane, microtranslations bornées et auditées, union des huit
  vues de respiration, rapports et étapes persistées ; aucun appel LLM ajouté.
- Géométrie acceptée immuable, liens réciproques et retours garantis par le moteur.
- Seuil ouvert au nord, à l'est et au sud. Dans le parcours local, le moteur compte
  les issues ouvertes sans voisin connu encore accessibles, hors départ consommé.
  Une cible de deux ou trois sorties, retour compris, est tirée à 50/50 et conservée
  dans la provenance de chaque nouveau lieu. Le LLM choisit les directions parmi
  les approches praticables ; le schéma JSON borne leur nombre. Le moteur complète
  ou réduit une proposition hors cible en conservant les liens fixes. Une impasse
  proposée reste admise si un autre chemin inexploré est accessible. La dernière
  branche garde une continuation, y compris en secours narratif. La géométrie peut
  limiter la cible ; le ratio est une probabilité de tirage, pas un quota exact des
  pièces visitées. Ce réglage ne modifie ni le décor, ni les consignes et audits de
  composition 3D. Les cibles, propositions et corrections sont archivées ; les
  revisites ne relancent ni tirage ni composition. Les anciennes scènes sont conservées.
- [Soutiens de verre](SOUTIENS_VERRE.md) de 0,25 m sous les composantes flottantes :
  graphe de contacts OBB, coque suivant l'emprise et la sous-face inclinée, après
  correction et validation opaques ; exclusions caméra/approches, aperçu réfracté, secours opaque
  et archives séparées. Aucun appel LLM supplémentaire ; bruit optique encore visible.
- Narration, propositions d'issues et chaleur par le modèle, puis validation autoritative.
- Température corporelle couplée à la température d'échantillonnage des prochains appels.
- Inventaire déterministe : trois outils conservés, consultation et examen sans inférence ni évolution thermique.
- Six langues sélectionnées au lancement ; consigne répétée à chaque appel et secours localisés.
- HUD compilé et traduit : initiales cardinales, températures alignées, cadres ouverts épais et lumineux, fermés fins et additifs.
- Nouvelle graine et exploration à chaque lancement ; aucun chargement d'archive dans SDL.
- Archives de recherche automatiques : descriptions, `.scene`, images, essais rejetés, profondeur/ID, prompts, métriques et états.

La [continuité scénique](CONTINUITE_SCENES.md) est désormais explicite : le même
instantané du lieu quitté, de son décor accepté et des événements publiés alimente
la composition locale, ses corrections et la narration. Le catalogue cyclique de
cinq briefs est retiré du runtime ; `portal` reste un identifiant de visibilité,
sans imposer une porte. Aucun appel de planification ou de résumé n'est ajouté.

Le budget est compté avec le tokenizer et le template Gemma réellement chargés.
Le plafond des événements/notes est de 1 024 tokens, configurable par
`--continuity-memory-tokens`, avec réserve de sortie et marge de 32 tokens.
La sélection conserve au plus six événements et deux lieux récents. Si nécessaire,
les descriptions de cette copie sont réduites à leur première phrase complète.
Les lieux, scènes, descriptions de référence, liens et historique complet restent
conservés pendant la partie. Les requêtes et décisions de réduction sont archivées.
Les tests mécaniques passent ; la revue réelle relève encore des extrapolations
littéraires et ne démontre pas une continuité artistique systématique.

La visière utilise `assets/masque-scaphandre-interieur-v3.png` : texture RGBA
chargée une fois par fenêtre, alpha classique et mise à l'échelle linéaire,
superposée à la scène avant le HUD. Les températures sont centrées horizontalement
dans l'œil gauche (25 % de la largeur), à hauteur inchangée. La boussole est centrée
dans l'œil droit (75 %), dégagée du joint droit ; son centre vertical est partagé
avec celui du bloc des deux températures. Le dessin procédural du masque est supprimé.
Le chargement PNG requiert SDL 3.4 minimum (version fournie : 3.4.12).
Le post-traitement périphérique, les huit vues de respiration, les matériaux
diélectriques et la lumière distante restent inchangés.
Les parcours hybrides et en deux passes sont des diagnostics explicites ;
ils ne remplacent pas le parcours local normal.

## Distribution et construction

Build développeur : `scripts/build_native.py --publish`. Le modèle de développement
se prépare par `download_gemma.bat`, puis se contrôle par `--verify-only`.
Le [Setup Windows](WINDOWS_DISTRIBUTION.md) 3.0.1 contient les ressources,
les DLL redistribuables et les notices, sans GGUF ni volumes séparés. Après la copie
du jeu, Inno Setup télécharge Gemma à la révision épinglée et vérifie son SHA-256.
Un modèle existant est réutilisé seulement après contrôle de taille 64 bits et SHA.
Le jeu fonctionne ensuite hors ligne. `--portable-only` reste une option explicite
de fabrication d'un dossier complet avec GGUF ; le Setup standard n'en dépend pas.
Un changement de packaging se construit avec `--skip-build`, sans rebuild CUDA.

Les anciens transports d'inférence et profils concurrents ont été supprimés.
Le téléchargement final de l'installeur reste distinct du runtime local.
Les qualifications dépassées ont été retirées de la documentation courante et
conservées dans l'[archive de recherche](HISTORIQUE_RECHERCHE.md), ainsi que dans Git.
Le kit externe du duo reste distinct du runtime livré et n'est pas distribué.
