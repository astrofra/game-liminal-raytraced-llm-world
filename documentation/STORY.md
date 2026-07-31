# Le Désert des tokens

## Synthèse concise du projet

*Le Désert des tokens* est une fiction interactive générative située dans un datacenter autonome construit au milieu d’un désert. Un officier y est affecté pour surveiller une attaque numérique massive, annoncée comme imminente mais dont la nature, l’origine et même l’existence demeurent incertaines.

Le datacenter actualise la fonction de la forteresse dans *Le Désert des Tartares* (D. Buzzati) : un lieu isolé, organisé autour de l’attente d’un événement qui pourrait ne jamais survenir. Il évoque également *En attendant les barbares* (J.M. Coetzee), dans la mesure où l’institution chargée d’identifier la menace peut finir par la produire elle-même, en transformant chaque panne, variation de trafic ou phénomène naturel en signe d’une offensive dissimulée. Il ne s’agit cependant pas d’une adaptation, mais d’une fiction originale consacrée aux infrastructures numériques, à l’intelligence artificielle et à la matérialité écologique du calcul.

## Le datacenter comme forteresse écologique

L’infrastructure mobilise en permanence ses capacités de calcul pour analyser les journaux réseau, les tentatives d’authentification, les anomalies thermiques, les corruptions de données et les signaux observés dans le désert. Or cette activité produit elle-même de la chaleur, consomme de l’eau et accélère la dégradation des systèmes de refroidissement.

La menace extérieure attendue masque ainsi une destruction bien réelle : celle que le datacenter inflige à son propre environnement. Plus il cherche à se protéger, plus il épuise les ressources nécessaires à sa survie. Le centre transforme de l’électricité, de l’eau et de la chaleur en tokens afin de déterminer si quelqu’un cherche à l’attaquer ; plus il calcule, plus il trouve de raisons de continuer à calculer.

Dans une version autonome destinée à une projection, l’officier peut être incarné par un **joueur fantôme scénarisé**. Une série de commandes préparées — consulter les rapports, inspecter le refroidissement, observer l’horizon, interroger le système, rédiger le rapport quotidien — garantit la durée et la progression générale, tandis que le LLM génère en direct les réponses, les interprétations et les variations du monde.

Le rapport quotidien constitue une mécanique centrale. Une formule prudente comme « origine indéterminée » peut être réinterprétée par le système comme « attaque d’origine dissimulée confirmée ». La fiction se construit ainsi par sédimentation administrative de l’incertitude.

## Les trois températures

Le projet fait coïncider trois sens du mot *température* : la température atmosphérique du désert, la température matérielle du datacenter et la température statistique du modèle de langage.

Lorsque le désert chauffe, la température d’échantillonnage du LLM augmente. Les textes passent progressivement d’un registre administratif sobre à des interprétations plus spéculatives, contradictoires et métaphoriques. L’absence de preuve peut devenir la preuve d’une dissimulation ; les dunes, la poussière ou la chaleur peuvent être considérées comme les vecteurs d’une attaque distribuée.

Cette relation produit une boucle de rétroaction :

```text
calcul accru
    ↓
consommation et chaleur
    ↓
réchauffement du désert
    ↓
augmentation de la température du LLM
    ↓
interprétations plus absurdes et plus prolixes
    ↓
davantage de tokens et de calcul
```

La température ne doit toutefois pas rendre tout le système aléatoire. L’état matériel du datacenter et le compilateur de scènes 3D restent pilotés à très basse température. Seuls le système de défense et le narrateur dérivent. Le monde conserve donc une réalité minimale ; c’est son interprétation institutionnelle qui devient pathologique.

La montée peut s’achever par un retournement : lorsque le datacenter coupe ses unités de calcul, la température du modèle retombe et son dernier message redevient froid et factuel — aucune attaque externe n’a été identifiée, mais les réserves d’eau sont épuisées.

## Le lien avec la démoscène

