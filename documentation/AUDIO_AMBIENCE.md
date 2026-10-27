# Ambiances : extraction, mixage et recette — 27 octobre 2026

Le pilote de la [roadmap](roadmap-features-audio-mixage-llm.md) est raccordé au
parcours local. Les quatre OGG restent inchangés. Le catalogue est livré dans
`assets/sfx/ambience_features.json` ; aucune dépendance Python ou FFmpeg n'est
nécessaire au joueur.

## Extraction et annotations

```powershell
python -m pip install -r scripts/requirements-audio.txt
python scripts/analyze_ambience.py --input assets/sfx --output assets/sfx/ambience_features.json
# Recalcul complet, même si le cache est valide :
python scripts/analyze_ambience.py --input assets/sfx --output assets/sfx/ambience_features.json --force
```

FFmpeg et ffprobe doivent être dans le PATH. Versions observées : Python 3.12.5,
NumPy 2.0.2, SciPy 1.14.1, FFmpeg/ffprobe 7.0.2 essentials Gyan.
Les versions exactes, la configuration complète, son SHA-256, les SHA des sources
et la version du script sont dans le catalogue. La version du catalogue est
l'empreinte de son contenu canonique, hors champ `catalog_version`.
`--config` et `--annotations` permettent de fournir les deux fichiers explicitement.

L'analyse couvre la totalité des boucles, à fréquence native pour les niveaux,
puis à 48 kHz pour le spectre (`resample_poly`, Hann périodique 2048, pas 512,
FFT par blocs de 64 trames). La dernière trame est complétée par des zéros,
mais son RMS utilise le nombre d'échantillons réellement présents. Le spectre
agrège la puissance des canaux ; une stéréo en opposition ne disparaît pas.
Les fréquences au-delà du Nyquist source sont exclues. Le bin Nyquist appartient
à la dernière bande disponible. La somme des parts disponibles vaut 1 à
l'arrondi publié près. La flatness utilise un plancher relatif à la puissance
moyenne, et non un plancher absolu dépendant du volume. Les trames sous le seuil
de silence sont exclues des statistiques spectrales. Les conventions, unités
et raisons des valeurs nulles restent dans les données.

Le RMS est exprimé en dBFS, la crête échantillonnée en dBFS, la crête vraie en
dBTP et la sonie intégrée en LUFS. `loudnorm` fournit ses mesures **d'entrée** ;
aucun son normalisé n'est écrit. Les valeurs non applicables sont `null`, jamais
NaN/Infinity. Le cache est invalidé par une modification du fichier, de la
politique, du script ou des versions d'outils. Les annotations sont toujours
relues. La publication passe par un fichier temporaire relu/validé puis remplacé
atomiquement ; un échec conserve le catalogue précédent. La sortie ne peut
écraser ni une source ni un fichier éditorial.

`ambience_analysis_config.json` expose les seuils absolus des fiches. Ces fiches
ne déduisent ni source ni émotion des noms `blade`, `bug`, `night`, `static`.
`ambience_annotations.json` est séparé et n'est jamais réécrit par l'extracteur :

```json
{
  "schema_version": 1,
  "tracks": {
    "blade": {
      "text": "Description écrite et validée à l'écoute par l'auteur.",
      "provenance": "human",
      "trim_db": 0.0,
      "trim_validated": false
    }
  }
}
```

Cet exemple ne décrit pas la piste réelle. Le fichier livré ne contient aucune
annotation. Un trim non nul nécessite `trim_validated: true`, dans [-24, +6] dB ;
le pilote conserve 0 dB sur chaque piste, avec `max_gain: 1`. Une nouvelle
extraction répercute les annotations et trims dans le catalogue.

## Runtime

`stb_vorbis` v1.22 est vendorisé, sans modification, à la révision
`003501f6d7a8668ce8b6d875e34dd97ad0d5d5b5`. Provenance, SHA-256 et double licence
MIT/domaine public sont sous `vendor/stb/`. La licence est incluse dans le
packaging Windows. Aucun SDL_mixer, FFmpeg, service ou DLL de décodeur n'est ajouté.

