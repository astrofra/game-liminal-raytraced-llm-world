# Qualification Ministral — 24 octobre 2026

**Roadmap Ministral terminée : runtime v0.7.0 intégré, Q4 conservé par défaut, distribution 2.2.0 validée localement.** La décision repose sur les campagnes natives, les scènes directes et leur revue qualitative ; aucun gain général de vitesse ou de narration n'est revendiqué.

## Périmètre et preuves

Application Windows CUDA/SDL/OpenMP, RTX 4060 8 Go (8 188 MiB), Intel i7-10700, matériel constant. Les poids officiels des trois profils sont contrôlés en taille et SHA-256. Le [manifeste](../scripts/ministral_models.json) fixe leurs révisions. Les sorties Gemma préexistantes sont conservées.

Preuves privées de construction et de campagne : `C:/works/duo-runtime/liminal-ministral-20261024/`. Les sources amont sont `6b36c2305644fd30db7cce3f4840c74574f31ce9` et `620d2225cc436ab0b001e3200e590e3b286ef9ab` (v0.7.0). L'audit du vendor initial n'a trouvé aucun écart avec la première révision.

Les références natives A et B ont été reconstruites avec MSVC 19.44.35229 / CUDA 13.0.88 et des sources applicatives strictement identiques, vérifiées par empreintes. A utilise l'ancien runtime, B v0.7 sans adaptation. BJ, C et D utilisent la même application adaptée Jinja, avec Q4, Q5 et Reasoning respectivement. Les exécutables sont figés avant la campagne native ; aucune compilation simultanée ne doit contaminer ses mesures.

La première passe diagnostique `eryx-A-baseline` est exclue : compilation concurrente, ancien passage des arguments accentués en CP1252, corpus depuis corrigé. La campagne finale passe les commandes par fichiers UTF-8. Le binaire historique reste conservé séparément de la référence reconstruite.

## Benchmark direct de scènes

[Tableau détaillé, paramètres, erreurs et images](MINISTRAL_SCENE_COMPARISON.md).

| Variante | Syntaxe et caméra non dégénérée | Génération médiane |
|---|---:|---:|
| Ancien CLI, Q4 | 10/10 | 21,95 s |
| CLI v0.7, Q4 | 10/10 | 22,19 s |
| CLI v0.7, Q5 | 8/10 | 25,97 s |
| CLI v0.7, checkpoint Reasoning | 8/10 | 19,97 s |
| Même checkpoint, instruction explicite des canaux | 8/10 | 20,27 s |

Les dix programmes `.scene` et PNG Q4 sont identiques octet pour octet entre les deux CLI. La comparaison CLI conserve l'ancien exécutable MSVC 19.41 (`version 28, cd60aba`), dont la révision annoncée ne permet pas d'affirmer une identité avec le vendor déclaré. La comparaison native A/B contrôle, elle, le toolset et les sources applicatives.

Les temps comprennent lancement, chargement et génération CLI, sans hachage préalable ni rendu. Ils viennent des mesures par cas : le temps total du planificateur comprend des pauses et ne doit pas être utilisé comme latence. Dix cas à une graine ne suffisent pas à classer universellement les modèles.

Q5 invente des directives dans `badge_vestibule` et `control_hub`. Le checkpoint Reasoning invente également des directives ; sa seconde passe ajoute une erreur de paramètres de `plane`. **Aucun bloc de réflexion n'est observé sur ces vingt réponses**, même avec l'instruction explicite. Il s'agit de mesures du checkpoint Reasoning en réponse directe, pas d'une accélération prouvée par raisonnement. Le parseur Ministral amont rend ce bloc optionnel ; l'intégration native respecte ce comportement.

### Lecture visuelle

Le contrôle automatique vérifie la syntaxe et une base de caméra non dégénérée. Il ne détecte pas une vue masquée par un objet ni ne juge la composition.

Les dix images Q4 ont été examinées. `cooling_exchange_bay` est presque entièrement obstruée ; `sally_port_checkpoint` et `control_hub` cadrent des masses très proches, `badge_vestibule` une colonne centrale, et `server_aisles_dense` une grille au premier plan. `entry_gate_dusk`, `backup_vault`, `loading_dock_dust`, `roof_watch_dusk` et `stairhead_parapet` offrent une structure plus lisible, avec des limites de cadrage ou de répétition. La stabilité entre CLI ne résout donc pas ces défauts existants.

