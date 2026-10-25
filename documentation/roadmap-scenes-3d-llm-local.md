# Roadmap — Scènes 3D générées par un LLM local

Projet : **Within the Latent Walls / Entre les Murs Latents**  
Branche examinée : `ministral-reasoning`, commit `263d3a6`  
Date : **25 octobre 2026**  
Statut au 25 octobre : **première tranche intégrée, jalons 1–6 exécutés ; qualification artistique partielle**. Le corps ci-dessous conserve le plan initial. Voir le [bilan mesuré et les images](LOCAL_SCENE_ROADMAP_RESULTS.md).

**Décision utilisateur ultérieure, appliquée :** un seul lieu de départ prédéfini, nouvelles découvertes générées pendant la partie, nouvelle exploration à chaque lancement. Les scènes restent stables pendant une partie et sont archivées pour la recherche, sans reprise par le jeu suivant. Cette décision remplace la reprise entre lancements prévue au jalon 5 et l'élargissement aux cinq branches fixes. Voir [le fonctionnement actuel](SINGLE_START_RESEARCH.md).

| Jalon | Résultat observé |
|---|---|
| 1. Comparer | Cinq briefs × deux graines : Gemma 8/10, Ministral 8B Q4 4/10 acceptés techniquement, images archivées. |
| 2. Composer | Cadre partagé Python/C++, 8–20 boîtes libres ; diversité visible, supports et passerelles encore inégaux. |
| 3. Corriger | Masquage détecté et une correction réelle réussie sur fixture contrôlée ; échecs antérieurs conservés. |
| 4. Inférer | Ollama/Gemma retenu ; résidence observée entre appels, annulation et délai testés. |
| 5. Jouer | Branche `extraction_field → east`, publication atomique, sauvegarde contrat 2, revisite et rechargement sans recomposition. |
| 6. Recette | 9/10 découvertes natives ; médiane 4,324 s et p95 4,835 s à 8 spp. SDL testé en succès et annulation, 4 CTest et 72 tests Python réussis. |

Les seuils techniques de la branche pilote sont atteints. La qualité artistique n'est pas certifiée par ces scores ; l'élargissement aux autres branches reste à réaliser après correction des limites visibles.

## Objectif

Obtenir rapidement une tranche jouable : découvrir un lieu, faire générer sa géométrie par un LLM local, l’afficher, puis le retrouver intact à la visite suivante.

Le premier chemin de validation repose sur **Python + Ollama + le raytracer existant**. La solution retenue sera ensuite intégrée dans le jeu. Chaque étape doit produire un résultat observable avant de passer à la suivante.

## Constats de départ

- Dans le jeu actuel, les nouveaux lieux sont principalement composés par le compilateur hybride C++, à partir de métadonnées proposées par le LLM.
- La génération directe de programmes `.scene` existe dans le banc d’essai, mais ne constitue pas le chemin normal de création des voisins inconnus.
- `GenerateChatCompletion()` charge les poids et crée un contexte à chaque appel, puis les libère à la sortie de la fonction. Ce comportement appartient à l’intégration actuelle ; llama.cpp permet de conserver le modèle chargé.
- Un programme syntaxiquement valide peut donner une vue inutilisable. La fixture Q4 `cooling_exchange_bay` place notamment un bloc devant la caméra qui masque l’ensemble de l’image.
- Le gain de vitesse de Gemma observé dans un autre projet avec Python/Ollama reste à reproduire sur cette tâche, dans des conditions comparables.

## 1. Départager les modèles dans les mêmes conditions

Réutiliser le script Python/Ollama pour tester le Gemma déjà utilisé et Ministral sur **cinq briefs Eryx courts, avec deux essais par brief**.

### Conditions communes

- Même matériel et même moteur d’inférence pour cette première comparaison.
- Consigner les noms exacts, versions, quantifications et paramètres de réflexion.
- Précharger chaque modèle avant les essais chronométrés ; mesurer séparément le démarrage à froid.
- Utiliser les mêmes consignes, budgets de sortie et paramètres de rendu.
- Exécuter les essais séquentiellement et éviter la résidence simultanée de modèles qui saturerait la VRAM.

### Mesures