La démoscène n’est pas le sujet littéral du récit. Elle intervient comme **régime esthétique et technique de fabrication**.

Le projet reprend une logique dans laquelle la contrainte matérielle n’est pas seulement un manque, mais le moteur d’une écriture. La puissance de la machine contemporaine est répartie de manière volontairement dissymétrique : le GPU est principalement mobilisé par l’inférence du LLM, tandis que l’image reste produite par un raytracer élémentaire, composé de primitives simples, d’une lumière instrumentale, d’une résolution limitée et d’un bruit visible.

L’intelligence artificielle n’abolit donc pas la contrainte : elle la déplace. La dépense computationnelle quitte l’image pour migrer vers le langage, la fiction et l’instabilité du monde.

L’écriture visuelle prolonge le régime perceptif identifié dans le moodboard de jeux et de démos des années 1980 et 1990 : peu de volumes, des masses franches, de grands aplats, des zones opaques, des couloirs et des seuils, une interface très présente et un texte qui complète ce que l’image ne montre pas. Il ne s’agit pas de reproduire un style rétro, mais de réactiver une **expressivité sous contrainte** : un monde incomplet mais opératoire, que le spectateur doit reconstruire mentalement.

La contrainte doit être inscrite dans le système pour ne pas devenir un simple effet graphique : nombre limité de primitives, absence de textures photographiques, faible budget lumineux, résolution interne fixe, budget de tokens et fenêtre de contexte maîtrisés, latence exposée dans l’interface. La technique devient alors une condition visible de la forme.

## Visualiser la contrainte sur Amiga

Pour expliquer cette notion au public, il est insuffisant de dire que les anciens ordinateurs « n’étaient pas puissants ». Le visualiseur DMA de WinUAE permet de montrer concrètement comment les ressources de l’Amiga sont distribuées entre le CPU, le Copper, le Blitter, les bitplanes, les sprites, l’audio et les autres canaux d’accès à la mémoire Chip.

Une comparaison en trois temps peut rendre ce partage lisible :

1. un Workbench statique ;
2. l’ouverture ou le déplacement d’une fenêtre ;
3. une séquence précisément identifiée d’une démo mobilisant plusieurs composants.

La visualisation se transforme alors en partition matérielle. Elle montre que l’esthétique de la démoscène ne résulte pas seulement d’une faible puissance globale, mais d’une orchestration temporelle : quel composant peut accéder au bus, à quel moment, pour produire quelle partie de l’image ou du son.

Cette démonstration historique peut être mise en regard de la télémétrie de *Le Désert des tokens* : charge CPU et GPU, occupation de la VRAM, vitesse d’inférence, nombre de tokens, température et temps de rendu. Les architectures ne sont pas équivalentes, mais elles rendent visible un même principe : la forme de l’œuvre dépend de la manière dont les ressources de la machine sont distribuées.

## Démoscène, médiation et intelligence artificielle

Le projet réunit ainsi les trois axes de la recherche.

La **démoscène** fournit le principe d’une expression située dans l’architecture, produite en temps réel et transformant la contrainte en esthétique.

La **médiation** intervient à plusieurs niveaux : l’interface donne accès au monde par machine interposée ; le raytracer traduit partiellement la fiction du LLM ; WinUAE rend visible l’organisation matérielle d’une démo ancienne ; l’œuvre réactive pour un public contemporain un régime perceptif hérité de la micro-informatique.

L’**intelligence artificielle** produit le récit, interprète les anomalies et reçoit une place matériellement mesurable dans l’allocation du calcul. Son coût énergétique n’est plus dissimulé : il devient une variable fictionnelle et esthétique.

> **En déplaçant la dépense computationnelle de l’image vers l’inférence, *Le Désert des tokens* interroge la manière dont l’IA peut servir de médiation contemporaine à une esthétique de la démoscène fondée sur la contrainte, le temps réel, l’incomplétude visuelle et la visibilité de la machine.**