Les huit images Q5 valides ont également été examinées. `entry_gate_dusk` est plus dégagée et `server_aisles_dense` montre mieux le volume intérieur ; `cooling_exchange_bay` reste obstruée, `loading_dock_dust` cadre un obstacle très proche et `stairhead_parapet` une grille dominante. Ces améliorations locales ne compensent pas automatiquement les deux échecs syntaxiques.

Les huit images valides de la seconde passe Reasoning (DP) ont été examinées : `sally_port_checkpoint` et `cooling_exchange_bay` sont plus dégagées, mais les compositions répètent souvent porte, scanner et caisse. `roof_watch_dusk` présente notamment des objets apparemment flottants sur un fond presque vide. Aucun de ces essais ne démontre une amélioration visuelle générale.

Les briefs historiques datacenter sont volontairement conservés alors que le vocabulaire du prompt est Eryx ; cette tension limite leur emploi comme score artistique de la carrière vénusienne.

### Répétitions directes Reasoning

Avec l'instruction explicite des canaux et les graines 42/43/44, les résultats sont respectivement 8/10, 10/10 et 10/10, soit 28/30 scènes syntaxiquement valides avec caméra non dégénérée. Les médianes sont 20,27/18,47/20,31 s. **Zéro caractère de réflexion extrait sur ces trente réponses.** Les [résultats et images des trois graines](MINISTRAL_SCENE_SEEDS.md) sont conservés séparément des premiers essais.

Une lecture complémentaire de `cooling_exchange_bay` et `roof_watch_dusk` aux graines 43 et 44 montre des vues plus dégagées, mais des compositions toujours répétitives, des volumes parfois apparemment flottants et des espaces peu caractérisés. Cette vérification porte sur quatre images supplémentaires, pas sur une notation artistique exhaustive des deux reprises.

## Construction et protocole

- C++17, helpers Jinja/parseur officiel, séparation stricte entre réflexion ouverte et réponse finale.
- Budgets distincts, contexte borné, EOS requis, annulation coopérative et délai incluant chargement.
- [11 contrôles natifs réels réussis](generated/ministral_qualification/validation/native-protocol.json) dans `protocol-v2`, dont grammaire Q4/Q5, réponse Reasoning, annulation avant/pendant chargement et pendant réflexion, flux final, budget, délai et contexte.
- Tests de canaux C++ et 57 tests Python réussis ; trois auto-tests historiques réussis.
- [Sept scènes canoniques](generated/ministral_qualification/validation/canonical-renders.json) : PNG identiques entre référence reconstruite et application adaptée.
- [Arguments, sauvegardes et chemins PNG accentués vérifiés](generated/ministral_qualification/validation/unicode-paths.json).
- Double acceptation d'un token par l'ancien code de grammaire corrigée ; le sampler accepte déjà le token.

Les journaux de chaque appel adapté donnent chargement, inférence, durée totale, température réelle, tokens par canal et offload. Le runner mesure tous les appels et échantillonne RAM/VRAM. Les latences complètes incluent les rechargements ; aucun modèle conservé en mémoire entre appels n'est introduit dans cette comparaison.

## Eryx, graines et distribution

Les [cinq campagnes natives sont comparées ici](MINISTRAL_NATIVE_COMPARISON.md), avec leurs [mesures et provenances JSON](generated/ministral_qualification/native-comparison.json). Chacune prévoit 40 cas et des sessions de 15/30/28 tours. Les reprises sur les graines 43/44 couvrent les cinq branches, une commande contradictoire, la surchauffe et une demande de ressources illimitées ; la graine 42 est couverte par la matrice principale. Les assertions narratives nécessitent une lecture distincte des assertions mécaniques.

