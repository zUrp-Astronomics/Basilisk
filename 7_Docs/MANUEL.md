# Basilisk — réalisation et utilisation

[English](MANUAL.md)

## Projet

### Dix-huit millimètres. Démerde-toi avec ça.

La monture Sony E laisse 18 mm entre la baïonnette et le capteur. Basilisk y case le fût, les contacts, le contrôleur et le port USB, en gardant de la place pour la caméra astro. Une idée discutable. Un objet très satisfaisant.

Il joue le boîtier photo pour un objectif E-mount : alimentation, mise au point et diaphragme avec un seul câble USB-C. Le pilotage passe par Ekos/INDI, ASCOM ou le banc web de la page produit. L'objectif fournit ses limites de mise au point ; la carte mémorise une position de référence. Aucun moteur à sangler au fût, aucune courroie à tendre.

### Ce que vous construisez

Un adaptateur matériel ouvert autour d'un ESP32-C3, d'une carte en anneau, d'un boîtier imprimé et du fût métallique d'une bague Meike. Le projet part de Pinefeat ; la carte, le firmware et les fichiers de fabrication sont dans ce dépôt.

Le boîtier est conçu pour les caméras planétaires Player One hexagonales non refroidies : Mars-C, Mars-C II, Mars-M II, Mars 662M ; Neptune-C, Neptune-M, Neptune-C II, Neptune 664C ; Uranus-C et Uranus-M ; Saturn-C SQR et Saturn-M SQR. Les modèles refroidis sortent de cette conception. L'adaptation aux ZWO et ToupTek n'est pas établie.

Le travail au banc documenté ici porte sur un Samyang AF 135 F1.8 FE et un Sony FE 24–105 mm F4 G OSS. La [référence E-mount](../7_Docs/E-Mount/README.md) distingue les observations et les déductions concernant les autres objectifs. Décrire un protocole ne certifie pas une compatibilité.

### Des pièces aux étoiles

