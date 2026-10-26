# Second correctif après partie — 26 octobre 2026

## Retour observé et principe retenu

Partie examinée : `run-20261026T202754Z-3892110083`, dans les archives locales.
La précision « au sol » faisait échouer la reprise de craie. Le prélèvement de
particules renvoyait une citation sans rapport avec la cible ; un second appel
inventait ensuite une impossibilité physique. Deux arrivées se ressemblaient
fortement, et un mouvement au nord conservait un retour annoncé à l'ouest.
L'inventaire n'appelait pas le modèle, mais relançait le rendu SDL.

Clarification utilisateur : **le LLM interprète les formulations libres**.
Le moteur valide la proposition et exécute les effets. Les raccourcis exacts
`i` / `inventaire` / `inventory` et de boussole restent directs, conformément à
la réponse explicite de l'utilisateur. Le premier essai de reconnaissance C++
des compléments nominaux a été retiré, ainsi que ses tests DUO-040.

## Changements

- Un appel structuré interprète l'intention et la cible : perception, déplacement,
  inventaire, prise/dépôt, carnet ou instruments. Les phrases indirectes et les
  pronoms ne passent plus par une liste C++ de verbes dans le parcours local.
  Les anciens diagnostics hors parcours local restent distincts.
- Le modèle reçoit les objets portés/déposés, les preuves littéraires retenues et
  les résultats de quatre actions récentes. La demande courante est placée en
  dernier ; les anciennes commandes ne sont pas répétées dans ce contexte.
  Le contrat `player_interpretation_v2` est un objet JSON simple de neuf champs,
  sans variantes imbriquées. Le schéma fixe les IDs admissibles et borne un index
  de phrase source ; le moteur retrouve la citation réelle depuis cet index.
  L'ordre de sortie commence par une brève reformulation de l'intention, puis
  l'action et l'identité. Origine et verdict sont déduits du résultat structuré,
  jamais de la phrase du joueur. Aucun exemple n'ajoute
  de poussière ou d'autre objet imaginaire à la scène.
- Pour un objet connu, le modèle désigne l'action et l'identité ; le moteur
  vérifie possession, portabilité et placement. Il ne réinterprète pas une phrase
  libre pour remplacer l'action du modèle. Une source littéraire reste soumise
  à sa citation réelle, aux gardes d'état et à sa disponibilité finie.
- Une validation refusée ne déclenche plus de narration supplémentaire et ne
  modifie pas la température. La reformulation interne du modèle n'est pas
  présentée comme un geste accompli ni comme une cause physique du refus.
- Les arrivées ne réinjectent plus les anciens paragraphes descriptifs comme
  modèles de rédaction. Les objets fournis appartiennent à la destination,
  y compris avant sa publication. Les indications cardinales sont complétées
  à partir des sorties acceptées, également pour les anciennes arrivées mises
  en cache. « Vous vous trouvez dans… » n'est plus assimilé à une acquisition.
- SDL conserve la vue et l'animation pour la consultation d'inventaire, sans
  reconstruire la scène ni calculer une nouvelle image. Les archives écrivent
  `view_reused` et `visual_reference`, sans prétendre produire une nouvelle preuve
  visuelle. Les exports headless continuent de fournir une scène et une image.

## Qualification

**Build natif et 14 CTest passent**, contrôle final en 20,35 s ; `git diff --check`
passe. Les essais intermédiaires sont conservés dans
`C:/works/duo-runs/game-liminal-raytraced-llm-world/interaction-feedback-2/`.
Les sorties des builds et des tests sont dans `DUO-041-01` sous la même racine.

Recettes headless observées avec Gemma, graine 351565786, sur des copies des états
réels de la partie utilisateur :

- `intent-chalk-*` : sept commandes. Reprise « ramasser la craie au sol », dépôt,
  reprise « celle qui est à mes pieds, je la remets avec mes affaires », dépôt,
  reprise « je voudrais reprendre ce que je viens de poser », inventaire, nord.
  Cinq transferts corrects de la même craie, révision 1 → 6, toujours trois instances
  et trois outils portés à la fin. Le retour annoncé est bien **sud**, avec les
  passages nord/est/sud de la pièce atteinte. Aucun nouvel objet n'est créé.
- `final-particles-*` : cinq commandes. « ramasser des particules minérales »,
  inventaire, dépôt, « ramasser les particules minérales au sol », inventaire.
  Une seule instance d'échantillon, quatre instances au total, révision 0 → 3.
  La source est la phrase réellement observée sur les fines particules ; son
  prélèvement reste épuisé et l'échantillon repris conserve son identité.

Nouvelle partie SDL finale, sans état chargé :
`semantic-sdl-final/run-20261026T215054Z-674350976`, graine 351565786,
800 × 400, quatre échantillons par pixel. Douze commandes, fermeture 0 :
arrivée à l'est, inventaires intercalés, deux dépôts/reprises de craie, puis
deux demandes successives « je voudrais emprunter le passage vers l'est ».
Les deux demandes créent bien deux nouvelles étapes du parcours. Trois pièces
générées, toujours trois instances d'objets, craie portée, aucun dépôt restant,
révision finale 4. **Aucun secours narratif dans cette recette.** Les descriptions
varient entre les pièces ; cela ne certifie pas leur précision littéraire exhaustive.

Les cinq inventaires sont mesurés à **0,281 / 0,468 / 0,515 / 0,688 / 1,063 s**
entre l'envoi et le relevé par le pilote, capture comprise. Ce n'est pas un
microbenchmark du moteur. Tous indiquent une vue réutilisée, zéro appel
d'interprétation, zéro nouveau rendu ; les références d'image sont vérifiées.
Les transferts demandent 11–13 s dans cette partie, l'arrivée initiale 42 s et
les deux déplacements libres suivants 23–26 s. Un transfert libre mobilise
l'interprète puis le narrateur ; les raccourcis évitent l'interprétation.

Captures avec visière examinées, notamment le dépôt de craie et la dernière
pièce. La construction publiée est disponible via le lanceur habituel.
La publication a terminé sans recompilation supplémentaire ; aucun commit ni push.
Les exécutables de `build_native/Release` et `build/Release` ont le même SHA-256 :
`62c056c76a02692fe777e9c4263be62a46cfbe63f154b99f17ba328a6f123a56`.

Les essais initiaux ont notamment révélé des confusions entre `use` et `drop`,
des inversions de transfert et une direction répétée interprétée comme un séjour
sur place. Ils ne sont pas comptés comme des recettes réussies. Les ajustements
portent sur le contrat et le contexte du modèle, sans ajout de parsing C++ des
formulations concernées.

Le modèle et les poids du jeu restent Gemma 4 E4B Q4_K_M via llama.cpp intégré.
Le duo DeepSeek sert au développement et ne change pas le runtime du jeu.
