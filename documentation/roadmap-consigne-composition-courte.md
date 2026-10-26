# Roadmap — Une courte consigne de composition

Projet : **Within the Latent Walls / Entre les Murs Latents**  
Date : **26 octobre 2026**  
Référence : contrat local examiné au commit `5baab66` de `main`  
Statut : **proposition ; aucun essai de cette consigne n’a encore été effectué**.

## 1. Objectif et décision

Tester directement l’ajout d’une **courte direction artistique textuelle** pour encourager des scènes plus lisibles, plus économes et moins dispersées, tout en conservant la liberté d’improvisation du LLM.

**Un seul facteur change : la consigne de composition.** Pour ce pilote, conserver les budgets actuels, notamment les **8 à 20 boîtes**, ainsi que le modèle, le contexte historique, la caméra, l’éclairage et les passes procédurales.

Le modèle continue de choisir l’identité architecturale du prochain lieu et d’écrire le `.scene` dans le même appel. Aucun catalogue, gabarit de niveau, appel de planification ou mécanisme de sélection artistique n’est ajouté.

## 2. Principe artistique

La simplicité recherchée concerne les **relations entre les éléments** : hiérarchie des masses, place du vide, cohérence des proportions et des directions, rôle des éléments secondaires.

Elle ne correspond pas à un nombre imposé de gestes, à une symétrie obligatoire ou à une réduction automatique des primitives. Plusieurs boîtes peuvent former un ensemble simple ; une répétition peut établir un rythme. Une rotation singulière peut rester intéressante si elle appartient à la composition.

Les six références visuelles servent à la revue humaine. Leurs structures ne sont pas décrites au modèle comme des recettes à reproduire. Le pilote ne demande pas non plus leur reproduction photographique.

## 3. Consigne proposée

Ajouter ce paragraphe anglais au prompt interne, cohérent avec la langue actuelle des instructions géométriques :

```text
Compose with economy and a clear hierarchy of masses and empty space. Let related volumes share proportions or directions when this strengthens the whole. Use secondary elements to support the composition rather than add unrelated gestures. Repetition may establish rhythm. Preserve room for silence and visual breathing. Choose the architecture freely from the supplied continuity.
```

Sens en français :

> Compose avec économie et une hiérarchie claire des masses et des espaces vides. Des volumes apparentés peuvent partager des proportions ou des directions lorsque cela renforce l’ensemble. Les éléments secondaires servent la composition plutôt que de multiplier les gestes indépendants. La répétition peut établir un rythme. Préserve des espaces de silence et de respiration visuelle. Choisis librement l’architecture à partir de la continuité fournie.

Ce paragraphe exprime des préférences. Il ne devient ni un schéma de sortie, ni une liste de critères géométriques obligatoires. Les contraintes du moteur et les faits établis restent prioritaires.

## 4. Lot COMP-01 — Ajouter le texte au chemin réellement utilisé

### Modification minimale

Dans `assets/scene_generation/local_v1.json`, ajouter le paragraphe à **`exploration_instruction`**, à la suite de la consigne ouverte existante. Conserver cette dernière, notamment l’improvisation depuis l’historique et la sortie directe en `.scene`.

Ce point d’injection permet de réutiliser le chemin existant :

`LocalSceneExplorationBrief()` → `BuildLocalSceneProgramPrompt()` → génération locale.

Le nom historique de la fonction ne doit pas conduire à réintroduire une sélection de briefs. Le contenu reste une instruction commune, ouverte, identique pour toutes les découvertes.

### Vérifications nécessaires

- Vérifier dans les messages effectivement envoyés que le paragraphe apparaît une seule fois par requête géométrique.
- Le conserver dans les corrections et recompositions, à travers le même constructeur de prompt.
- Garder intact le bloc de continuité, avec son comptage en tokens et sa politique de réduction.
- Préserver le format exclusif `.scene` : aucune justification artistique ou réponse intermédiaire demandée.
- Versionner le contrat et vérifier que l’empreinte de prompt existante reflète le nouveau contenu.
- Régénérer l’en-tête embarqué et reconstruire le binaire selon le mécanisme existant ; une modification du JSON doit atteindre l’exécutable réellement lancé.

