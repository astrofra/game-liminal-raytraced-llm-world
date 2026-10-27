# Roadmap — Une enquête à indices dans Entre les Murs Latents

**Option 2 : preuves persistantes, hypothèses révisables, exploration générative.**  
Suivi du pilote : [implémentation, contrat et qualification](ENQUETE_INDICES.md).
Les sections suivantes conservent le cahier des charges ; le bilan distingue
les mécanismes livrés et leurs limites de vérification.
Date : 27 octobre 2026.  
Dépôt : https://github.com/astrofra/game-liminal-raytraced-llm-world  
Base examinée : `main`, commit `f87b230b42560fd6fe236d8a93ab3b11f993baa5`.

## 1. Intention

Donner une motivation à l'exploration en reliant trois questions : où est passé le prospecteur Vey, qu'est-ce qui fait obstacle dans cet espace, et quel rôle les cristaux jouent-ils dans cette situation ?

Le joueur observe, prélève, conserve des traces, confronte des indices et révise ses hypothèses. Le sens se construit progressivement à partir de ses actions et de ce qu'il découvre.

Les lieux restent improvisés par le LLM, avec leur continuité et leur géométrie conservée. Le scénario définit quelques faits et possibilités de découverte ; il ne prescrit pas une succession d'abri, de portique et de couloir. Aucun retour à un catalogue cyclique de compositions.

Cette roadmap porte sur l'enquête. Elle n'impose ni score de collecte, ni mort de Vey, ni révélation cosmologique, ni fin automatique. Un dénouement pourra ensuite s'appuyer sur les preuves enregistrées.

## 2. Point de départ réel du code

Les trois chaînes de `HardState::unresolved_threats` sont initialisées dans `MakeInitialHardState()` :

- `locate prospector Vey` ;
- `verify the invisible obstruction` ;
- `recover a crystal sample`.

Le canevas initial est aussi présent dans `MakeInitialSoftState()` et `documentation/STORY.md`. Ce dernier décrit encore un parcours ancien ; son statut doit être corrigé avant de l'utiliser comme description de l'implémentation.

| Brique existante | Utilisation dans l'enquête |
|---|---|
| `SessionState`, identités des lieux et liens conservés | Mémoire autoritative des découvertes et de leur localisation |
| `ObjectState`, instances, sources et dépôts | Référence au même échantillon ou objet après transport et revisite |
| Descriptions de référence et observations retenues | Provenance textuelle des observations ; elles ne constituent pas automatiquement une conclusion d'enquête |
| `InvisibleBarrier`, contact et `discovered` | Événement possible de confirmation d'une obstruction |
| Interprète structuré des commandes libres | Compréhension des demandes d'observation, de prélèvement et de carnet |
| Appel narratif et schéma de proposition | Présentation des indices et propositions bornées de découverte |
| Contexte de continuité et budget de tokens | Transmission compacte de la progression, indépendante de l'historique récent |
| Publication des tours et archives | Acceptation atomique et traçabilité des preuves |

Deux lacunes sont à traiter explicitement :

1. Le contexte local actuel ne transmet pas `unresolved_threats` comme progression active ; l'ancien ajout/retrait de chaînes n'est pas un système d'enquête.
2. Dans la base examinée, aucune création de barrière invisible n'a été trouvée dans une nouvelle partie normale ; l'insertion explicite observée est dans le test de navigation. Le simple branchement sur `discovered` laisserait donc cette piste sans occasion de progresser.

## 3. Contrat artistique et responsabilités

### Le scénario établit

- Vey est porté disparu ; ce qui lui est arrivé reste inconnu au début.
- L'expédition comprend une prospection de cristaux et l'examen d'obstructions suspectées.
- Quelques catégories de traces peuvent relier ces trois fils.
- Une information annoncée comme vérifiable doit avoir une occasion effective d'être découverte.

Vey appartient à la fiction propre au projet. Ne pas attribuer ses détails biographiques à la nouvelle de Lovecraft et Sterling.

### Le LLM décide

