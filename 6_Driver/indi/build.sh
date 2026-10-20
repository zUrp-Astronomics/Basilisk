#!/usr/bin/env bash
# SOURCE: 6_Driver/indi/README.md (cmake puis make) — le driver INDI compilé et lié
# AUTHOR: engineer
# DATE: 2026-10-03
# STATUS: actif — joué par le job `indi` de la CI ; se lance depuis n'importe quel répertoire (révisé le 2026-10-19,
#         ticket #645 : libindi 2)
# Compile et lie le driver par son CMakeLists.txt, tel quel, dans build/ à côté de ce script (recréé à chaque fois) :
# build/indi_pinefeat_cef_focus. Prérequis : cmake, make, g++, pkg-config, libindi-dev 2.x. Il n'installe rien ; ce
# que le driver fait est joué par test.sh.
set -euo pipefail
ICI=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
build="$ICI/build"
rm -rf "$build"
cmake -S "$ICI" -B "$build"
make -C "$build"
