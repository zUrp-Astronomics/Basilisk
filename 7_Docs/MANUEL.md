# emount-bench — une carte pour piloter un objectif Sony E depuis un PC

**Date** : 2026-10-06
**Dernière révision** : 2026-10-20 (ticket #653 : le README et le manuel de `9_Assets/manual/` sont au projet,
l'affiche et les images de `9_Assets/` à l'org) ; 2026-10-20 (ticket #648 : le firmware en 1.3, le driver INDI en
1.2) ; 2026-10-20 (ticket #651 : le matériel v1.0, `2_Hardware/` et `3_3D-Models/`) ; 2026-10-19 (ticket #645 : le
driver INDI du dépôt est celui d'INDI, `pinefeat_cef`, avec ses
correctifs et notre extension en deux séries de patchs ; démontré en CI) ; 2026-10-19 (ticket #641 : le point de rosée ou de givre, calculé par la carte dans `z`, affiché
par la page de banc et publié par le driver INDI) ; 2026-10-16 (ticket #635 : la page de banc en français ou en anglais, et les navigateurs qui
l'ouvrent) ; 2026-10-16 (ticket #633 : la release porte aussi la page de banc) ; 2026-10-16 (ticket
#630 : le flash en un clic depuis la page produit d'abord, les voies manuelles en secours) ; 2026-10-16 (ticket
#626 : la procédure de flash sur les trois fichiers de la release, dont deux voies qui gardent la marque ; le site
n'installe rien) ; 2026-10-12 (ticket #617 : une version publiée sur sa release GitHub) ;
2026-10-11 (ticket #606 : la carte v1.0 seule, la XIAO de banc retirée ; la version 0.9) ;
2026-10-11 (ticket #602 : le firmware livré sous le nom `basilisk_FW`, l'image unique, les
scripts de flash) ; 2026-10-11 (ticket #594 : le capteur SHT40 lu et rapporté, `z`, `:GT#`, le driver INDI ; la table du
§ Matériel dit ce que le firmware lit) ; 2026-10-11 (ticket #590 : les FAULT des rails dans `DEBUG`) ; 2026-10-10
**Statut** : actif — le manuel de la carte, en français : l'ancien `README.md` du dépôt, déplacé tel quel par le ticket #495 quand le README est devenu la vitrine du projet
**Référencé par** : `README.md` (§ Documentation)
**Dérivé de** : `README.md` avant le ticket #495 (révision du 2026-10-06, commit `6fd8349`)

Une petite carte (la carte v1.0, à ESP32-C3) se branche entre un PC, en USB, et un objectif à monture Sony E, sur ses
contacts. Elle joue le rôle du boîtier : elle met l'objectif sous tension, fait son initialisation, puis le pilote à la demande — mise au point, ouverture, une position mémorisée
(« la marque »). Son usage premier est l'astrophotographie : l'objectif devient un focuser pour INDI/Ekos (Linux) ou pour ASCOM (Windows). Cible première :
**Samyang AF 135 mm F1.8 FE**.

Le PC lui parle par une ligne série : une commande par ligne, une réponse par ligne. Ce protocole est le contrat de la
carte, il est dans **`7_Docs/PROTOCOL.md`**.

## Matériel

Le firmware se construit pour la carte v1.0, seule. Son brochage est dans `4_Firmware/components/phy/pins.h`.

**La carte v1.0** : un ESP32-C3FH4 nu (4 Mo de flash intégrée) ; les dossiers de fabrication sont dans
`1_Board/`. Aucun GPIO n'y reste libre :

| GPIO | Signal | Rôle |
|---|---|---|
| 0 | XDETECT | contact de présence de l'objectif, tiré haut par la carte : 0 = présent |
| 1 | TXD → objectif | UART1, 750 000 bauds |
| 2 | RXD ← objectif | UART1 |
| 3 | LENS_CS | entrée, tirée bas par la carte : l'objectif la lève quand il émet |
| 4 | VD | impulsion basse d'environ 60 µs, 60 Hz |
| 5 | BODY_CS | sortie, tirée bas par la carte : la carte la lève quand elle émet |
| 6 | FAULT_MOTOR | défaut du rail moteur, actif à 0 : lu par le firmware, compté et journalisé (`DEBUG`, `* rail`) |
| 7 | EN_MOTOR | commande du rail moteur 5 V de l'objectif, active à 0 |
| 8 | EN_LOGIC | commande du rail logique 3,3 V de l'objectif, active à 0 |
| 9 | FAULT_LOGIC | défaut du rail logique, actif à 0 : lu par le firmware, compté et journalisé (`DEBUG`, `* rail`) |
| 10 | LED | LED de statut, allumée à l'état haut |
| 18, 19 | USB | la ligne série (USB Serial/JTAG du C3) |
| 20, 21 | SCL, SDA | capteur de température et d'humidité SHT40 : lu par le firmware une fois par seconde (`z`, `:GT#`) |

- Les deux rails de l'objectif sont commutés et limités en courant sur la carte (deux TPS2552), alimentés par l'USB-C :
  rien d'autre à câbler. Au démarrage, des tirages de la carte les tiennent coupés, et le firmware ne les allume qu'à
  la demande de la séquence de démarrage de l'objectif.
- Les contacts de la monture sont dix pointes, P1 à P10.

Et encore :

- Les niveaux de l'objectif sont en 3,3 V : pas d'adaptation.
- **XDETECT doit être câblé** : la carte ne démarre l'objectif que quand ce contact dit « présent » (tenu 300 ms), et
  elle coupe ses rails et met ses lignes au repos 2 ms après sa retombée, s'il est toujours absent. Tant qu'il ne dit pas « présent », rien n'est émis :
  TXD, BODY_CS et VD sont au repos (entrées, tirage bas), les rails coupés.

## Le firmware

Le firmware est dans `4_Firmware/` (ESP-IDF 5.3.2, C, aucune dépendance hors ESP-IDF). `v` rend sa version,
`<majeur>.<mineur>` : celle de `PROJECT_VER`, dans `4_Firmware/CMakeLists.txt`, `1.3` (son mineur fait lire les
bornes aux drivers INDI, § S'en servir avec INDI / Ekos). Elle est indépendante du numéro de la
release, son tag.

### Le récupérer

**Une version publiée** est sur la dernière release GitHub du dépôt,
`https://github.com/zUrp-Astronomics/Basilisk/releases/latest`. Elle porte quatre fichiers :

- `basilisk_<tag>_esp32c3_flash.zip` : à plat, le bootloader (`…-bootloader.bin`), la table de partitions
  (`…-partition-table.bin`), l'application (`….bin`), `flash_args` qui les place, et les scripts `flash.sh` et
  `flash.cmd` ;
- `basilisk_<tag>_esp32c3.bin` : l'image unique, à écrire en `0x0` ;
- `manifest.json` (format ESP Web Tools) : le site de l'org le copie, avec l'image unique, pour le bouton
  d'installation de la page produit ;
- `basilisk_<tag>_bench.html` : la page de banc de cette version (`5_App/emount-bench.html` au tag, § La page de
  banc), à ouvrir dans le navigateur ; la page produit l'ouvrira en ligne, quand le site le publie.

**Pour qui développe**, la CI de la forge construit le firmware à chaque run (job `firmware-build`) et publie
l'artefact **`firmware-esp32c3`** : les mêmes fichiers séparés, `flash_args` et les scripts, plus l'image unique
`…-full.bin`. Les noms portent la version et la cible, par exemple en 1.3 `basilisk_FW-v1.3-esp32c3.bin`. On le
télécharge depuis la page du run, ou par l'API de la forge (`GET /api/v1/repos/fleet/basilisk/actions/artifacts`, puis
`…/artifacts/<id>/zip`).

### Le flasher

**Depuis la page produit, en un clic.** La page produit, `https://zurp-astronomics.github.io/basilisk/`, installe
depuis le navigateur (Firefox, Chrome ou Edge, sur ordinateur) la dernière version publiée, la carte branchée en
USB-C : bouton d'installation, choisir le port de la carte. Elle écrit l'image unique en `0x0` : **la marque est
remise à zéro**.

**Les autres voies**, avec les fichiers de la release. On branche la carte à l'ordinateur en USB-C, avec un câble de
données ; rien d'autre : l'outil de flash fait passer la carte en téléchargement par l'USB. Trois voies, dont deux
gardent la marque :

- **Dans le navigateur, sans rien installer — la marque est gardée.** Firefox 151 ou plus, Chrome ou Edge, sur
  ordinateur ; pas Safari, pas de mobile. Décompresser le zip, ouvrir le flasheur web d'Espressif, esptool-js
  (`https://espressif.github.io/esptool-js/`), **Connect**, choisir le port de la carte, puis ajouter les trois `.bin`
  aux adresses que donne `flash_args` : `0x0` le bootloader, `0x8000` la table de partitions, `0x10000` l'application.
  **Program**. Ne pas cliquer **Erase Flash**.
- **En ligne de commande — la marque est gardée.** Avec esptool (`pip install esptool`, ou l'exécutable autonome
  d'Espressif), depuis le dossier décompressé, celui du zip ou de l'artefact : `sh flash.sh [PORT]` (Linux, macOS),
  `flash.cmd [PORT]` (Windows). Le script écrit les fichiers séparés de `flash_args`, rien d'autre ; sans `PORT`,
  esptool cherche la carte. À la main, c'est :

  ```sh
  python -m esptool --chip esp32c3 -p PORT -b 460800 write_flash "@flash_args"
  ```

- **Installation propre — la marque est effacée.** L'image unique (`basilisk_<tag>_esp32c3.bin` de la release,
  `…-full.bin` de l'artefact), écrite en `0x0` avec esptool-js ou esptool : pour une puce neuve ou qui a porté autre
  chose. Elle remplit la NVS de `0xFF` : la marque est remise à zéro.

`PORT` : `/dev/ttyACM0` sous Linux, `/dev/cu.usbmodem…` sous macOS, `COMx` sous Windows (la carte y apparaît comme
« Périphérique série USB »), sans pilote.

### Le construire soi-même

ESP-IDF 5.3.2 chargé :

```sh
. "$IDF_PATH/export.sh"
bash 4_Firmware/build.sh
```

Le firmware se construit dans `4_Firmware/build/esp32c3` (le `sdkconfig` y est généré, rien de versionné n'est
réécrit) ; l'artefact est dans `4_Firmware/build/esp32c3/artefact/`, et la commande de flash, ci-dessus, marche depuis
ce dossier. La console ESP-IDF est muette : la carte n'écrit rien sur l'USB qui ne soit
pas une réponse.

## Ce que fait la carte

- **Elle ne parle jamais la première.** Aucune bannière, aucune ligne spontanée, sauf après `LOG ON`.
- **Elle démarre l'objectif seule** dès que XDETECT le dit présent : rails, VD, poignée de main, initialisation d'un
  boîtier, identification, homing de l'objectif. `t` suit cette séquence (`boot_state=powering`, `identifying`, …) ;
  pendant elle, `e` répond `y` et les commandes qui ont besoin de l'objectif `er busy boot`.
- **La marque** : une position de mise au point mémorisée **dans la carte** (NVS), par objectif et par focale — rien
  n'est écrit dans l'objectif. `j` la lit, `js` la pose à la position courante, `jg` y va, `jx` l'efface. Le bouton du
  fût fait de même au relâchement : appui de moins d'1 s, aller à la marque ; de 1 à 4 s, la poser ; plus long, rien. À
  la fin de chaque démarrage, la carte revient seule à la marque (`boot_state=restoring`), puis passe en `ready`.
- **Les bornes** de la mise au point (`r`) sont celles que l'objectif publie lui-même, resserrées de 5 pas ; elles ne
  sont ni mesurées ni codées. Une cible hors de ces bornes est refusée (`er range limits`).
- **L'ouverture** : `a` rend la plage, `a<f>` règle l'ouverture, `o` relit celle que l'objectif rapporte. Quand le
  commutateur de l'objectif met sa bague sur l'ouverture, tourner la bague règle l'ouverture, par tiers de diaphragme
  (trois par seconde au plus).
- **L'objectif perdu** (plus de trame pendant 2 s) : la carte refait son démarrage. Après quatre échecs consécutifs, elle
  s'arrête en `boot_state=fault` ; `b` relance, ou débrancher puis rebrancher l'objectif.
- **La LED de statut** dit l'état : un souffle lent sans objectif, un clignotement faible pendant l'initialisation, une
  respiration en `ready`, un motif de mouvement pendant un déplacement, des séries d'éclats en `fault`, dont le nombre
  dit la cause. Les motifs et leurs valeurs sont en tête de `4_Firmware/components/led/led.c`. `k` lit son
  intensité maximale, `k<n>` la règle (0 à 100 %, `k0` l'éteint) ; elle revient à 100 % à chaque démarrage de la carte.
- **La température et l'humidité** : la carte v1.0 lit son capteur SHT40 une fois par seconde ; `z` rend la température
  en °C, l'humidité relative en %RH et le seuil de condensation sur l'optique que la carte en calcule, en °C : le point
  de rosée quand il est positif ou nul, le point de givre quand il est négatif (`21.37 48.02 9.91`, `-3.05 87.50 -4.27`) ;
  le seuil est `-` quand il n'est pas défini (l'humidité à 0, l'air au-dessus de 60 °C, un point de givre sous -65 °C),
  et `z` rend `-` seul quand la mesure est inconnue (pas encore lue, capteur muet, lecture fausse, bus bloqué, dernière
  lecture de plus de 3 s) : jamais un nombre. La carte n'en fait rien d'autre : elle ne compense pas la mise au point,
  c'est Ekos qui décide, et elle n'agit pas sur le seuil, elle le donne. Le capteur est sur la carte, contre
  l'arrière de l'objectif : ce qu'il mesure de l'air dépend du boîtier.
- **`DEBUG`** donne une ligne de diagnostic : la cause du dernier redémarrage de la carte, les lignes du journal perdues,
  le résultat du dernier déplacement ; combien de fois la protection de chaque rail de l'objectif
  s'est déclenchée (`fault_motor`, `fault_logic`, une ligne `* rail` au journal à chaque fois) — la carte le dit et ne
  coupe rien. **`LOG ON`**, **`LOG ALL`**, **`LOG OFF`** ouvrent et ferment le journal (lignes
  `* …`) ; `LOG ALL` y ajoute chaque trame échangée avec l'objectif.

Les lettres servies, leurs réponses et leurs erreurs exactes sont dans `7_Docs/PROTOCOL.md`. Une lettre ou une commande en
majuscules que la carte ne sert pas répond `er nocap`.

## S'en servir avec INDI / Ekos

**Le driver du dépôt, `6_Driver/indi/`** : le driver Pinefeat d'INDI, sous son nom (`indi_pinefeat_cef_focus`, « Pinefeat
CEF » dans un profil Ekos, le périphérique « Pinefeat EF Lens Controller »), avec deux séries de patchs proposables à INDI : des correctifs, puis notre extension. Il se déclare en version 1.2, celle que lui donnent les
correctifs ; l'extension ne la change pas.
Contre la carte, qui annonce l'extension, il ajoute l'ouverture relue, l'arrêt, la marque, la LED de statut, le
redémarrage de l'objectif et son nom, les bornes de l'objectif comme plage de la position, et, sur la carte v1.0, la
température (`FOCUS_TEMPERATURE`, celle qu'Ekos lit pour refocaliser sur un écart de température), l'humidité et le point
de rosée ou de givre que la carte calcule, publiés seulement quand la carte les connaît. Il s'installe à côté du driver
du paquet d'INDI, sans l'écraser. Sa compilation, son installation et ses propriétés : `6_Driver/indi/README.md`.

**Le driver Pinefeat d'origine** (`indi_pinefeat_cef_focus`, livré avec INDI) : la carte le sert aussi, sur la position,
le mouvement et l'ouverture, sans rien configurer sur la carte ; la distance y reste inconnue (`d` rend `-`).

Les deux ne lisent les bornes (`r`) que si le mineur de `v` est au moins 3, la règle d'INDI, que notre driver garde : avec
la version de la carte (`1.3`, choisie pour cela : décision de l'humain), ils les lisent ; sous un mineur 3, ils ne les liraient plus, notre driver
extension comprise.

Dans les deux cas : le port série de la carte (la vitesse est sans effet, c'est un port USB).

Un logiciel qui ne connaît que le protocole Moonlite peut aussi piloter la mise au point (`:GP#`, `:FG#`…,
`PROTOCOL.md` § 4) et lire la température (`:GT#`), qui y vaut 0 °C tant que la carte ne la connaît pas : Moonlite n'a
pas de forme pour « inconnue ».

## S'en servir avec ASCOM (Windows)

Le **driver ASCOM de Pinefeat** (https://github.com/pinefeat/cef135) s'emploie tel quel : la carte sert tout ce qu'il
envoie, `v e r f m c a`, avec les réponses qu'il attend (`PROTOCOL.md` § 2.1). Choisir le port COM de la carte.

## La page de banc (`5_App/`)

`5_App/emount-bench.html` : une page seule, sans installation, qui parle à la carte par Web Serial. L'ouvrir dans **Firefox
151 ou plus, Chrome ou Edge**, sur ordinateur. Elle est en français ou en anglais : la langue du navigateur au départ, puis
la bascule **FR | EN** en haut de la page, retenue d'une visite à l'autre ; la bascule ne coupe pas la connexion, et le
journal garde la langue où chaque ligne a été écrite.
Si le navigateur refuse Web Serial sur `file://` : `python3 -m http.server` dans `5_App/`, puis
`http://localhost:8000/emount-bench.html`.

- **Connecter** : la page envoie `v` jusqu'à une réponse (l'ouverture du port peut redémarrer la carte), puis lit l'état ;
  elle relit ensuite la position et le mouvement 5 fois par seconde, le reste une fois par seconde.
- **Objectif** : identité (`i`), état de la carte (`t`) ; Redémarrer l'objectif (`b`), Alim OFF / ON (`p0` / `p1`).
- **Capteur de la carte** (`z`, une fois par seconde, objectif monté ou non) : la température, l'humidité, et le point de
  rosée, ou de givre quand il est négatif, tels que la carte les calcule ; « — » pour ce qu'elle ne connaît pas.
- **Focus** : ±10 / ±100 pas, curseur entre les bornes avec la marque, « Aller à », Stop (`q`), Marquer (`js`), Aller à
  la marque (`jg`).
- **Ouverture** : curseur au tiers de diaphragme entre les bornes lues (`a`), ±⅓, pleine ouverture ; la valeur affichée
  est celle que l'objectif rapporte (`o`).
- **Journal** : `LOG ON`, `LOG ALL`, `LOG OFF` ; les trames sont décodées ; une ligne de commande libre ; le journal
  s'exporte en fichier texte.

La page ne porte que ce que la carte sert : ni homing (`h`), ni calibration (`c`), ni module, firmware de l'objectif,
focale ou distance (`n`, `w`, `l`, `d`), ni `DUMP`, `SNIFF`, `SDRIVE`.

Toute autre application série convient aussi : une ligne par commande, `PROTOCOL.md`.

## Le commutateur Custom du Samyang AF 135 (`5_App/tool_station/`)

Le Samyang AF 135 F1.8 FE a un commutateur à deux positions, M1 et M2 ; ce que fait chaque position (AF, MF, ou la bague
sur l'ouverture) est une configuration rangée dans **sa flash**. `5_App/tool_station/tool_station.py` la lit et l'écrit
par la carte, sans l'outil de Samyang :

```sh
pip install -r 5_App/tool_station/requirements.txt     # pyserial
python 5_App/tool_station/tool_station.py --read --port /dev/ttyACM0
python 5_App/tool_station/tool_station.py --M1 AF --M2 AP --port /dev/ttyACM0    # le réglage d'usine
python 5_App/tool_station/tool_station.py --man        # le mode d'emploi complet
```

Avant d'écrire, l'outil montre la configuration lue et celle qu'il va écrire, et demande une confirmation ; après, il
relit. **Après une lecture ou une écriture, débrancher puis rebrancher l'objectif.** La carte refuse tout autre objectif
que le 135 (`er nocap custom`). Le format est établi sur le firmware 1.06 de l'objectif ; sur une autre version il n'est
pas vérifié : **l'écriture se fait aux risques de l'utilisateur.** À la console de la carte, les mêmes opérations sont
`CUSTOM READ` et `CUSTOM WRITE <M1><M2>` (`PROTOCOL.md` § 3.1).

## Sécurité : rien n'écrit dans un objectif

Un objectif dont la mémoire est abîmée ne se répare pas. Tout ce que la carte émet vers l'objectif passe par un seul
filtre, `4_Firmware/components/bench_core/bench_core.c`, compilé derrière chaque émission, sans option pour le lever. C'est
une **liste blanche** : ce qu'elle ne liste pas est refusé, dont la mise à jour du firmware de l'objectif (`0x14`, `0x15`)
et toute commande de service qui écrit sa mémoire. Aucune commande ne permet de construire une trame arbitraire.

**Une exception, une seule, décidée par l'humain** : l'écriture de la configuration du commutateur Custom du **Samyang AF
135** (`CUSTOM WRITE`, que sert `tool_station`). Le filtre ne la laisse passer qu'au 135 reconnu, et seulement pour les
neuf valeurs valides ; la même commande vers un autre objectif, un autre Samyang compris, est refusée.

Le canal de service des Samyang (`0x40`) n'est ouvert qu'à un Samyang reconnu, par le nom ou par le code que l'objectif
donne de lui-même.

## Tests (sans carte)

```sh
bash 4_Firmware/run.sh               # le firmware : gcc seul
bash 5_App/run.sh                    # la page de banc : node ; Playwright et Chromium pour test_app.js
bash 5_App/tool_station/run.sh       # tool_station : python3
```

Chaque script se lance depuis n'importe quel répertoire. La suite du firmware joue le vrai code de la carte (tout sauf
ce qui touche au matériel) contre deux faux objectifs, un Samyang AF 135 et un Tamron F051, et contre les traces réelles
du 135 (`4_Firmware/traces`), puis des mutants qui doivent tous la faire échouer. Pour `5_App/test/test_app.js` :
`npm install` à la racine (Playwright épinglé dans `package.json`) et un Chromium sous
`/opt/pw-browsers/chromium-*/chrome-linux/chrome` ou désigné par `CHROMIUM_PATH`. Le driver INDI est compilé et lié par
`bash 6_Driver/indi/build.sh`, et démontré par `bash 6_Driver/indi/test.sh` (libindi 2, astyle, python3 : son README,
§ « Ce que la CI démontre »).

La CI (`.gitea/workflows/ci.yml`) joue ces trois commandes, construit le firmware, et compile, lie et démontre le driver
INDI.

## Arborescence

```
README.md          la vitrine du projet, tenue par le projet (l'org y fait une passe de style)
CLAUDE.md          l'environnement de travail du dépôt : conventions, commandes, pièges (pour qui le modifie)
0_Datasheets/      les datasheets des composants de la carte
1_Board/           la carte v1.0 : schéma, vues, modèle 3D (le STEP, zippé), Gerbers, BoM, placement (son README)
2_Hardware/        la mécanique hors carte : le boîtier v1.0 (un STEP, toutes ses pièces), la nomenclature hors carte (CSV),
                   une capture du boîtier v1.0b ; les caméras compatibles, le remplacement des vis (son README)
3_3D-Models/       le boîtier v1.0 à imprimer (3MF) ; matières, tolérances, état de validation (son README)
4_Firmware/        le firmware (ESP-IDF) ; sim/, les faux objectifs et les tests ; traces/, les traces du 135 ; run.sh,
                   build.sh ; gen_lens_names.py, le générateur de la table des noms d'objectifs
5_App/             la page de banc (emount-bench.html) et ses tests (run.sh)
  tool_station/    le commutateur Custom du Samyang AF 135
6_Driver/indi/     le driver INDI : `pinefeat_cef` d'INDI, ses correctifs et notre extension (LGPL 2.1, son README)
7_Docs/
  PROTOCOL.md      le contrat série de la carte : lettres, réponses, erreurs, journal
  DOCTRINE.md      pourquoi le code est écrit comme il l'est
  MANUEL.md        ce fichier
8_References/      ce que le projet consulte, pas ce qu'il produit (la documentation d'interopérabilité des
                   objectifs ira dans 7_Docs/)
9_Assets/          la fiche (au format du site) et l'affiche lues par le site zUrp, les images du README ; l'affiche
                   et les images sont tenues par l'org
```

## Licence

- **Hardware et conception : OCL v1.1** (Open Community Licence, sans module complémentaire, `LICENSE-HARDWARE`,
  https://github.com/OpenCommunityLicence/OpenCommunityLicence) : `1_Board/`, `2_Hardware/`, `3_3D-Models/`.
- **Tout le reste : GPL-3.0** (`LICENSE`) : `4_Firmware/`, `5_App/`, `7_Docs/`, `9_Assets/`, `.gitea/`, `.github/` et
  les fichiers de la racine.

Exceptions, qui gardent leur licence : `6_Driver/indi/` est sous LGPL 2.1 (`6_Driver/indi/LICENSE`, il dérive du
driver Pinefeat) ; les datasheets de `0_Datasheets/` et les documents de tiers de `8_References/` restent à leurs
auteurs ; les deux tables de noms d'objectifs générées par `4_Firmware/gen_lens_names.py` depuis ExifTool (`Sony.pm`, licence Perl),
`4_Firmware/components/host/lens_names.c` et la table `LENS_NAMES` de `5_App/emount-bench.html` (la table seule, pas
le reste de la page), gardent la licence de leur source.