Il interprète la demande, compose le lieu, situe les possibilités autorisées dans cette composition, décrit les observations et propose des rapprochements entre preuves. Ses formulations peuvent varier selon l'expérience du joueur.

Il utilise les identités et possibilités fournies par le moteur. Il ne déclare pas seul Vey retrouvé, un cristal acquis ou une hypothèse démontrée.

### Le C++ garantit

Identités, provenance, disponibilité, localisation, conditions de découverte, transitions d'état et conservation. Il valide des données structurées et des conditions de jeu ; il n'ajoute pas un parseur de formulations libres comme « examiner le fragment au pied du mur ».

Le choix scénaristique d'autoriser une trace ne doit pas être présenté comme une découverte spontanée du modèle. Archiver distinctement la possibilité fournie, sa mise en scène proposée et ce qui a réellement été publié.

## 4. Les trois fils de l'enquête

| Fil | Question de départ | Premières preuves possibles | Progression recherchée |
|---|---|---|---|
| Vey | Où est passé le prospecteur ? | Équipement identifié, trace de passage ou relevé authentifié | Établir son passage et poursuivre une piste ; ne pas déduire sa mort d'un objet abandonné |
| Obstruction | Pourquoi une direction apparemment libre est-elle impraticable ? | Contact validé puis observation complémentaire | Distinguer le fait de l'obstacle des explications de sa nature ou de sa fonction |
| Cristal | Quel spécimen rechercher, et pourquoi ici ? | Source attestée, prélèvement identifié, association avec une autre trace | Passer du prélèvement à l'examen de son contexte et de sa signification |

Ces exemples constituent des possibilités de contenu. Ils ne doivent pas tous apparaître dans chaque lieu ni devenir trois étapes obligatoires dans un ordre fixe.

Un même indice peut contribuer à plusieurs fils. Par exemple, un instrument identifié comme appartenant à Vey, retrouvé près d'une zone de prélèvement, peut relier son passage à la prospection. La proximité seule ne prouve pas son intention ni son destin.

## 5. Distinguer faits, hypothèses et progression

### Preuve ou constat

Une entrée persistante décrit précisément ce qui est établi, avec sa source : objet acquis, contact enregistré, observation autorisée et acceptée, ou dispositif effectivement examiné.

Un texte d'ambiance mentionnant une masse étrange n'est pas automatiquement une preuve de construction extraterrestre. Une note du joueur reste une note du joueur.

### Hypothèse

Une proposition interprétative référence des preuves qui la soutiennent et, éventuellement, la contredisent. États minimaux : `open`, `supported`, `contradicted`.

Une hypothèse soutenue reste présentée comme telle tant que le scénario ne fournit pas une condition explicite de confirmation. Le pilote n'a pas besoin de probabilités numériques ou d'un moteur général de raisonnement.

### Fil d'enquête

États minimaux : `open`, `in_progress`, `resolved`. La résolution dépend de conditions définies pour la question concernée, pas du nombre de tours ou de mots produits.

Pour Vey, « son passage est confirmé » peut résoudre une sous-question sans résoudre « qu'est-il devenu ? ». Ne pas afficher toute l'enquête comme terminée pour une simple première trace.

### Acquisition et possession

Avoir prélevé un cristal est un événement durable. Porter actuellement ce cristal est une propriété consultée dans le registre d'objets. Le déposer ne supprime pas la découverte ; une demande ultérieure nécessitant l'échantillon vérifie sa localisation actuelle.

## 6. État minimal proposé

Ajouter une petite structure d'enquête à `SessionState`, versionnée et révisée. Garder les identifiants anglais stables et les textes affichés localisés.

| Entrée | Champs essentiels |
|---|---|
| Fil | `id`, `status`, `evidence_ids`, `hypothesis_ids` |
| Preuve | `id`, `kind`, `thread_ids`, `place_id`, `object_id` éventuel, tour, événement/source, fait établi |
| Hypothèse | `id`, proposition, état, preuves favorables et contradictoires |
| Possibilité de découverte | `id`, fait autorisé, lieu ou objet lié, préconditions, état réservé/publié/découvert |
| Ensemble | `schema_version`, `revision`, collections précédentes |

