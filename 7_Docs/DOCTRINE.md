# Doctrine

[English](DOCTRINE.en.md)

Le critère : à trois heures du matin dans un champ, le firmware doit tenir son rôle sans réclamer son auteur. L'effort porte sur ce qui rend le comportement prévisible, jusque dans la perte de liaison, l'échec de stockage et l'arrêt d'un mouvement.

## Écrire ce qui doit rester vrai

Avant de modifier le code, énoncer l'invariant, son propriétaire et les événements qui peuvent le rendre caduc. Une valeur dérivée doit avoir une source identifiée et une durée de validité explicite. Une frontière doit établir ce qu'elle garantit à ses consommateurs ; empiler les mêmes contrôles ne remplace pas ce contrat.

Les représentations doivent empêcher les combinaisons d'états contradictoires autant que le langage le permet. Une donnée absente ou périmée n'est pas une mesure plausible. Une émission incertaine n'est pas un refus certain. L'admission d'un mouvement n'est pas son arrivée.

## Payer la simplicité

Supprimer, puis simplifier, puis raccourcir. Une abstraction attend son deuxième cas concret ; une dépendance doit justifier le code qu'elle fait entrer. Le firmware dépend d'ESP-IDF ; les outils de développement ont leurs propres dépendances.

Une garde défend un cas nommé et démontrable. Des tests qui passent après sa suppression ne suffisent pas à prouver qu'elle ne sert à rien : ils peuvent manquer le cas. Une complexité née d'un incident réel porte une connaissance à conserver avant de la refondre.

## Tenir les frontières

Le contrat USB est [PROTOCOL.md](PROTOCOL.md). La liste blanche d'émission de `bench_core` est la dernière barrière avant l'objectif ; elle ne devient pas une option de diagnostic. L'exception Custom du Samyang AF 135 est explicite, bornée et documentée. Un canal de lecture ne doit pas promettre l'absence universelle d'effets persistants d'un firmware propriétaire.

Les contrats internes vivent près du code : durée de vie, appels permis, origine des valeurs, contexte matériel. Les commentaires expliquent les décisions que les instructions seules ne donnent pas. Ils ne racontent pas la session de l'agent.

## Vérifier sans gonfler la preuve

Une transformation cohérente, des vérifications adaptées, des résultats lisibles. Les suites, simulations et mutations cherchent les défauts et les angles morts ; elles ne remplacent pas une mesure matérielle. Une analyse statique privée ne devient pas une observation parce qu'un simulateur la reproduit.

Un défaut connu reste nommé jusqu'à son traitement. Les rapports disent sur quelle révision ils portent. L'exigence d'ingénierie se lit dans ces contrats et leurs vérifications ; elle n'a besoin ni de comparaison héroïque ni de label inventé.