- Temps de chargement, de traitement du prompt, de génération et de rendu.
- Nombre de tokens produits et débit de génération.
- Taux de scènes utilisables au premier essai.
- Taux de réussite après correction et temps total pour obtenir une scène utilisable.
- Lecture humaine de la composition et de la fidélité au brief.

**Livrable :** une planche de rendus et un tableau comparatif. Choisir le modèle selon le résultat obtenu dans le temps disponible. Cette petite campagne sert à décider du prochain lot, pas à établir un classement général des modèles.

## 2. Obtenir une composition libre dans un cadre simple

Adapter le banc existant pour demander directement un programme `.scene`, sans passer par une recette d’archétype.

### Contrat initial

- Caméra, sol et éclairage fournis et fixes pour les premiers essais.
- Environ **8 à 20 volumes** composés par le modèle : positions, dimensions, rotations et assemblages.
- Un brief décrit une relation spatiale forte : petite entrée sous une masse immense, traversée au-dessus d’une excavation, repère lointain entre deux parois, etc.
- Le prompt précise les axes, les unités, le centre des boîtes, leurs dimensions complètes et la syntaxe autorisée.
- Quelques exemples courts enseignent des assemblages sans fournir de niveaux complets à recopier.
- La réponse finale contient seulement le programme ; une sortie tronquée est un échec.

Le modèle reste l’auteur des formes et de leurs relations. Les paramètres fixes permettent d’évaluer sa composition sans cumuler immédiatement les difficultés de cadrage et d’éclairage.

**Livrable :** plusieurs scènes reconnaissables et différentes entre elles. Commencer par une génération directe ; ajouter une passe de plan uniquement si les résultats montrent son utilité.

## 3. Fermer la boucle de correction

Compléter l’audit existant par les contrôles qui éliminent les défauts les plus visibles.

### Contrôles prioritaires

- Programme complet, syntaxe valide et valeurs numériques finies.
- Dimensions positives et complexité bornée, y compris après expansion des prefabs.
- Caméra hors des solides.
- Détection des obstacles proches qui masquent presque toute la vue.
- Visibilité du repère principal lorsque le brief en demande un.

Les contrôles doivent tenir compte des rotations et de la géométrie réelle des objets concernés. Une enveloppe globale de prefab peut produire des faux positifs autour d’une ouverture. Les diagnostics de masquage doivent être contextualisés : un gros premier plan volontaire n’est pas automatiquement un défaut.

### Retour au modèle

Fournir un diagnostic mesurable et ciblé, par exemple :

> `cooling_block_4` masque toute la vue à proximité immédiate de la caméra. Déplace ou redimensionne ce bloc en conservant la composition générale.

Autoriser **une correction complète**, avec un délai global maximal partagé entre génération et correction. Après épuisement du budget, conserver explicitement l’échec dans les résultats.

**Livrable :** une scène défectueuse détectée puis corrigée automatiquement. Un audit technique réussi reste distinct du jugement artistique sur l’image.

**Premier jalon décisif :** un script court qui transforme un brief en décor visible et sait réparer un mauvais placement.

## 4. Choisir une seule solution d’inférence pour la première version

Les mesures précédentes déterminent l’intégration.

| Solution retenue | Travail nécessaire |
|---|---|
| Ollama | Ajouter un client local dans le jeu et maintenir le modèle chargé entre les appels. |
| llama.cpp intégré | Sortir le chargement des poids de `GenerateChatCompletion()` et conserver le modèle pendant la session. |

Dans les deux cas, prévoir une annulation, un délai maximal et des métriques par appel. La durée de vie du modèle doit être distincte de celle du contexte et de l’historique de conversation. Réinitialiser ou recréer le contexte lorsque la tâche exige une génération indépendante.

Dimensionner l’usage de la VRAM pour éviter de maintenir inutilement plusieurs modèles simultanément. La première tranche doit utiliser un seul backend retenu ; elle n’exige pas d’implémenter les deux solutions.

**Livrable :** deux générations successives sans rechargement des poids.

## 5. Brancher une seule sortie du jeu

Choisir une des cinq branches d’exploration existantes.

### Première découverte

1. Préparer le brief et les contraintes de navigation appartenant au moteur.
2. Générer, contrôler et éventuellement corriger la scène.
3. Publier ensemble le lieu, sa géométrie et son lien de retour après acceptation.
4. Sauvegarder le programme accepté.

