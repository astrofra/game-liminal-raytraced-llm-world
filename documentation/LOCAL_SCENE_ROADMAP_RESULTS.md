# Scènes locales — première tranche jouable

25 octobre 2026. Application de la [roadmap](roadmap-scenes-3d-llm-local.md), avec [DUO-015](duo/reviews/DUO-015.md) pour les briefs et le rapport ; intégration moteur, backend, audits, tests et revue par Codex.

**Bilan historique de la première tranche.** La décision suivante remplace le parcours de sept lieux et la reprise entre lancements : voir [un seul départ et archives de recherche](SINGLE_START_RESEARCH.md). Les commandes et résultats ci-dessous décrivent la version mesurée, pas le lancement actuel.

## Jouer

Lancer `play_within_the_latent_walls.bat`, choisir la langue, puis saisir **est**. Le jeu démarre au champ d'extraction et utilise **Gemma 4 E4B via Ollama local**. Aucun choix de mode n'est demandé. **Ouest** revient à la carrière ; **est** retrouve le lieu accepté. Le lanceur sauvegarde dans `output/local_scene_session.json`.

Un lancement de l'exécutable sans argument ouvre également SDL au champ d'extraction. Le lanceur reste l'entrée recommandée : il fixe le dossier de travail et la sauvegarde. Ollama doit être disponible sur `127.0.0.1:11434` avec `gemma4:e4b` installé (`ollama pull gemma4:e4b` sur une nouvelle machine).

La tranche directe porte sur **une seule sortie**, `extraction_field → east`. Les sept lieux canoniques, leurs liens et la barrière invisible restent autoritatifs. Les quatre autres branches conservent provisoirement leur compilation hybride, avec le même backend Ollama pour leurs appels LLM. Leur extension à la composition directe constitue le lot suivant.

Reprendre une sauvegarde reste une commande de développement :

```bat
play_within_the_latent_walls.bat --load-state output\local_scene_session.json
```

## Images et résultats conservés

- [Comparaison Gemma / Ministral : vingt essais](benchmarks/eryx-local/campaign-02/comparison.png), [tableau détaillé](benchmarks/eryx-local/campaign-02/REPORT.md), [mesures](benchmarks/eryx-local/campaign-02/measurements.json) et [identité des modèles](benchmarks/eryx-local/campaign-02/manifest.json).
- [Dix découvertes dans le runtime natif](benchmarks/eryx-local/runtime/contact-sheet.png) et [mesures de la recette](benchmarks/eryx-local/runtime/measurements.json).
- [Bloc masquant la vue : avant/après correction](benchmarks/eryx-local/repair/before-after.png), avec les [diagnostics avant](benchmarks/eryx-local/repair/before.audit.json) et [après](benchmarks/eryx-local/repair/after.audit.json).
- [Première session jouée](benchmarks/eryx-local/runtime/pilot.png) et [même lieu après rechargement](benchmarks/eryx-local/runtime/reloaded.png) : PNG identiques octet pour octet sur cet essai.

Les `.scene` sont conservés à côté des PNG. Les images marquées **rejetées / diagnostic** ont été rendues séparément pour examiner le défaut ; leur rendu supplémentaire ne fait pas partie du temps d'acceptation mesuré. Une case noire sans image signifie qu'aucun rendu diagnostic n'a été produit, pas une scène noire acceptée.

Les requêtes, sorties brutes, journaux et profils locaux restent dans `C:/works/duo-runs/game-liminal-raytraced-llm-world/`, dossiers `roadmap-local-benchmarks`, `roadmap-local-runtime` et `roadmap-local-models`. Ils ne sont pas intégrés au dépôt.

## Comparaison mesurée

Machine : Windows, Intel Core i7-10700, RTX 4060 **8 Go**. Ollama `0.40.10`. Cinq briefs, deux graines par brief, traitement séquentiel des modèles. Le modèle précédent est déchargé ; le suivant est préchargé et amorcé avant les essais chronométrés. Les historiques sont indépendants.

Paramètres communs : contexte 8192, sortie 2048 tokens, température 0,2, top-k 40, top-p 0,95, graines 42/43 ; réflexion désactivée pour Gemma, non disponible pour le Ministral Instruct importé. Caméra, sol, ciel et éclairage fixes ; 8–20 boîtes, rotations permises. Rendu 800 × 400, 8 spp, 3 rebonds, un échantillon direct, graine 77. Une correction complète maximum ; délai global de 120 secondes, rendu compris.

