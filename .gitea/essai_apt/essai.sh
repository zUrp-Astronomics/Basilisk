#!/usr/bin/env bash
# Essai du repli d'apt_update (.gitea/setup_host.sh, jobs `indi` et `firmware` de ci.yml) face au miroir
# de l'archive Ubuntu en panne. Joué par .gitea/workflows/trial-apt-repli.yml, une fois par image et par
# scénario, en root, dans l'image du job visé.
#
# usage : essai.sh <job> <image> <scénario>
#   <job>      test | indi | firmware : le job de ci.yml dont l'étape « Outillage… » est jouée, lue dans
#              ci.yml au moment de l'essai (pas une copie) ; <image> doit être son `container:`.
#   <scénario> sain                       miroir intact : vert, sans repli ;
#              incoherent                 les hôtes de l'archive que déclarent les sources de l'image
#                                         (archive.ubuntu.com, security.ubuntu.com…) envoyés par /etc/hosts
#                                         sur miroir.py, qui relaie l'archive réelle mais altère l'index de
#                                         <codename>-updates/main : vert, par le repli, et après lui plus
#                                         aucune source active ne vise l'archive, et un apt-get update de
#                                         plus passe (celui de Playwright, celui d'après le PPA d'INDI) ;
#              injoignable                ces hôtes envoyés sur 127.0.0.1, où rien n'écoute : rouge (un
#                                         paquet requis ne s'installe pas) ;
#              incoherent-sans-instantane incoherent, et snapshot.ubuntu.com injoignable : rouge, après le
#                                         repli.
#
# Le workflow a posé, AVANT d'installer de quoi cloner (git, python3, python3-yaml), l'état initial de
# l'image sous /tmp/etat-initial (/var/lib/apt/lists, /etc/hosts, ca-certificates installé ou absent) :
# l'essai le remet en place avant de jouer l'étape. ca-certificates est retiré s'il manquait à l'image
# (ubuntu:24.04 et 26.04) : sans magasin de certificats, c'est la condition pour laquelle apt_update coupe
# Verify-Peer sur l'instantané. Ce que l'installation préalable laisse, ce sont git, python3 et
# python3-yaml, déjà là quand l'étape les demande ; les autres (gcc, cmake, libindi-dev, les dépendances de
# Chromium…) passent par l'apt de l'étape.
set -euo pipefail
ICI=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
RACINE=$(cd "$ICI/../.." && pwd)
INITIAL=/tmp/etat-initial
job=$1 image=$2 scenario=$3
TMP=$(mktemp -d)
menage() { local p; p=$(jobs -p); [ -z "$p" ] || kill $p; rm -rf "$TMP"; }
trap menage EXIT
echc() { echo "ÉCHEC DE L'ESSAI ($job, $scenario) : $*" >&2; exit 1; }

# L'étape réelle, lue dans ci.yml.
python3 - "$RACINE/.gitea/workflows/ci.yml" "$job" "$image" "$TMP" <<'PY'
import shlex, sys, yaml
ci, job, image, tmp = sys.argv[1:]
j = yaml.safe_load(open(ci))['jobs'][job]
if j['container'] != image:
    sys.exit(f'ci.yml : le job {job} tourne dans {j["container"]}, pas dans {image}')
etapes = [e for e in j['steps'] if e.get('name', '').startswith('Outillage')]
if len(etapes) != 1 or '${{' in etapes[0]['run']:
    sys.exit(f'ci.yml : le job {job} doit avoir une seule étape « Outillage… », sans expression')
open(f'{tmp}/etape.sh', 'w').write(etapes[0]['run'])
open(f'{tmp}/etape.env', 'w').write(''.join(f'export {k}={shlex.quote(str(v))}\n' for k, v in etapes[0].get('env', {}).items()))
PY

# Les index apt de l'image, tels qu'avant l'installation préalable (qui ne touche pas /etc/apt).
rm -rf /var/lib/apt/lists
cp -a "$INITIAL/lists" /var/lib/apt/lists
cat "$INITIAL/hosts" > /etc/hosts
ca_image=$(cat "$INITIAL/ca-certificates")
if [ "$ca_image" = absent ]; then
    dpkg --purge ca-certificates > /dev/null
    rm -rf /etc/ssl/certs
fi
dpkg-query -W -f='${Status}' ca-certificates 2>/dev/null | grep -q 'ok installed' && ca=installé || ca=absent
[ "$ca" = "$ca_image" ] || echc "ca-certificates $ca, alors qu'il est $ca_image dans l'image"
echo "ca-certificates : $ca, comme dans l'image$([ "$ca" = installé ] || echo ', et pas de /etc/ssl/certs')"

