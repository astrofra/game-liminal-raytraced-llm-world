# Décors dynamiques — bilan de développement et qualification

24 octobre 2026. **Implémentation expérimentale ; qualification non acquise ; défaut hybride.** Ce document distingue les contrôles du logiciel des résultats du vrai modèle. L'optimisation de la latence est différée, conformément à la demande de l'utilisateur.

## Ce qui est implémenté

La voie native demande un `SceneDesignV1` puis un programme `.scene` à Ministral. Les positions, dimensions et assemblages proviennent des réponses du modèle. Le guide artistique est embarqué dans le binaire ; le corpus du banc n'est pas consulté par le jeu. Il n'y a pas de choix d'une scène préparée ni de secours hybride dans le mode dynamique.

Le moteur audite syntaxe, coût, enveloppes, appuis, caméra, sol, passages et visibilité. Il rend un aperçu, autorise deux appels correctifs au total et conserve l'état précédent en cas d'échec. Les scènes acceptées sont sauvegardées avec plan, liaisons d'objets, provenance et empreintes. Les revisites et observations chargent le texte accepté. Les mutations visuelles ne sont pas ouvertes en v1.

Les commandes et limites exactes figurent dans le [guide runtime](GENERATION_DYNAMIQUE_DECORS_RUNTIME.md). Le [plan approuvé](GENERATION_DYNAMIQUE_DECORS_PLAN.md) reste la référence des critères de promotion.

## Validation du logiciel

| Contrôle exécuté | Observation |
|---|---|
| Trois CTest natifs | Canaux LLM, soleil et pipeline dynamique réussis |
| Tests Python | 57 tests existants et 4 nouveaux tests du pilote réussis |
| Auto-tests natifs | Navigation, animation et post-process réussis |
| Dix rendus de non-régression | Sept lieux Eryx et trois références historiques : PNG identiques octet pour octet à l'exécutable antérieur |
| SDL, démarrage et animation | Fermeture automatique propre, huit vues calculées ; cet essai ne compose pas de nouveau lieu |
| SDL, annulation avec vrai runtime | Commande `north` depuis le plateau, fermeture demandée à 3 s : sortie propre en environ 8 s, appel CPU annulé avant le premier token, aucun nouveau lieu ni mouvement ; sauvegarde au chemin accentué relue |
| Transactions, annulation, persistance | Tests avec réponses injectées : échec sans mouvement, cinq créations, retour/revisite, quarante observations, hash altéré refusé, annulation à chaque phase et avant exécution |

Les réponses injectées prouvent le comportement du pipeline, **pas la capacité de Ministral à composer une scène**. Les cinq créations du test ne sont donc pas présentées comme cinq voisins réels générés dans les branches Eryx.

| Lot du plan | État vérifiable |
|---|---|
| DYN-01, contrat et guide | Implémentés ; schéma d'écriture progressivement simplifié après les erreurs observées |
| DYN-02, génération native | Banc réel et exports disponibles ; résultats directs illustrés, qualification des deux passes encore ouverte |
| DYN-03, audit spatial | Implémenté, cas adverses et assemblages ouverts testés |
| DYN-04, corrections et aperçu | Boucle bornée et annulation testées ; les rejets réels restent comptés |
| DYN-05, qualité visuelle | Critère non acquis ; aucune campagne complète ni note humaine à l'aveugle |
| DYN-06, raccord au jeu | Branche native explicite implémentée ; cinq branches vérifiées avec réponses injectées, recette réelle ouverte |
| DYN-07, persistance | Programme exact et provenance sauvegardés ; intégrité, ancienne lecture et revisites testées avec réponses injectées |
| DYN-08, SDL | Progression, conservation de l'ancienne vue et annulation implémentées ; annulation réelle vérifiée |
| DYN-09, recette | Régressions et compilation réussies ; guide embarqué et rendu hors dépôt vérifiés ; promotion du défaut et nouvel installeur non effectués |

## Essais réels et corrections observées

