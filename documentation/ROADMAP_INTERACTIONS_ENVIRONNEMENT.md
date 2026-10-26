# Roadmap — Prendre, déposer et retrouver les objets

26 octobre 2026 — **Lots 1 à 5 implémentés en duo Codex / DeepSeek**, avec reprise et corrections des tests par Codex. Voir le [bilan de livraison et de qualification](INTERACTIONS_ENVIRONNEMENT.md). Le lot 6 reste prévu. Le texte suivant conserve le contrat initial.

## Expérience visée

Le joueur doit pouvoir agir sur les objets du lieu : ramasser un fragment de gravats, poser une possession, partir explorer et retrouver cette même possession à son retour. « Regarder tout autour » et « look around » doivent décrire les objets réellement présents, y compris ceux déposés plusieurs tours auparavant.

Le fonctionnement est hybride : **le LLM comprend le geste et apprécie sa plausibilité ; le moteur conserve les objets et décide des changements d'état autorisés.** Le modèle peut conclure qu'un fragment évoqué dans la description est saisissable ou qu'un mur ne l'est pas. Une prise validée peut matérialiser dans l'état un objet jusque-là implicite. Le modèle ne modifie jamais seul l'inventaire et ne déplace pas une possession depuis une autre pièce.

La persistance demandée concerne **la même partie**, indépendamment du nombre de tours et de la mémoire du modèle. Fermer le jeu et lancer une nouvelle partie conserve le fonctionnement actuel : une nouvelle exploration. La reprise de sauvegarde entre lancements reste hors périmètre.

Parcours de référence :

1. La description littéraire inventée par le LLM évoque des gravats dans A ; aucun objet correspondant n'a encore besoin d'exister dans une liste explicite.
2. « Prendre gravats » est interprété comme une prise de ce fragment, si la cible est unique et accessible. Sinon, le jeu demande une précision.
3. Le moteur attribue une identité au fragment et l'ajoute à l'inventaire ; il mémorise ce prélèvement dans A pour éviter de recréer le même fragment.
4. Dans B, « déposer le fragment » le retire de l'inventaire et l'ajoute à la liste explicite des objets déposés dans B.
5. Après plusieurs déplacements, revenir dans B et « regarder tout autour » le fait réapparaître dans la description et dans le rendu correspondant.
6. « Reprendre le fragment » retire son entrée de la liste explicite de B et restitue le même objet dans l'inventaire. Une seconde prise ne le duplique pas.

## Deux origines pour une prise dans la pièce

| Origine | Ce qui atteste l'objet | Traitement d'une prise acceptée |
|---|---|---|
| **Implicite** | La description littéraire du lieu inventée par le LLM, conservée par le moteur ; aucune entrée préalable dans la liste des dépôts n'est requise | Le LLM interprète la cible et juge si elle est prenable. Le moteur valide la proposition, crée l'instance dans l'inventaire et mémorise sa provenance et son retrait de la fiction locale. |
| **Explicite** | La liste persistante des objets que le joueur a déposés dans cette pièce auparavant | Le moteur retire l'entrée de cette liste et ajoute la même identité à l'inventaire, en une seule transaction. |

Ces deux origines décrivent **où l'on trouve l'objet lors d'une prise**, pas deux classes immuables. Un objet d'origine littéraire devient une possession suivie dès sa première acquisition. S'il est ensuite déposé, il rejoint la liste explicite de sa pièce de dépôt, comme les trois outils initiaux. Sa provenance littéraire reste conservée.

La liste explicite est donc une liste **courante**, pas un historique des dépôts. Un objet repris n'y figure plus et ne doit plus être annoncé au sol par « regarder tout autour ». L'historique peut conserver l'événement, mais ne fait pas autorité sur sa présence actuelle.

Les deux sources sont rapprochées avant toute acquisition : si la description évoque une craie qui correspond à une entrée explicite, la prise réutilise cette identité. Elle ne crée pas une seconde craie implicite. Si cette correspondance est ambiguë, demander une précision.

## Point de départ vérifié

