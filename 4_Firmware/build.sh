#!/usr/bin/env bash
# SOURCE: build ESP-IDF 5.3.2 de 4_Firmware/, cible esp32c3 (la carte v1.0)
# AUTHOR: engineer
# DATE: 2026-10-11
# STATUS: actif — joué tel quel par le job `firmware-build` de .gitea/workflows/ci.yml et par le job `build` de
#         .github/workflows/firmware.yml ;
#         ticket #528 : la panique du chien de garde et l'ISR de l'UART en IRAM vérifiées sur le build ;
#         ticket #574 : une seconde cible, l'ESP32-C3 de la carte v1.0 ;
#         ticket #602 : l'artefact de chaque cible, build/<cible>/artefact/ (fichiers nommés, image unique, scripts de flash) ;
#         ticket #606 : la cible esp32c3 seule (la carte de banc est retirée du projet)
#
# Usage : bash 4_Firmware/build.sh      (ESP-IDF 5.3.2 chargé : . $IDF_PATH/export.sh)
#
# Le firmware se construit dans 4_Firmware/build/esp32c3, son sdkconfig compris (-DSDKCONFIG), jamais dans
# 4_Firmware/ : rien de versionné n'est réécrit par un build (le piège du sdkconfig de la v1, CLAUDE.md § Gotchas).
# ESP-IDF y applique sdkconfig.defaults, puis sdkconfig.defaults.esp32c3. Puis la console muette
# est vérifiée sur ce qui a réellement été compilé : le niveau de journal par défaut, le niveau maximal (celui qui
# compile ou retire chaque ESP_LOG) et celui du bootloader valent 0 (NONE), dans le sdkconfig et le sdkconfig.h du
# build. De même, ce qu'aucun test hôte ne joue et que la carte seule prouvera (ticket #528) : le chien de garde
# initialisé au démarrage et qui redémarre la carte (INIT, PANIC), l'ISR de l'UART en IRAM (sdkconfig.defaults dit
# pourquoi).
set -euo pipefail
ICI=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
command -v idf.py >/dev/null || { echo "idf.py introuvable : charger ESP-IDF (. \$IDF_PATH/export.sh)" >&2; exit 2; }
[ $# -eq 0 ] || { echo "usage : bash 4_Firmware/build.sh (sans argument : la cible esp32c3, la seule)" >&2; exit 2; }
cible=esp32c3

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

# L'artefact (ticket #602), dans build/<cible>/artefact/, à plat : chaque fichier à flasher porte le nom du projet,
# la version (PROJECT_VER, lue dans le project_description.json du build) et la cible, pour qu'on ne flashe pas
# celui d'une autre carte. flash_args y est réécrit avec ces noms : il marche depuis ce dossier. Un fichier qu'il
# listerait sans nom prévu ici fait échouer le build plutôt que d'être livré sans être renommé.
art="$build/artefact"
mkdir "$art"
nom=$(python - "$build" "$cible" "$art" <<'PY'
import json, os, shutil, sys
build, cible, art = sys.argv[1:]
d = json.load(open(os.path.join(build, "project_description.json")))
nom = "%s-v%s-%s" % (d["project_name"], d["project_version"], cible)
noms = {"bootloader/bootloader.bin": nom + "-bootloader.bin",
        "partition_table/partition-table.bin": nom + "-partition-table.bin",
        d["app_bin"]: nom + ".bin"}
lignes = open(os.path.join(build, "flash_args")).read().split("\n")
sortie = [lignes[0]]
for l in filter(None, lignes[1:]):
    adresse, chemin = l.split()
    shutil.copyfile(os.path.join(build, chemin), os.path.join(art, noms[chemin]))
    sortie.append("%s %s" % (adresse, noms.pop(chemin)))
assert not noms, "flash_args ne liste pas %s" % sorted(noms)
open(os.path.join(art, "flash_args"), "w").write("\n".join(sortie) + "\n")
print(nom)
PY
)
# L'image unique, à écrire en 0x0 : merge_bin remplit de 0xFF les trous entre les morceaux, NVS comprise ; elle
# remet donc la marque à zéro. Les scripts de flash ne l'écrivent pas : ils passent par flash_args.
(cd "$art" && python -m esptool --chip "$cible" merge_bin -o "$nom-full.bin" @flash_args)
sed "s/@CIBLE@/$cible/" "$ICI/flash/flash.sh" > "$art/flash.sh"
sed -e "s/@CIBLE@/$cible/" -e 's/$/\r/' "$ICI/flash/flash.cmd" > "$art/flash.cmd"
echo "$cible : artefact :" $(cd "$art" && ls)
