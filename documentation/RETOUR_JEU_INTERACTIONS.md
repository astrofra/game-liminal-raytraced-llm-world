# Retour de partie — interactions du 26 octobre 2026

La session `run-20261026T175239Z-2531023610` a révélé un écart entre les tests
de conservation et la souplesse réellement offerte au joueur. « Regarder par
terre » décrivait une poussière que le ramassage ne recevait pas. Le dépôt et la
reprise de la craie réussissaient dans l'état, mais leurs narrations étaient
rejetées par une interdiction générale des transferts. L'observation sans mention
de la craie remplaçait aussi tout le paragraphe par le même secours.

## Correction

Les observations acceptées conservent désormais leurs phrases dans
`observed_descriptions`, par lieu, indépendamment de la fenêtre d'historique.
Leur publication suit celle du tour : une annulation, une réponse rejetée ou une
simple note du joueur n'ajoute aucune preuve. Les mentions de dépôts mobiles et
de sources déjà résolues ne deviennent pas de nouvelles sources à prélever.
Les hypothèses restent exclues ; une négation porte sur la cible concernée,
sans interdire un objet simplement décrit « sans marques ».

Le résolveur reçoit la référence du lieu et un extrait des observations
conservées. La sélection conserve au plus douze phrases et 1 800 octets,
priorisant les détails demandés ; la mémoire complète reste sérialisée. Le LLM
choisit parmi des citations et identités locales, sans filtre exigeant que la
commande reprenne leurs mots exacts. Il apprécie la correspondance et la
plausibilité ; le moteur vérifie citation, identité, accès, révision, propriété
et conservation. Une demande de poussière ou de gravats peut désigner un petit
échantillon. Une absence reçoit un motif concret, et non une clarification
automatique. Ce motif utilise l'appel d'interprétation déjà effectué.

Un objet conserve aussi le nom employé lors de sa première prise. La résolution
locale tolère les pluriels simples ; les autres formulations passent par
l'interprète. Si le verbe prendre ou déposer est reconnu, le schéma impose ce
sens. Un échantillon n'efface pas toute sa matière source. Le prélèvement reste
borné à un par source ; quantités, conteneurs et extractions répétées restent
hors périmètre.

Le narrateur reçoit l'action achevée, le nom de l'objet et ses positions avant et
après. La garde autorise ce transfert précis, tout en refusant les acquisitions
supplémentaires détectées. Une simple omission de dépôt est complétée localement
en conservant la prose ; une couverture contradictoire reste rejetée. La phrase
mécanique de confirmation n'est ajoutée que si le geste n'est pas déjà raconté.
Le statut des prélèvements n'est plus récité à chaque réponse valide.

Le premier emplacement de dépôt est avancé de 20 cm lorsque la visibilité, le
support et les collisions le permettent, avec nouvelle validation sur les huit
vues. Le premier point valide reste utilisable si cet ajustement échoue.

## Qualification

Les tests couvrent les narrations de dépôt/reprise, l'omission corrigée sans
remplacement, les effets inventés, la couverture contradictoire, la conservation
des observations après quatorze tours et une revisite, la sérialisation,
l'annulation et l'absence de nouvelle source issue d'un dépôt observé.

Les recettes avec Gemma et SDL, leurs mesures et le binaire final sont consignés
dans la [revue DUO-039](duo/reviews/DUO-039.md). Les journaux et images restent
hors dépôt dans `C:/works/duo-runs/game-liminal-raytraced-llm-world/interaction-feedback/`.

La souplesse sémantique dépend toujours du modèle. Les gardes linguistiques ne
constituent pas une preuve exhaustive de fidélité de toute paraphrase ; la
recette examine aussi les réponses affichées. Les premiers essais de ce correctif
ont révélé un refus trop prudent de la poussière, une inversion prendre/déposer
et une inversion narrative : leurs traces sont conservées, puis les contrats et
gardes ont été corrigés. Aucune promesse de compréhension universelle ou de
réduction du temps d'inférence n'en découle.
