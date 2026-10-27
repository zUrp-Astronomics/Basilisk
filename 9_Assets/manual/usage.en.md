---
date: 2026-10-26
status: publication candidate
---

## Brancher, laisser démarrer

Monter l'objectif et brancher l'USB-C. Basilisk l'alimente, l'identifie, lui laisse faire son référencement puis revient à la marque enregistrée, s'il y en a une. Les commandes qui nécessitent l'objectif répondent occupé pendant le démarrage. Retirer l'objectif coupe son alimentation.

Piloter le focus depuis le banc ou le logiciel astro. La plage autorisée vient de l'objectif, avec une marge de cinq pas à chaque extrémité. Il n'y a pas de calibration manuelle. Une consigne hors plage est refusée. Régler le diaphragme dans la plage annoncée ; la valeur affichée est relue sur l'objectif.

## Garder une marque de focus

La marque appartient à **la carte**, associée à l'identité de l'objectif et à sa focale nominale arrondie au millimètre. Elle conserve une position et son sens d'approche. La poser à la position à retrouver ; y revenir depuis le banc ou le pilote INDI fourni. Un démarrage normal la rappelle automatiquement, avec le sens d'approche mémorisé pour reprendre le jeu mécanique.

Sur un objectif dont le bouton de fût est pris en charge, l'action est décidée au relâchement :

| Durée de l'appui | Action |
|---|---|
| Moins d'une seconde | Aller à la marque |
| Au moins 1, moins de 4 secondes | Poser la marque ici |
| 4 secondes ou plus | Aucune action |

Le bouton est utilisé uniquement à l'état prêt, sans mouvement en cours. La marque de la carte est distincte d'une éventuelle fonction de preset native de l'objectif.

Lors d'une récupération après perte de l'objectif ou redémarrage à chaud de la carte, Basilisk tente de retrouver la cible du mouvement interrompu, sinon la dernière position connue, pour le même objectif à la même focale. Cette information reste en mémoire, sans écriture flash ; couper l'alimentation de la carte l'efface. La récupération peut déplacer et référencer à nouveau l'objectif. Elle ne garantit pas une continuité physique du focus.

## Bague et diaphragme

Quand l'objectif annonce que sa bague commande le diaphragme, Basilisk traduit sa rotation en tiers de diaphragme, au maximum trois par seconde. Sur le Samyang AF 135, ce rôle dépend de l'affectation M1/M2 ; voir [Installation](setup.fr.md).

## Lire la LED

| Motif | Signification |
|---|---|
| Fixe pendant 1 s à la mise sous tension | Carte alimentée |
| Gonflement doux toutes les 10 s | Pas d'objectif |
| Un éclair long, puis clignotement lent et faible | Objectif détecté, démarrage |
| Montée rapide, puis respiration lente | Prêt |
| Scintillement rapide, puis fixe pendant 1 s | Retour à la marque, puis arrivée |
| Impulsions courtes et rapides / longues et lentes | Position de focus croissante / décroissante |
| Un éclair bref | Diaphragme modifié |
| Trois éclairs / extinction progressive | Marque posée / effacée |
| Double éclair toutes les 2 s | Nouvelle tentative après échec du démarrage ou perte de l'objectif |
| Groupes répétés de 1 / 3 éclairs, séparés par une pause | Arrêt après perte de l'objectif / échec du référencement |

La luminosité se règle de 0 à 100 % ; zéro éteint la LED. La carte repart à 100 % après mise sous tension. Le pilote INDI fourni peut conserver le réglage dans sa configuration et le renvoyer à la connexion.

## Température et humidité

Le SHT40 rapporte la température locale et l'humidité relative une fois par seconde. La carte calcule aussi un point de rosée ou de givre. **Le couplage thermique du capteur à l'air, à l'objectif et à l'électronique n'est pas caractérisé** : ces valeurs ne mesurent pas la condensation sur les optiques.

Une mesure absente, invalide ou vieille de plus de trois secondes est indisponible dans le banc et l'extension INDI Basilisk. Le seuil calculé est aussi indisponible à humidité nulle, au-dessus de 60 °C ou sous un point de givre de −65 °C. Moonlite a le repli distinct décrit dans [Installation](setup.fr.md).

Le firmware rapporte ces valeurs ; il ne compense pas le focus et ne commande pas de chauffage. Ekos peut exploiter la propriété de température selon sa propre politique de refocalisation.

## Quand la session déraille

Si aucun des deux flux d'état n'arrive pendant deux secondes, la carte tente de redémarrer l'objectif. Elle s'arrête après quatre tentatives consécutives en échec. Relancer l'objectif depuis le banc ou le pilote INDI fourni, ou le retirer puis le remonter.

Pour un signalement exploitable, ouvrir le journal du banc, activer `LOG ON` (`LOG ALL` pour toutes les trames), reproduire l'événement et exporter le texte. Joindre `DEBUG`, la version du firmware de la carte et l'identité de l'objectif. Un défaut d'alimentation est compté dans le diagnostic ; le seul silence de la carte n'en donne pas la cause. Elle n'émet les événements du journal qu'après activation.

Le [contrat USB](../../7_Docs/PROTOCOL.md) définit chaque commande, réponse et erreur. Le [manuel du dépôt](../../7_Docs/MANUEL.md) ajoute le fonctionnement hors ligne et les choix de conception.