| Modèle | Premier essai | Après correction | Médiane des réussites | p95 des réussites | Médiane de tous les essais |
|---|---:|---:|---:|---:|---:|
| `gemma4:e4b` | 6/10 | **8/10** | **4,317 s** | **7,852 s** | 4,741 s |
| `eryx-ministral8b:q4` | 3/10 | 4/10 | 19,185 s | 33,933 s | 32,624 s |

Il s'agit d'acceptation **technique**, pas d'une note artistique. Sur un si petit échantillon, le p95 est essentiellement le maximum ; il n'est pas une estimation robuste de la performance future.

Le Ministral importé réutilise exactement les poids Q4 du jeu, SHA-256 `33e7a72cf5e6e2cfc2f2847075acc013d68bba023e35310cef86b5cf8fdca761`. Aucun nouveau poids Ministral n'a été téléchargé. Gemma est le paquet Ollama local `gemma4:e4b`, déclaré `7.5B / Q4_K_M` par l'API, avec `draft_num_predict=3` dans ses paramètres de paquet. Le gain observé compare ces deux configurations disponibles, avec leurs templates et leur décodage effectifs ; il n'isole pas une propriété générale des architectures.

Chargement après déchargement du modèle : Gemma **5,805 s**, Ministral 8B **3,199 s** dans cette campagne. Ce n'est pas un test de disque froid après redémarrage. Les premiers essais exploratoires ont aussi observé 31–34 s pour Gemma ; le cache système et les conditions de démarrage comptent. Le 14B préinstallé a atteint 120 s sans réponse complète dans le test exploratoire et n'a pas été utilisé comme substitut du 8B dans le tableau.

La première campagne stricte avait obtenu 7/10 pour Gemma et 1/10 pour Ministral. La seconde accepte, de manière identique pour les deux modèles, un unique bloc Markdown entourant **toute** la réponse, comme le chemin dynamique existant. Elle ne recherche jamais un programme au milieu de prose et ne transforme jamais une troncature en succès. Les diagnostics de repère caché y nomment le principal obstacle et comptent les rayons vers le repère.

## Recette de la branche native

Dix découvertes indépendantes de la sortie est, graines 42–51, modèle déjà résident, même rendu 800 × 400 / 8 spp : **9/10 réussites**, toutes au premier programme. Médiane des réussites **4,324 s**, p95 **4,835 s**. En incluant l'échec : médiane **4,388 s**, p95 **8,207 s**. Durées mesurées autour du processus complet, audit, aperçu, sauvegarde et rendu final compris.

La graine 49 reste un échec après correction : le repère n'occupe qu'une sonde de vue au lieu des trois demandées. Aucun fichier de nouvel état n'a été créé pour cet essai. Le seuil technique de 8/10 et les cibles de latence sont donc atteints pour cette branche et cette campagne, sans généralisation aux cinq branches.

La session `est → look → ouest → est`, puis une reprise `look → ouest → est`, conserve le texte et son SHA-256. Les deux rendus finaux sont identiques. Un seul appel de composition est observé dans la première session, aucun dans la reprise. Des appels thermiques restent possibles lors des déplacements ; ils ne recomposent pas le décor.

Les appels natifs successifs de cette session rapportent environ **4–5 ms de chargement**, contre plusieurs secondes lors d'un vrai chargement : les poids restent résidents. Le client utilise `keep_alive=10m`, des messages neufs par tâche, un délai global et des callbacks d'annulation sur le thread appelant. Il ne charge pas parallèlement les poids llama.cpp. Les modèles cloud sont refusés par cette voie du jeu.

La session SDL automatique de vingt secondes a aussi créé et sauvegardé une salle directe. La fermeture SDL pendant la génération conserve zéro lieu généré, zéro déplacement et le tour initial. Une annulation native pendant la phase initiale est observée en environ **47 ms**, sans réponse finale publiée. L'essai de délai global à 1 ms échoue proprement, sans nouvelle sauvegarde. Ces valeurs sont des observations locales ; elles ne garantissent pas une borne temps réel du système d'exploitation.

## Correction et validation

Le parseur strict vérifie syntaxe, nombres finis, tailles positives, arités, identifiants et budgets. L'audit utilise les triangles rendus, les rotations et les solides réels, plutôt que de remplir artificiellement les enveloppes des prefabs. Il contrôle la caméra, son support, le repère, le masquage proche et le dégagement du retour. Le contrat direct interdit les prefabs et limite les boîtes à vingt ; l'audit général conserve aussi les limites après expansion pour les chemins de diagnostic.

Le masquage proche est une règle du cadre de test : plus de 90 % des sondes rencontrent un obstacle à moins de 3 m. Un grand volume lointain n'est donc pas rejeté automatiquement. Le repère demandé doit couvrir au moins trois des 2048 sondes ; cette mesure peut être sensible aux surfaces coplanaires et aux objets très fins.

