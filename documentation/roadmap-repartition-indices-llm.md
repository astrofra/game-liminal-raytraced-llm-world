# Roadmap — Confier au LLM la répartition persistante des indices

Projet : **Within the Latent Walls / Entre les Murs Latents**  
Dépôt : <https://github.com/astrofra/game-liminal-raytraced-llm-world>  
Date : **27 octobre 2026**  
Base examinée : **`af9124c3377bc319586c3a8d6ace6c75a14914c2`**  
Statut : proposition d’évolution ; les mécanismes décrits comme cibles restent à implémenter.

## 1. Intention

Le LLM décide, au fil de l’exploration, quels indices se trouvent dans chaque nouveau lieu. Cette répartition participe à son écriture du monde : il peut ménager un intervalle sans découverte, rapprocher plusieurs éléments ou dissocier leurs apparitions.

Après validation et publication du lieu, le choix devient un fait durable de la partie. Les retours, les reformulations et les corrections de narration utilisent cet état. Ils ne relancent jamais la répartition.

Le moteur conserve l’autorité sur les identités, les placements réalisables, la navigation et les conditions d’acquisition des preuves. L’initiative narrative de placement appartient au modèle.

Le premier lot réutilise les quatre possibilités existantes : spécimen cristallin, obstruction invisible, outil de Vey et plaquette de relevé. Leur contenu et leurs révélations restent définis par le scénario. L’invention de nouvelles familles d’indices est une extension distincte.

## 2. Point de départ dans le code

À la base examinée :

- `SelectInvestigationOpportunity()` choisit le prochain indice côté C++. Une rotation dépendant de la graine ordonne les trois premières possibilités ; le relevé de Vey vient ensuite.
- `PrepareSceneInvestigation()` tente de placer cette possibilité dans le nouveau lieu. Le modèle reçoit auparavant un brief correspondant à ce choix déjà effectué.
- Les objets, opportunités, preuves et hypothèses disposent déjà d’un état persistant dans `SessionState`.
- La création locale utilise une génération de géométrie `.scene`, puis un appel narratif structuré qui propose aussi les issues, les températures et les gains audio.
- La publication passe par un candidat, des contrôles d’invariants et une dernière vérification d’annulation.

L’évolution consiste à remplacer le choix préalable du moteur par une proposition explicite du modèle, puis à enregistrer la décision acceptée, y compris lorsqu’elle ne place aucun indice.

## 3. Répartition des responsabilités

| Décision ou opération | Responsable cible |
|---|---|
| Placer des indices dans ce nouveau lieu ou le laisser sans indice | LLM |
| Choisir les identités et leurs rapprochements dans ce lieu | LLM |
| Choisir un emplacement parmi les possibilités proposées | LLM |
| Fournir les possibilités physiques compatibles | Moteur |
| Vérifier unicité, disponibilité, placement et navigation | Moteur |
| Publier le lieu, les objets et leur répartition | Moteur, atomiquement |
| Déterminer si un examen, contact ou transfert établit une preuve | Moteur, après interprétation de la demande par le LLM |
| Restituer les faits acquis et leurs incertitudes | Narration et carnet à partir de l’état accepté |

La graine continue de servir à l’échantillonnage. Elle ne choisit plus un ordre d’indices dans une fonction C++.

Ne pas ajouter de parseur C++ de langage naturel. Les propositions structurées sont validées par identité et provenance, conformément à `AGENTS.md`.

## 4. Intégration recommandée : utiliser l’appel narratif de création

Pour ce pilote, conserver le générateur `.scene` et ses audits. La sélection des indices intervient dans **l’appel narratif existant de première arrivée**, après la préparation de la géométrie candidate.

Séquence cible :

1. Composer et auditer la géométrie du futur lieu, sans réserver un indice à l’avance.
2. Calculer les emplacements possibles pour les objets et les approches compatibles avec une obstruction.
3. Présenter ces possibilités au modèle, avec l’état de l’enquête et les indices encore disponibles.
4. Dans sa réponse structurée habituelle, le modèle propose la répartition et écrit le texte correspondant.
5. Valider la sélection, préparer les objets et les contraintes de navigation sur une copie, puis vérifier les autres propositions et la narration.
6. Composer la couche d’objets, vérifier le candidat complet et publier après la dernière sonde d’annulation.

