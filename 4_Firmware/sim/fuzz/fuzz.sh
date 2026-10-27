#!/usr/bin/env bash
# Le script d'entrée du banc de fuzzing de 4_Firmware/ (README.md à côté) : hors de la suite (CLAUDE.md § Test) et de la
# CI (ci.yml), aucun script de la suite ne l'appelle ni ne lit ce répertoire.
#
#   fuzz.sh construit <sortie>                    les onze harnais, compilés dans <sortie> comme la suite les compile
#                                                 (-std=c11 -Wall -Wextra -Wpedantic -Werror -O2) avec les sanitizers de
#                                                 run.sh (SAN), leurs sources lues dans sim/test/programmes.sh
#   fuzz.sh campagne <sortie> <cible> <graine> <n>  n cas tirés de la graine ; chaque premier échec d'une vérification
#                                                 réduit et rangé sous cas/<cible>/, nommé par sa propriété
#   fuzz.sh enum <sortie> [<processus>]           les énumérations : sht40 (les 131072 mots), journal (chaque fonction,
#                                                 chaque champ à sa borne), climat (les 175 millions de couples, en
#                                                 tranches sur <processus> processus, nproc par défaut)
#   fuzz.sh rejoue <sortie> <cas>                 un cas rangé, rejoué ; sa cible est le nom de son répertoire
#   fuzz.sh reduit <sortie> <cas> <réduit>        un cas réduit tant que la même vérification échoue
#   fuzz.sh rejoue-tout <sortie>                  chaque cas rangé rejoué deux fois, les deux sorties comparées octet pour
#                                                 octet, et le verdict attendu (celui que porte son nom) vérifié
#   fuzz.sh mord <sortie> [<harnais> [<n°>…]]     chaque faute de fautes.txt (de ce harnais seul, ces n° seuls, s'ils
#                                                 sont donnés)
#                                                 injectée dans une copie temporaire de 4_Firmware/, son harnais
#                                                 recompilé et joué : l'oracle visé doit mordre ; MORD_JOBS fautes à la
#                                                 fois (nproc par défaut)
#   fuzz.sh libfuzzer <sortie> <cible> <s> [<g>]  F4 : la cible d'octets (rx, lens_rx, bench, ligne) sous libFuzzer, s
#                                                 secondes au plus (-max_total_time, posé avant de lancer ; g : son
#                                                 -seed, 1 par défaut), depuis les graines du générateur et les cas
#                                                 rangés de F1 ; chaque trouvaille rejouée deux fois par le pilote gcc,
#                                                 réduite et imprimée en hexadécimal ; la couverture des graines seules
#                                                 et du corpus final comparée. Il faut clang (CLANG), libFuzzer et,
#                                                 pour lire la couverture, llvm-symbolizer dans le PATH
#
# <sortie> est un répertoire hors du dépôt (rien n'y est écrit d'autre que cas/). Les chemins partent de ce script : il
# se lance depuis n'importe quel répertoire. Il lit, hors de sim/fuzz/, les sources du firmware et leur liste,
# sim/test/programmes.sh, sous la racine de 4_Firmware/ (F2 ci-dessous).
set -euo pipefail
ICI=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
F2=$(cd "$ICI/../.." && pwd)    # la racine de 4_Firmware/ : les sources que les harnais compilent
CAS="$ICI/cas"
CIBLES=(rx lens_rx bench sht40 journal climat rtc session hote commande ligne)
CIBLES_LF=(rx lens_rx bench ligne)                         # les cibles d'octets (F4) ; les cibles à scénario n'y sont pas
CFLAGS=(-std=c11 -Wall -Wextra -Wpedantic -Werror -O2)
SAN=(-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer)   # run.sh, sans GARDE_S
# clang, sous libFuzzer : les avertissements imprimés sans arrêter (la suite et ses options sont à gcc) ; les sanitizers
# de SAN, et fuzzer.
LF_CFLAGS=(-std=c11 -Wall -Wextra -Wpedantic -O1 -g -DFZ_LIBFUZZER -fsanitize=fuzzer,address,undefined
           -fno-sanitize-recover=all -fno-omit-frame-pointer)
GRAINES=1000                                               # les cas du générateur (graine 1) donnés à libFuzzer

usage() {
    sed -n '5,28p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'
    exit 2
}

