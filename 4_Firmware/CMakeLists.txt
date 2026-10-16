# SOURCE: le projet ESP-IDF 5.3.2 de 4_Firmware/, cible esp32c3 (la carte v1.0) (spec § 12, § 13)
# AUTHOR: engineer
# DATE: 2026-10-11
# STATUS: actif — construit par 4_Firmware/build.sh (job `firmware-build` de .gitea/workflows/ci.yml) ;
#         PROJECT_VER rendu tel quel par `v` ; ticket #574 : la cible esp32c3 ;
#         ticket #602 : le projet s'appelle basilisk_FW ; ticket #606 : la cible esp32c3 seule, la version 0.9
cmake_minimum_required(VERSION 3.16)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
set(PROJECT_VER "0.9")   # la seule source de `v` (<maj>.<min>, sans suffixe : décision de l'humain)
project(basilisk_FW)   # le nom du firmware : son binaire, l'en-tête de l'application, les fichiers livrés (build.sh)
