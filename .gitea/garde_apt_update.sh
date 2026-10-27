#!/usr/bin/env bash
# apt_update existe en trois copies qui doivent rester identiques au caractère près : celle de
# .gitea/setup_host.sh (le job `test` et la sonde), et celles des étapes Outillage des jobs `indi` et
# `firmware` de .gitea/workflows/ci.yml, écrites en ligne parce qu'apt y passe avant git. Ce script échoue
# si setup_host.sh n'en porte pas exactement une, si ci.yml n'en porte pas exactement deux, ou si une copie de
# ci.yml, une fois désindentée, diffère de celle de setup_host.sh. Joué par le job `test` de ci.yml et par
# l'essai du repli (.gitea/workflows/trial-apt-repli.yml).
set -euo pipefail
ICI=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

# copies <fichier> <préfixe> : écrit chaque définition d'apt_update du fichier, désindentée, dans
# <préfixe>.1, <préfixe>.2… et affiche leur nombre.
copies() {
    awk -v pre="$2" '
        !dans && /^ *apt_update\(\) \{$/ { dans = 1; n++; ind = index($0, "a") - 1; fin = substr($0, 1, ind) "}" }
        dans { print substr($0, ind + 1) > (pre "." n) }
        dans && $0 == fin { dans = 0 }
        END { print n + 0 }' "$1"
}

n=$(copies "$ICI/setup_host.sh" "$TMP/hote")
[ "$n" = 1 ] || { echo "setup_host.sh : $n définitions d'apt_update, 1 attendue" >&2; exit 1; }
n=$(copies "$ICI/workflows/ci.yml" "$TMP/ci")
[ "$n" = 2 ] || { echo "ci.yml : $n définitions d'apt_update, 2 attendues (jobs indi et firmware)" >&2; exit 1; }
for i in 1 2; do
    diff -u "$TMP/hote.1" "$TMP/ci.$i" || { echo "ci.yml : la copie $i d'apt_update diffère de setup_host.sh" >&2; exit 1; }
done
echo "apt_update : trois copies identiques ($(wc -l < "$TMP/hote.1") lignes)"
