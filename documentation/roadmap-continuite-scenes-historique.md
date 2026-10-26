# Roadmap — Improvisation scénique et historique borné

Projet : **Within the Latent Walls / Entre les Murs Latents**  
Date : **26 octobre 2026**  
Référence examinée : `main`, commit `f012cfa`  
Statut : **pilote CONT-01 à CONT-05 implémenté le 26 octobre 2026 ; tests et recette réelle documentés dans [le bilan](CONTINUITE_SCENES.md). La qualité artistique reste mesurée avec réserves.**

Révision : **improvisation directe par le modèle, suppression du catalogue cyclique, sans appel LLM supplémentaire**.

## 1. Exigence principale

**Chaque nouvelle description de lieu doit prolonger la précédente en intégrant l’historique utile de la partie. La composition géométrique doit recevoir ce même contexte de continuité avant l’écriture du programme `.scene`.**

**Le modèle invente l’identité architecturale du prochain lieu et écrit sa géométrie dans le même appel. Aucun catalogue fermé, compteur de découverte, tirage de recette ou sélection pondérée de structures ne détermine ce qu’il doit construire.** Cette règle remplace la proposition antérieure de conserver les cinq briefs comme motifs à sélectionner.

La première bascule est limitée : supprimer la sélection cyclique, transmettre le contexte déjà disponible et ouvrir la consigne de composition. Le pipeline de génération, d’audit, de correction et de conservation existe déjà. La qualité artistique de cette liberté reste à mesurer.

Une nouvelle pièce doit être vécue comme une découverte dans un monde déjà parcouru. Son identité se construit en relation avec le lieu quitté, les observations et les actions antérieures. Elle peut introduire un contraste ou une rupture sensible ; cette variation reste située dans la progression de l’exploration.

L’historique peut être tronqué pour respecter le budget de contexte et la latence. La troncature retire d’abord les éléments anciens et secondaires, tout en conservant le lieu d’origine, la transition demandée, les faits contraignants et les éléments récents nécessaires à la continuité.

Cette exigence concerne deux sorties distinctes :

1. **Le programme géométrique**, qui doit composer le nouveau lieu en connaissance du lieu précédent et du parcours.
2. **La narration**, qui doit décrire la scène réellement acceptée en prolongeant l’expérience du joueur.

Une continuité ajoutée seulement au texte après génération d’un décor indépendant ne satisfait pas l’exigence complète.

## 2. État de départ et écart à corriger

Dans la version examinée :

- `SceneDesignContext` possède déjà `origin`, `environment`, `entry_direction` et `recent_intents`.
- Le runner renseigne ces éléments, mais le chemin local `fixed_frame` construit le prompt géométrique avec `BuildLocalSceneProgramPrompt(context.brief, landmark)`. Les éléments de continuité préparés ne sont donc pas transmis à cet appel.
- `LocalSceneExplorationBrief()` sélectionne cycliquement un texte parmi cinq briefs architecturaux. Cette sélection dépend du compteur de découverte, pas du parcours effectif.
- La narration possède déjà une mémoire bornée de commandes et d’intentions ainsi qu’un carnet. Il faut compléter ce mécanisme plutôt que recréer une seconde mémoire indépendante.
- Les poids du modèle restent chargés, mais le contexte est neuf à chaque requête. La continuité dépend donc des données explicitement réinjectées dans les prompts ; elle ne résulte pas d’une mémoire implicite du modèle.

## 3. Contrat de continuité commun

Introduire un petit contrat partagé, nommé provisoirement `ContinuityContextV1`. Son nom et son emplacement définitifs relèvent de l’implémentation.