Le catalogue est chargé une fois depuis les ressources de l'exécutable.
Au démarrage audio, les SHA des OGG sont vérifiés ; les fichiers sont décodés
entièrement en flottants, convertis en stéréo 48 kHz par SDL3 et conservés en RAM
(environ 40 Mio de PCM pour les quatre sources). Limites du pilote : mono/stéréo,
8–96 kHz source, 120 secondes et 16 Mio encodés par piste. Une ressource absente,
une empreinte incohérente ou un périphérique indisponible désactive la lecture
avec diagnostic ; le jeu reste utilisable.

Chaque piste boucle continuellement, y compris à gain nul. Le seuil initial
possède un mix pré-scripté : blade=0,1, bug=0, night=0,2, static=0,3. Dès
l'écran de choix de langue, le lecteur monte depuis zéro vers ces cibles,
sans commande ni appel LLM. Elles sont bornées par les plafonds du catalogue et
archivées avec la provenance `scripted_quarry_threshold`. Ce réglage de départ
reste à qualifier artistiquement ; il ne constitue pas une annotation humaine
des pistes. Les prochains tours narratifs peuvent librement modifier le mix,
y compris le rendre silencieux ; le démarrage ne se réapplique pas aux revisites.

Recette du correctif du 27 octobre 2026 : binaire recompilé et publié, 15 CTest
réussis, dont démarrage non silencieux et conservation d'un silence explicite
après un tour et une sauvegarde. Lancement SDL réel sans sélectionner de langue
ni saisir de commande : 346 080 trames produites, cibles atteintes après la rampe,
aucune erreur de callback et fermeture normale, zéro tour. Preuves hors dépôt :
`C:/works/duo-runs/game-liminal-raytraced-llm-world/AUDIO-START-01/`.
Cette recette vérifie l'alimentation du périphérique, pas une écoute humaine.

Une rampe linéaire de cinq secondes est calculée par échantillon. Une nouvelle
cible part du gain atteint ; une cible identique ne relance pas la rampe.
`--audio-ramp-seconds 0..60` règle cette durée. Le callback utilise un tampon
fixe de 1024 trames, sans décodage, inférence, fichiers ni allocation applicative.
SDL sérialise les instantanés et les cibles avec le verrou du flux audio.

Le gain global est `min(0.25, 0.8 / somme(pics_PCM × trims × gains_max))`.
Les pics sont vérifiés au chargement, après décodage et conversion. Même la somme
cohérente des quatre pics garde 20 % de marge dans le domaine des échantillons.
Un garde final borne les échantillons à ±0,98. Ce calcul ne prétend pas garantir
une même sonie perçue entre pistes ni mesurer la crête analogique reconstruite.

Le prompt narratif existant reçoit les quatre fiches, les annotations distinctes,
les cibles acceptées et, en SDL, les gains atteints au lancement de la commande.
Le schéma exige les quatre nombres de `audio_mix`. Leur somme n'est pas contrainte.
La validation indépendante vérifie IDs, types, valeurs finies et bornes ; aucune
intention sonore ne crée un fait de jeu. Un champ absent/invalide ou une narration
de secours conserve le dernier mix. L'audio ne rejette pas une proposition
mécanique recevable. Aucun appel LLM n'est ajouté. Les raccourcis d'inventaire
conservent le mix sans inférence.

Les cibles restent dans le résultat préparé jusqu'à validation du tour. Le
frontend ne les transmet au flux qu'après publication de la vue et de l'état.
Une annulation ou un échec de rendu conserve donc le mix en cours. Les archives
de tour enregistrent catalogue, demande, validation et cibles retenues ;
`audio-events.jsonl` conserve démarrage, changement de cible, positions de lecture,
horloge audio, gain global, état du callback et fermeture. Les instantanés de
session contiennent les cibles, sans ajouter de reprise de partie dans SDL.

## Diagnostic et écoute

```powershell
build_native/Release/liminal_audio_diagnostic.exe
# Entrer quatre gains : blade bug night static. Exemple : 1 0 0 0
# Une autre ligne change les cibles sans redémarrer. q quitte.
build_native/Release/liminal_audio_diagnostic.exe --ramp 2
build_native/Release/liminal_audio_diagnostic.exe --seconds 10 --ramp 5
build_native/Release/liminal_audio_diagnostic.exe --wav C:/preuves/transitions.wav
```

L'export WAV de 48 secondes enchaîne les quatre pistes, les solos, un retargetage
en cours de rampe, le maximum commun et le silence. Il est calculé par le même
cœur de mixage que SDL, sans modèle. Les métriques imprimées permettent de
contrôler les curseurs, les gains atteints et les erreurs de callback.

