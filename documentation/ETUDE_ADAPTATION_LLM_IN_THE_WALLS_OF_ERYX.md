# Étude : entraîner le LLM à partir d’In the Walls of Eryx

Date : **2026-10-24**. Projet : *Within the Latent Walls / Entre les Murs Latents*.

**Cadre retenu : œuvre de recherche-création, non commercialisée et non destinée à la duplication, avec liberté d’interprétation artistique.** L’objectif est d’impliquer davantage le LLM dans la transformation du matériau littéraire. Le matériel cible reste inchangé : RTX 4060 8 Go, exécution locale. Aucun achat de matériel ni service d’entraînement distant n’entre dans cette proposition.

Statut : **étude**, sans constitution de jeu d’entraînement, modification du runtime ou apprentissage exécuté. Inspection du répertoire de travail à partir du commit `b39475110e094e5c518b7edeeca721d8f079ca6c`. Ce troisième document complète les études [Gemma 4 E4B](./ETUDE_MIGRATION_MINISTRAL_GEMMA_E4B.md) et [Ministral / llama.cpp](./ETUDE_MISE_A_JOUR_MINISTRAL_LLAMA_CPP.md).

## 1. Proposition centrale

**Oui, une adaptation du modèle à partir de la nouvelle est possible et artistiquement pertinente.** La voie recommandée est un petit adaptateur LoRA entraîné sur des situations et des réponses élaborées à partir du texte, avec une expérience séparée d’apprentissage sur le texte brut. Cela permettrait d’étudier deux transformations différentes : l’imprégnation par une œuvre et l’apprentissage d’une manière d’interpréter des situations de jeu.

Le résultat recherché serait un **modèle Eryx situé**, plus sensible à certaines relations : une mesure contredit une perception ; une ressource modifie un jugement ; un trajet paraît familier sans être reconnu ; un objet convoité détourne l’attention d’une issue. Ce déplacement est plus intéressant pour le projet qu’une accumulation de références à Lovecraft ou qu’une imitation automatique de sa prose.

La préparation du corpus ferait partie du travail d’auteur : choisir les situations, les interpréter, écrire leurs transpositions et décider quelles contradictions méritent de devenir jouables. L’adaptateur serait alors une trace matérielle de cette lecture, inspectable par ses effets et réversible.

Trois éléments doivent rester distincts :

- **Les poids adaptés** orientent les dispositions du modèle : choix de détails, interprétation, types de propositions.
- **Le corpus consultable** fournit des passages et leur provenance lorsque cela est utile.
- **L’état du jeu** conserve les faits de la partie : lieux, inventaire, ressources et actions déjà validées.

Entraîner le modèle ne lui donne ni mémoire automatique des parties, ni pouvoir supplémentaire dans le moteur. Pour modifier son rôle, il faut aussi définir ce que l’application lui demande et quelles propositions elle accepte.

## 2. Le matériau disponible

