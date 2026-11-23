# État technique — 23 novembre 2026

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

Le démarrage sans argument ouvre SDL en plein écran sans bordure à la résolution
du bureau, demande la langue puis lance directement le parcours local.
Le rendu logique reste en 1000 × 800, avec des marges noires centrées selon
l'écran. Les captures d'interaction couvrent aussi ces marges.
Les ressources sont résolues depuis l'exécutable ; les sorties
diagnostiques explicites restent relatives au dossier courant de l'appelant.
Le [bilan de migration](BILAN_RUNTIME_AUTONOME.md) distingue les vérifications
réelles des limites de qualification restantes.

## Jeu et recherche

- [Dialogue du maître du jeu](DIALOGUE_MAITRE_DU_JEU.md). Le modèle route les
  formulations libres vers la conversation, l'observation ou une action. Les
  questions utilisent une réponse courte et une mémoire d'échanges bornée en
  tokens. Le moteur conserve l'état de simulation et la vue courante pendant
  le dialogue. Après interprétation de la commande, SDL affiche la réponse en
  cours de génération sous la question. La narration validée prend la place de
  cet aperçu. Échap retire l'aperçu et annule l'interaction. Les archives SDL
  conservent la capture finale du HUD, les états avant et après et les traces
  par appel LLM.
  Les gestes sensoriels simples sur le décor suivent l'observation : le moteur
  peut écarter les métadonnées de transfert superflues d'un `perceive` valide,
  sans modifier les validations de prise, dépôt ou déplacement. Le narrateur
  répond au geste courant à travers la protection du scaphandre ; l'observation
  conserve son exposition thermique et sa mémoire habituelles. Voir le
  [correctif et sa recette](duo/reviews/DUO-067.md).

- À l'arrivée ou au retour dans un lieu, le raytracer calcule la première image
  pendant que le modèle prépare la description. Pour une nouvelle pièce, les
  deux attendent la validation de la géométrie. SDL présente d'abord la vue
  provisoire, puis la prose progressive directement après la commande, sans
  en-tête de chargement ni message de calcul dans le récit. La boussole
  reste visible, avec les quatre voies fermées et sans les repères de paroi du
  lieu précédent. Les sorties de destination s'affichent avec la validation du
  déplacement, qui publie aussi la description et la vue finales.
  Lorsqu'un fragment halluciné est demandé, la description structurée sert de
  préparation interne et l'aperçu commence avec le fragment. Échap retire
  l'aperçu et revient au lieu de départ.

- [Enquête à indices](ENQUETE_INDICES.md) : état versionné et révisé, quatre
  possibilités dont la [répartition est choisie par le LLM](REPARTITION_INDICES_LLM.md)
  dans l'appel narratif de création, après la géométrie. Schéma 2,
  décisions persistantes par lieu, choix vides et regroupements autorisés,
  aucune rotation C++ ni quota ; cristal lié
  au registre d'objets, contact invisible effectif et deux traces de Vey.
  Preuves et hypothèses distinctes, bilan interprété par le LLM puis restitué
  depuis l'état validé, six langues, publication atomique et migration sans
  preuves rétroactives. Les chaînes historiques sont des projections.
  Vey reste disparu ; aucune fin automatique. Recette Gemma et SDL en français
  effectuée, carnet défilant ; résultats et limites dans le bilan.

- [Parois repérées](PAROIS_REPEREES.md) : après contact, couche de verre calculée
  depuis les barrières découvertes du lieu et l'orientation d'origine. Face,
  gauche et droite dans le rendu ; arrière hors champ. Boussole ambrée et barrée
  pour toute direction concernée. Aucun nouveau fait, objet ou appel LLM ;
  scène acceptée inchangée, recomposition après retour et sérialisation.