Les preuves déjà publiées restent conservées au-delà du budget des prompts. Les traductions, reformulations et résumés ne créent pas de nouvelles identités.

La possibilité de découverte sert à distinguer un élément préparé mais pas encore observé d'un fait connu du joueur. Sa vérité cachée ne doit pas entrer prématurément dans le contexte visible du narrateur.

Ne pas dupliquer les objets ni leurs coordonnées : les références vont vers les registres existants. Conserver les événements factuels ; les nouvelles interprétations s'ajoutent ou révisent les hypothèses sans effacer les observations précédentes.

## 7. Garantir des découvertes sans figer les lieux

### Politique minimale

Définir une politique de distribution des indices, distincte du générateur de géométrie. Elle privilégie les fils encore sans piste et espace les découvertes pour laisser vivre l'exploration.

Le budget de nouvelles découvertes peut rendre une possibilité admissible ; il ne valide jamais automatiquement une preuve. Une revisite ne consomme pas de nouveau budget et ne fait pas surgir un indice simplement parce que le joueur insiste.

Pour le pilote, viser au moins une occasion concrète par fil au cours d'une courte exploration. Fixer une borne de test explicite, par exemple six nouveaux lieux acceptés dans les fixtures contrôlées. Ajuster ensuite le rythme en partie réelle.

### Réservation et publication

1. Le moteur réserve éventuellement une possibilité autorisée pour un futur lieu.
2. Le générateur reçoit son contenu utile, ses contraintes et les faits déjà connus du joueur.
3. Le LLM compose librement la situation et sa géométrie.
4. Le moteur vérifie la compatibilité de la mise en scène.
5. Le lieu et la possibilité sont publiés ensemble après acceptation du tour.
6. L'observation ou le geste requis rend la preuve acquise.

Si une proposition est rejetée, annulée ou impossible à représenter, la possibilité reste disponible pour une autre tentative. Ne pas consommer un indice dans un lieu jamais publié.

Un échec répété produit un diagnostic technique et conserve l'état. Il ne fabrique ni preuve ni impossibilité physique pour le joueur.

### Compatibilité avec le langage de scène

Le contrat local reste fondé sur les boîtes. Ne pas réintroduire un prefab obligatoire, un plan architectural complet ou un catalogue de scènes pour représenter un indice.

Un petit spécimen peut utiliser la couche mobile existante. Un indice spatial peut être attaché à un lieu, une direction ou un volume déjà validé. Une information uniquement textuelle doit rester compatible avec l'image et avoir une interaction qui permet réellement de l'observer.

## 8. Cas particulier : l'obstruction invisible

Cette piste demande une véritable opportunité de contact, pas seulement une phrase de mission.

- Réserver une direction encore inconnue, sans lien publié, dans un lieu compatible.
- Vérifier que l'approche ne semble pas déjà bouchée par une masse opaque ; sinon, le contact ne démontre pas une obstruction invisible.
- Conserver les liens de retour établis et au moins une continuation utile accessible dans le graphe exploré.
- Enregistrer la barrière avant de pouvoir publier un contact avec elle. L'absence d'observation ne signifie pas que la barrière est absente.
- Lors d'une tentative validée, conserver le lieu, la direction, l'événement et le fait du contact ; l'action refusée ne déplace pas le joueur.
- Un second examen peut enrichir le dossier si une observation complémentaire est effectivement autorisée et réalisable.

Le verre visible des soutiens ne constitue pas cette preuve. Ne pas modifier la topologie des pièces revisitées pour mettre en scène un changement impossible. Le pilote conserve les garanties actuelles de navigation.

Une barrière et sa preuve restent stables après découverte. Si la pose est impossible dans le lieu courant, reporter la possibilité sur un autre lieu compatible.

## 9. Raccord aux objets et à l'interprétation

### Le cristal

