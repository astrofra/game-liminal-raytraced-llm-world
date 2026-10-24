# Étude du remplacement de Ministral par Gemma 4 E4B

Date : **2026-10-24**. Projet : *Within the Latent Walls / Entre les Murs Latents*.

Statut : **étude de faisabilité et recommandation**, sans migration ni benchmark d'inférence exécutés. Analyse du répertoire de travail, y compris ses modifications non commitées ; référence Git : `b896babfe2546df2bcc49519bf1e4f6ba0907125`. Les observations décrivent le code présent, pas uniquement cette révision.

## 1. Recommandation

**Gemma 4 E4B mérite un prototype comparatif, avec réflexion activée seulement pour les tâches qui en bénéficient. Un remplacement immédiat de Ministral n'est pas encore justifié.** Le bénéfice recherché est une meilleure interprétation des commandes et une meilleure cohérence des propositions narratives et spatiales. Il reste à le démontrer sur Eryx.

Trois constats déterminent la décision :

- Le changement exige une adaptation de l'intégration C++, même sans réflexion. Le moteur Gemma 4 existe déjà dans la dépendance embarquée, mais le chemin de formatage des conversations utilisé par l'application ne le prend pas en charge.
- Avec réflexion, il faut séparer strictement le raisonnement de la réponse finale avant tout parsing JSON, traitement de scène ou affichage. Le mécanisme actuel d'extraction du premier objet JSON est particulièrement risqué.
- Le suffixe E4B ne promet ni une consommation mémoire divisée par deux ni un tour de jeu deux fois plus rapide. Le fichier GGUF Q4_K_M identifié est même légèrement plus gros que celui de Ministral.

La mise à jour de `llama.cpp` est **autorisée si nécessaire**, conformément à la précision apportée pendant cette étude. Elle fait partie des options de migration ; elle ne remplace pas l'adaptation de l'appelant.

## 2. Modèle étudié et comparaison utile

