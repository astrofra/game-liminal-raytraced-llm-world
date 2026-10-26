# Continuité des scènes — pilote du 26 octobre 2026

La roadmap CONT-01 à CONT-05 est implémentée. Une découverte reçoit le lieu
effectivement quitté, sa description de référence, les faits de sa géométrie
acceptée, la transition, l'état moteur et un extrait des événements publiés.
Le modèle invente l'architecture et écrit le `.scene` dans le même appel. Les
corrections reçoivent ce même contexte ; la narration reçoit ensuite les faits
de la géométrie finale, après les éventuelles suppressions d'occultants.

Les cinq briefs cycliques sont retirés du runtime. Ils restent uniquement dans
`tests/data/legacy_local_briefs.json` pour la recette historique de réparations.
Le repère `portal` demeure contrôlé par l'audit de visibilité : le prompt précise
que son nom ne prescrit ni porte, ni ouverture, ni position centrale. L'en-tête,
les deux exemples de syntaxe et les audits techniques restent disponibles.

## Conservation pendant la partie

Tous les lieux générés restent dans `SessionState::generated_rooms`, avec leurs
scènes acceptées, leurs descriptions et leurs liens. Une revisite recharge le
même programme sans recomposition. `reference_descriptions` conserve aussi la
référence du seuil prédéfini. Les descriptions ne proviennent jamais du dernier
texte global à la place de la référence du lieu.

L'historique existant est enrichi par les identifiants de départ et d'arrivée,
le résultat moteur et la description de référence associée. Seul un tour publié
par `UpdateSessionStateFromTurn` peut l'alimenter. Un refus publié reste un refus ;
une génération échouée ou une annulation ne crée aucun faux souvenir d'arrivée.
Les anciennes archives sans ces résultats ne sont pas promues en faits validés.
Le lancement normal recommence toujours une partie ; aucune reprise automatique
entre lancements n'a été ajoutée.

## Réduction du contexte envoyé

`ContinuityContextV1` est une copie de données structurées. `FitContinuityPrompt`
compte les messages complets avec le tokenizer et le template du modèle chargé,
y compris le candidat et les diagnostics lors d'une réparation. Le comptage ne
décode aucun token de réponse et ne constitue pas un appel LLM supplémentaire.

- Réserve inchangée : 2 048 tokens pour la scène locale, 1 280 pour une fiche
  narrative, 896 pour un tour narratif ordinaire ; marge technique de 32 tokens.
- Plafond configurable des événements/notes : 1 024 tokens par défaut, réduit au
  contexte réellement disponible après les instructions et le noyau obligatoire.
- Retrait des consultations sans conséquence et doublons ; au plus six événements
  pertinents concernant les deux derniers lieux distincts, dans l'ordre chronologique.
- Au besoin, retrait des notes puis des événements anciens. Le dernier résultat
  utile reste présent. Ensuite, réduction de sa description, puis de celle de
  l'origine, à leur première phrase complète. Les limites UTF-8 sont préservées.
- Identifiants, transition, inventaire, ressources, liens et faits géométriques
  ne sont jamais coupés. Si le noyau et la sortie réservée ne tiennent toujours
  pas, diagnostic `context_limit`, sans soumission au backend.

Une phrase unique trop longue n'est pas coupée arbitrairement. Ce pilote ne fait
pas de synthèse sémantique : son dernier recours peut donc être un refus explicite.
La réduction n'altère ni les lieux, ni les descriptions complètes conservées,
ni l'historique de session ou les archives de recherche.

## Traces et vérifications

Chaque tentative géométrique archive ses messages, son prompt templatisé effectif,
sa graine, ses temps et sa sélection. Les métriques narratives conservent la même
politique : budgets, comptages avant/après, IDs retenus, omissions et champs abrégés.
`selection_ms` inclut le chargement/vérification du modèle lorsqu'il est froid ;
il faut le distinguer du temps de traitement du prompt et du décodage.
Le diagnostic `--scene-artifacts` exporte également un `.turn.json` pour un tour.

Tests observés : neuf tests CTest passent, dont les nouvelles vérifications du
budget et du parcours ; 57 tests Python dans `scripts`, quatre dans `tests`.
Les faux backends capturent les messages réellement transmis : origine après
A → B → A → C, réparation et recomposition, faits après suppression d'occultant,
refus, compteur sans catalogue, historiques surdimensionnés, réserve de sortie,
texte français/japonais, préservation intégrale de la session, revisites et annulations.
Les compteurs des tests unitaires sont explicitement faux ; la recette native
vérifie séparément l'égalité avec le comptage Gemma.

