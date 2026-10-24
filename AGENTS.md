# Consignes du projet

## Projet

- Lire `README.md` et `documentation/TECHNICAL_STATE.md` avant un changement produit.
- Le jeu utilise C++17, CMake, SDL3 et llama.cpp. Le moteur conserve l'autorité sur l'état et la navigation.
- Préserver les modifications locales et indexées existantes. Ne pas modifier les dépendances vendored pour un lot documentaire.
- Le duo de développement est distinct du runtime LLM du jeu ; sa configuration ne change pas les modèles du jeu.

## Duo Codex + Copilot CLI

- Kit Windows : `C:/works/projects/ai-tooling/codex-copilot-ollama-windows/duo-codex-copilot-ollama`.
- Lire `docs/WORKFLOW.md` et `docs/WINDOWS.md` dans ce kit avant une délégation.
- Codex coordonne : contrats, interfaces, mémoire, sauvegardes, sémantique, code généré, builds, revue finale et intégration.
- Les lots bornés et à faible risque sont délégués à DeepSeek via Copilot CLI/Ollama, après écriture d'un contrat TASK.
- Depuis le dossier du kit, utiliser uniquement l'interface fournie : `python scripts/duo.py doctor` et `python scripts/duo.py run ...`. Voir `documentation/duo/SETUP.md` pour les chemins du projet.
- Le risque appartient au travail, pas à l'exécutant. Les décisions transverses restent à Codex.
- L'exécutant travaille uniquement dans le worktree isolé et sur la branche du contrat. Aucun shell, git, MCP ou outil web ; outils limités à `view`, `glob`, `grep`, `create`, `edit`.
- Chaque lot précise les lectures, les fichiers exacts autorisés en écriture et les critères de livraison. `--allow-file` est un audit après exécution, pas un confinement du système.
- Codex crée les dossiers parents vides des nouveaux fichiers avant le run : l'outil `create` du CLI ne les crée pas. Le contrat demande d'arrêter et de rapporter un échec d'outil, sans contournement hors contrat.
- Aucune intégration automatique. Une livraison reste « À vérifier » jusqu'à la revue du contenu, des chemins d'outils et des changements par Codex.
- Codex effectue les tests appropriés puis l'intégration sélective. Ne pas revendiquer un résultat non observé.
- Les commits sont faits par Codex seulement, selon l'autorisation du dépôt. Jamais de push implicite ; aucune autorisation héritée d'un autre projet.
- Ne pas modifier `.vscode/settings.json` ni les profils utilisateur. Aucun secret, profil ou journal d'exécution dans le kit ou le dépôt.
- Le suffixe `:cloud` utilise Internet ; `COPILOT_OFFLINE` désactive les services GitHub du CLI, pas le cloud Ollama.
- En cas de correction : nouveau contrat, nouveau worktree et nouveau dossier de preuves ; deux tentatives maximum avant révision du contrat.
- Si le CLI ou le modèle est indisponible, consigner le blocage et poursuivre la préparation ; ne jamais simuler une livraison DeepSeek.

## Suivi

- Contrats : `documentation/duo/tasks/`. Revues : `documentation/duo/reviews/`.
- Mettre à jour `documentation/duo/PROGRESS.md` après chaque lot.
- Les journaux et profils restent hors dépôt, dans `C:/works/duo-runs/game-liminal-raytraced-llm-world/`.
