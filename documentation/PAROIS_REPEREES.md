# Parois repérées — 27 octobre 2026

Une obstruction rencontrée doit être identifiable dans l'image. Après le
premier contact validé, le moteur ajoute une fine surface transparente au lieu
courant : bord, reflet et réfraction la rendent visible sans fermer la vue.
La boussole marque sa direction par un cadre ambré barré, avec une interruption
du trait autour de la lettre pour conserver sa lisibilité. La liste des passages
à l’arrivée exclut aussi les obstructions déjà découvertes. Avant contact, le
repère est absent et l'issue demeure présentée comme ouverte.

## Orientation et persistance

Le cap est celui de la scène acceptée à sa création, conservé même lors d'un
retour : `forward_direction`, sinon l'opposé de `return_direction`. Le seuil
canonique regarde l'est. Avec un cap nord, O est à gauche, N en face et E à
droite. Avec un cap est, N est à gauche et E en face. La surface de face est
légèrement décalée à droite pour laisser voir le décor central.

Une direction derrière la caméra reste signalée sur la boussole, sans fausse
paroi devant le joueur. Une ancienne scène sans provenance d'orientation fiable
conserve également le signal de boussole seul. Aucun cap n'est déduit d'un texte
libre ou du dernier mouvement.

`known_barrier.cpp` compose la couche `engine-known-obstructions-v1` à partir
des seules barrières `discovered` du lieu courant. Les `glass_box` portent des
noms `known_obstruction_<direction>`. Une recomposition remplace sa couche au
lieu de la doubler. L'architecture enregistrée, les objets et la caméra ne sont
pas modifiés. Le suffixe est ajouté après validation de l'état candidat et
composition des objets, avant la dernière vérification d'annulation.

La couche est recalculée après un retour et une sérialisation diagnostique ;
les raccourcis d'inventaire SDL réutilisent la vue existante. Aucun champ de
sauvegarde, appel LLM, fait d'enquête ou nouvel objet n'est ajouté. Le verre est
une convention de représentation après découverte, pas une preuve sur la
matière ou la cause de l'obstruction. Les soutiens de verre restent distincts.

## Validation

- Cinq groupes de tests proposés par DeepSeek, relus et corrigés par Codex :
  découverte et lieu courant, orientation, trois côtés visibles et arrière,
  répétition sans altération du monde, sérialisation et sortie du lieu.
- Parcours Codex avec réponses LLM simulées et moteur réel : contact annulé puis
  accepté, aucun déplacement, répétition, départ, retour et sérialisation avec
  provenance de scène version 3. La scène d'origine est comparée à chaque étape.
- Les 15 CTest passent. Les tests Python n'ont pas été relancés pour ce lot C++.
- Quatre rendus diagnostiques 960 × 480, 24 échantillons, à partir d'une copie
  de la pièce où le joueur avait été bloqué : avant contact, face, gauche et
  droite. Les trois surfaces sont visibles ; les côtés gardent un bord plus
  sombre et le bruit optique du matériau existant. L'archive joueur est intacte.

- Deux parcours SDL réels de cinq commandes avec Gemma, interface française :
  N, N, contact N, retour S, revisite N. Le second qualifie la version finale.
  Le compteur de déplacements reste à 2 au contact. Le signal est absent avant,
  présent au contact, absent dans l'autre lieu, puis retrouvé à la revisite.
  Les six animations de huit vues se terminent. Captures examinées sous le
  masque ; la liste de retour indique « est, sud », sans nord. Un seul fait
  `obstruction_contact` est acquis ; l'architecture enregistrée n'a aucun suffixe.
- Binaire compilé puis publié dans `build/Release/` avec le lanceur habituel.
  Empreintes du build, de la copie et du manifeste comparées :
  `9a0d5ed5f2d70d46509dc5e638d0eaa86fa7a8a6487adc668dc1affee3c702e0`.

La prose libre peut encore répéter la description du lieu après un contact.
Ce lot corrige son repère visuel et la liste autoritative des passages, sans
ajouter d'analyse sémantique C++ de la narration.
Preuves privées : `C:/works/duo-runs/game-liminal-raytraced-llm-world/BARRIER-20261027/`.
Voir la [revue DUO-048](duo/reviews/DUO-048.md).
