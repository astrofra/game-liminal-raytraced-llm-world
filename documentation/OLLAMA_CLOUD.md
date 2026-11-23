# Ollama Cloud

Le mode **local reste le défaut**. Pour jouer en cloud, `OLLAMA_API_KEY` doit
être définie dans l'environnement du processus qui lance le jeu. Lancer ensuite
`play_within_the_latent_walls_cloud.bat`. Si la clé manque, ce lanceur s'arrête
avant toute construction, sans en afficher la valeur.

```powershell
# Local, également choisi en l'absence d'argument :
build/Release/liminal_cornell_renderer.exe --llm-backend local

# Cloud, avec OLLAMA_API_KEY déjà définie dans l'environnement :
build/Release/liminal_cornell_renderer.exe --llm-backend ollama-cloud

# Modèle cloud explicitement choisi :
build/Release/liminal_cornell_renderer.exe --llm-backend ollama-cloud --cloud-model gemma4:31b
```

Le lanceur original conserve son comportement. Les deux utilisent le même
exécutable et transmettent les arguments supplémentaires. Le lanceur cloud
impose son backend après ces arguments. Le choix est fixe pour la partie ;
aucune bascule automatique ni substitution de modèle.

## Gameplay

Les règles, l'interprétation des formulations libres par le LLM, les objets,
les indices, les sorties, la chauffe, le rendu, les langues et l'audio restent
ceux du moteur existant. Il valide toujours les propositions avant de publier
l'état. Une panne cloud annule le résultat en préparation : la commande peut
être retentée depuis le même état.

Le modèle distant par défaut est **DeepSeek V4.1 Flash** (`deepseek-v4.1-flash`),
contre **Gemma 4 E4B** local. `--cloud-model gemma4:31b` permet de retrouver le
modèle du premier pilote cloud. Ce choix runtime est indépendant du duo de développement.
Les textes et les mondes générés peuvent donc différer. Le mode cloud reste
**expérimental** : une réponse valide structurellement peut encore être refusée
par les validations narratives du jeu et remplacée par leur texte de secours.

Les passes littéraires liées à la chaleur gardent leurs consignes de progression,
mais leur température d'échantillonnage cloud est plafonnée à 1 pour préserver
le JSON et la langue. Si leur contenu est invalide ou tronqué, la narration
préparatoire validée est conservée : cela ne bloque plus le déplacement ou le
transfert d'objet. Les propositions d'état, les erreurs réseau, l'annulation et
les délais restent stricts. Le comportement d'échantillonnage local est inchangé.

## Réseau, validation et archives

- HTTPS direct vers `https://ollama.com/api/chat`, sans serveur Ollama local.
  Les prompts et le contexte sélectionné sont transmis au fournisseur.
  Certificats vérifiés ; redirections interdites.
- Authentification par `OLLAMA_API_KEY` uniquement. Aucun fichier de clé,
  argument secret ou en-tête d'authentification dans les archives.
- Schémas transmis dans les prompts, puis validation C++ des champs, types,
  bornes et variantes. Au maximum une correction structurelle dans le délai
  du même appel ; aucun nouvel essai automatique sur erreur de transport.
  La correction reçoit le candidat rejeté et utilise une température au plus
  égale à 0,2. Les alternatives de schéma refusées précisent les champs et
  valeurs en cause. Une enveloppe Markdown JSON complète peut être retirée avant la
  validation stricte ; les commentaires autour du JSON restent refusés.
- Les réponses structurées attendent cette validation avant leur aperçu.
  Les réponses ordinaires arrivent progressivement. Échap annule localement ;
  le fournisseur peut avoir déjà traité la requête.
- Estimation conservatrice du contexte par octets UTF-8 et overhead,
  plafond de 32 768 par défaut avec réserve de sortie, sans GGUF.
  `--llm-context` permet un plafond explicite. Le comptage local reste exact.
  Le quota de mémoire récente cloud est une cible : après retrait des notes et
  événements anciens, le dernier événement utile peut le dépasser si la requête
  complète respecte le plafond total. Une observation ne bloque donc pas la
  sortie suivante à cause du seul quota de mémoire (correctif DUO-075).
  Un tour cloud est limité à 180 secondes, 24 tentatives et 32 768 tokens
  de sortie. Chaque appel conserve son délai et autorise au plus deux budgets
  de sortie lorsqu'une correction est nécessaire.
- Archives SDL : messages exacts envoyés, chaque tentative, réponses,
  décisions, usages disponibles, états, mixage et captures. Le template
  serveur, sa révision immuable et ses réglages effectifs ne sont pas exposés
  par le fournisseur : aucune identité ou déterminisme inventé.

L'installateur reste inchangé et continue de préparer le modèle local.
L'usage cloud dépend du compte Ollama et de ses conditions de facturation.

## Qualification du 23 novembre 2026

Après les échecs de fin de partie, DeepSeek a réussi trois requêtes archivées
problématiques dès le premier essai et trois déplacements complets depuis les
états utilisateur exacts. Un parcours SDL de 28 commandes, dont 11 déplacements,
monte jusqu'à 43 °C : aucun échec technique sur 55 appels cloud, dépôt puis
reprise par pronom fonctionnels. Une narration a utilisé le secours sémantique
du moteur. Les 18 CTest passent. Voir la [revue DUO-076](duo/reviews/DUO-076.md)
pour le protocole de comparaison et les limites de cet échantillon.

Qualification initiale du pilote Gemma :

Accès réel à `gemma4:31b`, sortie JSON et streaming vérifiés. Partie SDL de sept
commandes avec inventaire, dialogue, dépôt/reprise par pronom, nouvelle pièce,
retour et carnet : états et captures complets. Une correction JSON a réussi ;
deux narrations ont utilisé le secours existant. Un parcours complémentaire a
vérifié cristal, indices, dépôt/reprise et direction fermée.

Clé absente, HTTP 401 réel, annulation et timeout vérifiés ; l'échec 401
conserve exactement l'état dans SDL. Le parcours cloud a utilisé un chemin
GGUF inexistant. Le parcours local sans clé conserve CUDA et le modèle résident.

Voir la [revue détaillée](duo/reviews/DUO-074.md) pour les tests et limites.
Ce n'est pas le benchmark statistique complet CLOUD-05 de la
[roadmap](ROADMAP_OLLAMA_CLOUD.md).