mapfile -t hotes < <(grep -rhoE '[a-z0-9.-]*(archive|security)\.ubuntu\.com' /etc/apt/sources.list /etc/apt/sources.list.d/ 2>/dev/null | sort -u)
[ "${#hotes[@]}" -gt 0 ] || echc "aucune source de l'image ne vise l'archive : rien à mettre en panne"
echo "sources de l'image qui visent l'archive :"
grep -rlE '(archive|security)\.ubuntu\.com' /etc/apt/sources.list /etc/apt/sources.list.d/ 2>/dev/null | sed 's/^/  /'
echo "hôtes détournés : ${hotes[*]}"

# Le proxy http qu'apt prend pour l'archive : le runner en injecte un (/etc/apt/apt.conf.d/01lcars-proxy,
# Proxy-Auto-Detect vers son cache). Par lui, c'est le cache qui résout l'hôte et /etc/hosts ne détourne
# rien : un hôte détourné passe donc en DIRECT (la seule configuration d'apt que l'essai ajoute), et le faux
# miroir relaie, lui, par ce proxy, le chemin qu'apt aurait pris.
P='' D=''
eval "$(apt-config shell P Acquire::http::Proxy D Acquire::http::Proxy-Auto-Detect)"
if [ -z "$P" ] && [ -n "$D" ] && [ -x "$D" ]; then P=$("$D" "http://${hotes[0]}/ubuntu/" || true); fi
case $P in http://*) proxy=${P#http://}; proxy=${proxy%%/*} ;; *) proxy=DIRECT ;; esac
echo "proxy http d'apt pour l'archive : $proxy"

detourne() {
    for h in "$@"; do
        echo "127.0.0.1 $h" >> /etc/hosts
        echo "Acquire::http::Proxy::$h \"DIRECT\";" >> /etc/apt/apt.conf.d/00essai-detournement
    done
}
case $scenario in
    sain) ;;
    incoherent|incoherent-sans-instantane)
        amont=()
        for h in "${hotes[@]}"; do amont+=("$h=$(getent ahostsv4 "$h" | awk 'NR == 1 { print $1 }')"); done
        suite="$(. /etc/os-release && echo "$VERSION_CODENAME")-updates"
        python3 "$ICI/miroir.py" "$suite" "$proxy" "${amont[@]}" > /dev/null 2> "$TMP/miroir.log" &
        for _ in $(seq 50); do (: < /dev/tcp/127.0.0.1/80) 2>/dev/null && break; sleep 0.2; done
        detourne "${hotes[@]}"
        [ "$scenario" = incoherent ] || detourne snapshot.ubuntu.com ;;
    injoignable) detourne "${hotes[@]}" ;;
    *) echc "scénario inconnu" ;;
esac

echo "─── étape « Outillage » du job $job, telle que dans ci.yml ───"
set +e
(cd "$RACINE" && . "$TMP/etape.env" && bash --noprofile --norc -eo pipefail "$TMP/etape.sh") 2>&1 | tee "$TMP/etape.log"
rc=${PIPESTATUS[0]}
set -e
echo "─── fin de l'étape : code $rc ───"

grep -q "miroir de l'image en échec" "$TMP/etape.log" && repli=oui || repli=non
echo "repli : $repli"
if [ -f "$TMP/miroir.log" ]; then cat "$TMP/miroir.log"; fi
case $scenario in
    sain)
        [ "$rc" = 0 ] || echc "rouge sur un miroir sain"
        [ "$repli" = non ] || echc "repli sur un miroir sain" ;;
    incoherent)
        grep -q 'servi altéré' "$TMP/miroir.log" || echc "le faux miroir n'a servi aucun index altéré : la panne n'a pas eu lieu"
        grep -q 'Hash Sum mismatch' "$TMP/etape.log" || echc "apt n'a pas vu de Hash Sum mismatch"
        [ "$repli" = oui ] || echc "pas de repli"
        [ "$rc" = 0 ] || echc "rouge malgré le repli"
        echo "dépôts actifs après le repli :"
        apt-get indextargets --format '$(URI)' | sed 's|/dists/.*||' | sort -u | sed 's/^/  /'
        if apt-get indextargets --format '$(URI)' | grep -E '(archive|security)\.ubuntu\.com'; then
            echc "une source active vise encore l'archive après le repli"
        fi
        apt-get update -qq || echc "l'apt-get update suivant le repli échoue" ;;
    injoignable)
        [ "$rc" != 0 ] || echc "vert alors que l'archive est injoignable"
        [ "$repli" = non ] || echc "repli sur une archive injoignable (apt n'en fait qu'un avertissement)"
        grep -qE "^E: (Unable to locate package|Package '.*' has no installation candidate)" "$TMP/etape.log" \
            || echc "le rouge ne vient pas de l'installation d'un paquet requis" ;;
    incoherent-sans-instantane)
        grep -q 'servi altéré' "$TMP/miroir.log" || echc "le faux miroir n'a servi aucun index altéré : la panne n'a pas eu lieu"
        [ "$repli" = oui ] || echc "pas de repli"
        [ "$rc" != 0 ] || echc "vert alors que l'instantané est injoignable" ;;
esac
echo "ESSAI CONFORME ($job, $image, $scenario)"