Une possibilité autorisée doit identifier le spécimen ou sa source comme pertinent pour l'enquête, avec un identifiant sémantique stable. Ne pas valider la mission par recherche du mot « cristal » dans un libellé traduit.

La prise passe par le mécanisme existant : attestation, identité explicite ou matérialisation depuis une source, portabilité, retrait persistant et publication atomique. Le registre d'enquête référence l'instance créée ; il ne crée pas une seconde récompense.

La preuve d'acquisition est enregistrée une seule fois. Déposer, quitter, revenir et reprendre le même échantillon modifie sa localisation, sans créer un nouvel indice ou un nouveau prélèvement.

### Les traces de Vey

Une pince ordinaire ne devient pas celle de Vey parce que le modèle emploie ce nom. L'identification doit faire partie du contenu autorisé et être révélée par une interaction appropriée.

Le prompt actuel interdit de nouvelles personnes et des inscriptions lisibles. Introduire des exceptions ciblées pour les indices explicitement fournis par le moteur, si le pilote choisit un relevé ou un objet identifié. Conserver l'interdiction d'inventer librement un témoin, une inscription ou une rencontre.

### Les demandes libres

L'interprète comprend des demandes comme « que sait-on de Vey ? », « comparer ce prélèvement à ce que j'ai trouvé avant » ou « faire le point ». Il renvoie une intention structurée et des références admissibles.

Le moteur n'ajoute pas de règles de préfixes pour ces formulations. Les commandes exactes déjà directes restent directes. Le pilote peut limiter les comparaisons à la consultation des preuves disponibles ; il ne prétend pas implémenter toutes les utilisations des outils ou la combinaison des objets.

## 10. Contextes LLM et validation des propositions

Ajouter un contexte compact `investigation` aux appels concernés :

- **Interprétation :** questions actives, objets pertinents, preuves connues et possibilités accessibles dans le lieu courant.
- **Géométrie :** faits scéniques autorisés pour ce lieu, avec contraintes de représentation ; aucune liste imposée de formes architecturales.
- **Narration :** progression après les actions acceptées, découvertes de ce tour et incertitudes à préserver.

Les hypothèses ne sont pas transmises comme géométrie certaine. Les indices préparés dans d'autres lieux et les réponses cachées ne sont pas exposés comme connaissances du joueur.

Utiliser les appels existants. Si nécessaire, enrichir la proposition narrative d'un petit tableau de références de découvertes ou d'hypothèses admissibles. Le modèle ne choisit pas directement le nouvel état de chaque fil et n'attribue aucun score.

Le moteur vérifie identifiants, révision, accès, préconditions, provenance et absence de doublon. Les faits issus d'un transfert ou d'un contact sont déduits des événements moteur ; un texte seul ne prouve pas un geste.

Une annulation conserve tous les états précédents. Si la partie enquête d'une réponse est invalide, aucune preuve ni conclusion associée n'est publiée. Les actions indépendantes déjà validables suivent le mécanisme de publication du tour, avec diagnostic et texte de secours cohérent si nécessaire. Si une découverte est indispensable à l'identité d'un nouveau lieu, rejeter l'ensemble candidat plutôt que publier une promesse incohérente.

Le budget de tokens conserve les preuves nécessaires à l'action et les questions actives, puis réduit les événements anciens. Ne pas ajouter d'appel de résumé ou de planification au pilote. Les schémas doivent rester simples, adaptés au petit modèle local.

## 11. Présentation au joueur

Donner brièvement les raisons de l'expédition au départ, dans la langue choisie. Distinguer le mandat initial des connaissances acquises.

En cours de partie :

- Une découverte importante reçoit une mention située dans la narration.
- Le carnet peut présenter « établi », « supposé » et « à vérifier », avec des références compréhensibles aux lieux ou objets.
- Une demande de bilan fournit une synthèse fidèle des preuves ; elle peut être incomplète et reconnaître les incertitudes.
- Une nouvelle hypothèse n'est pas annoncée comme une victoire.

