# Prendre, déposer et retrouver — 26 octobre 2026

Implémentation des lots 1 à 5 de la roadmap des interactions, avec maintien du
lot 2 de l'inventaire. Binaire publié et recettes exécutées ; les observations ci-dessous
distinguent les contrôles mécaniques des essais du modèle.

## Jouer

`inventaire` et l'examen des trois outils restent immédiats, sans inférence ni
chaleur. `déposer la craie`, `reprendre la craie`, `drop chalk` et `take chalk`
transfèrent une seule identité. `regarder tout autour` / `look around` restitue
les objets actuellement déposés. Les homonymes portent des numéros de sélection.

Une petite cible mentionnée dans la description conservée peut être prise sans
catalogue préalable. Le modèle apprécie le geste ; le moteur vérifie le passage,
la cible demandée, la localisation et les retraits précédents. Une source de
gravats fournit au plus un fragment dans cette version. Une source inconnue,
hypothétique, épuisée ou fixe n'autorise aucune acquisition.

Les dépôts sont limités aux surfaces proches, soutenues, libres et visibles
depuis les huit vues de respiration, dans les zones dégagées du masque et du HUD. Le seuil initial et les pièces générées
utilisent le même chemin. Craie, tablette, pinces et fragments ont des formes
simples. La pièce acceptée reste conservée ; seule la couche mobile change.

Les usages de la carte, les marques de craie, l'outillage contextuel, les dons,
jets, cachettes, conteneurs et extractions multiples restent hors périmètre.
Une nouvelle partie retrouve seulement les trois outils initiaux.

## Contrat moteur

- `HardState::inventory_items` reste l'autorité pour les possessions. Le registre
  donne l'identité, le libellé, les alias, les propriétés et la provenance.
- La partition inventaire / listes des pièces est validée. Un transfert se
  prépare sur une copie ; un résultat périmé ou déjà appliqué n'est pas republié.
- Les sources littéraires sont conservées indépendamment des six événements
  récents transmis au modèle. Cette version borne chaque phrase source à un
  prélèvement. Deux cibles d'une même phrase nécessiteront un contrat plus fin.
- Le schéma distingue identités explicites, citations implicites et absence de
  cible. Révision et candidats sont contraints au décodage, puis revérifiés.
  Une garde lexicale et des synonymes usuels empêchent la substitution d'une
  cible sans rapport avec la demande ; les formulations non résolues échouent
  sans mutation.
- Une décision physique durable est mémorisée ; un refus d'accès temporaire ne
  devient pas une propriété définitive. Les identifiants diagnostiques inconnus
  sont conservés sans leur attribuer de capacité de manipulation.
- L'interprétation libre fait au plus un appel, suivi d'au plus une narration.
  Les transferts simples se résolvent localement. Une clarification et la
  confirmation « déjà porté » ne modifient pas la chaleur. Les autres tours
  gardent la proposition thermique bornée existante, appliquée une seule fois.
- Le narrateur reçoit les possessions candidates, les dépôts et les retraits.
  Le moteur ajoute la phrase exacte du transfert. La couverture structurée des
  dépôts et leur mention dans le texte sont vérifiées ; une omission ou une
  réapparition d'une source prélevée reçoit un secours localisé.
- Les propositions ne sont pas affichées comme réussies pendant leur préparation.
  L'annulation abandonne instances, transferts, chaleur et rendu candidats.

La sérialisation diagnostique inclut `objects.version = 1`. Un ancien état sans
registre conserve ses possessions, sans ajouter des outils manquants ni du
butin. Aucun chargement d'archive n'est ajouté au jeu SDL.

## Rendu et archives

Les noms graphiques mobiles sont une représentation hexadécimale stable de
l'identité, suivie d'un indice de pièce graphique. Une reprise retire la couche
déposée. Un objet littéraire doté d'une petite primitive identifiée peut retirer
cette primitive après contrôle de taille, distance et visibilité ; le prélèvement
de matière conserve son tas. Le programme architectural original est archivé.

Chaque commande archive les inventaires avant/après, la proposition, sa réponse
brute, le motif, les registres, les sources et les révisions. Les tours avec des
objets déplacés conservent leurs images et leur scène composée. La visibilité,
les identifiants graphiques et la profondeur sont recalculés depuis cette scène,
sans réutiliser les données de la géométrie initiale. Ces données restent des
preuves de recherche et ne servent pas de mémoire au gameplay.

## Qualification

Les tests C++ contrôlent conservation, partitions invalides, révisions,
sérialisation ancienne/nouvelle, homonymes, annulation tardive, absence de surface,
sources finies et décisions durables. Le parcours contrôlé dépose, quitte la pièce,
effectue quatorze observations, revient puis reprend le même objet. Les réponses
injectées couvrent les omissions, les fausses acquisitions, les cibles sans
rapport avec la demande et les six langues de secours. Images de dépôt/reprise
et du seuil examinées pendant le développement.

Validation finale : **14 CTest et 64 tests Python passent**. Le binaire de
développement est publié dans `build/Release/` pour le lanceur habituel.

Gemma réel : dépôt, observation et reprise de la craie confirmés ; prise d’un
caillou littéraire, dépôt, observation puis reprise sous la même identité ;
prélèvement d’un fragment de gravats, seconde demande sans doublon. Ces sources
littéraires sont des fixtures diagnostiques conservées, et non une preuve de
qualité artistique systématique des nouvelles descriptions.

SDL : quatre commandes réelles, dépôt → observation → reprise → inventaire,
avec Gemma, fermeture normale, captures et archives examinées. La première
capture a révélé un dépôt masqué ; le placement corrigé est visible sous le HUD
et disparaît à la reprise. Quatre images et quatre passes profondeur/ID montrent
les révisions 1 puis 2. Les petits objets gardent des formes simples ; les effets
optiques périphériques réduisent leur définition.

Mesures locales : trois narrations pour la boucle explicite, 20,69 s cumulées
d’appels ; dix appels pour la recette littéraire de huit commandes, 75,01 s,
32 338 tokens d’entrée et 1 806 tokens de sortie. Le chargement et le comptage
préalable ne sont pas toujours inclus dans cette somme ; ce ne sont pas des
promesses de latence. Les interactions libres restent bornées à une interprétation
et une narration, sans boucle de réparation.

Les premiers essais Gemma ont confirmé le dépôt et la reprise explicites. Les
premières prises littéraires ont révélé des clarifications excessives et des
branches/révisions incohérentes ; elles ont été refusées sans acquisition. Le
schéma a été renforcé à partir de ces observations. Les résultats de la recette
finale sont consignés dans la [revue DUO-038](duo/reviews/DUO-038.md).

## Duo

Codex : interfaces, état, sérialisation, validation, résolution, couche mobile,
narration, archives, parcours d'intégration et build.

DeepSeek : tests de conservation proposés via Copilot CLI/Ollama dans deux
worktrees isolés. Les deux runs ont continué après un échec d'outil et sont
non conformes à la règle d'arrêt. Le second fichier a été relu intégralement,
repris et corrigé sous responsabilité Codex ; il ne constitue pas une livraison
automatiquement validée. Voir les contrats et revues DUO-037 / DUO-038.
