---
date: 2026-10-26
status: publication candidate
---

## Firmware et première connexion

Le bloc **Install** ci-dessous programme la carte par USB avec l'image complète de la release : **cette installation efface la marque de mise au point enregistrée dans la carte**. Pour une mise à jour qui la conserve, utiliser le paquet `_flash.zip` de la release et son script `flash.sh` ou `flash.cmd` ; le [manuel du dépôt](../../7_Docs/MANUEL.md) décrit cette voie hors ligne.

Ouvrir le **banc** de cette page, sélectionner le port USB de la carte et se connecter. Banc et firmware proviennent de la même release. Monter un objectif et attendre l'état prêt. Son identité, ses bornes de focus et sa plage d'ouverture doivent apparaître avant de commander un mouvement. Une seule application peut utiliser le port série : déconnecter le banc avant d'ouvrir le logiciel astro.

Pour une nuit sans réseau, télécharger auparavant le fichier `_bench.html` de la release : c'est le banc autonome. Si l'accès depuis un fichier local est refusé, servir son dossier avec `python3 -m http.server` et l'ouvrir depuis `http://localhost:8000/`. Le navigateur doit fournir Web Serial.

## Ekos / INDI

Sélectionner **Pinefeat CEF** comme focuser et choisir le port série de la carte. Le pilote INDI standard gère le focus et le diaphragme. Celui du dépôt ajoute la marque, la relecture de l'ouverture, l'identité et le redémarrage de l'objectif, la luminosité de la LED, les bornes de focus rapportées et les propriétés du capteur.

Le pilote fourni nécessite **libindi 2**. Depuis la racine du dépôt :

```sh
cmake -S 6_Driver/indi -B build/indi
cmake --build build/indi
sudo cmake --install build/indi
```

Installer auparavant les outils de compilation C/C++, CMake, pkg-config et le paquet de développement libindi. L'installation va par défaut dans `/usr/local` ; conserver ce préfixe pour laisser intact le pilote de la distribution. `command -v indi_pinefeat_cef_focus` indique le binaire trouvé. Le périphérique INDI conserve le nom amont **Pinefeat EF Lens Controller**. Le réglage du débit série n'a pas d'effet sur l'USB natif.

La [référence du pilote](../../6_Driver/indi/README.md) détaille ses propriétés, sa provenance et ses séries de correctifs.

## ASCOM et autres clients

Utiliser le [pilote ASCOM Pinefeat](https://github.com/pinefeat/cef135) et choisir le port COM de la carte. Basilisk implémente son contrat de commandes USB.

Les clients Moonlite peuvent piloter le focus et lire la température. Ce protocole rend **0 °C quand la température est inconnue** ; le banc et l'extension INDI Basilisk peuvent représenter explicitement cette absence. Pour un autre client, suivre le [contrat USB](../../7_Docs/PROTOCOL.md).

## Commutateur Custom du Samyang AF 135

Pour ce modèle reconnu, la [tool station](../../5_App/tool_station/) permet de lire et régler les affectations M1/M2 : AF, MF ou commande du diaphragme par la bague (AP). Ce réglage réside **dans la flash de l'objectif**, contrairement à la marque de Basilisk.

Le [manuel du dépôt](../../7_Docs/MANUEL.md) donne les commandes et le périmètre. L'outil demande confirmation avant d'écrire et relit le réglage. **Couper puis remettre l'alimentation de l'objectif après une lecture comme après une écriture.** Cette configuration est facultative ; le pilotage normal du focus et du diaphragme n'en a pas besoin.
