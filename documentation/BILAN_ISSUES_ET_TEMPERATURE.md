# Issues improvisées et boucle thermique restaurée

25 octobre 2026 — mise en œuvre de l’[étude](ETUDE_ISSUES_ET_TEMPERATURE.md), duo Codex / DeepSeek, lot [DUO-020](duo/tasks/DUO-020.md).

## Contrat artistique et comportement

Le LLM produit dans un même appel la narration, le titre et le résumé, sa proposition pour les quatre issues et son évaluation thermique motivée. Le moteur valide ces propositions puis applique le tour. Il n’impose ni chaîne est/ouest, ni hausse thermique automatique. Aucun appel thermique supplémentaire n’est ajouté au mode local ; l’ancien double appel des revisites est supprimé de ce circuit.

Le seuil initial reste la seule scène écrite à l’avance et conserve sa sortie est. Après cette première traversée, chaque lieu peut proposer des embranchements ou une impasse. Une direction ouverte ne compose son voisin qu’au moment où le joueur l’emprunte. Un voisin déjà visité retrouve sa scène et ses liens. Un lieu accepté ne change pas ses issues au gré des descriptions suivantes ; le retour reste ouvert.

Le LLM peut faire monter, stabiliser ou baisser BODY selon l’exposition, l’effort et la protection qu’il évalue. Arrivée, revisite, observation, attente explicite et réponse à une action impossible passent par cette évaluation. Lire un instrument, écrire ou consulter le carnet reste une opération immédiate, sans appel narratif ni évolution thermique. L’absence de saisie et le temps réel de calcul n’ajoutent aucune chaleur.

Il n’y a pas de nouvelle température de coque ou d’air du scaphandre : EXT reste la température extérieure et BODY celle du corps. Ce sont des paramètres de fiction, pas une simulation physiologique.

## Proposition et validation

Le JSON contient exactement `title`, `summary`, `narration`, `thermal`, `exits`.

```json
{
  "title": "Les distances retenues",
  "summary": "Un intervalle sous le ciel, entre des masses sombres.",
  "narration": "Exemple de texte littéraire fondé sur la scène...",
  "thermal": {
    "external_temperature_c": 464,
    "body_temperature_c": 37.2,
    "reason": "L’exposition pendant la traversée augmente légèrement la chaleur corporelle."
  },
  "exits": {"north": true, "east": false, "south": false, "west": true}
}
```

Cet exemple illustre le format ; il n’est pas une réponse observée ni un texte de remplacement injecté dans le jeu.

| Champ | Validation moteur |
|---|---|
| EXT | Entier entre 300 et 520 °C ; variation maximale de 20 °C par action. |
| BODY | Nombre fini entre 34 et 43 °C ; variation maximale de 0,4 °C, avec tolérance numérique de 0,0001. |
| Issues | Exactement quatre booléens ; retour et liens connus préservés ; ouverture uniquement dans les directions admissibles. |
| Structure | Champs requis, types stricts, absence de doublons, limites de longueur et profondeur JSON. |

Les directions fixes et les bornes numériques sont également transmises au décodage JSON contraint ; la validation indépendante reste obligatoire. Les bornes sont des garde-fous du prototype. Elles ne décident pas le sens de l’évolution. Une proposition hors contrat est refusée entièrement ; le moteur ne corrige pas silencieusement ses nombres ou ses issues. La prose d’une proposition refusée n’est pas affichée.

La validation littéraire intervient ensuite : longueur, identifiants internes, répétition d’une arrivée et certains marqueurs d’incohérence. Une proposition mécanique valide peut être appliquée même si sa prose est remplacée par le texte de secours. Cette distinction figure dans les métriques (`proposal_accepted`, `success`, `fallback_reason`). La cohérence sémantique de toute métaphore reste une limite de cette validation.

## Dégagements et orientation

La caméra locale garde son cadrage fixe. Le nord du monde n’est donc pas systématiquement le haut de l’image : l’axe avant correspond au déplacement qui a amené le joueur ici, l’arrière au retour. Avec la convention du renderer, la droite de l’image correspond à −X local. La correspondance entre avant/arrière/gauche/droite et les quatre points cardinaux est fournie au modèle et archivée dans sa requête.

Le moteur mesure quatre approches à hauteur du personnage, avec intersections triangle/boîte et test d’intérieur des solides, verre compris. Il vérifie également le support au sol sur trois points de chaque approche. Le retour conserve le dégagement déjà imposé par la validation locale : centre `(0, 1.25, -9)`, dimensions `(1.6, 2, 6)`. L’avant explore jusqu’à `z=-3` ; les côtés s’étendent sur six mètres depuis la zone de la caméra. Il s’agit d’une condition géométrique nécessaire à une transition entre lieux, pas d’un contrôleur de marche continu qui prouverait tout le parcours jusqu’à l’horizon.

