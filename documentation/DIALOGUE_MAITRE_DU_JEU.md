# Dialogue du maître du jeu

20 novembre 2026. [Roadmap](ROADMAP_DIALOGUE_MAITRE_DU_JEU.md).

Le parcours SDL transmet chaque phrase libre à Gemma pour en déterminer
l'intention. Une question, une hypothèse ou une demande de conseil suit la
branche de conversation. Le modèle répond à partir du lieu, des instruments,
de l'enquête et des échanges récents. Le moteur inscrit la réponse dans
l'historique et conserve la vue, l'inventaire et l'état thermique.

Un déplacement, une prise ou un dépôt passe par les validations du moteur.
Une vraie ambiguïté peut appeler une question sur les objets candidats. Les
identifiants et la demande précédente accompagnent la relance immédiate.
L'observation explicite peut enrichir les connaissances et conserve son
comportement d'ambiance. Le premier contact avec une paroi reste une découverte
visible. Les tentatives suivantes peuvent réutiliser la vue. Les raccourcis
exacts d'inventaire et de boussole restent disponibles.

## Mémoire et autorité

Chaque réponse de conversation utilise un champ `reply`. Le contrat ne
contient aucune opération d'état. Le fil récent contient les questions,
les réponses affichées, le résultat moteur et les références validées. Le
sujet proposé par l'interprète détermine les faits utiles au maître du jeu.
Une question sur l'heure reçoit le statut de l'horloge. Une relance peut
recevoir aussi les échanges précédents.
L'interprète et le maître du jeu comptent leurs prompts avec le tokenizer et
le template Gemma. La sélection examine les douze derniers échanges, conserve
au plus six événements sur deux lieux et respecte le plafond configuré de
mémoire ainsi que la réserve de sortie. Les échanges écartés restent dans
l'historique et les archives. La récupération d'un sujet plus ancien que
cette sélection reste à qualifier.

L'architecture d'un lieu accepté reste persistante. La conversation ne demande
ni composition ni raytracing dans SDL. Les tests headless qui exportent une
image peuvent préparer la scène pour leur export.

## Archives SDL

Sous `interactions/00000/`, l'archive conserve le premier écran jouable et
son état. Les dossiers suivants correspondent aux commandes soumises. Chacun
contient les états avant et après, les références et l'empreinte de la scène,
le mix audio, la capture `screen.png` et le texte associé. Les références
pointent vers les fichiers de scène de la même archive.

`calls/` conserve les requêtes formatées et leurs paramètres, les résultats
et les octets bruts disponibles. Le résultat de validation et la sortie
affichée figurent dans le tour et la présentation. La capture SDL contient
la visière, la boussole, les températures et le panneau de dialogue. Elle
est lue avant la présentation puis conservée une fois la présentation
confirmée. Les raccourcis sans inférence ont un compteur d'appels nul.

`completion.json` indique la disponibilité de ces preuves. Une fermeture
pendant un appel peut laisser une tentative et ses données partielles sans
capture. Les anciens fichiers par lieu et par tour servent toujours à la
comparaison. Une nouvelle partie repart du seuil.

## Recette

Le corpus relu contient 30 questions françaises, cinq relances et deux
questions dans chacune des cinq autres langues. Le pilote peut aussi jouer
un dépôt, un retour au seuil et une reprise par pronom.

```powershell
python scripts/qualify_dialogue.py --suite questions --out C:/preuves/dialogue-questions
python scripts/qualify_dialogue.py --suite journey --seed 4243 --out C:/preuves/dialogue-parcours
python scripts/qualify_dialogue.py --suite followups --seed 4244 --out C:/preuves/dialogue-relances
python scripts/qualify_dialogue.py --suite objects --seed 4244 --out C:/preuves/dialogue-objets
python scripts/qualify_dialogue.py --suite languages --out C:/preuves/dialogue-langues
python scripts/qualify_dialogue.py --suite shortcuts --out C:/preuves/dialogue-raccourcis
```

Le pilote lit les archives et contrôle l'état de simulation, le rendu,
le rôle et le budget des appels et les fichiers de preuve. La lecture des
réponses et des captures complète ces contrôles mécaniques.

### Référence avant changement

Le binaire précédent a joué les 30 questions françaises au seuil avec Gemma
et la graine 4242. Il a classé 25 entrées comme observations, deux comme
clarifications, deux comme refus d'objet et une comme consultation de
l'enquête. Le compteur de simulation est passé de 1 à 31, et BODY de 37,0 à
37,1 °C. Aucun déplacement n'a eu lieu. Hors premier chargement, le temps cumulé
des appels LLM par interaction avait une médiane de 13,55 s et un p95 de 18,93 s
sur cette série headless. La recette SDL mesure aussi la présentation et
l'archivage. Les deux mesures couvrent donc des étapes différentes.

