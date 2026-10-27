# Enquête à indices — pilote du 27 octobre 2026

Le mandat initial est affiché dans les six langues : suivre la piste du
prospecteur Vey, vérifier des obstructions signalées et prélever un spécimen
cristallin. Vey appartient à la fiction du projet. Son sort reste indéterminé.
Les lieux continuent d'être composés librement ; leurs liens et leur géométrie
acceptés restent stables.

## Boucle jouable

Le LLM choisit zéro, une ou plusieurs possibilités dans chaque nouveau lieu,
parmi quatre identités uniques et des emplacements offerts par le moteur.
L'appel narratif de première arrivée effectue ce choix après la géométrie ;
aucun appel de planification n'est ajouté. Il n'y a ni ordre imposé par la
graine, ni délai garanti d'apparition. Une décision vide est persistante ;
les revisites et réparations de texte ne distribuent rien.
Voir le [bilan de répartition](REPARTITION_INDICES_LLM.md).

| Possibilité | Geste requis | Ce qui est établi |
|---|---|---|
| Spécimen cristallin | Examiner, puis prendre l'objet identifié | Correspondance avec la référence de l'expédition ; acquisition distincte et durable |
| Obstruction invisible | Tenter sa direction, puis examiner l'approche | Contact sans déplacement ; absence de masse opaque expliquant ce contact |
| Outil de prélèvement identifié | Examiner sa plaquette | Correspondance avec le matériel de Vey et résidu cristallin |
| Plaquette de relevé | Examiner le relevé | Marque authentifiée de Vey, prélèvement consigné puis départ du point de prélèvement |

Le cristal et l’outil seuls ne soutiennent pas l’idée que Vey serait resté au
point de prélèvement, même lorsqu’ils se trouvent ensemble. Cette hypothèse
reste ouverte jusqu’au relevé qui la contredit. Les deux traces soutiennent l'hypothèse de la prospection de Vey. Le relevé
contredit l'hypothèse qu'il serait resté au point de prélèvement. Les constats
restent conservés lorsqu'une interprétation est contredite. Aucune de ces traces
ne prouve une mort, une rencontre ou une origine extraterrestre.

Les formulations libres passent par Gemma. `examine_clue` référence une
possibilité accessible et une révision ; `review_investigation` consulte le
dossier. Une demande de comparaison produit ce même bilan fidèle : le pilote
n'ajoute pas un raisonneur général ni des hypothèses arbitraires rédigées par
le modèle. La prise et le dépôt utilisent les transferts d'objets existants.
Les raccourcis exacts de boussole et d'inventaire restent directs.

Le carnet présente « établi », « supposé » et « à vérifier ». Les preuves citent
leur lieu, avec un numéro stable pour distinguer les titres identiques ; le
spécimen acquis indique sa localisation actuelle. La console défile à la molette
ou avec Page précédente/Page suivante ; un bilan s'ouvre à son début. Les notes
personnelles restent explicitement séparées. Le mandat n'est pas récité dans
chaque réponse. Aucun nouvel écran, score, appel de planification ou appel de
résumé n'est ajouté.

## Autorité et publication

`SessionState::investigation`, schéma 2, politique `eryx-clues-llm-v2`, conserve
opportunités, preuves, trois fils, deux hypothèses et les décisions par lieu.
Une décision porte son origine (`llm` ou `legacy_preserved`), les identités
sélectionnées et le tour de publication. Elle reste immuable.
Le fil `obstruction` résout la question de sa vérification après contact et
examen ; sa nature reste inconnue. `vey` et `crystal` restent en cours après
leurs premières preuves. La possession actuelle provient exclusivement du
registre d'objets ; la preuve d'acquisition n'est jamais supprimée par un dépôt.

Le moteur compose et audite la géométrie, offre des sites compatibles, puis
valide la proposition `investigation_placement` du narrateur. Les objets sont
préparés avant la vérification de leur présence dans la prose et les identités
déclarées par le modèle. Les petits marqueurs utilisent la
couche mobile, avec placement visible, sol et emprise contrôlés. Le spécimen
est portable ; les deux traces de Vey servent à l'examen. Aucun mot « cristal »
ou « Vey » dans un texte ordinaire ne suffit à créer une preuve.

Après le retour joueur du 27 octobre, une tentative de prise d'un objet non
transportable affiche explicitement : « Cet objet ne peut être ni emporté ni
déplacé. Il reste sur place. » Cette restriction est conservée ; le message
ne présente plus le refus comme une cible introuvable. Voir [DUO-046](duo/reviews/DUO-046.md).

Une barrière ne peut utiliser qu'une approche géométriquement dégagée, sans
lien publié. Elle protège le retour et conserve une autre continuation dans
le graphe exploré. Elle est enregistrée avant tout contact. Les soutiens de
verre et les sorties simplement fermées ne constituent pas des preuves.
Après un premier contact, la [paroi repérée](PAROIS_REPEREES.md) devient visible
par transparence et la direction est barrée en ambre sur la boussole. Ce repère
aide à relier le refus au lieu ; il n'établit ni une matière ni une origine
pour l'obstruction et ne révèle rien avant le contact.
Une sélection absente, incomplète ou incompatible rejette le lieu et le
déplacement entiers, sans consommer les possibilités. Une sélection valide
accompagnée d'une prose ou de températures invalides peut être publiée avec
un texte de secours qui conserve exactement ces choix.

