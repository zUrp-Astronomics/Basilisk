#!/bin/sh
# SOURCE: ticket #602 — le script de flash livré dans chaque artefact du firmware (gabarit : 4_Firmware/build.sh y écrit la cible)
# AUTHOR: engineer
# DATE: 2026-10-11
# STATUS: actif — copié par 4_Firmware/build.sh dans 4_Firmware/build/<cible>/artefact/, la cible écrite à sa place
#
# Usage : sh flash.sh [PORT]        (PORT : /dev/ttyACM0… ; sans lui, esptool cherche la carte)
#
# Écrit le bootloader, la table de partitions et l'application que liste flash_args, rien d'autre : ni erase_flash,
# ni l'image -full.bin, qui remplit de 0xFF la NVS et remettrait la marque à zéro. La carte passe en téléchargement
# par la remise à zéro d'esptool sur l'USB de la puce ; aucun bouton n'est supposé.
cd "$(dirname "$0")" || exit 1
if command -v esptool >/dev/null 2>&1; then e=esptool
elif command -v esptool.py >/dev/null 2>&1; then e=esptool.py
elif python3 -m esptool version >/dev/null 2>&1; then e="python3 -m esptool"
else
    echo "esptool introuvable : pip install esptool, ou l'exécutable autonome d'Espressif (https://github.com/espressif/esptool/releases) dans le PATH" >&2
    exit 1
fi
exec $e --chip @CIBLE@ -b 460800 ${1:+-p "$1"} write_flash @flash_args
