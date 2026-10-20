#!/usr/bin/env bash
# SOURCE: 6_Driver/indi/README.md (§ Ce que la CI démontre)
# AUTHOR: engineer
# DATE: 2026-10-19
# STATUS: actif — joué par le job `indi` de la CI (ticket #645) ; se lance depuis n'importe quel répertoire
# Ce que la CI démontre du driver, dans build-test/ à côté de ce script (recréé à chaque fois) :
#   1. le driver de ce répertoire est l'amont nommé, puis patches/fixes, puis patches/basilisk : les deux séries,
#      retirées de ses fichiers dans l'ordre inverse, rendent les fichiers d'indilib au commit nommé (leurs identifiants
#      de blob git) ; appliquées sur eux, elles rendent ses fichiers, octet pour octet ;
#   2. chaque série suit le style d'indilib : les lignes qu'elle touche passent astyle avec les options du README
#      d'indilib, comme son scripts/check-codestyle.sh ;
#   3. les trois états compilés sans avertissement (l'amont, l'amont et les correctifs, le driver de ce répertoire)
#      et joués par test/test_driver.py contre un faux contrôleur Pinefeat, puis le driver contre une fausse carte
#      Basilisk.
# Prérequis : git, g++, pkg-config, libindi-dev 2.x, astyle, python3. Ne lit rien hors de ce répertoire.
set -euo pipefail
ICI=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)

# L'amont visé (UPSTREAM.md) : indilib/indi au commit de la v2.2.5, drivers/focuser/pinefeat_cef.{h,cpp}
UPSTREAM_COMMIT=1dd9b34f24df6fd74ed45be17823fc937e192b47
UPSTREAM_BLOB_CPP=8ba29fef695e36f8c2267e27fbfea8a28556e03b
UPSTREAM_BLOB_H=4d99784cafa6f8375a65a096b11f42e1dc200449
ASTYLE_OPTIONS="--style=allman --align-reference=name --indent-switches --indent-modifiers --indent-classes"
ASTYLE_OPTIONS+=" --pad-oper --indent-col1-comments --lineend=linux --max-code-length=124"

work="$ICI/build-test"
rm -rf "$work"
mkdir -p "$work"
f=drivers/focuser

# Un arbre d'indilib réduit au driver : un dépôt git, pour que git apply et astyle y lisent les chemins des patchs
arbre() {
    mkdir -p "$work/$1/$f"
    git -C "$work/$1" init -q
    git -C "$work/$1" config user.email test@localhost
    git -C "$work/$1" config user.name test
}

echo "── 1. Les séries sur l'amont ${UPSTREAM_COMMIT}"
arbre upstream
cp "$ICI/pinefeat_cef.h" "$ICI/pinefeat_cef.cpp" "$work/upstream/$f/"
mapfile -t fixes < <(ls "$ICI"/patches/fixes/*.patch)
mapfile -t basilisk < <(ls "$ICI"/patches/basilisk/*.patch)
for ((i = ${#basilisk[@]} - 1; i >= 0; i--)); do git -C "$work/upstream" apply -R "${basilisk[$i]}"; done
for ((i = ${#fixes[@]} - 1; i >= 0; i--)); do git -C "$work/upstream" apply -R "${fixes[$i]}"; done
for x in cpp:$UPSTREAM_BLOB_CPP h:$UPSTREAM_BLOB_H; do
    obtenu=$(git -C "$work/upstream" hash-object "$f/pinefeat_cef.${x%%:*}")
    if [ "$obtenu" != "${x#*:}" ]; then
        echo "pinefeat_cef.${x%%:*} sans les séries : blob $obtenu, l'amont est ${x#*:}" >&2
        exit 1
    fi
done
git -C "$work/upstream" add -A && git -C "$work/upstream" commit -q -m upstream
echo "ok   sans les deux séries, les fichiers sont ceux de l'amont (blobs $UPSTREAM_BLOB_CPP, $UPSTREAM_BLOB_H)"

# Chaque série appliquée par git am, commit par commit, sur l'état qui la précède ; le style vérifié sur les lignes
# qu'elle touche (scripts/check-codestyle.sh d'indilib : les fonctions modifiées, passées à astyle)
serie() {
    local nom=$1 avant=$2; shift 2
    cp -r "$work/$avant" "$work/$nom"
    git -C "$work/$nom" am -q "$@"
    local base
    base=$(git -C "$work/$avant" rev-parse HEAD)
    for x in h cpp; do
        local diff="$work/$nom.$x.diff"
        # shellcheck disable=SC2086
        git -C "$work/$nom" diff --function-context --unified=1 "$base" HEAD -- "$f/pinefeat_cef.$x" \
            | sed -e '/diff --git/,+3d;/^@@/d;/^-/d;s/^+/ /' | cut -d' ' -f2- > "$diff"
        # shellcheck disable=SC2086
        if ! astyle $ASTYLE_OPTIONS -n < "$diff" | diff -u "$diff" - > "$diff.style"; then
            echo "$nom : pinefeat_cef.$x hors du style d'indilib" >&2
            cat "$diff.style" >&2
            exit 1
        fi
    done
    echo "ok   $nom : $# patch(s) appliqués par git am, style d'indilib"
}
serie fixes upstream "${fixes[@]}"
serie final fixes "${basilisk[@]}"
for x in h cpp; do
    if ! cmp -s "$work/final/$f/pinefeat_cef.$x" "$ICI/pinefeat_cef.$x"; then
        echo "l'amont, puis les deux séries, ne rendent pas pinefeat_cef.$x de ce répertoire" >&2
        diff -u "$ICI/pinefeat_cef.$x" "$work/final/$f/pinefeat_cef.$x" >&2 || true
        exit 1
    fi
done
echo "ok   l'amont, puis les correctifs, puis l'extension : les fichiers de ce répertoire"

echo "── 2. Les trois états compilés sans avertissement (libindi $(pkg-config --modversion libindi))"
for etat in upstream fixes final; do
    # shellcheck disable=SC2046
    g++ -std=c++17 -O1 -Wall -Wextra -Werror $(pkg-config --cflags libindi) -o "$work/$etat.bin" \
        "$work/$etat/$f/pinefeat_cef.cpp" -lindidriver
done
echo "ok   upstream.bin, fixes.bin, final.bin"

echo "── 3. Les trois états joués"
echec=0
for jeu in "upstream pinefeat" "fixes pinefeat" "final pinefeat" "final basilisk"; do
    set -- $jeu
    echo "── $1, $2"
    python3 "$ICI/test/test_driver.py" "$work/$1.bin" "$1" "$2" || echec=1
done
exit $echec