## Résultats observés et limites

- Build Windows réussi et binaire publié dans `build/Release/`, utilisé par le
  lanceur. Les 15 tests CTest et les 114 tests Python (110 scripts + 4 tests)
  passent. Packaging vérifié par ses tests de contenu/licence ; aucun nouveau
  Setup fabriqué pour ce lot.
- Tests synthétiques : silence, sinus, bruit, impulsions, changement d'amplitude,
  stéréo opposée/corrélée, sources à 44,1/8/4 kHz, flatness indépendante du gain,
  jonction de même RMS mais de signe opposé, cache et atomicité.
- Les extractions normale, avec cache et forcée donnent les mêmes octets.
  Les SHA des OGG restent inchangés. Une copie réellement corrompue est rejetée
  par ffprobe et le catalogue précédent reste intact.
- Tests C++ : IDs et gains invalides, limites par piste, indépendance du résultat
  mécanique, rampes interrompues, horloge indépendante des blocs, curseurs à gain
  nul, somme cohérente avec dépassement flottant de source et trims +6 dB.
- Tests de tour : prompt et schéma, un seul appel narratif, publication différée,
  maintien du mix sur absence/erreur/secours, annulation après proposition valide,
  inventaire direct et sérialisation des cibles.
- Lecture sur le périphérique SDL réel : quatre pistes au maximum, puis
  retargetages durant une rampe ; aucun échec de callback observé. L'export de
  48 secondes culmine à 0,366493 avec le gain global 0,235404.
- Pilote audio SDL `dummy`, 52 secondes : 2 481 152 trames, les quatre curseurs
  ont rebouclé, aucun échec de callback. Un pilote volontairement indisponible
  laisse démarrer le binaire publié et consulter l'inventaire, sortie normale 0,
  événement d'indisponibilité archivé.
- Partie SDL réelle de 45 secondes, commande française « regarder tout autour » :
  Gemma a proposé blade=0,1, bug=0, night=0,2, static=0,3. Narration acceptée,
  5148 tokens de prompt, 275 tokens de sortie ; cible atteinte, boucles non
  redémarrées, 2 160 480 trames fournies sans erreur de callback.
- Parcours réel Gemma avec le binaire publié : nord vers un nouveau lieu, sud
  vers le seuil, puis inventaire. Trois tours réussis sans secours narratif ;
  un lieu généré conservé, nouvelles cibles audio acceptées aux arrivées et
  conservées par l'inventaire. Le mix final vaut 0,2 / 0,1 / 0,3 / 0,4.

Ces observations ne valent pas validation artistique à l'écoute par l'auteur.
Les quatre pistes sont mono et leurs niveaux diffèrent (RMS environ -10,55,
-17,98, -15,73 et -18,31 dBFS). Les discontinuités premier/dernier échantillon
mesurées sont environ -0,0053, +0,0343, +0,0082 et +0,1006. Les jonctions, notamment
`static`, restent à écouter ; aucune correction automatique des OGG ni validation
humaine fictive n'est appliquée. Les seuils de vocabulaire sont testés sur signaux
de référence, mais leur adéquation sensible aux boucles reste à qualifier.

Preuves locales hors dépôt : `C:/works/duo-runs/game-liminal-raytraced-llm-world/`
(`AUDIO-SDL-01/`, `audio-transitions.wav`, `audio-retarget.txt`, journaux de build).
Le parcours découverte/retour est dans `audio-journey.txt` et
`audio-journey-state.json` ; le récapitulatif de hashes/tests est dans
`audio-validation-summary.json`.
Voir les [revues DUO-042](duo/reviews/DUO-042.md) et
[DUO-043](duo/reviews/DUO-043.md) pour l'attribution et les réserves des livraisons.

Références d'implémentation : [stb_vorbis](https://github.com/nothings/stb/blob/003501f6d7a8668ce8b6d875e34dd97ad0d5d5b5/stb_vorbis.c),
[flux SDL](https://wiki.libsdl.org/SDL3/SDL_OpenAudioDeviceStream),
[conversion complète PCM](https://wiki.libsdl.org/SDL3/SDL_ConvertAudioSamples),
[synchronisation SDL](https://wiki.libsdl.org/SDL3/SDL_LockAudioStream).