### Revisite et persistance

Recharger le programme accepté sans relancer la composition. Modifier `RefreshGeneratedRoomCache()` pour préserver cette géométrie au lieu de la reconstruire depuis les métadonnées.

Commencer avec une narration d’arrivée simple, liée aux éléments effectivement acceptés. Les sorties, barrières invisibles et déplacements restent sous l’autorité du moteur.

En cas d’échec ou d’annulation, conserver le lieu courant et l’état validé : aucun lien fantôme, aucun déplacement facturé et aucune arrivée racontée avant publication.

**Livrable :** partir, découvrir, revenir, sauvegarder, relancer et retrouver le même décor.

## 6. Passer une petite recette avant d’élargir

Les seuils suivants sont des **objectifs proposés**, pas des performances observées.

| Critère | Objectif initial |
|---|---|
| Génération | Au moins 8 scènes acceptables sur 10, correction comprise. |
| Diversité | Plusieurs compositions clairement distinctes à la lecture des images. |
| Intégrité | Aucun déplacement ni lien publié après échec. |
| Persistance | Revisite et rechargement sans nouvelle composition. |
| Réactivité | Annulation fonctionnelle et attente totale mesurée. |
| Latence | Cible : médiane sous 5 s et p95 sous 15 s pour une scène acceptée, correction éventuelle et premier rendu compris ; à confirmer sur la machine cible. |

Sur dix essais, le p95 empirique correspond pratiquement au maximum observé ; il ne constitue pas une estimation robuste. Si les cibles de latence ne sont pas atteintes, publier les valeurs réelles et décider du compromis avant de multiplier les fonctionnalités.

Ajouter quelques contrôles ciblés : scène masquée connue, caméra dans un solide, réponse tronquée, annulation, échec de génération, sauvegarde/rechargement et conservation des lieux canoniques.

**Livrable :** une courte session jouable démontrant la chaîne complète et ses limites mesurées.

## Ordre d’élargissement

Après réussite de la première branche :

1. Étendre la génération aux cinq branches ouvertes.
2. Libérer progressivement la caméra, puis l’éclairage, en conservant les contrôles associés.
3. Relier davantage d’objets interactifs à la géométrie acceptée.
4. Enrichir la continuité entre lieux et les mécanismes topologiques.
5. Évaluer une passe de plan, un retour visuel multimodal ou un entraînement spécifique seulement face à une limite précisément observée.

## Points d’entrée dans le dépôt

| Fichier | Rôle dans la roadmap |
|---|---|
| `scripts/run_scene_generation_benchmark.py` | Base du banc direct et du rapport de résultats. |
| `src/turn_contract.cpp` | Contrat de génération et référence compacte du langage. |
| `src/scene.cpp` | Parsing, primitives et informations nécessaires aux contrôles. |
| `src/scene_compiler.cpp` | Audit actuel et séparation des chemins de compilation. |
| `src/llm_runtime.cpp` | Durée de vie du modèle, appels et métriques. |
| `src/turn_runner.cpp` | Création, publication et conservation des lieux générés. |

## Références

- [Branche examinée](https://github.com/astrofra/game-liminal-raytraced-llm-world/tree/ministral-reasoning)
- [Plan de génération dynamique existant](https://github.com/astrofra/game-liminal-raytraced-llm-world/blob/ministral-reasoning/documentation/GENERATION_DYNAMIQUE_DECORS_PLAN.md)
- [État technique](https://github.com/astrofra/game-liminal-raytraced-llm-world/blob/ministral-reasoning/documentation/TECHNICAL_STATE.md)
- [Qualification Ministral](https://github.com/astrofra/game-liminal-raytraced-llm-world/blob/ministral-reasoning/documentation/MINISTRAL_QUALIFICATION_REPORT.md)
- [API Ollama : maintien en mémoire et métriques](https://docs.ollama.com/api/generate)

Cette roadmap privilégie une preuve fonctionnelle courte avant l'extension du système. Les mesures et modifications réalisées depuis sa rédaction sont décrites dans le [bilan d'application](LOCAL_SCENE_ROADMAP_RESULTS.md).
