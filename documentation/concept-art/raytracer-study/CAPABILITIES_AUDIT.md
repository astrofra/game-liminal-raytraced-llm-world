# Audit statique du raytracer — référence avant lumière directionnelle

Statut : **relu, corrigé et intégré par Codex** — [revue DUO-010](../../duo/reviews/DUO-010.md).
Date : 2026-10-24
Base : `bd4306f4131fbf56784ac630a122f989a7298912`
Branche : `codex/duo-010`
Audit initial : DeepSeek via Copilot CLI/Ollama. Vérification et corrections : Codex.

Périmètre du contenu technique : audit statique de `src/scene.cpp`, `src/renderer.cpp`, `src/core.h` et
`documentation/SCENE_FORMAT_V1.md`. Aucune scène créée, aucun rendu, aucune image consultée,
aucun test exécuté par l’exécutant. Les numéros de ligne renvoient à la base ci-dessus.
La revue signale aussi un comptage de lignes sur les autres `src/*.cpp`, hors lectures prévues.
Les expériences de Codex figurent dans le [comparatif](README.md). Une `distant_light` a ensuite été ajoutée à la demande de l’utilisateur ; cet audit conserve la photographie du moteur avant cet ajout.

## Table capacité / preuve / conséquence probable

| Capacité | Preuve (fichier:lignes) | Conséquence probable |
| --- | --- | --- |
| Boîte et rotation de primitive | `AddBoxPrimitive` scene.cpp:625-670 ; `RotateEulerDegrees` 569-597 ; application 648 | 12 triangles, 6 quads à normale orientée ; `rot(x,y,z)` en degrés, ordre X puis Y puis Z, autour du centre. |
| Plan fini | `AddPlanePrimitive` scene.cpp:599-623 ; parsing 2462-2491 | Quad de 2 triangles ; `normal()` ne fait qu'orienter la géométrie, le plan reste double face (renderer.cpp:345-347). |
| Préfab : capacité géométrique interne | `AddCapsulePrimitive` scene.cpp:1083-1158 ; `AddCrystalShardPrimitive` 1184-1232 ; `AddRockPrefab` 1459-1516 | Capsules (12 segments radiaux), prismes hexagonaux pointus (24 triangles), rochers à 6 sommets jitterés. Cette capacité n'est pas exposée en grammaire. |
| Rotation de prefab | Seule la branche `box` lit `rot(` (scene.cpp:2521) ; les prefabs passent par `AddPrefabChildBoxRotated` 703-722 | Absence dans la grammaire : les angles sont codés en dur par prefab (beacon ±16° 1249-1250, gate -12° 781, pylon ±7° 1415-1416). |
| Palette ocre | `InferMaterialSemantic` scene.cpp:193-199 (`ground` en égalité stricte, `desert`, `dune`, `sand`, `ridge`, `outcrop`) ; `BuildSemanticAlbedo` 205-214 | L'ocre dépend du nom de matériau. `sandstone` contient `sand` et passe en ocre ; un plan nommé `ground_main` n'en bénéficie pas. |
| Déclencheur `led` | scene.cpp:185-187, 217-218, 233-234, 1725 | Toute sous-chaîne `led` force albedo/émission rouges et exclut la surface de `emissive_triangles` ; un nom de prefab contenant `led` contamine tous ses enfants (suffixés, 683-690). |
| Déclencheur `cactus` | scene.cpp:189-191, 215-216 | Sous-chaîne dans le nom ⇒ vert (0.38, 0.47, 0.34) mis à l'échelle de `gray()`. |
| Verre verrouillé | `AddGlassPrimitiveMaterial` scene.cpp:437-457 | IOR 1.52, dispersion 0.035, absorption (0.45, 0.22, 0.08), transmission 0.84-0.99, cutoff 0.02 ; non exposé en grammaire, `glow()` ne règle que l'émission. |
| Éclairage émissif | `CollectEmissiveTriangles` scene.cpp:1719-1729 ; `EstimateEmissiveTriangleLighting` renderer.cpp:644-702 | Liste de triangles, pas de lampes. Les LEDs rouges sont exclues de cet échantillonnage direct ; un chemin qui les frappe peut toutefois recevoir leur émission (renderer.cpp:824-825). |
| Spot caméra | `BuildCameraLightState` renderer.cpp:166-193 ; `EstimateCameraSpotLighting` 704-766 | Area light analytique attachée à la caméra (cone/range) ; aucun triangle ajouté ⇒ ni visible dans le cadre, ni présent dans les reflets ou `emissive_triangles`. |
| Ciel aux rebonds | `SampleSkyBackground` renderer.cpp:238-271 ; rayon sans intersection, ligne 804 `* 2.0f` | Le ciel éclaire bien les surfaces via les chemins diffus qui atteignent l’environnement. Absence d’échantillonnage direct explicite du ciel (NEE environnementale), pas absence d’éclairage ni de GI. |
| Profondeur effective | `TracePath` renderer.cpp:799, 909-915, 931-934 | `max_path_events = max_bounces + max_glass_bounces` ; le rayon sortant du dernier rebond diffus est tiré puis abandonné (932-934), donc ciel et indirect au-delà de `max_bounces-1` rebonds diffus ne sont pas suivis. |
| Verre sans éclairage direct | renderer.cpp:831-906 (`continue` en 906) ; `EstimateDirectLighting` 768-778 | Pas de NEE à une interface diélectrique ; transport par Fresnel/réfraction, puis émission ou surfaces/ciel atteints. `TraceShadowTransmittance` filtre les ombres à travers le verre. Pas de technique dédiée aux caustiques ; leur qualité ne se déduit pas de ce seul audit. |
| Tone mapping | `ToneMapChannel` renderer.cpp:964-969 ; `ToneMapToRgb` 971-980 | `1-exp(-v*exposure)` puis gamma 1/2.2 par canal ; pas de bloom, pas de courbe filmique, pas de balance des blancs ; `exposure` global (core.h:320). |
| Textures, UV, rugosité | `Material` core.h:197-207 ; `Triangle` core.h:223-234 | Aucun champ roughness/metal/specular, aucune UV : ni texture, ni normal map, ni carte de rugosité. Un albédo uniforme par matériau. |
| Diffus vs spéculaire | `brdf = albedo / kPi` renderer.cpp:695 et 718 ; spéculaire seulement `EvaluateDielectricFresnel` 394-432 | Diffus strictement lambertien ; le seul reflet spéculaire vient du verre diélectrique. |
| Sélection des lumières | renderer.cpp:655-660 (uniforme), 664-676 (cosinus), 692-693 (pdf) | Sélection par triangle : une lampe-boîte (12 triangles) est 6× plus probable qu'un panneau-plan (2), mais ses faces arrière donnent `cos_light <= 0` ; un cluster lumineux (5 × 24 = 120 triangles) peut dominer la liste. Le PDF compense la probabilité : le problème attendu concerne la variance, pas une multiplication automatique de l’énergie. |
| Limites de rendu peu échantillonné | core.h:312-336 (16 spp, 2 lumières, 9 verre) ; renderer.cpp:67 et 937 (clamp 6.0) ; RR 920-927 | Bruit résiduel, fireflies plafonnées à 6 par échantillon, pas de dénoiseur ; l'éclairage de grandes surfaces par petites lampes reste granuleux. |
| Validation et propriétés inconnues | parseur scene.cpp:1790-2527 | Une directive inconnue échoue (2527) ; une propriété non reconnue, par exemple `roughness(...)`, est ignorée silencieusement. |
| `detail()` du cluster cristallin | scene.cpp:2139-2162 (`unused_detail`) ; `AddCrystalClusterPrefab` 1255-1262 | `detail()` est accepté puis ignoré pour `prefab_crystal_cluster` ; seuls `gray()` et `glow()` agissent. |
| Émission d'un plan | renderer.cpp:824-827 (ajout inconditionnel) ; 664-676 (normale brute) | Un panneau `emit()` est lumineux vu des deux côtés mais n'éclaire que du côté de sa normale géométrique. |

