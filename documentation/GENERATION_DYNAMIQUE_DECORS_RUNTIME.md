# Décors imaginés pendant la partie

Le mode `dynamic` demande à Ministral un plan spatial, puis le programme `.scene` correspondant. Les lieux ne proviennent pas d'un catalogue de niveaux. Le guide embarqué transmet des principes de composition ; le corpus de briefs sert au banc et n'est pas consulté par le jeu.

L'implémentation est en qualification. Le défaut reste `hybrid` tant que la campagne visuelle n'a pas établi les critères du [plan approuvé](GENERATION_DYNAMIQUE_DECORS_PLAN.md). L'optimisation du temps d'inférence est différée à la demande de l'utilisateur.

## Essayer dans le jeu

```powershell
./play_within_the_latent_walls.bat --scene-generation-mode dynamic
```

Les sept lieux canoniques gardent leur géométrie. Une sortie vers un voisin inconnu déclenche la composition ; le passage inverse est obligatoire. Les barrières invisibles et le retour canonique non réciproque restent sous l'autorité du moteur. Pour comparer l'ancien comportement, choisir explicitement `--scene-generation-mode hybrid`.

Pendant le travail, la dernière vue reste affichée. Les phases distinguent composition, construction, vérification et aperçu. Échap demande l'annulation, y compris pendant les lignes de rendu CPU ; les appels de calcul déjà engagés restent coopératifs. La première image finale doit réussir avant la publication SDL. Les images animées suivantes portent l'identifiant de génération et sont rejetées si le lieu a changé.

Si la création échoue, le joueur reste dans son lieu, sans déplacement facturé, nouveau lien ou barrière fictionnelle ajoutée. Une nouvelle commande peut réessayer. Aucun appel au compositeur hybride ne masque l'échec du mode dynamique.

## Contrat et contrôles

`SceneDesignV1` contient titre et description, caméra, enveloppe du lieu, groupes de masses orientés, supports, point focal, liaisons entre objets et groupes, volumes libres des passages et intention lumineuse. Les identifiants et conventions géométriques restent anglais ; les textes visibles suivent la langue choisie. Ce JSON ne contient aucun changement de score, d'inventaire ou de topologie.

Le moteur fournit le côté d'arrivée, les directions admissibles, le lieu précédent et les intentions des derniers lieux. Les groupes peuvent contenir des ouvertures : leur enveloppe ne devient pas un obstacle plein.

Le schéma demande les dimensions avant les centres pour rendre le calcul des hauteurs explicite. La proposition utilise `bounds: "auto"` : le moteur calcule cette métadonnée à partir des enveloppes orientées, des passages et de la caméra proposés. Il n'ajoute aucun objet et ne déplace aucune masse. L'enveloppe numérique obtenue est conservée dans le plan accepté. Ce choix évite de demander au modèle de recalculer une boîte globale redondante, source d'échecs observés pendant le pilote.

Le sol est une proposition obligatoire : `ground` donne son identifiant, `width_x`, `depth_z`, `center_x` et `center_z`. La référence verticale locale est `floor_y=0` pour les nouveaux plans ; elle ne fixe pas l'altitude du lieu dans le monde. Cette notation décrit un plan horizontal voulu par le modèle ; elle est conservée comme un groupe `ground` d'épaisseur nulle dans le plan normalisé. Le moteur n'en agrandit pas les dimensions pour corriger un passage mal placé. Le programme `.scene` reste écrit par Ministral et doit reproduire ce sol. Les anciennes notations X/Z en tableaux et les anciens plans à une autre altitude restent lisibles.

Le repère principal est décrit dans `focal_mass`, avant les groupes secondaires. Son identifiant devient directement `focal_object_id` dans le plan normalisé. Cela évite qu'un repère annoncé après une liste pleine fasse référence à un objet jamais défini. Le modèle choisit toujours sa forme, ses dimensions, son emplacement et ses appuis ; aucune scène de référence n'est insérée.