[Assemblage](#assemblage) couvre la carte, les pièces imprimées et l'interface caméra. [Installation](#installation) relie le firmware à vos logiciels. [Utilisation](#utilisation) couvre le focus, le diaphragme, la marque et le diagnostic.

Les sections suivantes détaillent aussi la conception et le travail hors ligne. Les fichiers sont là pour construire, examiner et adapter. Le résultat compte ; l'atelier est le prix d'entrée.

## Assemblage

### Les pièces sur l'établi

| Pièce | Source |
|---|---|
| Carte Basilisk v1.0 et composants | [Dossier de fabrication](../1_Board/README.md) |
| Boîtier, parties haute et basse | [3MF du boîtier](../3_3D-Models/Basilisk_v1.0_enclosure.3mf) |
| Entourage USB | [3MF USB](../3_3D-Models/Basilisk_v1.0_USB.3mf) |
| Fût métallique mâle, vis et loquet | Un demi-kit de bagues Meike MK-S-AF3A 10/16 mm |
| Fixation de la caméra | Trois vis M2.5×12 |
| Caméra | Un modèle indiqué dans [La bête](#projet) |

La [nomenclature mécanique](../2_Hardware/Basilisk_v1.0_BoM.csv) est distincte de celle de la carte. L'[assemblage STEP](../2_Hardware/Basilisk_v1.0_enclosure.step) contient les pièces du boîtier à adapter.

### Commander la carte

Utiliser les Gerber, BoM et fichiers de placement du dossier de fabrication. Les paramètres sont **2 couches, épaisseur 1,0 mm, cuivre 1 oz, masque noir, sérigraphie blanche, HASL RoHS**. L'assemblage se fait sur la face supérieure uniquement. P1–P10 sont dix pogo pins individuels ; CN2 n'est pas monté comme connecteur séparé.

L'assemblage JLCPCB est la voie documentée. À la main, il faut souder un microcontrôleur QFN, des passifs 0402 et de petits composants de puissance à pad exposé.

### Imprimer et ajuster

Pour les parties structurelles haute et basse, **le PC-CF est le matériau utilisé et validé sur les pièces du projet**. Le PETG est une alternative envisagée ; ABS et ASA n'ont pas été essayés. Ne pas utiliser de PLA pour ces pièces structurelles. L'entourage USB est en PETG transparent pour laisser voir la LED.

Vérifier l'ajustement du loquet avec votre matériau et votre imprimante : une reprise de quelques dixièmes de millimètre peut être nécessaire. Les trois vis M2.5×12 remplacent les M2.5×8 de la plaque avant de la caméra ; elles compensent la hauteur ajoutée et maintiennent des filets en prise.

### État de l'assemblage

L'assemblage final v1.0 avec les 3MF fournis n'a pas encore été vérifié et aucune séquence de montage pas à pas n'est validée. Les fichiers et indications de matériaux sont disponibles ; il manque encore ce dernier contrôle d'ajustement et ses photographies pour constituer un guide de reproduction complet. Aucun couple, épaisseur de cale ou ordre de serrage n'est inventé pour combler ce manque.

![Boîtier Basilisk, rendu CAO de la précédente révision v1.0b](../9_Assets/basilisk-enclosure.webp)

L'illustration montre la **v1.0b**, révision précédente ; utiliser les fichiers v1.0 liés ci-dessus.

Après vérification du montage, passer à l'[Installation](#installation).

## Installation

### Firmware et première connexion

La [page produit](https://zurp-astronomics.github.io/basilisk/) propose le programmateur web avec l'image complète de la release : **cette installation efface la marque de mise au point enregistrée dans la carte**. Pour une mise à jour qui la conserve, utiliser le paquet `_flash.zip` de la release et son script `flash.sh` ou `flash.cmd` ; voir [Le nécessaire hors ligne](#le-nécessaire-hors-ligne).

Ouvrir le **banc** de la page produit, sélectionner le port USB de la carte et se connecter. Banc et firmware proviennent de la même release. Monter un objectif et attendre l'état prêt. Son identité, ses bornes de focus et sa plage d'ouverture doivent apparaître avant de commander un mouvement. Une seule application peut utiliser le port série : déconnecter le banc avant d'ouvrir le logiciel astro.

Pour une nuit sans réseau, télécharger auparavant le fichier `_bench.html` de la release : c'est le banc autonome. Si l'accès depuis un fichier local est refusé, servir son dossier avec `python3 -m http.server` et l'ouvrir depuis `http://localhost:8000/`. Le navigateur doit fournir Web Serial.

### Ekos / INDI

Sélectionner **Pinefeat CEF** comme focuser et choisir le port série de la carte. Le pilote INDI standard gère le focus et le diaphragme. Celui du dépôt ajoute la marque, la relecture de l'ouverture, l'identité et le redémarrage de l'objectif, la luminosité de la LED, les bornes de focus rapportées et les propriétés du capteur.

Le pilote fourni nécessite **libindi 2**. Depuis la racine du dépôt :

```sh
cmake -S 6_Driver/indi -B build/indi
cmake --build build/indi
sudo cmake --install build/indi
```

Installer auparavant les outils de compilation C/C++, CMake, pkg-config et le paquet de développement libindi. L'installation va par défaut dans `/usr/local` ; conserver ce préfixe pour laisser intact le pilote de la distribution. `command -v indi_pinefeat_cef_focus` indique le binaire trouvé. Le périphérique INDI conserve le nom amont **Pinefeat EF Lens Controller**. Le réglage du débit série n'a pas d'effet sur l'USB natif.

La [référence du pilote](../6_Driver/indi/README.md) détaille ses propriétés, sa provenance et ses séries de correctifs.

### ASCOM et autres clients

Utiliser le [pilote ASCOM Pinefeat](https://github.com/pinefeat/cef135) et choisir le port COM de la carte. Basilisk implémente son contrat de commandes USB.

Les clients Moonlite peuvent piloter le focus et lire la température. Ce protocole rend **0 °C quand la température est inconnue** ; le banc et l'extension INDI Basilisk peuvent représenter explicitement cette absence. Pour un autre client, suivre le [contrat USB](../7_Docs/PROTOCOL.md).

### Commutateur Custom du Samyang AF 135

Pour ce modèle reconnu, la [tool station](../5_App/tool_station/) permet de lire et régler les affectations M1/M2 : AF, MF ou commande du diaphragme par la bague (AP). Ce réglage réside **dans la flash de l'objectif**, contrairement à la marque de Basilisk.

La section [Configuration M1/M2 facultative](#configuration-m1m2-facultative) donne les commandes et le périmètre. L'outil demande confirmation avant d'écrire et relit le réglage. **Couper puis remettre l'alimentation de l'objectif après une lecture comme après une écriture.** Cette configuration est facultative ; le pilotage normal du focus et du diaphragme n'en a pas besoin.

## Utilisation

### Brancher, laisser démarrer

Monter l'objectif et brancher l'USB-C. Basilisk l'alimente, l'identifie, lui laisse faire son référencement puis revient à la marque enregistrée, s'il y en a une. Les commandes qui nécessitent l'objectif répondent occupé pendant le démarrage. Retirer l'objectif coupe son alimentation.

Piloter le focus depuis le banc ou le logiciel astro. La plage autorisée vient de l'objectif, avec une marge de cinq pas à chaque extrémité. Il n'y a pas de calibration manuelle. Une consigne hors plage est refusée. Régler le diaphragme dans la plage annoncée ; la valeur affichée est relue sur l'objectif.

### Garder une marque de focus

La marque appartient à **la carte**, associée à l'identité de l'objectif et à sa focale nominale arrondie au millimètre. Elle conserve une position et son sens d'approche. La poser à la position à retrouver ; y revenir depuis le banc ou le pilote INDI fourni. Un démarrage normal la rappelle automatiquement, avec le sens d'approche mémorisé pour reprendre le jeu mécanique.

Sur un objectif dont le bouton de fût est pris en charge, l'action est décidée au relâchement :

| Durée de l'appui | Action |
|---|---|
| Moins d'une seconde | Aller à la marque |
| Au moins 1, moins de 4 secondes | Poser la marque ici |
| 4 secondes ou plus | Aucune action |

Le bouton est utilisé uniquement à l'état prêt, sans mouvement en cours. La marque de la carte est distincte d'une éventuelle fonction de preset native de l'objectif.

Lors d'une récupération après perte de l'objectif ou redémarrage à chaud de la carte, Basilisk tente de retrouver la cible du mouvement interrompu, sinon la dernière position connue, pour le même objectif à la même focale. Cette information reste en mémoire, sans écriture flash ; couper l'alimentation de la carte l'efface. La récupération peut déplacer et référencer à nouveau l'objectif. Elle ne garantit pas une continuité physique du focus.

### Bague et diaphragme

Quand l'objectif annonce que sa bague commande le diaphragme, Basilisk traduit sa rotation en tiers de diaphragme, au maximum trois par seconde. Sur le Samyang AF 135, ce rôle dépend de l'affectation M1/M2 ; voir [Installation](#installation).

### Lire la LED

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

### Température et humidité

Le SHT40 rapporte la température locale et l'humidité relative une fois par seconde. La carte calcule aussi un point de rosée ou de givre. **Le couplage thermique du capteur à l'air, à l'objectif et à l'électronique n'est pas caractérisé** : ces valeurs ne mesurent pas la condensation sur les optiques.

Une mesure absente, invalide ou vieille de plus de trois secondes est indisponible dans le banc et l'extension INDI Basilisk. Le seuil calculé est aussi indisponible à humidité nulle, au-dessus de 60 °C ou sous un point de givre de −65 °C. Moonlite a le repli distinct décrit dans [Installation](#installation).

Le firmware rapporte ces valeurs ; il ne compense pas le focus et ne commande pas de chauffage. Ekos peut exploiter la propriété de température selon sa propre politique de refocalisation.

### Quand la session déraille

Si aucun des deux flux d'état n'arrive pendant deux secondes, la carte tente de redémarrer l'objectif. Elle s'arrête après quatre tentatives consécutives en échec. Relancer l'objectif depuis le banc ou le pilote INDI fourni, ou le retirer puis le remonter.

Pour un signalement exploitable, ouvrir le journal du banc, activer `LOG ON` (`LOG ALL` pour toutes les trames), reproduire l'événement et exporter le texte. Joindre `DEBUG`, la version du firmware de la carte et l'identité de l'objectif. Un défaut d'alimentation est compté dans le diagnostic ; le seul silence de la carte n'en donne pas la cause. Elle n'émet les événements du journal qu'après activation.

Le [contrat USB](../7_Docs/PROTOCOL.md) définit chaque commande, réponse et erreur. Les sections suivantes ajoutent le fonctionnement hors ligne et les choix de conception.

## Le nécessaire hors ligne

Télécharger les fichiers depuis les [releases du dépôt](https://github.com/zUrp-Astronomics/basilisk/releases). Le `_bench.html` ouvre les commandes autonomes ; aucune copie du projet Astro n’est nécessaire.

Avant de partir, conserver le `_flash.zip` et le `_bench.html` de la release, ce dépôt et le pilote choisi sur l'ordinateur. Le site est un point d'entrée pratique ; le fonctionnement de l'appareil n'en dépend pas.

Extraire le paquet de flash dans un dossier et lancer `bash flash.sh /dev/ttyACM0` sous Linux, ou `flash.cmd COM3` sous Windows, avec le port réel de la carte. Ces scripts écrivent les images séparées du bootloader, de la table de partitions et de l'application décrites par `flash_args`, en conservant la NVS. L'image fusionnée `_esp32c3.bin` du programmateur web écrit depuis l'adresse zéro et remet la NVS à zéro. Garder ensemble les fichiers d'une même release. Les scripts nécessitent Python et esptool ; suivre leur contrôle de dépendances.

Le tag de release et la réponse `v` sont deux identifiants distincts : la release nomme le paquet livré, `v` rend la version majeure/mineure du firmware de la carte. Relever les deux pour signaler un problème. Télécharger le banc courant ne met pas à jour la carte.

## Configuration M1/M2 facultative

Depuis la racine du dépôt :

```sh
pip install -r 5_App/tool_station/requirements.txt
python 5_App/tool_station/tool_station.py --read --port /dev/ttyACM0
python 5_App/tool_station/tool_station.py --M1 AF --M2 AP --port /dev/ttyACM0
python 5_App/tool_station/tool_station.py --man
```

Utiliser le port série réel. `AF`, `MF` et `AP` affectent les rôles autofocus, focus manuel et diaphragme par la bague. L'outil lit la valeur actuelle, demande confirmation avant d'écrire et relit le résultat. Il s'agit de l'écriture explicitement admise dans la mémoire non volatile du Samyang AF 135 reconnu. Le format est établi sur le firmware objectif 1.06 ; la carte ne vérifie pas cette version. L'association de M1 à la position haute du commutateur vient de l'analyse, pas d'une vérification au banc. Contrôler le résultat physique après remise sous tension ; l'affectation d'usine est M1 = AF, M2 = AP. Couper et remettre l'alimentation de l'objectif après une lecture également.

## Conception

### Encombrement et mécanique

Le tirage de la monture E est de 18 mm. La carte en anneau mesure 50,87 × 36,3 mm, avec les composants sur une seule face. Les pièces imprimées positionnent électronique et caméra autour de la monture métallique et du loquet récupérés sur la bague Meike. L'[assemblage STEP](../2_Hardware/Basilisk_v1.0_enclosure.step) est la référence dimensionnelle ; l'illustration v1.0b ne dessine pas les pièces finales v1.0. Une plaque avant ressemblante ne démontre pas l'adaptation à une autre caméra.

### Alimentation et signaux

L'USB-C apporte l'alimentation et les données USB natives. L'objectif reçoit deux rails commutés séparément, moteur 5 V et logique 3,3 V, avec limitation de courant et signalement des défauts. Dix pogo pins assurent les contacts de monture. La détection de présence commande la session ; le retrait coupe les rails. Le [schéma](../1_Board/Basilisk-v1.0_1-Schematics.pdf) définit l'électronique ; la [référence E-mount](E-Mount/protocol.md) donne les contacts, temporisations et trames.

### Repérage de la carte v1.0

| GPIO | Signal |
|---|---|
| 0 | XDETECT |
| 1 / 2 | TXD / RXD |
| 3 / 5 | LENS_CS / BODY_CS |
| 4 | VD |
| 6 / 9 | FAULT_MOTOR / FAULT_LOGIC |
| 7 / 8 | EN_MOTOR / EN_LOGIC |
| 10 | LED |
| 18 / 19 | USB |
| 20 / 21 | SCL / SDA |

La [table de broches](../4_Firmware/components/phy/pins.h) et le schéma définissent le câblage : détection, commandes de rails et défauts actifs à zéro ; LED active à un. Aucun GPIO libre. VD cadence à 60 Hz avec une impulsion basse d’environ 60 µs. Présence confirmée pendant 300 ms avant démarrage ; retrait confirmé pendant 2 ms avant coupure et mise au repos des lignes.

### Deux protocoles, un appareil

Le PC parle le [contrat USB](PROTOCOL.md), en lignes de texte. L'objectif parle le [protocole de monture](E-Mount/protocol.md), binaire. Un acquittement USB peut annoncer l'admission d'une commande ; il ne prouve pas à lui seul l'arrivée du focus. Résultat du mouvement et relecture sont des observations distinctes. La liste blanche d'émission limite les messages qui peuvent atteindre l'objectif ; la mise au point normale n'a pas besoin de commandes de calibration usine ou de mise à jour du firmware objectif.

La marque de la carte réside en NVS. L'état de récupération à chaud est distinct et ne survit pas à la coupure d'alimentation de la carte. L'affectation Custom facultative appartient à l'objectif. Ces trois durées de vie comptent pour choisir une méthode de mise à jour ou interpréter un redémarrage.

### Où vit la connaissance du code

La [doctrine](DOCTRINE.md) expose les principes de conception. Les contrats, propriétaires des données et contraintes matérielles appartiennent aux en-têtes et commentaires proches du [firmware](../4_Firmware/). Le manuel décrit les interfaces et la réalisation ; ce n'est pas une deuxième spécification d'implémentation. Mesures, simulations et analyses statiques ont des portées distinctes, précisées dans le [rapport de confiance](CONFIANCE.md) et la [provenance E-mount](E-Mount/provenance.md).

## Construire le firmware

Utiliser ESP-IDF **5.3.2** pour l'unique cible prise en charge, **ESP32-C3**. Depuis la racine du dépôt, avec `IDF_PATH` pointant vers le SDK installé :

```sh
. "$IDF_PATH/export.sh"
bash 4_Firmware/build.sh
```

Les résultats sont dans `4_Firmware/build/esp32c3/artefact/` : application, bootloader, table de partitions, image fusionnée, `flash_args`, `flash.sh` et `flash.cmd`. Le build vérifie que ses arguments de flash ordinaires ne recouvrent pas la NVS. Compiler n'est pas nécessaire pour utiliser les binaires d'une release.

Pour le développement, [SUITES.md](SUITES.md) est la référence maintenue des vérifications et prérequis. Les tests firmware utilisent le compilateur hôte et Python ; ceux du banc utilisent Node et l'installation Playwright épinglée du dépôt ; la tool station a son propre point d'entrée. La construction et la vérification des correctifs INDI nécessitent libindi 2 et les outils de [son README](../6_Driver/indi/README.md). Un essai contre un objectif simulé établit un comportement face à ce modèle, pas une nouvelle compatibilité matérielle.