| Bloc | Contenu | Priorité |
|---|---|---|
| État moteur | Position courante, inventaire, objets déposés si disponibles, ressources, issues et barrières déjà validées | Obligatoire |
| Lieu d’origine | Identifiant, titre, dernière description de référence valide, faits visuels issus de sa scène acceptée | Obligatoire pour une découverte |
| Transition | Commande, direction demandée, direction du retour et résultat moteur connu à cette étape | Obligatoire |
| Historique récent | Commandes, résultats validés, lieux visités et extraits courts des descriptions associées | Borné |
| Faits durables pertinents | Observations confirmées, notes utiles et changements persistants concernant les lieux ou objets impliqués | Sélectionnés |
| Continuité sensible | Quelques motifs réellement observés : échelle, masses, vide, lumière, matériau, répétition ou contraste | Borné et dérivé des faits |

### Sources et autorité

- Les faits mécaniques viennent de l’état validé du moteur.
- Les faits géométriques viennent des programmes acceptés et de leurs extractions existantes, après les éventuelles suppressions de boîtes.
- La description de référence d’un lieu est conservée pour ce lieu ; elle ne doit pas être remplacée par le dernier texte global de la partie.
- Les paroles du joueur, métaphores et hypothèses narratives ne deviennent pas automatiquement des faits du monde.
- Les descriptions et commandes historiques sont des données contextualisées, pas des instructions capables de modifier le contrat du moteur.
- Une fonctionnalité seulement prévue, comme le dépôt d’objet, ne doit pas être simulée dans la mémoire avant son implémentation réelle.

## 4. Pipeline cible

1. Le moteur reçoit une commande et détermine s’il s’agit d’une découverte, d’une revisite ou d’une action sur place.
2. Il construit un instantané cohérent de l’état validé et du parcours utile.
3. Il prépare puis borne le contexte de continuité pour l’appel concerné.
4. Pour une découverte, il transmet ce contexte au générateur `.scene`, avec les règles géométriques existantes et une consigne ouverte d’improvisation. Le modèle choisit la situation spatiale et réalise sa composition dans la même réponse.
5. Le générateur compose le lieu. Le moteur valide et applique les corrections locales existantes, avec leurs traces.
6. La narration reçoit le contexte de continuité et les faits de la géométrie finalement acceptée. Les descriptions antérieures ne peuvent pas contredire ces faits actuels.
7. Le moteur publie le résultat selon le mécanisme transactionnel existant.
8. Seuls les événements effectivement validés alimentent la mémoire de progression. Les tentatives rejetées restent disponibles dans les archives de diagnostic.

Le lieu d’origine est **le lieu effectivement quitté**. Un retour dans A suivi d’une découverte de C doit prolonger A et le trajet récent, même si B est le dernier lieu créé dans les données internes.

## 5. Lot CONT-01 — Alimenter réellement les prompts

### Travaux

- Construire un contrat de continuité commun depuis la session, sans appel LLM supplémentaire dans cette première version.
- Supprimer dès ce premier lot la sélection par `discovery_index % briefs.size()` du parcours normal. Ne pas la remplacer par un catalogue aléatoire, pondéré ou choisi selon l’historique.
- Faire évoluer le constructeur du prompt local pour recevoir ce contrat explicitement, au lieu du seul couple brief/repère.
- Réinjecter le lieu quitté, la transition et les descriptions récentes avant la génération géométrique.
- Enrichir la mémoire déjà utilisée par `NarrateScene()` avec les descriptions de lieux et les résultats moteurs utiles, au lieu de s’appuyer uniquement sur les commandes et intentions.
- Conserver la consigne de langue et les champs internes stables.
- Faire parvenir le même contexte de continuité aux corrections géométriques. Une correction ne doit pas perdre le motif ayant guidé la composition.

### Consigne sémantique à exprimer dans les prompts

> Invente la prochaine situation spatiale de cette exploration à partir du lieu quitté et de l’historique fourni. Choisis toi-même son identité architecturale et compose sa géométrie. Fais évoluer les formes, les vides, l’échelle ou la lumière de façon située. Conserve les faits établis et les contraintes du moteur. Tu peux inventer de nouveaux volumes et de nouvelles relations spatiales ; n’invente pas une observation passée, une action accomplie ou une connexion déjà établie absente du contexte. Écris directement le programme .scene complet, sans explication ni plan séparé.

