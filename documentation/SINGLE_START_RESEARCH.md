# Un seul départ, une nouvelle exploration à chaque partie

Évolution du 25 octobre : voir le [document historique archivé](HISTORIQUE_RECHERCHE.md) pour les descriptions restaurées, le carnet, les trois compositions au maximum et les nouvelles traces profondeur/ID.

Évolution suivante : les [document historique archivé](HISTORIQUE_RECHERCHE.md), puis validées par le moteur.

25 octobre 2026 — évolution demandée après la première tranche de la roadmap.

## Jouer

Le [Setup standard](WINDOWS_DISTRIBUTION.md) télécharge le GGUF en dernière phase ; le jeu fonctionne ensuite hors ligne, sans service externe. Le dossier portable complet inclut le GGUF. Le [bilan natif](BILAN_RUNTIME_AUTONOME.md) décrit la qualification du runtime.

Lancer `play_within_the_latent_walls.bat`. Le jeu commence au **seuil de la carrière**, seul lieu Eryx prédéfini. Son décor natif s'inspire du [concept 12, variante raytracer](concept-art/12-les-vingt-cinq-centimetres-du-vide-raytracer-v2.png) : dalle de verre de 25 cm, parois monumentales à gauche, sol ocre et horizon dégagé à droite. Le [rendu moteur](benchmarks/eryx-local/single-start/start.png) montre la transposition. Les texels procéduraux et la variation de rugosité du concept ne sont pas implémentés par ce lot ; les matériaux diffus restent simples.

Au seuil, **nord, est et sud** découvrent un lieu généré directement par Gemma via llama.cpp intégré. Ensuite, les issues indiquées permettent de découvrir un voisin ou de revenir vers un lieu connu ; leur direction dépend des propositions acceptées du LLM et des réservations du moteur. La génération intervient à la demande, jamais au chargement d'une archive. La graine initiale change à chaque lancement ; les découvertes et nouvelles tentatives ont des graines distinctes. Le brief s'appuie sur le lieu quitté et les événements validés, sans géométrie prédéfinie.

Dans une même partie, les lieux acceptés restent en mémoire et les revisites conservent leur géométrie. À la fermeture, cette mémoire ne devient pas l'entrée de la partie suivante. SDL refuse `--load-state`. La lecture d'un état reste disponible uniquement dans les outils headless de diagnostic.

La navigation reste textuelle. Après au maximum trois compositions, un échec conserve le joueur au lieu précédent. Au départ, seul l’ouest est fermé. Les lieux suivants proposent leurs propres issues cardinales, avec retour garanti et géométrie validée. BODY peut monter, se stabiliser ou baisser selon le LLM, et modifie l’échantillonnage du tour suivant.

Depuis le 26 octobre, le moteur protège aussi la possibilité d'explorer : une
impasse est autorisée tant qu'une autre issue inexplorée reste accessible dans
le graphe connu. Quand la dernière est empruntée, le nouveau décor doit offrir
au moins une continuation, et le moteur réserve deux nouvelles issues s'il y a
deux approches dégagées, sinon une. Le retour n'entre pas dans ce compte. Le LLM
reçoit ces ouvertures comme des faits imposés avant de narrer ; un échec de sa
réponse ne les ferme pas. Une géométrie qui condamnerait la dernière branche est
retentée dans le budget normal, sans modifier les lieux déjà acceptés. Ce
garde-fou concerne le parcours local normal ; les diagnostics anciens restent
distincts.

## Inventaire

`i` ou `inventaire` affiche les possessions ; `examiner la tablette`, `examiner la craie`, `examiner les pinces` décrit les trois outils. Les commandes et réponses sont disponibles dans les six langues : voir le [document historique archivé](HISTORIQUE_RECHERCHE.md). Ces consultations ne demandent aucune inférence et ne changent pas BODY. Les outils restent portés ; le dépôt et le ramassage dans le décor ne sont pas encore disponibles.

## Archives automatiques de recherche

Sous Windows, chaque partie écrit un dossier unique dans :

```text
%LOCALAPPDATA%\WithinTheLatentWalls\research\run-<date-UTC>-<identifiant>\
```

Sur cette machine : `C:/Users/fra/AppData/Local/WithinTheLatentWalls/research/`. Le chemin exact s'affiche aussi dans la console du lanceur. Les archives et journaux restent hors du dépôt. Aucun dossier existant n'est réutilisé pour une nouvelle partie, même avec une graine diagnostique identique.

