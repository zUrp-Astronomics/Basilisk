#!/usr/bin/env bash
# SOURCE: outillage des tests hôte en CI
# AUTHOR: engineer
# DATE: 2026-09-23
# STATUS: actif — joué par le job `test` de .gitea/workflows/ci.yml ET par la sonde
#         .gitea/workflows/probe-test-relevance.yml (une seule définition : les deux ne dérivent pas)
#
# Script de CI : il tourne en root dans l'image catthehacker/ubuntu:act-latest (apt, écriture sous
# /opt). Pas destiné à un poste de dev — là, `npm install` à la racine suffit, avec CHROMIUM_PATH ou
# un Chromium sous /opt/pw-browsers (voir ## Test du CLAUDE.md).
#
# ⚠ TOUT S'INSTALLE HORS DE $HOME, ET C'EST LE POINT. La sonde rejoue la suite sous `env -i` avec un
# HOME VIERGE : un Chromium laissé dans le cache par défaut de Playwright (~/.cache/ms-playwright)
# y serait introuvable. D'où :
#   - le module `playwright` dans node_modules/ à la racine du dépôt (résolu par require() depuis
#     le test de la page dans Chromium, sans NODE_PATH, où qu'il soit sous la racine) ;
#   - Chromium sous /opt/pw-browsers, le chemin que ce test lit quand CHROMIUM_PATH est
#     absent (et env -i l'enlève).
set -euo pipefail
RACINE=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
export DEBIAN_FRONTEND=noninteractive

# apt-get update, et son repli quand le miroir de l'image rend une erreur. Le 2026-10-24, le miroir
# (archive.ubuntu.com, en http, que le runner fait passer par son cache, 198.18.0.1:3142) a servi plus
# de 6 h un index corrompu de noble-updates/main (Hash Sum mismatch) : chaque job sur noble mourait à
# apt-get update, avant son premier test (run 2823). Le repli met les sources Ubuntu sur l'instantané
# de la veille de l'archive (snapshot.ubuntu.com) : un état publié, figé, signé de la même clé, servi
# en https, hors de ce cache. Il est écrit dans les sources : les apt-get update qui suivent (celui de
# Playwright, celui d'après le PPA d'INDI) le lisent aussi.
# Rien n'est masqué : l'update de l'instantané doit réussir à son tour, et l'installation qui suit échoue
# si un paquet requis manque. Un hôte injoignable, apt n'en fait qu'un avertissement (pas de repli) :
# le job rougit alors à l'installation.
# La vérification TLS est coupée pour ce seul hôte : ubuntu:24.04 et 26.04 n'ont pas ca-certificates
# avant leur premier apt. La confiance d'apt n'en dépend pas, c'est la signature de l'InRelease et le
# hash de chaque fichier, les mêmes que sur les sources http de l'image.
# ⚠ MÊME FONCTION, AU CARACTÈRE PRÈS, DANS LES JOBS `indi` ET `firmware` DE ci.yml (en ligne : apt y
# vient avant git, donc avant le dépôt) : on change l'une, on change les autres. Le job `test` le vérifie
# (.gitea/garde_apt_update.sh). Le repli et ses cas négatifs se rejouent sur le runner, dans les trois images,
# face à un faux miroir au Release incohérent : .gitea/workflows/trial-apt-repli.yml.
apt_update() {
    apt-get update -qq && return
    local veille
    veille=$(date -u -d '1 day ago' +%Y%m%dT%H%M%SZ)
    echo "apt-get update : miroir de l'image en échec, sources Ubuntu sur l'instantané $veille" >&2
    printf '%s\n' 'Types: deb' "URIs: https://snapshot.ubuntu.com/ubuntu/$veille/" \
        "Suites: $(. /etc/os-release && echo "$VERSION_CODENAME $VERSION_CODENAME-updates $VERSION_CODENAME-security")" \
        'Components: main restricted universe multiverse' \
        'Signed-By: /usr/share/keyrings/ubuntu-archive-keyring.gpg' > /etc/apt/sources.list.d/ubuntu.sources
    echo 'Acquire::https::snapshot.ubuntu.com::Verify-Peer "false";' > /etc/apt/apt.conf.d/99instantane
    apt-get update -qq
}

# Avant Playwright, qui refait son propre apt-get update (--with-deps) : sur un miroir en échec, c'est
# l'instantané qu'il lira.
apt_update
if ! command -v gcc >/dev/null; then
    apt-get install -y -qq gcc libc6-dev >/dev/null
fi
for outil in gcc python3 node npm; do
    command -v "$outil" >/dev/null || { echo "outil absent de l'image : $outil" >&2; exit 1; }
done
echo "node $(node --version), npm $(npm --version), $(gcc --version | head -n 1), $(python3 --version)"

cd "$RACINE"
npm install --no-audit --no-fund --no-package-lock
PLAYWRIGHT_BROWSERS_PATH=/opt/pw-browsers npx --no-install playwright install --with-deps chromium

# Le test de la page prend le premier /opt/pw-browsers/chromium-*/chrome-linux/chrome. La disposition
# `chrome-linux` est celle de Playwright 1.49.1 (épinglé dans package.json) : si une montée de
# version la change, c'est ici que ça doit casser, pas dans le test.
ls /opt/pw-browsers/chromium-*/chrome-linux/chrome
