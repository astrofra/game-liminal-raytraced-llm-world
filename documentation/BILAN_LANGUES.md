# Langue de session, narration et HUD

25 octobre 2026 — correction en duo Codex / DeepSeek (DUO-022).

## Règle retenue

La langue choisie au démarrage, `SessionState.language`, est la référence pendant toute la partie. La langue d'une commande, d'une note ou d'un texte produit par le modèle ne modifie jamais ce choix.

**Les libellés du HUD sont des traductions écrites à l'avance et compilées dans le code.** Aucun appel LLM ne traduit le HUD. Le choix initial sélectionne la table correspondante ; les valeurs affichées suivent ensuite l'état du jeu. La boussole conserve ses petites cases et ses initiales, conformément à la demande de l'utilisateur :

| Langue | Nord | Est | Sud | Ouest |
|---|---|---|---|---|
| Anglais | N | E | S | W |
| Français | N | E | S | O |
| Norvégien | N | Ø | S | V |
| Danois | N | Ø | S | V |
| Allemand | N | O | S | W |
| Italien | N | E | S | O |

L'identité des initiales françaises et italiennes est normale : Nord, Est, Sud, Ovest. Les raccourcis gardent leur interprétation selon la langue choisie, notamment O = ouest en italien et O = est en allemand.

## Diagnostic confirmé

Dans l'archive `run-20261025T164533Z-460693452`, la session reste `it`. Les deux premières descriptions sont italiennes ; après `SUD`, la troisième réponse commence par « Vous tentez de progresser vers le sud ». Le changement vient donc du texte généré, pas d'une modification de la langue de session.

Le prompt système contenait une consigne de vouvoiement français pour toutes les langues. La langue demandée n'était indiquée que dans les données du prompt. Aucun contrôle multilingue n'empêchait l'affichage d'une réponse française dans une partie italienne ; le filtre d'interface ne repérait que certains textes anglais. Le secours des nouvelles scènes ne distinguait que français et anglais.

## Changements

- Instruction système propre à la langue sélectionnée, répétée à chaque appel narratif : titre, résumé, narration et justification thermique dans cette langue ; commandes et souvenirs restent des données.
- Contrôle lexical commun aux six langues sur les textes proposés avant leur publication. Une dérive claire produit `wrong_output_language` et laisse la réponse de secours localisée. La réponse brute reste consultable dans les traces de recherche.
- Réponses de secours de scène traduites dans les six langues. Une description mémorisée clairement étrangère est remplacée puis réécrite dans la langue de session au cours de l'appel narratif habituel, sans recomposer la scène ni rouvrir ses issues.
- Application unique des propositions mécaniques validées : un texte refusé ne double ni déplacement ni évolution thermique. Aucun appel de réparation linguistique supplémentaire.
- Contrôle d'affichage multilingue pour les anciens textes manifestement étrangers ; les archives originales ne sont pas réécrites. Les notes saisies par le joueur restent ses citations, dans leur langue d'origine.
- Libellés de préparation, fin et échec sélectionnés dans les tables compilées du HUD ; boussole à initiales précalculées. Attente reconnue aussi avec les raccourcis traduits `vent`, `warte`, `aspetta` et `attendi`.
- Diagnostic SDL `--sdl-smoke-language en|fr|no|da|de|it`, uniquement avec `--sdl-smoke-test-ms`. Le lancement normal conserve le choix de langue initial.

## Vérification

- 9 suites CTest, 72 tests Python et auto-test de navigation.
- Tests indépendants DeepSeek : six paragraphes natifs, rejets croisés, cas réellement observé, chaînes ambiguës et 24 couples langue/direction. Codex a adapté les tests aux initiales demandées après la livraison.
- Tests d'intégration : séries de commandes de langues différentes, persistance du choix après sauvegarde/relecture, réponse française rejetée dans les cinq autres langues, panne du narrateur, correction d'un lieu mémorisé sans nouvelle composition ni changement des issues.
- Avec **Gemma 4 E4B réel**, une partie italienne de sept tours : deux découvertes, direction refusée, observation, commande française, retour connu, inventaire. Tous les textes adressés au joueur sont restés italiens. Six appels narratifs, aucun appel de traduction supplémentaire ; réponses narratives de 3,01 à 4,72 s. Première composition : 13,50 s, seconde : 3,78 s, temps muraux incluant les coûts Ollama.
- Douze tours réels supplémentaires : refus puis observation dans chacune des six langues. Lecture des douze réponses : langue attendue conservée, même lorsque l'observation est demandée en français.
- Six recettes SDL d'inventaire : langue finale correcte, commande développée dans cette langue, sortie 0 et archive close. Modèle volontairement inexistant : aucun besoin d'inférence pour le HUD ou l'inventaire.

Une capture du build final, `language-validation/hud-final-it.png`, a été relue : températures et inventaire italiens, boussole N/E/S/O dans les cases compactes.

Preuves et images hors dépôt : `C:/works/duo-runs/game-liminal-raytraced-llm-world/language-validation/`. La recette réelle a repris après le signal explicite de fin du traitement Ollama concurrent.

## Portée

Le contrôle lexical détecte les dérives évidentes ; il ne certifie pas chaque mot d'un texte mêlant plusieurs langues. Les titres très courts restent ambigus et norvégien/danois ne sont pas départagés sur leur seule syntaxe commune. Les recettes réelles sont une campagne limitée, pas une garantie de toutes les générations futures. La fidélité aux formes visibles et la qualité littéraire restent des axes de recherche distincts de la langue.

SHA-256 identique dans les deux dossiers Release : `C03E186026F1B8BC0C429528299C53B439200AC62AAE26D1BF607BE4FE1A81D9`.

Le build publié est celui lancé par `play_within_the_latent_walls.bat`. Aucun commit ni push par Codex.