Les objets sélectionnés sont préparés **avant** le contrôle de couverture des dépôts par la narration. Le validateur doit comparer le texte et `deposited_ids` à cette liste candidate, et non à la liste antérieure au choix du modèle.

Cette architecture conserve le nombre d’appels de base. Elle implique une limite assumée : la géométrie précède le choix précis des indices. Le LLM décide de leur insertion dans un lieu déjà composé ; il ne dessine pas encore cette architecture autour d’un indice choisi dans la même réponse.

Si ce dernier degré de composition devient nécessaire, faire évoluer ultérieurement l’appel géométrique vers une réponse contenant géométrie et choix d’indices. Cela demande un contrat de sortie différent ; ce n’est pas une condition du présent pilote. Aucun appel de planification supplémentaire n’est prévu ici.

## 5. Contexte fourni au modèle

Ajouter au contexte de création un bloc d’écriture distinct du dossier connu du joueur :

- questions d’enquête et constats déjà établis ;
- indices déjà placés, avec les lieux et identités nécessaires à la continuité ;
- possibilités encore disponibles, présentées sans ordre prioritaire ;
- emplacements admissibles dans le lieu candidat ;
- compteur de nouveaux lieux acceptés depuis la dernière distribution, fourni comme information de rythme ;
- rappel que le modèle peut choisir un ensemble vide ou plusieurs indices compatibles.

Pour le placement, les fiches publiques et le rôle narratif autorisé suffisent généralement. Ne pas transmettre systématiquement toutes les révélations cachées de chaque objet. Lorsque des informations d’auteur sont nécessaires, les séparer explicitement des connaissances du joueur et des descriptions permises à l’arrivée.

L’interprète des commandes continue de recevoir les faits connus et les indices actuellement accessibles. Il ne reçoit pas la liste des réponses cachées ou de tous les objets encore non placés.

Les décisions et preuves conservées dans l’état restent disponibles au-delà des six événements récents. Leur contexte est compact et budgété avec le tokenizer réel ; une réduction de prompt n’efface aucun fait persistant.

## 6. Contrat de proposition

Enrichir uniquement la réponse de création par un petit champ structuré, par exemple :

```json
{
  "investigation_placement": {
    "revision": 7,
    "placements": [
      {
        "opportunity_id": "vey_equipment",
        "site_id": "ground_02"
      },
      {
        "opportunity_id": "invisible_obstruction",
        "site_id": "approach_east"
      }
    ]
  }
}
```

Ce fragment s’ajoute aux champs narratifs, thermiques, topologiques et audio existants. Il ne constitue pas leur réponse complète.

Règles du contrat :

- `revision` référence l’état d’enquête à partir duquel la proposition a été préparée.
- `opportunity_id` appartient aux possibilités encore disponibles et autorisées pour cette réponse.
- `site_id` appartient aux emplacements du lieu candidat. Le moteur déduit le lieu cible ; le modèle ne peut pas écrire dans une autre pièce.
- `placements: []` est une décision valide et définitive de laisser ce lieu sans indice scénarisé à sa création.
- Chaque possibilité apparaît au plus une fois. Deux objets ne peuvent pas occuper le même emplacement incompatible.
- Le pilote permet plusieurs indices dans un lieu. Sa borne maximale suit le catalogue fini et les emplacements réellement disponibles ; elle ne réintroduit pas une règle « un indice par pièce ».
- L’obstruction est unique dans ce catalogue ; elle utilise une approche, pas un emplacement d’objet.

Le schéma contraint les identifiants disponibles. La validation C++ reste indépendante de la grammaire et vérifie aussi les combinaisons de placements.

Un champ absent, une réponse tronquée ou une proposition invalide ne valent jamais décision vide.

## 7. État persistant : enregistrer aussi les décisions vides

Les opportunités déjà publiées suffisent à mémoriser les indices présents. Elles ne suffisent pas à distinguer une pièce volontairement vide d’une pièce dont le choix n’a jamais été traité.

Ajouter un enregistrement de décision par lieu, lié à l’état d’enquête :

| Donnée | Utilité |
|---|---|
| `place_id` | Lieu dont la répartition est figée |
| `origin` | `llm` ou `legacy_preserved` |
| `opportunity_ids` | Liste acceptée, éventuellement vide |
| `committed_turn` | Tour de publication de la décision |

Les noms et champs exacts restent à adapter au code. Éviter un second registre concurrent de coordonnées : les opportunités et les objets existants restent les références spatiales autoritatives. Les sites temporaires servent à valider la proposition ; leur résolution utile figure dans la provenance archivée.

