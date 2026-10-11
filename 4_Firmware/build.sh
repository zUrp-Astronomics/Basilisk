#!/usr/bin/env bash
# SOURCE: build ESP-IDF 5.3.2 de 4_Firmware/, cibles esp32s3 et esp32c3
# AUTHOR: engineer
# DATE: 2026-10-10
# STATUS: actif — joué tel quel par le job `firmware-build` de .gitea/workflows/ci.yml ;
#         ticket #528 : la panique du chien de garde et l'ISR de l'UART en IRAM vérifiées sur le build ;
#         ticket #574 : deux cibles, la XIAO ESP32-S3 du banc et l'ESP32-C3 de la carte v1.0
#
# Usage : bash 4_Firmware/build.sh [esp32s3|esp32c3]...      (ESP-IDF 5.3.2 chargé : . $IDF_PATH/export.sh)
#         sans argument, les deux cibles.
#
# Chaque cible se construit dans 4_Firmware/build/<cible>, son sdkconfig compris (-DSDKCONFIG), jamais dans
# 4_Firmware/ : rien de versionné n'est réécrit par un build (le piège du sdkconfig de la v1, CLAUDE.md § Gotchas).
# ESP-IDF y applique sdkconfig.defaults, puis sdkconfig.defaults.<cible>. Puis, pour chaque cible, la console muette
# est vérifiée sur ce qui a réellement été compilé : le niveau de journal par défaut, le niveau maximal (celui qui
# compile ou retire chaque ESP_LOG) et celui du bootloader valent 0 (NONE), dans le sdkconfig et le sdkconfig.h du
# build. De même, ce qu'aucun test hôte ne joue et que la carte seule prouvera (ticket #528) : le chien de garde
# initialisé au démarrage et qui redémarre la carte (INIT, PANIC), l'ISR de l'UART en IRAM (sdkconfig.defaults dit
# pourquoi).
set -euo pipefail
ICI=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
command -v idf.py >/dev/null || { echo "idf.py introuvable : charger ESP-IDF (. \$IDF_PATH/export.sh)" >&2; exit 2; }
cibles=("$@")
[ ${#cibles[@]} -gt 0 ] || cibles=(esp32s3 esp32c3)
for cible in "${cibles[@]}"; do
    case "$cible" in
    esp32s3|esp32c3) ;;
    *) echo "cible inconnue : $cible (esp32s3 ou esp32c3)" >&2; exit 2 ;;
    esac
done

for cible in "${cibles[@]}"; do
    echo "== $cible"
    build="$ICI/build/$cible"
    rm -rf "$build"
    idf.py -C "$ICI" -B "$build" -DIDF_TARGET="$cible" -DSDKCONFIG="$build/sdkconfig" build

    muet=0
    for o in CONFIG_LOG_DEFAULT_LEVEL CONFIG_LOG_MAXIMUM_LEVEL CONFIG_BOOTLOADER_LOG_LEVEL; do
        if ! grep -qx "$o=0" "$build/sdkconfig" || ! grep -qx "#define $o 0" "$build/config/sdkconfig.h"; then
            echo "$cible : console non muette : $o ne vaut pas 0 dans le sdkconfig ou le sdkconfig.h du build" >&2
            muet=1
        fi
    done
    [ "$muet" -eq 0 ]
    echo "$cible : console muette : journal (défaut et maximum) et bootloader à 0 dans le sdkconfig et le sdkconfig.h du build"

    sur=0
    for o in CONFIG_ESP_TASK_WDT_INIT CONFIG_ESP_TASK_WDT_PANIC CONFIG_UART_ISR_IN_IRAM; do
        if ! grep -qx "$o=y" "$build/sdkconfig" || ! grep -qx "#define $o 1" "$build/config/sdkconfig.h"; then
            echo "$cible : sûreté : $o n'est pas posée dans le sdkconfig ou le sdkconfig.h du build" >&2
            sur=1
        fi
    done
    [ "$sur" -eq 0 ]
    echo "$cible : sûreté : chien de garde initialisé et en panique, ISR de l'UART en IRAM, dans le sdkconfig et le sdkconfig.h du build"
done
