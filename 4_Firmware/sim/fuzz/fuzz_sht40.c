/* Cible 7 de plans/fuzzing.md (l'atelier), première moitié : le vrai components/sensor/sht40.c contre le faux SHT40
 * bit par bit (sim/sht40_sim.c). Propriétés P1 (les sanitizers), P2 et P11. L'autre moitié, `z` et `:GT#` sur tous les
 * couples atteignables, est fuzz_climat.c.
 *
 * Un cas : une suite de lectures, 8 octets chacune : S_T (2 octets), S_RH (2), les fautes, le nombre de mesures au CRC
 * faux, puis le délai en millisecondes (2 octets) entre la lecture et le jugement qui la suit (sht_judge). Les fautes
 * (octet 4) : bit 0 le capteur absent, bit 1 le mot au CRC faux est l'humidité (sinon la température), bit 2 le capteur
 * pris au milieu d'une lecture (sht40_sim_stuck) avant la lecture, bit 3 SDA court-circuitée pendant la lecture. Ce sont
 * les fautes que le faux capteur sait jouer (sim/sht40_sim.h) ; rien d'autre ne touche les lignes.
 *
 * La commande propre `mots` énumère les 65536 S_T (S_RH fixé), puis les 65536 S_RH (S_T fixé), par sht_measure contre le
 * faux capteur : chaque conversion comparée à la formule de la DS calculée ici. */
#define _POSIX_C_SOURCE 200809L

#include <math.h>
#include <string.h>

#include "fuzz.h"
#include "sht40.h"
#include "sht40_sim.h"

const char   FZ_CIBLE[] = "sht40";
const size_t FZ_CAP = 4096;

enum { C_CMD, C_ISSUE, C_VALEUR, C_TEMPS, C_STARTS, C_JUGE, C_N };

const fz_check_t FZ_CHECKS[] = {
    [C_CMD] = {"P11", "commandes", "sht40.h : le code n'envoie que la mesure 0xFD et la remise à zéro 0x94, jamais la "
                                   "chauffe ([D120]) ; le faux capteur note les commandes reçues (sim/sht40_sim.h)"},
    [C_ISSUE] = {"P11", "issue", "sht40.h, sht_cycle (deux mesures, un retour au repos entre elles, qui libère un capteur "
                                 "pris : sht_recover), et bsk_sensor.h : SDA court-circuitée, BUS ; absent, NACK ; sinon une "
                                 "mesure ratée par le capteur pris (la première) ou par chaque CRC faux, et OK s'il en reste "
                                 "une, CRC sinon"},
    [C_VALEUR] = {"P11", "valeur", "sht40.h et DS § 4.5 : T = -45 + 175 S_T / 65535 °C, RH = -6 + 125 S_RH / 65535 %RH bornée "
                                   "à 0..100, en centièmes arrondis au plus proche (formule calculée ici, en double)"},
    [C_TEMPS] = {"P2", "duree", "sht40.h : « chaque opération a un nombre fixe de pas », une lecture ratée « quelques "
                                "dizaines de millisecondes au plus » : au plus 30 ms du temps virtuel du faux capteur"},
    [C_STARTS] = {"P2", "starts", "sht40.h : deux mesures (une écriture, une lecture) et deux retours au repos (une remise "
                                  "à zéro) : au plus 6 conditions START par lecture"},
    [C_JUGE] = {"P11", "jugement", "bsk_sensor.h : une valeur inconnue n'est jamais rendue comme un nombre (t_c, rh_c à 0 "
                                   "hors OK) ; périmée (STALE) au-delà de 3 s après la dernière lecture réussie"},
};
const size_t FZ_N_CHECKS = C_N;

/* La formule de la DS (§ 4.5), en centièmes : aucun cas d'égalité à une demi-unité (17500 S / 65535 et 12500 S / 65535
 * n'ont jamais une partie fractionnaire de 1/2 : 35000 S et 25000 S sont pairs, 65535 impair). */
static long t_attendu(uint16_t s) { return lround(100.0 * (-45.0 + 175.0 * s / 65535.0)); }
static long rh_attendu(uint16_t s)
{
    long v = lround(100.0 * (-6.0 + 125.0 * s / 65535.0));
    return v < 0 ? 0 : v > 10000 ? 10000 : v;
}

static uint8_t at(const uint8_t *d, size_t n, size_t i) { return i < n ? d[i] : 0; }

