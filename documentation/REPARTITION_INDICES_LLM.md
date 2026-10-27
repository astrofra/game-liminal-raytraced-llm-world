# Répartition persistante des indices par le LLM

Implémentation de la [roadmap du 27 octobre 2026](roadmap-repartition-indices-llm.md).
Duo réel Codex / DeepSeek : [contrat et revue DUO-047](duo/reviews/DUO-047.md).
Le modèle de développement reste distinct de Gemma, utilisé par le jeu.

## Comportement

Chaque nouveau lieu est composé et audité avant le choix des indices. Le même
appel narratif de première arrivée reçoit les possibilités encore disponibles,
leurs descriptions publiques et des sites admissibles. Il produit
`investigation_placement` avec la révision et zéro à quatre couples
`opportunity_id` / `site_id`. Le relevé peut précéder les autres traces ; plusieurs
objets peuvent se trouver ensemble. Une décision vide a la même permanence.

Le sélecteur C++ et sa rotation par graine ont été supprimés. La graine influence
l’échantillonnage, sans choisir une succession d’indices. Aucun quota ni seuil
de rattrapage ne force une découverte après un nombre de lieux donné.

Le moteur offre jusqu’à trois positions au sol, espacées, soutenues et visibles.
Il vérifie aussi leur visibilité en groupe sur les huit vues de respiration.
Une obstruction utilise seulement une approche dégagée sans lien existant ;
son issue doit rester ouverte jusqu’au contact. Le retour et une autre frontière
d’exploration sont préservés. Les cartes d’issues autorisées sont contraintes
pendant le décodage, puis validées contre la sélection réellement proposée.

## Conservation et publication

Le schéma 2 `eryx-clues-llm-v2` conserve une décision par lieu : origine `llm`
ou `legacy_preserved`, identités sélectionnées et tour de publication. Les
choix antérieurs sont immuables. Les preuves restent distinctes : voir un objet
ne l’identifie pas et ne l’acquiert pas. Les contenus cachés ne figurent pas dans
les fiches d’écriture transmises à l’arrivée.

La sélection est validée avant les températures, le texte et la couverture des
objets. Absence de champ, sortie incomplète, clés répétées, révision périmée,
identité indisponible, site incompatible ou collision rejettent la création
entière. Le moteur ne substitue pas un autre indice, site ou sens de barrière.
Une sélection valide avec une prose ou une température refusée conserve ses
objets et reçoit le secours localisé. Même une liste de dépôts inventée après
une décision vide est corrigée. La dernière sonde d’annulation précède la
publication des lieux, liens, objets, décisions, preuves et cibles audio.

Les revisites et réparations de métadonnées n’incluent pas le champ de placement
dans leur contrat. Un champ inattendu ne redistribue rien. Les anciens lieux,
y compris le seuil, sont préservés lors de la migration v1 ou sans enquête.
Une nouvelle scène utilise ensuite le nouveau contrat. SDL commence toujours
une partie neuve ; cette migration ne crée pas une interface de chargement.

Les mentions littéraires d’un objet explicite sont liées à son identité.
Après déplacement, la mention initiale ne peut pas produire une deuxième
instance. Les anciens exports reconstruisent ce lien depuis leurs références
conservées. L’acquisition d’un cristal et l’examen de l’outil ne soutiennent
plus l’idée que Vey serait resté au point de prélèvement. Cette hypothèse reste
ouverte jusqu’au relevé qui la contredit, sans effacer les autres constats.

Les archives conservent demande d’écriture, proposition, préparation ou rejet,
états avant/candidat et publication. `state.json` contient les décisions
acceptées. Les faits connus restent obligatoires dans le contexte ; la mémoire
récente reste ajustée par le budget existant et son tokenizer réel.

## Vérification

Les tests injectés et les essais du modèle réel sont distincts. Les six groupes
DeepSeek ont été relus et corrigés par Codex. Les parcours supplémentaires
vérifient choix vide/simple/groupé sur une géométrie identique, ordre commençant
par le relevé, refus atomiques, annulation après préparation, correction de
métadonnées, retours, prises et dépôts, migration, hypothèses et conservation
après 45 événements. Les secours avec plusieurs objets sont vérifiés dans les
six langues. Le choix de ces fixtures n’est pas une mesure du comportement
spontané de Gemma.

Les archives privées de recette sont conservées sous
`C:/works/duo-runs/game-liminal-raytraced-llm-world/DISTRIBUTION-20261027/`.
Les journaux de compilation et du duo restent dans `DUO-047-01` au même parent.

La première série réelle a détecté un défaut du schéma d’issues : toutes les
créations déclenchaient une correction du nombre d’ouvertures, donc un secours
narratif. Le schéma a été corrigé pour inclure les cartes compatibles avec une
obstruction éventuelle ; une seconde série indépendante qualifie ce correctif.

### Résultats des tests et de la série Gemma finale