# compiler <racine> <cible> <sortie> [options…] : fuzz_<cible> sous <sortie>.
compiler() {
    local racine=$1 cible=$2 sortie=$3
    shift 3
    (
        . "$racine/sim/test/programmes.sh"
        programme_includes "$racine"
        programme_sources "$racine" "fuzz_$cible"
        gcc "${CFLAGS[@]}" "${SAN[@]}" "$@" "${INCLUDES[@]}" "${SOURCES[@]}" "${LIENS[@]}" -o "$sortie/fuzz_$cible"
    )
}

# lf_compiler <racine> <cible> <sortie> : lf_<cible> sous <sortie>, la même cible sous libFuzzer (fuzz.c, FZ_LIBFUZZER).
lf_compiler() {
    local racine=$1 cible=$2 sortie=$3
    (
        . "$racine/sim/test/programmes.sh"
        programme_includes "$racine"
        programme_sources "$racine" "fuzz_$cible"
        "${CLANG:-clang}" "${LF_CFLAGS[@]}" "${INCLUDES[@]}" "${SOURCES[@]}" "${LIENS[@]}" -o "$sortie/lf_$cible"
    )
}

construit() {
    mkdir -p "$1"
    gcc --version | head -n 1
    for c in "${CIBLES[@]}"; do
        compiler "$F2" "$c" "$1"
        echo "   fuzz_$c"
    done
}

# La cible d'un cas rangé : le nom de son répertoire (cas/<cible>/<propriété>-….bin).
cible_de() { basename "$(dirname "$1")"; }

rejoue_tout() {
    local sortie=$1 n=0 faux=0 cas cible attendu motif
    while IFS= read -r cas; do
        cible=$(cible_de "$cas")
        n=$((n + 1))
        "$sortie/fuzz_$cible" rejoue "$cas" > "$sortie/rejeu1.txt" 2>&1 || true
        "$sortie/fuzz_$cible" rejoue "$cas" > "$sortie/rejeu2.txt" 2>&1 || true
        attendu=$(basename "$cas" .bin | cut -d- -f1-2 | tr '-' ' ')
        case $attendu in                       # le processus meurt : le rapport du sanitizer, la ligne de la garde
            'P1 sanitizer') motif='runtime error|ERROR: AddressSanitizer' ;;
            'P2 garde') motif='^ECHEC P2 garde ' ;;
            *) motif="^VERDICT ECHEC $attendu " ;;
        esac
        if ! cmp -s "$sortie/rejeu1.txt" "$sortie/rejeu2.txt"; then
            echo "   ÉCHEC : ${cas#"$ICI/"} : deux rejeux, deux sorties"
            faux=$((faux + 1))
        elif ! grep -qE "$motif" "$sortie/rejeu1.txt"; then
            echo "   ÉCHEC : ${cas#"$ICI/"} : le verdict n'est plus « $attendu » ($(grep '^VERDICT' "$sortie/rejeu1.txt" || echo 'aucun'))"
            faux=$((faux + 1))
        else
            echo "   ${cas#"$ICI/"} : deux sorties identiques ($(wc -c < "$sortie/rejeu1.txt") octets), $(grep -m1 -E "$motif" "$sortie/rejeu1.txt" | cut -c1-60)"
        fi
    done < <(find "$CAS" -name '*.bin' | sort)
    echo "BILAN rejeu : $n cas, $faux écart(s)"
    [ "$faux" = 0 ]
}

enum() {
    local sortie=$1 procs=${2:-$(nproc)} t0 de a pas k
    "$sortie/fuzz_sht40" mots
    "$sortie/fuzz_journal" enum
    t0=$(date +%s)
    pas=$(( (17501 + procs - 1) / procs ))
    for ((k = 0; k < procs; k++)); do
        de=$(( -4500 + k * pas ))
        a=$(( de + pas - 1 ))
        [ "$a" -gt 13000 ] && a=13000
        [ "$de" -gt 13000 ] && break
        "$sortie/fuzz_climat" couples "$de" "$a" > "$sortie/climat_$k.txt" 2>&1 &
    done
    wait
    cat "$sortie"/climat_*.txt
    echo "BILAN climat : $procs processus, $(( $(date +%s) - t0 )) s murales"
    ! grep -q '^ECHEC' "$sortie"/climat_*.txt
}