static bool commandes_ok(size_t *nc, uint8_t *mauvaise)
{
    uint8_t cmd[512];
    size_t n;
    memset(cmd, 0xFD, sizeof cmd);                       /* le faux capteur ne garde que les premières (64) : le reste reste 0xFD */
    n = sht40_sim_cmds(cmd, sizeof cmd);
    *nc = n;
    for (size_t i = 0; i < n && i < sizeof cmd; i++)
        if (cmd[i] != 0xFD && cmd[i] != 0x94) {
            *mauvaise = cmd[i];
            return false;
        }
    return true;
}

void fz_run(fz_ctx_t *c, const uint8_t *d, size_t n)
{
    sht_state_t s;
    uint64_t t = 1000000u;
    memset(&s, 0, sizeof s);
    sht40_sim_reset();
    for (size_t i = 0, k = 0; i < n && c->check < 0; i += 8, k++) {
        uint16_t st = (uint16_t)(at(d, n, i) | at(d, n, i + 1) << 8), srh = (uint16_t)(at(d, n, i + 2) | at(d, n, i + 3) << 8);
        uint8_t f = at(d, n, i + 4), nb = at(d, n, i + 5) & 3u;
        uint32_t dt = at(d, n, i + 6) | (uint32_t)at(d, n, i + 7) << 8;
        bool absent = f & 1u, court = f & 8u;
        uint64_t v0 = sht40_sim_now_us();
        unsigned s0 = sht40_sim_starts();
        size_t nc;
        uint8_t mauvaise = 0;
        bsk_sensor_st_t attendu;
        bsk_sensor_t v;
        sht40_sim_present(!absent);
        sht40_sim_signals(st, srh);
        sht40_sim_bad_crc(nb, f & 2u ? 1u : 0u);
        if ((f & 4u) && !absent) sht40_sim_stuck();      /* un capteur absent ne tient aucune ligne (sht40_sim.h) */
        sht40_sim_short(court);
        sht_cycle(&s, t);
        sht40_sim_short(false);
        sht40_sim_bad_crc(0, 0);
        attendu = court ? BSK_SENSOR_BUS : absent ? BSK_SENSOR_NACK : nb + ((f & 4u) ? 1u : 0u) >= 2 ? BSK_SENSOR_CRC : BSK_SENSOR_OK;
        fz_tr(c, "lecture %zu : S_T=%u S_RH=%u absent=%d crc_faux=%u(%s) pris=%d court=%d -> st=%d t=%ld rh=%ld, %llu µs, %u "
                 "START",
              k, st, srh, absent, nb, f & 2u ? "rh" : "t", (f & 4u) && !absent, court, (int)s.st, (long)s.t_c, (long)s.rh_c,
              (unsigned long long)(sht40_sim_now_us() - v0), sht40_sim_starts() - s0);
        if (!commandes_ok(&nc, &mauvaise)) {
            fz_fail(c, C_CMD, "lecture %zu : la commande %02X reçue par le capteur", k, mauvaise);
            return;
        }
        if (s.st != attendu) {
            fz_fail(c, C_ISSUE, "lecture %zu : issue %d, attendue %d", k, (int)s.st, (int)attendu);
            return;
        }
        if (s.st == BSK_SENSOR_OK && (s.t_c != t_attendu(st) || s.rh_c != rh_attendu(srh))) {
            fz_fail(c, C_VALEUR, "lecture %zu : S_T=%u S_RH=%u -> %ld, %ld ; attendu %ld, %ld", k, st, srh, (long)s.t_c,
                    (long)s.rh_c, t_attendu(st), rh_attendu(srh));
            return;
        }
        if (sht40_sim_now_us() - v0 > 30000u) {
            fz_fail(c, C_TEMPS, "lecture %zu : %llu µs de temps virtuel", k, (unsigned long long)(sht40_sim_now_us() - v0));
            return;
        }
        if (sht40_sim_starts() - s0 > 6u) {
            fz_fail(c, C_STARTS, "lecture %zu : %u conditions START", k, sht40_sim_starts() - s0);
            return;
        }
        /* le jugement, dt ms après la lecture : t avance de la lecture (1 s, BSK_SENSOR_PERIOD_MS) puis de dt */
        {
            uint64_t ok_at = s.at_us, quand = t + (uint64_t)dt * 1000u;
            bool perime = s.st == BSK_SENSOR_OK && quand > ok_at && quand - ok_at > 3000000u;
            sht_judge(&s, quand, &v);
            if (v.st != (perime ? BSK_SENSOR_STALE : s.st) || (v.st != BSK_SENSOR_OK && (v.t_c || v.rh_c)) ||
                (v.st == BSK_SENSOR_OK && (v.t_c != s.t_c || v.rh_c != s.rh_c))) {
                fz_fail(c, C_JUGE, "lecture %zu : jugée %u ms après, st=%d t=%ld rh=%ld (lecture st=%d)", k, (unsigned)dt,
                        (int)v.st, (long)v.t_c, (long)v.rh_c, (int)s.st);
                return;
            }
            t = quand + 1000000u;
        }
    }
}

