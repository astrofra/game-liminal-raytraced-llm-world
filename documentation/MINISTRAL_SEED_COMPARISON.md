# Reprises ciblées sur les graines 43 et 44

Chaque reprise couvre huit cas isolés : les cinq branches génératives, `ambiguity-007`, `thermal-004` et `resources-009`. Les budgets, exécutables et fixtures restent ceux de BJ/C/D. Les sessions longues ne sont pas rejouées ici. La graine 42 figure dans la [matrice principale](MINISTRAL_NATIVE_COMPARISON.md).

Les 48 reprises passent les assertions automatisées. Sur les quinze créations de salle des trois graines, les métadonnées viennent du modèle dans 12 cas pour Q4, 14 pour Q5 et 8 pour Reasoning. Ces nombres ne constituent pas une validation du récit : avec Reasoning/graine 44, `resources-009` remonte arbitrairement l'oxygène de `critical` à `stable`, et `generation-002` introduit de l'anglais dans la narration française. Voir la revue du [bilan de qualification](MINISTRAL_QUALIFICATION_REPORT.md).

Les assertions mécaniques ne constituent pas un score artistique. Les latences ci-dessous portent sur les tours dont le processus a réussi, y compris leurs replis éventuels. Les échecs sont comptés séparément. Les anciens binaires n'exposent pas les métriques complètes des auxiliaires.

Les scores ci-dessous couvrent les positions attendues, liens persistants, barrières, scènes sauvegardées, retours des branches et absence de balises de réflexion dans l'état. Ils ne valident pas automatiquement la justification d'un changement de ressource, les valeurs thermiques annoncées ou la cohérence narrative ; ces points nécessitent une revue séparée des sauvegardes et du texte.

| Variante | Cas sans erreur mécanique | Tours de session sans erreur | Échecs natifs | Sans réparation/repli observé | p50 / p95 succès | Pic VRAM échantillonné |
|---|---:|---:|---:|---:|---:|---:|
| ministral-8b-q4-s43 | 8/8 | 0/0 | 0 | 7 | 18.21 / 39.18 s | 6411 MiB |
| ministral-8b-q4-s44 | 8/8 | 0/0 | 0 | 7 | 17.14 / 39.58 s | 6378 MiB |
| ministral-8b-q5-s43 | 8/8 | 0/0 | 0 | 7 | 20.68 / 45.59 s | 7132 MiB |
| ministral-8b-q5-s44 | 8/8 | 0/0 | 0 | 7 | 20.10 / 44.43 s | 7269 MiB |
| ministral-8b-reasoning-q4-s43 | 8/8 | 0/0 | 0 | 6 | 20.44 / 37.26 s | 6484 MiB |
| ministral-8b-reasoning-q4-s44 | 8/8 | 0/0 | 0 | 6 | 20.28 / 41.12 s | 6512 MiB |

Les commandes cardinales connues sont majoritairement déterministes et peuvent ne solliciter que l'auxiliaire thermique. Pour isoler le coût d'une nouvelle salle :

| Variante | Branches sans erreur mécanique | Métadonnées modèle / repli | Génération de salle : p50 / p95 du tour complet |
|---|---:|---:|---:|
| ministral-8b-q4-s43 | 5/5 | 4 / 1 | 17.48 / 22.17 s |
| ministral-8b-q4-s44 | 5/5 | 5 / 0 | 15.77 / 18.22 s |
| ministral-8b-q5-s43 | 5/5 | 4 / 1 | 18.34 / 24.04 s |
| ministral-8b-q5-s44 | 5/5 | 5 / 0 | 18.39 / 20.48 s |
| ministral-8b-reasoning-q4-s43 | 5/5 | 3 / 2 | 19.85 / 20.62 s |
| ministral-8b-reasoning-q4-s44 | 5/5 | 4 / 1 | 19.37 / 21.05 s |

## ministral-8b-q4-s43

Preuves : `C:\works\duo-runtime\liminal-ministral-20261024\repeat-ministral-8b-q4-seed43`.

Arrêts des appels : `{'eos': 12}`. Offload observé : `['35/35']`.


## ministral-8b-q4-s44

Preuves : `C:\works\duo-runtime\liminal-ministral-20261024\repeat-ministral-8b-q4-seed44`.

Arrêts des appels : `{'eos': 10, 'token_limit': 1}`. Offload observé : `['35/35']`.


## ministral-8b-q5-s43

Preuves : `C:\works\duo-runtime\liminal-ministral-20261024\repeat-ministral-8b-q5-seed43`.

Arrêts des appels : `{'eos': 11, 'token_limit': 1}`. Offload observé : `['35/35']`.


## ministral-8b-q5-s44

Preuves : `C:\works\duo-runtime\liminal-ministral-20261024\repeat-ministral-8b-q5-seed44`.

Arrêts des appels : `{'eos': 10, 'token_limit': 1}`. Offload observé : `['35/35']`.


## ministral-8b-reasoning-q4-s43

Preuves : `C:\works\duo-runtime\liminal-ministral-20261024\repeat-ministral-8b-reasoning-q4-seed43`.

Arrêts des appels : `{'eos': 14}`. Offload observé : `['35/35']`.


## ministral-8b-reasoning-q4-s44

Preuves : `C:\works\duo-runtime\liminal-ministral-20261024\repeat-ministral-8b-reasoning-q4-seed44`.

Arrêts des appels : `{'eos': 14}`. Offload observé : `['35/35']`.