Les données sont préparées sur une copie. La publication dépend du tour
complet, des invariants des objets et de l'enquête et de la dernière sonde
d'annulation. Une ancienne révision ne peut pas être publiée. Un rejet de
scène ou une annulation ne consomme pas la possibilité. Une réponse de
consultation peut porter une référence de focus valide sans créer de preuve.

Les faits connus et les questions restent dans la partie obligatoire des
contextes, indépendamment des six événements récents. Les conclusions cachées
des traces non examinées sont absentes de ce contexte. Le générateur géométrique ne reçoit aucun indice réservé. Le narrateur de
création reçoit un bloc d’écriture distinct : possibilités publiques, sites,
décisions antérieures et rythme des lieux sans indice. Les contenus cachés
des traces ne sont pas communiqués à l’arrivée.
Le narrateur reçoit les effets validés, les indices accessibles et les
incertitudes. Le budget existant réduit d'abord la mémoire récente.

## Compatibilité et archives

Un état ancien sans `investigation` démarre avec trois fils ouverts et aucune
preuve. Un état v1 conserve ses opportunités et preuves ; les hypothèses sont
recalculées selon leurs justifications admissibles. Les lieux existants et le
seuil reçoivent une décision `legacy_preserved`, sans redistribution. Objets,
liens et barrières sont conservés. Seuls de futurs lieux utilisent le nouveau
contrat. Une mention littéraire liée à un objet déplacé conserve son identité
et ne peut pas recréer cet objet dans son ancien lieu. Les exports restent diagnostiques : aucune reprise de
sauvegarde dans SDL n'est ajoutée.

`unresolved_threats` et `soft_state.active_hypotheses` sont des projections de
compatibilité. `threats_add/remove` ne peuvent plus modifier l'enquête.
La chaîne historique du prélèvement disparaît après acquisition, même si la
question plus large du rôle des cristaux reste ouverte.

Les archives de tours contiennent l'enquête avant et candidate, le statut de
publication, la demande d’écriture, la sélection proposée, sa préparation
ou son rejet et la décision persistante. Une proposition rejetée n’est pas une
découverte. Les identités et révisions figurent dans `state.json`.

## Qualification du premier pilote (avant répartition LLM)

Les résultats ci-dessous concernent la politique v1. La qualification de la
version actuelle figure dans [REPARTITION_INDICES_LLM.md](REPARTITION_INDICES_LLM.md).

Le binaire compilé est publié dans `build/Release/liminal_cornell_renderer.exe` ;
son empreinte correspond à celle de `build_native` et à `runtime-build.json`.
Le lancement habituel ouvre ce pilote. Aucun commit ni push n'a été effectué.

Le [lot DeepSeek DUO-045](duo/reviews/DUO-045.md) fournit six groupes de tests,
relus et corrigés par Codex. Deux parcours transversaux supplémentaires couvrent
deux ordres de découverte dans la borne de six nouveaux lieux compatibles,
contact répété, dépôt ailleurs et reprise tardive, références périmées,
rejet de scène, annulation tardive, faux identifiants, roundtrip et six langues.
Les anciennes fixtures de géométrie désactivent explicitement la distribution
d'indices pour conserver leurs oracles architecturaux ; les nouveaux parcours
la gardent active.

Résultats observés :

- **15/15 CTest et 114 tests Python.** Les tests supplémentaires de Codex
  vérifient aussi la dernière continuation, les versions numériques invalides
  et une collision d'identité lors de la migration.
- **Gemma réel, français :** quatre lieux générés, six faits conservés,
  contact puis examen de l'obstruction, deux traces de Vey, dépôt du spécimen
  dans un autre lieu puis retour et reprise. Les bilans conservent les
  incertitudes, les deux statuts d'hypothèses et un score inchangé. Les essais
  ont permis de corriger les références objet/indice redondantes et de préciser
  les directions de transfert. Un secours narratif a été observé lors d'une
  revisite, sans altération des faits ni de la navigation.
- **SDL réel, français :** nouvelle partie et six commandes acceptées
  (déplacement, examen, prise par pronom, dépôt, reprise reformulée, bilan).
  Mandat, spécimen, retrait visuel, localisation et défilement du carnet
  vérifiés sur les captures de la fenêtre. Un échantillon seul ne crée pas
  d'hypothèse sur Vey.
- **Archive du joueur :** migration de l'export `run-20261027T072857Z-1411155189`
  vers une sortie séparée ; lieux et objets conservés à l'identique,
  aucune preuve rétroactive. L'original n'a pas été modifié.

Preuves privées sous
`C:/works/duo-runs/game-liminal-raytraced-llm-world/ERYX-01-20261027/`.
Les six langues sont couvertes par les contrôles automatisés ; la recette
Gemma/SDL ci-dessus porte sur le français. Les tests injectés prouvent le
contrat moteur, pas la compréhension systématique du modèle ni la qualité
artistique de toutes les compositions.