- [Audio d'ambiance](AUDIO_AMBIENCE.md) : catalogue acoustique reproductible des
  quatre OGG, fiches et annotations distinctes, gains proposés dans le même appel
  narratif et publiés après acceptation du tour. Décodage stb_vorbis au chargement,
  sortie SDL3, boucles continues à gain nul, rampes par échantillon et marge sur
  les pics simultanés. Le jeu continue si l'audio est indisponible. Aucune
  attente du premier tour pour le son : le seuil pré-scripté démarre avec les
  cibles 0,1 / 0 / 0,2 / 0,3 par la même rampe de cinq secondes, dès le premier
  écran de sélection de langue, sans appel LLM. Aucune
  dépendance Python/FFmpeg au runtime. Trims à 0 dB ; validation artistique des
  jonctions et de l'équilibre encore à effectuer par l'auteur.

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
- Les raccourcis `i`, `inventaire` et `inventory` ouvrent directement l'inventaire.
  Gemma reformule le texte à la température narrative active. Le moteur vérifie
  les noms des objets portés et peut joindre la liste exacte à une phrase générée.
  La consultation conserve la température corporelle, la vue et l'animation.
- Le LLM interprète toutes les formulations libres du parcours local en un appel
  structuré : déplacement, observation, inventaire, prise/dépôt, carnet et instruments.
  Le C++ distribue les intentions, vérifie les identités, la provenance et les préconditions,
  puis publie l'état. Les raccourcis de boussole restent directs. Aucun parseur de
  compléments nominaux n'est ajouté. Voir le [second correctif de partie](RETOUR_JEU_INTERACTIONS_2.md).
- [Interactions avec les objets](INTERACTIONS_ENVIRONNEMENT.md) : dépôt/reprise par identité,
  prise littéraire validée, sources finies et décisions durables, partition autoritative
  inventaire/pièces. Couche mobile déterministe, placements visibles à travers le masque,
  annulation atomique et archives par révision. Le [retour de partie](RETOUR_JEU_INTERACTIONS.md)
  ajoute les observations persistantes, les prélèvements interprétés sémantiquement,
  la narration des transferts validés et le complément local des dépôts oubliés.
  Six langues de secours ; aucun chargement
  de partie ajouté. Les usages spécifiques de la carte, de la craie et des pinces restent prévus.
- Six langues sélectionnées au lancement ; consigne répétée à chaque appel et secours localisés.
- HUD compilé et traduit : initiales cardinales, températures alignées, cadres ouverts épais et lumineux, fermés fins et additifs.
- Nouvelle graine et exploration à chaque lancement ; aucun chargement d'archive dans SDL.
- Archives de recherche automatiques : descriptions, `.scene`, images, essais rejetés, profondeur/ID, prompts, métriques et états.

La [continuité scénique](CONTINUITE_SCENES.md) est désormais explicite : le même
instantané du lieu quitté, de son décor accepté et des événements publiés alimente
la composition locale et ses corrections. Pour une nouvelle arrivée, la narration
conserve la géométrie de départ et les événements validés, mais retire leurs anciens
paragraphes descriptifs du prompt. Son contexte d'objets est celui de la destination.
La boussole indique les sorties validées pendant l'exploration.
Le catalogue cyclique de
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

La visière utilise `assets/hud-mask.png` : texture RGBA
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

## Diagnostic thermique

Le HUD affiche le corps et l'extérieur. Avec le profil Gemma par défaut,
37 °C corporels donnent une température d'échantillonnage de 0,10 et chaque
degré supplémentaire ajoute 0,16 jusqu'au plateau de 0,90 à 42 °C.
Sans diagnostic activé, un déplacement local réussi ajoute exactement 0,5 °C,
revisites comprises, avec un plafond de 43 °C. Dix déplacements depuis 37 °C
atteignent donc 42 °C. Le moteur prépare cette valeur avant l'aperçu visuel et
la narration ; le schéma impose au modèle de la recopier. L'échantillonnage
narratif utilise cette nouvelle valeur, avec un plancher de 0,72 dès 39,5 °C.
L'interprétation et la composition géométrique utilisent encore le début du tour.
Un secours narratif ne retire pas la chauffe d'un déplacement accepté ; une
annulation ne publie ni le déplacement ni sa chauffe. Un déplacement bloqué,
une conversation et un inventaire ne reçoivent pas ce supplément. Les autres
expositions narratives conservent leur variation proposée bornée à 0,4 °C.

| Commande à saisir dans le jeu | Effet |
|---|---|
| `/chaleur` | Inspecte le profil, l'isolation, la température corporelle et le réglage du prochain appel narratif. |
| `/chaleur normal`, `/chaleur moyen`, `/chaleur extrême` | Ajoute respectivement 0, 1 ou 2,5 °C aux expositions admissibles, après validation de la proposition thermique. |
| `/isolation 0` à `/isolation 100` | Atténue la composante de chauffe de 0 à 100 %. Le refroidissement proposé reste applicable. |
| `/chaleur pas` | Simule immédiatement une exposition de base de +0,4 °C, avec le supplément et l'isolation choisis. |
| `/chaleur reset` | Ramène le corps à 37 °C avec les réglages courants. |
| `/chaleur llm 5` ou `/chaleur llm 10` | Choisit la température du fragment halluciné. La valeur par défaut est 5. |

Le diagnostic est désactivé au démarrage ; ses réglages commencent à normal et
0 % d'isolation additionnelle. L'activation explicite d'un réglage thermique
remplace la chauffe automatique par déplacement par les règles du diagnostic.
La simple consultation `/chaleur` ne l'active pas.
Les commandes directes utilisent zéro appel LLM et laissent le
temps de simulation en place. Elles recalculent la vue si la visière franchit
un palier optique. Deux `pas` en mode extrême à 0 % passent de 37 à
42,8 °C. Le corps reste borné à 34–43 °C. Dans ce diagnostic, l'interprétation
des commandes et la génération du script local utilisent le réglage de référence
à 37 °C. La description et le dialogue suivent la température corporelle réelle.
Les archives contiennent les températures avant et après et la température
d'échantillonnage de chaque appel.

La prose de scène se dérègle dès 39,5 °C, y compris en jeu normal. À partir de 42 °C,
un bref appel dédié à la prose est échantillonné à 5 ou à 10, après validation
de la proposition structurée. Il part d'un fait visible et joue avec des
inventaires, des répétitions, des comptes contradictoires et des phrases
disloquées. Si la génération échoue, la description structurée reste disponible.
Le moteur fournit les objets au sol et affiche les sorties sur la boussole. L'écriture sous
contrainte s'inspire des procédés oulipiens. [Georges Perec et l'Oulipo](https://oulipo.net/fr/Oulipiens/GP)

Au-dessus de 39 °C corporels, huit déformations gaussiennes modifient les rayons
caméra avant leur intersection avec le décor. L'effet est plus marqué à 42 °C.
Les rayons rouge, vert et bleu suivent des déviations d'amplitudes différentes.
La déformation et la dispersion s'atténuent progressivement jusqu'au bord des
gouttes.
Les gouttes restent aux mêmes coordonnées pendant la respiration dans une pièce.
Leur position se décale légèrement d'un lieu à l'autre et se retrouve à la
revisite. SDL compose le masque et le HUD sur le résultat du raytracer. Les
captures des interactions et les rendus des sessions headless incluent l'effet.

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
