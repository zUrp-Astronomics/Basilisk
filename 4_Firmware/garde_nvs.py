#!/usr/bin/env python3
# La garde de la NVS : aucune entrée de flash_args n'écrit dans la partition `nvs`, celle qui range la marque
# (nvs_flash_init, components/store/store.c, ouvre la partition de ce nom).
#
# Usage : python3 garde_nvs.py <flash_args> <partition-table.bin>
#
# flash.sh, flash.cmd et la procédure manuelle du README gardent la marque parce qu'ils n'écrivent que ce que liste
# flash_args. Une entrée déborde si la plage qu'elle écrit, son adresse plus la taille de son fichier (chemin relatif au
# dossier de flash_args, comme esptool le lit), recouvre la plage de la partition `nvs`. Cette plage est lue dans la
# table de partitions binaire du build, celle qu'on flashe, jamais écrite ici : une table ou un bootloader qui
# changeraient effaceraient sinon la marque en silence.
# Table binaire d'ESP-IDF : des entrées de 32 octets, AA 50, type, sous-type, adresse et taille (u32 petit-boutistes),
# nom (16 octets, complétés de zéros), drapeaux ; une ligne EB EB porte la somme MD5 ; la table finit au premier
# FF FF.
# Sort en 0 si aucune entrée ne déborde, en 1 sinon (une ligne par entrée fautive) ou si la table ne porte pas
# exactement une partition `nvs`, en 2 sur un mauvais usage.
import os
import struct
import sys


def nvs(table):
    trouvees = []
    for i in range(0, len(table) - 31, 32):
        e = table[i:i + 32]
        if e[:2] == b"\xaa\x50":
            adresse, taille = struct.unpack_from("<II", e, 4)
            if e[12:28].rstrip(b"\0") == b"nvs":
                trouvees.append((adresse, taille))
        elif e[:2] == b"\xff\xff":
            break
        elif e[:2] != b"\xeb\xeb":
            sys.exit("garde de la NVS : table de partitions illisible, entrée inconnue à l'octet %d" % i)
    if len(trouvees) != 1:
        sys.exit("garde de la NVS : la table de partitions porte %d partitions `nvs`, il en faut une" % len(trouvees))
    return trouvees[0]


def main(argv):
    if len(argv) != 2:
        print("usage : python3 garde_nvs.py <flash_args> <partition-table.bin>", file=sys.stderr)
        return 2
    flash_args, chemin_table = argv
    with open(chemin_table, "rb") as f:
        debut, taille = nvs(f.read())
    fin = debut + taille
    dossier = os.path.dirname(os.path.abspath(flash_args))
    with open(flash_args) as f:
        entrees = [l.split() for l in f.read().split("\n")[1:] if l.strip()]
    fautes = 0
    for adresse, chemin in entrees:
        a = int(adresse, 0)
        b = a + os.path.getsize(os.path.join(dossier, chemin))
        if a < fin and debut < b:
            print("garde de la NVS : %s écrit 0x%x-0x%x, sur la partition nvs 0x%x-0x%x : la marque serait effacée"
                  % (chemin, a, b, debut, fin), file=sys.stderr)
            fautes += 1
    if fautes:
        return 1
    print("garde de la NVS : %d entrées de flash_args, aucune sur la partition nvs 0x%x-0x%x"
          % (len(entrees), debut, fin))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