Le contrat d'écriture v8 utilise des identifiants courts : `g00` pour le sol, `m00` pour le repère, `m01` à `m03` pour les autres groupes. Ce sont des références internes, sans forme ou placement prédéfini. Les noms des primitives commencent par cet identifiant exact. Chaque groupe décrit aussi sa forme et ses ouvertures dans `shape` : une enveloppe seule ne permet pas de distinguer un bloc plein d'un assemblage de poteaux. Les plans normalisés déjà conservés restent au format `SceneDesignV1` ; les anciens identifiants et l'absence de `shape` restent lisibles.

| Limite v1 | Valeur |
|---|---|
| Groupes du plan | Nouveau plan : un sol, un repère principal et 1 à 3 groupes secondaires ; lecture des plans conservés : 2 à 24 groupes |
| Objets liés | 1 à 3 |
| Primitives déclarées | 96 maximum |
| Prefabs | 8 maximum |
| Géométrie développée | 8 192 triangles, 512 matériaux maximum |
| Coordonnées locales | ±256 m |
| Programme | 65 536 octets maximum ; 1 023 par ligne |
| Aperçu | 256 × 128, 4 échantillons/pixel, 3 rebonds diffus |
| Corrections | 2 au total, partagées entre plan et programme |
| Délai total de sécurité | 900 000 ms, réglable par `--scene-timeout-ms` |

Les contrôles rejettent propriétés inconnues, doublons, nombres invalides, caméra dégénérée, dépassements et sorties sans EOS. Après expansion native, ils vérifient les sommets des groupes, la caméra dans les solides réels, le sol sous la caméra et les passages, les contacts des supports et l'occupation des passages. Les sondes 64 × 32 recherchent notamment un point focal invisible et une vue presque entièrement bouchée. L'aperçu mesure les images noires, saturées ou sans variation utile.

Un unique bloc Markdown entourant exactement le programme peut être déballé : son contenu est conservé octet pour octet, puis audité. Aucune recherche de code dans une réponse narrative, aucun déplacement ou ajout automatique de géométrie. Les métriques comptent ces emballages. Chaque correction utilise une graine dérivée et enregistrée, afin qu'un même diagnostic ne reproduise pas à l'identique la même sortie.

Ces contrôles ne constituent ni une simulation de marche continue ni une preuve de stabilité physique. Les passages sont des volumes locaux associés au déplacement discret du jeu. Les sondes et seuils lumineux ne remplacent pas la revue artistique.

Pour les programmes courts de la voie en deux passes, la correction utilise un JSON de remplacements de lignes. Le modèle écrit chaque ligne nouvelle ; le moteur conserve les lignes non désignées puis réaudite l'ensemble du programme. Les indices invalides, doublons ou réponses tronquées n'appliquent aucune modification. Les programmes longs et la référence directe gardent la correction par remplacement complet. Le budget partagé de deux corrections reste inchangé ; `program_patch_repairs` compte les propositions de remplacement terminées par EOS. Aucune correction géométrique n'est inventée par le moteur.

L'arrivée est rédigée dans le plan avant le programme et publiée seulement après acceptation. Les associations d'objets sont contrôlées géométriquement ; la prose ne bénéficie pas d'une vérification linguistique exhaustive. Une description imprécise reste donc un défaut possible à relever dans la revue, même si elle ne peut pas modifier l'état autoritaire du jeu.

## Persistance et interactions

Chaque lieu accepté conserve son programme exact, son plan et ses associations, les versions du contrat/format/guide, le profil, le nom du fichier modèle utilisé, la graine, le nombre de corrections et les empreintes SHA-256 du programme et du plan. Le nom du modèle permet de distinguer une surcharge `--model` des paramètres du profil. Les sauvegardes ne contiennent pas les pensées privées ni les journaux d'inférence.

La revisite charge ce texte sans appel de composition. `look`/`regarder` et l'examen explicite d'un identifiant lié reprennent la description acceptée. Les autres actions sont encore interprétées pour reconnaître une direction, mais les mutations géométriques sont refusées en v1 et n'altèrent pas l'état. Cela limite volontairement les interactions de ces nouveaux lieux à l'observation et à l'exploration pour cette première version.

