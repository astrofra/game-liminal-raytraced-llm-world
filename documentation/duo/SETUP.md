# Duo Codex / Copilot CLI / Ollama — Windows

Déploiement du 24 octobre 2026 pour **Within the Latent Walls**.

Codex prépare les contrats, orchestre les lots et valide les livraisons. Copilot CLI exécute les petits lots via `deepseek-v4.1-flash:cloud`, routé par Ollama local. Ce dispositif de développement est indépendant de llama.cpp et des modèles utilisés par le jeu.

## Implantation

| Élément | Chemin / valeur |
|---|---|
| Consignes persistantes | `AGENTS.md` à la racine du projet |
| Kit externe | `C:/works/projects/ai-tooling/codex-copilot-ollama-windows/duo-codex-copilot-ollama` |
| Configuration locale sans secret | `config.json` dans le kit |
| Exécutable Copilot natif | `C:/Users/fra/AppData/Local/Microsoft/WinGet/Links/copilot.exe` |
| Endpoint | `http://127.0.0.1:11434/v1` |
| Protocole | `responses` |
| Modèle | `deepseek-v4.1-flash:cloud` |
| Limites du kit | 600 secondes ; 128000 tokens de contexte ; 16000 tokens de sortie |
| Contrats et revues | `documentation/duo/tasks/` et `documentation/duo/reviews/` |
| Worktrees | `C:/works/duo-worktrees/game-liminal-raytraced-llm-world/` |
| Journaux et profils privés | `C:/works/duo-runs/game-liminal-raytraced-llm-world/` |

Le kit reste externe : ne pas copier les profils et journaux dans le dépôt. Pour une autre machine, adapter ces chemins et installer les prérequis selon le guide Windows du kit.

## Versions vérifiées

- Python 3.12.5 et Git 2.46.0.windows.1.
- Copilot CLI 1.0.103, installé avec WinGet ; PowerShell 7.6.8 installé comme dépendance.
- Ollama 0.40.10 déjà installé ; référence cloud ajoutée avec `ollama pull deepseek-v4.1-flash:cloud`.
- Les 45 tests du kit passent sous Windows.

Le premier appel a reçu HTTP 401. `ollama signin`, puis la validation par l'utilisateur dans son navigateur, ont connecté cette machine. Aucune clé API n'a été demandée, copiée ou enregistrée dans le projet. La validation d'une connexion navigateur à ollama.com seule ne remplace pas cette connexion du client local.

## Diagnostic

Depuis PowerShell :

```powershell
Set-Location 'C:/works/projects/ai-tooling/codex-copilot-ollama-windows/duo-codex-copilot-ollama'
python scripts/duo.py doctor
python tests/test_duo.py
```

`doctor` vérifie les prérequis sans inférence ; le résultat d'un lot réel vérifie l'accès cloud et l'utilisation des outils. En cas de déconnexion du compte, utiliser `ollama signin` et valider dans le navigateur. Ne pas mettre de clé dans `config.json`.

## Lancer un nouveau lot

1. Codex écrit un contrat basé sur `templates/TASK.md` du kit et choisit un commit de base. Les changements non commités du checkout principal ne figurent pas dans ce commit : en tenir compte explicitement.
2. Codex crée une nouvelle branche et un worktree propre avec `git worktree add -b <branche> <worktree> <commit>`. Il crée aussi les dossiers parents vides des fichiers attendus : l'outil `create` de Copilot 1.0.103 ne les crée pas. Les dossiers vides ne salissent pas le statut Git. Le contrat exige un arrêt en cas d'échec d'outil, sans contournement.
3. Depuis le kit, Codex lance la commande ci-dessous avec des chemins nouveaux pour chaque lot. `--allow-file` est répétable, sans glob.
4. Codex examine `summary.json`, les événements ciblés, le contenu des nouveaux fichiers et le diff. La livraison reste « À vérifier » jusqu'à cette revue.
5. Codex consigne la revue, intègre sélectivement les fichiers acceptés et met à jour [PROGRESS.md](PROGRESS.md).

```powershell
# Exemple historique : ne pas réutiliser ce dossier --out, déjà réservé à la recette.
python scripts/duo.py run --worktree C:/works/duo-worktrees/game-liminal-raytraced-llm-world/DUO-002 --branch codex/duo-002 --task C:/works/projects/game-liminal-raytraced-llm-world/documentation/duo/tasks/DUO-002.md --out C:/works/duo-runs/game-liminal-raytraced-llm-world/DUO-002-01 --allow-file documentation/duo/DUO_DEMO.md
```

Pour une configuration alternative : `python scripts/duo.py --config C:/chemin/config.json doctor`. L'option globale précède la sous-commande.

## Portée de la recette

Le premier lot demande seulement une note de cinq lignes sur les rôles du duo. Il vérifie le trajet réel Codex → Copilot CLI → Ollama → DeepSeek, l'écriture d'un fichier autorisé et la revue. Il ne valide pas la qualité de futures modifications C++, les performances du modèle ou le comportement du jeu.

Recette **validée** avec DUO-002 : modèle effectif exact, un appel `create`, un appel `view`, un seul fichier livré, aucun échec d'outil et aucun écart. Voir la [revue](reviews/DUO-002.md) et la [note livrée](DUO_DEMO.md). La note conserve volontairement son statut de livraison « À vérifier » ; la décision finale appartient à la revue.

Les restrictions du kit limitent les outils à `view`, `glob`, `grep`, `create`, `edit`. Un worktree et `--allow-file` ne constituent pas un sandbox du système d'exploitation. `COPILOT_OFFLINE` coupe les services GitHub du CLI ; l'inférence DeepSeek reste cloud.

## Références

- Kit : `README.md`, `docs/WINDOWS.md`, `docs/WORKFLOW.md`, `docs/VALIDATION.md` dans son dossier externe.
- [Installation officielle Copilot CLI](https://docs.github.com/en/copilot/how-tos/copilot-cli/set-up-copilot-cli/install-copilot-cli).
- [Fournisseurs personnalisés Copilot CLI](https://docs.github.com/en/copilot/how-tos/copilot-cli/customize-copilot/use-byok-models).
- [Connexion Ollama Cloud](https://docs.ollama.com/cloud).
