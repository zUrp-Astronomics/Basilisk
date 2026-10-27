#!/usr/bin/env python3
# Le test de l'outillage de release du firmware : la garde de la NVS (4_Firmware/garde_nvs.py) et le manifeste
# (4_Firmware/manifest.py), joués comme build.sh et le job `release` les appellent, en sous-processus.
#
# Usage : python3 test_release.py <manifest.py> <garde_nvs.py>
#
# La garde est jouée sur une table de partitions binaire écrite ici, au format d'ESP-IDF, avec les adresses de
# partitions_singleapp_large.csv d'ESP-IDF 5.3.2 (sdkconfig.defaults) recopiées à la main : nvs en 0x9000 (0x6000),
# phy_init en 0xf000 (0x1000), factory en 0x10000 (1500 Ko). Les entrées de flash_args sont celles du build (0x0
# bootloader, 0x8000 table, 0x10000 application), puis des copies fautives, chacune une entrée qui recouvre la NVS, et
# ses bords, qui ne la recouvrent pas ; puis une table dont la NVS est agrandie à 0x8000, pour prouver que sa taille
# est lue dans la table, et un mauvais nombre d'arguments (0, 1, 3). Le manifeste est comparé, octet pour octet, à un
# attendu écrit à la main d'après la norme de l'org (le contrat avec le site, ticket #670).
# Sort en 0 si tout passe, en 1 sinon. Rien n'est écrit dans le dépôt (un répertoire temporaire).
import hashlib
import os
import struct
import subprocess
import sys
import tempfile

ECHECS = 0


def verifie(ok, quoi, obtenu):
    """obtenu : ce que le script a rendu, affiché en cas d'échec seulement."""
    global ECHECS
    print("   %s : %s" % ("ok" if ok else "ÉCHEC", quoi))
    if not ok:
        print("      obtenu : %r" % (obtenu,))
        ECHECS += 1


def joue(script, *args):
    p = subprocess.run([sys.executable, script, *args], capture_output=True, text=True)
    return p.returncode, p.stdout, p.stderr


def entree(type_, sous_type, adresse, taille, nom):
    return b"\xaa\x50" + struct.pack("<BBII", type_, sous_type, adresse, taille) + nom.ljust(16, b"\0") + b"\0" * 4


def table(*entrees):
    corps = b"".join(entrees)
    return (corps + b"\xeb\xeb" + b"\xff" * 14 + hashlib.md5(corps).digest()).ljust(0xC00, b"\xff")


TABLE = table(entree(1, 2, 0x9000, 0x6000, b"nvs"),
              entree(1, 1, 0xF000, 0x1000, b"phy_init"),
              entree(0, 0, 0x10000, 0x177000, b"factory"))
TETE = "--flash_mode dio --flash_freq 80m --flash_size 4MB"


def garde(script, dossier, cas, tables=TABLE):
    """cas : [(adresse, taille du fichier)] ; rend (code, stdout, stderr) de la garde sur ce flash_args."""
    d = tempfile.mkdtemp(dir=dossier)
    lignes = [TETE]
    for i, (adresse, taille) in enumerate(cas):
        with open(os.path.join(d, "f%d.bin" % i), "wb") as f:
            f.write(b"\x5a" * taille)
        lignes.append("0x%x f%d.bin" % (adresse, i))
    with open(os.path.join(d, "flash_args"), "w") as f:
        f.write("\n".join(lignes) + "\n")
    with open(os.path.join(d, "partition-table.bin"), "wb") as f:
        f.write(tables)
    return joue(script, os.path.join(d, "flash_args"), os.path.join(d, "partition-table.bin"))