Une approche libre autorise le LLM à ouvrir une issue, sans l’y obliger. Aucune ouverture n’est tirée au hasard par le moteur. Le contrôle de visibilité 256 × 128 et les réparations locales existants sont conservés.

## Boucle d’échantillonnage

`BODY au début du tour → température du modèle → proposition → validation → BODY suivant`.

La fonction existante donne `0.10 + 0.80 × clamp((BODY−37)/5, 0, 1)`. Sans température de diagnostic explicite, BODY 37,0 donne 0,100 ; 37,2 donne 0,132 ; 37,4 donne 0,164. Avec une base de diagnostic positive, on lui ajoute la dérive par rapport à 0,10, dans la limite existante de 1,50.

Les écrasements fixes à 0,65 pour la narration et à 0,2 pour la composition locale ont été retirés. Les deux appels d’une découverte utilisent le corps au début du même tour. La nouvelle température agit à partir du tour suivant, sans boucle circulaire. Les nouvelles propositions partagent l’appel littéraire existant ; le budget est de 1280 tokens pour une fiche, 896 pour une réponse courante, avec délai narratif maximal conservé de 30 secondes.

## Échecs, sauvegardes et recherche

Si une scène est valide mais que la proposition échoue, la scène est conservée, la température reste inchangée et seul le retour est ouvert temporairement. Le lieu est marqué en attente (`proposal_pending`, `metadata_fallback_used`). Une action narrative suivante peut compléter sa fiche et ses issues, sans nouvelle composition. Les consultations immédiates du carnet ou des instruments ne déclenchent pas cette reprise. Une impasse réellement choisie dans une proposition valide reste une impasse.

Une annulation ne valide ni température, ni issue, ni nouveau lieu. Les données de diagnostic restent archivables avec le statut du tour annulé.

Le contrat de scène passe à **3**, avec provenance `local_direct_v2` / `local-topology-v2`, direction du retour, axe avant, quatre issues acceptées et indicateur d’attente. Le chargement diagnostique vérifie les empreintes et la concordance avec les issues de l’état. Les anciens contrats 1 et 2 restent lisibles. Cela ne réactive aucune reprise automatique : chaque lancement SDL recommence au seuil, à 37 °C, avec une nouvelle graine.

Les traces existantes conservent réponse brute, scène, images et état. Les métriques narratives ajoutent température d’échantillonnage, BODY avant/après, EXT accepté, justification thermique, issues et contraintes. Le statut global du tour indique si ces propositions ont finalement été validées dans la partie. Le premier portrait d’un lieu reste son image initiale ; une reprise de proposition se lit dans le tour et l’état suivants.

## Vérification

- Sept suites CTest réussies, dont le validateur livré par DeepSeek et les tests d’intégration étendus.
- Parcours simulé traversant les quatre directions, liens réciproques et scènes stables ; vérification des températures 0,100 → 0,132 → 0,164 transmises aux appels.
- Attente, variation rejetée, annulation sans effet, reprise d’une fiche sans recomposition, sauvegarde/relecture et provenance altérée couvertes.
- Un obstacle latéral transparent interdit l’approche correspondante ; le verre n’est pas confondu avec un espace libre.
- 72 tests Python de scène et auto-test natif de navigation réussis.

- Deux démarrages SDL sans inférence, sorties 0 : archives closes distinctes, graines 1866610271 et 198556001, seuil canonique, zéro lieu généré, BODY 37 °C et une image initiale chacun.
- Build Release publié dans `build/Release/liminal_cornell_renderer.exe`, utilisé directement par `play_within_the_latent_walls.bat`.

Un premier essai réel a composé la scène avec Gemma en 4,52 s d’inférence, après 102,35 s d’attente/chargement signalés par Ollama. Les deux appels narratifs ont expiré à 30 s, pendant la concurrence avec une autre tâche `ministral-3:14b`. La scène a été conservée en attente de proposition, BODY est resté à 37 °C et seul le retour est ouvert. Cet essai ne valide ni la qualité narrative ni la latence normale. L’utilisateur a demandé de laisser l’autre tâche continuer, puis a annoncé qu’il signalerait sa fin : la recette Gemma complète attend ce signal.

Preuves locales : `C:/works/duo-runs/game-liminal-raytraced-llm-world/topology-thermal-validation/`. Les mesures de précédents livrables restent historiques et ne qualifient pas ce nouveau couplage.