Un essai SDL précédent a aussi montré le chemin d'échec à froid. Le contrôle
du fichier du modèle a dépassé 30 s, la question sur la survie a reçu un
refus d'objet et le jeu a remplacé la vue. Le comptage qui prépare les
nouveaux prompts utilise le délai général pour ce chargement initial.

### Parcours d'objets et retour

Le parcours SDL à la graine 4243 a enregistré sept commandes et le premier
écran jouable. Inventaire et ouest fermé ont chacun réutilisé la vue sans
inférence. Le dépôt a modifié la couche d'objet. La question suivante a
conservé cette vue, puis le passage à l'est a généré un lieu. De retour au
seuil, la craie était toujours à sa position précédente. « Je reprends ce
que je viens de poser » a transféré le même identifiant dans l'inventaire.
Les références et empreintes de scène des huit captures ont été vérifiées.

Deux fixtures Gemma headless à la graine 4244 ont qualifié le vocabulaire des
objets. « Je ramasse le couteau » et « Je le repose » ont transféré la même
dague. Avec deux dagues au sol, le jeu a demandé laquelle et conservé les deux
identifiants dans l'historique. « Celui au manche sombre » a transféré
uniquement la dague demandée. Ces fixtures n'ajoutent aucun objet au jeu normal.
Une requalification a retrouvé ce résultat après correction d'une proposition
que le modèle avait étiquetée comme conversation tout en donnant les deux
candidats. Une telle proposition ne déclenche aucun transfert. Le moteur
présente les candidats validés et attend la précision.

### Raccourcis et interruptions

Une recette SDL dédiée a enchaîné `N`, `S`, `Nord`, `Sud`, `O`, `Ouest`,
`i`, `inventaire` et `inventory`. Les quatre premières entrées ont joué deux
allers-retours entre les mêmes lieux. Le premier `N` a généré le seul nouveau
lieu. `O` et `Ouest` ont constaté le même passage fermé. Aucune entrée n'a
utilisé l'interprète. Les blocages et les inventaires ont réutilisé la vue
sans inférence. Les tests du moteur couvrent également `E` et `Est`, ainsi
que `i` et le libellé local de l'inventaire dans les six langues.

La fermeture SDL pendant une demande a produit une entrée `not_committed`
avec états avant et après identiques. Le dossier signale l'absence de capture
du résultat. Un test injecte aussi un échec d'écriture d'une requête et
vérifie que la preuve reste marquée incomplète.

### Questions et relances

Avec Gemma et la graine 4242, les 30 questions ont suivi la branche de
conversation. Chaque interaction a conservé le même état de simulation,
la même vue SDL et les mêmes cibles audio. Deux appels sont archivés par
question. Les 31 dossiers, écran initial compris, contiennent une capture
complète et des références de scène dont les empreintes ont été vérifiées.
La médiane d'envoi à affichage est de 4,64 s et le p95 de 5,24 s une fois le
modèle chargé. La première réponse a pris 22,17 s. Le traitement de preuve
après présentation a pris 0,20 s en médiane.

Les cinq relances à la graine 4244 ont passé les mêmes contrôles mécaniques.
« Et pourquoi ? » reprend le seuil et la dalle évoqués juste avant. Une
demande de précision revient cependant à la description du seuil. Une série
SDL antérieure au dernier ajustement du contexte a vérifié deux questions
dans chacune des cinq autres langues.

L'auto-évaluation du corpus français donne une moyenne de 7,8/10 et 19
réponses sur 30 à 8/10 ou plus. La cible de 90 % n'est pas atteinte. La réponse
sur l'heure reste centrée sur l'absence d'horloge. Les réponses sur le passage
nord s'appuient sur l'indication de la boussole. Le ton et la pertinence
restent inégaux. Une question sur la boussole reçoit une suggestion de
consulter un manuel ou un équipier, et une réponse mentionne une piste de
Vey vers le nord sans preuve. Ces phrases n'ont ajouté aucun fait au registre
du moteur. La connaissance de l'intrigue et le contrôle de ces extrapolations
restent à améliorer.

Les résultats, captures et notes par question sont conservés hors dépôt sous
`C:/works/duo-runs/game-liminal-raytraced-llm-world/`, notamment dans
`MJ-00-baseline`, `MJ-05-questions-05`, `MJ-05-followups-05`,
`MJ-05-journey-04`, `MJ-05-objects-04`, `MJ-05-shortcuts-01` et
`MJ-05-cancellation`. Les 15 CTest et 114 tests Python passent.