**Livrable :** une découverte et son prompt archivé démontrent l’utilisation réelle de la nouvelle consigne.

## 5. Lot COMP-02 — Comparaison rapide sur un contexte identique

Conserver une référence du prompt actuel, puis comparer :

| Variante | Contenu |
|---|---|
| A | Consigne ouverte et continuité actuelles. |
| B | Même contenu, avec le court paragraphe de composition. |

Premier essai proposé : **trois graines, deux variantes**, soit six générations à partir d’un même état de départ validé. Si possible, choisir cet état dans une partie existante dont la composition paraît trop dispersée.

Pour chaque paire, conserver le même historique, la même graine, le même modèle et les mêmes paramètres d’échantillonnage et de rendu. Garder aussi les mêmes règles de correction, de nettoyage coplanaire et d’ajout du verre selon ce qui est effectivement intégré au moment de l’essai.

Examiner les premières propositions et les versions finales lorsque des corrections procédurales sont intervenues. Archiver les échecs et les reprises pour éviter de ne montrer que les meilleurs résultats.

### Revue visuelle

- La relation principale entre les masses et les vides se lit-elle plus facilement ?
- Les éléments secondaires appartiennent-ils davantage à l’ensemble ?
- Les répétitions et orientations produisent-elles une cohérence perceptible ?
- Les variantes gardent-elles des identités différentes et des surprises intéressantes ?
- Le gain éventuel s’accompagne-t-il d’une uniformisation des silhouettes ?

Montrer d’abord les rendus sans leur narration, puis examiner les textes pour juger la continuité. L’évaluation esthétique reste humaine ; aucun validateur C++ ne rejette une scène parce qu’elle manque supposément de simplicité.

**Livrable :** une planche A/B, quelques observations précises et les prompts associés. Cette petite série permet une décision pratique, pas une conclusion statistique générale.

## 6. Lot COMP-03 — Vérifier dans une exploration réelle

Si les premiers rendus sont intéressants, jouer une courte exploration de plusieurs découvertes, avec un retour dans un lieu connu.

Vérifier que la consigne encourage une cohérence sans enfermer toutes les pièces dans la même organisation. L’historique peut propager des motifs : observer si cette mémoire crée une continuité sensible ou une répétition excessive.

La consigne peut légitimement produire autre chose que les six références. Juger le résultat selon l’expérience recherchée et la diversité que l’auteur souhaite conserver.

Deux parties qui divergent dès la première découverte n’ont ensuite plus le même contexte. Ne pas présenter leurs pièces suivantes comme une comparaison contrôlée du seul paragraphe ajouté ; conserver la distinction entre paires A/B et session exploratoire.

Mesurer le nombre d’appels, les corrections, le taux d’acceptation et les durées. Le paragraphe ajoute des tokens au prompt ; aucun gain de vitesse ni de qualité n’est garanti à l’avance.

## 7. Tests et limites du lot

### Vérifications ciblées

- Le nouveau texte atteint le chemin local normal, ses corrections et sa recomposition.
- L’historique et les contraintes techniques restent présents.
- La sortie reste exclusivement géométrique, sans nouveau champ obligatoire.
- Le nombre normal d’appels LLM est inchangé.
- Les scènes acceptées restent immuables lors des revisites.

Réutiliser les tests de construction des prompts et les audits existants. Les tests mécaniques attestent la transmission du texte ; ils n’attestent pas sa réussite artistique.

### Décision après essai

- **Amélioration convaincante :** conserver le paragraphe et documenter les rendus.
- **Résultat neutre :** le signaler sans multiplier immédiatement les consignes.
- **Uniformisation ou perte de surprises :** alléger le texte ou revenir à la référence.

Le minimum de huit boîtes reste inchangé dans cette expérience. Il pourra être réexaminé séparément si le remplissage demeure un problème ; ce chantier ne fait pas partie du présent lot.

## Définition de terminé

Une courte consigne a été ajoutée au prompt réellement utilisé, comparée à la référence sur quelques cas et examinée en partie. Le pipeline existant reste en place : **improvisation LLM, historique borné, validation et corrections C++, rendu et conservation des lieux**.

La livraison distingue clairement la modification effectuée, les observations visuelles, les mesures et la décision de garder ou retirer la consigne.
