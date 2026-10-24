# Étude de mise à jour de Ministral et de llama.cpp

Date : **2026-10-24**. Projet : *Within the Latent Walls / Entre les Murs Latents*.

**Périmètre : matériel cible inchangé, RTX 4060 8 Go.** Aucun scénario d'achat ou de changement de machine n'est étudié, conformément à la demande. Cette étude complète celle sur [Gemma 4 E4B](./ETUDE_MIGRATION_MINISTRAL_GEMMA_E4B.md).

Statut : analyse documentaire et inspection du code, **sans mise à jour, téléchargement de poids ni benchmark d'inférence**. Référence du dépôt : `b896babfe2546df2bcc49519bf1e4f6ba0907125`, avec prise en compte des modifications présentes dans le répertoire de travail. Les propositions ci-dessous ne constituent pas des fonctionnalités déjà implémentées.

## 1. Réponse et recommandation

**Oui, des modèles Mistral plus récents existent en local. En revanche, aucun successeur compact officiel de Ministral 3 8B Instruct 2512 n'a été identifié dans les sources consultées au 2026-10-24.** Le modèle déjà choisi par le projet reste une référence actuelle de la gamme Ministral ; son suffixe `2512` ne signifie pas qu'un remplacement plus récent et adapté à 8 Go existe nécessairement.

Pour ce projet et ce matériel, les pistes concrètes sont :

1. **Conserver Ministral 3 8B Instruct Q4_K_M comme référence**, et évaluer séparément une mise à jour de `llama.cpp`.
2. **Essayer la quantification Q5_K_M** du même modèle si la marge mémoire le permet : changement peu invasif, bénéfice qualitatif probablement limité et à mesurer.
3. **Évaluer Ministral 3 8B Reasoning Q4_K_M** pour les tâches complexes : poids de taille comparable, mais adaptation du protocole et coût de génération plus importants.
4. Garder **Ministral 3 3B** comme piste d'allègement ou de traitement auxiliaire, sans le présenter comme une montée en qualité garantie.

La mise à jour de `llama.cpp` est autorisée si nécessaire. Une version candidate précise a été identifiée : **v0.7.0**, publiée le 2026-10-15. Sa validation doit précéder toute bascule. Le changement de runtime, celui du modèle et celui de la quantification doivent rester mesurables séparément.

## 2. État installé et sens du mot « mise à jour »

| Élément | Référence actuelle |
|---|---|
| Modèle sélectionné | `mistralai/Ministral-3-8B-Instruct-2512-GGUF` |
| Fichier | `Ministral-3-8B-Instruct-2512-Q4_K_M.gguf` |
| Révision fixée dans le packaging | `0102285ad796bd99af90f58de616092e5630e970` |
| Taille attendue | `5 198 911 904` octets, soit 5,20 Go / 4,84 Gio |
| SHA-256 attendu | `33e7a72cf5e6e2cfc2f2847075acc013d68bba023e35310cef86b5cf8fdca761` |
| Runtime vendorisé | `6b36c2305644fd30db7cce3f4840c74574f31ce9`, commit daté du 2026-07-30 |
| Exécution | API native C/C++ de `llama.cpp`, backend CUDA, sans serveur obligatoire |

Les références proviennent de [download_ministral.py](../scripts/download_ministral.py), [build_installer.py](../scripts/build_installer.py), [llm_runtime.h](../src/llm_runtime.h) et [VENDORED_COMMIT.txt](../vendor/llama.cpp/VENDORED_COMMIT.txt).

