# Correction des faces coplanaires — 26 octobre 2026

Les lots COP-01 à COP-04 de la [roadmap](roadmap-correction-faces-coplanaires.md)
sont implémentés. La passe est activée pour les nouvelles scènes du parcours
local, après les corrections opaques existantes et **avant la détection des
blocs flottants et la construction du verre**. Les lieux déjà acceptés sont
réutilisés sans nouvelle correction.

DeepSeek a livré les quinze tests et les trois fixtures du
[lot DUO-033](duo/reviews/DUO-033.md), via Copilot CLI/Ollama dans un worktree
isolé. Codex a revu les outils et fichiers livrés, implémenté le moteur,
l'intégration et la persistance, puis effectué les tests et la recette visuelle.
Le modèle, les prompts de composition et le nombre d'appels LLM du jeu restent
inchangés. Aucun commit ni push.

## Détection et déplacement

`SceneBoxFaces` fournit les six quadrilatères originaux utilisés aussi pour
trianguler les boîtes. Ordre stable : +Z, −Z, −X, +X, +Y, −Y ; mêmes rotations
Euler X/Y/Z que le moteur. Deux faces d'une même boîte ne sont jamais comparées.
Les boîtes de verre sont exclues. Le sol local est un `plane`, donc immobile ;
les autres ressources fixes se déclarent par `fixed_ids` ou `--fixed ID`.

Le préfiltre d'orientation retient au moins une face avec
`dot(normal, normalize(eye-centre)) > 0.05`, sur l'union des huit vues de
respiration. Ce filtre ne prouve ni la présence dans le champ ni la visibilité.
Après préfiltrage AABB, le test vérifie les normales à **0,1°**, les distances
des huit sommets aux deux plans à **1 mm**, puis l'aire de leur intersection
convexe dans une base du plan, supérieure à **10⁻⁶ m²**. Les calculs de distance
et de découpage sont en double précision, sur les sommets float du moteur.

Les normales opposées sont rapportées comme `opposed_joint`, sans correction
automatique. Seuls les `same_direction_conflict` comptent dans l'objectif.
Les arêtes communes et les simples recouvrements d'AABB ne suffisent pas.

Pour chaque conflit, l'algorithme essaie ±1 cm, puis ±2 cm, puis ±4 cm selon la
normale réelle du plan. À amplitude égale, il essaie le second bloc puis le
premier ; entre les deux signes admissibles, il préfère préserver les contacts.
Le bloc entier est déplacé en réécrivant uniquement son `pos(...)`. Identifiant,
matériau, dimensions et rotations sont conservés ; géométrie et BVH sont rechargés.

Chaque candidat doit réduire strictement le nombre global de conflits ciblés,
sans nouveau conflit, avec une séparation supérieure à 2 mm par défaut sur tous
les sommets. Il doit aussi respecter les limites ±256 m, ne pas aggraver la
pénétration du sol au-delà de 1 mm, laisser les huit positions de caméra hors
des masses et préserver les approches initialement libres. Dans le jeu, le
callback ajoute le contrat local et l'audit autoritatif complet : visibilité
du repère, encombrement proche, retour et identité des approches claires.

Plafonds : **16 translations**, **5 cm de longueur cumulée par bloc**, **256 essais**
par défaut, au plus 512 configurables. Une scène contient au plus 96 boîtes
opaques dans le module, contre 8–20 pour le contrat local. Les rejets n'écrivent
rien dans la scène courante. Les résidus conservent la meilleure scène valide et
le statut `residual_conflicts`, avec les paires et les raisons de rejet.

## Intégration, archives et stabilité

L'ordre effectif est : modèle → normalisation/audits/suppressions/aperçu opaque →
COP → audit et nouvel aperçu si déplacement → graphe des flottants → verre →
aperçu réfracté → faits/narration/publication.

La correction reste provisoire jusqu'à son contrôle final. Si celui-ci échoue
ou dépasse le délai, la scène opaque précédemment acceptée est conservée ; le
rapport indique `opaque_fallback` et les déplacements proposés sont annulés.
L'annulation utilisateur interrompt la génération. Aucun rejet COP ne déclenche
une nouvelle composition LLM. Le secours propre au verre conserve ensuite
l'opaque corrigé accepté.

Une réapplication explicite sur une scène augmentée retire les soutiens générés
avant de considérer une modification opaque. Ils doivent être reconstruits avec
`BuildGlassSupports` avant publication. Les boîtes de verre originales restent
intactes. Le pipeline du jeu applique exactement cet ordre.

Le marqueur `engine-coplanar-v1` est lié au SHA-256 du texte traité. Il empêche
une seconde translation sur une sortie déjà traitée, même résiduelle. Une
modification du texte invalide le marqueur. Une entrée sans conflit n'est pas
réécrite. Le chargement et les revisites utilisent directement la scène finale,
sans exécuter ces passes à chaque frame.

Chaque lieu conserve les étapes suivantes :