« Gemma E4B » est interprété ici comme **`google/gemma-4-E4B-it`**, qui propose un mode de réflexion configurable. Il existe aussi un **Gemma 3n E4B**, modèle distinct : cette étude ne lui attribue pas les propriétés de Gemma 4. Sources : [fiche Google Gemma 4 E4B](https://huggingface.co/google/gemma-4-E4B-it), [fiche Gemma 3n](https://ai.google.dev/gemma/docs/gemma-3n/model_card).

| Critère | Modèle actuel | Candidat |
|---|---|---|
| Référence | Ministral 3 8B Instruct 2512 | Gemma 4 E4B IT |
| Paramètres annoncés | 8,4 milliards pour le modèle de langage | 4,5 milliards effectifs ; environ 8 milliards avec embeddings |
| Réflexion | Checkpoint Instruct ; la famille propose séparément des checkpoints Reasoning | Mode activable sur le checkpoint IT |
| Contexte annoncé | 256k tokens | 128k tokens |
| Utilisation dans le projet | Texte vers JSON ou texte de scène | Même périmètre recommandé |
| Format local | GGUF Q4_K_M | GGUF Q4_K_M pour la première comparaison |
| Licence indiquée par les fiches | Apache 2.0 | Apache 2.0 |

Les embeddings par couche, ou PLE, expliquent l'écart entre paramètres effectifs et totaux de Gemma. Ils ne disparaissent pas du fichier de poids. La réduction du contexte maximal théorique n'est pas un obstacle pour une application dont la configuration initiale est à 4 096 tokens. Sources : [fiche Ministral 3 8B](https://huggingface.co/mistralai/Ministral-3-8B-Instruct-2512), [architecture Gemma 4](https://ai.google.dev/gemma/docs/core/model_card_4).

### Artefact GGUF concret pour le prototype

Un candidat reproductible a été identifié dans le dépôt de l'équipe `ggml-org`, distinct du dépôt original de Google :

| Métadonnée | Valeur |
|---|---|
| Dépôt | `ggml-org/gemma-4-E4B-it-GGUF` |
| Révision consultée | `51db8454ac67b58ed848e54d4111be2ad16ba4bf` |
| Fichier | `gemma-4-E4B-it-Q4_K_M.gguf` |
| Taille publiée | `5 335 289 824` octets, soit environ 5,34 Go / 4,97 Gio |
| SHA-256 publié | `90ce98129eb3e8cc57e62433d500c97c624b1e3af1fcc85dd3b55ad7e0313e9f` |

Ces métadonnées ont été vérifiées dans [l'API du dépôt à cette révision](https://huggingface.co/api/models/ggml-org/gemma-4-E4B-it-GGUF/revision/51db8454ac67b58ed848e54d4111be2ad16ba4bf?blobs=true) ; le fichier n'a pas été téléchargé et son contenu n'a donc pas été vérifié localement. La [liste des fichiers à cette révision](https://huggingface.co/ggml-org/gemma-4-E4B-it-GGUF/tree/51db8454ac67b58ed848e54d4111be2ad16ba4bf) sert de référence, plutôt que la branche mouvante `main`.

Le téléchargement Ministral actuel attend `5 198 911 904` octets dans [download_ministral.py](../scripts/download_ministral.py). Ce candidat Gemma est **environ 136 Mo, soit 2,6 %, plus volumineux**. Cela compare des fichiers, pas la VRAM effectivement occupée. Le même nom de quantification n'assure pas non plus une perte de qualité identique entre architectures.

### Ce que permettent de conclure les benchmarks publics

Google publie notamment 58,6 % à GPQA Diamond, 52,0 % à LiveCodeBench v6 et 42,5 % à AIME 2026 pour E4B dans sa [fiche de modèle](https://ai.google.dev/gemma/docs/core/model_card_4). Ce sont des indices de capacité, pas des mesures de cohérence fictionnelle, de français littéraire ou de conformité à notre contrat JSON.

La fiche Ministral sépare les tableaux Instruct, Reasoning et Base. Comparer les chiffres de sa variante Reasoning au checkpoint Instruct installé serait trompeur. Aucun gain relatif sur ce projet, ni gain propre à l'activation de la réflexion, ne peut être chiffré à partir de ces tableaux. Un quatrième candidat optionnel serait Ministral 3 8B Reasoning, pour distinguer l'effet du raisonnement de celui du changement de famille.

## 3. Intégration actuelle : constats dans le dépôt

| Élément | Observation et conséquence |
|---|---|
| Runtime | [llm_runtime.cpp](../src/llm_runtime.cpp) charge le GGUF via l'API C de `llama.cpp`. Aucun serveur HTTP n'est nécessaire. |
| Configuration | [llm_runtime.h](../src/llm_runtime.h) fixe notamment `n_ctx=4096`, `n_predict=768`, `n_batch=2048`, `n_gpu_layers=-1`, Flash Attention activée, grammaire JSON désactivée. |
| Dépendance | [VENDORED_COMMIT.txt](../vendor/llama.cpp/VENDORED_COMMIT.txt) indique `6b36c2305644fd30db7cce3f4840c74574f31ce9`. |
| Construction | [CMakeLists.txt](../CMakeLists.txt) lie `llama`, désactive `LLAMA_BUILD_COMMON` et les outils, et déclare le projet en C++11. |
| Format des messages | `ApplyChatTemplate()` appelle `llama_chat_apply_template()` ; aucun paramètre de réflexion n'est exposé. |
| Sortie | Une seule chaîne `response_text` contient tous les tokens décodés, y compris les marqueurs spéciaux. |
| Parsing | [turn_runner.cpp](../src/turn_runner.cpp) tente le JSON complet, puis `ExtractFirstJsonObject()`, puis une réparation LLM et éventuellement un résultat de secours. |
| Appels auxiliaires | Traduction des textes joueur limitée à 384 tokens ; mise à jour thermique limitée à 160 ; génération de salle et éventuel audit de scène sont d'autres chemins à adapter. |
| Interface | [sdl_frontend.cpp](../src/sdl_frontend.cpp), `OnHeadlessTurnStream()`, transmet les fragments bruts au terminal et actualise un statut SDL. |
| Durée mesurée | `inference_time_ms` commence après chargement du modèle et création du contexte. Ce n'est pas le temps complet d'attente du joueur. |

Le modèle et le contexte sont recréés à chaque `GenerateChatCompletion()`. Un tour peut donc payer plusieurs chargements : génération principale, réparation, localisation, traitement thermique ou scène candidate selon le chemin suivi. Une mesure limitée au débit de tokens manquerait une partie importante du coût.

Autre particularité : **la température d'échantillonnage dépend de la température corporelle du joueur**. `BuildBodyDrivenGenerationConfig()` remplace ou décale la température de base, avec un plafond à 1,50. Le zéro du constructeur ne signifie donc pas que tous les tours se déroulent en décodage glouton. Ce couplage artistique doit être recalibré et préservé explicitement lors de la migration.

La navigation canonique et les barrières sont déjà contrôlées par le moteur. Même un déplacement résolu sans génération narrative peut appeler le LLM pour la thermique. Il faut distinguer « déplacement déterministe » et « tour sans appel LLM ».

## 4. Bénéfice attendu de la réflexion pour Eryx

Les bénéfices ci-dessous sont des **hypothèses à tester**, issues de l'analyse des tâches du jeu.

| Tâche | Bénéfice plausible | Politique proposée |
|---|---|---|
| Commande ambiguë ou composée | Distinguer intention, objet visé et préconditions ; produire une clarification utile | Réflexion sur les cas complexes |
| Création d'une salle | Mieux concilier lieu source, orientation, archétype, objets utilisables et contraintes de composition | Premier terrain d'essai avec réflexion |
| Continuité narrative | Réduire les contradictions entre ressources, historique récent, mesures et description | Comparer sur sessions complètes |
| Proposition topologique future | Confronter plusieurs contraintes avant de proposer une mutation | Usage possible, toujours soumis au validateur moteur |
| Déplacement cardinal connu | Très peu de bénéfice : le moteur connaît déjà la destination | Conserver la résolution déterministe |
| Thermique, localisation, réparation JSON simple | Coût potentiellement supérieur au bénéfice pour des sorties courtes | Réflexion désactivée initialement |

Exemple de cas discriminant : « Je marque la paroi invisible, puis je rejoins le scanner par le chemin déjà parcouru. » Une bonne réponse doit distinguer la marque, l'intention de déplacement, les liens réellement accessibles et l'impossibilité éventuelle de faire toute la séquence en un tour.

Le raisonnement ne donne au modèle aucune information absente du prompt. Il ne remplace ni la persistance des lieux, ni le graphe de navigation, ni les validations du moteur. Il peut également chercher à rendre réciproque un retour volontairement incohérent, ou à matérialiser un mur qui doit rester invisible : dans cette œuvre, une meilleure logique générale ne garantit pas une meilleure fidélité artistique.

La vision et l'audio de Gemma ne donnent aucun bénéfice automatique à la chaîne textuelle actuelle. Examiner les images raytracées demanderait une intégration multimodale séparée, des poids complémentaires et un nouveau protocole. Ce n'est pas nécessaire au remplacement étudié.

## 5. Coût de la réflexion : latence, mémoire et rythme de jeu

Un modèle qui réfléchit produit des tokens supplémentaires avant sa réponse utile. Pour un appel, l'ordre de grandeur est :

```text
durée = chargement + préparation du contexte + traitement du prompt
      + tokens de réflexion / débit de décodage
      + tokens de réponse / débit de décodage
      + validation
```

**Exemple purement illustratif, sans mesure locale :** 512 tokens de réflexion à 40 tokens/s ajoutent 12,8 secondes ; 1 024 ajoutent 25,6 secondes. Un meilleur débit de Gemma peut être entièrement absorbé par ce supplément. À l'inverse, éviter une réparation ou un nouvel appel peut compenser une partie du coût.

La cible documentée est une RTX 4060 8 Go, dans [LLAMA_CUDA_SPECS.md](./LLAMA_CUDA_SPECS.md). Un GGUF d'environ 5,34 Go constitue un candidat plausible, **pas une preuve qu'une configuration donnée tient intégralement en VRAM**. Il faut ajouter cache KV, buffers de calcul et autres allocations ; mesurer aussi la RAM et la part réellement déportée sur GPU. L'organisation des embeddings et de l'attention peut changer ce bilan.

Le runtime augmente actuellement `n_ctx` jusqu'à au moins `prompt_tokens + n_predict + 32`, et `n_batch` jusqu'à la taille du prompt ; `n_ubatch` reçoit la même valeur. Augmenter seulement `n_predict` pour autoriser la réflexion peut donc aussi accroître les allocations. La migration doit définir un plafond mémoire/contexte et un micro-batch maîtrisé.

Pour le prototype, essayer **0, 256, 512 et 1 024 tokens de réflexion**, en conservant un budget distinct pour la réponse finale. Ces valeurs sont des points de mesure, pas des réglages validés. Le plafond actuel de 768 tokens au total peut être consommé avant le premier JSON complet. Les limites de 160 et 384 tokens rendent particulièrement inadaptée une activation globale sur tous les appels.

L'interface doit rester réactive et annulable pendant la réflexion ; un statut discret suffit. Le délai jusqu'au premier texte utile, la durée complète du tour et leurs valeurs p50/p95 importent davantage que l'apparition rapide d'un premier token technique.

## 6. Impact logiciel

### 6.1. Premier obstacle : le format de conversation

Le dépôt contient déjà [src/models/gemma4.cpp](../vendor/llama.cpp/src/models/gemma4.cpp), ainsi qu'un traitement Gemma 4 dans [common/chat.cpp](../vendor/llama.cpp/common/chat.cpp). Cependant, le chemin effectivement appelé passe par [src/llama-chat.cpp](../vendor/llama.cpp/src/llama-chat.cpp), dont le format intégré `gemma` correspond aux anciens marqueurs `<start_of_turn>` / `<end_of_turn>`.

Gemma 4 emploie notamment `<|turn>` et `<turn|>`. L'API simplifiée retourne `-1` quand le template n'est pas reconnu ; l'application retourne alors `Chat template application failed.` Le simple changement de `--model` n'est donc pas une voie compatible avec le template Gemma 4 standard de cette version. C'est un constat de code, pas un échec reproduit avec un GGUF chargé. Voir le [format officiel Gemma 4](https://ai.google.dev/gemma/docs/core/prompt-formatting-gemma4).

**Solution privilégiée :** raccorder le moteur Jinja et les helpers de conversation de `common`, réutiliser le template du modèle et sélectionner explicitement la réflexion. Cela évite d'entretenir un formatteur Gemma artisanal. Dans la copie actuelle, la cible `common` exige `cxx_std_17` : son ajout entraîne donc aussi une compilation de la cible consommatrice au moins en C++17, à rendre explicite et à valider sous Visual Studio/CUDA.

### 6.2. Place d'une mise à jour de llama.cpp

La mise à jour est possible. Le choix d'une nouvelle révision doit être lié à un besoin vérifié : compatibilité du GGUF choisi, correction Gemma/CUDA, gestion des budgets ou simplification du parsing. Aucun numéro de version « minimal garanti » n'a été établi dans cette étude.

Procédure proposée :

1. Construire un prototype isolé avec la copie actuelle pour vérifier le raccordement Jinja et le chargement du candidat.
2. Si nécessaire, sélectionner une révision précise de [l'amont llama.cpp](https://github.com/ggml-org/llama.cpp), relever ses changements d'API et comparer les modifications locales du répertoire vendor avant remplacement.
3. Mettre à jour les sources vendor et `VENDORED_COMMIT.txt` ensemble ; reconstruire dans un répertoire neuf pour éviter de réutiliser des objets ou caches incompatibles.
4. Tester Gemma **et Ministral** sur le même nouveau runtime. Conserver l'ancienne révision pour le retour arrière et la baseline.

Un serveur local `llama-server` est une alternative pour un banc d'essai, mais son intégration au jeu ajouterait démarrage de processus, protocole HTTP, gestion des erreurs et packaging. L'intégration native reste le choix recommandé ici. Un succès dans `llama-cli` ou `llama-server` ne valide pas à lui seul notre exécutable : ces outils disposent d'une couche `common` que l'application n'utilise pas encore.

### 6.3. Activer la réflexion et séparer les canaux

Google documente `enable_thinking=True` dans son template et le marqueur d'activation `<|think|>` dans le message système. Le raisonnement est encadré par `<|channel>thought` et `<channel|>`, avant la réponse finale. Ce protocole diffère des balises génériques `<think>...</think>`. Source : [mode réflexion Gemma](https://ai.google.dev/gemma/docs/capabilities/thinking).

Évolution proposée de nos structures, **non implémentée** :

- `LlmGenerationConfig` : profil de modèle, politique de réflexion, budget de réflexion, budget de réponse, délai maximal, `top_k` et `top_p` configurables.
- `LlmGenerationResult` : réponse finale distincte du flux brut, compteurs de tokens par canal, raison d'arrêt explicite, état de complétude et mesures de temps par phase.
- Callback de streaming : événements typés « réflexion », « réponse », « fin » et contrôle d'annulation indépendant du texte visible.

La séparation doit être incrémentale et robuste aux marqueurs coupés entre fragments, à une réflexion vide, à son absence et à une génération interrompue. Préférer le parseur Gemma déjà présent dans `common` après validation de son API. Le flux brut peut servir à un diagnostic explicite ; seul le canal final doit alimenter les parseurs métier, la narration et l'historique de jeu.

**Cas critique :** si le raisonnement contient un JSON d'essai puis la réponse finale un JSON corrigé, `ExtractFirstJsonObject()` peut sélectionner le brouillon. Comme `ParseTurnResultJson()` tolère de nombreux champs absents, ce brouillon peut même être accepté sans réparation. Supprimer simplement les balises serait insuffisant : il faut écarter le contenu du canal entier et valider le schéma final.

### 6.4. Grammaire JSON et arrêt de génération

La grammaire actuelle est appliquée dès le premier token lorsqu'elle est activée. Une grammaire n'acceptant que du JSON empêcherait la production du canal de réflexion. Deux stratégies sont possibles :

- génération libre, séparation des canaux, puis validation stricte de la réponse finale : prototype plus simple, sans garantie syntaxique pendant la génération ;
- grammaire tenant compte du protocole ou activation différée à l'entrée dans le canal final : cible plus robuste, à tester avec les helpers de la révision retenue.

Le JSON générique de `json.gbnf` ne garantit pas le schéma métier ni la validité d'une transition. Les validations autoritatives restent nécessaires.

Aujourd'hui, `GenerateChatCompletion()` retourne `success=true` même si la boucle s'arrête à `n_predict` sans fin de génération ; `reached_eog` existe mais ne conditionne pas le parsing du tour. Il faut distinguer fin normale, budget épuisé, annulation et erreur. Une sortie interrompue pendant la réflexion ne doit jamais devenir une action par extraction opportuniste d'un objet JSON.

Un budget de réflexion n'est pas seulement une valeur plus grande de `n_predict`. Sa mise en œuvre doit réellement compter ce canal et définir comment obtenir une réponse finale ou abandonner proprement. La présence de `--reasoning-budget` dans les outils vendor ne signifie pas que notre boucle manuelle l'applique. Prévoir une réserve de tokens pour la réponse et un repli sans réflexion en cas d'échec, borné pour éviter les cascades d'appels.

### 6.5. Prompts, échantillonnage et règles de jeu

Dans [turn_contract.cpp](../src/turn_contract.cpp) et les messages système de `turn_runner.cpp`, reformuler « uniquement JSON » comme une contrainte sur la **réponse finale** lorsque la réflexion est active. Conserver les clés et identifiants en anglais, les textes joueur dans la langue choisie et l'interdiction de transformer la topologie invisible en géométrie visible.

Google recommande `temperature=1.0`, `top_p=0.95`, `top_k=64` dans sa [fiche Gemma 4](https://ai.google.dev/gemma/docs/core/model_card_4). Notre runtime utilise `top_k=40` en échantillonnage non glouton. Il faut comparer un profil contrôlé et un profil conseillé par le fournisseur, puis recalibrer le couplage à la température corporelle. Copier aveuglément les réglages de Ministral ou imposer globalement ceux de Google fausserait l'évaluation.

La réflexion ne doit pas être assimilée à une température élevée : ce sont deux axes différents. Une montée de désorientation du joueur ne doit pas activer implicitement un budget de calcul illimité.

### 6.6. Fichiers et surfaces concernés

| Surface | Travail attendu | Importance |
|---|---|---|
| `src/llm_runtime.h/.cpp` | Jinja, profils, canaux, budgets, arrêt, chronométrage ; éventuellement modèle résident | Élevée |
| `CMakeLists.txt`, `vendor/llama.cpp` | Intégration `common`, C++17 ; mise à jour vendor si nécessaire | Moyenne à élevée |
| `src/turn_runner.h/.cpp` | Réflexion par tâche, parsing du canal final, validation, secours, métriques de tous les appels | Élevée |
| `src/turn_contract.cpp` | Contraintes sur la réponse finale et fidélité Eryx | Moyenne |
| `src/main.cpp`, `src/sdl_frontend.cpp` | Options explicites, statuts et streaming, annulation | Moyenne |
| Scripts de téléchargement et `.bat` | Manifeste par modèle, révision, taille et SHA-256 ; sélection du profil | Faible à moyenne |
| `scripts/build_installer.py`, `packaging/within_the_latent_walls.iss` | URL, dossier, nom, empreinte, raccourcis et manifeste de distribution | Moyenne |
| `packaging/MODEL_NOTICE.txt`, `INSTALLATION_README.txt`, documentation | Provenance Google et conversion GGUF, notices et procédures | Faible |
| Scripts de benchmark | Matrice comparative, mêmes états initiaux, mesures complètes | Moyenne |

Le path tracer, le format `.scene`, les prefabs et la sauvegarde des états validés n'ont pas besoin d'être réécrits pour ce changement. Ajouter la provenance du modèle aux résultats et aux nouvelles salles aiderait toutefois à analyser une session mêlant des générations de plusieurs modèles. Changer de modèle ne doit pas invalider automatiquement les salles déjà persistées.

Un modèle conservé en mémoire pourrait réduire les rechargements répétés, indépendamment du choix Gemma. Cette optimisation demande une gestion de durée de vie, de remise à zéro du contexte et d'annulation ; elle doit être mesurée séparément pour ne pas attribuer son bénéfice au modèle.

## 7. Protocole de décision

### Matrice minimale

| Variante | Objectif |
|---|---|
| A0 : Ministral, runtime actuel | Baseline de l'application |
| A1 : Ministral, runtime adapté ou mis à jour | Isoler l'effet de l'infrastructure |
| B : Gemma E4B, réflexion désactivée | Isoler l'effet du changement de modèle |
| C : même Gemma, réflexion sélective | Mesurer le gain net de la réflexion |

Employer les mêmes états initiaux et contrats sémantiques, avec le template propre à chaque modèle. Relever quantification, empreintes, runtime, GPU/pilote, contexte, paramètres effectifs et graine. Une graine identique ne rend pas les générations comparables token par token entre modèles ; pour les profils stochastiques, répéter avec plusieurs graines.

Préparer au moins **40 cas Eryx**, puis quelques sessions de 15 à 30 tours : déplacements canoniques, barrière invisible, retour non réciproque de l'abri, commandes ambiguës, ressources insuffisantes, thermique, langues, cinq branches génératives, revisites et sauvegarde/rechargement. Les baselines datacenter de [HYBRID_SCENE_GENERATION_BENCHMARK.md](./HYBRID_SCENE_GENERATION_BENCHMARK.md) restent historiques ; elles ne démontrent pas la qualité Eryx.

Mesurer :

- conformité JSON et schéma dès la première réponse, taux de réparation, de secours et de troncature ;
- contradictions narratives, actions inventées, fidélité aux objets visibles, aux ressources et à la topologie ;
- qualité du français, concision, action concrète proposée au joueur, diversité des nouvelles salles ;
- latence complète p50/p95, premier texte utile, nombre d'appels, tokens par canal, chargement et traitement du prompt ;
- VRAM/RAM maximales, offload effectif, stabilité CUDA, temps de compilation et rendu mesurés séparément.

Prévoir des tests ciblés de protocole : JSON dans la réflexion, marqueur fragmenté, réflexion vide, budget atteint avant la réponse, annulation, réponse finale invalide et absence de fuite dans l'historique. Rejouer `--navigation-self-test`, la compilation/rendu des scènes et la persistance avec la nouvelle construction. Ces validations sont proposées pour la migration ; elles n'ont pas été exécutées dans le cadre documentaire présent.

### Critères proposés avant bascule

La bascule demande zéro violation observée des invariants moteur et zéro interprétation d'un brouillon comme action sur le corpus, aucune régression de persistance, un taux de réponses finales valides au moins égal à la baseline, et un gain qualitatif identifiable sur les cas complexes.

Le budget d'attente acceptable doit être fixé pour l'expérience artistique. À défaut, comparer au minimum le p95 actuel et réserver les tours plus longs à la découverte d'une nouvelle salle. Si C améliore peu B, retenir Gemma sans réflexion ; si B ne surpasse pas A1 au compromis qualité/latence/mémoire, conserver Ministral. Une préférence pour Gemma ne doit pas être déduite du seul suffixe E4B.

## 8. Plan de migration et charge indicative

1. **Compatibilité** : sélectionner l'artefact, raccorder Jinja, tester les deux modèles sans réflexion et décider si une mise à jour vendor apporte une correction nécessaire.
2. **Protocole** : séparer les canaux, gérer les budgets et arrêts, adapter les validations, le streaming et les appels auxiliaires.
3. **Expérimentation** : mesurer B puis C, régler les profils et la politique par tâche, vérifier l'effet sur l'écriture et le rythme.
4. **Distribution** : seulement si les résultats le justifient, changer le profil par défaut, les téléchargements et l'installeur, puis tester une installation propre et le retour à Ministral.

Ordre de grandeur d'ingénierie, **non engagement** : environ 1 à 2 jours pour la compatibilité, 2 à 4 jours pour le protocole robuste et 2 à 4 jours pour l'évaluation et la distribution, soit **5 à 10 jours de travail**. Un problème CUDA, une évolution importante des API `common` ou une campagne artistique étendue peut dépasser cette plage. Un changement de fichier de poids serait rapide ; une migration exploitable inclut les garanties ci-dessus.

Le retour arrière repose sur le maintien du profil Ministral, de ses métadonnées de téléchargement et de la révision runtime de référence. Pas de suppression des poids actuels ni de conversion destructive des sauvegardes.

**Décision recommandée : lancer un prototype Gemma 4 E4B textuel, d'abord sans réflexion puis avec réflexion sur la génération de salles et les commandes complexes.** La mise à jour de `llama.cpp` est disponible comme moyen technique. Le bénéfice déterminant sera une meilleure cohérence jouable pour un coût d'attente acceptable, à établir par la comparaison locale.