| Existant | Conséquence pour cette évolution |
|---|---|
| [`HardState::inventory_items`](../src/game_state.h) et son initialisation/sérialisation dans [`game_state.cpp`](../src/game_state.cpp) | Conserver les trois identifiants historiques et la consultation déterministe. La liste seule ne décrit pas les possessions déposées. |
| Catalogue et `ParseInventoryRequest` dans [`inventory.cpp`](../src/inventory.cpp) | Des verbes de prise et de dépôt sont reconnus, mais le catalogue concerne les trois outils initiaux. Reconnaître un verbe ne signifie pas exécuter un transfert. |
| `InventoryReply` et routage précoce dans [`turn_runner.cpp`](../src/turn_runner.cpp) | Les manipulations sont actuellement refusées. Ce routage doit laisser passer la nouvelle résolution d'interactions. |
| `SceneActionOutcome` et `NarrateScene` dans le même fichier | Le parcours local interdit les manipulations ; le narrateur reçoit l'inventaire initial du tour et des faits de scène. Il faudra lui transmettre l'état candidat après l'action validée. |
| `SessionState::current_place_id`, `GeneratedRoom::room_id`, liens entre lieux | Utiliser les identifiants de lieux persistants, y compris pour le seuil canonique. Un titre traduit ou le seul enum `LocationId` ne suffit pas pour distinguer les pièces générées. |
| Géométrie acceptée conservée lors des retours | Ajouter une couche d'objets mobiles sans recomposer l'architecture à chaque prise ou dépôt. |
| Archives dans [`research_archive.cpp`](../src/research_archive.cpp) | Étendre les preuves à la localisation des objets ; les archives restent des traces, pas un mécanisme de mémoire du gameplay. |

Les chaînes `visible_objects`, les noms techniques de primitives et les faits narratifs ne forment pas encore un registre d'instances manipulables. Les anciens champs libres `inventory_add` / `inventory_remove` ne doivent pas servir de raccourci pour donner au LLM le contrôle de la possession.

