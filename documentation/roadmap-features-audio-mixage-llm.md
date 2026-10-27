# Spécification et roadmap — Features audio hors ligne et mixage piloté par LLM

Projet : **Within the Latent Walls / Entre les Murs Latents**  
Date : **26 octobre 2026**  
Statut au 27 octobre 2026 : **pilote implémenté — AUDIO-01/02/03 raccordés, AUDIO-04 vérifié techniquement ; écoute et validation artistique des trims/jonctions encore à effectuer par l’auteur**.

L'[implémentation et la recette](AUDIO_AMBIENCE.md) donnent les commandes réelles,
les conventions retenues, les versions, les résultats observés et les limites.
Les propositions ci-dessous restent la spécification de référence.

## 1. Objectif

Prétraiter les boucles d’ambiance par un script Python, stocker leurs caractéristiques dans les données du jeu et fournir au LLM un petit catalogue textuel exploitable pour choisir les pistes à mettre en avant.

Le LLM propose des **niveaux cibles** en fonction du lieu, de l’historique et de la situation. Le moteur C++ conserve la lecture des boucles et fait évoluer leurs gains progressivement en temps réel.

Les quatre fichiers initiaux sont :

- `assets/sfx/blade-loop.ogg`
- `assets/sfx/bug-loop.ogg`
- `assets/sfx/night-loop.ogg`
- `assets/sfx/static-loop.ogg`

**Leurs noms servent d’identifiants techniques. Ils ne constituent pas des descriptions vérifiées de ce qu’on entend.** Aucune description perceptive de ces quatre fichiers n’a été établie à l’écoute dans ce document.

## 2. Répartition des rôles

| Élément | Responsabilité |
|---|---|
| Script Python, hors ligne | Décoder, mesurer et produire un catalogue reproductible. |
| Données éditoriales | Conserver les descriptions et intentions validées à l’écoute par l’auteur. |
| LLM du jeu | Associer les propriétés sonores à la situation et proposer les gains cibles. |
| Moteur C++ | Valider les identifiants/gains, lire les boucles, interpoler et maîtriser le niveau de sortie. |

La première version fonctionne sans modèle audio, sans embeddings et sans appel LLM supplémentaire. Les caractéristiques sont calculées une fois, puis recalculées seulement si le son ou la politique d’analyse change.

## 3. Trois types de données à distinguer

### Mesures acoustiques

Valeurs calculées avec unités et méthode explicites : niveau, répartition spectrale, variations temporelles, propriétés stéréo.

### Interprétation acoustique prudente

Quelques mots dérivés de règles documentées : « dominante grave », « énergie stable », « variations fréquentes ». La règle et sa version restent accessibles. Un descripteur spectral n’identifie pas à lui seul un instrument, une source réelle ou une émotion.

### Annotation artistique facultative

Texte fourni ou validé par l’auteur : caractère inquiétant, organique, enveloppant, mécanique, etc. Stocker sa provenance comme annotation humaine. Le script ne fabrique pas ces intentions à partir du nom du fichier.

Cette séparation permet au LLM de créer des associations son/image sans lui imposer un tableau du type « couloir → piste X » ou « extérieur → piste Y ».

## 4. Pipeline d’analyse Python proposé

Outil livré : `scripts/analyze_ambience.py`. Commande :

```bash
python scripts/analyze_ambience.py --input assets/sfx --output assets/sfx/ambience_features.json
```

### Dépendances et décodage

- Python, NumPy, SciPy et FFmpeg/ffprobe constituent une base suffisante pour le pilote.
- Librosa peut remplacer certains calculs si cette dépendance est déjà retenue, mais n’est pas nécessaire au runtime du jeu.
- Fixer et consigner les versions utilisées lors de l’implémentation.
- Décoder en PCM flottant, conserver le nombre de canaux et mesurer les niveaux sur le signal original.
- Pour l’analyse spectrale, employer une fréquence commune, par exemple 48 kHz, avec rééchantillonnage explicite. Ne pas présenter les fréquences créées par suréchantillonnage comme une information nouvelle.
- Analyser toute la boucle ; traiter par blocs si nécessaire. Ne pas extrapoler la description globale depuis les seules premières secondes.
- Ne pas réécrire, normaliser ou remplacer les OGG sources pendant l’extraction.

### Fenêtrage initial

Hypothèse de départ : fenêtre de Hann de 2 048 échantillons, pas de 512 à 48 kHz. Consigner ces paramètres et les conventions exactes de calcul.

Analyser les canaux séparément et agréger leurs puissances pour la représentation spectrale globale. Éviter un simple mélange mono gauche + droite : une opposition de phase pourrait annuler une partie du son analysé.

