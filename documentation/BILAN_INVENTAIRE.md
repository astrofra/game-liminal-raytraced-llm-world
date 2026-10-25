# Inventaire remis en service — trois outils, six langues

25 octobre 2026. Premier livrable de la [roadmap](ROADMAP_REMISE_EN_SERVICE_INVENTAIRE.md), réalisé en duo Codex / DeepSeek ([DUO-021](duo/reviews/DUO-021.md)). **Lots 1 et 2 livrés**, build de développement publié pour le lanceur habituel.

## Utilisation

Dans `play_within_the_latent_walls.bat`, saisir `i` ou la commande d’inventaire dans la langue choisie. Le résultat vient directement de l’état du joueur : tablette cartographique, craie de délimitation et pinces à échantillons au début d’une nouvelle partie. Ces objets restent transportés pendant les déplacements et les revisites.

| Langue | Inventaire | Exemple d’examen |
|---|---|---|
| Français | `inventaire` | `examiner la craie` |
| Anglais | `inventory` | `examine the chalk` |
| Norvégien (Bokmål) | `inventar` | `undersøk kritt` |
| Danois | `inventar` | `undersøg kridt` |
| Allemand | `inventar` | `untersuche die Kreide` |
| Italien | `inventario` | `esamina il gesso` |

Les noms, descriptions, listes vides, confirmations, clarifications et refus existent dans les six langues. `i` est commun à toutes. La casse, les espaces, les accents usuels et la ponctuation sont normalisés avant résolution. En français, « qu’est-ce que je porte ? », « qu’est-ce que je transporte ? », « je voudrais examiner la tablette » et « prends la craie » sont également reconnus. Les commandes ambiguës ne choisissent pas arbitrairement un outil. `regarder` seul conserve son sens d’observation du lieu.

Examiner un outil affiche une courte description. Demander de prendre un outil déjà porté confirme sa possession, sans doublon. Un outil absent de l’état diagnostique est signalé comme absent. Les objets inconnus d’anciens états restent conservés et affichés sous leur identifiant, sans capacité inventée.

## Ce qui est livré

- La liste existante `HardState::inventory_items` reste l’unique autorité sur les possessions. Pas de nouvelle classe de sac, de seconde liste d’équipement ni de migration de sauvegarde.
- `inventory.h/.cpp` fournit un catalogue de trois identités, leurs textes localisés et une résolution bornée des commandes. L’affichage conserve l’ordre de l’état et ne répète pas un identifiant dupliqué ; il ne modifie pas la liste sous-jacente.
- Les consultations, examens et refus explicites d’inventaire sont traités au point d’entrée commun du moteur, avant les appels LLM. SDL et le mode headless partagent le comportement. Aucun appel narratif, aucune composition ni évolution thermique pour ces commandes. Le compteur de tours et l’historique enregistrent l’échange ; le compteur de déplacements reste inchangé.
- Une fiche de lieu en attente reste en attente pendant la consultation d’inventaire. Aucun retry narratif caché n’est déclenché.
- Le narrateur reçoit les possessions réellement portées et les descriptions des outils, avec une consigne explicite de conservation. Le schéma mécanique ne lui permet toujours pas d’ajouter ou retirer un objet. Un filtre supplémentaire rejette certaines formulations d’acquisition ou de perte non autorisées ; il ne constitue pas une preuve générale de cohérence sémantique de toute prose.
- Les archives ajoutent l’intention et les listes `inventory_before` / `inventory_after` à chaque résultat. Les nouvelles parties repartent avec les trois outils initiaux ; les archives ne sont jamais réinjectées.

## Limites explicites

L’affichage utilise le texte existant. Le panneau d’icônes reste une option de présentation ultérieure.

La consultation d’une carte du parcours, les marques physiques à la craie, les prélèvements, le dépôt et la reprise dans une scène restent les extensions des lots 3 et 4. Les commandes correspondantes ne simulent pas un effet déjà accompli. Aucune quatrième possession ou cible ramassable n’est créée par ce livrable.

Les tests de narration utilisent un callback contrôlé pour vérifier le contexte et le rejet d’une fausse acquisition. Une recette littéraire avec le vrai Gemma reste à effectuer lorsque l’utilisateur libérera Ollama ; aucune inférence locale n’a été ajoutée pendant son autre traitement. Les commandes d’inventaire livrées sont indépendantes de cette disponibilité.

## Vérifications observées

- **8/8 suites CTest**, dont catalogue/résolution et intégration au moteur ; **72 tests Python** de scène ; auto-test natif de navigation.
- Intégration : possessions conservées après déplacement et retour, consultation pendant une fiche en attente, examen sans appel, inventaire vide, outil absent, objet diagnostique inconnu, sauvegarde/relecture et annulation sans effet. Le prompt reçoit les trois possessions initiales ou une liste vide lorsque l’état est vide. Une réponse de test annonçant une clé ramassée est rejetée sans acquisition.
- **Six sessions exécutables, 42 commandes au total**, sept par langue : liste, trois examens, prise d’un outil déjà porté, dépôt refusé et usage non exécuté. Noms localisés vérifiés, intentions attendues, possessions conservées, BODY à 37 °C, aucune métrique d’inférence. Le modèle configuré était volontairement inexistant (`inventory-test-unavailable`).
- Une session française supplémentaire de neuf commandes a également fonctionné avec ce modèle inexistant : zéro token généré, zéro temps d’inférence ; 24,31 ms de préparation mesurés sur cet essai, hors rendu.
- **SDL**, commande `i`, modèle inexistant : sortie 0 ; tour accepté `read_inventory`, trois objets affichés, listes avant/après égales, aucun déplacement, BODY inchangé, image initiale et archive close. Archive : `run-20261025T163310Z-1042613307`.
- `git diff --check` réussi. Build Release publié ; aucun commit ni push effectué.

Preuves hors dépôt : `C:/works/duo-runs/game-liminal-raytraced-llm-world/inventory-validation/` (`six-languages.json`, sessions et archive SDL). La relecture et les ajustements de la livraison DeepSeek sont détaillés dans la revue DUO-021.