Machine : RTX 4060 de 8 Go, Intel i7-10700. Profil principal inchangé : Ministral 3 8B Instruct Q4, modèle fixé par le manifeste du dépôt. Le banc utilise la température 0,6 et le contexte 8 192. Les variantes d'implémentation conservent des dossiers séparés ; leurs échecs ne sont pas effacés des preuves.

- Premier essai avec Ollama 14B encore résident : délai global de 900 secondes atteint, aucune scène. Après autorisation, le modèle Ollama a été déchargé, sans suppression de ses fichiers. Un essai CPU intermédiaire a été interrompu ; il ne constitue pas une mesure de qualité.
- Premières variantes GPU : confusions entre centre et base des boîtes, passages mal placés, enveloppes trop petites. Les contrôles ont rejeté les propositions.
- Ordonner les champs du schéma avec les dimensions avant les centres a réduit les erreurs de hauteur dans l'essai concerné. Cela ne prouve pas une amélioration générale.
- `bounds: "auto"` remplace le recalcul manuel d'une enveloppe redondante. Le moteur déduit cette boîte des groupes, passages et caméra ; il ne change aucun de ces éléments. Le plan conservé contient les dimensions numériques calculées.
- Les premiers programmes contenaient des balises Markdown malgré la consigne. Un unique emballage exact est maintenant déballé avant l'audit ; aucune prose n'est interprétée comme du code, aucune géométrie n'est réparée automatiquement.
- Les appuis impossibles et le sol absent sous caméra/passages sont maintenant signalés dès le plan. Les diagnostics donnent les dimensions minimales nécessaires à la correction du sol.
- Un essai séparé du profil Reasoning Q4 a échoué sur le protocole de canaux : aucun canal final exploitable. Aucune chaîne de pensée n'a été utilisée comme programme final et le profil du jeu reste inchangé.

Le pilote compare terrasse asymétrique, cour nocturne et escalier oblique, avec les voies directe, deux passes et hybride. Une image de l'hybride ne compte jamais comme succès dynamique. La variante R5 a terminé neuf essais : 0/3 acceptés en deux passes, 0/3 en direct et 3/3 en hybride. Les contrôles et consignes ont ensuite évolué ; ces chiffres restent des résultats de mise au point et ne sont pas une campagne indépendante.

R6 et R7 ont réellement tenté `north` depuis le plateau dans le jeu headless, à la température thermique initiale de 0,1. Rejets après respectivement environ 318 et 235 secondes, sans sauvegarde d'un nouveau lieu. R6 échoue sur des appuis, un vide traité comme une masse puis une sortie tronquée. R7 corrige le plan mais son programme omet `gray()` sur un panneau lumineux après consommation des deux corrections. Le sol a ensuite reçu une notation explicite et le repère est décrit avant les groupes secondaires.

La variante R8 a terminé neuf essais : 0/3 acceptés en deux passes, 3/3 en direct et 3/3 en hybride. Les durées des deux passes sont de 194 à 217 secondes, contre 37 à 82 secondes en direct et 16 à 19 secondes en hybride. Les vues directes de la terrasse et de la cour nocturne ont été inspectées : la première cadre essentiellement un mur ; la seconde montre une colonnade diurne et ne respecte pas l'intention nocturne. Les [images comparées et programmes exacts](concept-art/dynamic-scenes-study/README.md) montrent ces limites plutôt que de confondre acceptation technique et réussite artistique. Les rejets des deux passes portent notamment sur un repère absent et des identifiants raccourcis entre plan et programme ; le contrat d'écriture v4 décrit désormais le repère avant les groupes secondaires et utilise des identifiants courts.

Des builds et contrôles CPU ont tourné pendant certains essais : les durées sont des coûts observés de développement, pas une mesure de performance isolée.

