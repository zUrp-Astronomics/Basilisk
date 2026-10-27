# emount-bench — l'environnement de travail du dépôt

Carte de bord pilotant des objectifs Sony E-mount pour l'astrophotographie, exposée à Ekos/INDI : la
carte v1.0 (ESP32-C3FH4 nu). Cible n° 1 : **Samyang AF 135 F1.8 FE**.

Ce fichier ne change que si une convention, une commande, un piège ou une règle en vigueur change ; ce
qu'un ticket a fait va dans ses messages de commit. Le comportement du firmware n'y est
pas : il est dans `7_Docs/PROTOCOL.md` (le contrat USB), la spec de l'atelier (états, décisions) et les
en-têtes de code.

## Conventions

### Interdits — non négociables

- **Jamais d'écriture dans la mémoire d'un objectif, hors les exceptions décidées par l'humain.**
  `bench_core` est la dernière barrière : tout ce que la carte émet vers l'objectif passe par lui,
  et ce qu'il ne liste pas est refusé. La liste exacte, exceptions comprises, est dans
  `4_Firmware/components/bench_core/bench_core.c` et `7_Docs/PROTOCOL.md`. C'est un **verrouillage**, au
  sens du Therac-25 : ça ne devient pas une option, ça ne se contourne pas « juste pour tester »,
  et ses tests (`4_Firmware/sim/test/test_bench_core.c`, `garde_emission.sh`) restent. Une commande
  nouvelle entre dans la liste par un ticket : avec la preuve, sur les décompilés, qu'elle n'écrit
  rien ; ou, si elle écrit, sur décision expresse de l'humain, réservée au modèle déclaré qu'il a
  nommé. Les décompilés restent privés : `bench_core.c` décrit chaque sous-commande, admise ou
  refusée, en termes de protocole, et renvoie à la preuve publiée, `7_Docs/E-Mount/samyang.md`
  (§ 6.3 les sous-commandes, § 7.2 la carte des écritures) ; le ticket qui ouvre une entrée écrit
  ce qu'il a établi dans le document de `7_Docs/E-Mount/` qui lui correspond, selon ses règles. Un
  objectif briqué ne se dé-brique pas.
- **Aucune modification du dépôt `samyang-reverse`** de l'utilisateur.
- **Aucune pull request vers indilib.**
- **`PROTOCOL.md` est le contrat**, pas de la documentation. Figé : `f r e a d s c` compatibles
  Pinefeat octet pour octet (réponses ≤ 15 caractères, jamais `ok` dans une erreur), repli
  Moonlite intact. `v` n'en est pas : elle rend la version du firmware de la carte, `<maj>.<min>`,
  sans suffixe (décision de l'humain). Le reste évolue quand ça apporte quelque chose, et
  `PROTOCOL.md` change dans le même commit que le code.
- **La carte ne parle jamais la première** : les événements `* …` seulement après `LOG ON`.
- **Un pilote n'envoie de message hors norme qu'à un objectif reconnu par signature** : un FE
  24-105 G qui reçoit un `'V'` part en tempête de `0x02` jusqu'à coupure d'alimentation.
- **Bornes mesurées, jamais codées** : rien ne suppose un modèle d'objectif, aucune position de
  repos en dur. (Seule exception, la précédente : la signature qui autorise un pilote à parler.)

### Méthode

- **Code embarqué : simple, robuste, fiable, sans fioriture** (l'humain). Pas de fonction qui ne
  fait pas mieux marcher la carte. Un test défend un comportement réel du firmware ou un défaut
  constaté, jamais un problème imaginé. Une règle de ce fichier vient de l'humain ou d'un fait
  technique démontré, et on peut dire lequel.
- Une transformation, un commit, une suite verte (les commandes de § Test). Pas de lot, pas
  de « pendant que j'y suis ».
- Ne pas refactorer ce qui a été payé par un incident : la complexité qu'il a laissée encode une
  connaissance que la relecture ne retrouvera pas.
- Écrire l'invariant avant le code.
- Une garde ne se justifie jamais par « ça ne casse pas sans elle », mais par le cas qu'elle
  défend, nommé, et l'endroit où ce cas est démontré.
- Pas d'abstraction pour un futur hypothétique ; le deuxième cas d'usage la justifie, le premier
  jamais.
- Pas de dépendance pour économiser vingt lignes. **Le firmware n'en a aucune hors ESP-IDF** —
  c'est une propriété, pas un hasard. L'outillage de développement en a, et elles ne tournent
  jamais sur la carte : Playwright épinglé dans `package.json` (`5_App/test/test_app.js`), Python 3
  (`tool_station` et son test, le générateur de la table des noms, le `manifest.json` des releases, la garde de la
  NVS et leur test, la démonstration du driver INDI), Node (tests de l'app), libindi 2 et astyle (la démonstration du
  driver INDI).
- Dire quand on ne sait pas. Sans matériel, on n'affirme aucun comportement de l'objectif : on dit
  ce que prouvent les tests hôte, et face à quel faux objectif.
- `PROJECT_VER` (`4_Firmware/CMakeLists.txt`) se bumpe **avant** de construire un binaire qu'on livre,
  sinon il porte l'ancienne version.
- **Il n'y a pas de gel** : le firmware se modifie par ticket, comme le reste (« un code buggé se
  corrige », l'humain).
- **Ce qui juge à la place du matériel** : la CI (suites de § Test, builds), les faux objectifs de
  `4_Firmware/sim` (faux 135, faux F051) et son faux SHT40. Ils disent ce que fait notre firmware face à notre
  modèle de l'objectif ou du capteur ; ce que le modèle simplifie ou fixe sans source est déclaré dans sa source (en
  tête de `sim/lens135.c`, de `sim/lens_std.c` et de `sim/sht40_sim.h`, paramètres [NÉ] de `sim/lens_std.h`). Ce
  fichier ne dit pas ce qui a été validé sur matériel.
- **Avant d'écrire dans `4_Firmware/`, code ou tests : `7_Docs/SUITES.md`** (une seule liste de
  sources, attendus écrits à la main, chaque mutant tué, constats de `test_std`, faux objectifs, PHY
  simulée et scriptée, magasin en mémoire, arguments des programmes).

