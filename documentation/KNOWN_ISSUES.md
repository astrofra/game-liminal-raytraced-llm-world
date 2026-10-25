# Limites connues — runtime autonome

- Le premier appel vérifie et charge le GGUF ; son coût ne représente pas les réponses à chaud.
- La validité d'une scène et la visibilité de la caméra ne garantissent pas sa qualité artistique.
- Les détecteurs de dérive linguistique sont heuristiques ; les essais des six langues ne prouvent pas l'absence de toute dérive future.
- L'annulation reste coopérative : le moteur attend le retour des opérations de chargement/décodage en cours.
- Le paquet est testé sur la machine de développement avec environnement réduit ; un Windows vierge constitue une qualification distincte.
- Les anciens résultats ne sont pas transposables au nouveau runtime ; voir l'[historique de recherche](HISTORIQUE_RECHERCHE.md).

Mesures, essais réussis et échecs observés dans le [bilan courant](BILAN_RUNTIME_AUTONOME.md).