| Fichier | Contenu |
|---|---|
| `README.md` | Index lisible des lieux, descriptions et liens vers les images. |
| `manifest.json` | Début UTC, backend, modèle, graine initiale, paramètres de rendu et consignes communes. |
| `places/place-*.txt` | Description du lieu et brief de composition pour les lieux générés. |
| `places/place-*.scene` | Programme exact du lieu accepté ; le premier lieu est également archivé. |
| `places/place-*.png` | Première image affichée, post-traitement inclus, sans HUD ; pas les huit images de respiration. |
| `places/place-*.pre-coplanar.scene`, `.opaque.scene`, `.scene` | Opaque accepté avant correction, opaque corrigé, scène finale avec les soutiens retenus. |
| `places/place-*.coplanar.json`, `.supports.json` | Paramètres, essais, décisions et empreintes des deux passes moteur. |
| `places/place-*.json` | Identifiant, description, empreinte de scène, modèle, graine et provenance. |
| `events.jsonl`, `transcript.txt` | Commandes envoyées au moteur, résultats, narration et erreurs. |
| `turns/turn-*.json` | Réponse, prompt disponible, métriques de composition, provenance et résultat du tour. |
| `turns/turn-*.scene` | Scène affichable du tour lorsqu'elle existe. |
| `turns/turn-*.candidate.scene` | Dernier candidat produit, y compris en cas de rejet ; peut être incomplet et n'est pas forcément accepté. |
| `state.json` | Dernier état validé, pour analyse et diagnostic, jamais chargé par une nouvelle partie SDL. |
| `closed.json` | Marqueur de fermeture normale ; son absence signale une interruption possible. |

Les écritures se font au démarrage puis après chaque tour, sans attendre la fermeture. Les revisites ne dupliquent pas les images de lieu ; leur narration reste dans la trace des tours. Une annulation n'ajoute pas de lieu validé. Les archives gardent chaque tentative de composition, ses programmes bruts/normalisés, les réparations locales et les bitmaps profondeur/ID disponibles. Les propositions thermiques et topologiques figurent dans les réponses et métriques des tours. Les raccourcis locaux d'interface ne sont pas des tours moteur.

`--research-dir <dossier>` sert aux recettes pour changer la racine, et crée toujours un nouveau sous-dossier. `--llm-seed` permet une reproduction diagnostique ; le lancement normal n'impose aucune graine.

## Nettoyage et compatibilité

Les six autres fixtures Eryx, leurs identifiants actifs, leurs descriptions, le graphe prédéfini et son retour non réciproque ont été supprimés. Le corpus et le script de qualification exclusivement dédiés à l'ancien parcours de sept lieux ont aussi été retirés ; Git conserve leur historique. Les archétypes procéduraux et les trois fixtures datacenter encore utilisés par les diagnostics restent disponibles.

Une sauvegarde qui référence un lieu supprimé est refusée avec un message explicite, avant normalisation. Elle n'est ni convertie silencieusement ni modifiée. Les anciennes sauvegardes dans `output/` restent sur disque ; le lanceur n'écrit plus de fichier de reprise partagé.

Le nouveau `glass_box` réutilise le matériau diélectrique existant. C'est un volume visible de verre physique, distinct des barrières topologiques invisibles. Les générations locales restent limitées aux boîtes opaques de leur contrat actuel.

## Réalisation

DeepSeek a livré la composition initiale du seuil dans le lot [DUO-016](duo/reviews/DUO-016.md). Codex a revu la scène, corrigé le cadrage, posé les fragments dans le sol, ajouté le volume de verre, supprimé l'ancien parcours et implémenté l'exploration et l'archivage. Les mesures de la [rapport historique archivé](HISTORIQUE_RECHERCHE.md) restent historiques : elles ne constituent pas une qualification des nouvelles graines aléatoires ou de toutes les découvertes successives.

## Recette historique avant migration native

- Quatre CTest réussis : protocole, rendu, génération, navigation, archives distinctes et intégrité des états ; 72 tests Python de scène réussis, auto-test de navigation réussi.
- Deux lancements SDL indépendants, graines `32388405` et `19736041` : un lieu généré différent à chaque fois, archives closes distinctes, descriptions et prompts non vides. Les `.scene` archivés sont identiques aux programmes présents dans l'état validé. [Partie 1](benchmarks/eryx-local/single-start/play-01.png), [partie 2](benchmarks/eryx-local/single-start/play-02.png), [mesures minimales](benchmarks/eryx-local/single-start/examples.json).
- Parcours headless réel `east → east → west → west`, graine 42 : deux lieux générés, quatre liens, retour à `canonical:quarry_threshold` sans recomposition des lieux acceptés.
- Un essai précédent de trois découvertes a échoué sur le troisième repère, trop peu visible après correction. L'échec est conservé dans les preuves ; il n'est pas compté comme réussite. Les nouvelles tentatives SDL changent de graine pour permettre une autre composition.
- `--sdl --load-state ...` est refusé explicitement. Les anciens fichiers sont laissés intacts.
- Fermeture SDL pendant une génération : archive créée dans le dossier Windows par défaut, résultat `not_committed`, zéro lieu généré et zéro déplacement ; état initial intact et marqueur de fermeture écrit.

Preuves privées de cette recette : `C:/works/duo-runs/game-liminal-raytraced-llm-world/single-start/`. Aucun nouvel installeur livré ; l'exécutable de développement utilisé par le lanceur est publié.