La révision, la taille et l'empreinte du Q4_K_M publiées dans [l'API du dépôt officiel](https://huggingface.co/api/models/mistralai/Ministral-3-8B-Instruct-2512-GGUF?blobs=true) correspondent encore à ces références. **Retélécharger le même fichier n'apporterait donc pas de nouveaux poids.** Cette comparaison porte sur les métadonnées du projet et du fournisseur ; aucun calcul de hash des poids présents sur disque n'a été relancé.

Il faut distinguer quatre opérations :

- **Nouvelle génération de modèle** : nouveau checkpoint, potentiellement nouvelles capacités. Aucun remplacement compact plus récent n'a été trouvé ici.
- **Autre variante du même modèle** : Instruct vers Reasoning, avec un comportement et un protocole différents.
- **Autre quantification ou conversion GGUF** : compromis précision, taille et performance ; ce n'est pas un nouvel entraînement.
- **Nouveau runtime** : corrections et optimisations d'inférence ; les connaissances apprises du modèle ne changent pas.

Une modification récente d'une fiche Hugging Face ne suffit pas à prouver une modification des poids. La comparaison des empreintes des artefacts est déterminante.

## 3. Existe-t-il un Mistral local plus récent ?

**Oui, notamment Mistral Small 4 et Mistral Medium 3.5. Ils sont écartés du scénario retenu en raison de leur volume.** Le catalogue Mistral distingue ces modèles de la gamme Ministral, toujours référencée en version 25.12. Sources : [catalogue officiel actuel](https://docs.mistral.ai/models), [catalogue des checkpoints Ministral du fournisseur](https://huggingface.co/api/models?author=mistralai&search=Ministral&limit=100).

| Modèle récent | Disponibilité et caractéristique vérifiées | Décision pour cette étude |
|---|---|---|
| Mistral Small 4, mars 2026 | Poids disponibles ; 119 milliards de paramètres au total, 6 milliards actifs par token hors embeddings/sortie ; réflexion configurable | Exclu de la cible 8 Go |
| Mistral Medium 3.5, avril 2026 | Poids disponibles ; modèle dense de 128 milliards de paramètres, instruction et réflexion réunies | Exclu de la cible 8 Go |
| Mistral Large 4, annonce du 2026-10-06 | Préversion API ; annonce de publication des poids fin octobre ; mille milliards de paramètres annoncés | Exclu ; aucun artefact local validé dans cette étude |

Sources primaires : [annonce Small 4](https://mistral.ai/news/mistral-small-4/), [fiche Medium 3.5](https://huggingface.co/mistralai/Mistral-Medium-3.5-128B), [annonce Large 4](https://mistral.ai/news/mistral-large-4/).

Pour comprendre l'exclusion de Small 4 : 119 milliards de paramètres à exactement 4 bits représenteraient déjà **59,5 Go de données de poids**, avant surcoûts de quantification, contexte et buffers. Il s'agit d'un calcul théorique, pas de la taille mesurée d'un GGUF. Les 6 milliards de paramètres actifs réduisent le calcul par token, mais n'éliminent pas le stockage des autres experts. Une configuration reposant massivement sur un autre budget mémoire et sur des transferts CPU/GPU sort du périmètre demandé.

Les appellations **Mistral**, **Ministral** et **Magistral** ne désignent donc pas des tailles interchangeables. Magistral Small 1.2 est par exemple un modèle de 24 milliards de paramètres ; son GGUF officiel Q4_K_M publié fait environ 14,33 Go. Ce n'est pas une mise à jour compacte de Ministral. Sources : [fiche Magistral](https://huggingface.co/mistralai/Magistral-Small-2509), [métadonnées de son GGUF](https://huggingface.co/api/models/mistralai/Magistral-Small-2509-GGUF?blobs=true).

Le projet n'a pas à attendre un hypothétique « Ministral 4 » : aucun checkpoint officiel correspondant n'a été identifié. Cette conclusion reste datée ; elle ne préjuge pas des prochaines publications.

## 4. Options réalistes à matériel constant

Les tailles suivantes sont celles des fichiers publiés, **pas la VRAM totale requise**. Les sources sont les API officielles des dépôts [8B Instruct](https://huggingface.co/api/models/mistralai/Ministral-3-8B-Instruct-2512-GGUF?blobs=true), [8B Reasoning](https://huggingface.co/api/models/mistralai/Ministral-3-8B-Reasoning-2512-GGUF?blobs=true), [3B Instruct](https://huggingface.co/api/models/mistralai/Ministral-3-3B-Instruct-2512-GGUF?blobs=true), [3B Reasoning](https://huggingface.co/api/models/mistralai/Ministral-3-3B-Reasoning-2512-GGUF?blobs=true) et [14B Instruct](https://huggingface.co/api/models/mistralai/Ministral-3-14B-Instruct-2512-GGUF?blobs=true).

| Option | Fichier publié, Go décimaux | Apport recherché | Limite / priorité |
|---|---:|---|---|
| 8B Instruct Q4_K_M actuel | 5,199 | Référence connue du jeu | À conserver pour comparer |
| 8B Instruct Q5_K_M | 6,059 | Réduire l'erreur de quantification | +860 Mo, soit +16,5 % ; mesurer la marge VRAM et la qualité |
| 8B Reasoning Q4_K_M | 5,199 | Commandes complexes, cohérence des propositions | Réflexion plus longue, nouveau traitement des réponses |
| 3B Instruct Q4_K_M | 2,147 | Réduire l'empreinte et potentiellement la latence | Qualité sur les contraintes Eryx à vérifier |
| 3B Reasoning Q4_K_M | 2,147 | Réflexion avec davantage de marge mémoire | Ni supériorité au 8B Instruct ni vitesse de tour garanties |
| 14B Instruct Q4_K_M | 8,240 | Capacité supérieure potentielle | Environ 7,67 Gio de fichier, presque toute la VRAM avant allocations ; non retenu en priorité |
| 8B Instruct Q8_0 | 9,029 | Quantification moins destructive | Fichier seul supérieur à 8 Gio ; non retenu |

### 4.1. Q5_K_M : la modification la plus simple

Le modèle, sa famille de tokenizer et son format de conversation restent les mêmes. Le travail concerne surtout le profil, le téléchargement et le packaging. Le gain attendu est une meilleure fidélité au modèle d'origine, pas une nouvelle aptitude au raisonnement. Il peut être imperceptible sur des réponses courtes ou dominé par la qualité du prompt.

Sur 8 Go, ce candidat mérite un essai mesuré avec le contexte réellement utilisé et les allocations CUDA. Un léger gain de qualité qui provoque un offload CPU coûteux ou davantage d'échecs mémoire ne justifierait pas le changement.

### 4.2. Reasoning : la principale évolution fonctionnelle dans la même famille

Ministral 3 8B Reasoning est un **checkpoint distinct**, disponible en GGUF officiel. Sa taille Q4_K_M est presque identique à celle d'Instruct, à 1 536 octets près. Il reste plausible sur la même enveloppe de poids, mais son calcul et son cache de contexte augmentent avec les tokens de réflexion.

Il pourrait mieux traiter une commande combinant objet, précondition et itinéraire, ou concilier archétype, sortie, objets utilisables et continuité lors de la création d'une salle. Ce sont des hypothèses pour Eryx, pas des résultats obtenus. Les benchmarks de mathématiques et de code de la famille ne mesurent pas directement le français narratif ni le respect de notre topologie.

**Il ne faut pas promettre un bouton réflexion universel sur le modèle Instruct actuel.** Le template officiel Reasoning inspecté ne comporte ni `enable_thinking` ni `reasoning_effort`. Cette observation ne démontre pas qu'il soit impossible d'influencer son comportement, mais elle ne permet pas de supposer le même mode hybride que Small 4 ou Gemma 4. Source : [template Reasoning officiel](https://huggingface.co/mistralai/Ministral-3-8B-Reasoning-2512/blob/main/chat_template.jinja).

Pour une politique par tâche, le choix le plus explicite serait de router vers Instruct ou Reasoning. **Deux modèles 8B Q4 représentent ensemble environ 10,40 Go de fichiers** : leur résidence simultanée complète sur 8 Go n'est pas une cible réaliste. Les échanges de modèle et les transferts doivent entrer dans la mesure de latence. Un seul Reasoning pour tous les appels simplifie la sélection mais risque de pénaliser les tâches courtes.

### 4.3. 3B : optimisation éventuelle, pas mise à niveau automatique

Un 3B peut être utile pour les sorties thermiques courtes ou la localisation, voire comme profil rapide. Toutefois, deux modèles, deux configurations et une politique de sélection ajoutent des coûts de chargement et de maintenance. Commencer par un essai du 3B seul sur le même corpus permet de savoir si cette complexité serait justifiée.

La recommandation initiale reste un modèle par profil d'essai. Le routage multi-modèle vient ensuite, uniquement si les mesures le motivent.

## 5. Faut-il mettre à jour llama.cpp ?

### Écart de version établi

| Référence | Valeur vérifiée |
|---|---|
| Commit déclaré dans le projet | `6b36c2305644fd30db7cce3f4840c74574f31ce9` |
| Date de ce commit amont | 2026-07-30 |
| Dernière release renvoyée par l'API GitHub lors de l'étude | `v0.7.0`, publiée le 2026-10-15 |
| Commit visé par ce tag annoté | `620d2225cc436ab0b001e3200e590e3b286ef9ab` |
| Comparaison GitHub | 1 476 commits d'avance, aucun de retard par rapport à la référence vendor |

Sources : [commit de référence](https://github.com/ggml-org/llama.cpp/commit/6b36c2305644fd30db7cce3f4840c74574f31ce9), [release v0.7.0](https://github.com/ggml-org/llama.cpp/releases/tag/v0.7.0), [comparaison des révisions](https://github.com/ggml-org/llama.cpp/compare/6b36c2305644fd30db7cce3f4840c74574f31ce9...v0.7.0). Le tag a été résolu jusqu'au commit, et pas seulement jusqu'à l'objet Git de type `tag`.

Ce décompte mesure l'écart entre deux révisions amont. Il ne garantit pas que le répertoire vendor local soit identique octet pour octet à la révision déclarée : les éventuelles adaptations locales doivent être inventoriées avant remplacement.

### Apports pertinents et limites

Les notes v0.7.0 signalent notamment des corrections CUDA concernant Flash Attention, des lectures hors limites MMQ et des différences d'arrondi sous MSVC. Elles mentionnent aussi une réorganisation de la couche conversation autour de `common_chat_session`, ainsi qu'un cache GPU pour experts MoE. Ces changements justifient une évaluation de correction et de compatibilité ; ils ne prouvent pas que le jeu actuel souffre de chacun de ces défauts. Source : [notes de version](https://github.com/ggml-org/llama.cpp/releases/tag/v0.7.0).

Le gain annoncé pour des experts MoE déportés ne s'applique pas automatiquement au Ministral dense utilisé ici. Les évolutions du serveur ou de son interface web ne sont pas non plus des améliorations directes de notre boucle native.

Une vérification statique de [llama.h à v0.7.0](https://github.com/ggml-org/llama.cpp/blob/v0.7.0/include/llama.h) retrouve les symboles principaux employés par le projet : chargement du modèle, création du contexte, template simplifié, tokenisation/décodage, grammaire et réglage des threads. Les champs `offload_kqv`, `flash_attn_type` et `n_ubatch` sont présents. **La présence des noms ne constitue pas une validation de signatures, de compilation ou de comportement.**

### Décision par scénario

| Scénario | Mise à jour requise ? |
|---|---|
| Conserver le Q4_K_M actuel | Non pour une fonctionnalité nouvelle ; candidate pour corrections et maintenance |
| Passer au Q5_K_M officiel | Aucun nouveau support d'architecture a priori nécessaire ; valider sur la version actuelle d'abord |
| Passer à Reasoning | La copie actuelle contient déjà des helpers Ministral Reasoning ; adapter l'intégration est indispensable, changer la version n'est pas nécessairement le prérequis |
| Mutualiser l'adaptation avec Gemma 4 | Une base récente peut être utile ; sélectionner une révision commune et valider les deux modèles |

**Recommandation : qualifier v0.7.0 dans une construction isolée, sans en faire une condition artificielle au premier essai Q5.** L'adopter si les validations passent et si son intérêt de maintenance ou de correction est établi. Aucun gain de vitesse local n'est chiffré dans cette étude.

## 6. Impact logiciel de la variante Reasoning

### Template, prompt système et canaux

[llm_runtime.cpp](../src/llm_runtime.cpp) utilise `llama_chat_apply_template()`, une voie simplifiée. Dans la copie vendor, les formats Mistral intégrés savent produire les enveloppes système/utilisateur usuelles ; cela ne signifie pas qu'ils exécutent le Jinja complet du checkpoint, ses instructions par défaut ou son historique de réflexion.

[common/chat.cpp](../vendor/llama.cpp/common/chat.cpp) contient déjà `common_chat_params_init_ministral_3()`, un traitement des blocs `reasoning_content` et les marqueurs **`[THINK]` / `[/THINK]`**. Le projet ne construit actuellement pas cette couche : [CMakeLists.txt](../CMakeLists.txt) force `LLAMA_BUILD_COMMON=OFF` et ne lie que `llama`.

La voie recommandée est d'utiliser les helpers de conversation et le Jinja de la révision retenue, avec un parseur de sortie correspondant. La cible `common` demande C++17 dans la copie actuelle et dans v0.7.0 ; son intégration ferait évoluer l'exigence de la cible applicative actuellement déclarée C++11. Une mise à jour de la dépendance seule ne réalise pas ce raccordement.

Mistral recommande un prompt système spécifique, une température de 0,7 et la conservation des traces de raisonnement dans les conversations multi-tours. Le prompt fourni demande aussi une réponse Markdown, incompatible avec notre contrat final JSON : il faut adapter cette partie tout en préservant le protocole Reasoning. Sources : [fiche Reasoning](https://huggingface.co/mistralai/Ministral-3-8B-Reasoning-2512), [prompt système fourni](https://huggingface.co/mistralai/Ministral-3-8B-Reasoning-2512/blob/main/SYSTEM_PROMPT.txt).

Cette recommandation sur l'historique diffère de celle de Gemma 4. Dans le jeu actuel, les appels reconstruisent le contexte depuis l'état et l'historique métier ; ils ne rejouent pas une conversation complète avec traces. Pour le prototype, conserver cette architecture d'appels autonomes et déclarer ce choix dans les mesures. Si un historique technique Reasoning est ensuite ajouté, le stocker séparément et le borner ; **ne jamais injecter les brouillons dans la narration, la mémoire canonique ou les actions validées**.

### Parsing et génération structurée

Les adaptations principales sont les mêmes catégories que pour Gemma, avec un protocole propre à Ministral :

- distinguer flux brut, réflexion et réponse finale ; tenir compte des balises éventuellement présentes dans le préfixe de génération et des fragments incomplets ;
- appliquer `ParseTurnResultJson()` et les parseurs de salle/scène uniquement à la réponse finale ;
- remplacer la récupération aveugle du premier objet JSON : un brouillon dans `[THINK]` peut contenir un objet syntaxiquement valide mais abandonné ;
- distinguer fin normale, budget atteint, annulation et erreur ; aujourd'hui, atteindre `n_predict` peut encore donner `success=true` ;
- rendre la grammaire compatible avec une phase de réflexion libre, puis contraindre ou valider la réponse finale ; une grammaire JSON active dès le premier token bloquerait les balises de réflexion.

La copie actuelle de `common` prévoit, dans son chemin Ministral avec schéma, un JSON encadré de clôtures Markdown. Il faudra vérifier le contenu réellement extrait par ce helper et le contrat de l'application, plutôt que supposer que « JSON only » produit partout les mêmes octets. Ce point est tiré du code local et peut évoluer avec une nouvelle révision.

### Budget, température et latence

Les valeurs actuelles sont de 768 tokens par génération principale, 384 au maximum pour la localisation et 160 pour la thermique. Elles deviennent facilement insuffisantes si la réflexion occupe une grande partie de la sortie. Définir des budgets séparés, une réserve pour la réponse finale et un arrêt explicite ; tester les cas où le raisonnement n'atteint jamais sa fermeture.

À titre d'illustration uniquement, 512 tokens supplémentaires à 40 tokens/s ajoutent 12,8 secondes. Le débit n'a pas été mesuré ici. La réflexion peut cependant économiser une réparation : seul le temps complet jusqu'à un tour validé permet de trancher.

Le code adapte l'échantillonnage à la température corporelle du joueur dans `BuildBodyDrivenGenerationConfig()`. Il faut recalibrer ce mécanisme pour Reasoning, sans confondre température d'échantillonnage et budget de réflexion. Une commande surchauffée ne doit pas déclencher une génération non bornée.

Enfin, le modèle et le contexte sont recréés à chaque appel. Conserver le modèle en mémoire peut améliorer les temps indépendamment d'une mise à jour ; cette optimisation doit rester un facteur distinct du benchmark. Le chronomètre d'inférence actuel exclut le chargement : ajouter une mesure de durée complète et celle de tous les appels auxiliaires.

## 7. Téléchargement, packaging et retour arrière

Deux candidats suffisent pour les premiers essais, en plus de la référence actuelle :

| Champ | Instruct Q5_K_M | Reasoning Q4_K_M |
|---|---|---|
| Dépôt sous `mistralai/` | `Ministral-3-8B-Instruct-2512-GGUF` | `Ministral-3-8B-Reasoning-2512-GGUF` |
| Révision | `0102285ad796bd99af90f58de616092e5630e970` | `4413bc9b7b091ddad28101650d1395f5e045a328` |
| Fichier | `Ministral-3-8B-Instruct-2512-Q5_K_M.gguf` | `Ministral-3-8B-Reasoning-2512-Q4_K_M.gguf` |
| Taille, octets | `6 059 268 512` | `5 198 910 368` |
| SHA-256 publié | `7a5454127ec772e2389f0e71a77fedb88b83d4366d8a69facd0cfd0898f04d35` | `894aa3645ef8708a81dbe201c26105ce37c4c741252c89c5a78f81b49ac438c6` |

Sources : [métadonnées Instruct à révision fixe](https://huggingface.co/api/models/mistralai/Ministral-3-8B-Instruct-2512-GGUF/revision/0102285ad796bd99af90f58de616092e5630e970?blobs=true), [métadonnées Reasoning à révision fixe](https://huggingface.co/api/models/mistralai/Ministral-3-8B-Reasoning-2512-GGUF/revision/4413bc9b7b091ddad28101650d1395f5e045a328?blobs=true). Empreintes publiées consultées ; aucun fichier de poids vérifié localement pendant l'étude.

Le script Python de téléchargement actuel ne passe pas `revision=` à `hf_hub_download()`, alors que le packaging fixe sa révision. Son contrôle de hash détecte une substitution, mais il faut aligner ces deux chemins pour des téléchargements reproductibles.

Un manifeste partagé par profil réduirait les duplications entre le runtime, les scripts, les `.bat`, `build_installer.py` et `packaging/within_the_latent_walls.iss`. Il devrait contenir dépôt, révision, fichier, taille, empreinte et réglages associés. Aucun projecteur multimodal n'est nécessaire pour le périmètre textuel.

Conserver les anciens poids, l'exécutable de référence et les configurations de construction. La mise à jour doit préserver les sauvegardes et les salles déjà validées ; aucun changement du format `.scene` ou du path tracer n'est requis. Ajouter la provenance du modèle aux nouveaux résultats facilite les diagnostics, sans régénérer arbitrairement les lieux existants.

## 8. Validation et plan d'exécution

### Matrice pour isoler les gains

| Essai | Modèle | Runtime / objectif |
|---|---|---|
| A | 8B Instruct Q4_K_M | Copie actuelle, baseline |
| B | Même fichier exact | v0.7.0, effet de la mise à jour seule |
| C | 8B Instruct Q5_K_M | Même runtime que B, effet de la quantification |
| D | 8B Reasoning Q4_K_M | Runtime adapté aux canaux, effet de Reasoning |
| E, optionnel | 3B Instruct ou Reasoning Q4_K_M | Allègement, si A à D ne satisfont pas le rythme du jeu |

Lors de D, rejouer également le 8B Instruct avec la couche de templates adaptée : autrement, une amélioration due à Jinja serait attribuée à tort au checkpoint Reasoning.

Réutiliser le corpus Eryx proposé dans [l'étude Gemma](./ETUDE_MIGRATION_MINISTRAL_GEMMA_E4B.md) pour permettre une comparaison commune : au moins 40 cas et quelques sessions de 15 à 30 tours, avec commandes ambiguës, ressources, barrière invisible, retour non réciproque, cinq branches génératives, revisites et rechargement. Les benchmarks datacenter historiques ne remplacent pas cette validation.

Mesurer les réponses finales valides du premier coup, réparations, replis, contradictions, qualité du français, latences p50/p95, tokens par canal, durée complète incluant chargements, pic VRAM/RAM et offload effectif. Les paramètres réellement utilisés, notamment la température après modulation corporelle, doivent être consignés. Répéter les profils stochastiques sur plusieurs graines.

### Vérifications de construction et de régression

1. Inventorier les écarts locaux du vendor, puis préparer une copie ou un checkout isolé de v0.7.0 avec son commit exact ; ne pas écraser aveuglément le répertoire courant.
2. Construire en Release dans un nouveau répertoire, avec les options CUDA/SDL/OpenMP du projet. Reconstruire les bibliothèques et l'exécutable ensemble ; un binaire `llama-cli` récent ne met pas à jour l'application liée statiquement.
3. Vérifier `--llama-info`, les auto-tests existants, la génération réelle, les scènes canoniques et les sauvegardes. Confirmer l'offload GPU plutôt que déduire son fonctionnement de la seule compilation CUDA.
4. Tester interruption, dépassement de budget, grammaire, JSON dans la réflexion et absence de fuite du raisonnement dans les résultats métier.
5. Mettre à jour ensemble les sources vendor, `VENDORED_COMMIT.txt`, les profils et le packaging seulement après validation. Vérifier une installation propre et le retour au profil actuel.

### Critères de décision

Adopter une variante si elle apporte une amélioration identifiable sans régression de validité, de persistance ou des invariants de navigation, et avec une latence compatible avec l'expérience actuelle. Pour Q5, exiger une marge mémoire mesurée suffisante sur le corpus. Pour Reasoning, comparer son gain sur les cas complexes au supplément de temps et aux éventuels rechargements.

Si les gains sont négligeables, **rester sur Ministral 3 8B Instruct Q4_K_M est une conclusion valable**. La récence d'une release ou d'un modèle ne constitue pas à elle seule un bénéfice pour le jeu.

## 9. Charge indicative et ordre recommandé

Estimations d'ingénierie, sans engagement ni temps de campagne exhaustive :

| Lot | Ordre de grandeur |
|---|---|
| Qualification v0.7.0 avec le modèle actuel | 1 à 3 jours selon compatibilité et régressions |
| Essai Q5_K_M et profil reproductible | 0,5 à 1 jour, hors campagne narrative étendue |
| Intégration robuste Reasoning, canaux, budgets et validation Eryx | 3 à 6 jours supplémentaires |

Une partie du travail sur les canaux, budgets et mesures serait mutualisable avec le prototype Gemma. Les formateurs, marqueurs et politiques d'historique doivent toutefois rester propres à chaque modèle.

**Ordre recommandé : référence actuelle → qualification de llama.cpp → essai Q5 → essai Reasoning → décision commune avec Gemma.** À matériel inchangé, le choix se joue entre la qualité, le respect du contrat et le temps d'attente des variantes compactes. Les Mistral 2026 beaucoup plus volumineux sont exclus de ce plan.
