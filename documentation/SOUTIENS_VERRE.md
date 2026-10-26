# Pilote des soutiens de verre — 26 octobre 2026

Les quatre lots de la [roadmap](roadmap-soutiens-verre-blocs-flottants.md) sont
implémentés. Après revue des comparaisons sans HUD, la passe est activée pour les
nouvelles scènes locales acceptées. Deux vues SDL ont ensuite été examinées avec
le masque et le HUD courants. Les lieux existants conservent leur scène enregistrée.

DeepSeek a livré les fixtures et tests bornés du [lot DUO-032](duo/reviews/DUO-032.md).
Codex a corrigé les oracles, réalisé le générateur, l'intégration, les tests de
maillage, les builds, les rendus et la revue finale. Aucun commit ni push.

## Géométrie et décisions

`Scene::boxes` conserve les boîtes originales avec les mêmes sommets et axes
Euler X/Y/Z que le rendu. Le graphe de contact utilise un préfiltre AABB puis les
15 axes séparateurs OBB ; tolérance de contact **0,01 m**, vide minimal **0,05 m**.
Le contact latéral propage l'ancrage. Il ne s'agit pas d'un calcul de stabilité.

La coque suit l'enveloppe convexe XZ des huit sommets, jamais son AABB. Son
épaisseur intérieure vaut **0,25 m**. Les bandes périphériques sont partitionnées
sans recouvrement puis découpées sur les faces inférieures. Les arêtes sont
soudées et subdivisées aux jonctions. Une seule surface fermée est triangulée :
dessus et dessous des bandes, parois externes et internes, aucune face verticale
entre cellules. Le centre reste vide, sans dalle ni plafond qui le traversent.
Le sommet utilise l'enveloppe inférieure affine réelle ; les candidats sont
entièrement au-dessus du sol, donc leurs morceaux ont une hauteur positive.

Le matériau passe par le même constructeur que `glass_box gray(0.66)` au seuil :
IOR 1,52, dispersion 0,035, absorption (0,45 ; 0,22 ; 0,08), transmission calculée
par le moteur et aucun alpha ajouté. Les triangles sont sérialisés explicitement
avec la directive moteur `engine_glass_triangle`, interdite au contrat LLM.
Le chargement des archives relit ces triangles sans exécuter le générateur.

Les candidats sont traités par altitude minimale puis identifiant. Un ajout
valide suffit pour sa composante de contact. Une omission laisse essayer les
autres candidats. Les rapports distinguent notamment :

| Décision | Sens |
|---|---|
| `grounded` | Connecté au sol, directement ou via les boîtes opaques originales |
| `below_gap` | Vide trop petit, au plus 5 cm |
| `candidate` | Détection seule, sans construction |
| `added` / `component_supported` | Coque ajoutée / même assemblage déjà soutenu |
| `narrow_footprint` | Aucun intérieur non dégénéré à épaisseur constante |
| `reserved_volume` | Intersection avec caméra, approche, retour ou exclusion explicite |
| `opaque_overlap` / `existing_glass_overlap` | La coque couperait un autre volume original |
| `support_overlap` | Risque de chevauchement avec un soutien précédent |
| `triangle_budget` | Plafond de 4 096 triangles ajoutés atteint |
| `geometry_omitted` | Géométrie numériquement invalide, raison détaillée conservée |
| `preview_omitted` / `component_unsolved` | Proposition retirée après contrôle du rendu final |

Le contrôle des volumes originaux utilise les cellules convexes de la coque et
les OBB. Entre deux soutiens, le contrôle reste conservateur sur les bornes du
premier : il peut omettre un ajout qui aurait été possible. Les préfabs ne sont
pas des candidats ; le contrat local produit des `box`. Les boîtes de verre
originales n'ancrent pas le graphe opaque et ne sont pas modifiées.

## Intégration et conservation

La scène opaque passe toujours par la normalisation, le contrat 8–20 boîtes,
l'audit, les suppressions d'occultants et son aperçu. Ensuite seulement viennent
les soutiens. Un second aperçu 256 × 128, 4 échantillons, vérifie le rendu réfracté,
sa variation lumineuse et une luminance moyenne d'au moins 35 % de l'originale.
Les approches claires doivent rester identiques. Échec ou délai de ce complément :
scène opaque conservée, statut `opaque_fallback`. L'annulation utilisateur reste
prioritaire. Aucune correction de soutien ne repart vers le LLM.

Cette vérification d'image est grossière : elle ne prouve pas que chaque repère
est lisible après réfraction. La revue visuelle reste nécessaire pour qualifier
d'autres familles de compositions. L'activation concerne le parcours local ;
les diagnostics historiques `direct` et `two-pass` conservent leur comportement.