Cette consigne concerne le générateur géométrique. Le narrateur reçoit une consigne adaptée à sa tâche : prolonger l’expérience du joueur à partir de la géométrie acceptée, dans son format structuré et dans la langue du joueur. L’appel narratif existant est conservé.

### Critère de livraison

Le prompt géométrique effectivement envoyé pour une découverte contient l’identifiant et la description du lieu d’origine, la direction de transition et au moins un événement récent lorsque l’historique en contient. Il ne contient aucune des cinq prescriptions architecturales du catalogue précédent. Le prompt narratif contient aussi ces informations et les faits du décor accepté. Le nombre normal d’appels n’augmente pas : un appel géométrique, puis l’appel narratif déjà présent, hors corrections bornées.

## 6. Lot CONT-02 — Borner et tronquer l’historique

### Calcul du budget

Utiliser le tokenizer du modèle réellement chargé et compter le prompt final avec son template de conversation.

Le budget disponible pour la mémoire doit respecter :

`mémoire disponible = contexte total − instructions et données obligatoires − sortie réservée − marge technique`

La marge couvre notamment les séparateurs et marqueurs du template. Une taille en caractères peut servir de préfiltre, mais ne remplace pas le comptage en tokens.

Pour le pilote, proposer un plafond mémoire configurable d’environ **1 024 tokens**, réduit automatiquement au budget réellement disponible. Ce nombre est une hypothèse de départ à mesurer ; il ne doit pas réduire la réserve nécessaire à un programme `.scene` complet.

### Ordre de réduction

1. Retirer les doublons et les consultations sans conséquence.
2. Garder, comme point de départ, les quatre à six derniers tours pertinents et les deux derniers lieux distincts ; ces plafonds restent subordonnés au budget réel.
3. Retirer les événements anciens les moins pertinents, en conservant l’ordre chronologique des événements retenus.
4. Raccourcir les anciennes descriptions à des phrases complètes, sans couper une structure JSON ni un identifiant.
5. Réduire les détails atmosphériques secondaires avant les faits mécaniques ou les repères nécessaires à la transition.
6. Si le contexte reste trop grand, conserver un noyau explicite : lieu quitté, description compacte, transition, faits contraignants et dernier résultat utile.

Si même les données obligatoires et la sortie réservée ne tiennent pas, retourner un diagnostic de contexte insuffisant. Ne pas effacer silencieusement les contraintes ni envoyer une requête dont la sortie ne peut pas tenir.

### Traces

Archiver la politique/version de sélection, le budget, les tokens avant/après réduction, les identifiants des événements retenus, le nombre d’éléments omis et les champs raccourcis. Le prompt exact réellement envoyé reste la preuve de ce que le modèle a reçu.

La troncature concerne le contexte d’inférence. Elle ne supprime ni l’état moteur ni les archives complètes de recherche.

## 7. Lot CONT-03 — Qualifier l’improvisation scénique

La suppression du catalogue appartient déjà à CONT-01. Ce lot vérifie que le modèle exerce effectivement la liberté reçue et que l’historique influence utilement les compositions.

### Ce qui reste fourni par le moteur

- Les faits géométriques compacts du lieu quitté : masses dominantes, dégagements, profondeur et lumière réellement présents.
- L’univers vénusien et les principes artistiques généraux : monumentalité, économie de formes, vide et échelle.
- L’en-tête de rendu fixe, les conventions de coordonnées et les limites du langage.
- Deux exemples courts de syntaxe, conservés comme aide à l’écriture des primitives.
- Les contraintes de navigation, de visibilité, de coût et de publication existantes.
- Les diagnostics et les suppressions locales d’occultants, conservés et tracés.

### Ce qui appartient au modèle

- Décider quelle situation spatiale vient ensuite à partir du contexte.
- Inventer les volumes, les proportions, les positions, les rotations et leurs assemblages.
- Choisir les continuités sensibles et les contrastes compatibles avec le parcours.