### Doctrine

`7_Docs/DOCTRINE.md` gouverne les arbitrages. Le minimum à en retenir :

- Le critère est l'**irréversibilité** : un firmware dans une bague, sur une monture, dans un champ,
  à trois heures du matin, n'a pas de rollback.
- **Supprimer > simplifier > raccourcir.** Une ligne supprimée ne se teste pas, ne régresse pas,
  ne peut pas être fausse.
- **Sur-spécifié, sous-implémenté.** La rigueur dans la spécification, l'économie dans le code.
- **Un mineur vu est un défaut.** S'il existe une catégorie « vu, jugé sans importance », le mot
  « propre » ne porte plus d'information.
- Les suites hôte ne sont pas un filet : **ce sont le comité de revue**. Rien ne les affaiblit.

### Chemins, scripts, dépôt

- Chaque script et chaque test résout ses chemins depuis sa propre place, jamais depuis le
  répertoire courant : un script se lance depuis n'importe quel répertoire, et un programme de test
  reçoit en argument, obligatoire, ce qu'il lit hors de son répertoire. La CI n'appelle qu'un script
  d'entrée par composant, qui vit dans ce composant (`5_App/run.sh`, `4_Firmware/run.sh`,
  `4_Firmware/build.sh`, `5_App/tool_station/run.sh`, `6_Driver/indi/build.sh`, `6_Driver/indi/test.sh`). Un
  composant qui lit un fichier d'un autre le déclare en une ligne de son script d'entrée, et nulle part ailleurs.
