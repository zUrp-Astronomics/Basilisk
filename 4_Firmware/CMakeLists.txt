# SOURCE: le projet ESP-IDF 5.3.2 de 4_Firmware/, cibles esp32s3 (la XIAO du banc) et esp32c3 (la carte v1.0) (spec § 12, § 13)
# AUTHOR: engineer
# DATE: 2026-10-10
# STATUS: actif — construit par 4_Firmware/build.sh (job `firmware-build` de .gitea/workflows/ci.yml), une fois par
#         cible ; PROJECT_VER rendu tel quel par `v` ; ticket #574 : la cible esp32c3
cmake_minimum_required(VERSION 3.16)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
set(PROJECT_VER "1.0")   # la seule source de `v` (<maj>.<min>, sans suffixe : décision de l'humain)
project(emount_bench2)
