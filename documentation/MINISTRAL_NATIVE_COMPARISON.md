# Qualification native Eryx : mesures

A : ancien runtime/Q4 ; B : v0.7/Q4 sans adaptation applicative ; BJ : v0.7/Jinja/Q4 ; C : v0.7/Jinja/Q5 ; D : v0.7/Jinja/Reasoning. Les budgets sont de 768 tokens finaux et, pour D, 512 tokens de réflexion.

Les dénominateurs comptent les tours réellement exécutés. C termine les sessions de 15 et 30 tours, puis s'arrête au tour 17/28 de la session mixte : 62 tours exécutés sur 73 prévus, dont 61 réussis. Les 11 tours suivants n'ont pas été exécutés. Ses deux échecs sont des arrêts contrôlés pour budget de réponse atteint, sans sauvegarde ; la comparaison de ses percentiles avec les campagnes complètes est donc limitée.

Les assertions mécaniques ne constituent pas un score artistique. Les latences ci-dessous portent sur les tours dont le processus a réussi, y compris leurs replis éventuels. Les échecs sont comptés séparément. Les anciens binaires n'exposent pas les métriques complètes des auxiliaires.

Les scores ci-dessous couvrent les positions attendues, liens persistants, barrières, scènes sauvegardées, retours des branches et absence de balises de réflexion dans l'état. Ils ne valident pas automatiquement la justification d'un changement de ressource, les valeurs thermiques annoncées ou la cohérence narrative ; ces points nécessitent une revue séparée des sauvegardes et du texte.

| Variante | Cas sans erreur mécanique | Tours de session sans erreur | Échecs natifs | Sans réparation/repli observé | p50 / p95 succès | Pic VRAM échantillonné |
|---|---:|---:|---:|---:|---:|---:|
| A | 38/40 | 73/73 | 0 | 85 | 4.83 / 41.27 s | 7086 MiB |
| B | 38/40 | 73/73 | 0 | 92 | 5.74 / 42.02 s | 7207 MiB |
| BJ | 38/40 | 73/73 | 0 | 100 | 5.04 / 53.30 s | 6408 MiB |
| C | 37/40 | 61/62 | 2 | 97 | 5.63 / 47.37 s | 7149 MiB |
| D | 40/40 | 73/73 | 0 | 105 | 4.91 / 38.00 s | 6394 MiB |

Les commandes cardinales connues sont majoritairement déterministes et peuvent ne solliciter que l'auxiliaire thermique. Pour isoler le coût d'une nouvelle salle :

| Variante | Branches sans erreur mécanique | Métadonnées modèle / repli | Génération de salle : p50 / p95 du tour complet |
|---|---:|---:|---:|
| A | 5/5 | 4 / 1 | 16.12 / 21.44 s |
| B | 5/5 | 5 / 0 | 16.25 / 16.84 s |
| BJ | 5/5 | 3 / 2 | 17.16 / 21.59 s |
| C | 5/5 | 5 / 0 | 18.10 / 20.28 s |
| D | 5/5 | 1 / 4 | 22.34 / 22.40 s |

## A

Preuves : `C:\works\duo-runtime\liminal-ministral-20261024\eryx-A-utf8`.

Arrêts des appels : `{}`. Offload observé : `[]`.

- `barrier-003` : Barrier contact incremented move_count (code 0).
- `nonreciprocal-002` : location: expected scanner_station, got prospect_shelter (code 0).

## B

Preuves : `C:\works\duo-runtime\liminal-ministral-20261024\eryx-B-v070-legacy`.

Arrêts des appels : `{}`. Offload observé : `[]`.

- `barrier-003` : Barrier contact incremented move_count (code 0).
- `nonreciprocal-002` : location: expected scanner_station, got prospect_shelter (code 0).

## BJ

Preuves : `C:\works\duo-runtime\liminal-ministral-20261024\eryx-BJ-v070-jinja`.

Arrêts des appels : `{'eos': 147, 'token_limit': 15}`. Offload observé : `['35/35']`.

- `barrier-003` : Barrier contact incremented move_count (code 0).
- `nonreciprocal-002` : location: expected scanner_station, got prospect_shelter (code 0).

## C

Preuves : `C:\works\duo-runtime\liminal-ministral-20261024\eryx-C-v070-q5`.

Arrêts des appels : `{'eos': 120, 'token_limit': 17}`. Offload observé : `['35/35']`.

- `barrier-003` : Barrier contact incremented move_count (code 0).
- `nonreciprocal-002` : location: expected scanner_station, got prospect_shelter (code 0).
- `ambiguity-008` : Native turn failed or did not save (code 1).
- `session-003-mixed-ambiguity-resources-language-17` : Native turn failed or did not save (code 1).

## D

Preuves : `C:\works\duo-runtime\liminal-ministral-20261024\eryx-D-v070-reasoning`.

Arrêts des appels : `{'eos': 138, 'final_limit': 5, 'reasoning_limit': 18}`. Offload observé : `['35/35']`.