Détecter le silence avant d’interpréter les features. Les trames silencieuses ne doivent pas produire une fausse « tonalité », une instabilité extrême ou des divisions par zéro.

## 5. Features utiles pour la première version

| Famille | Données à stocker | Utilité et limite |
|---|---|---|
| Identité | ID stable, chemin relatif, SHA-256 des octets, durée, fréquence d’origine et canaux | Relier les mesures au fichier exact. |
| Niveau | RMS en dBFS, crête échantillonnée en dBFS, facteur de crête en dB | Connaître la réserve de niveau et les écarts entre pics et énergie moyenne. Le RMS n’est pas une mesure complète de sonie. |
| Sonie | Sonie intégrée LUFS et true peak dBTP si disponibles via FFmpeg | Aider au réglage des trims ; distinguer true peak et crête échantillonnée. Les clips trop courts ou silencieux peuvent ne pas fournir une mesure exploitable. |
| Spectre | Centroïde spectral, rolloff à 85 %, flatness spectrale | Décrire la répartition et la concentration du spectre. La flatness ne suffit pas à affirmer une hauteur musicale ou une source sonore. |
| Bandes | Parts de puissance sous 250 Hz, entre 250 et 2 000 Hz, entre 2 000 et 8 000 Hz, puis au-dessus de 8 000 Hz jusqu’à la bande analysable | Donner au LLM une distinction compacte grave/médium/aigu. Stocker les bornes et gérer les fréquences sources insuffisantes. |
| Évolution | Percentiles 10/50/90 du RMS par trame, écart interpercentile en dB | Distinguer une énergie régulière d’une forte variation temporelle. |
| Variations spectrales | Flux spectral positif et nombre de pics détectés par seconde | Décrire une activité mesurée, sans l’assimiler automatiquement à des impacts ou à un tempo. |
| Stéréo | Corrélation gauche/droite et rapport d’énergie Side/Mid | Décrire la différence entre canaux, pas prouver une réverbération ou un espace réel. Pour un son mono : métriques stéréo non applicables. |
| Bouclage | Écart d’amplitude à la jonction par canal, comparaison d’énergie au début et à la fin | Signaler une jonction potentiellement audible ; ce diagnostic ne remplace pas l’écoute. |

Pour chaque série temporelle pertinente, conserver des statistiques compactes plutôt que toutes les trames. Définir clairement les conventions : centroïde sur magnitude, parts de bandes sur puissance, flatness sur puissance, normalisation du flux spectral et seuils de détection des pics.

Les bandes absentes de l’enregistrement sont marquées non disponibles ; ne pas fabriquer un ratio aigu artificiellement faible après suréchantillonnage. Utiliser des dénominateurs cohérents et vérifier la somme des parts sur les bandes réellement disponibles.

Un indice de régularité par autocorrélation de l’enveloppe peut être ajouté ensuite, avec période et confiance. Le pilote ne force pas de BPM sur des ambiances qui n’ont pas de pulsation identifiable. MFCC, chroma et longs vecteurs de features ne sont pas nécessaires au premier catalogue destiné au LLM.

## 6. Transformer les mesures en fiches compactes

Produire deux vues à partir de la même analyse :

1. **Catalogue détaillé JSON**, livré avec les données et utile au moteur/diagnostic.
2. **Fiches LLM**, de l’ordre de 20 à 40 mots par piste, issues d’un formateur déterministe.

Exemple de formulation **fictif, sans attribution à un fichier joint** :

> Dominante grave ; énergie assez stable ; spectre diffus ; peu de variations rapides détectées. Annotation de l’auteur : présence sourde et enveloppante.

### Politique de qualification

- Stocker les seuils dans une petite configuration versionnée.
- Calibrer les adjectifs acoustiques sur quelques signaux de référence et à l’écoute des boucles réelles.
- Éviter un classement relatif forcé des quatre pistes : la moins aiguë du lot peut néanmoins rester aiguë dans l’absolu.
- Employer une formulation modérée ou omettre un qualificatif lorsque la mesure ne permet pas de conclure.
- Ne pas générer automatiquement des termes comme « insectes », « machine », « vent » ou « nuit » à partir des noms ou de quelques features.
- Conserver les notes d’auteur dans un fichier séparé, par exemple `ambience_annotations.json`, afin qu’une nouvelle extraction ne les écrase pas.

Le LLM reçoit les descriptions compactes et quelques dimensions utiles, pas une matrice de centaines de valeurs. Des scores normalisés peuvent l’accompagner si leur sens est défini ; ils ne remplacent pas les mesures sources ni leur provenance.

## 7. Format des données du jeu

Structure recommandée du catalogue :

