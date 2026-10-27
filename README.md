<!-- zurp-readme-header:begin — paste this block once, never again: the poster and the badges update themselves at each build of the site — do not edit it -->
<div align="center">

<a href="https://zurp-astronomics.github.io/basilisk/"><img src="9_Assets/basilisk.webp" alt="zUrp Astronomics product poster" width="420"></a>

![status](https://img.shields.io/endpoint?url=https%3A%2F%2Fzurp-astronomics.github.io%2Fbrand%2Fstatus%2Fbasilisk.json)
![software licence](https://zurp-astronomics.github.io/brand/badges/basilisk/software.svg)
![hardware licence](https://zurp-astronomics.github.io/brand/badges/basilisk/hardware.svg)

</div>

<!-- zurp-readme-header:end -->

# Basilisk

**Bend the glass to your will.**

[Français](#français) · [English](#english) · [Site / web bench](https://zurp-astronomics.github.io/basilisk/) · [Releases](https://github.com/zUrp-Astronomics/basilisk/releases)

## Français

Adaptateur Sony E pour caméra astro : alimentation, focus et diaphragme par un seul câble USB-C. Carte ESP32-C3 en anneau, mécanique imprimée, interface USB compatible Pinefeat pour INDI et ASCOM. La conception vise les caméras Player One hexagonales non refroidies ; les détails de montage et le périmètre mécanique sont dans le manuel.

**Pour construire et utiliser :** [manuel autonome](7_Docs/MANUEL.md), ou [page produit](https://zurp-astronomics.github.io/basilisk/) avec installation du firmware et banc en direct. Le banc se télécharge aussi avec chaque release pour travailler hors ligne. L'image complète du programmateur web efface la marque enregistrée ; le paquet `_flash.zip` fournit la mise à jour qui la conserve.

### Carte du dépôt

| Dossier | Ce qu'on y trouve |
|---|---|
| [0_Datasheets](0_Datasheets/) | Documents des composants |
| [1_Board](1_Board/) | Schéma, Gerber, BoM, placement et vues de la carte |
| [2_Hardware](2_Hardware/) | Source STEP, nomenclature mécanique et interface caméra |
| [3_3D-Models](3_3D-Models/) | Pièces prêtes à imprimer |
| [4_Firmware](4_Firmware/) | Firmware ESP-IDF 5.3.2, cible ESP32-C3, simulateurs et vérifications |
| [5_App](5_App/) | Banc HTML autonome et outil Custom du Samyang AF 135 |
| [6_Driver](6_Driver/) | Pilote INDI Pinefeat, correctifs et extensions Basilisk |
| [7_Docs](7_Docs/) | Réalisation, utilisation, contrat USB et référence E-mount |
| [8_References](8_References/) | Références externes |
| [9_Assets](9_Assets/) | Fiche, images et manuel consommés automatiquement par le site |

### Interfaces et développement

Le [contrat USB](7_Docs/PROTOCOL.md) définit les commandes et leurs résultats ; la [référence E-mount](7_Docs/E-Mount/README.md) documente l'autre côté de la carte, avec la provenance des observations et analyses. Les invariants et contrats internes vivent dans les commentaires du code. Les [suites](7_Docs/SUITES.md) décrivent les vérifications ; les [rapports de confiance](7_Docs/CONFIANCE.md) sont datés et ne valent que pour leur révision.

Après chargement de l'environnement ESP-IDF 5.3.2, `bash 4_Firmware/build.sh` construit les images. Les points d'entrée de vérification sont `4_Firmware/run.sh`, `5_App/run.sh`, `5_App/tool_station/run.sh` et les scripts du [pilote INDI](6_Driver/indi/README.md). Le [manuel](7_Docs/MANUEL.md) détaille les prérequis et les artefacts.

Les sources distinguent la validation de la carte et celle de l'assemblage final : la séquence de montage v1.0 reste à vérifier et photographier. La température et l'humidité sont des mesures locales du capteur ; leur représentativité de l'air ou de l'objectif n'est pas établie.

### Origines

[Pinefeat](https://github.com/pinefeat/cef135) fournit le point de départ et la compatibilité des clients. La référence E-mount crédite les travaux publics dont elle part dans sa section [Prior art](7_Docs/E-Mount/protocol.md#10-prior-art). Les analyses supplémentaires restent distinguées des captures réelles.

## Licence

- **Conception matérielle** — cartes, mécanique et modèles 3D : [Open Community License v1.1](LICENSE-HARDWARE).
- **Tout le reste** — firmware, logiciel, documentation et images : [GNU GPL v3.0](LICENSE).

Les contenus tiers gardent leur propre licence :

- `6_Driver/indi/` est sous LGPL 2.1 (`6_Driver/indi/LICENSE`) ;
- les tables de noms d'objectifs générées depuis ExifTool (`4_Firmware/components/host/lens_names.c`, la table
  `LENS_NAMES` de `5_App/emount-bench.html`) gardent la licence de leur source ;
- les datasheets de `0_Datasheets/` et les documents de `8_References/` appartiennent à leurs auteurs.


## English

Sony E adapter for an astro camera: power, focus and aperture over one USB-C cable. An ESP32-C3 ring-shaped board, printed mechanics and a Pinefeat-compatible USB interface for INDI and ASCOM. The enclosure targets uncooled, hexagonal Player One cameras; the manual records the mechanical scope and build details.

**To build and use it:** the [standalone manual](7_Docs/MANUAL.md), or the [product page](https://zurp-astronomics.github.io/basilisk/) with firmware installation and a live bench. Each release also provides the bench for offline use. The web installer's merged image erases the saved mark; the `_flash.zip` package provides an update that preserves it.

### Repository map

| Folder | Contents |
|---|---|
| [0_Datasheets](0_Datasheets/) | Component documentation |
| [1_Board](1_Board/) | Schematic, Gerber, BoM, placement and board views |
| [2_Hardware](2_Hardware/) | STEP source, mechanical BoM and camera interface |
| [3_3D-Models](3_3D-Models/) | Printable parts |
| [4_Firmware](4_Firmware/) | ESP-IDF 5.3.2 firmware for ESP32-C3, simulators and checks |
| [5_App](5_App/) | Standalone HTML bench and Samyang AF 135 Custom tool |
| [6_Driver](6_Driver/) | Pinefeat INDI driver, fixes and Basilisk extensions |
| [7_Docs](7_Docs/) | Build and use, USB contract and E-mount reference |
| [8_References](8_References/) | External references |
| [9_Assets](9_Assets/) | Sheet, images and manual ingested automatically by the site |

### Interfaces and development

The [USB contract](7_Docs/PROTOCOL.md) defines commands and results; the [E-mount reference](7_Docs/E-Mount/README.md) documents the other side of the board and the evidence behind it. Internal contracts and invariants live in code comments. The [suite reference](7_Docs/SUITES.md) describes verification; the [confidence report](7_Docs/TRUST.md) is dated and applies to its stated revision. The detailed USB contract and suite reference are currently in French.

With the ESP-IDF 5.3.2 environment loaded, `bash 4_Firmware/build.sh` builds the images. Verification entry points are `4_Firmware/run.sh`, `5_App/run.sh`, `5_App/tool_station/run.sh` and the [INDI driver's](6_Driver/indi/README.md) scripts. The [manual](7_Docs/MANUAL.md) describes prerequisites and build artifacts.

Board validation and final assembly validation are separate: the v1.0 assembly sequence still needs checking and photographs. Temperature and humidity are local sensor readings; how well they represent the air or lens has not been established.

### Origins

[Pinefeat](https://github.com/pinefeat/cef135) provides the starting point and client compatibility. The E-mount reference credits earlier public work in [Prior art](7_Docs/E-Mount/protocol.md#10-prior-art). Further static analysis remains distinguished from real captures.

## License

- **Hardware design** — boards, mechanics and 3D models: [Open Community License v1.1](LICENSE-HARDWARE).
- **Everything else** — firmware, software, documentation and images: [GNU GPL v3.0](LICENSE).

Third-party material keeps its own licence:

- `6_Driver/indi/` is under LGPL 2.1 (`6_Driver/indi/LICENSE`);
- the lens-name tables generated from ExifTool (`4_Firmware/components/host/lens_names.c`, the `LENS_NAMES` table of `5_App/emount-bench.html`) keep their source's licence;
- the datasheets in `0_Datasheets/` and the documents in `8_References/` belong to their authors.


<!-- zurp-readme-footer:begin — paste this block once, at the end of the README, never again: the plate and the signature update themselves at each build of the site — do not edit it -->
<p align="center"><a href="https://zurp-astronomics.github.io/"><img src="https://zurp-astronomics.github.io/brand/zurp-plate.webp" alt="zUrp Astronomics enamel plate" width="400"></a></p>

<p align="center"><img src="https://zurp-astronomics.github.io/brand/signature.png" alt="zUrp Astronomics signature" width="400"></p>

<p align="center"><a href="https://zurp-astronomics.github.io/">Website</a> · <a href="https://github.com/zUrp-Astronomics">GitHub organisation</a></p>
<!-- zurp-readme-footer:end -->