Le projet contient déjà [IN_THE_WALLS_OF_ERYX.md](./IN_THE_WALLS_OF_ERYX.md), transcription anglaise de la nouvelle de H. P. Lovecraft et Kenneth Sterling, publiée en octobre 1939. Sa notice renvoie à la transcription et à l’historique éditorial du H. P. Lovecraft Archive. Sources : [texte de référence](https://www.hplovecraft.com/writings/texts/fiction/iwe.aspx), [historique de publication](https://www.hplovecraft.com/writings/fiction/iwe.aspx).

### Mesure effectuée sur le fichier local

| Élément | Observation |
|---|---|
| Langue | Anglais |
| Début du corps compté | Premier paragraphe du récit, après les notices documentaires |
| Volume | **11 957 mots lexicaux**, environ **66 459 caractères** |
| Découpage Markdown | 123 blocs non vides séparés par des lignes blanches, titres et séparateurs compris |
| SHA-256 du fichier | `de629c4851eb86a9687ad4a35f0b46f4592adabebc7cd2914fab8fdf972ed2a6` |

Le comptage des mots reconnaît les séquences alphanumériques et les mots composés par apostrophe ou trait d’union. Il ne s’agit **pas d’un comptage de tokens** : celui-ci devra utiliser le tokenizer exact du modèle retenu. Un ordre de grandeur de quelques dizaines de milliers de tokens est plausible, sans constituer une mesure.

C’est un corpus très petit à l’échelle d’un LLM. Il permet une spécialisation expérimentale ; il ne suffit pas à former un modèle général depuis zéro. Découper le même texte en milliers de fenêtres ou le paraphraser massivement augmente le nombre d’exemples, sans créer autant d’observations littéraires indépendantes.

Le texte est peut-être déjà partiellement présent dans les données de préentraînement du modèle. Rien dans cette étude ne permet de l’établir. Une bonne réponse à un questionnaire sur la nouvelle, après adaptation, ne prouverait donc pas à elle seule que l’apprentissage a produit le résultat.

### Une source, plusieurs positions de lecture

La nouvelle offre notamment le journal d’un prospecteur, ses essais de mesure, ses interprétations des habitants de Vénus et un rapport final de compagnie. Ces voix n’ont pas la même position ni les mêmes intérêts. Le dénouement rapporte une ouverture proche que le prospecteur épuisé n’a pas trouvée : il ne faut pas transformer automatiquement cette situation en preuve que le labyrinthe littéraire modifie réellement sa topologie.

Les mutations de connexions du projet sont une **transposition artistique explicite**, décrite dans [STORY.md](./STORY.md) et [SPEC.md](./SPEC.md). L’adaptation devra préserver la différence entre ce que dit la source, ce qu’un personnage croit, et ce que l’œuvre interactive décide d’inventer.

La notice historique signale aussi le vocabulaire colonial et déshumanisant du narrateur. Le conserver dans le document source permet l’étude ; l’absorber indistinctement comme voix par défaut du jeu produirait une autre orientation artistique. Une annotation des voix et de leur évolution permet de travailler cette tension au lieu de l’aplatir. Ce choix relève ici de l’écriture du projet.

Le cadre non commercial demandé est enregistré. Les crédits des deux auteurs, la provenance de la transcription et la notice existante accompagnent le corpus de travail ; ce document ne formule pas de conclusion juridique sur le statut du texte ou d’une traduction particulière.

## 3. Que voudrait dire « entraîner sur le roman » ?

Plusieurs opérations souvent confondues produisent des effets différents.

| Méthode | Modification des poids ? | Ce qu’elle peut apporter | Limite pour Eryx |
|---|---|---|---|
| Consigne et dossier de motifs dans le prompt | Non | Cadre interprétatif immédiat, facile à modifier | Dépend du contexte fourni à chaque appel |
| Passages retrouvés dans le corpus, ou RAG | Non dans la proposition présente | Ancrage précis, provenance, sélection de fragments pertinents | N’apprend pas à lui seul une manière durable d’agir |
| Poursuite d’apprentissage sur le texte brut | Oui, éventuellement via LoRA | Familiarité avec vocabulaire, associations, rythme et formes du récit | Risque de répétition, mémorisation et dérive vers la continuation anglaise |
| Adaptation supervisée sur des situations Eryx | Oui, via LoRA/QLoRA | Apprendre à transformer un état et une commande en proposition conforme au projet | Demande des exemples écrits et vérifiés |
| Ajustement de tous les paramètres | Oui | Transformation plus large | Inadapté au petit corpus et à la cible 8 Go |
| Entraînement depuis zéro | Oui | Construction d’un modèle neuf | Hors échelle pour ce matériau et ce matériel |

LoRA désigne une manière de modifier peu de paramètres ; ce n’est pas un objectif d’apprentissage. On peut utiliser un LoRA pour apprendre à poursuivre le texte brut ou pour apprendre à répondre à des situations annotées. QLoRA conserve le modèle principal quantifié et gelé, tout en entraînant ces petits ajouts. Sources : [LoRA dans PEFT](https://huggingface.co/docs/peft/main/en/conceptual_guides/lora), [article QLoRA](https://arxiv.org/abs/2305.14314).

Les travaux sur l’adaptation au domaine montrent l’intérêt possible d’une phase supplémentaire d’apprentissage, mais leurs résultats ne démontrent pas qu’une seule nouvelle suffise pour améliorer notre jeu. De même, le RAG est une méthode d’accès à une mémoire documentaire ; son intérêt ici doit être évalué dans un contexte artistique, différent des tâches factuelles de ses études fondatrices. Sources : [Gururangan et al., 2020](https://aclanthology.org/2020.acl-main.740/), [Lewis et al., 2020](https://arxiv.org/abs/2005.11401).

### Pourquoi ne pas commencer par injecter tout le texte ?

La configuration initiale du jeu est à 4 096 tokens, partagés avec les instructions, l’état, l’historique et la réponse. Le récit complet n’y tient vraisemblablement pas. Le runtime peut agrandir le contexte automatiquement, mais ce mécanisme ne garantit ni une marge mémoire suffisante ni une latence acceptable.

Un petit dossier de motifs ou quelques fragments ciblés offre un témoin plus économique. Pour ce corpus, un index par motifs et une recherche lexicale peuvent suffire au premier essai ; une base vectorielle et un autre modèle d’embeddings ne sont pas indispensables. Des mots-clés bilingues doivent relier les commandes françaises au texte anglais.

## 4. Comment impliquer davantage le LLM dans l’œuvre ?

### Lui confier une lecture opératoire

Le modèle pourrait sélectionner ce qui devient sensible dans une situation : le décalage d’une mesure, l’usure d’un repère, la différence entre proximité visible et accessibilité, le glissement d’une certitude vers une hypothèse. Ces choix peuvent nourrir les champs existants de narration, de clarification, de continuité et de contraintes de scène.

| Motif lu dans la source | Transposition possible dans le jeu | Condition à préserver |
|---|---|---|
| Objet visible mais inaccessible | Le scanner indique une proximité qui ne correspond à aucun passage connu | Ne pas inventer un déplacement déjà accompli |
| Cartographie par contact | Le joueur utilise une marque, un instrument ou un objet pour sonder une limite | Une observation nouvelle doit avoir une cause jouable |
| Épuisement et perception | Le texte devient plus hésitant et les mesures moins faciles à interpréter | Les ressources et coordonnées validées restent fiables pour le moteur |
| Retour trompeur | Un repère reconnaissable contredit un trajet mémorisé | Distinguer contradiction authored, proposition et fait accepté |
| Logique extractive | Un cristal promet un gain tout en rendant la poursuite de l’extraction problématique | Laisser au joueur une décision effective |
| Rapport rétrospectif | Une seconde voix relit les traces d’une partie | Ne pas présenter cette voix comme nécessairement omnisciente |

Exemple de réponse nouvelle, rédigée pour illustrer une direction et **non issue d’un modèle entraîné** :

> La balise reste en vue, mais le gant bute avant de la rejoindre. La mesure donne trois mètres ; elle ne dit rien du chemin. Vous pouvez marquer ce point ou poursuivre le contact vers l’ouest.

Ce texte ferait apparaître une relation perceptive, un écart de mesure et une action possible. Le but n’est pas que le modèle récite un passage précis à chaque rencontre.

### Définir son autorité séparément de son apprentissage

Le code actuel possède déjà `ProtectCanonicalTopologyFromNarrativeDelta()` et `RestoreAuthoritativeCanonicalTopology()`. Les propositions libres de mutation topologique ne sont pas une fonctionnalité générale activée par l’ajout d’un adaptateur.

Deux niveaux sont donc à distinguer :

1. **À court terme** : adapter les réponses dans les contrats existants et la génération des nouvelles salles. Le modèle intervient davantage dans leur interprétation et leur composition sémantique.
2. **Recherche ultérieure** : lui faire proposer une modification explicite de connexion ou de barrière, avec préconditions et validation par le moteur. Cela demande un schéma et un mécanisme d’application supplémentaires, indépendamment de l’entraînement.

L’espace d’intervention peut être élargi sans lui donner l’autorité sur tous les faits. C’est précisément la négociation entre ses propositions et les contraintes du monde qui peut devenir perceptible dans l’œuvre.

L’entraînement reste effectué en atelier. Pendant une partie, l’historique et les traces de jeu évoluent, mais les poids restent fixes. Apprendre après chaque action introduirait une dérive difficile à analyser et un coût incompatible avec ce premier protocole. Les sessions pourront alimenter une version ultérieure du corpus, après sélection et réécriture par l’auteur.

## 5. Construire un corpus d’adaptation

### Trois couches à conserver séparément

**A. Archive littéraire.** Conserver le fichier source intact, avec son empreinte, puis produire une copie de travail : texte nettoyé des notices techniques, titres et changements de voix préservés. Affecter des identifiants stables aux passages ; ne pas apprendre les en-têtes documentaires comme s’ils appartenaient au récit.

**B. Lecture annotée.** Regrouper les paragraphes en unités de situation. Pour chacune, relever le point de vue, les faits rapportés, les hypothèses du narrateur, les motifs, les ressources en jeu et les transpositions possibles. Conserver plusieurs lectures lorsque le passage les autorise.

**C. Situations interactives.** Écrire des exemples `état + commande + contraintes -> réponse finale`. Leurs sorties suivent les contrats réels produits par `BuildTurnResultSchemaText()` ou `BuildGeneratedRoomSchemaText()`. Les métadonnées littéraires restent à côté de ces exemples ; elles ne deviennent pas par accident de nouvelles clés obligatoires du jeu.

Chaque exemple devrait porter un identifiant, ses passages sources, son groupe de scénario, son auteur ou procédé de génération, sa version de schéma et son statut de relecture. Une proposition synthétique n’entre dans l’ensemble d’entraînement qu’après vérification littéraire et vérification du contrat.

### Familles d’exemples utiles

- Interpréter une commande ambiguë sans prétendre connaître une sortie absente de l’état.
- Décrire une surface invisible à partir d’un contact ou d’une mesure.
- Préserver une ressource et un objet déjà présents malgré une narration troublée.
- Composer une salle jouable à partir de quelques motifs, sans transformer tout lieu en piège.
- Répondre sobrement à une action ordinaire : attendre, regarder, revenir ou vérifier un instrument.
- Distinguer les convictions d’un personnage des faits du monde.
- Produire plusieurs transpositions d’un motif dans des situations différentes, sans forcer la fin du récit original.
- Clarifier une action impossible ou insuffisamment spécifiée.

Inclure des exemples où aucune anomalie ne survient. Sinon, l’adaptateur risque d’associer « Eryx » à l’obligation de produire un mur invisible, une panne et une catastrophe à chaque tour.

Les mauvaises réponses peuvent servir à l’annotation ou au jeu d’évaluation. Pour une première adaptation supervisée, ne pas les mélanger aux réponses attendues sans format explicite de correction : on entraînerait aussi leurs défauts. L’apprentissage par préférences pourra être étudié plus tard si un corpus de comparaisons artistiques suffisant existe.

### Langue, échelle et découpage

La source est anglaise ; les textes joueur visés sont notamment français. Préparer des consignes et des réponses françaises originales, avec des identifiants et clés techniques anglais comme dans le jeu. Une simple traduction automatique intégrale ne remplace pas ce travail de transposition. Conserver aussi quelques cas dans les autres langues réellement utilisées pour détecter une régression.

Proposition d’échelle, **sans prétendre à un seuil scientifique** :

- pilote de 60 à 100 situations relues pour vérifier toute la chaîne ;
- corpus de travail de 300 à 600 exemples si le pilote montre un effet intéressant ;
- répartition indicative : 50 % de tours, 20 % de nouvelles salles, 15 % de clarification/continuité et 15 % d’actions ordinaires sans anomalie.

Le nombre de tokens sera calculé après assemblage des vrais prompts. Un budget de 512 tokens peut être suffisant pour une expérience de continuation brute, mais trop court pour le schéma et l’état d’un tour complet. Ne pas tronquer systématiquement les réponses structurées pour faire rentrer l’entraînement en mémoire.

Répartir les groupes de situations, par exemple en 70 % entraînement, 15 % validation et 15 % test, **avant** d’en fabriquer les variantes. Tous les dérivés d’un même passage ou scénario restent dans la même partition. Des fenêtres presque identiques réparties au hasard donneraient une illusion de généralisation.

Si l’expérience sur texte brut utilise toute la nouvelle, un passage de cette nouvelle ne peut plus servir de texte inédit de test pour cette expérience. L’évaluation commune doit alors porter sur des situations nouvelles écrites séparément. Les adaptations et les prompts de validation ne doivent pas être révisés en consultant continuellement le jeu de test final.

## 6. Deux expériences d’entraînement complémentaires

### Expérience A : imprégnation par le texte brut

Objectif : observer ce que produit une courte poursuite d’apprentissage autorégressive sur la nouvelle, au moyen d’un adaptateur. Le modèle prédit les tokens suivants dans des segments continus, avec les changements de section préservés.

Bénéfice plausible : renforcer certaines associations matérielles et certaines formes de focalisation. Risques : répétition de noms et de phrases, continuation automatique de l’intrigue, réponse en anglais, confusion entre voix narrative et règles du jeu, dégradation du suivi d’instructions.

Cette expérience répond directement à l’idée d’« entraîner sur l’œuvre ». Elle mérite une branche identifiable même si elle n’est pas la meilleure pour jouer. Un modèle qui se met à répéter, à surinterpréter ou à rejouer un événement pourrait constituer un résultat de recherche-création ; il faudrait le documenter comme tel, plutôt que le déclarer automatiquement supérieur.

Sur un corpus aussi petit, multiplier les époques expose surtout le modèle aux mêmes séquences. Observer plusieurs points d’arrêt précoces ; une baisse de perte d’apprentissage ne prouve pas un enrichissement artistique.

### Expérience B : adaptation supervisée des comportements

Objectif : apprendre les transpositions interactives préparées dans la section précédente. C’est la voie recommandée pour intégrer un adaptateur à la version jouable.

La perte d’apprentissage doit viser les réponses attendues. TRL propose des jeux `prompt-completion` et des jeux de conversations ; le masquage des seuls messages assistant dépend notamment du support du template. Inspecter les tokens réellement supervisés avant d’entraîner : une mauvaise configuration pourrait apprendre principalement le long schéma d’entrée. Source : [documentation SFTTrainer](https://huggingface.co/docs/trl/sft_trainer).

Commencer avec le checkpoint **Instruct**, sans mode de réflexion supplémentaire, pour isoler l’effet de l’adaptation. Le texte littéraire n’est pas un corpus de traces de raisonnement ; ajouter des monologues synthétiques ne démontre pas que le modèle acquiert une meilleure logique. Des justifications courtes, rédigées par l’annotateur pour expliquer un choix de transposition, peuvent rester des métadonnées de recherche.

Une combinaison « imprégnation brute puis adaptation supervisée » est possible, mais seulement après comparaison des deux branches séparées. Sinon, on ne saura pas quelle opération produit l’effet observé.

## 7. Faisabilité sur RTX 4060 8 Go

**QLoRA est la piste d’entraînement réaliste ; la réussite sur Ministral 8B doit être vérifiée par un petit essai mémoire.** Charger un GGUF de 5,2 Go pour générer ne prouve pas que le même modèle pourra être entraîné : il faut aussi conserver des activations, calculer les gradients des adaptateurs et maintenir l’état de l’optimiseur.

Un ajustement complet du 8B en 16 bits dépasse déjà 8 Go pour les poids seuls, avant gradients et optimiseur. Il est exclu du protocole. Les minima génériques publiés par Unsloth donnent 6 Go pour un 8B en QLoRA et 3,5 Go pour un 3B, mais précisent qu’ils varient selon le modèle et la configuration. Ce ne sont pas des mesures garanties pour Ministral 3 avec nos séquences. Source : [exigences Unsloth](https://unsloth.ai/docs/get-started/fine-tuning-for-beginners/unsloth-requirements).

### Configuration de départ à éprouver

| Paramètre | Proposition de pilote, non validée |
|---|---|
| Modèle | Ministral 3 8B Instruct ; repli sur 3B si nécessaire |
| Base d’entraînement | Checkpoint Hugging Face compatible, à révision fixée, avec branche textuelle et tokenizer identifiés |
| Quantification pendant l’apprentissage | 4 bits, typiquement NF4 avec double quantification |
| Paramètres entraînés | Adaptateurs LoRA textuels ; base et composantes vision gelées |
| Rang LoRA | 8 pour commencer ; 16 seulement si la marge le permet |
| Modules | Projections d’attention identifiées dans le modèle ; extension éventuelle après mesure |
| Micro-batch | 1 |
| Longueur | Mesurer 512 puis 1 024 tokens ; augmenter seulement si les exemples complets et la mémoire l’exigent/permettent |
| Économie mémoire | Gradient checkpointing, cache de génération désactivé pendant l’entraînement |
| Accumulation | Ajustable pour le batch effectif ; ne réduit pas la mémoire d’une séquence unique |
| Apprentissage | Petit balayage de taux, par exemple `2e-5` et `1e-4`, avec validation et arrêt précoce |

Cette grille est un protocole d’essai, pas une recette démontrée. Contrôler la précision de calcul effectivement supportée, le nombre de paramètres entraînables et le pic VRAM lors d’une étape complète, validation comprise. Éviter d’exécuter simultanément l’inférence du jeu.

Unsloth documente le fine-tuning de Ministral 3 ; PEFT documente la préparation des modèles quantifiés et leur adaptation. Il faut figer un ensemble compatible de versions PyTorch, Transformers, PEFT, TRL, bitsandbytes et, si utilisé, Unsloth, après un test réel. Sources : [guide Ministral 3](https://unsloth.ai/docs/models/tutorials/ministral-3), [quantification et PEFT](https://huggingface.co/docs/peft/developer_guides/quantization).

L’environnement d’entraînement Python reste distinct de l’application C++ distribuée. Windows natif ou WSL2 sur la même machine peuvent servir selon les dépendances disponibles ; WSL2 n’augmente pas la VRAM. Aucun cloud n’est nécessaire au plan proposé. Vérifier aussi la RAM et l’espace disque disponibles pour les checkpoints et exports avant le pilote.

Si le 8B dépasse la mémoire, réduire les paramètres du pilote compatibles avec des exemples complets, puis essayer le 3B. **Un adaptateur entraîné sur le 3B ne se charge pas sur le 8B.** L’essai 3B doit être comparé à son propre modèle non adapté ; il ne prouve pas ce que donnerait un 8B adapté. Gemma demanderait également un entraînement et une qualification séparés.

## 8. Retour dans l’application native

### Le GGUF actuel n’est pas le point de départ de QLoRA

Le chemin PEFT/Unsloth proposé utilise des poids et une configuration Hugging Face, puis une quantification d’entraînement. Il ne modifie pas directement le fichier GGUF Q4_K_M du jeu. La quantification NF4 de l’apprentissage et Q4_K_M de l’inférence sont deux formats distincts.

Fixer l’identité de la base : famille, taille, variante Instruct, révision, tokenizer, template et origine des poids. Les distributions FP8, BF16 et leurs conversions ne doivent pas être supposées identiques par leur seul nom. Si l’identité avec le GGUF actuel n’est pas assurée, reconvertir une base de référence cohérente et mesurer cette base **sans adaptateur** avant de mesurer l’adaptation.

### Deux chemins d’export

**Adaptateur séparé, recommandé pour l’expérience.** Convertir les matrices LoRA au format attendu par `llama.cpp`, puis les charger avec la base compatible. Cela facilite la comparaison avec/sans adaptation et évite de remplacer les poids de référence.

Le dépôt contient déjà [convert_lora_to_gguf.py](../vendor/llama.cpp/convert_lora_to_gguf.py), une conversion textuelle [Mistral3/Ministral3](../vendor/llama.cpp/conversion/mistral3.py) et les API `llama_adapter_lora_init()`, `llama_set_adapters_lora()` et `llama_adapter_lora_free()` dans [llama.h](../vendor/llama.cpp/include/llama.h). Leur présence rend la voie plausible ; **elle ne valide pas encore la conversion des tenseurs précis produits par l’entraînement choisi**.

**Poids fusionnés, solution secondaire.** Fusionner l’adaptateur dans la base de référence, exporter puis quantifier en GGUF. Cette voie simplifie le chargement du jeu mais demande davantage de mémoire de travail et de stockage, et rend l’analyse de l’effet de quantification plus importante. Ne pas en faire le premier chemin sur la machine cible.

Faire un aller-retour complet sur un minuscule adaptateur pilote avant de préparer un grand corpus : quelques étapes d’apprentissage, sauvegarde, conversion, chargement et comparaison des sorties en Python puis dans `llama.cpp`. Un problème de nommage ou de support d’architecture doit être découvert à ce stade.

### Modifications à prévoir

| Surface | Évolution envisagée |
|---|---|
| Corpus et scripts hors runtime | Extraction, annotations, partitions, entraînement, export et manifestes reproductibles |
| `src/llm_runtime.h/.cpp` | Chemin d’adaptateur et échelle, chargement associé au modèle, activation sur le contexte, libération et diagnostics |
| `src/main.cpp` | Option de sélection de l’adaptateur, sans nouveau format de monde |
| `src/turn_contract.cpp` | Dossier de motifs ou passages ciblés, si cette condition expérimentale est retenue |
| `src/turn_runner.cpp` | Provenance des générations, mesures et maintien des validateurs existants |
| Validation et journal | Configuration exacte, base, adaptateur, corpus, graines, observations et cas rejetés |

La copie actuelle de `llama.cpp`, déclarée au commit `6b36c2305644fd30db7cce3f4840c74574f31ce9`, possède les primitives LoRA. Une mise à jour peut être nécessaire en cas de défaut de conversion ou d’exécution ; elle n’est pas un préalable établi par cette étude.

Les valeurs d’échelle de l’adaptateur pourraient devenir une variable d’atelier, par exemple désactivé, atténué, nominal. Elles ne constituent pas un réglage linéaire garanti de « fidélité littéraire ». Changer l’adaptateur en présence d’un cache KV réutilisé exige de réinitialiser le contexte concerné ; le modèle et son historique calculé doivent rester cohérents.

## 9. Évaluer une transformation artistique, pas seulement une perte

### Comparaison proposée

| Condition | Poids | Contexte littéraire |
|---|---|---|
| T0 | Base non adaptée | Prompt actuel du jeu |
| T1 | Même base | Dossier de motifs écrit pour l’expérience |
| T2 | Même base | Même dossier + passages ciblés |
| T3 | Adaptateur sur texte brut | Même dossier que T1 |
| T4 | Adaptateur supervisé | Même dossier que T1 |
| T5, optionnelle | Adaptateur supervisé | Même dossier + mêmes règles de sélection de passages que T2 |

T3/T1 et T4/T1 isolent l’effet des poids ; T2/T1 mesure l’accès au corpus. T5/T2 permet ensuite de vérifier si l’adaptation apporte quelque chose en présence des passages. Conserver le même modèle de base, sa quantification et le runtime pour chaque comparaison directe.

Sur 40 à 60 situations inédites et quelques sessions complètes, proposer une lecture à l’aveugle des réponses, puis documenter les désaccords. Une évaluation par l’auteur est recevable pour un premier travail d’atelier ; des lecteurs supplémentaires apporteraient ensuite des interprétations différentes. Ne pas utiliser le modèle lui-même comme seul juge de sa transformation.

### Axes d’observation

- **Ancrage Eryx** : les motifs deviennent-ils des relations jouables, ou seulement un vocabulaire reconnaissable ?
- **Singularité** : le modèle produit-il des situations nouvelles, ou rejoue-t-il toujours le même piège ?
- **Focalisation** : sait-il distinguer mesure, expérience, hypothèse et discours de compagnie ?
- **Ouverture** : le joueur conserve-t-il plusieurs gestes possibles, sans être forcé vers le dénouement de la nouvelle ?
- **Continuité** : les objets, ressources, lieux et traces persistent-ils correctement ?
- **Qualité de langue** : le français reste-t-il naturel, bref et lié à l’action ?
- **Rythme** : la nouvelle orientation augmente-t-elle la latence, la longueur ou le besoin de réparations ?
- **Répétition** : quels fragments et motifs reviennent, à quelle fréquence, et avec quel effet esthétique ?

Les mesures techniques complètent la lecture : validité JSON au premier essai, complétude, taux de réparation, respect des invariants, longueur, temps total et mémoire. Pour la proximité au texte source, mesurer les segments répétés dans la même langue et effectuer une lecture bilingue ; l’absence de mots identiques en français ne démontre pas l’absence de paraphrase mémorisée.

Tester aussi des commandes ordinaires et des configurations qui contredisent l’intrigue originale : passage effectivement libre, retour réussi, refus d’extraire un cristal. Un modèle impliqué dans l’œuvre doit répondre au joueur présent, pas imposer à toutes les parties le destin du personnage littéraire.

Conserver les cas où l’adaptation échoue. Ils peuvent renseigner la recherche : le récit absorbé rigidifie-t-il le monde ? Le texte source augmente-t-il la propension à prendre une perception pour un fait ? Une version moins cohérente techniquement produit-elle un trouble intéressant, et à quel prix pour la possibilité d’agir ? Ces questions demandent des traces commentées, pas un score unique.

## 10. Plan de réalisation et décision proposée

1. **Établir la lecture de travail** : passages identifiés, voix, motifs et écarts assumés entre nouvelle et jeu ; produire le dossier commun à T1–T5.
2. **Vérifier la chaîne technique** : pilote QLoRA très court, mesure mémoire et aller-retour jusqu’au runtime natif. Repli 3B si le 8B ne tient pas dans des conditions utiles.
3. **Préparer le pilote supervisé** : 60 à 100 situations relues, contrats complets, groupes séparés et test indépendant.
4. **Comparer les premiers effets** : modèle d’origine, prompt enrichi, adaptateur brut et adaptateur supervisé. Arrêter une branche si elle n’apporte rien d’observable.
5. **Approfondir la version intéressante** : enrichir le corpus, éventuellement ajouter les passages ciblés, réaliser des sessions et documenter les transformations.
6. **Décider du rôle dans l’œuvre** : adaptateur permanent, variable d’atelier, version expérimentale distincte ou simple résultat documenté. L’ouverture à des mutations topologiques est un lot logiciel séparé.

Ordre de grandeur de travail humain : 2 à 4 jours pour la lecture annotée et le pilote de données, 1 à 3 jours pour la qualification technique, puis 3 à 6 jours pour les essais comparatifs et leur analyse, soit **environ 6 à 13 jours**, hors élargissement important du corpus. Ces estimations ne sont pas des engagements. Le temps GPU doit être extrapolé à partir d’un débit d’entraînement mesuré, du nombre de tokens réellement supervisés et des passages effectués ; aucun temps d’apprentissage local n’a été mesuré ici.

**Recommandation finale : engager l’expérience, en faisant de l’adaptation supervisée par LoRA la piste principale et de l’apprentissage sur la nouvelle brute une branche de recherche explicite.** Le corpus consultable sert de témoin et de mémoire documentaire. L’apport recherché est une transformation identifiable de la manière dont le modèle lit et propose le monde d’Eryx, dont l’auteur peut ensuite décider la place dans l’œuvre.