Cette roadmap prolonge la [remise en service de l'inventaire](ROADMAP_REMISE_EN_SERVICE_INVENTAIRE.md), dont les lots 1 et 2 sont livrés. Elle rend prioritaires le dépôt/reprise et les objets du décor, précédemment repoussés au lot 4 et au-delà. Les usages de la carte, de la craie et des pinces restent des extensions indépendantes.

## Contrat entre le moteur et le LLM

| Responsabilité | Moteur classique | LLM du jeu |
|---|---|---|
| Comprendre une demande | Alias simples, domaine des intentions et candidats autorisés | Reconnaître « ramasse », « emporte », « pose », les formulations libres et les synonymes |
| Désigner la cible | Fournir les dépôts, l'inventaire, la description du lieu et ses retraits mémorisés | Sélectionner une identité existante ou proposer une cible implicite fondée sur la description ; clarifier si nécessaire |
| Évaluer la saisissabilité | Faire respecter ancrage, accès, capacités et interdictions établies | Juger les cas ouverts à partir de propriétés attestées, avec une justification brève |
| Déplacer un objet | Vérifier et appliquer un transfert unique | Proposer l'action ; aucune écriture directe dans l'état |
| Mémoriser | Conserver localisation et identité pendant toute la partie | Recevoir à chaque appel les faits utiles à jour |
| Raconter | Fournir résultat exact et secours cohérent | Décrire ce résultat dans la langue choisie |

### Laisser une vraie place au jugement du modèle

Prévoir trois politiques de prise : **autorisée**, **interdite**, **à évaluer**. Un mur structurel est interdit ; une craie déposée est déjà connue comme transportable ; un élément nouvellement déclaré peut nécessiter une appréciation contextuelle. Le modèle peut accepter ou refuser ce dernier, mais il ne peut pas contredire un ancrage fixe ni inventer un poids, une toxicité ou un outil obligatoire pour motiver son choix.

Une décision retenue sur une propriété durable est mémorisée avec les propriétés et conditions qui la fondent. Changer la formulation de la même demande ne doit pas faire alterner arbitrairement oui et non. Un refus circonstanciel, par exemple une cible hors d'atteinte, est distingué d'une propriété permanente ; il pourra être réévalué si les conditions changent.

En cas d'incertitude réelle, demander une précision ou constater l'impossibilité de conclure. L'indisponibilité du modèle laisse fonctionner l'inventaire et les opérations simples déjà résolues par le moteur ; elle ne transforme jamais une cible inconnue en acquisition autorisée.

### Distinguer objets, décor et matières

- **Objet transportable :** par exemple la craie ou un petit fragment détaché ; il peut être implicite avant sa première prise, puis devient une instance suivie.
- **Décor fixe :** mur, sol, bloc monumental ; il peut être décrit ou examiné, mais pas pris.
- **Source de matière :** un tas de gravats évoqué dans la description peut donner lieu à une proposition de prélèvement par le LLM. Le moteur enregistre une source et une disponibilité bornée lors de sa première résolution ; il n'est pas nécessaire de prédéclarer chaque fragment à la création du lieu. Pas de découpe arbitraire de géométrie ni d'extraction infinie dans cette version.

« Prendre gravats » peut donc viser un fragment du tas décrit, sans faire entrer tout le tas dans l'inventaire. Si la cible est ambiguë, préciser le choix. Une mention dans la description littéraire du lieu constitue bien une source admissible, même sans objet structuré ni primitive graphique correspondante. En revanche, la seule affirmation du joueur, une évocation hypothétique ou un objet décrit dans une autre pièce ne suffisent pas. Une ancienne description reste utilisable en tenant compte des retraits et prélèvements intervenus depuis.

## État persistant proposé

Les noms ci-dessous sont conceptuels ; l'API C++ sera figée dans le premier lot.

| Donnée | Rôle |
|---|---|
| Registre d'instances, indexé par `object_id` | Identité stable, type, libellé, alias, description et propriétés validées ; deux fragments homonymes restent distincts |
| `HardState::inventory_items` | Liste autoritative des identités portées, en conservant les identifiants des trois outils initiaux |
| Liste explicite par `place_id` | Identités des objets actuellement déposés ; ajout au dépôt, retrait à la reprise ; placement local et référence visuelle éventuelle |
| Description littéraire de référence par lieu | Texte accepté, référence stable et révision, permettant au LLM de résoudre une cible implicite sans catalogue préalable |
| Retraits et prélèvements implicites par lieu | Lien entre source littéraire et identité créée ; disponibilité restante et retrait mémorisé pour empêcher les réapparitions |
| Politique d'interaction | Prise autorisée/interdite/à évaluer, conditions pertinentes et décision sémantique retenue |
| Provenance | Origine initiale ou littéraire, lieu et tour de matérialisation, référence au passage source, source éventuelle de matière et référence visuelle si elle existe |
| Révision d'état et résultat d'action | Vérification des propositions périmées et application unique d'un transfert |

**Invariant central : chaque objet matérialisé figure exactement une fois, soit dans l'inventaire, soit dans la liste explicite d'une pièce.** Les objets encore implicites restent dans la description, sans exiger d'entrée dans ce registre. La provenance et le journal de retrait ne dupliquent pas la localisation. Une fonction commune résout le propriétaire depuis cette partition ; aucun booléen `owned` supplémentaire ni liste frontend indépendante. Les éléments fixes restent attachés à leur pièce et ne passent jamais dans l'inventaire.

Le registre est créé avec les possessions initiales, puis enrichi à chaque première prise implicite validée. Une nouvelle pièce conserve sa description de référence sans imposer de conversion exhaustive de ses noms en objets. Un retour, un résumé narratif ou une réparation de langue ne réinitialise ni les listes explicites ni les retraits implicites. Les nouveaux identifiants sont attribués par le moteur ; les traductions et alias ne les changent pas.

Étendre la sérialisation diagnostique et l'archivage de session avec une version explicite. Les anciens états dépourvus de registre gardent leurs possessions, sans inventer de butin dans les pièces. Préserver les identifiants inconnus, sans leur attribuer de capacités imaginaires. Cette compatibilité ne crée pas une fonction de chargement dans SDL.

## Déroulement d'une commande

1. **Router la demande.** Conserver la consultation immédiate de l'inventaire et l'examen des outils connus. Résoudre localement les commandes sans ambiguïté ; solliciter le LLM pour les formulations libres et les décisions ouvertes. « Regarder tout autour » doit rester une observation de la pièce.
2. **Construire le contexte des deux sources.** Pour prendre : liste explicite de la pièce, description littéraire et retraits implicites, plus possessions pertinentes pour répondre « déjà porté ». Pour déposer : objets réellement portés. Pour regarder : description actualisée et liste explicite courante. Ne pas exposer d'objets de pièces lointaines comme candidats disponibles.
3. **Obtenir une proposition structurée.** Schéma borné avec intention, origine de la cible, verdict et motif. Branche explicite : identité existante. Branche implicite : référence au passage descriptif, désignation et propriétés proposées, sans identité arbitraire attribuée par le modèle. Pas de tableaux libres d'ajout/retrait. Une proposition ambiguë ne déclenche aucun transfert.
4. **Valider et préparer l'état candidat.** Vérifier localisation, accès, politique, conditions et révision. Pour une prise explicite, retirer l'entrée de la pièce et ajouter son identité à l'inventaire. Pour une prise implicite, vérifier la référence littéraire et les prélèvements antérieurs, rapprocher les identités déjà connues, puis créer l'instance et enregistrer son retrait dans la même transaction. Pour un dépôt, retirer de l'inventaire et ajouter à la liste explicite. Un emplacement de dépôt impossible refuse toute l'opération. Une commande traite un objet à la fois dans cette version.
5. **Préparer la restitution.** Donner au narrateur le résultat autorisé et les objets de l'état candidat, pas seulement l'inventaire d'avant le tour. La chaleur suit le contrat existant, appliqué une fois ; aucun coût thermique automatique lié au nombre d'appels LLM.
6. **Publier le tour atomiquement.** Conserver la barrière d'annulation existante jusqu'au dernier contrôle. État des objets, rendu et texte définitif doivent décrire la même révision. Une panne narrative utilise un secours fidèle ; elle ne relance pas le transfert. Aucune réussite provisoire n'est présentée comme acquise avant publication.

Une réponse malformée, une identité explicite inconnue, une source littéraire introuvable ou une contradiction avec l'état courant ne modifie pas la possession. Une cible implicite n'est pas rejetée au seul motif qu'elle ne possède pas encore d'identité. La politique thermique d'une observation ou d'une tentative refusée reste celle du tour existant ; consulter l'inventaire ou clarifier une cible ne doit pas devenir une manipulation effective.

## « Regarder tout autour » et cohérence du rendu

La description du lieu doit combiner **la fiction littéraire actualisée par les retraits implicites** et **la liste explicite des objets actuellement déposés**, en respectant l'architecture persistante. Un résumé d'arrivée mémorisé ne peut pas réintroduire un fragment pris depuis, ni effacer une craie déposée ensuite. Un objet retiré de la liste explicite après reprise ne doit plus être décrit comme posé. Les faits dynamiques priment sur l'historique et les résumés.

Pour une observation générale, fournir au LLM la description de référence, les retraits/prélèvements et la liste explicite courante. Faire produire une liste structurée des identités déposées décrites, vérifier la couverture, puis compléter par une phrase locale ou utiliser un secours si le texte oublie un dépôt. Cette couverture ne demande pas d'énumérer tous les objets implicites de la prose. La liste structurée seule ne prouve pas que la prose est fidèle : la recette doit aussi examiner les textes réellement affichés.

Limiter d'abord les dépôts à des emplacements accessibles et perceptibles depuis la vue de la pièce, afin de garantir leur redécouverte. Si aucun emplacement ne convient, refuser clairement le dépôt, sans perdre l'objet. Les cachettes et objets occultés demanderont ultérieurement une mécanique de recherche dédiée.

Le décor fixe accepté reste inchangé. Une couche dynamique, déterministe et attachée au lieu, ajoute ou retire les représentations d'objets mobiles. Après mutation visuelle, invalider les images et vues de respiration concernées, ainsi que les informations de visibilité, profondeur et ID qui en dépendent. Une cible implicite peut être seulement littéraire : l'absence de primitive graphique ne bloque pas sa prise. Si elle possède déjà une représentation identifiable, prévoir son retrait visuel sans recomposer l'architecture. Un prélèvement dans un tas peut laisser le tas visible, mais pas présenter le fragment emporté comme toujours disponible.

Un prototype purement textuel peut valider les transferts. **La livraison jouable exige ensuite la cohérence texte/image**, sans génération complète de la pièce à chaque manipulation. Une représentation simple mais lisible suffit ; la variété artistique des petits objets peut venir après.

## Lots et critères de passage

### Lot 1 — Identités et conservation des objets · P0

Définir le registre, les listes explicites, les sources littéraires et retraits implicites, les transferts atomiques et leur validation. Initialiser les trois outils sans changer leurs identifiants. Couvrir les lieux canoniques et générés, la sérialisation et les états anciens.

**Livraison :** tests moteur sans inférence prouvant dépôt, déplacement du joueur, retour, reprise, inventaire vide, refus des doublons et conservation après annulation. Aucune commande publique ne promet encore une interaction visuelle incomplète.

### Lot 2 — Compréhension hybride et décision située · P0, après le lot 1

Définir les deux branches du schéma de proposition : identité explicite ou cible littéraire implicite. Raccorder le parseur existant au résolveur commun SDL/headless. Prévoir alias multilingues, clarification des homonymes, rapprochement des deux sources, politique de saisissabilité et cache des décisions durables. Distinguer « déposer ici » de « donner à quelqu'un » ou « jeter au loin » : les alias actuellement regroupés sous `Drop` ne doivent pas simuler un don ou une destruction.

**Livraison :** commandes françaises et anglaises de prise/dépôt reconnues ; prise possible d'un objet seulement décrit, sans prédéclaration ; modèle capable de juger une cible à évaluer ; mur refusé, objet absent des deux sources refusé, cible ambiguë clarifiée. Tests avec réponses contrôlées, puis observation du vrai modèle intégré. Les six langues conservent consultation et secours cohérents.

### Lot 3 — Objets du décor et placements visibles · P0, après le lot 1

Conserver les descriptions littéraires des lieux et matérialiser les objets implicites à la première prise validée, avec provenance et retrait persistant. Couvrir au moins un fragment issu de gravats décrits ; ne pas imposer un catalogue préalable de butin. Développer la couche mobile, la prise visuelle lorsqu'une représentation existe et les points de dépôt. Les trois outils initiaux sont déposables et reprenables eux aussi.

**Livraison :** chaque objet acquis reçoit une identité ; le fragment pris n'est plus disponible dans sa source littéraire et sa représentation éventuelle est actualisée. Déposer le fragment ou la craie ajoute une entrée explicite et sa représentation ; reprendre la retire. Revisiter ne régénère ni les objets ni le décor. Un échec ou une annulation de matérialisation ne laisse ni instance fantôme ni source épuisée à tort.

### Lot 4 — Narration fidèle et boucle complète · P0, après les lots 2 et 3

Assembler état candidat, résultat d'action, observation générale et rendu. Adapter les interdictions narratives et les gardes actuels pour accepter les effets effectivement validés. Traiter explicitement les résumés anciens, les omissions du LLM, l'annulation et les secours localisés.

**Livraison :** le parcours de référence fonctionne dans SDL et en headless, y compris après plus de douze tours pour dépasser la fenêtre actuelle de continuité narrative. « Regarder tout autour » et « look around » restituent le dépôt ; la reprise conserve son identité. **C'est le premier jalon jouable couvrant le besoin complet.**

### Lot 5 — Qualification et archives · P0, avant diffusion

Tracer commande, intention proposée, origine implicite/explicite, référence littéraire éventuelle, décision sémantique, motif de validation/refus, identités et localisations avant/après, retraits/prélèvements, révision, secours et annulation. Conserver les données de rendu utiles pour vérifier les retours. Mesurer appels LLM, durée et budgets ; viser au plus un appel d'interprétation et un de narration pour une interaction libre, sans boucle de réparation illimitée.

**Livraison :** matrice de recette passée sur les chemins moteur, headless et SDL appropriés ; observation du modèle local avec cas acceptés et refusés ; documentation joueur et état technique mis à jour avec uniquement les résultats constatés. La performance est mesurée, sans promesse chiffrée avant recette.

### Lot 6 — Enrichissements ultérieurs · P1/P2

Usages des pinces sur des cibles déclarées, propriétés contextuelles plus riches, recherche d'objets cachés et meilleure variété visuelle. Extraction de matière avec quantité, conteneurs, combinaison, consommation, destruction, dons, actions multiples et sauvegarde intersessions exigent des contrats séparés. Aucune de ces extensions ne bloque la boucle prendre → déposer → revenir → reprendre.

## Points d'intégration et organisation

| Zone | Travail prévu |
|---|---|
| [`game_state.h`](../src/game_state.h), [`game_state.cpp`](../src/game_state.cpp) | Registre, placements, invariants, initialisation et sérialisation |
| [`inventory.h`](../src/inventory.h), [`inventory.cpp`](../src/inventory.cpp) | Résolution des identités, libellés des nouveaux objets, alias et consultation |
| [`turn_runner.h`](../src/turn_runner.h), [`turn_runner.cpp`](../src/turn_runner.cpp) | Proposition structurée, validation, transfert préparé, contexte narratif et annulation |
| [`scene.h`](../src/scene.h), [`scene.cpp`](../src/scene.cpp), [`sdl_frontend.cpp`](../src/sdl_frontend.cpp) | Ancrages, couche mobile, invalidation du rendu et présentation cohérente |
| [`research_archive.cpp`](../src/research_archive.cpp) | Preuves d'interaction, état dynamique et références aux images |
| [`tests/inventory_test.cpp`](../tests/inventory_test.cpp), tests natifs et [`CMakeLists.txt`](../CMakeLists.txt) | Conservation, routage, narration contrôlée et raccord des nouvelles recettes |

Codex garde les interfaces, la sémantique des objets, la mémoire de session, les transactions et l'intégration. Les lots bornés de documentation, d'alias ou de tests sur API figée peuvent être confiés à DeepSeek suivant le protocole du dépôt. Le duo de développement ne change ni Gemma ni le runtime llama.cpp du jeu.

Chaque lot se termine par une revue des changements et des preuves. Pour l'implémentation, exécuter les tests natifs concernés puis `ctest --test-dir build_native -C Release --output-on-failure`, et les suites Python si leurs chemins sont touchés. La validation textuelle seule ne remplace pas la recette visuelle dans SDL.

## Matrice de recette à exécuter lors de l'implémentation

Matrice issue du lot documentaire [DUO-027](duo/reviews/DUO-027.md), relue et précisée par Codex, puis adaptée à la distinction implicite/explicite demandée par l'utilisateur. Les scénarios exécutés, essais Gemma et vérifications SDL sont distingués dans le [bilan](INTERACTIONS_ENVIRONNEMENT.md) ; cette matrice demeure le contrat de référence.

| Cas | Précondition et commande/parcours | Résultat attendu |
|---|---|---|
| Équipement initial | Nouvelle partie, `inventaire` | Trois outils, une occurrence chacun ; aucun outil présenté comme trouvé sur place. |
| Dépôt | Craie portée, « déposer la craie » | Craie absente de l'inventaire et présente une seule fois dans la pièce. |
| Rendu du dépôt | Observer immédiatement après le dépôt | La craie apparaît au placement retenu ; image et texte décrivent la même situation. |
| Persistance longue | Déposer, parcourir plusieurs lieux pendant plus de douze tours, revenir | Même identité et même placement, indépendamment de l'historique transmis au modèle. |
| Observation FR/EN | Après retour, « regarder tout autour » puis « look around » | L'objet déposé figure explicitement dans chacune des restitutions ; aucune mutation. |
| Reprise explicite | « Prendre la craie » dans sa pièce, puis « regarder tout autour » | Entrée retirée de la liste explicite ; retrait visuel ; une occurrence dans l'inventaire ; craie plus décrite comme posée. |
| Première prise implicite | Description évoquant un petit caillou, aucune entrée explicite ni instance préalable ; « prendre le caillou » | Jugement LLM puis création moteur d'une identité dans l'inventaire ; provenance et retrait de la source littéraire enregistrés ensemble. |
| Transition des deux origines | Prendre ce caillou implicite dans A, le déposer dans B, le reprendre | Ajout puis retrait de la liste explicite de B, identité inchangée ; aucune réapparition implicite dans A. |
| Chevauchement des sources | Description mentionnant le même objet que la liste des dépôts | Réutilisation de l'identité explicite, aucun doublon implicite ; clarification si la correspondance reste incertaine. |
| Prise implicite répétée | Caillou déjà emporté, ancienne description toujours conservée ; répéter avec un synonyme après plusieurs tours | Retrait mémorisé respecté, aucune seconde instance créée. |
| Double prise | Craie déjà reprise, répéter la prise | Réponse « déjà portée », aucun doublon. |
| Inventaire vide | Déposer successivement les trois outils | Consultation vide explicite ; les trois outils restent récupérables. |
| Dépôt impossible | Objet absent de l'inventaire, ou aucun placement admissible | Refus, aucune perte et aucun ajout dans la pièce. |
| Objet inexistant | Pièce sans cristal, « prendre le cristal » | Aucun objet créé ; explication située. |
| Objet distant | Pierre dans A, demander sa prise depuis B | Aucun transfert à distance. |
| Mur | Mur fixe attesté, « prendre le mur », y compris si le LLM propose oui | Refus autoritatif ; géométrie et inventaire inchangés. |
| Gravats finis | Tas littéraire dont la première résolution valide un seul prélèvement disponible ; « prendre gravats », puis nouvelle tentative | Première matérialisation d'un fragment identifié ; deuxième tentative sans nouvelle acquisition. Le tas n'est pas transporté ni effacé. |
| Homonymes | Deux fragments distincts, « prendre le fragment » | Clarification sans mutation ; demande précisée ensuite = un seul transfert, autre fragment conservé. |
| Jugement ouvert | Cible attestée avec politique « à évaluer », propriétés suffisantes | Tester une proposition favorable et une défavorable compatibles avec les propriétés ; seul le cas accepté et validé transfère l'objet. |
| Stabilité du jugement | Répéter une demande avec synonymes, conditions inchangées | Propriété durable conservée ; pas d'alternance arbitraire entre prise possible et impossible. |
| Proposition malformée | JSON invalide, identité explicite inconnue ou référence littéraire introuvable | Aucun transfert ni matérialisation ; secours ou clarification, sans annoncer une prise réussie. |
| Narration défaillante | Transfert préparé et validé, narration invalide, aucune annulation | Publication avec secours fidèle ; un seul transfert. |
| Effet inventé | LLM proposant un objet absent des dépôts et sans fondement dans la description, ou une mutation supplémentaire | Effet rejeté. Une cible implicite fondée sur la description peut légitimement devenir une quatrième possession. |
| Annulation | Annuler avant la publication finale, y compris après préparation d'une première prise implicite | État initial conservé ; aucune instance ni marque de retrait créée ; aucune surcouche publiée ni réussite affichée comme définitive. |
| Ancien résumé / omission | Résumé mentionnant un objet désormais emporté ; LLM omettant un dépôt actuel | Absence de l'objet emporté ; présence du dépôt assurée par la restitution corrigée ou de secours. |
| Persistance diagnostique | Sérialiser puis relire un état avec dépôts, homonymes et retraits implicites | Identités, listes explicites, provenances et sources restantes identiques ; aucun doublon, sans ajout d'un chargement SDL. |
| Nouvelle partie | Après plusieurs dépôts, relancer une exploration | Trois outils initiaux ; aucun placement hérité de l'ancienne partie. |

Compléter ces cas par les régressions de navigation, de chaleur, de consultation sans inférence et de langue. Pour chaque échec injecté, contrôler les données avant/après et le texte affiché ; pour chaque mutation visible, examiner aussi l'image. Une narration convaincante ne constitue pas, seule, une preuve de transfert.