- **15/15 CTest, 114 tests Python** (4 dans `tests`, 110 dans `scripts`).
- Gemma 4 E4B Q4_K_M local, français, contexte 8192, graines 0, 1 et 2.
  Budget fixé avant la série : jusqu’à trois nouveaux lieux, au maximum six
  commandes de navigation par graine. La sélection n’est jamais forcée.
- **14 commandes, 10 acceptées : 8 créations et 2 revisites.** Quatre refus
  géométriques répètent la même tentative à la graine 0, où le repère est masqué.
  Aucune sélection n’est demandée sur ces géométries rejetées.
- Séquences publiées : graine 0 **3 puis 1** ; graines 1 et 2 **3, 1, 0**.
  Les trois objets au sol sont regroupés, puis l’obstruction est placée.
  Les deux lieux vides surviennent après épuisement du catalogue : cette série
  ne démontre pas une pause volontaire alors que des indices restent disponibles.
  Aucune piste n’est restée absente au terme du budget, mais aucun ordre
  différent n’a été observé entre les trois graines.
- Les huit créations acceptées utilisent chacune **deux appels LLM**, géométrie
  puis narration/distribution. Les revisites utilisent un appel narratif.
  Sept créations conservent la prose, éventuellement complétée par la présence
  des objets. Un secours `dynamic_object_coverage` écarte des dépôts annoncés
  dans le mauvais lieu ; il conserve l’obstruction sélectionnée.
- Prompts narratifs des créations : **5 168 à 5 716 tokens réels** ; sorties
  **421 à 582 tokens**, pour une réserve de 1 280 et une marge technique de 32.
  Les deux revisites atteignent au maximum 6 255 tokens. Aucun dépassement
  de contexte dans cette série. Les réponses examinées ne révèlent ni
  identification de Vey ni contact invisible à l’arrivée.

Les deux archives du joueur, `run-20261027T072857Z-1411155189` et
`run-20261027T101205Z-495624285`, ont été migrées vers des sorties séparées.
Lieux, liens, barrières, opportunités et tous les champs existants du registre
d’objets sont identiques. Le nouveau champ optionnel `mentions` est vide à ce
stade. Les SHA-256 des originaux sont inchangés.

Un export v1 comportant six faits acquis a ensuite produit une nouvelle
création : **6 304 tokens de prompt**, dont 5 794 obligatoires et 510 de mémoire,
avec 1 280 tokens réservés à la réponse et une marge de 32, dans 8 192.
Les six faits restent présents ; 16 événements anciens sont omis du prompt,
sans être effacés de l’état. Une revisite préalable atteint 6 606 tokens.
L’export initial du joueur sans enquête a également créé un lieu après un
retour dans un lieu ancien, en conservant intégralement les salles antérieures.

### Recette SDL française

Une nouvelle partie a accepté **14 commandes** : création, trois examens,
prise reformulée du spécimen, deuxième création, dépôt, contact invisible,
examen de l’obstruction, retour, observation, retour au dépôt, reprise et bilan.
Les deux décisions de création restent identiques pendant les retours. Les six
instances d’objets (trois outils initiaux et trois marqueurs) conservent leurs
identités ; le spécimen termine dans les possessions. Les six faits sont uniques.
L’hypothèse de la prospection est soutenue ; celle du maintien au point de
prélèvement est contredite et n’a aucun soutien injustifié.

Les captures montrent les trois marqueurs ensemble dans le masque, le retrait
du spécimen, son dépôt ailleurs et le carnet avec ses rubriques et son défilement
Page suivante/Page précédente. Le journal d’animation contient les huit vues
0 à 7. L’audio reste actif : sept publications de cibles, aucune erreur de
callback, positions de lecture qui progressent. Cette vérification technique
ne constitue pas une nouvelle validation artistique du mixage.

Le harnais avait initialement envoyé des commandes vides parce que le menu
Démarrer conservait le focus Windows. Après fermeture de ce menu et activation
de la fenêtre du jeu, les quatorze commandes du parcours ont été exécutées.
Aucun changement du produit n’a été nécessaire pour ce problème d’automatisation.

### Livraison

Binaire compilé et publié dans `build/Release/liminal_cornell_renderer.exe`.
Son SHA-256 correspond à `build_native/Release/liminal_cornell_renderer.exe`
et aux manifestes `runtime-build.json`. README, état technique, documentation
d’enquête, roadmap et suivi du duo sont à jour. Aucun commit ni push effectué.

## Limites

Le catalogue reste composé de quatre possibilités scénaristiques. Le LLM choisit
leur répartition ; il n’invente ni nouvelles familles de preuves ni explication
au sort de Vey. La géométrie précède ce choix. Le nombre fini de sites, les
contraintes de navigation et les contrôles de sortie peuvent refuser une
proposition. Aucun délai de découverte n’est garanti, et les décisions vides
restent irréversibles dans la partie. Une série de trois graines ne mesure pas
la variété narrative générale du modèle. La qualification jouée est française ;
les autres langues disposent de tests de contrats et de secours.
