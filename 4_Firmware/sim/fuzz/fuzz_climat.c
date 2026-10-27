/* Cible 7 de plans/fuzzing.md (l'atelier), seconde moitié : `z` et `:GT#` (components/host/host.c, climate,
 * condensation, moon_temperature), lus à travers bsk_host_line, sur une mesure que le harnais donne (bsk_sensor_read,
 * comme test_host). Propriétés P1 (les sanitizers), P2, P10 et P11. La première moitié, sht40.c contre le faux capteur,
 * est fuzz_sht40.c.
 *
 * Les couples atteignables : sht40.c ne rend que t_c = -4500 + round(17500 S_T / 65535), soit chaque entier de -4500 à
 * 13000, et rh_c borné à 0..10000, soit chaque entier de 0 à 10000 (bsk_sensor.h) : 17501 × 10001 couples, que la
 * commande propre `couples <t_de> <t_a>` énumère tous (par tranches de températures, pour les partager entre processus),
 * avec chaque issue inconnue (NONE, NACK, CRC, BUS, STALE) en plus.
 *
 * L'oracle est écrit ici, sans le code testé, d'après 7_Docs/PROTOCOL.md :
 *   - `z` (§ 2, ligne 84) : `-` seul quand la mesure est inconnue ; sinon `<°C> <%RH> <seuil>`, deux décimales, le signe à
 *     part ; le seuil `-` quand il n'est pas défini (humidité à 0, air au-dessus de 60 °C, point de givre sous -65 °C),
 *     sinon le point de rosée (positif ou nul) ou de givre (négatif) du guide CIMO de l'OMM (WMO-No. 8, partie I, chap. 4,
 *     annexe 4.B), calculé ici en long double ; un écart d'au plus un demi-centième, plus 1e-6, est admis (l'arrondi) ;
 *   - `:GT#` (§ 4, lignes 375-381) : la température en demi-degrés arrondie au plus proche, en hexadécimal signé sur
 *     16 bits, 4 chiffres et `#` ; `0000#` tant qu'elle est inconnue. Une demi-unité exacte (t_c ≡ 25 mod 50) admet ses
 *     deux voisins : PROTOCOL.md ne dit pas comment elle s'arrondit ;
 *   - les deux : une réponse par ligne, `\n` après `z`, rien après `:GT#` (§ 1, règle 1 ; § 4), dans BSK_HOST_REPLY_MAX.
 *
 * Un cas : une suite de mesures, 9 octets chacune : l'issue (modulo 6), t_c (4 octets, ramené dans -4500..13000), rh_c
 * (4 octets, ramené dans 0..10000). */
#define _POSIX_C_SOURCE 200809L

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "bsk_host.h"
#include "bsk_journal.h"
#include "bsk_sensor.h"
#include "bsk_session.h"
#include "fuzz.h"
#include "../test/phy_script.h"
#include "store_sim.h"

const char   FZ_CIBLE[] = "climat";
const size_t FZ_CAP = 9000;

enum { C_Z_FORME, C_Z_MESURE, C_Z_SEUIL, C_GT, C_FIN, C_N };

const fz_check_t FZ_CHECKS[] = {
    [C_Z_FORME] = {"P10", "z_forme", "PROTOCOL.md § 2, `z` : `-` seul quand la mesure est inconnue, jamais un nombre ; sinon "
                                     "trois champs séparés d'une espace"},
    [C_Z_MESURE] = {"P11", "z_mesure", "PROTOCOL.md § 2, `z` : la température et l'humidité de la mesure, deux décimales, le "
                                       "signe à part"},
    [C_Z_SEUIL] = {"P11", "z_seuil", "PROTOCOL.md § 2, `z` : le seuil `-` quand il n'est pas défini, sinon le point de rosée "
                                     "ou de givre de l'OMM (WMO-No. 8, annexe 4.B), arrondi au centième"},
    [C_GT] = {"P11", "gt", "PROTOCOL.md § 4, `:GT#` : demi-degrés arrondis au plus proche, hexadécimal signé 16 bits, "
                           "`0000#` inconnue"},
    [C_FIN] = {"P10", "fin", "PROTOCOL.md § 1, règle 1 et § 4 : `\\n` après `z`, aucune fin de ligne après `:GT#`, la "
                             "réponse dans BSK_HOST_REPLY_MAX"},
};
const size_t FZ_N_CHECKS = C_N;

/* ─────────────────────────── la mesure donnée ─────────────────────────── */

static bsk_sensor_t mesure;

void bsk_sensor_read(bsk_sensor_t *v) { *v = mesure; }

static bool pret;

static void demarrer(void)
{
    static bsk_session_params_t p;
    if (pret) return;
    ps_init(1000000u);
    bsk_session_params_default(&p);
    store_sim_reset();
    bsk_session_init(&p);
    bsk_host_init("1.3", "poweron", ps_now);
    pret = true;
}

/* ─────────────────────────── l'oracle ─────────────────────────── */