Construction locale validée avec le cache existant :

```powershell
python scripts/build_native.py --build-dir build_ministral_v070 --publish
ctest --test-dir build_ministral_v070 -C Release --output-on-failure
python scripts/qualify_continuity.py --out C:/preuves/continuite-nouveau-dossier
```

Le premier lancement par défaut avait créé `build_native` et relancé inutilement
CUDA. Cette reconstruction a été arrêtée ; le cache compatible existant a été
réutilisé. Le dernier build indique `ninja: no work to do`. Un essai de copie du
binaire pendant la recette a rencontré le verrou Windows ; la publication a été
refaite avec succès après fermeture du processus.

## Recette réelle et limites artistiques

Preuves locales : `C:/works/duo-runs/game-liminal-raytraced-llm-world/CONT-real-01/`
et `CONT-real-02/`. Modèle Gemma 4 E4B Q4_K_M du manifeste, backend natif GPU,
langue française, graine initiale 4242, rendu de recette 512 × 256 à huit samples.
Le script suit les issues réellement proposées ; il ne crée pas de lien pour
forcer le parcours de test. La comparaison paire retire uniquement le bloc de
données de continuité, en conservant les instructions, paramètres et graine.
Les deux sondes comparatives ne publient aucun lieu dans la partie.

La première recette a parcouru refus de prise → découverte A → découverte B →
retour A → découverte C. Trois lieux sont conservés ; la scène de A reste identique
au retour. Les découvertes demandent deux appels ordinaires ; B a nécessité deux
corrections géométriques supplémentaires. Les tokens comptés avant envoi sont
identiques à ceux rapportés par le runtime, y compris pendant ces corrections.

Sur la paire exploratoire, le traitement du prompt était de 850 ms avec continuité
contre 372 ms sans ce bloc ; décodage de 10,39 s contre 9,17 s ; temps total froid
de 27,82 s contre 26,33 s. Un seul couple ne constitue pas une mesure statistique.
Les temps de découverte complets incluent aussi la narration, les audits et le
chargement froid dû au lancement séparé de chaque diagnostic.

La recette finale `CONT-real-02`, sur le binaire publié, a également terminé ce
parcours avec trois découvertes, cette fois sans correction : deux appels par
découverte, un pour la revisite et zéro pour le refus d'inventaire. Les trois
prompts géométriques comptent exactement 2 415, 2 203 et 2 444 tokens avant envoi
comme dans le runtime ; leurs mémoires récentes valent 100, 259 et 485 tokens.
Les quatre prompts narratifs comptent exactement 3 587, 3 511, 3 621 et 3 744 tokens.
Tous respectent la réserve de sortie et la marge. Le fichier de vérification
confirme aussi l'égalité de la scène et du résumé de A au retour et la bonne
origine de C, avec trois lieux conservés dans l'état final.

| Mesure finale | Avec continuité | Consigne ouverte seule |
|---|---:|---:|
| Traitement du prompt | 948 ms | 374 ms |
| Décodage géométrique | 9,74 s | 9,75 s |
| Appel total, chargement froid inclus | 27,32 s | 26,71 s |

Les découvertes complètes finales prennent 38,03 s, 36,39 s et 36,19 s dans ce
diagnostic qui relance le processus ; environ 16,5 s correspondent à chaque
chargement/vérification froid. En partie normale, le modèle reste résident.

Revue des images, programmes et textes : les compositions montrent des variations
de hauteur, densité et disposition des masses, mais aussi une répétition de masses
frontales et latérales. La paire présente des compositions différentes ; cela ne
suffit pas à démontrer une continuité spatiale intentionnelle. La narration rappelle
le trajet mais ajoute parfois poussière, strates ou fractures que les faits visuels
ne prouvent pas. Une narration de retour reste trop générique pour établir à elle
seule une reconnaissance précise du lieu. Le pilote garantit la transmission et
l'intégrité mécanique ; la fidélité et la diversité artistiques restent à améliorer.

Le duo est tracé par [DUO-028](duo/reviews/DUO-028.md) et
[DUO-029](duo/reviews/DUO-029.md). Codex a réalisé le contrat partagé, la mémoire,
le runtime, l'intégration, les builds et la recette ; DeepSeek a livré l'audit et
les tests bornés. Aucun commit ni push n'a été effectué.