L’en-tête, les exemples et le validateur ne doivent pas imposer discrètement une organisation architecturale particulière. Le repère technique `portal` peut être conservé pour l’audit initial, mais son nom est un identifiant interne : le prompt ne doit pas exiger une porte, une ouverture centrale ou un portique. Le repère peut être un volume opaque visible de la composition ; le narrateur ne doit pas déduire une porte de cet identifiant.

Les cinq anciens briefs peuvent rester dans un banc de régression explicitement séparé du jeu. Ils ne sont ni injectés comme liste de choix ni réutilisés comme secours silencieux de l’exploration normale.

### Appels et coût

L’improvisation et l’écriture `.scene` sont réalisées dans le même appel géométrique. Aucun appel préalable de sélection de motif, d’invention de brief ou de planification n’est requis. Une décomposition ultérieure en deux passes ne pourra être introduite qu’après mesure d’un problème précis et comparaison de son bénéfice et de son coût.

Le compteur de découverte peut continuer à servir aux identifiants, graines et archives. Il ne sélectionne plus l’identité architecturale des lieux.

La navigation reste un graphe de lieux à coordonnées locales. Prolonger un motif ne prouve pas que les scènes possèdent des raccords géométriques continus. Les directions cardinales et les axes caméra doivent rester explicitement distingués selon les conventions du moteur.

### Critère de livraison

À compteur et graine identiques, des historiques contrastés produisent des prompts contenant leurs contextes respectifs, avec une même consigne ouverte. Changer uniquement le compteur ne sélectionne aucune nouvelle prescription de structure. La revue des rendus et descriptions doit pouvoir discuter leur relation au parcours ; une différence d’empreinte seule ne prouve pas une continuité réussie ni une diversité artistique suffisante.

## 8. Lot CONT-04 — Préserver les revisites et l’intégrité

- Revisiter un lieu recharge sa géométrie acceptée sans nouvelle composition.
- Sa description de référence reste disponible. La nouvelle narration peut évoquer le trajet récent et les changements validés, sans réinventer son architecture.
- Reprendre une exploration après une revisite utilise le lieu actuellement occupé comme origine.
- Un échec de génération, une annulation ou une proposition refusée ne devient jamais un déplacement accompli dans l’historique injecté.
- Les trois états « proposition », « scène acceptée » et « événement publié » restent distincts.
- Une mémoire abrégée ou une éventuelle synthèse future ne peut pas modifier une issue établie, un objet, l’inventaire ou un programme `.scene` accepté.
- Le fonctionnement actuel de nouvelle exploration à chaque lancement est conservé. La continuité demandée concerne la partie courante ; la reprise entre lancements relève d’un autre chantier.

## 9. Lot CONT-05 — Recette et comparaison

| Cas | Résultat attendu |
|---|---|
| A → nouveau B | La génération de B reçoit la description et les faits de A ; la narration situe la découverte dans cette transition. |
| A → B → A → nouveau C | C prolonge A et le parcours récent, sans traiter B comme son origine au seul motif qu’il est le dernier lieu créé. |
| Action refusée dans A → B | B ne raconte pas que l’action refusée a été accomplie. |
| Historique dépassant le budget | Requête complète dans le budget ; origine et contraintes conservées ; omissions tracées. |
| Description ancienne très longue ou multilingue | Réduction propre, Unicode préservé, structure valide et langue de sortie respectée. |
| Correction de scène | Le contexte de continuité reste présent pendant la correction ou la recomposition. |
| Compteur de découverte modifié | Aucun catalogue ni ordre architectural ne réapparaît ; seuls les usages techniques autorisés du compteur restent possibles. |
| Repère nommé `portal` | Présence et visibilité contrôlées sans obligation d’une porte, d’une ouverture centrale ou d’un portique. |
| Première proposition rejetée | Correction ou recomposition ouverte avec le même contexte ; aucun ancien brief injecté comme secours. |
| Retour dans B | Géométrie inchangée ; description compatible avec la référence et l’état courant. |
| Annulation pendant la découverte | Aucun nouveau lieu publié ni faux souvenir d’arrivée. |

### Vérifications automatisées