Une fois accepté, cet enregistrement est immuable pendant la partie. L’état de découverte peut évoluer après examen ; la localisation actuelle d’un objet portable peut évoluer par transfert. Ces évolutions ne réécrivent pas sa décision d’apparition ni son lieu d’origine.

Versionner le schéma d’enquête et identifier la nouvelle politique, par exemple `eryx-clues-llm-v2`. Adapter ensemble sérialisation, parseur, dérivation et invariants, actuellement limités au schéma et à la politique précédents.

## 8. Cohérence physique et navigation

### Objets

Réutiliser le registre et la couche mobile. Pour permettre plusieurs objets, calculer un petit ensemble d’emplacements visibles, soutenus et non superposés. Vérifier le groupe complet après sélection : des sites valides isolément peuvent devenir incompatibles ensemble.

Le modèle choisit le site ; le moteur fournit sa transformation physique validée. Les noms de sites sont temporaires et n’apparaissent pas dans l’interface du joueur.

### Obstruction invisible

Les sites proposés doivent respecter les garanties actuelles : approche dégagée, aucun lien publié traversé, retour préservé et au moins une autre continuation accessible dans le graphe.

La direction retenue doit rester une tentative de départ possible jusqu’au contact. Ne pas fermer cette issue dans la proposition d’issues, sinon le joueur ne peut pas faire l’expérience attendue. L’obstruction refuse ensuite le mouvement sans déplacement.

La proposition d’issues et le placement de l’obstruction sont validés ensemble. Le moteur peut refuser une combinaison impossible ; il ne substitue pas un autre indice ni une autre direction de son propre chef. Conserver les réparations topologiques existantes lorsqu’elles sont compatibles avec le choix du modèle ; rejeter le candidat si leur combinaison ne l’est pas.

La présence d’une barrière ne constitue pas encore une preuve connue. Le contact puis l’examen restent deux événements distincts.

## 9. Publication, échecs et revisites

| Situation | Comportement cible |
|---|---|
| Sélection valide, texte et candidat valides | Publier le lieu, la décision, les objets et les liens ensemble |
| Sélection valide et explicitement vide | Publier le lieu avec une décision vide figée |
| Sélection valide, défaut de prose récupérable | Conserver exactement cette sélection et produire un secours localisé cohérent avec les objets candidats |
| Sélection absente, invalide, périmée ou impossible | Ne pas publier ce nouveau lieu ; aucun indice consommé et aucun déplacement accompli |
| Annulation avant publication | Conserver intégralement l’état précédent |
| Revisite ou observation d’un lieu accepté | Utiliser sa décision existante ; aucune nouvelle proposition de placement |

Le refus d’une sélection essentielle à ce nouveau lieu modifie le secours actuel, qui peut publier une géométrie valide malgré l’échec de la carte narrative. Rendre ce changement explicite et ciblé : une narration de secours reste possible après une sélection valide ; elle ne fabrique pas une sélection manquante.

Les contrôles des températures et de l’audio conservent leurs politiques de validation indépendantes. Un défaut de ces champs n’autorise jamais une nouvelle répartition.

Cas à traiter explicitement : une pièce marquée `metadata_fallback_used` peut aujourd’hui recevoir une nouvelle carte descriptive. Cette correction doit conserver la décision d’indices, les objets et les barrières déjà publiés. Ne jamais assimiler une réparation de titre ou de résumé à une création de lieu.

Les réponses de revisite utilisent un schéma sans champ de placement. Une proposition inattendue de placement y est refusée, sans créer d’objets.

## 10. Rythme et initiative narrative

Retirer le sélecteur automatique, la rotation par graine et la réservation imposée avant composition. Retirer aussi l’obligation de faire apparaître le relevé de Vey après les autres indices.

Une consigne d’auteur peut encourager le modèle à ménager un rythme, à donner des pistes concrètes et à tenir compte des questions sans réponse. Le compteur de lieux sans indice peut l’aider. Ces indications ne deviennent pas une règle C++ qui choisit l’indice au bout de N pièces.

Cette liberté admet des parties où le modèle tarde à distribuer un élément, rassemble plusieurs traces ou laisse une piste incomplète. Le signaler comme propriété du pilote. Mesurer ces comportements avant de décider d’éventuelles contraintes supplémentaires.