# une_faute <copie> <k> <prog> <fichier> <garde> <commande> <attendu> <fichier:orig:remp>… : la faute k injectée dans sa
# copie de 4_Firmware/, son harnais compilé et joué ; ce qu'elle imprime va dans <copie>/k.txt, et sa dernière ligne dit
# MORD ou non.
une_faute() {
    local copie=$1/$2 k=$2 prog=$3 fichier=$4 garde=$5 commande=$6 attendu=$7 i f o r
    shift 7
    mkdir -p "$copie/f2" "$copie/cas" "$copie/bin"
    cp -r "$F2/components" "$F2/include" "$F2/sim" "$copie/f2/"
    {
        echo "== faute $k : $prog, $fichier, attendu « $attendu »"
        for i in "$@"; do
            f=${i%%$'\x1f'*}; o=${i#*$'\x1f'}; r=${o#*$'\x1f'}; o=${o%%$'\x1f'*}
            if [ "$(grep -cF -- "$o" "$copie/f2/$f")" != 1 ]; then
                echo "   « $o » n'est pas exactement une fois dans $f"
                echo "NON"
                return
            fi
            ORIG="$o" REMP="$r" perl -0pi -e 's/\Q$ENV{ORIG}\E/$ENV{REMP}/' "$copie/f2/$f"
            diff -U0 --label "a/4_Firmware/$f" --label "b/4_Firmware/$f" "$F2/$f" "$copie/f2/$f" | sed 's/^/   /' || true
        done
        if ! compiler "$copie/f2" "${prog#fuzz_}" "$copie/bin" -DFZ_GARDE_S="$garde" 2> "$copie/cc.log"; then
            echo "   la copie fautive ne compile pas :"
            sed 's/^/      /' "$copie/cc.log" | head -n 5
            echo "NON"
            return
        fi
        # shellcheck disable=SC2086
        if [ "${commande%% *}" = campagne ]; then
            "$copie/bin/$prog" $commande "$copie/cas" > "$copie/sortie.txt" 2>&1 || true
        else
            "$copie/bin/$prog" $commande > "$copie/sortie.txt" 2>&1 || true
        fi
        if grep -aqF -- "$attendu" "$copie/sortie.txt"; then
            echo "   MORD : $(grep -a -m1 -F -- "$attendu" "$copie/sortie.txt" | cut -c1-220)"
            echo "OUI"
        else
            echo "   NE MORD PAS ($prog $commande) :"
            tail -n 4 "$copie/sortie.txt" | sed 's/^/      /'
            echo "NON"
        fi
    } > "$copie.txt" 2>&1
    rm -rf "$copie"
}

# mord <sortie> [<harnais>] : les blocs de fautes.txt (format en tête du fichier), MORD_JOBS à la fois (nproc par défaut),
# chacun dans sa copie ; les comptes rendus imprimés dans l'ordre du fichier.
mord() {
    local seul=${2:-} copie bloc=0 ok=0 garde tete prog attendu fichier commande orig ligne ex_f='' ex_o='' k
    local -a nums=("${@:3}") joues=()
    local jobs=${MORD_JOBS:-$(nproc)} actifs=0
    local -a EX=()
    copie=$(mktemp -d)
    trap 'rm -rf "$copie"' RETURN
    while IFS= read -r ligne || [ -n "$ligne" ]; do
        case "$ligne" in
            '@ '*) tete=${ligne#@ }; EX=() ;;
            '= '*) ex_f=${ligne#= } ;;
            '-- '*) ex_o=${ligne#-- } ;;
            '++ '*) EX+=("$ex_f"$'\x1f'"$ex_o"$'\x1f'"${ligne#++ }") ;;
            '- '*) orig=${ligne#- } ;;
            '+ '*)
                read -r prog fichier commande <<< "${tete%%|*}"
                attendu=${tete#*|}
                attendu=${attendu# }
                if [ -n "$seul" ] && [ "$prog" != "$seul" ]; then EX=(); continue; fi
                bloc=$((bloc + 1))
                if [ ${#nums[@]} -gt 0 ] && [[ " ${nums[*]} " != *" $bloc "* ]]; then EX=(); continue; fi
                joues+=("$bloc")
                garde=2
                case $prog in fuzz_session|fuzz_hote|fuzz_commande) garde=20 ;; esac
                if [ "$actifs" -ge "$jobs" ]; then wait -n || true; actifs=$((actifs - 1)); fi
                une_faute "$copie" "$bloc" "$prog" "$fichier" "$garde" "$commande" "$attendu" "${EX[@]}" \
                    "$fichier"$'\x1f'"$orig"$'\x1f'"${ligne#+ }" &
                actifs=$((actifs + 1))
                EX=()
                ;;
        esac
    done < "$ICI/fautes.txt"
    wait
    for k in "${joues[@]}"; do
        sed '$d' "$copie/$k.txt"
        [ "$(tail -n 1 "$copie/$k.txt")" = OUI ] && ok=$((ok + 1))
    done
    echo "BILAN morsures : $ok sur ${#joues[@]} fautes"
    [ "$ok" = "${#joues[@]}" ]
}

# couverture <journal de -print_coverage=1> : « <fonction> <fichier:ligne> <arêtes couvertes>/<arêtes> », une par
# fonction de components/, puis « PC <fichier:ligne> » pour chaque ligne d'une fonction couverte restée sans arête.
couverture() {
    awk '/^U?COVERED_FUNC:/ {
             for (i = 1; i <= NF; i++) if ($i == "edges:") { e = $(i + 1); f = $(i + 2); l = $(i + 3) }
             if (l ~ /components\//) print f, l, e
         }
         /UNCOVERED_PC:/ { if ($2 ~ /components\//) print "PC", $2 }' "$1" | sed 's|[^ ]*/4_Firmware/||g' | LC_ALL=C sort -u
}

# libfuzzer <sortie> <cible> <secondes> [<seed>] : en tête de ce fichier ; tout est écrit sous <sortie>/lf-<cible>/.
libfuzzer() {
    local sortie=$1 cible=$2 duree=$3 seed=${4:-1} lf t0 dt f v n=0 rep=0
    case " ${CIBLES_LF[*]} " in
        *" $cible "*) ;;
        *) echo "libfuzzer : « $cible » n'est pas une cible d'octets (${CIBLES_LF[*]})" >&2; exit 2 ;;
    esac
    lf=$sortie/lf-$cible
    rm -rf "$lf"
    mkdir -p "$lf/graines" "$lf/corpus" "$lf/trouve"
    echo "== F4, libFuzzer sur $cible : $("${CLANG:-clang}" --version | head -n 1) ; rejeu : $(gcc --version | head -n 1)"
    compiler "$F2" "$cible" "$sortie"
    lf_compiler "$F2" "$cible" "$sortie" 2> "$lf/clang.txt" || { cat "$lf/clang.txt"; exit 1; }
    echo "   lf_$cible lié ; avertissements de clang : $(grep -c 'warning:' "$lf/clang.txt" || true)"
    grep 'warning:' "$lf/clang.txt" | head -n 5 | sed 's/^/      /' || true
    for ((f = 0; f < GRAINES; f++)); do "$sortie/fuzz_$cible" ecrit 1 "$f" "$lf/graines/g1-$f.bin"; done
    case $cible in rx|lens_rx|bench) find "$CAS/$cible" -name '*.bin' -exec cp {} "$lf/graines/" \; 2> /dev/null || true ;; esac
    echo "   graines : $(find "$lf/graines" -type f | wc -l) cas ($GRAINES du générateur, graine 1, et les cas rangés de F1)"
    t0=$(date +%s)
    (cd "$lf" && "$sortie/lf_$cible" -max_total_time="$duree" -seed="$seed" -timeout=10 -rss_limit_mb=4096 \
        -print_final_stats=1 -artifact_prefix="$lf/trouve/" "$lf/corpus" "$lf/graines") > "$lf/campagne.txt" 2>&1 || true
    dt=$(( $(date +%s) - t0 ))
    echo "-- la campagne : $dt s murales (plafond $duree s, -seed=$seed)"
    grep -E 'INITED|DONE|^stat::|^Done |BILAN libFuzzer|^ECHEC|ERROR: (AddressSanitizer|libFuzzer|LeakSanitizer)|runtime error|^SUMMARY' \
        "$lf/campagne.txt" | sed 's/^/   /' || true
    for f in "$lf"/trouve/*; do
        [ -e "$f" ] || continue
        n=$((n + 1))
        echo "-- trouvaille $(basename "$f") : $(wc -c < "$f") octets"
        "$sortie/fuzz_$cible" rejoue "$f" > "$lf/r1.txt" 2>&1 || true
        "$sortie/fuzz_$cible" rejoue "$f" > "$lf/r2.txt" 2>&1 || true
        v=$(grep -m1 -aE '^VERDICT|^ECHEC P2 garde|runtime error|ERROR: AddressSanitizer' "$lf/r1.txt" || echo 'aucun verdict')
        cmp -s "$lf/r1.txt" "$lf/r2.txt" && echo "   deux rejeux gcc, deux sorties identiques : $v" ||
            echo "   deux rejeux gcc, deux sorties différentes : $v"
        if [ "$v" = 'VERDICT passe' ] || [ "$v" = 'aucun verdict' ] || ! cmp -s "$lf/r1.txt" "$lf/r2.txt"; then
            echo "   NON REPRODUIT SOUS GCC ; la sortie de libFuzzer :"
            tail -n 40 "$lf/campagne.txt" | sed 's/^/      /'
            continue
        fi
        rep=$((rep + 1))
        "$sortie/fuzz_$cible" reduit "$f" "$lf/reduit.bin" | sed 's/^/   /' || true
        echo "   le cas réduit, en hexadécimal (xxd -r -p le rend) :"
        od -An -tx1 -v "$lf/reduit.bin" | tr -d ' \n' | fold -w 128 | sed 's/^/      /'
        echo
    done
    "$sortie/lf_$cible" -runs=0 -print_coverage=1 "$lf/graines" > "$lf/cov-g.txt" 2>&1 || true
    "$sortie/lf_$cible" -runs=0 -print_coverage=1 "$lf/corpus" "$lf/graines" > "$lf/cov-f.txt" 2>&1 || true
    couverture "$lf/cov-g.txt" > "$lf/cov-g.lst"
    couverture "$lf/cov-f.txt" > "$lf/cov-f.lst"
    echo "-- la couverture (components/) : graines seules, puis graines et corpus final"
    grep -m1 INITED "$lf/cov-g.txt" | sed 's/^/   graines : /' || true
    grep -m1 INITED "$lf/cov-f.txt" | sed 's/^/   final   : /' || true
    echo "   fonctions dont libFuzzer a couvert plus d'arêtes (fonction, lieu, arêtes graines -> final) :"
    LC_ALL=C join -j 1 <(awk '$1 != "PC" { print $1 "@" $2, $3 }' "$lf/cov-g.lst" | LC_ALL=C sort) \
                       <(awk '$1 != "PC" { print $1 "@" $2, $3 }' "$lf/cov-f.lst" | LC_ALL=C sort) |
        awk '{ split($2, a, "/"); split($3, b, "/"); if (b[1] + 0 > a[1] + 0) { sub("@", " ", $1); print "      " $1, $2, "->", $3 } }' |
        head -n 40 || true
    echo "   lignes que les graines laissaient sans arête et que le corpus final couvre :"
    LC_ALL=C comm -23 <(grep '^PC ' "$lf/cov-g.lst" || true) <(grep '^PC ' "$lf/cov-f.lst" || true) | sed 's/^PC /      /' |
        head -n 60 || true
    echo "BILAN F4 $cible : $dt s (plafond $duree s), $(grep -m1 'stat::number_of_executed_units' "$lf/campagne.txt" | awk '{print $2}') exécutions, $(grep -m1 'stat::average_exec_per_sec' "$lf/campagne.txt" | awk '{print $2}') par s ; $n trouvaille(s), $rep reproduite(s) sous gcc"
}

[ $# -ge 2 ] || usage
cmd=$1
sortie=$2
case "$cmd" in
    construit) construit "$sortie" ;;
    campagne)
        [ $# = 5 ] || usage
        mkdir -p "$CAS/$3"
        "$sortie/fuzz_$3" campagne "$4" "$5" "$CAS/$3"
        ;;
    enum) enum "$sortie" "${3:-}" ;;
    rejoue) [ $# = 3 ] || usage; "$sortie/fuzz_$(cible_de "$3")" rejoue "$3" ;;
    reduit) [ $# = 4 ] || usage; "$sortie/fuzz_$(cible_de "$3")" reduit "$3" "$4" ;;
    rejoue-tout) rejoue_tout "$sortie" ;;
    mord) mord "$sortie" "${@:3}" ;;
    libfuzzer) [ $# = 4 ] || [ $# = 5 ] || usage; libfuzzer "$sortie" "${@:3}" ;;
    *) usage ;;
esac