/* Le seuil de l'OMM, en °C : défini (vrai) ou non ; `bord` vrai quand la valeur est à 1e-9 près de la limite de -65 °C. */
static bool seuil(long t_c, long rh_c, long double *x, bool *bord)
{
    long double t = t_c / 100.0L, l;
    *bord = false;
    if (rh_c == 0 || t_c > 6000) return false;
    l = logl(rh_c / 10000.0L) + 17.62L * t / (243.12L + t);
    *x = l >= 0 ? 243.12L * l / (17.62L - l) : 272.62L * l / (22.46L - l);
    *bord = fabsl(*x + 65.0L) < 1e-9L;
    return *x >= -65.0L;
}

/* Un nombre `[-]<entier>.<deux chiffres>` lu en centièmes depuis `s` ; rend la fin, NULL si la forme n'y est pas. */
static const char *centiemes(const char *s, long *v)
{
    bool neg = *s == '-';
    long e = 0;
    const char *p = s + neg;
    if (*p < '0' || *p > '9') return NULL;
    while (*p >= '0' && *p <= '9') e = e * 10 + (*p++ - '0');
    if (p[0] != '.' || p[1] < '0' || p[1] > '9' || p[2] < '0' || p[2] > '9') return NULL;
    *v = e * 100 + (p[1] - '0') * 10 + (p[2] - '0');
    if (neg) *v = -*v;
    return p + 3;
}

/* `z` et `:GT#` pour la mesure courante ; rend faux au premier écart. */
static bool juger(fz_ctx_t *c)
{
    static char out[BSK_HOST_REPLY_MAX];
    const char *fin = bsk_host_line("z", out, sizeof out);
    bool ok = mesure.st == BSK_SENSOR_OK;
    long t, rh, s;
    const char *p;
    if (strcmp(fin, "\n") || strlen(out) >= BSK_HOST_REPLY_MAX)
        return !fz_fail(c, C_FIN, "z : fin de ligne « %s », réponse de %zu octets", fin, strlen(out));
    if (!ok) {
        if (strcmp(out, "-")) return !fz_fail(c, C_Z_FORME, "z : issue %d, réponse « %s »", (int)mesure.st, out);
    } else {
        p = centiemes(out, &t);
        if (!p || *p != ' ' || !(p = centiemes(p + 1, &rh)) || *p != ' ')
            return !fz_fail(c, C_Z_FORME, "z : t=%ld rh=%ld, réponse « %s »", (long)mesure.t_c, (long)mesure.rh_c, out);
        {
            char attendu[64];                           /* le signe seulement sous 0, deux décimales (PROTOCOL.md § 2) */
            long a = labs((long)mesure.t_c);
            int k = snprintf(attendu, sizeof attendu, "%s%ld.%02ld %ld.%02ld ", mesure.t_c < 0 ? "-" : "", a / 100, a % 100,
                             (long)mesure.rh_c / 100, (long)mesure.rh_c % 100);
            if (t != mesure.t_c || rh != mesure.rh_c || strncmp(out, attendu, (size_t)k))
                return !fz_fail(c, C_Z_MESURE, "z : t=%ld rh=%ld, réponse « %s », attendu « %s… »", (long)mesure.t_c,
                                (long)mesure.rh_c, out, attendu);
        }
        {
            long double x = 0;
            bool bord, def = seuil(mesure.t_c, mesure.rh_c, &x, &bord);
            p++;
            if (!strcmp(p, "-")) {
                if (def && !bord)
                    return !fz_fail(c, C_Z_SEUIL, "z : t=%ld rh=%ld, seuil `-`, attendu %.4Lf", (long)mesure.t_c,
                                    (long)mesure.rh_c, x);
            } else if (!(p = centiemes(p, &s)) || *p) {
                return !fz_fail(c, C_Z_FORME, "z : t=%ld rh=%ld, réponse « %s »", (long)mesure.t_c, (long)mesure.rh_c, out);
            } else if (!def && !bord) {
                return !fz_fail(c, C_Z_SEUIL, "z : t=%ld rh=%ld, seuil %ld centièmes, non défini", (long)mesure.t_c,
                                (long)mesure.rh_c, s);
            } else if (def && fabsl((long double)s - 100.0L * x) > 0.5L + 1e-6L) {
                return !fz_fail(c, C_Z_SEUIL, "z : t=%ld rh=%ld, seuil %ld centièmes, attendu %.4Lf", (long)mesure.t_c,
                                (long)mesure.rh_c, s, 100.0L * x);
            }
        }
    }
    fin = bsk_host_line(":GT", out, sizeof out);
    if (strcmp(fin, "") || strlen(out) != 5 || out[4] != '#')
        return !fz_fail(c, C_FIN, ":GT# : fin « %s », réponse « %s »", fin, out);
    {
        char *e;
        long v = strtol(out, &e, 16);
        if (e != out + 4) return !fz_fail(c, C_GT, ":GT# : réponse « %s »", out);
        if (v >= 0x8000) v -= 0x10000;
        if (!ok) {
            if (v != 0) return !fz_fail(c, C_GT, ":GT# : issue %d, réponse « %s »", (int)mesure.st, out);
        } else {
            long q = labs((long)mesure.t_c), lo = q / 50, hi = (q + 49) / 50;
            long a = q % 50 == 25 ? lo : (q % 50 < 25 ? lo : hi), b = q % 50 == 25 ? hi : a;
            if (mesure.t_c < 0) {
                a = -a;
                b = -b;
            }
            if (v != a && v != b) return !fz_fail(c, C_GT, ":GT# : t=%ld, réponse « %s » (%ld), attendu %ld", (long)mesure.t_c, out, v, a);
        }
    }
    return true;
}