Éviter de réciter les trois objectifs après chaque tour. Le pilote n'ajoute pas de compteur de cristaux récompensant chaque prise. Le score existant reste inchangé tant qu'une règle distincte n'a pas été décidée.

L'enquête peut donner envie de continuer ou de revenir sur ses pas. Elle doit permettre des découvertes dans des ordres différents. Une restitution d'objet conserve sa trace ; une lecture morale du geste ne devient pas une vérité imposée au joueur.

## 12. Fichiers, migration et archives

| Zone du code | Travail proposé |
|---|---|
| `game_state.h/.cpp` et nouveau petit module d'enquête | État, invariants, sérialisation, initialisation et migration |
| `object_state.h`, `object_interactions.*` | Références stables aux spécimens et événements, sans dupliquer le registre |
| `object_resolution.cpp` | Intentions libres et candidats admissibles d'enquête |
| `turn_runner.cpp/.h` | Distribution des possibilités, contextes, validation et publication des effets |
| `scene_design.cpp`, continuité | Contraintes utiles au lieu et budget du contexte |
| `research_archive.cpp` | Preuves et hypothèses avant/après, propositions, rejets et provenance |
| `sdl_frontend.cpp` | Présentation sobre, seulement si nécessaire au carnet/bilan |
| Documentation | Corriger le statut de `STORY.md` et documenter le fonctionnement réellement livré |

L'état d'enquête devient l'autorité. Ne pas maintenir deux systèmes concurrents avec `unresolved_threats` et les anciens `threats_add/remove`. Pour compatibilité, conserver temporairement le champ historique ou en produire une projection, sans permettre qu'une suppression de chaîne valide une mission.

Un ancien état sans enquête conserve ses lieux et ses objets. Ne pas inférer des preuves à partir de ses seules intentions initiales ni remplir rétroactivement des pièces acceptées. Si l'état ancien ne contient pas d'éléments admissibles, initialiser les fils sans preuve et autoriser les opportunités sur de futures découvertes.

La persistance concerne la même partie et les exports diagnostiques ; cette roadmap n'ajoute pas une reprise de sauvegarde dans SDL. Les archives conservent la politique, les opportunités fournies, les preuves publiées et les conclusions autorisées. Les erreurs et propositions rejetées restent identifiées comme telles.

## 13. Lots d'implémentation

### Lot 1 — État et contrat minimal

Définir les trois fils, les conditions de chaque sous-question et la structure minimale. Implémenter sérialisation, migration et invariants ; préparer l'introduction du mandat. Aucun fil ne se résout sur simple déclaration du modèle.

### Lot 2 — Première boucle de preuve avec le cristal

Autoriser une source pertinente dans un lieu nouveau. La présenter, prélever, déposer dans un autre lieu, revisiter et reprendre le même objet. Vérifier une seule preuve d'acquisition et une localisation toujours exacte.

### Lot 3 — Occasion réelle de contact invisible

Brancher une création bornée de barrière compatible avec la navigation. Publier la barrière, enregistrer le contact puis restituer le constat dans le carnet. Une direction simplement fermée ne suffit pas à valider cette preuve.

### Lot 4 — Traces de Vey et premiers rapprochements

Distribuer deux indices distincts de passage et les relier à l'un des autres fils. Permettre une hypothèse soutenue ou contredite selon les éléments effectivement connus. La situation finale de Vey reste indéterminée tant que sa condition n'est pas satisfaite.

### Lot 5 — Parcours transversal et présentation

Faire le point, comparer des preuves, explorer dans un autre ordre, revisiter longtemps après et vérifier les six langues. Intégrer les contextes bornés et les archives. Effectuer une recette réelle Gemma puis SDL avant publication.

Chaque lot doit être relu et vérifié avant intégration. Respecter les consignes `AGENTS.md` du dépôt. Une délégation éventuelle suit le kit de développement existant ; elle n'ajoute aucune dépendance au runtime du jeu.

## 14. Validation et critères d'acceptation

### Contrôles automatisés significatifs