Le fait d’avoir tout placé ne signifie pas que tout a été observé ou compris. Le scénario reste borné à quatre possibilités ; confier leur distribution au modèle ne crée pas à lui seul une enquête infinie ni un dénouement.

## 11. Migration et conservation des parties

Pour un état ancien :

- préserver tous les lieux, liens, objets, barrières, preuves et hypothèses admissibles ;
- conserver les opportunités déjà placées, qui ne redeviennent jamais disponibles ;
- enregistrer les lieux déjà acceptés comme `legacy_preserved`, avec leurs opportunités existantes, éventuellement aucune ;
- ne pas attribuer rétroactivement ces décisions au LLM ;
- autoriser la nouvelle distribution uniquement dans de futurs lieux.

Le seuil initial reste prédéfini et figé. Le traiter explicitement comme un lieu préservé, sans appel de placement au démarrage.

La persistance concerne la partie en cours et ses exports. Cette évolution n’ajoute pas un chargement de sauvegarde dans SDL.

## 12. Corrections préalables issues de la revue

Deux défauts reproduits sur la base examinée doivent être couverts avant d’étendre la distribution :

1. **Duplication depuis une ancienne description.** Après prélèvement et dépôt ailleurs, une proposition de prise littéraire fondée sur la mention initiale peut recréer un objet. Lier la mention à l’identité existante et conserver son retrait dans la provenance, sans ajouter de parseur de commandes. Un objet déplacé ne doit pas redevenir une source neuve à son lieu d’origine.
2. **Soutien injustifié d’une hypothèse.** L’acquisition du cristal et l’examen de l’outil suffisent actuellement à soutenir l’idée que Vey serait resté à son point de prélèvement, même dans des lieux différents. Définir un soutien réellement pertinent pour cette proposition, ou la laisser ouverte. Un rapprochement spatial décidé par le modèle ne devient pas automatiquement une preuve de causalité ou de destin.

La nouvelle répartition ne doit pas aggraver ces défauts ni compter sur le prompt pour les empêcher.

## 13. Lots d’implémentation

| Lot | Travail | Critère de livraison |
|---|---|---|
| 0 — Conservation et hypothèses | Corriger les deux défauts précédents avec régressions ciblées | Aucune seconde instance issue d’une mention périmée ; aucun soutien sans justification pertinente |
| 1 — État et migration | Ajouter les décisions par lieu, versionner l’état et conserver les anciens mondes | Les décisions vides et les lieux importés restent figés après sérialisation |
| 2 — Choix du modèle | Fournir possibilités et sites ; enrichir l’appel de création ; retirer le sélecteur C++ | À géométrie identique, les propositions du modèle produisent des répartitions différentes, dont aucune et plusieurs |
| 3 — Validation et publication | Préparer les placements sur copie ; intégrer issues, objets, narration et annulation | Rejet atomique ; aucun réexamen de la répartition lors d’un retour ou d’une correction de texte |
| 4 — Qualification et archives | Tracer les propositions et décisions ; tester le modèle réel et SDL | Les décisions visibles et persistantes correspondent à celles acceptées, avec limites documentées |

Principales zones à modifier :

- `investigation_state.h`, `investigation.h/.cpp` : disponibilité, décisions, contrat, validation et migration ;
- `game_state.cpp` : sérialisation, lecture des versions et conservation des lieux importés ;
- `turn_runner.cpp/.h` : création du contexte, schéma, préparation du candidat, contrôle de présence et publication ;
- `object_render.cpp` et interactions : sites multiples et conservation des identités ;
- `research_archive.cpp` : demande de distribution, proposition, validation, décision publiée et rejets ;
- `tests/investigation_contract_test.cpp`, `tests/investigation_journey_test.cpp` : nouveaux oracles de décision ;
- documentation d’enquête et état technique : décrire la responsabilité réelle du modèle et les secours.

Réutiliser les modules existants. Les contrats de distribution ne doivent pas imposer une refonte de la géométrie, des prefabs, du mixage audio ou de l’interface. Respecter le workflow de développement de `AGENTS.md` pour l’implémentation.

## 14. Tests et recette

### Tests déterministes du moteur