| Artefact | Contenu |
|---|---|
| Tentatives de composition existantes | Programme brut, normalisé et opaque accepté avant COP |
| `.pre-coplanar.scene` | Opaque accepté à l'entrée de COP |
| `.opaque.scene` | Opaque définitif, après COP et son contrôle |
| `.scene` | Scène finale, avec les soutiens effectivement retenus |
| `.coplanar.json` | Version, caméras, paramètres, paires, essais, rejets, déplacements, positions, cumul, résidus, durées et empreintes |
| `.supports.json` | Décisions et empreintes du verre, basé sur l'opaque corrigé |

Ces champs sont également sérialisés dans l'état. La lecture vérifie la chaîne
d'empreintes avant COP → opaque corrigé → scène finale. Les faits narratifs sont
extraits de la géométrie corrigée et des soutiens présents. Le programme brut
reste une provenance ; le contexte de continuité utilise le lieu final conservé.

## Recette observée

Trois scènes synthétiques de matériaux distincts et une carrière déjà archivée
ont été rendues. Chaque comparaison couvre les **huit vues**, à caméra,
éclairage, graine 1337 et réglages identiques : 640 × 320, 32 échantillons,
3 rebonds diffus, 9 diélectriques, 8 threads. La respiration utilise les valeurs
du jeu : translation verticale 0,025 m, avant 0,012 m, pitch 0,35°, roll 0,15°.

| Scène | Conflits avant → après | Essais / déplacements | Tests polygonaux précis | Détection cumulée / passe |
|---|---:|---:|---:|---:|
| Frontale | 1 → 0 | 2 / 1 | 1 | 0,024 / 0,308 ms |
| Oblique Y 35° | 1 → 0 | 2 / 1 | 1 | 0,035 / 0,278 ms |
| Trois blocs | 3 → 0 | 4 / 2 | 6 | 0,118 / 0,622 ms |
| Carrière archivée | 1 → 0 | 2 / 1 | 4 | 0,104 / 0,577 ms |

Mesures ponctuelles du CLI, hors audits et aperçus propres au runtime ; les
compteurs incluent les scans des candidats. Elles ne constituent pas un
benchmark statistique. Chaque déplacement accepté mesure 1 cm dans ces cas.

Les diagonales concurrentes de l'oblique et des trois blocs disparaissent dans
les huit vues ; les silhouettes et ombres restent proches. Le frontal change
peu visuellement, mais le test géométrique confirme sa séparation. Dans la
carrière, `portal` avance de 1 cm selon −Z : la concurrence avec la grande face
disparaît, sans changement majeur de composition. Le verre est ensuite calculé
sur cette version : trois soutiens, 192 triangles, huit vues supplémentaires.
Les ombres et réfractions restent lisibles ; du bruit optique subsiste à 32 spp.

Source réelle : `%LOCALAPPDATA%/WithinTheLatentWalls/research/`
`run-20261026T062031Z-1577028766/places/place-00001.scene`.
Ce diagnostic réutilise une archive ; aucune nouvelle génération Gemma ni
nouvelle capture SDL n'est revendiquée pour cette passe.

Les 72 images, scènes, rapports, planches de comparaison et journaux sont dans
`C:/works/duo-runs/game-liminal-raytraced-llm-world/COP-20261026/` :
`frontal.comparison.png`, `oblique.comparison.png`, `three.comparison.png`,
`quarry.comparison-0.png` et `quarry.comparison-1.png`.
`visual-recipe.json` garde les réglages et l'empreinte du CLI de cette recette.

## Vérification et usage

Build Release MSVC publié dans `build/Release/liminal_cornell_renderer.exe`.
**13 tests CTest et 64 tests Python passent**. Les quinze cas DeepSeek sont
complétés par les contrôles de grandes faces quasi parallèles, intersection
plane malgré des AABB communes, seuil d'aire, rotations X/Y/Z, faces dorsales,
caméra, approches, sol, bornes, refus global, cumul réel et huit poses.
Le test d'intégration parcourt COP → verre → faits → sauvegarde → revisite →
archive, vérifie les positions sérialisées, rejette les étapes altérées et
constate un seul appel de composition. Les tests existants du verre passent.

```powershell
python scripts/build_native.py --publish
ctest --test-dir build_native -C Release --output-on-failure
build_native/Release/liminal_coplanar_cleanup.exe tests/data/coplanar/oblique.scene C:/preuves/cop/oblique --render
```

Le CLI accepte `--analyze`, `--render`, `--glass`, `--samples N` et `--fixed ID`.
Il produit les étapes `.before.scene`, `.corrected.scene`, `.scene` et leurs
rapports. `--render` compare les huit poses avant/après ; `--glass` ajoute le
verre après COP et une troisième séquence. Ce CLI vérifie les gardes géométriques
intrinsèques ; les audits de repère et du contrat local sont ajoutés par le jeu.

La passe vise seulement les faces opaques orientées vers les caméras surveillées.
Les faces dorsales, les joints opposés et les interfaces diélectriques restent
hors correction automatique. Le bruit, les biais de rayons secondaires et la
qualité artistique de compositions non examinées restent à qualifier séparément.
