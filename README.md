<!-- zurp-readme-header:begin — paste this block once, never again: the poster and the badges update themselves at each build of the site — do not edit it -->
<div align="center">

<a href="https://zurp-astronomics.github.io/basilisk/"><img src="9_Assets/basilisk.webp" alt="zUrp Astronomics product poster" width="420"></a>

![status](https://img.shields.io/endpoint?url=https%3A%2F%2Fzurp-astronomics.github.io%2Fbrand%2Fstatus%2Fbasilisk.json)
![software licence](https://zurp-astronomics.github.io/brand/badges/basilisk/software.svg)
![hardware licence](https://zurp-astronomics.github.io/brand/badges/basilisk/hardware.svg)

</div>

<!-- zurp-readme-header:end -->

<h1 align="center">Basilisk</h1>

<p align="center"><strong><em>Bend the glass to your will</em></strong></p>

<p align="center">
  <a href="https://zurp-astronomics.github.io/basilisk/">Website</a> ·
  <a href="7_Docs/">Documentation</a> ·
  <a href="../../releases">Releases</a> ·
  <a href="https://github.com/zUrp-Astronomics">zUrp Astronomics</a>
</p>

<p align="center"><strong>Français</strong> · <a href="#why-basilisk"><strong>English below</strong></a></p>

---

## Pourquoi Basilisk ?

Les caméras astro ne parlent pas Sony. **Basilisk, si.** Montez un objectif Sony E sur votre caméra astro et pilotez
sa mise au point et son ouverture depuis Ekos/INDI ou ASCOM — sans boîtier photo, un seul câble USB-C pour
l'alimentation et les données, et vous ne quittez plus la pièce chauffée. Basilisk parle le protocole série de
Pinefeat : les drivers INDI et ASCOM de Pinefeat la pilotent tels quels. Objectif cible n° 1 : le **Samyang AF 135 mm
F1.8 FE**.

## En bref

| | |
|---|---|
| Carte | en anneau, 50,87 × 36,3 mm, composants sur une seule face |
| Liaison | un câble USB-C pour l'alimentation et les données — aucune autre alimentation, aucun boîtier photo |
| Cerveau | ESP32-C3, USB natif |
| Alimentation de l'objectif | deux rails commutés et limités en courant (5 V moteur, 3,3 V logique), qui signalent chacun leurs défauts |
| Interface objectif | monture Sony E à 10 contacts sur pointes pogo unitaires, avec détection de l'objectif |
| Protection | ESD sur chaque ligne de l'objectif et sur l'USB |
| Pilotage | Ekos/INDI, ASCOM — compatible Pinefeat |

## Matériel

<p align="center"><img src="9_Assets/basilisk-board.webp" alt="Carte principale Basilisk v1.0" width="600"></p>

<p align="center"><img src="9_Assets/basilisk-board-top.webp" alt="Carte principale, dessus" width="300"> <img src="9_Assets/basilisk-board-bot.webp" alt="Carte principale, dessous" width="300"></p>

**La carte v1.0 est validée au banc** le 2026-10-19, avec deux objectifs : un Samyang AF 135 F1.8 FE et un Sony
FE 24-105 F4 G OSS. Ses fichiers de fabrication sont dans `1_Board/`.

Le boîtier imprimé est dans `3_3D-Models/`, sa source CAO (STEP) et la nomenclature hors carte dans `2_Hardware/`.
L'auteur valide les deux fichiers 3MF la semaine du 2026-10-20, à l'assemblage final.

<p align="center"><img src="9_Assets/basilisk-enclosure.webp" alt="Boîtier Basilisk v1.0b, rendu CAO : la bague E-mount avec ses contacts à pointes pogo et le port USB-C" width="600"></p>

<p align="center"><em>Le boîtier v1.0b, la révision précédente : aucun rendu du boîtier v1.0 n'est encore fourni.</em></p>

## Assemblage

Le manuel d'assemblage et d'utilisation est la [page produit](https://zurp-astronomics.github.io/basilisk/). Il y
arrive : le site le publie sur le feu vert de son responsable. Ce qui suit résume ce que donne le dépôt ; le détail est
dans [`2_Hardware/`](2_Hardware/README.md) et [`3_3D-Models/`](3_3D-Models/README.md).

**Les pièces** (nomenclature hors carte : [`2_Hardware/Basilisk_v1.0_BoM.csv`](2_Hardware/Basilisk_v1.0_BoM.csv)) :

- la carte Basilisk v1.0 (§ Fabriquer la carte) ;
- le boîtier imprimé : le cœur (dessus et dessous) et le contour USB ;
- **1 demi-kit Meike MK-S-AF3A** : le kit est une paire de bagues macro de 10 et 16 mm ; on n'y prend que le fût
  métallique mâle, ses vis et son loquet ;
- **3 vis M2.5×12** : elles remplacent les 3 vis M2.5×8 d'origine de la plaque avant de la caméra, pour compenser la
  hauteur ajoutée et garder des filets en prise.

**L'impression** :

| Fichier | Pièces | Matière |
|---|---|---|
| `3_3D-Models/Basilisk_v1.0_enclosure.3mf` | le cœur du boîtier : le dessus et le dessous | PC-CF validé ; PETG fera le travail ; ABS et ASA devraient tenir ; PLA à proscrire |
| `3_3D-Models/Basilisk_v1.0_USB.3mf` | le contour USB | PETG transparent, pour laisser passer la lumière des LED |

Les tolérances fonctionnent chez l'auteur ; selon la matière et la machine, elles sont à retoucher à la marge (au dixième
de millimètre pour le loquet, par exemple).

**Les caméras.** Compatibles : les caméras planétaires Player One non refroidies, au format hexagonal — Mars-C,
Mars-C II, Mars-M II, Mars 662M ; Neptune-C, Neptune-M, Neptune-C II, Neptune 664C ; Uranus-C, Uranus-M ; Saturn-C SQR,
Saturn-M SQR (liste relevée sur le site de Player One le 2026-10-20). Non compatibles, pour l'instant : les caméras
refroidies. Non vérifiées : ZWO et ToupTek.

## Logiciel

- **Firmware** — `4_Firmware/` : ESP-IDF, aucune autre dépendance. Pour l'ESP32-C3 de la carte v1.0 ; version 1.3.
- **Page de banc** — `5_App/emount-bench.html` : une page HTML seule qui pilote la carte par Web Serial, depuis
  Firefox 151 ou plus, Chrome ou Edge, sur ordinateur ; en français ou en anglais. Rien à installer. Chaque release la
  porte (`basilisk_<tag>_bench.html`), et la [page produit](https://zurp-astronomics.github.io/basilisk/) l'ouvrira,
  celle de la release qu'elle installe : à venir.
- **tool_station** — `5_App/tool_station/` : lit et règle le commutateur Custom M1/M2 du Samyang AF 135, sans l'outil
  de Samyang (Python).
- **Driver INDI** — `6_Driver/indi/` : **une version provisoire**. Le driver Pinefeat d'INDI, `pinefeat_cef` (binaire
  `indi_pinefeat_cef_focus`, version 1.2), avec deux séries de patchs à proposer à l'amont : des correctifs, puis
  l'extension Basilisk. Rien n'est encore soumis : l'auteur contactera Pinefeat d'abord. Si les patchs sont acceptés,
  ce dossier laissera la place à un lien vers INDI. Les drivers Pinefeat d'origine, INDI et ASCOM, pilotent aussi la
  carte.

## Installer le firmware

Depuis votre navigateur, sur la [page produit](https://zurp-astronomics.github.io/basilisk/) (Firefox, Chrome ou Edge),
sur ordinateur, la carte branchée en USB-C ; cette installation remet votre marque de mise au point à zéro.

### Les autres voies

Depuis les fichiers de la [dernière release](https://github.com/zUrp-Astronomics/Basilisk/releases/latest), la carte
branchée à l'ordinateur en USB-C par un câble de données (l'outil de flash la passe lui-même en mode téléchargement par
l'USB) :

- **Dans le navigateur, garde votre marque.** Firefox 151 ou plus, Chrome ou Edge, sur ordinateur (ni Safari, ni
  mobile). Dézippez `basilisk_<tag>_esp32c3_flash.zip`, ouvrez [esptool-js](https://espressif.github.io/esptool-js/)
  d'Espressif, cliquez **Connect** et choisissez le port de la carte, puis ajoutez les trois fichiers `.bin` aux adresses
  de `flash_args` : `0x0` le `…-bootloader.bin`, `0x8000` le `…-partition-table.bin`, `0x10000` l'application
  `….bin`. Cliquez **Program**. Ne cliquez pas **Erase Flash**.
- **En ligne de commande, garde votre marque.** Installez esptool (`pip install esptool`), dézippez
  `basilisk_<tag>_esp32c3_flash.zip`, puis lancez `sh flash.sh [PORT]` (Linux, macOS) ou `flash.cmd [PORT]` (Windows)
  depuis ce dossier.
- **Installation propre, efface votre marque.** Écrivez `basilisk_<tag>_esp32c3.bin` en `0x0` (esptool-js ou esptool),
  pour une puce neuve ou qui a porté un autre firmware.

Le port est `/dev/ttyACM0` sous Linux, `/dev/cu.usbmodem…` sous macOS et `COMx` sous Windows (« Périphérique série
USB ») ; aucun pilote n'est nécessaire.

## Fabriquer la carte

**La voie facile — assemblée par JLCPCB.** Envoyez le Gerber, la BoM et le pick-and-place de `1_Board/` et commandez la
carte assemblée, face du dessus seulement. Les dix contacts de l'objectif sont des pointes pogo unitaires (P1–P10),
listées avec les autres composants du dessus ; l'empreinte E-mount CN2 n'est pas câblée. Les options du PCB sont dans
`1_Board/Basilisk-v1.0_0-README.txt` : 2 couches, 1,0 mm, cuivre 1 oz, vernis noir, sérigraphie blanche, HASL RoHS.

**La voie difficile — tout faire soi-même.** Tout est dans `1_Board/`. Attendez-vous à du pas fin : un
microcontrôleur en QFN, des passifs 0402 et de minuscules composants de puissance à pad exposé.

## La documentation

- La [page produit](https://zurp-astronomics.github.io/basilisk/) — le manuel d'assemblage et d'utilisation (en
  anglais), qui y arrive.
- [`7_Docs/MANUEL.md`](7_Docs/MANUEL.md) — la référence technique de la carte : matériel et brochage, firmware, ce que
  fait la carte, drivers, page de banc, sécurité, tests (en français).
- [`7_Docs/PROTOCOL.md`](7_Docs/PROTOCOL.md) — le protocole série de la carte (en français).
- [`7_Docs/E-Mount/`](7_Docs/E-Mount/README.md) — l'interopérabilité Sony E-mount, vue du boîtier : comment un boîtier
  parle à son objectif (en anglais).

## État et feuille de route

- **v1.0 — publiée.** La carte v1.0, validée au banc le 2026-10-19 ; le firmware 1.3 ; le driver INDI 1.2, provisoire.
  Les 3MF du boîtier sont en validation la semaine du 2026-10-20, à l'assemblage final.
- **À venir.**
  - Le manuel d'assemblage et d'utilisation, et la page de banc, sur la
    [page produit](https://zurp-astronomics.github.io/basilisk/).
  - Les patchs du driver INDI proposés à l'amont, après un contact avec Pinefeat.
  - Le projet sur **Printables** et **Instructables**, en plus de GitHub : à venir.

## Remerciements

Basé sur [Pinefeat](https://github.com/pinefeat/cef135) : Basilisk parle son protocole série, et le driver INDI du dépôt
est son driver d'INDI.

## Contenu du dépôt

| Dossier | Contenu |
|---|---|
| [`0_Datasheets/`](0_Datasheets/) | les datasheets des composants, telles que les publient leurs fabricants |
| [`1_Board/`](1_Board/) | la fabrication de la carte : schéma, vues, Gerber, BoM, pick-and-place |
| [`2_Hardware/`](2_Hardware/) | la mécanique hors carte : boîtier (STEP), nomenclature mécanique, caméras compatibles |
| [`3_3D-Models/`](3_3D-Models/) | les fichiers prêts à imprimer (3MF), matières et tolérances |
| [`4_Firmware/`](4_Firmware/) | le firmware : sources, build, tests |
| [`5_App/`](5_App/) | les applications qui parlent au produit : la page de banc, tool_station |
| [`6_Driver/`](6_Driver/) | les drivers : le driver INDI provisoire |
| [`7_Docs/`](7_Docs/) | la documentation : référence technique, protocole série, interopérabilité Sony E-mount |
| [`8_References/`](8_References/) | les documents de référence externes : ce que le projet lit, pas ce qu'il produit |
| [`9_Assets/`](9_Assets/) | la vitrine : fiche produit, affiche, images du README, manuel de la page produit |

## Licence

- **Conception matérielle** — cartes, mécanique et modèles 3D : [Open Community License v1.1](LICENSE-HARDWARE).
- **Tout le reste** — firmware, logiciel, documentation et images : [GNU GPL v3.0](LICENSE).

Les contenus tiers gardent leur propre licence :

- `6_Driver/indi/` est sous LGPL 2.1 (`6_Driver/indi/LICENSE`) ;
- les tables de noms d'objectifs générées depuis ExifTool (`4_Firmware/components/host/lens_names.c`, la table
  `LENS_NAMES` de `5_App/emount-bench.html`) gardent la licence de leur source ;
- les datasheets de `0_Datasheets/` et les documents de `8_References/` appartiennent à leurs auteurs.

---

## Why Basilisk?

Astro cameras don't speak Sony. **Basilisk does.** Put a Sony E lens on your astro camera and drive
its focus and aperture from Ekos/INDI or ASCOM — no camera body, one USB-C cable for power and data,
and you never leave the warm room. It speaks the Pinefeat serial protocol, so the Pinefeat INDI and
ASCOM drivers work with it unchanged. First target lens: the **Samyang AF 135 mm F1.8 FE**.

## At a glance

| | |
|---|---|
| Board | ring-shaped, 50.87 × 36.3 mm, parts on one side only |
| Link | one USB-C cable for power and data — no other supply, no camera body |
| Brain | ESP32-C3, native USB |
| Lens power | two switched, current-limited rails (5 V motor, 3.3 V logic), each reporting its faults |
| Lens interface | 10-contact Sony E-mount on single pogo pins, with lens detection |
| Protection | ESD on every lens line and on USB |
| Control | Ekos/INDI, ASCOM — Pinefeat-compatible |

## Hardware

<p align="center"><img src="9_Assets/basilisk-board.webp" alt="Basilisk main board v1.0" width="600"></p>

<p align="center"><img src="9_Assets/basilisk-board-top.webp" alt="Main board, top" width="300"> <img src="9_Assets/basilisk-board-bot.webp" alt="Main board, bottom" width="300"></p>

**The v1.0 board is validated on the bench** on 2026-10-19, with two lenses: a Samyang AF 135 F1.8 FE and a Sony
FE 24-105 F4 G OSS. Its manufacturing files are in `1_Board/`.

The printable enclosure is in `3_3D-Models/`, its CAD source (STEP) and the bill of materials outside the board in
`2_Hardware/`. The author validates the two 3MF files in the week of 2026-10-20, at the final assembly.

<p align="center"><img src="9_Assets/basilisk-enclosure.webp" alt="Basilisk enclosure v1.0b, CAD render: the E-mount ring with its pogo-pin contacts and the USB-C port" width="600"></p>

<p align="center"><em>The enclosure v1.0b, the previous revision: no render of the v1.0 enclosure is provided yet.</em></p>

## Assembly

The assembly and user manual is the [product page](https://zurp-astronomics.github.io/basilisk/). It is on its way
there: the website publishes it on its maintainer's go. What follows sums up what the repository gives; the details are
in [`2_Hardware/`](2_Hardware/README.md) and [`3_3D-Models/`](3_3D-Models/README.md).

**The parts** (bill of materials outside the board: [`2_Hardware/Basilisk_v1.0_BoM.csv`](2_Hardware/Basilisk_v1.0_BoM.csv)):

- the Basilisk board v1.0 (§ Build);
- the printed enclosure: the core (top and bottom) and the USB surround;
- **1 half kit of Meike MK-S-AF3A**: the kit is a pair of 10 mm and 16 mm macro extension tubes; only the male metal
  barrel, its screws and its latch are used;
- **3 M2.5×12 screws**: they replace the 3 original M2.5×8 screws of the camera front plate, to compensate the added
  height and keep threads engaged.

**Printing**:

| File | Parts | Material |
|---|---|---|
| `3_3D-Models/Basilisk_v1.0_enclosure.3mf` | the enclosure core: the top and the bottom | PC-CF validated; PETG should do the job; ABS and ASA should hold; PLA not to be used |
| `3_3D-Models/Basilisk_v1.0_USB.3mf` | the USB surround | clear PETG, to let the light of the LEDs through |

The tolerances work for the author; depending on the material and the printer, they may need a marginal touch-up (to
the tenth of a millimetre for the latch, for example).

**Cameras.** Compatible: the uncooled Player One planetary cameras, in the hexagonal format — Mars-C, Mars-C II,
Mars-M II, Mars 662M; Neptune-C, Neptune-M, Neptune-C II, Neptune 664C; Uranus-C, Uranus-M; Saturn-C SQR, Saturn-M SQR
(list as read on Player One's website on 2026-10-20). Not compatible, for now: the cooled cameras. Not checked: ZWO
and ToupTek.

## Software

- **Firmware** — `4_Firmware/`: ESP-IDF, no other dependency. For the ESP32-C3 of the v1.0 board; version 1.3.
- **Bench page** — `5_App/emount-bench.html`: a single HTML page that drives the board over Web Serial from Firefox 151
  or later, Chrome or Edge, on a computer; in French or English. Nothing to install. Each release carries it
  (`basilisk_<tag>_bench.html`), and the [product page](https://zurp-astronomics.github.io/basilisk/) will open it, the
  one from the release it installs: coming.
- **tool_station** — `5_App/tool_station/`: reads and sets the M1/M2 custom switch of the Samyang AF 135, without
  Samyang's tool (Python).
- **INDI driver** — `6_Driver/indi/`: **a provisional version**. INDI's Pinefeat driver, `pinefeat_cef` (binary
  `indi_pinefeat_cef_focus`, version 1.2), with two series of patches to propose upstream: fixes, then the Basilisk
  extension. Nothing is submitted yet: the author will contact Pinefeat first. If the patches are accepted, this folder
  will give way to a link to INDI. The stock Pinefeat drivers, INDI and ASCOM, drive the board too.

## Flash

Install it from your browser on the [product page](https://zurp-astronomics.github.io/basilisk/) (Firefox, Chrome or Edge).
On a computer, with the board plugged in over USB-C; this install resets your focus mark.

### Other ways

From the files of the [latest release](https://github.com/zUrp-Astronomics/Basilisk/releases/latest), with the board
plugged into the computer over USB-C with a data cable (the flashing tool puts it into download mode over USB):

- **In the browser, keeps your focus mark.** Firefox 151 or later, Chrome or Edge, on a computer (not Safari, not
  mobile). Unzip `basilisk_<tag>_esp32c3_flash.zip`, open Espressif's
  [esptool-js](https://espressif.github.io/esptool-js/), click **Connect** and pick the board's port, then add the
  three `.bin` files at the addresses listed in `flash_args`: `0x0` the `…-bootloader.bin`, `0x8000` the
  `…-partition-table.bin`, `0x10000` the application `….bin`. Click **Program**. Do not click **Erase Flash**.
- **Command line, keeps your focus mark.** Install esptool (`pip install esptool`), unzip
  `basilisk_<tag>_esp32c3_flash.zip`, then run `sh flash.sh [PORT]` (Linux, macOS) or `flash.cmd [PORT]` (Windows)
  from that folder.
- **Clean install, erases your focus mark.** Write `basilisk_<tag>_esp32c3.bin` at `0x0` (esptool-js or esptool),
  for a new chip or one that ran other firmware.

The port is `/dev/ttyACM0` on Linux, `/dev/cu.usbmodem…` on macOS and `COMx` on Windows ("USB Serial Device"); no
driver is needed.

## Build

**The easy way — JLCPCB assembly.** Send the Gerber, BoM and pick-and-place from `1_Board/` and order
the board assembled, top side only. The ten lens contacts are single pogo pins (P1–P10), listed with
the other top-side parts; the E-mount footprint CN2 is not wired. PCB options are in
`1_Board/Basilisk-v1.0_0-README.txt`: 2 layers, 1.0 mm, 1 oz copper, black solder mask, white
silkscreen, HASL RoHS.

**The hard way — do it all yourself.** Everything is in `1_Board/`. Expect fine-pitch work: a QFN
microcontroller, 0402 passives and tiny exposed-pad power parts.

## Documentation

- The [product page](https://zurp-astronomics.github.io/basilisk/) — the assembly and user manual (English), on its
  way there.
- [`7_Docs/MANUEL.md`](7_Docs/MANUEL.md) — the technical reference of the board: hardware and pinout, firmware, what
  the board does, drivers, bench page, safety, tests (French).
- [`7_Docs/PROTOCOL.md`](7_Docs/PROTOCOL.md) — the board's serial protocol (French).
- [`7_Docs/E-Mount/`](7_Docs/E-Mount/README.md) — Sony E-mount interoperability, seen from the camera
  body: how a body talks to its lens (English).

## Status & roadmap

- **v1.0 — released.** The v1.0 board, validated on the bench on 2026-10-19; firmware 1.3; INDI driver 1.2,
  provisional. The enclosure 3MF files are in validation in the week of 2026-10-20, at the final assembly.
- **Coming.**
  - The assembly and user manual, and the bench page, on the
    [product page](https://zurp-astronomics.github.io/basilisk/).
  - The INDI driver patches proposed upstream, after a contact with Pinefeat.
  - The project on **Printables** and **Instructables**, besides GitHub: coming soon.

## Credits

Based on [Pinefeat](https://github.com/pinefeat/cef135): Basilisk speaks its serial protocol, and the INDI driver of
this repository is its INDI driver.

## Repository layout

| Folder | Contents |
|---|---|
| [`0_Datasheets/`](0_Datasheets/) | datasheets of the components, as published by their makers |
| [`1_Board/`](1_Board/) | board manufacturing files: schematics, views, Gerber, BoM, pick-and-place |
| [`2_Hardware/`](2_Hardware/) | mechanics outside the board: enclosure (STEP), mechanical BoM, compatible cameras |
| [`3_3D-Models/`](3_3D-Models/) | ready-to-print files (3MF), materials and tolerances |
| [`4_Firmware/`](4_Firmware/) | firmware: sources, build, tests |
| [`5_App/`](5_App/) | applications that talk to the product: the bench page, tool_station |
| [`6_Driver/`](6_Driver/) | drivers: the provisional INDI driver |
| [`7_Docs/`](7_Docs/) | documentation: technical reference, serial protocol, Sony E-mount interoperability |
| [`8_References/`](8_References/) | external reference documents: what the project reads, not what it produces |
| [`9_Assets/`](9_Assets/) | the showcase: product sheet, poster, README images, product-page manual |

## License

- **Hardware design** — boards, mechanics and 3D models: [Open Community License v1.1](LICENSE-HARDWARE).
- **Everything else** — firmware, software, documentation and images: [GNU GPL v3.0](LICENSE).

Third-party material keeps its own licence:

- `6_Driver/indi/` is under LGPL 2.1 (`6_Driver/indi/LICENSE`);
- the lens-name tables generated from ExifTool (`4_Firmware/components/host/lens_names.c`, the `LENS_NAMES` table of `5_App/emount-bench.html`) keep their source's licence;
- the datasheets in `0_Datasheets/` and the documents in `8_References/` belong to their authors.

---

<p align="center"><sub><a href="https://zurp-astronomics.github.io/">zUrp Astronomics</a> — a subsidiary of zUrp Industries. Because buying is cheating.</sub></p>
