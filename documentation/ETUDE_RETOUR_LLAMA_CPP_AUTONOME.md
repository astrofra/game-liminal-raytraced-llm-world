# Retour à un jeu autonome : llama.cpp + Gemma

25 octobre 2026 — étude Codex, avec audit de distribution DeepSeek revu par Codex ([DUO-024](duo/reviews/DUO-024.md)). **Pas de modification du runtime dans cette étude.**

## Décision cible

Le jeu doit fonctionner avec **llama.cpp intégré dans son propre processus et un GGUF Gemma livré avec lui**. Aucun Ollama, serveur local, compte, Python ou connexion réseau ne doit être nécessaire au joueur. Le modèle demeure Gemma : revenir à llama.cpp ne demande pas de revenir à l'ancien modèle.

L'autonomie implique deux choses : exécution hors ligne et **distribution complète hors ligne**, poids compris. Un petit installeur qui télécharge 5 Go au premier lancement ne suffit pas à cette cible. Les outils de fabrication peuvent accéder au réseau pour préparer les dépendances ; le paquet livré et son installation ne doivent pas en dépendre.

Le passage du jeu à Ollama était une dérive par rapport à cet objectif. Une activation du drapeau `native` seule ne la corrige pas : elle désactive aujourd'hui le parcours de scènes actuel et conserve des hypothèses spécifiques à Ministral.

## Ce qui est déjà disponible

| Élément observé | Conséquence |
|---|---|
| `vendor/llama.cpp`, commit `620d2225cc436ab0b001e3200e590e3b286ef9ab`, déjà lié au jeu | Pas besoin d'installer un moteur d'inférence chez le joueur. |
| Architecture `gemma4` dans `src/models/gemma4.cpp` du vendor et parseur Gemma 4 dans `common/chat.cpp` | Commencer par cette version épinglée ; une mise à jour générale du vendor n'est pas un prérequis démontré. |
| `models/gemma-4-e4b/gemma-4-E4B-it-Q4_K_M.gguf` déjà présent | Aucun nouveau téléchargement nécessaire pour les premiers essais sur cette machine. |
| `scripts/download_gemma.py` vérifie signature GGUF, taille et SHA-256 | Réutiliser ce contrôle dans le manifeste unique et le packaging. |
| JSON Schema déjà converti en grammaire dans `src/llm_runtime.cpp` | Conserver les contraintes des propositions narratives, thermiques et topologiques. |
| `src/dynamic_scene.cpp` appelle l'interface commune `GenerateChatCompletion` | La génération de géométrie n'est pas intrinsèquement liée à Ollama. |
| SDL, moteur, visibilité, langues, inventaire, archives distincts du transport LLM | Préserver ces acquis ; aucune raison de reconstruire le jeu ancien. |