def test_garde(script):
    print("== la garde de la NVS (garde_nvs.py)")
    with tempfile.TemporaryDirectory() as dossier:
        reel = [(0x0, 0x5000), (0x8000, 0xC00), (0x10000, 0xC0000)]
        c, out, err = garde(script, dossier, reel)
        verifie(c == 0 and out == "garde de la NVS : 3 entrées de flash_args, "
                                  "aucune sur la partition nvs 0x9000-0xf000\n",
                "les entrées du build passent, et la plage lue est 0x9000-0xf000", (c, out, err))

        bords = [(0x8000, 0x1000), (0xF000, 0x1000)]
        c, out, err = garde(script, dossier, bords)
        verifie(c == 0, "une entrée qui finit en 0x9000 et une qui commence en 0xf000 passent", (c, err))

        fautifs = [
            ("la table de partitions qui déborde d'un octet sur la NVS",
             [(0x0, 0x5000), (0x8000, 0x1001), (0x10000, 0xC0000)],
             "f1.bin écrit 0x8000-0x9001, sur la partition nvs 0x9000-0xf000"),
            ("une entrée dont le dernier octet est le dernier de la NVS", [(0xE000, 0x1000)],
             "f0.bin écrit 0xe000-0xf000, sur la partition nvs 0x9000-0xf000"),
            ("une entrée qui commence au dernier octet de la NVS", [(0xEFFF, 0x10)],
             "f0.bin écrit 0xefff-0xf00f, sur la partition nvs 0x9000-0xf000"),
            ("une entrée dans la NVS", [(0xA000, 0x100)],
             "f0.bin écrit 0xa000-0xa100, sur la partition nvs 0x9000-0xf000"),
            ("un bootloader qui couvre toute la NVS", [(0x0, 0x10000)],
             "f0.bin écrit 0x0-0x10000, sur la partition nvs 0x9000-0xf000"),
        ]
        for quoi, cas, attendu in fautifs:
            c, out, err = garde(script, dossier, cas)
            verifie(c == 1 and attendu in err and out == "", "refusée : %s" % quoi, (c, err))

        c, out, err = garde(script, dossier, [(0x0, 0x5000), (0x9000, 0x10), (0xA000, 0x10)])
        verifie(c == 1 and err.count("sur la partition nvs") == 2, "deux entrées fautives, deux lignes", (c, err))

        deplacee = table(entree(1, 2, 0x9000, 0x6000, b"autre"), entree(1, 2, 0x20000, 0x6000, b"nvs"),
                         entree(0, 0, 0x10000, 0x10000, b"factory"))
        c, out, err = garde(script, dossier, reel, deplacee)
        verifie(c == 1 and err == "garde de la NVS : f2.bin écrit 0x10000-0xd0000, "
                                  "sur la partition nvs 0x20000-0x26000 : la marque serait effacée\n",
                "la NVS est lue dans la table, par son nom : déplacée en 0x20000 (une autre partition de données NVS, "
                "`autre`, en 0x9000), l'application la recouvre", (c, err))

        for quoi, t in [("sans partition nvs", table(entree(0, 0, 0x10000, 0x177000, b"factory"))),
                        ("avec deux partitions nvs", table(entree(1, 2, 0x9000, 0x6000, b"nvs"),
                                                          entree(1, 2, 0x200000, 0x6000, b"nvs")))]:
            c, out, err = garde(script, dossier, reel, t)
            verifie(c == 1 and "il en faut une" in err, "refusée : une table %s" % quoi, (c, err))

        c, out, err = garde(script, dossier, reel, b"\x00" * 0xC00)
        verifie(c == 1 and "table de partitions illisible" in err, "refusée : une table illisible", (c, err))

        agrandie = table(entree(1, 2, 0x9000, 0x8000, b"nvs"), entree(0, 0, 0x20000, 0x10000, b"factory"))
        c, out, err = garde(script, dossier, [(0xF000, 0x1000)], agrandie)
        verifie(c == 1 and err == "garde de la NVS : f0.bin écrit 0xf000-0x10000, "
                                  "sur la partition nvs 0x9000-0x11000 : la marque serait effacée\n",
                "la taille de la NVS est lue dans la table : agrandie à 0x8000, une entrée qui ne recouvre que la "
                "partie ajoutée (0xf000-0x11000) est refusée", (c, err))

        for args in [(), ("flash_args",), ("flash_args", "partition-table.bin", "en trop")]:
            c, out, err = joue(script, *args)
            verifie(c == 2 and out == "" and err == "usage : python3 garde_nvs.py <flash_args> <partition-table.bin>\n",
                    "%d arguments : refusé en 2, l'usage sur l'erreur" % len(args), (c, out, err))


ATTENDU = """{
  "name": "Basilisk",
  "version": "1.0.1",
  "new_install_prompt_erase": true,
  "builds": [
    {
      "chipFamily": "ESP32-C3",
      "parts": [
        {
          "path": "basilisk_1.0.1_esp32c3.bin",
          "offset": 0
        }
      ]
    }
  ]
}
"""


def test_manifeste(script):
    print("== le manifeste (manifest.py)")
    c, out, err = joue(script, "1.0.1", "basilisk_1.0.1_esp32c3.bin")
    verifie(c == 0 and out == ATTENDU and err == "", "tag v1.0.1 : le manifeste attendu, octet pour octet", (c, out))
    for args in [(), ("1.0.1",), ("1.0.1", "basilisk_1.0.1_esp32c3.bin", "en trop")]:
        c, out, err = joue(script, *args)
        verifie(c == 2 and out == "" and err.startswith("usage"),
                "%d arguments : refusé en 2, rien sur la sortie" % len(args), (c, out, err))


def main(argv):
    if len(argv) != 2:
        print("usage : python3 test_release.py <manifest.py> <garde_nvs.py>", file=sys.stderr)
        return 2
    test_garde(argv[1])
    test_manifeste(argv[0])
    print("%s" % ("ÉCHEC : %d vérifications" % ECHECS if ECHECS else "   tout passe"))
    return 1 if ECHECS else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