/* ─────────────────────────── la campagne ─────────────────────────── */

static uint8_t at(const uint8_t *d, size_t n, size_t i) { return i < n ? d[i] : 0; }

void fz_run(fz_ctx_t *c, const uint8_t *d, size_t n)
{
    demarrer();
    for (size_t i = 0, k = 0; i < n && c->check < 0; i += 9, k++) {
        uint32_t t = 0, rh = 0;
        for (int j = 0; j < 4; j++) {
            t |= (uint32_t)at(d, n, i + 1 + (size_t)j) << 8 * j;
            rh |= (uint32_t)at(d, n, i + 5 + (size_t)j) << 8 * j;
        }
        mesure.st = (bsk_sensor_st_t)(at(d, n, i) % 6u);
        mesure.t_c = mesure.st == BSK_SENSOR_OK ? (int32_t)(t % 17501u) - 4500 : 0;
        mesure.rh_c = mesure.st == BSK_SENSOR_OK ? (int32_t)(rh % 10001u) : 0;
        fz_tr(c, "mesure %zu : issue %d t=%ld rh=%ld", k, (int)mesure.st, (long)mesure.t_c, (long)mesure.rh_c);
        if (!juger(c)) return;
    }
}

size_t fz_gen(fz_rng_t *r, uint8_t *out, size_t cap)
{
    static const int32_t BT[] = {-4500, -4499, -101, -100, -99, -51, -50, -49, -26, -25, -24, -1, 0, 1, 24, 25, 26, 49,
                                 50, 51, 99, 100, 5999, 6000, 6001, 12999, 13000};
    static const int32_t BH[] = {0, 1, 2, 99, 100, 101, 9999, 10000};
    uint32_t nm = 1 + fz_below(r, 64);
    size_t n = 0;
    for (uint32_t k = 0; k < nm && n + 9 <= cap; k++) {
        int32_t t = fz_chance(r, 1, 3) ? BT[fz_below(r, sizeof BT / sizeof BT[0])] : -4500 + (int32_t)fz_below(r, 17501);
        int32_t rh = fz_chance(r, 1, 3) ? BH[fz_below(r, sizeof BH / sizeof BH[0])] : (int32_t)fz_below(r, 10001);
        uint32_t ut = (uint32_t)(t + 4500), urh = (uint32_t)rh;
        out[n] = (uint8_t)(fz_chance(r, 5, 6) ? BSK_SENSOR_OK : fz_below(r, 6));
        for (int j = 0; j < 4; j++) {
            out[n + 1 + (size_t)j] = (uint8_t)(ut >> 8 * j);
            out[n + 5 + (size_t)j] = (uint8_t)(urh >> 8 * j);
        }
        n += 9;
    }
    return n;
}

/* ─────────────────────────── l'énumération ─────────────────────────── */

int fz_cmd(int argc, char **argv)
{
    fz_ctx_t c = {.check = -1};
    long de, a;
    unsigned long long nb = 0;
    if (argc != 3 || strcmp(argv[0], "couples")) return -1;
    de = strtol(argv[1], NULL, 10);
    a = strtol(argv[2], NULL, 10);
    if (de < -4500 || a > 13000 || de > a) {
        fprintf(stderr, "couples <t_de> <t_a>, dans -4500..13000\n");
        return 2;
    }
    demarrer();
    for (int st = 0; st <= BSK_SENSOR_STALE && c.check < 0; st++) {      /* chaque issue inconnue, une fois */
        if (st == BSK_SENSOR_OK) continue;
        mesure = (bsk_sensor_t){.st = (bsk_sensor_st_t)st};
        (void)juger(&c);
        nb++;
    }
    for (long t = de; t <= a && c.check < 0; t++)
        for (long rh = 0; rh <= 10000 && c.check < 0; rh++) {
            mesure = (bsk_sensor_t){.st = BSK_SENSOR_OK, .t_c = (int32_t)t, .rh_c = (int32_t)rh};
            (void)juger(&c);
            nb++;
        }
    if (c.check >= 0) printf("ECHEC %s %s : %s\n", FZ_CHECKS[c.check].prop, FZ_CHECKS[c.check].nom, c.why);
    printf("BILAN climat couples %ld..%ld : %llu mesures jugées (z et :GT#), %s\n", de, a, nb, c.check < 0 ? "aucun écart" : "un écart");
    return c.check < 0 ? 0 : 1;
}