| Bloc | Champs principaux |
|---|---|
| En-tête | `schema_version`, version du script, versions des outils, configuration et empreinte de configuration. |
| Source par piste | `id`, `path`, `sha256`, durée, fréquence et canaux. |
| Analyse | Paramètres spectraux, mesures avec unités, statistiques, valeurs indisponibles et raisons. |
| Fiche | Descripteurs acoustiques dérivés, texte compact et version de la règle. |
| Éditorial | Annotation humaine facultative et provenance. |
| Lecture | `trim_db` validé, gain maximal autorisé et éventuels avertissements de bouclage. |

Les IDs `blade`, `bug`, `night` et `static` peuvent être conservés pour le raccord aux fichiers. Leur stabilité est indépendante de la description sonore.

Utiliser du JSON UTF-8 strict : aucune valeur `NaN`, `Infinity` ou `-Infinity`. Représenter une mesure non exploitable par `null`, accompagnée d’une raison. Un son silencieux ne doit pas devenir une analyse valide remplie de nombres arbitraires.

La mise en cache utilise l’empreinte du fichier, la configuration et la version de l’outil. L’ordre d’écriture est stable. Écrire le nouveau catalogue dans un fichier temporaire, le valider, puis le publier atomiquement. Une extraction en échec ne remplace pas silencieusement le catalogue exploitable précédent.

## 8. Raccord au LLM et au mixer

### Catalogue dans le prompt

Transmettre les quatre fiches, les IDs autorisés et le mix actuellement appliqué dans l’appel narratif existant. Le contexte de lieu et l’historique restent ceux du jeu.

Demander au modèle de proposer les gains cibles, par exemple sous un champ `audio_mix` dont les quatre propriétés sont des nombres entre 0 et 1. **Ces gains ne sont pas des probabilités : ils n’ont pas à totaliser 1.** La sortie peut conserver le mix précédent ou rendre toutes les pistes silencieuses.

Le schéma JSON, son parseur, sa validation et la gestion des réponses de secours doivent évoluer ensemble. Ajouter uniquement une phrase au prompt ne suffit pas si le schéma actuel refuse le nouveau champ.

Les niveaux ne sont appliqués qu’après validation du tour. Une sortie audio absente ou invalide conserve le dernier mix valide ; elle ne doit pas invalider un déplacement ou une scène déjà recevables. Séparer le diagnostic audio des décisions mécaniques du jeu.

### Lecture continue et rampes

- Chaque boucle poursuit sa lecture même à gain nul ; une commande ne redémarre pas le fichier.
- Le moteur interpole du gain courant vers la nouvelle cible, par exemple sur cinq secondes pour le pilote, avec une durée configurable.
- Une nouvelle cible reçue pendant une rampe repart de la valeur effectivement atteinte, sans saut de niveau.
- Le lissage dépend du temps audio ou des échantillons, pas du framerate du raytracer.
- Le thread audio reçoit des cibles validées par un mécanisme adapté ; il ne lance ni inférence, ni décodage lourd, ni allocation non bornée.
- Le C++ valide les valeurs finies, les IDs et les bornes. Il conserve la maîtrise du gain global, de la marge de mixage et de la protection contre la saturation.

L’égalité numérique des gains ne garantit pas une égalité de niveau perçu. Conserver les fichiers sources et un `trim_db` par piste, initialement nul puis ajusté après mesure et écoute. Toute proposition de compensation de sonie doit plafonner l’amplification et rester distincte d’une normalisation automatique des fichiers.

Le catalogue sonore ne crée aucun fait narratif : monter une piste annotée « organique » ne prouve pas qu’une créature est présente. Le mix relève de l’expression sensible, pas de la topologie ou de l’inventaire.

## 9. Lots de réalisation

| Lot | Résultat concret |
|---|---|
| AUDIO-01 — Extraction | Script capable de mesurer les quatre fichiers et de produire un JSON validé, sans modifier les sons. |
| AUDIO-02 — Fiches | Règles de description versionnées, fiches compactes relues et annotations d’auteur facultatives. |
| AUDIO-03 — Intégration | Catalogue chargé par le jeu, champ de gains dans l’appel narratif et validation sans nouvel appel LLM. |
| AUDIO-04 — Écoute en jeu | Rampes continues, trims vérifiés, transitions et comportement en cas d’erreur examinés. |

Commencer par AUDIO-01/02. Un mixer de diagnostic réglable manuellement permet de vérifier les niveaux et les transitions avant de confier les cibles au modèle. Aucune réécriture du générateur de scènes n’est requise.

## 10. Recette et définition de terminé