Chaque lieu conserve `scene_text` final, `opaque_scene_text` et le rapport
`glass_supports_json`. Les empreintes des deux scènes sont vérifiées à la lecture
de l'état. L'archive écrit `.scene`, `.opaque.scene` et `.supports.json` séparés.
Le rapport contient version, paramètres, bloc source, voisins, composante,
contours, identifiant du verre, triangles, ajouts, omissions et vérification
réfractée. Les essais de composition restent opaques. Les revisites chargent la
scène finale immuable. Une réapplication explicite remplace son enveloppe moteur
et ne double pas les soutiens.

Les faits narratifs gardent la visibilité opaque originale et ajoutent les seuls
identifiants de soutiens réellement présents : verre visible décoratif, distinct
des barrières invisibles, sans nouvelle issue ni objet manipulable. Aucun prompt
d'improvisation ni modèle du jeu n'a été modifié ; aucun appel LLM ajouté.

## Recette visuelle observée

Trois fixtures synthétiques : boîte horizontale, rotation Y de 38°, inclinaison
(19°, 27°, 13°). Deux scènes originales proviennent de
`run-20261026T062031Z-1577028766/places/place-00001.scene` et `place-00002.scene`
dans les archives locales. Les captures de la roadmap ne sont pas reconstruites.

Chaque paire utilise la même source opaque, caméra, éclairage, graine 1337,
640 × 320 pixels, 32 échantillons, 3 rebonds diffus, 9 diélectriques et 8 threads.
Mesures locales ponctuelles, sans prétention de benchmark statistique :

| Scène | Soutiens | Triangles ajoutés | Construction | Rendu avant → après |
|---|---:|---:|---:|---:|
| Horizontale | 1 | 64 | 0,86 ms | 895 → 1 213 ms |
| Rotation Y | 1 | 64 | 0,90 ms | 886 → 1 448 ms |
| Inclinaison X/Y/Z | 1 | 136 | 1,61 ms | 893 → 1 483 ms |
| Archive 00001 | 3 | 192 | 2,32 ms | 3 225 → 3 406 ms |
| Archive 00002 | 3 | 192 | 2,13 ms | 2 753 → 3 922 ms |

Les jonctions et l'emprise sont lisibles sur les trois fixtures, sans fissure
visible à cette résolution. Les repères restent visibles à travers le verre.
Le cas incliné montre bien le raccord oblique, mais aussi davantage de bruit
sous la masse. Dans l'archive 00002, plusieurs interfaces assombrissent et
bruitent fortement le centre ; le verre n'est pas une paroi opaque, mais cette
surcharge reste une limite visuelle du pilote. Dans l'archive 00001, les masses
latérales inclinées sont connectées au sol via l'assemblage : leur vide reste
volontairement libre selon la règle conservatrice de connexion.

Les vues complètes SDL du cas incliné et de l'archive 00002 ont été examinées.
Le harnais privé utilise les objets compilés du jeu et `RunSdlFrontend`, sans
modifier le frontend ou injecter une option dans le jeu normal. Deux sorties
normales, code 0 ; aucune inférence. Le masque et le HUD préexistants sont conservés.

Toutes les paires PNG, scènes, rapports et captures de visière sont dans
`C:/works/duo-runs/game-liminal-raytraced-llm-world/VERRE-20261026/`.
Les images `horizontal.before.png` / `horizontal.after.png`, `yaw.*`, `tilted.*`
et `archive-0000{1,2}.*` permettent de refaire la revue.

## Vérifications et utilisation

Build et publication réussis avec `python scripts/build_native.py --publish`.
Onze CTest passent, ainsi que 60 tests Python de `scripts/` et 4 de `tests/`.
Le lanceur utilise le binaire publié ; l'installeur n'est pas reconstruit.

Les tests couvrent contacts, faux positifs AABB, piles, seuils, exclusions,
budgets, déterminisme et rechargement. Le maillage est contrôlé indépendamment :
arête utilisée deux fois en sens opposés, absence de faces dupliquées, volume
positif, sommet testé contre les triangles opaques originaux par lancer de rayon,
vide central et conservation exacte des triangles opaques. Huit orientations,
dont presque verticales, passent. Le parcours avec complétions simulées vérifie
un seul appel de composition, les faits narratifs, la revisite, l'archivage et
le refus d'une source opaque altérée ; aucune nouvelle qualification Gemma réelle
n'est revendiquée pour ce lot.

```powershell
# Analyse seule, sans changer le rendu normal
build_native/Release/liminal_glass_supports.exe scene.scene C:/preuves/candidats --analyze
# Scène augmentée, rapport et paire avant/après à paramètres identiques
build_native/Release/liminal_glass_supports.exe scene.scene C:/preuves/verre --render
ctest --test-dir build_native -C Release --output-on-failure
```

`DynamicSceneConfig::glass_supports=false` désactive la passe pour un appel de
diagnostic ou de test. L'outil autonome ne sollicite aucun modèle et n'altère pas
le fichier source. Les réglages du duo DeepSeek restent indépendants du runtime.
