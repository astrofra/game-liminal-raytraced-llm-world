# Documentation

Ce dossier sert de memoire de travail du projet.

Il est organise de facon a separer :

- la vision longue et la specification
- les notes de recherche libres
- l'etat technique courant
- les decisions stabilisees
- l'historique des iterations
- les erreurs, limites et risques connus

## Fichiers

- [SPEC.md](./SPEC.md) : vision artistique, cahier des charges, perimetre et roadmap.
- [NOTES.md](./NOTES.md) : notes de recherche, formulations, pistes theoriques et materiau de communication.
- [TECHNICAL_STATE.md](./TECHNICAL_STATE.md) : photographie technique du depot a l'instant present.
- [SCENE_FORMAT_V1.md](./SCENE_FORMAT_V1.md) : format de scene proprietaire v1 actuellement implemente.
- [DECISIONS.md](./DECISIONS.md) : decisions techniques et architecturales actees, avec justification.
- [PROJECT_JOURNAL.md](./PROJECT_JOURNAL.md) : journal chronologique des iterations, essais, corrections et resultats.
- [KNOWN_ISSUES.md](./KNOWN_ISSUES.md) : erreurs, limitations, dettes techniques et points de vigilance.

## Regle de maintenance

A chaque iteration significative du projet, la documentation doit etre mise a jour ainsi :

1. Ajouter une entree dans `PROJECT_JOURNAL.md`.
2. Mettre a jour `TECHNICAL_STATE.md` si des fonctionnalites, fichiers, commandes ou performances changent.
3. Mettre a jour `DECISIONS.md` si un choix devient explicite et structurant.
4. Mettre a jour `KNOWN_ISSUES.md` si un bug, une limite ou un risque apparait, disparait ou change de statut.

## Convention d'ecriture

- Les dates doivent etre absolues au format `YYYY-MM-DD`.
- Les observations factuelles doivent etre separees des intentions ou hypotheses.
- Les performances doivent indiquer le contexte de test quand il est connu.
- Les problemes non resolus doivent rester visibles, meme si une solution provisoire existe.