- Signaux de test : silence, sinusoïde, bruit, impulsions, changement d’amplitude, mono et stéréo en opposition de phase.
- Vérifier la cohérence des features et les valeurs non applicables, sans prétendre qu’un test numérique valide une émotion.
- Analyser les quatre boucles en entier ; examiner leur jonction et leurs niveaux à l’écoute par l’auteur.
- Refaire une extraction inchangée : mêmes mesures à la précision publiée et aucune modification des annotations humaines.
- Modifier un fichier ou la configuration : invalidation correcte du cache.
- Fournir un fichier corrompu : erreur explicite, catalogue précédent préservé.
- Tester IDs inconnus, gains hors limites ou non finis et absence de champ audio : dernier mix valide conservé.
- Jouer plusieurs transitions, annuler une génération et vérifier l’absence de redémarrage des boucles ou de saut de gain.
- Vérifier la marge de niveau lorsque les quatre pistes sont présentes et que leurs pics se superposent.
- Archiver la version du catalogue, les cibles demandées, les valeurs retenues et les événements de changement de mix.

**Terminé pour le pilote :** un prétraitement Python reproductible produit les données et fiches des quatre pistes ; le LLM peut choisir leurs niveaux à partir de ces fiches ; le moteur assure les transitions et la lecture sans analyse audio pendant la partie.

## 11. Prochaine itération — variété du mixage (À faire)

**Constat observé.** Partie `run-20261027T072857Z-1411155189`, instantané des huit
premiers tours d’une partie sans `closed.json` — donc pas la totalité d’une
partie : cinq lieux découverts, six propositions LLM et deux commandes sans
proposition (inventaire, consultation tablette). Trois mixes, ordre
blade/bug/night/static : tour 1 `0,2/0,1/0,3/0,4` ; tours 2 à 7 `0,1/0/0,2/0,3` ;
tour 8 `0,1/0,1/0,2/0,2`. Quatre des six propositions sont identiques ; le
plateau dure 3 min 13 entre la publication du tour 2 et le tour 8 ; deux
changements seulement après l’installation initiale. Le mixer n’a archivé aucune
erreur et les gains précédents ont été atteints : l’absence de variété des
propositions ne prouve pas une panne de lecture. Aucune mesure d’écoute humaine
ni capture de sortie n’a été produite.

**Hypothèses, non établies.** Fiches acoustiques trop similaires (toutes
décrivent un spectre concentré et des variations fréquentes) ; inertie possible
du mix courant dans le choix ; descriptions narratives répétitives. Trois titres
identiques successifs ont été observés, sans preuve de causalité sonore. Les gains
linéaires ne sont ni des probabilités ni des niveaux perçus égaux ; les trims
restent à 0 dB et ne sont pas validés à l’écoute.

**AUDIO-05 — fiches et écoute (À faire).** Produire des annotations humaines
distinctives issues de l’écoute des vraies boucles, avec provenance. Calibrer
seuils et trims par la mesure et l’écoute, sans source ni émotion inventée depuis
les IDs et sans réécriture des OGG.

**AUDIO-06 — choix expressifs (À faire).** Tester une consigne favorisant une
dominante, le contraste, le retrait et le silence lorsque la situation le
justifie, la conservation du mix restant possible. Écarter le changement forcé à
chaque déplacement, les quotas, le hasard arbitraire, le mapping lieu → piste et
tout parseur C++ de langage naturel. Conserver le même appel narratif et un
schéma/validation/annulation/rampes cohérents. Ce lot documentaire ne modifie pas
le runtime ; ces essais restent à réaliser.

**AUDIO-07 — recette comparée (À faire).** Rejouer les mêmes scènes et commandes,
avec les mêmes graines, avant et après, sur plusieurs parcours : nouvelles
arrivées, revisites et actions stationnaires. Séparer les non-propositions des
répétitions explicites. Compter les mixes distincts, la fréquence des répétitions,
la durée des plateaux, les plages de gains et les changements de dominante ;
distinguer variation de volume et équilibre. Archiver les cibles et les gains
réellement atteints, les raisons de conservation ou d’erreur déjà disponibles, et
écouter comparativement des passages reproductibles. Réussite : contrastes
perceptibles et cohérents quand le contexte change, continuité lorsqu’elle est
pertinente, sans seuil arbitraire de diversité.

Le correctif immédiat de démarrage est traité séparément par Codex ; il ne fait
pas partie de cette itération et n’est pas livré par ce document.

## Références techniques

- [FFmpeg — filtres audio et mesures de sonie](https://www.ffmpeg.org/ffmpeg-filters.html).
- [Librosa — implémentation et définitions des descripteurs spectraux](https://github.com/librosa/librosa/blob/main/librosa/feature/spectral.py).

Ces références décrivent les mesures, pas une méthode universelle de conversion du son en émotion. Les choix de seuils, de vocabulaire et de mixage restent à qualifier sur les sons du projet.