- Avec la même géométrie, accepter successivement des fixtures proposant zéro indice, un indice différent et plusieurs indices compatibles. Le résultat suit la proposition, jamais une sélection cachée du moteur.
- Accepter un relevé de Vey placé avant l’outil et le cristal. Aucun ordre hérité n’est requis.
- Refuser les identifiants inconnus, indices déjà placés, sites inventés, révisions périmées et combinaisons en collision.
- Après une décision vide, revisiter puis corriger les métadonnées : le lieu reste sans nouvel indice.
- Après plusieurs placements, partir, revenir, examiner, prendre, déposer ailleurs et reprendre : mêmes identités, preuves idempotentes et localisation exacte.
- Tenter un prélèvement depuis l’ancienne mention d’un objet déplacé : aucune seconde instance.
- Annuler après préparation des objets et avant publication : monde, décisions, opportunités disponibles, preuves et cibles audio inchangés.
- Provoquer un rejet de sélection : ne consommer aucun indice et ne publier aucun lieu partiel.
- Fournir une sélection valide avec une prose refusée : le secours conserve cette sélection et n’invente aucune découverte acquise.
- Vérifier le retour et la dernière continuation après placement d’une obstruction, y compris avec plusieurs autres indices présents.
- Charger un export ancien puis créer un nouveau lieu : les anciens lieux restent identiques et les indices déjà placés restent indisponibles.
- Dépasser largement les six événements récents : les décisions et faits utiles restent accessibles.
- Compter les appels : aucune inférence ajoutée à la création normale ; aucun appel de placement lors des revisites.

### Recette Gemma réelle

Observer une petite série de parties et de graines, avec un budget d’exploration explicite. Relever pour chaque lieu la sélection proposée, celle publiée, les indices encore disponibles, les échecs et les tokens.

Examiner la fréquence des lieux vides, des regroupements, des ordres différents et des pistes durablement absentes. Il s’agit de mesures, pas de quotas forcés ni de critères exigeant une variation à chaque graine.

Vérifier que la narration distingue présence et identification, qu’elle ne révèle pas prématurément la propriété d’un outil, et qu’un rapprochement ne devient pas un événement inventé. Contrôler les budgets avec le tokenizer réel.

### Recette SDL

Sur une partie française, vérifier la présence visuelle des objets sélectionnés, leur retrait, les retours et le carnet. Provoquer un contact avec l’obstruction puis son examen. Vérifier le maintien du mixage et de l’animation selon les règles existantes. Les contrats et les secours sont couverts dans les six langues ; distinguer cette couverture de la recette effectivement jouée.

## 15. Critères de terminé

- Le choix du modèle détermine quels indices apparaissent dans chaque nouveau lieu.
- Les choix vides et les regroupements sont des possibilités réelles.
- Aucune fonction C++ n’impose un ordre, un indice de remplacement ou une apparition après N lieux.
- Chaque décision acceptée reste stable pendant la partie, y compris après correction de narration.
- La localisation des objets et les preuves évoluent uniquement par les mécanismes autorisés.
- Les refus et annulations ne publient aucun état partiel.
- La navigation, l’audio et le format des scènes restent cohérents avec les décisions acceptées.
- Les archives permettent de distinguer le choix proposé par le modèle, son acceptation et la découverte ultérieure par le joueur.
- Les tests injectés et les observations du modèle réel sont rapportés séparément.

## Références sur la base examinée

- [Sélection actuelle et préparation des opportunités](https://github.com/astrofra/game-liminal-raytraced-llm-world/blob/af9124c3377bc319586c3a8d6ace6c75a14914c2/src/investigation.cpp#L181).
- [État actuel de l’enquête](https://github.com/astrofra/game-liminal-raytraced-llm-world/blob/af9124c3377bc319586c3a8d6ace6c75a14914c2/src/investigation_state.h).
- [Préparation actuelle dans le moteur de tours](https://github.com/astrofra/game-liminal-raytraced-llm-world/blob/af9124c3377bc319586c3a8d6ace6c75a14914c2/src/turn_runner.cpp#L2779).
- [Création locale et appel narratif](https://github.com/astrofra/game-liminal-raytraced-llm-world/blob/af9124c3377bc319586c3a8d6ace6c75a14914c2/src/turn_runner.cpp#L3753).
- [Fonctionnement et qualification du pilote existant](https://github.com/astrofra/game-liminal-raytraced-llm-world/blob/af9124c3377bc319586c3a8d6ace6c75a14914c2/documentation/ENQUETE_INDICES.md).

Cette roadmap transfère l’initiative de répartition au modèle. Elle conserve un état de monde explicite, vérifiable et durable, sans réécriture rétroactive des lieux acceptés.
