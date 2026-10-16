#!/usr/bin/env python3
# SOURCE: ticket #617 — le manifest.json d'une release GitHub du firmware, au format ESP Web Tools
# AUTHOR: engineer
# DATE: 2026-10-12
# STATUS: actif — appelé par le job `release` de .github/workflows/firmware.yml, sur la poussée d'un tag v*.*.*
#
# Usage : python3 manifest.py <tag> <image> > manifest.json
#
# Écrit sur la sortie standard le manifeste qu'ESP Web Tools lit pour installer le firmware : la release porte, à côté
# de lui, une seule image, <image> (basilisk_<tag>_esp32c3.bin, l'image fusionnée de l'artefact, à écrire en 0), qu'il
# désigne par son nom, relatif au manifeste. La version est le tag (v0.9.0), pas PROJECT_VER (0.9).
# new_install_prompt_erase : ESP Web Tools propose d'effacer la flash à une nouvelle installation ; l'image remet de
# toute façon la NVS, donc la marque, à zéro.
import json
import sys


def main(argv):
    if len(argv) != 2:
        print("usage : python3 manifest.py <tag> <image> > manifest.json", file=sys.stderr)
        return 2
    tag, image = argv
    m = {
        "name": "Basilisk",
        "version": tag,
        "new_install_prompt_erase": True,
        "builds": [{"chipFamily": "ESP32-C3", "parts": [{"path": image, "offset": 0}]}],
    }
    json.dump(m, sys.stdout, indent=2)
    sys.stdout.write("\n")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