Les anciennes salles sauvegardées conservent également leur texte de scène lors d'une visite sans changement visuel. Leur chemin historique de mutation reste disponible si un delta visuel est accepté ; elles ne sont pas converties rétroactivement. Un hash ou une version dynamique incompatible produit une erreur explicite.

## Génération isolée et comparaison

```powershell
./build/Release/liminal_cornell_renderer.exe `
  --generate-dynamic-scene C:/temp/brief.txt --scene-entry south `
  --scene-method two-pass --scene-artifacts C:/temp/essai `
  --output C:/temp/essai.png --width 512 --height 256 --samples 8
```

Le préfixe reçoit `.design.json`, `.scene` et `.metrics.json`. En cas d'échec, le dernier candidat reste disponible pour diagnostic mais n'est pas un lieu accepté. Le JSON de mesures distingue appels, EOS, corrections, tokens, chargement, inférence, durée totale et diagnostics. Les modes `direct` et `hybrid` servent de références ; le jeu dynamique impose toujours les deux passes.

`--run-turn --scene-generation-mode dynamic --scene-artifacts C:/temp/tour` permet le même export explicite pour une vraie tentative de création pendant une partie, y compris si elle échoue. Ces fichiers de diagnostic sont séparés de `--save-state` ; une proposition rejetée n'est jamais ajoutée à la sauvegarde. Cet export de tour unique ne s'applique pas à SDL ni à `--run-session`.

```powershell
python scripts/benchmark_dynamic_scenes.py `
  --exe build/Release/liminal_cornell_renderer.exe `
  --output C:/temp/banc-dynamique --limit 3 --seeds 77 1337
```

Le banc produit résultats JSON et comparateur HTML, refuse la réutilisation de dossiers de cas déjà remplis et laisse les notes artistiques à renseigner. `blind.html` mélange les vues en masquant les méthodes ; `review-key.json` conserve la correspondance et `visual-scores-template.json` fournit les sept notes vierges. Le profil est commun aux méthodes et la température créative vaut 0,6 pour que les graines puissent modifier l'échantillonnage. Dans les deux passes, la traduction technique du plan utilise au plus 0,2 ; les valeurs effectives sont enregistrées par appel. Le plan garde la température créative, influencée par la mécanique thermique pendant le jeu. L'hybride reçoit le brief comme contexte du lieu précédent : sa procédure historique de métadonnées et de compilation reste inchangée, avec provenance et éventuel secours visibles dans les mesures. Ses contraintes ne sont donc pas identiques à celles du nouveau contrat spatial.

`--gpu-layers 0` permet une qualification CPU isolée ; l'option native correspondante est `--llm-gpu-layers 0`. Ce choix matériel doit rester visible dans les résultats. Il ne modifie ni les modèles ni la logique du jeu.

Pour la campagne complète : dix briefs de calibration avec trois graines, puis les cinq réservés (`--split holdout --limit 5`). Les seuils proposés restent 90 % d'acceptation et un gain médian d'au moins un point de composition sur l'hybride, sans en déduire automatiquement une généralisation. Aucun objectif de latence ne conditionne cette livraison.

## Construction et ressources

Le guide [`composition_v1.txt`](../assets/scene_generation/composition_v1.txt) est incorporé au binaire par CMake ; le runtime portable ne dépend pas de la présence du dépôt. Le packaging existant copie aussi les assets. Le build ajoute `liminal_dynamic_scene_test` aux contrôles CTest.

```powershell
python scripts/build_native.py --publish
ctest --test-dir build_ministral_v070 -C Release --output-on-failure
python -m unittest discover -s tests -p test_dynamic_scene_benchmark.py
```

Les [revues DUO-011](duo/reviews/DUO-011.md) et [DUO-012](duo/reviews/DUO-012.md) distinguent les livraisons DeepSeek des contrats, corrections, tests et intégrations de Codex.