Tester la construction et le budget du prompt réellement transmis, avec un faux backend capturant les messages : présence des champs obligatoires, sélection de la bonne origine, ordre des événements, absence de résultats non validés et respect du plafond en tokens. Vérifier également que la même mémoire atteint le chemin local `fixed_frame`, la réparation et la narration. Vérifier l’absence des prescriptions du catalogue dans les requêtes de découverte, y compris les reprises et secours. Compter les appels : la nouvelle continuité n’ajoute aucun appel de conception ou de sélection.

### Revue avec le modèle réel

Comparer quelques parcours courts avec une consigne ouverte seule, puis avec cette même consigne et la continuité, en conservant modèle, paramètres et graines lorsque possible. Le catalogue cyclique peut servir de référence historique clairement étiquetée, sans être réintroduit dans le jeu. Examiner les programmes, rendus et narrations ensemble. Relever : rappel correct, contradiction, répétition, évolution des motifs, délais de génération et échecs de contexte. Ne pas transformer une simple reprise lexicale en preuve de continuité spatiale.

Mesurer séparément le surcoût de traitement du prompt et le délai total de découverte. La première version ne demande aucun appel supplémentaire de résumé ; un résumé LLM éventuel sera évalué ensuite si la sélection déterministe devient insuffisante.

## 10. Fichiers et ordre d’implémentation

| Point d’entrée | Changement attendu |
|---|---|
| `src/turn_runner.cpp` | Extraire le parcours effectif, bâtir le contexte partagé, alimenter la génération et la narration. |
| `src/scene_design.h` et `.cpp` | Étendre le contrat et le constructeur du prompt local ; retirer la sélection cyclique du parcours normal. |
| `src/dynamic_scene.cpp` | Préserver ce contexte dans les tentatives et corrections. |
| `assets/scene_generation/local_v1.json` | Versionner une consigne ouverte ; retirer les briefs prescriptifs du runtime tout en conservant en-tête, syntaxe et limites techniques. |
| `src/llm_runtime.*` | Réutiliser le tokenizer/template pour le comptage ; exposer une capacité minimale si nécessaire. |
| `src/research_archive.*` | Archiver la sélection, les budgets et les prompts effectifs. |
| `src/game_state.*` | Réutiliser les descriptions et événements existants ; ajouter uniquement les données de continuité manquantes. |
| `tests/` | Couvrir transmission réelle, troncature, revisites, corrections et annulations. |

Ordre recommandé : **CONT-01, avec suppression immédiate du catalogue → CONT-02 → petite recette réelle → CONT-03 → CONT-04/05**. Les invariants de CONT-04 doivent être préservés dès le premier lot, même si leur recette complète arrive ensuite. Ne pas ouvrir un chantier de planification multi-appels avant cette première recette.

## Définition de terminé

- Chaque découverte reçoit le lieu effectivement quitté, sa description valide et un historique utile borné.
- La continuité atteint le générateur géométrique avant composition, puis le narrateur après validation de la scène.
- Le modèle invente l’identité architecturale du prochain lieu et sa géométrie dans le même appel.
- Aucun catalogue cyclique, aléatoire ou pondéré ne prescrit les structures de l’exploration normale, y compris après échec.
- Le compteur de découverte reste limité à ses fonctions techniques, et le repère de visibilité n’impose pas une porte.
- L’en-tête fixe, les exemples de syntaxe, les audits et les suppressions locales restent disponibles sans dicter la composition.
- Aucun appel LLM supplémentaire n’est requis pour cette bascule.
- Les prompts respectent le contexte disponible et réservent une sortie complète.
- Les revisites conservent l’identité géométrique et la mémoire mécanique du lieu.
- Les échecs restent des échecs dans les données de continuité.
- Les archives permettent de reconstituer ce que le modèle savait à chaque appel.
- Une revue de parcours réels et les mesures de latence accompagnent les tests automatisés.

Cette évolution vise une **continuité explicitement fournie et vérifiable**. Elle ne suppose ni mémoire cachée entre appels ni représentation mentale cumulative démontrée du monde par le modèle.