R9 a accepté le premier plan pendant une vraie tentative depuis le plateau, puis rejeté les trois programmes successifs : groupes mal nommés, caméra enfermée, passages occupés et dépassements des enveloppes. Durée totale : 185 secondes, quatre appels terminés par EOS, aucune scène publiée. La mémoire de travail maximale observée est d'environ 5 153 Mio ; l'occupation GPU globale échantillonnée atteint environ 6 317 Mio, bureau compris.

Le contrat d'écriture v5 ajoute `shape` à chaque groupe pour transmettre les parties solides et les ouvertures à la seconde passe. Cette passe reçoit une consigne de traduction technique, sans relancer les consignes de composition libre. Pendant le pilote R10, un diagnostic de correction du sol a été rectifié : le champ d'auteur `ground.size` attend deux composantes X/Z, alors que le message donnait les trois composantes du plan normalisé. La campagne R10 a été arrêtée après les deux graines du premier brief, toutes deux rejetées ; la tentative native en cours a pu terminer. Les seize autres combinaisons prévues n'ont pas été exécutées. Les fichiers terminés et le motif d'arrêt sont conservés ; ce pilote interrompu n'est pas présenté comme une campagne complète. La correction du diagnostic fait l'objet d'une exécution suivante.

R11, en partie réelle, a corrigé son plan en deux appels supplémentaires puis produit un programme rejeté, notamment pour des commentaires `//` non autorisés : 300 secondes, aucune publication. Le programme contient aussi des boîtes présentées comme des évidements, qui ne créent pas d'ouverture dans ce langage sans CSG. R12 limite le plan à cinq groupes : il reste rejeté après 195 secondes, pour dépassements, appuis et passages remplis de géométrie. Les diagnostics et programmes complets sont exportés explicitement hors sauvegarde.

Le contrat v7 nomme séparément `width_x`, `depth_z`, `center_x` et `center_z` du sol, et fixe sa référence verticale locale à zéro. Ces choix éliminent des degrés de liberté redondants et des ambiguïtés de coordonnées, sans choisir les formes ou agrandir les surfaces à la place du modèle. Aucun gain général n'est déduit de ce seul changement de contrat.

Un essai distinct du Ministral 14B Q4 déjà installé a été engagé. Le fichier combiné d'Ollama contient 585 tenseurs ; le chargeur natif n'en consomme que 363 et refuse les 222 tenseurs de vision restants. R13 échoue donc au chargement, sans aucun token : ce n'est pas un échec de composition du 14B.

Une copie texte a ensuite été produite hors dépôt sur E:, sans changement du fichier Ollama ni requantification. Les 363 tenseurs conservés ont chacun été comparés par SHA-256 à l'original. Vocabulaire, types de tokens et règles de fusion correspondent exactement à ceux du GGUF officiel 8B ; le pré-tokeniseur a été identifié comme `tekken` et le template Jinja partagé a été repris, avec l'identité par défaut 14B. Aucun poids 8B n'a été copié. Le rapport de conversion et les empreintes restent dans les preuves externes. Le modèle est chargé par le vrai runtime natif, avec 24 couches GPU pour cet essai ; le profil et le modèle configurés par défaut dans le jeu n'ont pas changé. Cette surcharge expérimentale est identifiée par son fichier réel, indépendamment du nom du préréglage de paramètres.

R14, avec cette copie 14B, a atteint le délai global de 900 secondes. Le premier plan termine en environ 322 secondes, mais deux appuis sont impossibles ; la première correction en résout un, puis la seconde est interrompue par le délai. Aucun programme ni lieu accepté. Ce résultat ne justifie pas de remplacer le 8B par le 14B. L'essai utilise environ 8,3 Gio de mémoire de travail et 5,7 Gio GPU globaux ; ce coût inclut une importante partie CPU.

La version d'écriture v8 conserve le plan v7 et ajoute la correction ciblée des lignes des programmes courts, pour éviter les réécritures de parties déjà valides. Les tests vérifient aussi qu'une correction hors limites, dupliquée ou tronquée laisse le candidat intact et conserve ses diagnostics pour l'appel suivant. Les résultats réels de cette variante sont consignés séparément des essais antérieurs.

