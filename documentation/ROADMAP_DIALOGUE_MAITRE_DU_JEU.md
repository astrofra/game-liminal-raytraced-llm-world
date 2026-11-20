# Roadmap — un maître du jeu pour un dialogue fluide

20 novembre 2026. Feuille de route issue du retour de partie. Le
[bilan d'implémentation](DIALOGUE_MAITRE_DU_JEU.md) décrit le comportement livré
et les recettes observées. La référence de conception était `c6b0467`.

## 1. Expérience recherchée

Le joueur doit pouvoir parler naturellement à un maître du jeu : demander de
l'aide, exprimer une inquiétude, revenir sur un détail, agir par synonymes ou
désigner un objet par un pronom. Le modèle comprend et improvise ; le moteur
conserve la carte, les lieux, les objets, les mesures et les conséquences.

Une interaction textuelle n'est plus automatiquement un tour de simulation,
une description complète ou un nouveau calcul d'image. La question centrale
est : **le joueur demande-t-il une action susceptible de modifier le monde ?**
Le LLM propose la réponse ; le moteur détermine les effets effectivement autorisés.

Exemples de ton à viser, illustratifs et non répliques à coder :

| Demande | Réponse et comportement attendus |
|---|---|
| « Est-ce que je vais mourir ? » | Répondre à l'inquiétude à partir du scaphandre et des dangers connus. Par exemple : « Les relevés ne condamnent pas encore votre sortie. Gardez un œil sur la chaleur. » Aucun dommage, danger ou verdict de mort créé pour répondre. |
| « Qu'est-ce que je dois faire ? » | Proposer une piste liée aux indices connus ou aux sorties praticables, sans dévoiler l'inconnu ni imposer une solution unique. |
| « Quelle heure est-il ? » | Exploiter une horloge diégétique si elle existe ; sinon improviser une réponse sur l'absence de repère temporel. Ne pas inventer une heure exacte persistante ni lire l'heure Windows par défaut. |
| « Et pourquoi ? » | Continuer le sujet précédent, sans recommencer la présentation du lieu. |
| « Je prends le couteau » | Dans une fixture contenant une seule dague portable au sol, le LLM choisit son identifiant. Le moteur transfère cette dague. Cet exemple n'ajoute pas une arme au jeu actuel. |
| « Je pose le couteau » | Si cette même dague est portée, déposer la même instance, sans exiger son nom canonique. |
| « Je reprends ce que je viens de poser » | Retrouver le référent dans les événements validés et les objets présents. |

La génération hybride du décor reste au cœur du projet : composition proposée
par le LLM, compilation, contrôles géométriques, corrections et rendu par le
moteur. Les lieux acceptés persistent pendant la partie, avec leurs objets.
Les diagnostics nommés « hybride » ou « deux passes » ne deviennent pas pour
autant le nouveau parcours normal.

## 2. Point de départ vérifié dans le dépôt

Lire également l'[état technique](TECHNICAL_STATE.md), la
[continuité scénique](CONTINUITE_SCENES.md) et le
[second retour de partie](RETOUR_JEU_INTERACTIONS_2.md).

| Acquis | Limite pertinente pour ce chantier |
|---|---|
| `ResolveObjectAction`, dans `src/object_resolution.cpp`, interprète déjà les formulations libres par JSON contraint. | Le prompt regroupe explicitement observation, question, réflexion et attente sous `perceive`. Il manque une intention de conversation distincte. |
| `SceneActionOutcome` et `NarrateScene`, dans `src/turn_runner.cpp`, exploitent les faits du lieu. | La branche de séjour sur place repasse par la narration scénique, avec ses contrats de décor, d'objets, de chaleur et d'ambiance. Une conversation n'a pas son propre contrat. |
| Les lieux générés acceptés conservent leur géométrie ; les revisites ne recomposent pas normalement l'architecture par LLM. | Il faut distinguer génération 3D, reconstruction des couches, raytracing et répétition narrative. Le ressenti de « décor régénéré » ne prouve pas que ces quatre opérations se produisent toutes. |
| `view_unchanged` et `reuse_unchanged_view` permettent déjà de conserver la vue et l'animation pour l'inventaire dans SDL. | Cette voie doit couvrir les conversations, clarifications et consultations sans effet visuel. Les exports headless peuvent encore produire une image : ne pas les confondre avec le coût du parcours SDL. |
| Registre d'objets, propriétaires, sources littéraires, retraits et observations persistent. | La tolérance sémantique existe déjà, mais elle doit être qualifiée systématiquement sur noms approchés, pronoms, ambiguïtés et objets absents. |
| `FitContinuityPrompt`, dans `src/continuity.cpp`, compte les tokens avec le modèle réel. | La sélection actuelle (six événements, deux lieux, plafond mémoire par défaut de 1 024 tokens) ne constitue pas encore une mémoire de conversation dédiée. L'interprète fournit aussi ses propres événements récents. |
| Archives de recherche, scènes, réponses, métriques et décisions audio existent. | Les données sont réparties entre plusieurs champs ; les appels ne forment pas un journal exhaustif unifié. Les PNG ne sont pas des captures complètes de chaque interaction. |

Ce constat est une lecture statique, pas une nouvelle mesure de latence ni une
reproduction du retour du testeur. La première étape d'implémentation établira
une référence expérimentale avant modification.

## 3. Architecture cible : interpréter, résoudre, raconter

Conserver trois étages simples, exécutés avec le même runtime Gemma/llama.cpp.
Ce sont des rôles et des contrats distincts, pas trois modèles à charger.

```text
Entrée du joueur
  ├─ raccourci exact inventaire / boussole → commande structurée existante
  └─ formulation libre → LLM interprète → proposition structurée
                                             ↓
                          moteur : validation et calcul des effets
                            ├─ conversation / clarification → contexte en lecture seule
                            ├─ observation / carnet → découvertes autorisées
                            └─ action → candidat d'état et décor nécessaire
                                             ↓
                        LLM maître du jeu : réponse ciblée selon le résultat
                                             ↓
                   publication cohérente → affichage → archivage finalisé
```

### 3.1. Interprétation : une seule proposition bornée

Étendre ou extraire l'interprète existant plutôt qu'ajouter un classificateur
devant lui. Pour une phrase libre ordinaire : **un appel d'interprétation puis
un appel de réponse**, sans appel de composition pour une question.
Les raccourcis directs gardent leur comportement rapide.

Contrat conceptuel à stabiliser dans le premier lot d'architecture :

```json
{
  "kind": "conversation",
  "world_change_requested": false,
  "action": "none",
  "target_ids": [],
  "direction": null,
  "topic": "survival",
  "needs_clarification": false
}
```

Les catégories minimales sont `conversation`, `observation`, `action`,
`clarification`. Les actions existantes, révisions et références de sources
restent disponibles dans leur branche. Le schéma contraint leurs combinaisons ;
un `move` avec `world_change_requested=false` est incohérent et ne s'exécute pas.
Le booléen exprime une demande, jamais une autorisation ni un résultat.

Le modèle reçoit les identifiants et noms des objets accessibles, leur position
logique, les issues connues, les références utiles et les derniers échanges.
Il interprète la phrase entière, notamment négation, hypothèse et citation :
« si j'allais au nord ? » ou « ne prends pas la dague » ne déclenchent pas le geste.
Aucun dictionnaire de synonymes ni parseur de langage naturel C++ à ajouter.

Pour « je prends la dague et je pars au nord », le premier périmètre pose une
clarification courte sur l'action à effectuer d'abord, sans en exécuter une à
l'insu du joueur. Pour « je vais au nord, est-ce dangereux ? », une action unique
et sa question peuvent être traitées ensemble : réponse fondée sur le résultat
validé. Les séquences arbitraires d'actions restent une extension ultérieure.

### 3.2. Résolution : les effets décident du travail nécessaire

Le moteur produit un résultat structuré : action acceptée/refusée, motifs,
références résolues, différences d'état et opérations visuelles nécessaires.
Il distingue au minimum monde physique, connaissances, temps de simulation,
vue et ambiance. Ces effets sont dérivés de la validation, pas dictés par le LLM.

| Cas | Monde et connaissances | Travail visuel |
|---|---|---|
| Question, conseil, inquiétude, clarification | Historique conversationnel seulement | Réutiliser scène, texture et animation. |
| Consultation d'inventaire ou de mesures | Lire l'état existant | Réutiliser la vue. |
| Observation ciblée | Peut enrichir les observations ou révéler un indice déjà autorisé | Réutiliser la vue, sauf nouvelle couche visible justifiée. |
| Prise/dépôt accepté | Transfert d'une instance, sources et enquête actualisées si nécessaire | Recomposer les objets mobiles ; recalculer seulement la vue affectée. Architecture conservée. |
| Déplacement vers un lieu connu | Changer de lieu, appliquer ses objets et repères persistants | Réutiliser sa scène ; rendu si la vue requise n'est pas disponible à la bonne révision. |
| Déplacement vers un lieu inconnu | Préparer lieu, liens, objets et indices selon les règles existantes | Génération hybride complète et contrôles existants. |
| Déplacement bloqué déjà connu | Aucun déplacement | Réutiliser la vue ; réponse courte. |
| Premier contact avec une paroi invisible | Aucun déplacement, mais découverte persistante | Actualiser la couche de paroi et la boussole, sans réinventer le lieu. |
| Action impossible ou objet absent | Aucun transfert | Réutiliser la vue ; expliquer à partir du motif validé. |
| Attente explicitement demandée | Éventuelle avance thermique/temporelle selon les règles définies | Aucun nouveau décor sans effet visuel réel. |

Un simple oui/non ne suffit donc pas à décider de tout le rendu : un déplacement
échoué peut révéler une paroi ; une observation peut enrichir le carnet sans
changer la géométrie. Ces exceptions doivent être testées explicitement.

Choix proposé : **une conversation ne fait pas avancer BODY, la chaleur du lieu
ou une horloge de simulation**. L'animation et les boucles audio continuent.
Séparer numéro d'interaction et progression de simulation, sans réinterpréter
silencieusement les anciens compteurs sérialisés. L'attente devient une action
distincte. La température d'échantillonnage reste couplée à l'état thermique réel.

### 3.3. Réponse : un maître du jeu, plusieurs formats d'intervention

Créer un contrat de réponse conversationnelle allégé : texte, références
factuelles utiles et éventuellement question de clarification. Il n'expose
aucun champ de nouvelle géométrie, sortie, objet, température ou mix audio.
Le chemin d'action narre uniquement le résultat moteur validé. Une observation
explicite conserve son chemin de découverte, distinct du dialogue ordinaire.

Répondre d'abord à la demande, généralement en une à trois phrases. Une arrivée
peut installer le lieu ; une manipulation décrit son effet ; une question reçoit
une réponse. Une demande explicite de panorama ou d'explication longue peut
dépasser ce format. Éviter descriptions réinjectées, listes d'issues et rappels
d'inventaire non demandés. Préserver la langue choisie parmi les six existantes.

Improviser sensations, images et hypothèses cohérentes. Ne pas transformer une
crainte en menace effective, une suggestion en objectif obligatoire, une question
en preuve, ou une métaphore en objet ramassable. Le registre d'observations
actuel ne doit jamais absorber automatiquement une réponse conversationnelle.
Les mesures et noms propres factuels viennent de l'état ; les hypothèses restent
identifiées comme telles. Ne pas inventer une résolution de l'enquête de Vey.

Le schéma protège les mutations structurées, mais ne prouve pas à lui seul la
véracité de chaque phrase. Contrôler les références et incohérences détectables,
puis évaluer la prose en recette réelle. Ne pas remplacer cette difficulté par
un parseur sémantique C++. En cas d'échec, réponse de secours courte et localisée,
sans déplacer le joueur ni répéter la description entière.

Conserver une publication atomique pour les actions : état, objets, vue et
événements doivent correspondre. Une narration manquante peut être remplacée
par le résultat moteur localisé ; une annulation avant publication laisse l'état
antérieur. Ne pas afficher en streaming une réussite physique encore provisoire.
Borner délais, longueur et réparations ; aucune boucle ouverte d'agents.

## 4. Tolérance des objets et mémoire du dialogue

Résoudre les noms à partir de candidats situés : inventaire, objets de la pièce,
sources littéraires retenues et référents récents. « Couteau » peut désigner une
dague ; il ne crée pas un couteau supplémentaire. Le moteur contrôle identité,
propriétaire, portabilité, disponibilité, provenance et révision. Une cible
unique plausible s'exécute sans exiger une reformulation au vocabulaire exact.

Si deux lames sont présentes et qu'aucun contexte ne les départage, demander
« La dague au sol ou celle que vous portez ? ». Mémoriser les candidats de cette
clarification pour interpréter « celle au sol ». Une référence ancienne ne doit
pas permettre de prendre un objet laissé ailleurs. Une source consommée reste
épuisée. Une demande d'objet absent ne suffit jamais à le faire apparaître.

Conserver trois niveaux de mémoire :

1. **État autoritatif complet** : lieux, liens, scènes, objets, indices, mesures ;
   persistant pendant la partie, indépendant de la fenêtre du LLM.
2. **Fil conversationnel récent** : questions et réponses réellement affichées,
   sujet, référents par identifiant, clarification en attente. Distinct des faits.
3. **Contexte sélectionné pour chaque appel** : règles, commande actuelle, état
   pertinent, échanges récents et rappels nécessaires. Les détails omis du prompt
   ne sont jamais supprimés du monde ou des archives.

Compter chaque prompt complet avec le tokenizer et le template réels :
`entrée + réserve de sortie + marge <= n_ctx`. Étendre la politique existante
à l'interprétation et au dialogue ; mesurer séparément leurs budgets. Préserver
prioritairement commande, contraintes, cibles nécessaires et clarification en
cours ; retirer ensuite les répétitions et événements anciens. Un sujet éloigné
peut nécessiter une sélection ciblée de mémoire par le LLM, sous budget explicite.
Ne pas ajouter d'appel de résumé à chaque interaction. Une compression ultérieure
reste traçable, avec sources et statut non autoritatif.

Si le noyau indispensable dépasse la fenêtre, échouer proprement ou demander
une précision ; ne pas fournir silencieusement un faux inventaire « complet ».
Tester des dizaines d'échanges sans mouvement : la continuité doit survivre et
les coûts rester bornés. L'archivage intégral sur disque n'est pas limité par le
budget de contexte, et ne transforme pas les archives en sauvegarde jouable.

## 5. Tout archiver à chaque interaction

### 5.1. Réutiliser l'existant et fermer les lacunes

Sources principales : `src/research_archive.cpp`, `src/llm_runtime.cpp`,
`src/object_resolution.cpp`, `src/sdl_frontend.cpp` et le
[guide des archives](SINGLE_START_RESEARCH.md).

| Élément demandé | Déjà présent | Complément nécessaire |
|---|---|---|
| Prompts envoyés | Prompts de tour/thermique, consignes communes, prompt d'interprétation dans les métriques ; autres traces selon les phases. | Un enregistrement par appel, avec messages exacts, prompt formaté réellement envoyé, schéma/grammaire, paramètres et budget. Inclure corrections, échecs et annulations. |
| Réponses du modèle | Réponses brutes, réparations, interprétation, candidats de scène et texte affiché à différents endroits. | Relier chaque sortie à son appel ; conserver brut, résultat parsé/validé et version finalement affichée, sans écrasement entre tentatives. |
| Scène générée | Scène par lieu, scènes de tours disponibles, programmes rejetés et audits géométriques. | Identifier à chaque interaction la base acceptée, les couches objets/parois et la scène composée effective, même lorsqu'elles sont réutilisées. |
| Réglages sonores | Décision `audio` par tour et `audio-events.jsonl`, avec événements du frontend. | Instantané corrélé par interaction : cibles proposées/validées, gains effectifs à la capture, rampe, positions des boucles, catalogue, volume et indisponibilité éventuelle. |
| Capture d'écran | PNG issus du tampon de rendu, sans HUD ; première vue par lieu et images selon la révision d'objets. | Nouvelle capture du framebuffer SDL final après chaque réponse, avec visière, HUD et dialogue affiché, même si le raytracer n'a pas travaillé. |

Attention : `state.json` est aujourd'hui remplacé par le dernier instantané.
Des états d'objets et d'enquête figurent aussi dans les tours, mais cela ne donne
pas un instantané complet immuable avant/après chaque interaction. De même,
`objects.revision > 0` dans `WriteImage` n'est pas un test de différence entre
deux tours : ne pas déduire la cadence des images du seul fait qu'un objet vient
ou non de bouger. Voir la [revue DUO-049](duo/reviews/DUO-049.md).

### 5.2. Contrat d'archive cible

Une interaction commence à la soumission d'une entrée, y compris raccourci,
question, clarification, refus ou tentative annulée. Prévoir aussi une entrée
initiale pour le premier écran jouable. Un appel LLM est une sous-opération ;
il peut y en avoir zéro, deux ou davantage lors d'une génération avec corrections.

Structure indicative, versionnée à l'implémentation :

```text
run-.../
  manifest.json                 # versions, build, modèle/empreinte, paramètres
  interactions/000001/
    input.json                  # texte, langue, date, identifiant, état de départ
    calls/0001-request.json     # rôle, messages, prompt effectif, schéma, réglages
    calls/0001-response.json    # brut disponible, résultat, arrêt, tokens, durées
    state-before.json
    state-after.json
    outcome.json                # effets validés, refus, secours, publication
    scene.json                  # références/empreintes base + couches + rendu
    audio.json                  # proposition, validation et état effectif
    screen.png                  # nouvelle capture après présentation de la réponse
    presentation.json           # instant, dimensions, phase d'animation, texte complet
    completion.json             # complet, incomplet, annulé ou erreur
  artifacts/...                 # scènes, images de rendu, essais et audits immuables
```

Un artefact inchangé peut être stocké une seule fois et référencé par empreinte,
si la référence est autonome dans le dossier de partie et vérifiable. Rien n'est
omis sous prétexte que la vue n'a pas changé. La capture d'écran est, elle, prise
à chaque interaction : elle montre aussi le nouveau texte. Archiver son texte
complet séparément si le panneau défile. Aucune nouvelle composition ou passe
de raytracing ne doit être déclenchée seulement pour capturer l'interface.

Écrire la requête avant l'inférence et la réponse disponible à sa terminaison,
y compris arrêt anticipé. Conserver les tentatives rejetées, sans les confondre
avec l'état publié. Relier partout `run_id`, `interaction_id`, `call_id`, rôle,
révision et horodatage. Finaliser le manifeste d'interaction seulement après
affichage/capture et écritures réussies ; publication par fichiers temporaires
et remplacement atomique lorsque pertinent. Un crash peut laisser une interaction
incomplète identifiable, sans invalider les précédentes.

Si fenêtre fermée, capture impossible ou disque plein : enregistrer l'absence
et sa cause quand possible, signaler l'échec, ne jamais annoncer une archive
complète. Le mode de recette échoue sur une preuve obligatoire manquante.
Le jeu ne doit pas corrompre son état pour tenter de sauver une trace. Écriture
et encodage doivent rester bornés et ne pas bloquer l'audio ; mesurer coût disque
et temps d'archivage. Pas de suppression automatique des traces dans ce chantier.

Les archives du jeu restent sous `%LOCALAPPDATA%/WithinTheLatentWalls/research/`.
Les preuves de développement restent sous
`C:/works/duo-runs/game-liminal-raytraced-llm-world/`. Aucun journal brut dans Git.
Le headless produit ses preuves de rendu et signale explicitement l'absence de
capture SDL ; il ne suffit pas à valider le critère « écran réellement présenté ».

## 6. Lots et ordre de réalisation

| Lot | Livraison et zones concernées | Critère de sortie |
|---|---|---|
| MJ-00 — référence | Scénarios figés, état de départ, mesures du binaire actuel et inventaire des appels ; `scripts/`, `tests/`, rapport hors dépôt. | Reproduire questions répétitives, transferts, revisite ; distinguer composition, rendu, narration, temps et audio. Conserver tous les essais. |
| MJ-01 — traces | Journal par appel dans `llm_runtime`, corrélation dans `research_archive`, captures finales dans `sdl_frontend`. | Une interaction de chaque catégorie est inspectable de bout en bout, avec zéro appel explicite pour un raccourci ; échec d'écriture et annulation visibles. |
| MJ-02 — contrat et routage | Interprète, `turn_runner`, structures d'état/résultat ; séparer dialogue, observation, attente et effets. | Une série de questions ne change ni lieu, ni objets, ni mesures, ni scène ; zéro composition et zéro nouveau raytracing dans SDL. Parois et découvertes restent fonctionnelles. |
| MJ-03 — maître du jeu | Prompts conversation/action, fil récent, budget dans `continuity`, interface de réponse. | Réponses ciblées, relances compréhensibles, absence de faits mécaniques inventés sur le corpus ; aucune redescription automatique. |
| MJ-04 — références souples | Contexte et résolution sémantique des objets, clarifications persistantes ; réutiliser `object_interactions`. | Synonymes/pronoms transfèrent la bonne instance ; ambiguïté réelle clarifiée ; aucun objet fantôme, doublon ou prélèvement répété. |
| MJ-05 — recette autonome | Tests moteur, Gemma réel, parcours SDL, comparaison de transcriptions et captures, bilan documenté. | Critères ci-dessous satisfaits, défauts corrigés et rejoués ; limitations restantes nommées avec leurs preuves. |

Ordre : MJ-00 → MJ-01 → MJ-02 → MJ-03 → MJ-04 → MJ-05. Les scénarios d'objets
sont définis dès MJ-00 et exécutés à chaque lot concerné. Premier jalon jouable
après MJ-03 : questions fluides dans un monde stable, avec preuves complètes.
Ne pas attendre la fin pour jouer et relire les réponses réellement produites.

Codex garde contrats, interfaces, mémoire, sémantique, état, builds et intégration.
Les lots bornés de tests ou de documentation sont délégués via le kit du dépôt,
avec TASK, worktree, revue et mise à jour de `duo/PROGRESS.md`. Aucun changement
des modèles du jeu ne découle de la configuration du duo.

## 7. Tests et auto-évaluation pendant l'implémentation

**Codex exécute les tests et joue les recettes lui-même**, examine les traces et
les captures, évalue le résultat, corrige les défauts et rejoue les cas concernés.
La qualification ne doit pas être reportée au prochain test du joueur.

### 7.1. Matrice minimale

| Scénario | Oracle moteur | Évaluation de l'expérience |
|---|---|---|
| Mourir / quoi faire / heure / « pourquoi ? » | Aucune mutation physique, thermique ou audio ciblée ; aucune génération/rendu nouveau. | Répondre à chacune des demandes et maintenir le fil. |
| « Le passage nord est-il dangereux ? », négation, hypothèse | Aucun départ implicite. | Conseil ancré dans ce qui est connu. |
| Observation puis question puis observation d'indice | Seul le chemin autorisé peut enrichir les connaissances ; aucune création de source par la question. | Détail pertinent, aucune redescription imposée. |
| Prendre « couteau » quand une dague existe ; déposer/reprendre avec pronoms | Même identifiant, propriétaire exact, nombre d'instances constant. | Tolérance sans correction scolaire du vocabulaire. |
| Deux objets plausibles, puis « celui au sol » | Premier tour sans transfert, second sur le candidat demandé. | Une clarification utile, pas une boucle. |
| Objet absent, laissé ailleurs, source épuisée ou structure fixe | Aucun objet inventé ou dupliqué. | Refus contextuel bref, sans fausse contrainte matérielle. |
| A → B → A, avec dépôt en A | Même géométrie, objets retrouvés, liens réciproques. | Rappel ciblé plutôt que paragraphe identique à chaque retour. |
| Contact invisible initial puis tentative répétée | Premier contact révèle la paroi ; second n'invente rien. | Compréhension du blocage et boussole cohérente. |
| Attendre, mesurer, lire le carnet | Effets conformes à chaque catégorie, sans confusion avec conversation. | Réponse adaptée à la demande. |
| 40 échanges sur place et retour sur un ancien sujet | Budget respecté à chaque appel, monde intact, sources de mémoire traçables. | Pronoms récents et relances restent intelligibles. |
| JSON invalide, contexte saturé, timeout, annulation, échec de rendu | Aucune publication partielle ; pas de réussite racontée à tort. | Secours court, interface récupérable. |
| Fermeture pendant appel, disque plein, capture indisponible | États publiés préservés, trace incomplète explicitée. | Aucun succès d'archivage mensonger. |

Ajouter des formulations françaises familières, fautes de frappe et synonymes
non cités dans les prompts ; vérifier les autres langues du jeu. Le corpus doit
comporter des cas réservés à l'évaluation pour éviter d'ajuster seulement les
exemples connus. Une graine reproductible aide au diagnostic ; répéter sur au
moins trois graines avec Gemma réel, sans supposer une identité bit à bit GPU.

### 7.2. Niveaux de preuve et critères mesurables

- Tests déterministes : contrats, révisions, routage, différences d'état,
  annulation, budget et intégrité des archives ; doubles de LLM pour forcer les
  erreurs difficiles. Zéro violation des invariants exigée.
- Recettes avec le GGUF réel : comparer ancienne et nouvelle version sur les
  mêmes scénarios et paramètres. Relever appels par rôle, tokens, secours,
  demandes de précision, temps jusqu'au premier texte et réponse complète.
- Recettes SDL : inspecter les captures après questions, transfert, revisite et
  premier contact ; vérifier texte, visière, boussole, objets, continuité des
  animations et absence de clignotement. Des tests headless seuls ne suffisent pas.
- Archives : 100 % des interactions terminées de la recette disposent des cinq
  éléments demandés ou de références vérifiables ; les appels non exécutés sont
  explicitement absents. Les cas d'échec injectés sont marqués incomplets.
- Performance : conversation ordinaire plafonnée à deux appels, hors réparation
  bornée et documentée ; zéro appel de scène, zéro raytracing supplémentaire.
  Publier médiane et p95 sur au moins 30 questions, coût de capture compris,
  en séparant chargement initial et modèle résident. Fixer la cible en secondes
  à partir de MJ-00, sans promettre une latence matérielle non mesurée.
- Qualité : Codex note chaque réponse de 0 à 2 sur pertinence, continuité,
  concision, naturel et fidélité aux faits (0 : échec, 1 : partiel, 2 : satisfaisant).
  Cible : au moins 8/10 pour 90 % des réponses, aucune contradiction mécanique
  critique observée, aucune redescription complète non demandée dans la série
  de questions simples. Publier exemples réussis et ratés, pas seulement une moyenne.

Cette notation est une auto-évaluation explicitée, pas une preuve universelle
de qualité artistique. Un modèle juge éventuel peut aider à trier les réponses,
mais ne remplace ni l'état moteur comme oracle, ni la lecture de Codex. Un score
élevé ne compense jamais une duplication d'objet ou une mutation involontaire.

Commandes existantes à utiliser selon le code modifié :

```powershell
ctest --test-dir build_native -C Release --output-on-failure
python -m unittest discover -s scripts -p "test_*.py"
python -m unittest discover -s tests -p "test_*.py"
python scripts/verify_native_llm.py --exe build/Release/liminal_cornell_renderer.exe --out C:/preuves/nouvelle-recette
```

Compléter les pilotes existants (`qualify_continuity.py`, `--run-session`,
`--sdl-smoke-*`) par le corpus et la capture corrélée nécessaires ; ne pas
présenter une commande future comme déjà disponible. Le bilan final distingue
tests exécutés, observations qualitatives, échecs et points non qualifiés.

## 8. Limites du chantier

Pas de nouvelle carte prédéfinie, de parser naturel C++, de service distant de
jeu, de changement de GGUF, de système multi-modèle, ni de chargement de partie
SDL dans cette roadmap. Conserver les règles de génération, la persistance des
lieux, l'enquête et le mixage existants. Pour le premier périmètre, la conversation
conserve les cibles audio ; une modulation dramaturgique sans mutation du monde
pourra être étudiée ensuite avec son propre contrat et sa recette.

La livraison visée est une expérience où le joueur peut converser librement,
agir avec un vocabulaire souple, constater des conséquences cohérentes, et où
chaque interaction peut être examinée après coup à partir de preuves complètes.