- Absence de mutation de l'état source lors d'une préparation, d'un rejet ou d'une annulation tardive.
- Proposition avec ID inconnu, source inventée, mauvaise révision, objet distant ou précondition absente : aucun progrès.
- Citation attestée mais sans lien autorisé avec Vey : aucune identification de propriété inventée.
- Répétition de la même observation, revisite ou reprise d'un objet : aucune duplication de preuve.
- Deux observations distinctes peuvent soutenir une même hypothèse ; une preuve pertinente peut la contredire sans effacer les faits.
- Acquisition puis dépôt du cristal : preuve conservée, possession mise à jour ; aucun score supplémentaire.
- Contact avec une barrière invisible : preuve publiée une seule fois ; collision avec un mur opaque ou simple sortie fermée : aucune validation équivalente.
- Retour toujours possible ; aucune barrière ajoutée sur un lien déjà publié ou au prix de la dernière continuation utile.
- Indice candidat dans une scène rejetée : possibilité non consommée.
- Historique dépassant largement les six événements transmis : enquête conservée et contexte utile disponible.
- Sérialisation ancienne/nouvelle, identifiants stables et textes localisés.

### Recette intégrée

Dans une fixture d'exploration courte, découvrir des éléments des trois fils dans deux ordres différents. Prendre un échantillon, changer de lieu, le déposer, revenir plus tard et le reprendre. Rencontrer une obstruction puis examiner ses conséquences. Trouver des traces de Vey et demander un bilan.

Le bilan doit citer les faits réellement acquis et conserver les incertitudes. Il ne doit ni inventer une rencontre, ni déclarer l'échantillon encore porté s'il est déposé ailleurs, ni transformer une hypothèse en événement.

Les tests injectés vérifient le contrat et la conservation ; ils ne démontrent pas la compréhension du modèle réel. La recette Gemma examine des demandes reformulées et des pronoms, puis la cohérence de ce qui est affiché. Les vérifications SDL confirment que les indices visibles et les objets correspondent à l'état accepté.

### Définition de terminé pour le pilote

- Le joueur connaît les raisons de son expédition.
- Chaque fil possède une occasion concrète et testée de progresser.
- Les découvertes persistent et peuvent être reliées entre elles.
- Les hypothèses restent distinctes des faits.
- La composition des lieux reste libre et les liens existants restent fiables.
- Le partage des responsabilités LLM/C++ et le nombre d'appels de base sont conservés.
- L'audio, les corrections géométriques, les objets et leurs identités restent cohérents.
- Une demande de bilan rend perceptible le cheminement de l'enquête, même si tout n'est pas résolu.

## Références techniques

Liens figés sur la base examinée :

- [Intentions et état initial](https://github.com/astrofra/game-liminal-raytraced-llm-world/blob/f87b230b42560fd6fe236d8a93ab3b11f993baa5/src/game_state.cpp#L1305).
- [État de session](https://github.com/astrofra/game-liminal-raytraced-llm-world/blob/f87b230b42560fd6fe236d8a93ab3b11f993baa5/src/game_state.h).
- [Registre d'objets](https://github.com/astrofra/game-liminal-raytraced-llm-world/blob/f87b230b42560fd6fe236d8a93ab3b11f993baa5/src/object_state.h).
- [Interprétation structurée](https://github.com/astrofra/game-liminal-raytraced-llm-world/blob/f87b230b42560fd6fe236d8a93ab3b11f993baa5/src/object_resolution.cpp).
- [Continuité et publication des tours](https://github.com/astrofra/game-liminal-raytraced-llm-world/blob/f87b230b42560fd6fe236d8a93ab3b11f993baa5/src/turn_runner.cpp).
- [Canevas narratif à remettre à jour](https://github.com/astrofra/game-liminal-raytraced-llm-world/blob/f87b230b42560fd6fe236d8a93ab3b11f993baa5/documentation/STORY.md).

Cette roadmap conserve le cahier des charges initial ; le [bilan du pilote](ENQUETE_INDICES.md) décrit les mécanismes livrés et les vérifications observées.
