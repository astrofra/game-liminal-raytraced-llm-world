# Issues et température — mini-étude des régressions

25 octobre 2026. Diagnostic historique du mode local, corrigé après rappel du contrat artistique par le joueur.

**Mise en œuvre :** la restauration est décrite dans le [bilan issues et température](BILAN_ISSUES_ET_TEMPERATURE.md). Les sections suivantes conservent le diagnostic qui a motivé les corrections ; les mentions « actuel » désignent cette version antérieure.

**Le contrat défini confie au LLM la narration, les propositions d’issues ouvertes ou fermées et l’évaluation thermique. La température corporelle influence ensuite l’échantillonnage du modèle narratif. Le moteur valide et applique les décisions ; il ne doit pas remplacer cette improvisation par un parcours prédéfini ou une hausse thermique automatique.**

Ce contrat est explicite dans les [décisions thermiques](DECISIONS.md#L441), la [boucle artistique de STORY](STORY.md#L127) et le [prompt de création des lieux](../src/turn_contract.cpp#L555). Ce dernier demande au LLM de décider les quatre directions et les températures après l’arrivée. L’étude initiale proposait à tort de remplacer ces choix par des règles déterministes : cette recommandation est retirée.

## 1. Pourquoi uniquement EST et OUEST ?

Parce que le nouveau circuit local impose ces directions, en court-circuitant le contrat précédent :

- Le [départ](../src/scene_compiler.cpp#L364) bloque le nord, le sud et l’ouest.
- La [découverte locale](../src/turn_runner.cpp#L3140) ne se déclenche que vers l’est.
- Les [sorties d’un lieu généré](../src/turn_runner.cpp#L3233) se limitent à l’est et à son entrée, donc à l’ouest.

Le LLM compose le décor, puis produit une fiche littéraire, mais ce circuit ne recueille plus sa proposition d’issues. Le moteur fabrique ainsi une chaîne : départ ⇄ lieu 1 ⇄ lieu 2… Au départ, seul l’est est accessible.

**Ce n’est ni une préférence répétitive de Gemma ni une limitation du raytracer.** C’est une restriction du parcours de validation, conservée pendant la stabilisation de la génération. Garder une seule pièce canonique n’imposait pas cette linéarité. Le gain de fiabilité a été obtenu au prix d’une partie de l’improvisation prévue.

### Rétablissement recommandé

Réintroduire une proposition structurée du LLM pour les quatre directions, liée au lieu qu’il imagine. Le moteur vérifierait les directions, les liens déjà établis, la cohérence du retour et les contraintes géométriques avant de l’accepter. Il ne tirerait pas lui-même les sorties au hasard et ne les ouvrirait pas toutes par défaut.

Un voisin inconnu serait composé seulement lorsque le joueur emprunte une issue acceptée. Les lieux et liens déjà validés resteraient stables pendant la partie ; une modification ultérieure de topologie demanderait un mécanisme explicite, pas une contradiction introduite dans la prose.

Cela exige aussi de généraliser la découverte aujourd’hui réservée à l’est, la provenance qui exige un retour `west` ([lecture d’état](../src/game_state.cpp#L459)), et la correspondance entre directions logiques et [dégagement géométrique près de la caméra](../src/scene_validation.cpp#L275). Une ouverture dessinée ne constitue pas, à elle seule, une issue jouable.

## 2. La température va-t-elle monter progressivement ?

**Elle devrait pouvoir évoluer selon l’évaluation du LLM, mais cette boucle est actuellement partiellement débranchée.** Le contrat ne garantit pas une montée monotone : chaleur, effort et exposition peuvent la faire monter ; protection et circonstances peuvent la stabiliser ou la faire baisser.

Précision : le HUD affiche **EXT**, température extérieure, et **BODY**, température corporelle. Il n’existe pas de température distincte de l’air intérieur ou de la coque du scaphandre. Les [valeurs initiales](../src/game_state.h#L73) sont 464 °C et 37 °C, paramètres du prototype.

| Action dans le mode local actuel | Traitement thermique |
|---|---|
| Découvrir un nouveau lieu | Températures conservées ; le contrôleur thermique est contourné. |
| Revenir ou retraverser un lieu connu | Le LLM peut proposer une hausse, une baisse ou une stabilité. |
| Observer, écouter, attendre | Pas d’évolution thermique ; attendre n’avance pas actuellement une simulation. |
| Lire les températures, noter ou consulter le carnet | Lecture et mémoire seulement. |
| Direction bloquée, absence de saisie, attente de l’inférence | Pas de progression thermique dans ce circuit. |

L’appel existe pour les [lieux connus](../src/turn_runner.cpp#L3101), mais pas à la [validation d’une découverte](../src/turn_runner.cpp#L3258), ni pour les [commandes locales](../src/turn_runner.cpp#L3531). Explorer toujours plus loin peut donc maintenir BODY à 37 °C, tandis qu’un retour le fait éventuellement varier : c’est une asymétrie d’intégration.

Le [contrôleur existant](../src/turn_runner.cpp#L2231) demande de petites variations, normalement de 0 à 0,4 °C par action. Cette amplitude est une consigne au LLM, pas une limite de variation imposée par le code : seules les bornes absolues 34–43 °C sont appliquées au corps. En cas d’échec, la valeur précédente est conservée.

### Deuxième rupture : la température du modèle

La [fonction de couplage](../src/game_state.cpp#L606) existe encore : elle produit une composante d’échantillonnage de 0,10 à 0,90 selon la température corporelle. Mais les circuits actuels écrasent ensuite ce réglage : [0,2 pour la composition locale](../src/dynamic_scene.cpp#L117) et [0,65 pour la narration](../src/turn_runner.cpp#L2682).

Même si BODY augmente, la narration ne reçoit donc plus la dérive d’échantillonnage prévue. Une phrase décrivant une chaleur suffocante ne prouve ni une hausse de BODY ni une modification du réglage narratif.

### Rétablissement recommandé

Raccorder l’évaluation thermique du LLM aux actions pertinentes, y compris l’arrivée dans un nouveau lieu et l’attente explicite, puis réutiliser le couplage corporel pour la narration. Le moteur contrôlerait les types, les bornes, la variation maximale admise et l’application unique au tour validé, sans décider à la place du modèle si le personnage se réchauffe.

La boucle doit éviter une dépendance circulaire : **température corporelle au début du tour → réglage du modèle → proposition narrative et thermique du LLM → validation → nouvel état → réglage du tour suivant**. Le HUD et les relevés doivent refléter l’état effectivement accepté. Le temps de calcul réel ne doit pas ajouter de chaleur.

Pour préserver les temps de réponse, privilégier l’intégration des champs d’issues et de température aux appels structurés existants. Leur regroupement doit conserver la cohérence entre texte, décisions acceptées et scène ; il ne justifie pas d’afficher une conséquence rejetée. La composition géométrique peut garder un réglage distinct pour sa fiabilité, mais son éventuel découplage de la chaleur serait une exception artistique à décider explicitement, pas un remplacement silencieux du contrat.

## Priorité proposée

Restaurer d’abord la boucle thermique et son effet sur le modèle narratif ; rétablir ensuite les propositions d’issues du LLM et la navigation correspondante. Préserver les gains du dernier livrable : caméra valide, scènes stables dans la partie, archives de recherche et réparations locales. **Il s’agit de restaurer des règles déjà définies, pas d’inventer une nouvelle direction de jeu.**
