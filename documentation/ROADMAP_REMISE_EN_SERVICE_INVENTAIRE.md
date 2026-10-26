# Remettre l’inventaire en service avec les trois objets de départ

25 octobre 2026 — **lots 1 et 2 implémentés et testés** en duo Codex / DeepSeek. Voir le [document historique archivé](HISTORIQUE_RECHERCHE.md). Les lots 3 et 4 restent des extensions prévues. Le texte ci-dessous conserve le plan et ses critères.

26 octobre 2026 — La [roadmap des interactions avec l'environnement](ROADMAP_INTERACTIONS_ENVIRONNEMENT.md) précise et rend prioritaires le dépôt, la reprise et la collecte d'objets du décor. Elle remplace l'ordre de priorité du lot 4 ci-dessous ; ces nouvelles interactions restent à implémenter.

## Référence concrète : l’inventaire d’Athanor 2

L’utilisateur a fourni `E:/_games_/athanor_2/athanor-2-amiga/game` comme exemple de simplicité. Lecture du code effectuée le 25 octobre 2026, sans modification ni exécution du projet Athanor.

| Élément observé | Source dans le dossier fourni |
|---|---|
| Grille fixe de 10 × 2 cases | `game/game.h`, lignes 31–33. |
| Une case contient seulement un `short object_index` | `game/world.h`, structure `inventory_object`, lignes 74–77. |
| Case vide représentée par `-1` ; recherche de présence, retrait, ajout dans une case libre et refus des doublons | `game/game.c`, fonctions d’inventaire, lignes 1808–1904. |
| Objet en main distinct de la grille | `game/game.c`, variable `object_in_hand` et accesseurs. |
| Affichage des objets présents à partir de leurs sprites, ouverture et rafraîchissement du panneau | `game/gui.c`, fonctions à partir de la ligne 904. |
| Sauvegarde des cases occupées et d’une référence d’objet fondée sur le CRC16 de son nom | `game/game.c`, `gameLoadInventoryState` et `gameSaveInventoryState`, à partir de la ligne 2302. |

**Le noyau à reprendre est une collection d’identités d’objets avec quelques opérations explicites.** Ici, `HardState::inventory_items` fournit déjà cette collection et sa persistance. Conserver les identifiants texte existants ; reprendre le principe d’Athanor n’exige ni tableau de vingt cases, ni format binaire, ni CRC16.

Le premier livrable se ramène donc à : initialiser les trois possessions, vérifier leur présence, afficher la liste réelle et fournir cette liste au narrateur. Les opérations d’ajout et de retrait restent contrôlées par le moteur quand une mécanique les utilise effectivement. Les descriptions, libellés et éventuelles icônes appartiennent au catalogue et à la présentation.

Un petit panneau de trois entrées pourra compléter la commande `inventaire`, en réutilisant la même liste. Les icônes seraient des ressources fixes ; ouvrir ce panneau ne demanderait aucune génération d’image ou de texte. Ce panneau reste facultatif pour le premier livrable. Si une sélection « objet en main » devient utile, elle référencera un objet toujours compté parmi les possessions, avec une règle de conservation explicite.

Les usages détaillés ci-dessous sont des extensions de ce noyau. La remise en service de l’inventaire ne dépend pas du rendu d’une marque de craie, du dépôt dans une scène ou de la génération de nouveaux objets.

## Résultat attendu

Le joueur peut consulter ce qu’il transporte, examiner ses trois outils et les retrouver après chaque déplacement. Le narrateur connaît leur présence et leurs usages autorisés. L’inventaire est une donnée persistante de la partie, vérifiée par le moteur ; une phrase du LLM ne suffit jamais à créer, perdre ou utiliser un objet.

Les trois objets sont **possédés dès le départ**. Le jeu ne doit pas raconter qu’ils ont été ramassés dans un lieu généré. Chaque nouvelle partie retrouve cet équipement initial, sans importer les objets, notes ou traces d’une partie précédente.

| Identifiant existant à conserver | Libellé français | Fonction proposée |
|---|---|---|
| `survey map slate` | Tablette cartographique | Consulter les lieux visités et les liaisons réellement parcourues ; distinguer une issue ouverte d’un voisin déjà découvert. |
| `boundary chalk` | Craie de délimitation | Examiner l’outil ; dans une seconde étape, poser un repère local persistant sur une surface accessible. |
| `sample tongs` | Pinces à échantillons | Examiner et préparer l’outil ; une prise effective exige ultérieurement une cible saisissable déclarée par le moteur. |

Ces fonctions sont des propositions de mise en œuvre. Ni la craie ni les pinces ne doivent devenir un prétexte pour inventer une inscription, un cristal ou un prélèvement absent de l’état du jeu.

## Diagnostic de départ

- [`HardState::inventory_items`](../src/game_state.h) contient déjà la liste transportée. [`MakeInitialHardState`](../src/game_state.cpp) y place les trois objets ; la sérialisation et la lecture d’état conservent cette liste.
- L’ancien circuit de [`turn_runner.cpp`](../src/turn_runner.cpp) sait appliquer `inventory_add` et `inventory_remove`. Le mode local actuel passe par `SceneActionOutcome` et `NarrateScene`, qui n’autorisent pas ces manipulations.
- [`ExpandInfocomShortcutCommand`](../src/sdl_frontend.cpp) reconnaît `i` et `inventaire`, mais cela ne produit pas une restitution directe de la liste dans le moteur local.
- Le prompt narratif local ne fournit pas aujourd’hui un inventaire structuré des objets portés. Les masses visibles de la scène ne constituent pas un catalogue d’objets ramassables.

Il faut raccorder les données existantes au circuit de jeu actif. Réintroduire sans contrôle les tableaux libres `inventory_add` et `inventory_remove` permettrait au modèle de fabriquer des acquisitions sans cible réelle.

## Règles communes aux lots

1. **Une seule liste faisant autorité.** Conserver `HardState::inventory_items`. Un petit catalogue définit les trois objets, leurs libellés, alias et capacités ; il ne duplique pas leur possession dans une seconde liste.
2. **Identité stable.** Garder les trois identifiants existants pour la compatibilité. Les traductions et synonymes de commandes ne deviennent pas de nouveaux objets. Un objet unique ne peut apparaître deux fois dans l’inventaire.
3. **Possession vérifiée avant usage.** Une affirmation telle que « j’ai une clé » n’ajoute rien. « Prendre la craie » quand elle est déjà portée répond « Vous l’avez déjà » et ne crée pas de doublon.
4. **État avant narration.** Les commandes reconnues sont résolues par le moteur ; le narrateur reçoit le résultat exact autorisé. Une action refusée ne doit pas être racontée comme accomplie.
5. **Consultation immédiate.** Afficher l’inventaire, la fiche d’un outil ou les données connues de la tablette ne nécessite aucun appel LLM, aucune composition de scène et aucune variation thermique. Ces commandes restent tracées pour la recherche.
6. **Action effective et chaleur.** Une manipulation ayant un effet passe par le tour narratif et thermique existant : évaluation du LLM, validation moteur et application unique. Aucun incrément de chaleur automatique. L’échantillonnage reste fondé sur BODY au début du tour.
7. **Échec et annulation.** Préparer les effets avant validation du tour. Une annulation ne laisse ni objet déplacé ni repère créé. Une prose invalide reçoit un texte de secours fidèle aux seuls effets effectivement acceptés.
8. **État limité à la partie.** Les déplacements et revisites conservent l’équipement. Les archives enregistrent cet état sans alimenter le lancement suivant.

## Lot 1 — Consultation fiable : premier livrable jouable

**Priorité P0.** Rétablir le service minimal avant les manipulations du décor.

- Traiter `i`, `inventaire`, `inventory`, « qu’est-ce que je porte ? » et les équivalents pris en charge dans les six langues au niveau du moteur. SDL et le mode headless doivent obtenir le même résultat.
- Normaliser casse, espaces et variantes usuelles avec les utilitaires du projet. Une requête ambiguë reçoit une clarification courte ; elle ne déclenche pas une acquisition ou une utilisation implicite.
- Construire la réponse depuis la liste réelle, avec libellés localisés et ordre stable. Prévoir explicitement l’inventaire vide.
- Réutiliser la zone de texte actuelle : aucune nouvelle fenêtre ou option de lancement nécessaire.
- Enregistrer l’intention `read_inventory` et le contenu effectivement affiché dans les traces du tour. Cette lecture ne résout pas au passage une fiche de lieu en attente et ne compte pas comme déplacement.

Exemple attendu au démarrage :

> Vous transportez une tablette cartographique, une craie de délimitation et des pinces à échantillons.

**Critère de livraison :** cette liste reste exacte après une découverte, un retour et une lecture diagnostique d’état ; l’opération fonctionne même si le modèle est indisponible. Aucun appel LLM ni changement de BODY, des issues ou de la scène.

## Lot 2 — Les trois objets existent dans la fiction

**Priorité P0.** Compléter le premier livrable sans ajouter d’objets au monde.

- Ajouter des descriptions brèves et localisées pour « examiner la tablette », « regarder la craie » et « examiner les pinces ». Ces fiches décrivent seulement les capacités retenues, sans usure, batterie ou propriété surnaturelle inventée.
- Donner au narrateur les objets réellement portés, leurs capacités et le résultat de l’action du tour. Remplacer l’interdiction générale des manipulations par des permissions précises liées à ce résultat.
- Conserver la richesse littéraire des réponses libres : le LLM peut évoquer le poids des outils, leur présence et le geste autorisé, sans transformer cette évocation en effet supplémentaire.
- Protéger les trois objets de toute disparition narrative et de toute consommation implicite. Dans ce premier livrable, ils restent portés ; « jeter », « donner » ou « poser » explique que cette manipulation n’est pas encore disponible.
- Ne pas traiter « utiliser les pinces » comme une prise réussie : demander la cible lorsqu’elle manque, puis vérifier qu’elle existe et qu’elle est manipulable.

**Critère de livraison :** aucune fiche ne prétend que le joueur possède un outil absent de son état. Le modèle ne peut ni ajouter une quatrième possession ni retirer l’une des trois par son seul texte. Une commande refusée laisse l’inventaire intact.

## Lot 3 — Usages concrets, un effet vérifiable à la fois

**Priorité P1.** Chaque usage doit produire une conséquence définie et testable.

### Tablette : restituer le parcours connu

« Consulter la carte » affiche une synthèse textuelle du graphe réellement visité : lieu courant, lieux connus et liaisons parcourues. Les issues ouvertes encore inexplorées sont signalées comme telles, sans nommer leur futur voisin ni provoquer sa génération.

Réutiliser les identifiants de lieux, les liens et l’historique existants. Ne pas maintenir une seconde carte indépendante susceptible de les contredire. La tablette ne révèle pas de barrière invisible non découverte et ne fournit pas de mesure géométrique que le moteur n’a pas établie.

**Recette :** après un embranchement et un retour, la carte reflète les liens réels ; consulter la carte ne compose aucun lieu.

### Craie : un repère réellement mémorisé

Préparer une action bornée « marquer ici à la craie ». Le moteur vérifie la possession et une surface accessible depuis le point de vue actuel. En l’absence de cible admissible, aucune marque n’est créée.

Un repère accepté possède un identifiant, un lieu, une position ou un ancrage validé et le tour de création. Il reste attaché à ce lieu après un départ et un retour. L’historique narratif seul ne remplace pas cette donnée structurée.

Une marque annoncée comme visible doit être représentée dans l’image, par une surcouche locale persistante, sans recomposition LLM de la scène. Le programme `.scene` d’origine reste conservé ; l’archive doit aussi retenir la surcouche et une image montrant son effet. **Tant que cette représentation et l’ancrage ne sont pas prêts, ne pas annoncer une marque accomplie.**

Ne pas ajouter de jauge de craie ou de consommation arbitraire dans ce lot. Distinguer cette marque physique d’une note déjà prise dans le carnet.

**Recette :** la marque existe au même emplacement après revisite ; une annulation n’en crée aucune ; une nouvelle partie ne la récupère pas.

### Pinces : préparer l’outil sans fabriquer une cible

Autoriser l’examen et la préparation des pinces, avec une description cohérente de l’outil possédé. Ne pas introduire une mécanique d’équipement supplémentaire si elle n’a aucun effet utile.

Le prélèvement reste conditionné à une future cible déclarée saisissable, avec identité et emplacement propres. Une boîte monumentale, un nom technique du `.scene` ou le repère de rendu `portal` ne devient jamais un échantillon par interprétation du LLM.

**Recette :** « prendre ce bloc avec les pinces » ne crée ni bloc transporté ni cristal de remplacement. Le jeu répond à l’intention avec une explication située, sans fausse réussite.

## Lot 4 — Déposer et reprendre : extension distincte

**Priorité P2, après validation du premier livrable.** Cette étape étend la possession ; elle n’est pas nécessaire pour rétablir la liste des trois objets de départ.

Commencer uniquement par ces trois objets, sans catalogue de butin généré. Définir un propriétaire unique pour chaque objet : le joueur ou un emplacement dans un lieu. Un dépôt transfère l’objet ; une reprise effectue le transfert inverse. Reprendre deux fois le même objet ne doit jamais le dupliquer.

Ce lot exige une représentation locale des objets déposés, une position accessible validée, leur persistance à la revisite et leur archivage. Conserver une seule autorité sur la possession : la liste transportée et les objets présents dans les lieux doivent former un ensemble cohérent, validé atomiquement. Ne pas activer la commande avant que le dépôt puisse être retrouvé et représenté correctement.

Le ramassage d’autres objets improvisés par le LLM viendra dans une roadmap ultérieure : déclaration d’entités interactives, propriétés, liens avec le rendu et règles d’acquisition. Ce travail ne se réduit pas à autoriser `inventory_add`.

## Intégration technique et répartition du duo

| Zone | Intervention prévue |
|---|---|
| `src/game_state.h`, `src/game_state.cpp` | Liste existante, catalogue minimal et compatibilité ; données de repères seulement au lot 3. Préserver les objets inconnus des anciens états diagnostiques sans leur inventer des capacités. |
| `src/turn_runner.cpp` | Routage partagé des lectures, validation des possessions et des effets ; résultat fourni au narrateur ; transaction et annulation. |
| `src/sdl_frontend.cpp` | Alias et présentation cohérents avec le moteur, sans logique de possession parallèle. |
| `src/research_archive.cpp` | Commande, effet accepté/refusé, état avant/après utile à la recherche ; traces et images supplémentaires pour les mutations visuelles. |
| Tests natifs et CMake | Recettes d’inventaire et de persistance, callbacks comptant les appels ; raccord des cibles strictement nécessaires. |
| Documentation | Guide joueur, état technique et bilan de livraison distinguant les lots livrés des extensions. |

Codex garde la conception des identités, les contrats d’effets, la persistance, la narration et l’intégration. DeepSeek peut recevoir un lot borné de tests ou de résolution d’alias sur une API définie par Codex, avec contrat TASK, worktree isolé et revue selon `AGENTS.md`. Ne pas confondre ce duo avec le modèle du jeu.

Retirer les chemins devenus inutilisés après vérification de leurs appelants. Les anciens champs d’inventaire encore utilisés par les diagnostics ne sont pas du code mort à supprimer aveuglément. Éviter une seconde implémentation propre au frontend.

## Recette de livraison

| Cas | Résultat attendu |
|---|---|
| Nouvelle partie, `i`, puis `inventaire` | Trois objets, une occurrence chacun, noms localisés, zéro appel LLM. |
| Découverte, retour, inventaire | Même équipement ; aucune régénération pour consulter la liste. |
| Examiner chacun des trois outils | Description fidèle, sans acquisition ni effet caché. |
| « Prendre la craie » déjà portée | Confirmation de possession, aucun doublon. |
| « J’ai trouvé une clé », « prendre le portail » | Aucun ajout à l’inventaire. |
| État diagnostique vide ou outil absent | Affichage exact ; usage de l’outil absent refusé. |
| Sauvegarde et relecture diagnostique | Possessions préservées, sans réinitialisation intempestive. |
| Nouvelle partie après fermeture | Trois objets initiaux ; aucune récupération des traces ou mutations antérieures. |
| Lecture d’inventaire pendant une fiche de lieu en attente | Lecture seule, sans reprise narrative ou changement thermique. |
| Annulation d’une manipulation | Aucun effet partiel sur possession, repère ou état thermique. |
| Réponse LLM annonçant une acquisition non autorisée | Texte rejeté ou remplacé ; état inchangé. |

Les lots 1 et 2 constituent le **premier livrable attendu**. Les tests automatisés doivent vérifier les données et les appels réellement effectués ; une recette SDL vérifie ensuite l’affichage et les archives. Une courte session avec Gemma contrôle la cohérence littéraire, avec le runtime natif. Les lots 3 et 4 ajoutent leurs propres recettes d’effets et de rendu avant activation.

Publier le build de développement pour que le lanceur habituel donne accès au livrable, sans nouveau mode à sélectionner. Documenter séparément ce qui fonctionne et ce qui reste prévu. Le bilan distingue les recettes exécutées pour les lots 1–2 des critères encore à satisfaire pour les extensions.

## Références

- [document historique archivé](HISTORIQUE_RECHERCHE.md) : effets validés avant narration, absence de fausses acquisitions.
- [document historique archivé](HISTORIQUE_RECHERCHE.md) : état autoritatif, appel narratif commun, couplage BODY et annulation.
- [Nouvelle partie et archives de recherche](SINGLE_START_RESEARCH.md) : persistance pendant la partie, aucune réinjection au lancement suivant.