## Distinction grammaire / capacité interne

- Grammaire : `box` lit `rot()` (scene.cpp:2521) et `emit()` (2522) ; `plane` ne lit que `normal()`, `size()` et `emit()` (2462-2491) ; aucun prefab ne lit `rot()`.
- Options par directive : `emit()` uniquement sur `plane` et `box` ; `glow()` uniquement sur `prefab_crystal_scanner` (2134) et `prefab_crystal_cluster` (2161) ; `bars()` uniquement sur `prefab_gate` (1954).
- Capacité interne non exposée : capsules (1083-1158), prismes hexagonaux pointus (1184-1232), rochers jitterés (1459-1516), verre diélectrique (437-457).
- Paramètres verrouillés dans le moteur : IOR 1.52, dispersion 0.035, absorption (0.45, 0.22, 0.08) et cutoff 0.02 (450-454) ; angles des enfants de prefab (1249-1250, 1415-1416).
- Conséquence : l'absence d'une propriété dans le langage ne signifie pas absence de géométrie ; un prefab peut produire des formes obliques ou courbes que la grammaire ne sait pas décrire.

## Constats transverses

- Toute couleur passe par la palette (scene.cpp:205-241) : `gray()` module la luminance d'une teinte imposée, il ne choisit pas la teinte.
- Trois sources de lumière seulement : ciel atteint par échappement (renderer.cpp:804), triangles émissifs (644-702), spot caméra (704-766). Aucune lumière ponctuelle de scène.
- Le spot caméra ne crée aucune géométrie (166-193) : il éclaire sans être un objet du cadre.
- Le verre est un cas séparé (831-906) : pas d’échantillonnage direct à l’interface diélectrique, mais ses facettes émissives alimentent `emissive_triangles` (scene.cpp:1725).
- La matière fine n'existe pas : une seule albedo par matériau (core.h:197-207), donc la lisibilité vient du découpage géométrique et des valeurs `gray`/`emit`/`glow`.
- Les noms sont une API implicite : `led`, `cactus`, `sand`, `dune`, `desert`, `ridge`, `outcrop` changent matière et éclairage (scene.cpp:185-198).

## Points à confirmer expérimentalement

- Visibilité du spot caméra : aucun triangle n'est créé pour le panneau (renderer.cpp:166-193).
- Contribution du ciel après un ou deux rebonds diffus, et absence après le dernier rebond (932-934).
- Variance réelle d'une scène contenant `prefab_crystal_cluster` avec 2 échantillons de lumière par rebond.
- `glow()` n'a qu'une borne basse dans le code (scene.cpp:2134/2161, `std::max`) alors que la doc annonce une émission bornée.
- Effet des sous-chaînes `led`, `sand`, `dune`, `cactus` sur des noms de scène inattendus.
- Rendu d'un plan `emit()` observé de dos.
- Coût des ombres en présence de verre, plafonné à 16 intersections (renderer.cpp:69, 523-602).

## Lectures et écritures

- Contenu examiné : `src/scene.cpp`, `src/renderer.cpp`, `src/core.h`, `documentation/SCENE_FORMAT_V1.md`. Comptage global des `src/*.cpp` hors contrat signalé dans la revue.
- Écrite : `documentation/concept-art/raytracer-study/CAPABILITIES_AUDIT.md` (ce fichier), unique écriture.
- Aucune erreur d'outil rencontrée ; aucun test, rendu ou commit effectué.