- **Qui tient la vitrine.** Le `README.md` est au projet, et l'org n'y fait qu'une passe de style pour qu'il reste
  homogène entre ses produits (l'humain, 2026-10-20). Le manuel de la page produit, `9_Assets/manual/`, est au projet,
  au format que fixe le site (noms des fichiers, front matter, images : le `content/README.md` du dépôt du site ;
  l'architecte du site, avec l'accord de l'humain, 2026-10-20). La fiche `9_Assets/zurp.yml` est au format du site, et
  son `status` ne change que sur décision expresse de l'humain. L'org tient `LICENSE`, `LICENSE-HARDWARE`,
  `.github/workflows/zurp-site.yml`, l'affiche et les autres images de `9_Assets/`. Les licences, exceptions
  comprises, sont dans `README.md` (§ License) : `LICENSE` est la GPL-3.0, `LICENSE-HARDWARE` l'OCL v1.1,
  `6_Driver/indi/` en LGPL 2.1.
- L'arborescence et le nommage des fichiers de carte suivent le kit README de l'org, section
  « Arborescence et nommage des dépôts produit » : https://github.com/zUrp-Astronomics/.github/blob/main/readme-kit/README.md

## Build

Firmware ESP-IDF 5.3.2, une cible : `esp32c3` (la carte v1.0). `4_Firmware/` se construit par
`bash 4_Firmware/build.sh`, sans argument (environnement ESP-IDF chargé : `. "$IDF_PATH/export.sh"` ; dans
`4_Firmware/build/esp32c3`, sdkconfig compris ; échoue si la console n'est pas muette, ou si une entrée de
`flash_args` écrit sur la partition `nvs` de la table du build : `4_Firmware/garde_nvs.py`), job CI `firmware-build`,
qui publie à chaque run, PR comprises, l'artefact `firmware-esp32c3` : le dossier
`4_Firmware/build/esp32c3/artefact/` que `build.sh` remplit, à plat — `basilisk_FW-v<PROJECT_VER>-esp32c3.bin`,
`-bootloader.bin`, `-partition-table.bin`, `flash_args` sur ces noms, l'image unique `-full.bin` (en 0x0 ; elle
remet la NVS, donc la marque, à zéro) et `flash.sh`/`flash.cmd` (gabarits dans `4_Firmware/flash/`, qui
n'écrivent que `flash_args`). On les récupère sur la page du run, ou par
l'API : `GET /api/v1/repos/fleet/basilisk/actions/artifacts`, puis `…/artifacts/<id>/zip`. Chaque
composant porte ses invariants et ses pièges en commentaire, dans son `.c` et son `.h`. L'ancien firmware
(la v1) n'est plus dans le dépôt : il est archivé dans l'atelier, `sources/firmware-v1/`.

**Cible.** La carte v1.0 est un ESP32-C3FH4 nu (un cœur, 4 Mo de flash, rails de l'objectif commutés, EN
actifs à 0). La XIAO ESP32-S3 de banc est retirée du projet, et toutes les tâches sont épinglées sur le
cœur 0 (décisions de l'humain). Les broches sont dans la table du C3 (`components/phy/pins.h`), polarités
des EN et de la LED comprises ; une autre cible ne compile pas. Les réglages sont dans `sdkconfig.defaults`,
ceux qui tiennent au C3 dans `sdkconfig.defaults.esp32c3` (flash, horloge RTC). La console ESP-IDF est muette (niveaux de journal à NONE, bootloader compris, `sdkconfig.defaults`).
`CONFIG_ESP_TIMER_SUPPORTS_ISR_DISPATCH_METHOD=y` (`sdkconfig.defaults`) est exigé par l'`esp_timer`
rappelé en interruption de `components/phy/phy.c` (l'anti-rebond de D2).

**Carte des composants.** En-têtes partagés dans `4_Firmware/include/bsk_*.h`, privés à côté de leur `.c`.
Une flèche dit « appelle » ; aucune ne remonte. Les tâches, et qui peut appeler quoi depuis laquelle :
l'en-tête de chaque composant (`bsk_session.h`, `bsk_host.h`, `bsk_journal.h`, `bsk_store.h`, `bsk_sensor.h`).

- `main/main.c` (`app_main`, la tâche de SESSION) : la boucle des pas — `bsk_session_step`,
  `bsk_host_observe`, `bsk_session_real` (l'instant réel des commandes que l'USB dépose ensuite),
  `bsk_host_usb_service`, `bsk_led_board_service`.
- `host` (HOTE : `host.c`, `lens_names.c` en C pur ; `host_usb.c`, l'USB de la carte) -> `session`,
  `journal`, `led`, `sensor` : la ligne USB de `PROTOCOL.md`.
- `sensor` (`sht40.c` en C pur, le SHT40 sur un bus I2C mené à la main ; `sensor_board.c`, ses lignes et sa tâche de
  lecture sur la carte) : appelé par `main` (l'init) et `host` (la mesure) ; n'appelle aucun
  autre composant du firmware.
- `led` (`led.c` en C pur, les valeurs des motifs en tête du fichier ; `led_board.c`, le PWM de la carte)
  -> `session` (l'instantané seul).
- `session` -> `txn`, `phy`, `journal`, `store`. Le superviseur `session.c` -> `lens_rx`, `motion`, `ring`,
  `mark`, `drive`, `still`, `cmd`, `init`, `restore` ; `cmd.c` (la boîte de commandes et le bouton du fût)
  -> `motion`, `mark`, `ring`, `lens_rx` ; `init.c` (la séquence d'init) -> `txn`, `lens_rx`, `journal` ;
  `restore.c` (le retour à la marque) -> `motion`, `mark`, `lens_rx`, `journal`, `init` ; `motion.c` (MOUVEMENT)
  -> `drive`, `lens_rx`, `still` ; `ring.c` (la bague) -> `txn`, `lens_rx` ; `mark.c` (la marque) ->
  `store`, `lens_rx` ; `drive.c` (la consigne de mise au point) -> `txn` ; `still.c`, l'immobilité.
  `lens_rx.c` est le seul décodeur des trames reçues : un champ publié de plus s'y ajoute, sans toucher
  `session.c`.
- `txn` (TRANSACTION) -> `bench_core`, `journal` ; `bench_core` -> `phy` ; `journal` -> `phy_common`
  (`fr_encode`) ; `store` -> NVS ; `phy` -> les pilotes ESP-IDF. `phy_common.c` est commun à la PHY de la
  carte (`phy.c`) et à la PHY simulée (`sim/phy_sim.c`).
- `bench_core` est le seul appelant de `bsk_phy_send` (`sim/test/garde_emission.sh`) ; SESSION commande à
  la PHY les rails, les lignes, BODY_CS, la VD et lit ses événements, jamais l'émission.

## Test

    bash 5_App/run.sh                   # 5_App/ (la page de banc) : job `test`
    bash 5_App/tool_station/run.sh      # 5_App/tool_station/ : job `test`
    bash 4_Firmware/run.sh              # 4_Firmware/ : job `firmware`

## Doc

Lectures, dans l'ordre : `7_Docs/DOCTRINE.md` (pourquoi le code est écrit comme il l'est),
`7_Docs/PROTOCOL.md` (le contrat), `7_Docs/SUITES.md` (ce que joue chaque suite de § Test et ce que la
suite de `4_Firmware/` impose à qui y écrit). Ailleurs : `7_Docs/E-Mount/` (l'interopérabilité E-mount,
index et ordre de lecture dans son `README.md`), `7_Docs/MANUEL.md`, `6_Driver/indi/README.md`,
`7_Docs/CONFIANCE.md` (le rapport de confiance du firmware ; en anglais, `7_Docs/TRUST.md`). La file de
travail est `/home/projects.workshop/basilisk/backlog.md` (face atelier, hors du produit).

## Harness

5_App/test/ 5_App/run.sh 5_App/tool_station/test_tool_station.py 5_App/tool_station/run.sh 4_Firmware/traces/ 4_Firmware/sim/test/ 4_Firmware/run.sh

## Gotchas

- **Prérequis de `5_App/run.sh`** : node ; pour `5_App/test/test_app.js`, le module
  `playwright` (`npm install` à la racine, version épinglée dans `package.json`) et un Chromium sous
  `/opt/pw-browsers/chromium-*/chrome-linux/chrome` ou désigné par `CHROMIUM_PATH`. En CI (et dans la
  sonde), `.gitea/setup_host.sh` pose tout ça HORS de `$HOME` : la sonde joue la suite sous `env -i`
  avec un `HOME` vierge.
- **Playwright est épinglé à 1.49.1 exprès** : `test_app.js` cherche la disposition `chrome-linux`
  sous `/opt/pw-browsers`. Une montée de version peut la changer ; `.gitea/setup_host.sh` le vérifie.
- **Même image, même outillage** pour le job `test` de `ci.yml` et pour `probe-test-relevance.yml`
  (`catthehacker/ubuntu:act-latest` + `.gitea/setup_host.sh`) : on change l'un, on change l'autre.
- **Même image, même build** pour le job `firmware-build` de `ci.yml` et le job `build` de
  `.github/workflows/firmware.yml`, sa copie pour GitHub (`espressif/idf:v5.3.2`, `4_Firmware/build.sh`, artefact
  `firmware-esp32c3`) : on change l'un, on change l'autre. Le workflow GitHub ne joue aucun test et ne tourne que
  sur GitHub. La publication suit la norme de l'org : sur la poussée d'un tag `v*.*.*` (posé par Starfleet, sous le
  compte de l'humain ; jamais par Basilisk), son job `release` crée la release du tag avec quatre fichiers,
  `basilisk_<version>_esp32c3.bin` (l'image `-full.bin` de l'artefact, renommée), `manifest.json` (ESP Web Tools,
  écrit par `4_Firmware/manifest.py`, joué par `bash 4_Firmware/run.sh` : `sim/test/test_release.py`),
  `basilisk_<version>_esp32c3_flash.zip` (les fichiers de l'artefact qui flashent sans effacer la marque :
  `flash_args`, ses trois `.bin`, `flash.sh`, `flash.cmd`) et
  `basilisk_<version>_bench.html` (la copie exacte de `5_App/emount-bench.html` au tag), puis réveille le site de l'org
  (secret `ZURP_SITE_DISPATCH`). Le site copie le `manifest.json` et l'image de la dernière release et affiche le
  bouton d'installation (ESP Web Tools) sur la page produit, https://zurp-astronomics.github.io/basilisk/ ; ce
  bouton écrit l'image en 0x0 et remet donc la marque à zéro (accepté par l'humain). Le banc de la même release, le
  site le publiera tel quel sur la page produit (ticket site #105 ; d'ici là il ignore le fichier) : il le reconnaît
  au suffixe exact `_bench.html`, et n'en publie aucun si la release en porte deux. La procédure de flash, page
  produit d'abord, voies manuelles ensuite, est dans `README.md` § Flash ; on y renvoie à la page produit, jamais à
  l'URL d'un manifeste, qui change à chaque release. La version publiée est le tag sans son « v » : le tag
  `v1.0.1` nomme la release et publie `1.0.1`, dans les noms des fichiers et le `"version"` de `manifest.json`, qui
  garde ce nom (norme de l'org, `<produit>_<version>_<cible>`) ; `PROJECT_VER` reste `<maj>.<min>`.
- **`6_Driver/indi/` est l'amont d'INDI plus deux séries de patchs, et la CI le démontre hors de § Test** : le
  driver est `pinefeat_cef` d'indilib, puis `patches/fixes/`, puis `patches/basilisk/` ; ses deux fichiers et les
  patchs changent dans le même commit (son README, § « Refaire une série »). Le job `indi` (image `ubuntu:26.04`)
  installe **libindi 2** du PPA d'INDI (`ppa:mutlaqja/ppa`, 2.2.5 au 2026-10-19 ; il ne la publie que pour 26.04 ; la
  version jugée est celle qu'il sert le jour du run), joue `6_Driver/indi/build.sh` (`cmake` puis `make`, pkg-config,
  seul `indidriver` lié), puis `6_Driver/indi/test.sh` : les séries vérifiées sur l'amont (identifiants de blob) et au
  style d'indilib (astyle), puis l'amont, l'amont et les correctifs, et le driver joués sous libindi contre un faux
  Pinefeat, le driver contre une fausse Basilisk (`test/test_driver.py`). `test.sh` n'est pas dans § Test : la sonde
  rejoue § Test dans l'image du job `test`, où libindi 2 n'existe pas. Pas de cale libindi 1.9.9 (décision de
  l'humain : « on construit pas pour le passé »). Le binaire porte le nom de l'amont, `indi_pinefeat_cef_focus` :
  installé sous `/usr/local`, jamais sous `/usr`, où il remplacerait en silence celui du paquet.
- **La table des noms d'objectifs est générée** : `4_Firmware/components/host/lens_names.c` et la table
  `LENS_NAMES` de `5_App/emount-bench.html` sortent de `4_Firmware/gen_lens_names.py` (ExifTool `Sony.pm`,
  téléchargé, ou un `Sony.pm` local en argument). On régénère, on n'édite pas à la main ; aucune suite
  ne joue le générateur.
- `5_App/test/test_app.js` : toute `pageerror` (exception JavaScript non rattrapée dans la page) est
  affichée et comptée comme un échec, même si les vérifications fonctionnelles passent. Ne pas la
  signaler par `process.exitCode` : le `process.exit(fails ? 1 : 0)` final l'écraserait.
- **Le blocage au boot** observé une fois par la session web **n'est pas attribué au code** : câble
  USB fatigué et fils Dupont sur un proto sont les hypothèses de tête ; il ne redevient un défaut
  logiciel que s'il réapparaît avec un câblage sain (aucune trace n'en est dans le dépôt).
