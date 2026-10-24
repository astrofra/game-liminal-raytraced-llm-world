# Exécution de la roadmap Ministral

Début : 24 octobre 2026. Mandat : dérouler la roadmap en duo, à matériel constant (RTX 4060 8 Go). Référence : [étude](ETUDE_MISE_A_JOUR_MINISTRAL_LLAMA_CPP.md).

| Étape | État | Critère de sortie |
|---|---|---|
| Référence et retour arrière | Préservés et reconstruits | Ancien exécutable historique conservé ; A et B reconstruits avec MSVC 19.44 et sources applicatives strictement identiques (empreintes comparées) ; zéro écart du vendor ancien |
| Profils Q4/Q5/Reasoning | Vérifiés — DUO-005 | Trois poids contrôlés en taille et SHA-256 ; téléchargements fixés ; 57 tests Python |
| Qualification llama.cpp v0.7.0 | Retenue, vendor intégré | A/B comparés à sources et toolset constants ; 3 796 fichiers v0.7 vérifiés et 173 obsolètes retirés ; anciens builds préservés |
| Intégration native Jinja et canaux | Protocole et campagnes terminés | 11 tests natifs réussis, dont annulation pendant chargement et réflexion ; Q4/Q5/Reasoning chargés réellement ; 35 couches GPU ; garde fragmentée et chemins UTF-8 validés ; 7 rendus canoniques identiques |
| Validation Eryx et sessions | Terminée — corpus DUO-006 | 611 tours exécutés au total ; Q4 adapté 38/40 et 73/73, écarts de navigation identiques à la référence ; défauts métier recensés séparément |
| Comparaison Q5 / Reasoning | Terminée avec reprises et calibration | Q5 interrompt deux tours ; Reasoning varie selon la graine et le budget ; 70 scènes directes, lecture visuelle et limites explicites dans le bilan |
| Décision et distribution | Terminées localement, Q4 conservé | Build publié et `.bat` vérifiés ; installeur 2.2.0, téléchargement réel, lancement SDL, tour LLM, sauvegarde, retour à l'ancien runtime et désinstallation réussis ; sources et SHA-256 livrés |

Les sorties de la campagne Gemma sont conservées. Aucun changement du modèle par défaut n'est déduit de la seule réussite d'une compilation ou de la syntaxe des scènes.

La roadmap de migration est close. La piste 3B optionnelle n'est pas activée ; les défauts métier recensés sont des travaux distincts. La recette de distribution porte sur une installation neuve sur l'hôte Windows 11 actuel, avec `PATH` isolé. Signature et essais sur Windows vierge restent requis avant diffusion publique.

Les sept passes CLI et quatorze campagnes natives sont terminées et comparées dans le [bilan](MINISTRAL_QUALIFICATION_REPORT.md). La matrice principale distingue A ancien runtime reconstruit, B v0.7 sans adaptation, BJ Q4/Jinja, C Q5/Jinja et D Reasoning. Q5 arrête sa dernière session au tour 17/28 ; les onze tours suivants ne sont pas comptés comme exécutés. Les mesures ont été séquentielles, sans compilation concurrente. Les reprises sur deux graines, les quatre contrôles de navigation Reasoning et la calibration de cinq branches à 2 048 tokens sont conservés séparément.

Preuves de travail : `C:/works/duo-runtime/liminal-ministral-20261024/`. Les timings de la première passe diagnostique `eryx-A-baseline` ne sont pas comparables (compilation simultanée et commandes accentuées mal encodées). La campagne finale utilise des fichiers UTF-8 et des fixtures initiales exportées par le moteur.

La machine dispose de Visual Studio 2026, alors que CUDA 13.0 refuse son compilateur MSVC 19.51. La construction utilise une copie locale MSVC 19.44 extraite des paquets Microsoft du catalogue installé, avec SHA-256 vérifiés et provenance conservée dans `msvc143/provenance.json`. Aucun contrôle de version CUDA désactivé. L'installation système, tentée, a demandé une élévation et n'a pas modifié Visual Studio. `scripts/build_native.py` sait sélectionner ce toolset via le fichier ignoré `build_settings.local.json`.