int fz_cmd(int argc, char **argv)
{
    unsigned long faux = 0;
    if (argc != 1 || strcmp(argv[0], "mots")) return -1;
    size_t ncmd = 0;
    for (int mot = 0; mot < 2; mot++)
        for (uint32_t s = 0; s <= 0xFFFF; s++) {
            int32_t tc = 0, rhc = 0;
            uint16_t st = mot ? 0x6666 : (uint16_t)s, srh = mot ? (uint16_t)s : 0x7777;
            bsk_sensor_st_t r;
            if (s % 32 == 0) {                          /* les commandes relues par 32, le faux capteur en garde 64 */
                size_t nc = 0;
                uint8_t mauvaise = 0;
                if ((mot || s) && !commandes_ok(&nc, &mauvaise)) {
                    printf("ECHEC P11 commandes : %02X reçue\n", mauvaise);
                    faux++;
                }
                ncmd += nc;
                sht40_sim_reset();
            }
            sht40_sim_signals(st, srh);
            r = sht_measure(&tc, &rhc);
            if (r != BSK_SENSOR_OK || tc != t_attendu(st) || rhc != rh_attendu(srh)) {
                if (faux++ < 10)
                    printf("ECHEC P11 valeur : S_T=%u S_RH=%u -> st=%d t=%ld rh=%ld ; attendu %ld, %ld\n", st, srh, (int)r,
                           (long)tc, (long)rhc, t_attendu(st), rh_attendu(srh));
            }
        }
    {
        size_t nc;
        uint8_t mauvaise = 0;
        if (!commandes_ok(&nc, &mauvaise)) {
            printf("ECHEC P11 commandes : %02X reçue\n", mauvaise);
            faux++;
        }
        ncmd += nc;
        printf("BILAN sht40 mots : 131072 mesures (65536 S_T, 65536 S_RH), %lu écart(s), %zu commandes reçues\n", faux, ncmd);
    }
    return faux ? 1 : 0;
}

/* ─────────────────────────── le générateur ─────────────────────────── */

static uint16_t signal(fz_rng_t *r, bool hum)
{
    /* les bords : 0, 65535 ; T = 0 °C (S = 16852), 60 °C (39321), -45 et 130 ; RH = 0 % (3146) et 100 % (55050) */
    static const uint16_t BT[] = {0, 1, 16851, 16852, 16853, 39320, 39321, 39322, 65534, 65535};
    static const uint16_t BH[] = {0, 1, 3145, 3146, 3147, 55049, 55050, 55051, 65534, 65535};
    if (fz_chance(r, 1, 2)) return (hum ? BH : BT)[fz_below(r, 10)];
    return (uint16_t)fz_next(r);
}

size_t fz_gen(fz_rng_t *r, uint8_t *out, size_t cap)
{
    uint32_t nl = 1 + fz_below(r, 12);
    size_t n = 0;
    for (uint32_t k = 0; k < nl && n + 8 <= cap; k++) {
        uint16_t st = signal(r, false), srh = signal(r, true), dt;
        uint8_t f = 0;
        if (fz_chance(r, 1, 8)) f |= 1;
        if (fz_chance(r, 1, 2)) f |= 2;
        if (fz_chance(r, 1, 6)) f |= 4;
        if (fz_chance(r, 1, 10)) f |= 8;
        dt = fz_chance(r, 1, 3) ? (uint16_t)(1999 + fz_below(r, 3)) : (uint16_t)fz_below(r, 6000);
        out[n] = (uint8_t)st;
        out[n + 1] = (uint8_t)(st >> 8);
        out[n + 2] = (uint8_t)srh;
        out[n + 3] = (uint8_t)(srh >> 8);
        out[n + 4] = f;
        out[n + 5] = (uint8_t)(fz_chance(r, 1, 4) ? fz_below(r, 4) : 0);
        out[n + 6] = (uint8_t)dt;
        out[n + 7] = (uint8_t)(dt >> 8);
        n += 8;
    }
    return n;
}