Le support public de Gemma 4 dans llama.cpp et la disponibilité d'un GGUF instruct sont aussi documentés par [ggml-org](https://huggingface.co/ggml-org/gemma-4-E4B-it-GGUF). L'[exemple C++ officiel](https://github.com/ggml-org/llama.cpp/blob/master/examples/simple/simple.cpp) illustre l'appel direct au moteur, sans serveur. Les preuves locales ci-dessous portent toutefois sur notre version épinglée, pas sur la branche amont courante.

### Artefact Gemma vérifié sur disque

- Format : Q4_K_M, usage texte ; aucun projecteur image/audio requis pour les appels actuels.
- Taille : **5 335 289 824 octets**, soit 5,34 Go décimaux / environ 4,97 Gio.
- SHA-256 recalculé : `90ce98129eb3e8cc57e62433d500c97c624b1e3af1fcc85dd3b55ad7e0313e9f`.
- Révision déclarée par le script : `51db8454ac67b58ed848e54d4111be2ad16ba4bf`, dépôt `ggml-org/gemma-4-E4B-it-GGUF`.

L'identité du fichier local correspond au manifeste du script. L'ancienne révision n'a pas été récupérée avec l'outil web pendant cette étude : ne pas confondre vérification locale du SHA et nouvelle qualification du téléchargement distant. Les fiches actuelles [Google](https://huggingface.co/google/gemma-4-E4B-it) et [ggml-org](https://huggingface.co/ggml-org/gemma-4-E4B-it-GGUF) affichent Apache 2.0 ; conserver les notices exactes de la révision livrée.

## Obstacles précis dans notre code

### 1. Le mode natif change actuellement le jeu

Dans `src/main.cpp`, `use_ollama=true` est imposé au démarrage. Le passage natif sans mode explicite désactive `local_scenes` ; l'association native + local est ensuite refusée. Il faut retirer ces branchements et **garder le parcours local actuel comme seul démarrage normal**, avec sa première pièce, ses nouvelles découvertes et ses règles de retour.

### 2. Le runtime natif recharge le modèle à chaque appel

`GenerateChatCompletion` crée un `llama_model`, puis un contexte et un sampler locaux ; leurs destructeurs libèrent tout à la fin de chaque génération. Une découverte demande au moins la composition puis la narration : deux chargements successifs sont donc possibles. Le succès d'un petit essai CLI ne prouve pas que ce comportement préservera la latence d'une partie.

**Architecture recommandée :** un runtime possédant les poids pendant la session, avec un seul utilisateur d'inférence à la fois. Le premier lot conserve le modèle et crée un contexte neuf par requête pour simplifier l'isolation. Mesurer ce coût ; si nécessaire, réutiliser ensuite un contexte explicitement nettoyé (`llama_get_memory` / `llama_memory_clear`, API présente dans le vendor). Réinitialiser sampler, seed, parser et buffers à chaque appel. Ne jamais réutiliser implicitement un historique ou un cache d'une autre partie.

Le changement de température corporelle règle le sampler du prochain appel ; il ne doit pas recharger les poids. Annulation, échec de décodage et fermeture SDL doivent libérer les ressources après arrêt du worker, sans callback pointant vers une requête détruite. Le chargement froid doit être mesuré séparément des délais narratifs à chaud, actuellement plafonnés à 30 s.

### 3. La sortie est encore interprétée comme du Ministral

`src/llm_channels.h` et le chemin natif de `src/llm_runtime.cpp` utilisent `InspectMinistralChannels`, les marqueurs `[THINK]` et une substitution automatique vers un profil Ministral pour certains appels courts. Ces hypothèses doivent disparaître, ainsi que les noms et tests spécifiques à cet ancien modèle.

Pour Gemma, utiliser son template Jinja et le parseur de canaux correspondant dans llama.cpp, avec réflexion désactivée pour le parcours initial. Ne publier que le contenu final ; tester aussi le flux partiel, les tokens spéciaux, les sorties interrompues et le JSON contraint. Un simple remplacement de nom de fonction ne constitue pas une migration de protocole.

### 4. La distribution actuelle interdit d'embarquer les poids

`scripts/build_installer.py` refuse un `.gguf` dans le stage, écrit `model_embedded=false` et rejette les installeurs dépassant 1 Go. `packaging/within_the_latent_walls.iss` télécharge les poids pendant l'installation. Les profils et notices pointent vers Ministral.

Préparer d'abord un **dossier portable complet** : exécutable, ressources, DLL nécessaires, GGUF vérifié et notices. Puis adapter l'installeur à une source locale incluse ; envisager des volumes d'installation ou une archive adaptée à cette taille, à qualifier avant livraison. Supprimer le téléchargement obligatoire et les anciens contrôles de taille conçus pour un paquet sans modèle. Ne pas exiger CUDA Toolkit chez le joueur ; qualifier les DLL distribuées et le chemin CPU sur une machine propre.

Le jeu doit résoudre modèle, polices et ressources depuis le dossier de l'exécutable, indépendamment du dossier courant. Les captures et archives vont dans AppData. Un modèle absent ou corrompu produit un message localisé explicite, sans tentative de recours réseau.

## Suppression d'Ollama et de Ministral

| Périmètre | Travail prévu |
|---|---|
| `src/ollama_runtime.cpp`, `.h`, `tests/ollama_runtime_test.cpp` | Supprimer après raccordement natif et reprise des tests utiles d'annulation/délai/flux. |
| `src/llm_runtime.*`, `main.cpp` | Retirer `use_ollama`, `ollama_model`, `ollama_port`, le choix de backend et les paramètres CLI correspondants. Retirer les profils et substitutions Ministral. |
| `src/llm_channels.h`, `tests/llm_channels_test.cpp` | Remplacer les tests spécifiques par des tests des canaux Gemma et de non-publication de contenu incomplet. |
| `CMakeLists.txt`, `scripts/build_native.py` | Retirer les cibles Ollama et le lien HTTP ajouté pour le client du jeu ; renommer les cibles, dossiers et métadonnées portant l'ancien modèle. Préserver les dépendances réellement requises par llama.cpp. |
| `scripts/ministral_models.json`, `model_profiles.py`, `generate_model_header.py` | Un manifeste générique `models.json` avec un seul profil Gemma actif, source commune du runtime, des outils et du packaging. |
| `download_ministral.bat`, `ask_ministral.bat`, `scripts/download_ministral.py` | Supprimer ; garder un téléchargement Gemma réservé à la préparation du paquet. |
| Scripts de benchmark et tests associés | Retirer le client Ollama et les matrices Ministral ; conserver les briefs, graines et contrôles utiles dans un banc natif. Supprimer les scripts devenus sans appelant. |
| `dynamic_scene.cpp`, `research_archive.cpp`, vérificateurs de métriques | Écrire une provenance native : commit du moteur, SHA du modèle, contexte, sampler, temps de chargement et génération. Lire les anciennes archives comme données de recherche uniquement. |
| `packaging/*`, README, guides, notices, commandes d'aide | Réécrire pour le paquet Gemma autonome ; aucun conseil au joueur demandant Ollama ou Ministral. |

Faire une recherche finale des noms et chemins, pas seulement des imports. Les anciens poids et sorties de build Ministral présents localement ne doivent pas entrer dans le paquet. Leur ménage pourra suivre la validation, sur chemins explicitement vérifiés.

Les comptes rendus historiques ne doivent pas rester des instructions actives : les retirer de la documentation courante ou les exporter avec les preuves de recherche hors du checkout avant suppression, sans effacer les archives de parties. Git conserve les étapes du développement. La dépendance amont llama.cpp peut contenir du support générique d'autres modèles : il ne s'agit pas d'un profil activé dans notre jeu ; mutiler son code pour faire disparaître une chaîne de caractères n'est pas recommandé. Les occurrences restantes dans le vendor et les archives doivent être distinguées des références du produit, pas dissimulées par un renommage.

Le kit de développement Codex/DeepSeek est extérieur au jeu. Son usage d'Ollama n'est pas une dépendance de la distribution ; cette étude ne désinstalle ni ne reconfigure les outils ou modèles personnels de la machine.

## Plan d'implémentation

| Lot | Livraison et critère de sortie |
|---|---|
| A — Profil et protocole Gemma | Manifeste unique, chemins résolus depuis l'exécutable, template/parseur Gemma, texte libre et JSON contraint réels ; aucune substitution vers Ministral. |
| B — Runtime résident | Un chargement par session, appels sérialisés indépendants, mesures froid/chaud, annulation pendant chargement et décodage, aucun thread résiduel après fermeture. |
| C — Raccord au jeu actuel | Démarrage natif avec scènes locales ; découvertes, retours, refus, six langues, inventaire, chaleur et archives identiques du point de vue fonctionnel. |
| D — Retrait de l'ancien code | Ollama et profils Ministral supprimés des sources et outils du produit ; tests remplacés avant suppression des anciens ; pas de mode de compatibilité proposé au joueur. |
| E — Paquet autonome | Dossier portable puis installeur avec modèle et dépendances ; installation et première partie sur Windows propre, hors réseau, sans Ollama/Python/outils de compilation. |
| F — Qualification et nettoyage | Campagne de plusieurs parties, stabilité mémoire, performances, archives, notices, documentation et ménage des sorties obsolètes. |

Les contrats d'API, la résidence mémoire, les canaux et l'intégration restent à Codex. Les lots documentaires, inventaires de références et tests bornés peuvent être confiés à DeepSeek sous contrats distincts. Pas de retour à une ancienne version de la fiction ou des scènes pour raccourcir le portage.

## Recette indispensable

1. Petits appels réels de texte puis de JSON : langue correcte, EOS reconnu, aucun marqueur privé affiché. Cas arrêt volontaire, budget épuisé, JSON incomplet et modèle manquant.
2. Vingt appels successifs : un seul chargement des poids, aucune accumulation d'historique, mémoire stabilisée, températures de sampler vérifiables et isolation entre nouvelles parties.
3. Dix découvertes sur graines/briefs fixés, puis une partie d'au moins vingt tours avec branches cardinales, retours et refus. Même audit de caméra, même passe visibilité, aucune archive réinjectée.
4. Série de parties dans les six langues, incluant commandes d'une autre langue ; inventaire sans inférence et chaleur appliquée une seule fois.
5. Mesures GPU sur la RTX 4060 8 Go, GPU réellement disponible ; fallback CPU qualifié séparément. Comparer latences médiane et p95 au livrable actuel à prompts, budgets et graines documentés. La taille du GGUF ne suffit pas à prédire la VRAM du contexte.
6. Paquet copié sur Windows sans environnement de développement : lancement direct et depuis un autre dossier, utilisateur standard, chemin avec espaces/accents, réseau coupé, absence d'Ollama, présence des images et descriptions archivées.

La version Ollama utilisait des paramètres propres à son paquet, notamment `draft_num_predict=3` dans la campagne historique. On ne peut donc pas promettre la même vitesse en changeant simplement le transport, ni attribuer toute différence au modèle. Garder d'abord le GGUF déjà vérifié, mesurer, puis optimiser le runtime ; ne pas changer simultanément modèle, prompts et scène.

## Preuves de cette étude

Le fichier GGUF a été haché et sa taille vérifiée. Le `llama-cli.exe` construit avec notre commit embarqué a produit une phrase italienne avec Gemma, en CPU (`-ngl 0`), contexte 2048, réflexion désactivée, plafond 32 tokens : sortie 0, environ **5,73 s pour le processus complet**. Ce n'est ni une mesure de partie complète ni un benchmark GPU. Ollama était présent sur la machine mais n'est pas appelé par cette sonde native ; son service n'a pas été arrêté. Le GPU affichait 6 840 Mio occupés sur 8 188 Mio au relevé préalable, d'où l'isolement CPU.

Un second essai utilise directement **le runtime du jeu**, avec le même Gemma et zéro couche GPU : une composition locale à caméra fixe, un seul appel autorisé, contexte 8192, budget 2048 tokens et délai de 90 s. Résultat : **sortie 0, scène acceptée au premier appel, aucune réparation**, aperçu et audit de visibilité 256 × 128 réussis. Le modèle a produit 542 tokens, avec 656 tokens de prompt ; chargement mesuré à 1,46 s, inférence à 74,56 s et processus complet à 76,95 s. Ces mesures CPU ne préjugent pas de la latence GPU.

La sonde passe `--llm-backend native --scene-generation-mode hybrid --scene-method local --generate-dynamic-scene <brief>`. Le premier mode évite le verrou de démarrage actuel ; le second sélectionne effectivement le compositeur fixe, confirmé par `method=local-fixed-v1` dans les métriques. Cela valide le chemin de composition diagnostique, **pas encore une partie SDL native complète**. Le CLI utilisé dans le premier essai n'est pas un processus auxiliaire prévu pour le jeu livré : la cible reste l'inférence intégrée.

Les métriques exposent aussi du travail résiduel : le champ `profile` indique encore `ministral-8b-q4` alors que `model_path` désigne bien Gemma ; les temps séparés prompt/génération ne sont pas renseignés dans la trace native. Corriger ces métadonnées fait partie de la migration, sans réécrire les preuves brutes.

Preuves : `C:/works/duo-runs/game-liminal-raytraced-llm-world/native-return-study/` (`model.json`, `probe.json`, `gemma-native-cpu.log`, `scene-probe.json`, `native-scene.log`, `native-scene.metrics.json`, `native-scene.scene` et `native-scene.png`). Aucun nouveau build de produit ni nouvel installeur n'est livré par cette étude.

L'[audit du packaging](duo/AUDIT_DISTRIBUTION_AUTONOME.md) complète cette étude. La migration est faisable sur les composants déjà présents ; ses points déterminants sont la résidence du modèle, l'adaptation des canaux Gemma et une distribution contenant effectivement les poids.