R15 revient au 8B, avec le même contrat v7 que R14 : rejet après 192 secondes. Le plan est corrigé, puis le programme confond encore centre et base de plusieurs volumes ; la correction d'erreurs de spotlights laisse ces erreurs géométriques. Cette comparaison reste exploratoire : ni le 8B ni le 14B n'a fourni de lieu accepté dans ces deux essais.

R16, 8B avec graine 1337 et corrections ciblées v8, accepte le plan au premier appel. Le programme ajoute néanmoins des volumes de sortie ; les corrections introduisent ensuite des identifiants dupliqués. Rejet après 190 secondes, sans déplacement ni sauvegarde d'un nouveau lieu. Le contrôle réel prévu de quarante observations et d'une revisite n'a donc pas pu s'exécuter : il est explicitement marqué non testé, et non réussi par substitution d'une fixture.

## Répartition du duo

DeepSeek a livré le guide et les quinze briefs dans DUO-011/R1, puis le pilote de comparaison dans DUO-012/R1 ; DUO-013 apporte une relecture ciblée des consignes. Codex a établi les contrats, revu les accès et modifications, corrigé les livrables, implémenté le runtime, les contrôles, les sauvegardes et le raccord SDL, puis exécuté les tests. Les premières tentatives sans livraison conforme n'ont pas été intégrées. Voir les [revues 011](duo/reviews/DUO-011.md), [012](duo/reviews/DUO-012.md) et [013](duo/reviews/DUO-013.md).

Les preuves brutes restent hors dépôt sous `C:/works/duo-runs/game-liminal-raytraced-llm-world/DYNAMIC-SCENES-01/` : variantes `pilot*`, essai `reasoning`, non-régression `canonical-regression` et essai SDL. Le dépôt accueille les synthèses et les illustrations sélectionnées, sans journaux d'inférence.

## Décision de cette passe

**Le résultat jouable demandé n'est pas encore atteint.** Aucun lieu en deux passes n'a été accepté dans les essais réels de cette passe. Le pipeline natif est disponible en mode expérimental, mais le défaut reste hybride. La réussite des tests C++ ne remplace pas la qualification du modèle.

Les seuils proposés de 90 % d'acceptation et de gain médian d'un point de composition ne sont pas acquis. La campagne complète et les cinq briefs réservés n'ont pas été exécutés. Les seules images LLM acceptées présentées dans ce bilan proviennent de la référence directe ; elles ne sont pas des créations du nouveau mode de jeu.

Le build final a été publié dans `build/Release/` ; les trois CTest et le test natif de navigation passent. Les tests Python du pilote passent également. Aucun commit ni push n'a été effectué pour ce lot ; les modifications préexistantes ont été préservées. Les essais sont terminés et aucun calcul de génération n'est laissé en arrière-plan.

## Point de reprise

Le blocage porte sur la cohérence des propositions spatiales et leur traduction, alors que les contrôles logiciels et le rendu fonctionnent. La reprise doit d'abord obtenir un lieu réel accepté et vérifier sa conservation, avant d'étendre la matrice.

Une piste à évaluer est un contrat qui exprime les primitives et leurs appuis une seule fois, avec des coordonnées de base ou des relations explicites, puis les transcrit en `.scene`. Les formes et paramètres resteraient choisis par le LLM ; cette transcription ne choisirait aucun niveau préconstruit. Cette piste n'est pas implémentée et son gain n'est pas acquis. Elle répond aux confusions centre/base et aux divergences entre les deux descriptions observées dans les essais.

Les points d'entrée sont `src/scene_design.cpp` pour le contrat et les consignes, `src/dynamic_scene.cpp` pour les appels/corrections, `src/scene_validation.cpp` pour les audits, et `tests/dynamic_scene_test.cpp` pour les garanties d'état. Les preuves de chaque variante restent sous le dossier externe indiqué plus haut. Conserver les échecs, les critères de qualification et le défaut hybride lors de cette reprise.