La fixture contrôlée de correction contient huit boîtes valides et un `cooling_block_4` ajouté devant la caméra. L'audit détecte 100 % de masquage et nomme le bloc. Après précision du diagnostic sur les dimensions complètes et le dégagement du solide entier, une correction Gemma est acceptée en **4,338 s**. Les deux tentatives précédentes sur un décor généré à repère plus ambigu avaient échoué ; elles restent dans les preuves. Un premier essai mal assemblé par le banc avait produit une erreur syntaxique et est exclu de la démonstration de masquage.

Le préfixe de correction a été précisé après la campagne comparative et la recette native : préserver les directives non concernées et déplacer l'obstacle de premier plan avant de toucher au repère. Les chiffres des tableaux correspondent aux campagnes archivées, pas à une répétition non effectuée de chaque cas avec ce dernier préfixe.

## Contrats, persistance et nettoyage

`assets/scene_generation/local_v1.json` fournit le même cadre à Python et au C++ embarqué ; pas de deuxième texte de prompt à entretenir. Le compilateur CMake embarque la ressource dans l'exécutable. La publication du lieu et des deux liens n'arrive qu'après les audits et l'aperçu. Aucun récit d'arrivée n'est publié avant acceptation.

Les salles directes utilisent `scene_source=local_direct_v1`, contrat de sauvegarde **2**, empreintes du programme et de la provenance, modèle, graine, nombre de corrections et empreinte du prompt. `RefreshGeneratedRoomCache()` conserve ce programme accepté, y compris après observation. Les anciens contrats 1 restent lisibles. Un ancien exécutable qui ne connaît que le contrat 1 refusera les nouvelles salles : conserver l'exécutable correspondant aux sauvegardes.

Le lanceur n'impose plus le chemin du GGUF Ministral ni l'ancien mode dynamique et son départ au plateau. Les duplications de prompts ont été retirées. Les voies native/hybride/deux-passes encore appelées par les outils de diagnostic et leurs tests sont conservées pour la roadmap ; elles ne sont plus proposées au lancement normal. Aucune dépendance vendored modifiée. Aucun commit ni push effectué par Codex.

L'installeur 2.2 et son téléchargement Ministral décrivent la distribution précédente. Aucun nouvel installeur Ollama/Gemma n'est livré par cette tranche ; le lancement validé ici utilise l'environnement local déjà installé.

## Vérifier et reproduire

```powershell
python scripts/build_native.py --with-cli --publish
ctest --test-dir build_ministral_v070 -C Release --output-on-failure
python -m unittest discover -s scripts -p "test_*scene*.py"
python scripts/run_local_scene_benchmark.py --models gemma4:e4b eryx-ministral8b:q4 --out C:/works/duo-runs/game-liminal-raytraced-llm-world/nouvelle-campagne
python scripts/archive_local_scene_benchmark.py C:/works/duo-runs/game-liminal-raytraced-llm-world/nouvelle-campagne documentation/benchmarks/eryx-local/nouvelle-campagne --render-rejected
```

Le générateur de planches utilise Pillow. Le comparatif exige que les deux modèles nommés existent déjà. L'import local du GGUF suit la [procédure officielle Ollama](https://docs.ollama.com/import), sans requantification. Durées, compteurs, maintien en mémoire et flux suivent l'[API chat Ollama](https://docs.ollama.com/api/chat).

Résultats observés : quatre CTest réussis, 72 tests Python de scène réussis, auto-test de navigation réussi. Les tests de protocole HTTP emploient un serveur isolé sur loopback : fin normale, réponses incomplètes, troncature, erreur HTTP, erreur de syntaxe, annulation, délai partagé et absence de réflexion privée dans les callbacks. Les tests moteur couvrent les contrats 1/2, la publication atomique, les empreintes, la revisite, le rechargement, les ouvertures, les rotations et le retour dégagé.

## Limites et suite

La composition reste inégale : passerelles mal cadrées, masses flottantes, ouvertures bouchées et repères minces. La diversité de silhouettes est visible dans la planche ; l'acceptation technique ne prouve pas que huit scènes sur dix satisfont artistiquement le brief. La navigation dans cette tranche est textuelle : le contrôle du retour ne constitue pas une navigation physique complète entre tous les objets.

La première tranche est exploitable et mesurée. Avant d'étendre aux quatre autres sorties, améliorer le brief de passerelle et le contrôle des relations/supports sans imposer une recette de placement ; poursuivre la revue humaine des images. Caméra libre, éclairage libre, objets interactifs supplémentaires et topologie générative restent des élargissements distincts.