La [référence A complète](generated/ministral_qualification/eryx-A-summary.json) termine 38/40 cas isolés et 73/73 tours de session sans erreur mécanique, sans crash. p50/p95 du tour complet : 4,83/41,27 s ; pic VRAM échantillonné 7 086 MiB. Les cinq créations de salle ont une médiane de 16,12 s et un maximum de 21,44 s. Les écarts concernent `barrier-003` (compteur de déplacement incrémenté lors d'une formulation libre de contact) et `nonreciprocal-002` (formulation libre du retour ouest, destination attendue non atteinte). Les commandes cardinales simples passent. Les cinq branches sont créées, dont `generation-002` par repli de métadonnées ; ce repli ne doit pas être assimilé à une génération modèle réussie du premier coup.

Lecture narrative de la référence : les réponses utilisent globalement les six langues demandées, avec des emprunts anglais et des formulations parfois maladroites. Les questions thermiques distinguent bien extérieur et corps dans les cas examinés. Les demandes de ressources illimitées sont refusées. Des contradictions subsistent : `ambiguity-004` décrit un retour malgré une position inchangée ; `ambiguity-011` invente une téléportation dans la narration ; `thermal-004` mélange tutoiement/vouvoiement et comporte une faute d'accord. Quatre branches sur cinq réemploient le motif du sol qui s'effondre. Ces observations appartiennent à la référence, avant migration.

B et BJ sont également terminés : chacun obtient 38/40 cas et 73/73 tours de session sur les assertions automatisées, sans échec natif. B atteint 5,74/42,02 s en p50/p95, avec un pic de 7 207 MiB. BJ atteint 5,04/53,30 s, avec 6 408 MiB et 35/35 couches GPU. La baisse mémoire de BJ accompagne aussi le changement de contexte et de batch ; elle ne peut pas être attribuée au seul runtime. Sa hausse du p95 vient notamment des réparations supplémentaires dans la session mixte. Les cinq créations de salle ont une médiane de 16,25 s pour B et 17,16 s pour BJ. Les métadonnées proviennent du modèle dans 5/5 cas pour B, 3/5 pour BJ ; les deux autres utilisent le repli. Dans `generation-004`, BJ produit notamment un tableau JSON non fermé malgré une fin EOS normale.

**Ces scores couvrent la navigation, les liens, barrières, scènes sauvegardées et marqueurs privés ; ils ne certifient pas toutes les règles métier.** La revue des sauvegardes révèle des transitions de ressources insuffisamment justifiées : `resources-004` remonte l'énergie de `critical` à `strained` dans A, la conserve dans B et la remonte à `stable` dans BJ, alors que le texte BJ décrit encore une charge critique. Les demandes de ressources illimitées conservent les états critiques dans ces trois passes. Dans `thermal-005`, A et B abaissent aussi l'extérieur de 464 à 460 °C ; BJ conserve l'extérieur mais refroidit davantage le corps. Ces défauts et variations sont consignés dans les [problèmes connus](KNOWN_ISSUES.md). Le cas `thermal-004` démarre déjà à 40 °C : il exerce la surchauffe et ne prouve pas à lui seul le refus d'une autre consigne arbitraire.

La lecture des quarante cas BJ et des commandes libres de sa session mixte retrouve les contradictions de retour et de raccourci, ainsi que des fautes comme « ton combinaison » et des noms d'objets anglais. Le suivi des lumières est raconté sans déplacement effectif. Le refus des ressources illimitées est explicite ; une réponse illisible à la téléportation en session entraîne un repli conservant l'état. Aucun gain narratif général n'est établi.

### Q5 : meilleure complétude des salles, deux tours interrompus

C termine 37/40 cas, les sessions de 15 et 30 tours, puis échoue au tour 17/28 de la session mixte : 61/62 tours exécutés réussissent, 11 tours prévus restent non exécutés. `ambiguity-008` et `session-003…-17` atteignent 768 tokens finaux sans EOS ; le runtime arrête proprement le tour avant sauvegarde. Ce sont des échecs de budget, pas des crashs ou un manque de VRAM. Les 35 couches sont sur GPU, pour un pic échantillonné de 7 149 MiB. Le p95 de 47,37 s porte sur une campagne partielle et ne doit pas être comparé sans cette réserve aux campagnes complètes.

Les cinq branches de la graine 42 utilisent les métadonnées du modèle, avec une médiane de 18,10 s. Les reprises donnent 4/5 sur la graine 43 et 5/5 sur la graine 44, soit 14/15, contre 12/15 pour BJ sur ces mêmes trois graines. Les seize cas répétés Q5 passent les assertions suivies. Cela ne rejoue pas les deux commandes ayant échoué dans la campagne principale.

La lecture retrouve les déplacements racontés sans transition effective et des calques. `resources-004` conserve correctement l'énergie critique, mais `resources-001` invente une gourde et une gorgée d'eau malgré l'inventaire vide. Les narrations de salles restent proches entre elles (failles, plateformes inclinées, balises). Aucun gain qualitatif général n'est établi.

### Reasoning : intentions mieux résolues, régressions métier

D termine 40/40 cas et 73/73 tours sur les assertions suivies, avec p50/p95 de 4,91/38,00 s et un pic de 6 394 MiB. Il identifie correctement les intentions de `barrier-003` et `nonreciprocal-002` à la graine 42, sans réparation et sans réflexion observée sur ces deux réponses. Les reprises ciblées passent toutes deux à la graine 43 ; à la graine 44, la barrière passe mais le retour ouest échoue de nouveau, avec une intention générique `move` et une narration de déplacement malgré la position inchangée. La barrière passe donc 3/3 graines, le retour 2/3 : ce gain n'est pas un correctif fiable de la résolution d'intention.

La revue des sauvegardes empêche toutefois de conclure à une amélioration globale : `resources-008` remonte arbitrairement oxygène et énergie de `critical` à `stable`, tandis que `resources-004` remonte aussi l'énergie. `resources-001` invente de l'eau disponible. `resources-009` raconte l'activation d'un approvisionnement, alors que les états critiques restent inchangés. `thermal-004` appelle à tort « environnement » la valeur corporelle de 40 °C, malgré les 464 °C extérieurs. La réponse italienne mélange du français (« Giri autour ») ; d'autres langues contiennent des emprunts et des mots sans rapport. La session mixte conserve aussi des refroidissements extérieurs et téléportations imaginaires dans le récit.

Quatre créations de salle sur cinq utilisent le secours après dépassement des 512 tokens de réflexion ; la quatrième réussit directement, avec zéro token de réflexion. Sur l'ensemble de D, 88 appels utilisent le profil Reasoning, 20 produisent des tokens de réflexion et un seul de ces vingt termine correctement : des métadonnées de salle en session, avec 450 tokens de réflexion et 380 finaux. Les dix-neuf autres dépassent le budget de réflexion.

Les [48 reprises natives ciblées](MINISTRAL_SEED_COMPARISON.md) passent les assertions automatisées, mais confirment la variabilité. Reasoning produit les métadonnées de 3/5 salles à la graine 43 et 4/5 à la graine 44, soit 8/15 sur les trois graines. À la graine 44, `resources-009` remonte arbitrairement l'oxygène critique à stable et `generation-002` mélange de l'anglais à la narration française. Les pics VRAM des reprises atteignent 6 411 MiB pour Q4, 7 269 pour Q5 et 6 512 pour Reasoning ; toutes les couches des modèles adaptés restent sur GPU.

### Calibration du budget Reasoning

Le [rejeu des cinq branches à 2 048 tokens de réflexion](generated/ministral_qualification/reasoning-budget2048.json), même exécutable et même graine 42, obtient 4/5 métadonnées modèle au lieu de 1/5 à 512 tokens. Sa médiane de tour complet atteint 32,45 s et son maximum 43,32 s, contre 22,34/22,40 s pour la passe à 512 qui utilisait quatre replis. Les cas 002, 003 et 005 terminent avec respectivement 516, 1 195 et 543 tokens de réflexion. Le cas 004 reste une réponse directe identique, sans réflexion.

Le cas 001 termine après 1 167 tokens de réflexion avec un canal final vide ; il est rejeté et utilise le secours. Augmenter le budget améliore donc la complétude de certaines salles, mais ne suffit pas à garantir un contrat final valide. Cette calibration ne rejoue ni les quarante cas ni les sessions longues et ne qualifie pas un nouveau profil par défaut.

Les [états avant/après des 611 tours exécutés](generated/ministral_qualification/state-deltas.json) sont exportés pour permettre une revue des ressources et températures indépendante des scores automatiques. Les [reprises ciblées de navigation](generated/ministral_qualification/navigation-seeds.json) sont également conservées.

## Décision

La mise à jour technique vers `llama.cpp v0.7.0` est retenue avec l'intégration Jinja, les canaux séparés, les limites explicites, l'annulation coopérative, les chemins UTF-8 et la grammaire corrigée. Les 3 796 fichiers du nouveau vendor ont été vérifiés ; 173 fichiers obsolètes ont été retirés après contrôle de leur identité avec la référence. Les anciens exécutables et répertoires de build sont conservés.

**Ministral 3 8B Instruct Q4_K_M reste le profil par défaut**, y compris pour l'installeur et `run_scene_generation_benchmark.bat`. Les poids restent exactement ceux de la référence. Q5 et Reasoning sont disponibles comme profils explicites d'expérimentation ; aucun gain global de qualité ne justifie leur adoption par défaut. La hausse du p95 des commandes libres BJ est conservée dans le bilan, sans promesse d'accélération. La génération directe Q4 reste stable octet pour octet dans la comparaison CLI.

La piste 3B, optionnelle dans l'étude pour un autre compromis taille/latence, n'est pas activée dans cette décision. Aucun résultat 3B n'est revendiqué. Les corrections métier sur les ressources, la résolution des formulations libres et les cadrages restent des travaux distincts, documentés dans les problèmes connus.

Le format de sauvegarde et les calculs du path tracer restent inchangés. La branche 3B et les corrections métier constituent des suites possibles, hors de cette migration qualifiée.

## Livraison et recette finale

La [compilation publiée](generated/ministral_qualification/validation/published-build.json) depuis le vendor du dépôt réussit avec CUDA, SDL et OpenMP. Les empreintes du manifeste et des deux exécutables sont contrôlées ; les 24 fichiers applicatifs correspondent au binaire figé pour les mesures. Les trois auto-tests et CTest passent sur l'exécutable final, ainsi que les 57 tests Python sur les scripts. Le [rejeu final du lanceur `.bat`](generated/ministral_qualification/validation/published-bat-smoke.json), sur `entry_gate_dusk` en Q4, produit exactement la scène et le PNG de la campagne qualifiée. Ce contrôle s'ajoute aux 70 scènes comparatives.

L'[installation 2.2.0](generated/ministral_qualification/validation/installation.json) a été exécutée dans un dossier neuf sur cette machine Windows 11 : téléchargement réel des 5 198 911 904 octets, SHA-256 vérifié, démarrage SDL et tour LLM avec un `PATH` limité à Windows. Le tour charge une sauvegarde issue du runtime adapté, écrit un nom accentué et conserve les scènes générées ; la référence reconstruite sur l'ancien runtime relit également cette sauvegarde et conserve ses scènes. La désinstallation retire le programme et le modèle téléchargé sans modifier les fichiers AppData préexistants (aucun fichier présent dans ce test).

La première tentative de désinstallation a échoué parce que le script de recette ouvrait le même journal que l'option Inno `/LOG`. La reprise avec deux fichiers distincts réussit ; l'échec initial reste dans la preuve. Aucun correctif du produit n'a été nécessaire.

L'installeur, l'archive des sources correspondantes et leurs SHA-256 sont dans `output/installer/dist/`, version `2.2.0` ; les artefacts `2.1.0` sont conservés. Le [manifeste de livraison](generated/ministral_qualification/validation/distribution.json) donne tailles et empreintes. Ce test local ne remplace pas une machine Windows vierge : signature Authenticode, recette Windows 10/11 vierge et reprise d'un téléchargement interrompu restent les conditions de diffusion publique décrites dans [Windows Distribution](WINDOWS_DISTRIBUTION.md).

## Travail en duo

Copilot CLI / DeepSeek via Ollama a livré les lots bornés du manifeste/téléchargement (DUO-005), du corpus (DUO-006) et du raccordement des métadonnées de packaging (DUO-007). Codex a revu, corrigé et intégré chaque lot, réalisé l'adaptation native, les builds et les mesures. L'audit complémentaire DUO-008/R1 a produit des suggestions vérifiées séparément ; aucune n'est assimilée à un bug confirmé sans preuve. [Contrats et revues](duo/PROGRESS.md).
