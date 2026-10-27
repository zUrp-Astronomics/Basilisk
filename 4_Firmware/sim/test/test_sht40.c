/* SOURCE: Sensirion SHT4x datasheet v2 (0_Datasheets/Sensirion_SHT4x.pdf), § 4 ; plans/sht40.md § 4 de l'atelier ;
 *         spec de l'atelier [D120] — le SHT40 de la carte v1.0
 * AUTHOR: engineer
 * DATE: 2026-10-11
 * STATUS: actif — joué par 4_Firmware/run.sh (job `firmware`), sans argument
 *
 * Le vrai code du capteur (components/sensor/sht40.c) contre le faux SHT40 bit par bit (sim/sht40_sim.c) : une lecture,
 * le capteur absent, un CRC faux, le bus bloqué puis libéré, une valeur périmée, l'humidité bornée, la chauffe jamais
 * demandée. Les attendus sont écrits à la main : les conversions d'après les formules de la DS (§ 4.5), calculées à
 * part, en flottants ; le CRC d'après l'exemple de la DS (§ 4.3). */
#define _POSIX_C_SOURCE 200809L   /* alarm, la garde de durée */

#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "sht40.h"
#include "sht40_sim.h"

static int checks, fails;

/* La garde de durée, en secondes : 10 ; la passe sous sanitizers de 4_Firmware/run.sh et les mutants en -O0
 * (sim/test/mutants.sh) la relèvent (-DGARDE_S). */
#ifndef GARDE_S
#define GARDE_S 10
#endif
#define TEXTE_(x) #x
#define TEXTE(x) TEXTE_(x)

static void too_long(int sig)
{
    static const char m[] = "  ECHEC : le programme ne termine pas en " TEXTE(GARDE_S) " s (boucle sans fin)\n";
    (void)sig;
    (void)!write(1, m, sizeof m - 1);
    _exit(1);
}

#define CHECK(c, ...)                                                   \
    do {                                                                \
        checks++;                                                       \
        if (!(c)) {                                                     \
            fails++;                                                    \
            printf("  ECHEC %s:%d : ", __FILE__, __LINE__);             \
            printf(__VA_ARGS__);                                        \
            printf("\n");                                               \
        }                                                               \
    } while (0)

static sht_state_t S;

/* Un banc neuf : le faux capteur présent, au repos ; aucune lecture. */
static void fresh(void)
{
    sht40_sim_reset();
    memset(&S, 0, sizeof S);
}

/* Une lecture, à l'instant du faux capteur, puis jugée au même instant. */
static bsk_sensor_t cycle(void)
{
    bsk_sensor_t v;
    sht_cycle(&S, sht40_sim_now_us());
    sht_judge(&S, sht40_sim_now_us(), &v);
    return v;
}

/* Les commandes reçues par le capteur depuis fresh, comparées à `want` (n octets). */
static bool cmds_are(const uint8_t *want, size_t n)
{
    uint8_t got[16];
    size_t k = sht40_sim_cmds(got, sizeof got);
    return k == n && !memcmp(got, want, n);
}

/* Ni la chauffe ni rien d'autre : seules 0xFD et 0x94 sont jamais reçues. */
static bool only_measure_and_reset(void)
{
    uint8_t got[64];
    size_t k = sht40_sim_cmds(got, sizeof got);
    for (size_t i = 0; i < k && i < sizeof got; i++)
        if (got[i] != 0xFD && got[i] != 0x94) return false;
    return true;
}

static void t_crc(void)
{
    printf("CRC : l'exemple de la DS, CRC(0xBEEF) = 0x92\n");
    CHECK(sht_crc(0xBE, 0xEF) == 0x92, "CRC(0xBEEF) = 0x%02X, attendu 0x92", sht_crc(0xBE, 0xEF));
    CHECK(sht_crc(0xEF, 0xBE) != 0x92, "l'ordre des octets compte");
}

static void t_normal(void)
{
    bsk_sensor_t v;
    printf("une lecture : la mesure 0xFD, ses six octets, convertis (DS § 4.5)\n");
    fresh();
    sht40_sim_signals(0x6666, 0x6666);              /* T = -45 + 175 × 26214/65535 = 25,0000 ; RH = 44,0000 */
    v = cycle();
    CHECK(v.st == BSK_SENSOR_OK && v.t_c == 2500 && v.rh_c == 4400, "st %d, %ld, %ld : attendu OK, 2500, 4400", (int)v.st,
          (long)v.t_c, (long)v.rh_c);
    CHECK(cmds_are((const uint8_t[]){0xFD}, 1), "une seule commande, 0xFD");
    CHECK(sht40_sim_starts() == 2, "deux transactions, la commande puis la lecture (%u)", sht40_sim_starts());
    CHECK(sht40_sim_now_us() == 9000, "la lecture attend 9 ms entre la commande et l'en-tête de lecture, 8,2 au plus "
          "(DS table 4) (%llu µs)", (unsigned long long)sht40_sim_now_us());
    CHECK(sht40_sim_sda() && sht40_sim_scl(), "le bus au repos après la lecture");
    sht40_sim_signals(0x2000, 0x5A5A);              /* T = -23,1247 ; RH = 38,1176 */
    v = cycle();
    CHECK(v.st == BSK_SENSOR_OK && v.t_c == -2312 && v.rh_c == 3812, "st %d, %ld, %ld : attendu OK, -2312, 3812", (int)v.st,
          (long)v.t_c, (long)v.rh_c);
    sht40_sim_signals(0x8000, 0x0000);              /* T = 42,5013 */
    v = cycle();
    CHECK(v.st == BSK_SENSOR_OK && v.t_c == 4250, "T 0x8000 : %ld, attendu 4250", (long)v.t_c);
    sht40_sim_signals(0xFFFF, 0x0000);              /* T = 130,0000 */
    v = cycle();
    CHECK(v.st == BSK_SENSOR_OK && v.t_c == 13000, "T 0xFFFF : %ld, attendu 13000", (long)v.t_c);
    sht40_sim_signals(0x0000, 0x0000);              /* T = -45,0000 */
    v = cycle();
    CHECK(v.st == BSK_SENSOR_OK && v.t_c == -4500, "T 0x0000 : %ld, attendu -4500", (long)v.t_c);
    CHECK(only_measure_and_reset(), "la chauffe jamais demandée");
}

static void t_humidity_bounds(void)
{
    static const struct { uint16_t srh; int32_t want; const char *why; } c[] = {
        {0x0000, 0, "-6,0000 %RH, borné à 0"},
        {3140, 0, "-0,0108 %RH, borné à 0"},
        {3146, 0, "0,0006 %RH, arrondi à 0"},
        {55574, 10000, "100,0006 %RH, arrondi à 100,00"},
        {55580, 10000, "100,0121 %RH, borné à 100,00"},
        {0xFFFF, 10000, "119,0000 %RH, borné à 100,00"},
        {0x8000, 5650, "56,5010 %RH"},
    };
    printf("l'humidité bornée à 0-100 %%RH (DS § 4.5)\n");
    fresh();
    for (size_t i = 0; i < sizeof c / sizeof c[0]; i++) {
        bsk_sensor_t v;
        sht40_sim_signals(0x6666, c[i].srh);
        v = cycle();
        CHECK(v.st == BSK_SENSOR_OK && v.rh_c == c[i].want, "S_RH %u (%s) : %ld, attendu %ld", (unsigned)c[i].srh, c[i].why,
              (long)v.rh_c, (long)c[i].want);
    }
}

static void t_absent(void)
{
    bsk_sensor_t v;
    printf("le capteur absent : NACK, la valeur inconnue, une lecture ratée bornée en temps\n");
    fresh();
    v = cycle();
    sht40_sim_present(false);
    v = cycle();
    CHECK(v.st == BSK_SENSOR_NACK && v.t_c == 0 && v.rh_c == 0, "st %d, %ld, %ld : attendu NACK, 0, 0 (la valeur d'avant oubliée)",
          (int)v.st, (long)v.t_c, (long)v.rh_c);
    /* mesure (l'adresse sans acquittement), retour au repos (1 ms), mesure, retour au repos (1 ms) */
    CHECK(sht40_sim_now_us() == 9000 + 2000, "la lecture ratée a pris %llu µs, attendu 2000 après la première lecture",
          (unsigned long long)sht40_sim_now_us() - 9000);
    CHECK(sht40_sim_starts() == 2 + 4, "quatre transactions : deux mesures, deux remises à zéro (%u)", sht40_sim_starts() - 2);
    CHECK(sht40_sim_sda() && sht40_sim_scl(), "le bus au repos");
    sht40_sim_present(true);
    sht40_sim_signals(0x6666, 0x6666);
    v = cycle();
    CHECK(v.st == BSK_SENSOR_OK && v.t_c == 2500, "le capteur revenu, relu sans rien d'autre : st %d", (int)v.st);
    CHECK(only_measure_and_reset(), "la chauffe jamais demandée");
}

static void t_crc_fault(void)
{
    bsk_sensor_t v;
    printf("un CRC faux : relu après le retour au repos ; deux de suite, la valeur inconnue\n");
    fresh();
    sht40_sim_signals(0x6666, 0x6666);
    sht40_sim_bad_crc(1, 0);
    v = cycle();
    CHECK(v.st == BSK_SENSOR_OK && v.t_c == 2500, "le CRC de T faux, puis la seconde mesure : st %d, attendu OK", (int)v.st);
    CHECK(cmds_are((const uint8_t[]){0xFD, 0x94, 0xFD}, 3), "0xFD, la remise à zéro 0x94, 0xFD");
    fresh();
    sht40_sim_signals(0x6666, 0x6666);
    sht40_sim_bad_crc(1, 1);
    v = cycle();
    CHECK(v.st == BSK_SENSOR_OK && v.rh_c == 4400, "le CRC de RH faux, puis la seconde mesure : st %d, attendu OK", (int)v.st);
    CHECK(cmds_are((const uint8_t[]){0xFD, 0x94, 0xFD}, 3), "0xFD, la remise à zéro 0x94, 0xFD");
    fresh();
    sht40_sim_signals(0x6666, 0x6666);
    v = cycle();
    sht40_sim_bad_crc(2, 1);
    v = cycle();
    CHECK(v.st == BSK_SENSOR_CRC && v.t_c == 0 && v.rh_c == 0, "deux CRC faux : st %d, %ld, %ld, attendu CRC, 0, 0", (int)v.st,
          (long)v.t_c, (long)v.rh_c);
    CHECK(cmds_are((const uint8_t[]){0xFD, 0xFD, 0x94, 0xFD, 0x94}, 5), "0xFD, 0x94, 0xFD, 0x94 après la première lecture");
    v = cycle();
    CHECK(v.st == BSK_SENSOR_OK && v.t_c == 2500, "la lecture suivante, juste : st %d", (int)v.st);
}

static void t_stuck(void)
{
    bsk_sensor_t v;
    printf("le bus bloqué par le capteur, pris au milieu d'un octet : libéré, relu dans la même lecture\n");
    fresh();
    sht40_sim_signals(0x6666, 0x6666);
    sht40_sim_stuck();
    CHECK(!sht40_sim_sda(), "le banc : SDA tenue basse par le capteur");
    v = cycle();
    CHECK(v.st == BSK_SENSOR_OK && v.t_c == 2500 && v.rh_c == 4400, "st %d, %ld, %ld : attendu OK, 2500, 4400", (int)v.st,
          (long)v.t_c, (long)v.rh_c);
    CHECK(cmds_are((const uint8_t[]){0x94, 0xFD}, 2), "rien d'émis sur le bus bloqué ; la remise à zéro, puis 0xFD");
    CHECK(sht40_sim_sda() && sht40_sim_scl(), "le bus au repos");

    printf("le retour au repos seul (celui du démarrage) : le bus libéré, le capteur remis à zéro\n");
    fresh();
    sht40_sim_stuck();
    sht_recover();
    CHECK(sht40_sim_sda() && sht40_sim_scl(), "le bus au repos");
    CHECK(cmds_are((const uint8_t[]){0x94}, 1), "la remise à zéro 0x94, et rien d'autre");
    CHECK(sht40_sim_now_us() == 1000, "puis 1 ms d'attente, la remise à zéro (DS table 4) (%llu µs)",
          (unsigned long long)sht40_sim_now_us());

    printf("le bus bloqué hors du capteur (SDA à la masse) : BUS, rien d'émis, puis relu quand il est libéré\n");
    fresh();
    sht40_sim_signals(0x6666, 0x6666);
    v = cycle();
    sht40_sim_short(true);
    v = cycle();
    CHECK(v.st == BSK_SENSOR_BUS && v.t_c == 0 && v.rh_c == 0, "st %d, %ld, %ld : attendu BUS, 0, 0", (int)v.st, (long)v.t_c,
          (long)v.rh_c);
    CHECK(cmds_are((const uint8_t[]){0xFD}, 1), "aucune commande sur le bus bloqué");
    sht40_sim_short(false);
    v = cycle();
    CHECK(v.st == BSK_SENSOR_OK && v.t_c == 2500, "le bus libéré, la lecture suivante : st %d", (int)v.st);
    CHECK(only_measure_and_reset(), "la chauffe jamais demandée");
}

static void t_judge(void)
{
    bsk_sensor_t v;
    printf("le jugement : inconnue avant la première lecture, périmée au-delà de 3 s\n");
    fresh();
    v = (bsk_sensor_t){BSK_SENSOR_OK, 1, 1};        /* ce que le jugement doit remplacer */
    sht_judge(&S, 0, &v);
    CHECK(v.st == BSK_SENSOR_NONE && v.t_c == 0 && v.rh_c == 0, "avant toute lecture : st %d, attendu NONE", (int)v.st);
    sht40_sim_signals(0x6666, 0x6666);
    sht_cycle(&S, 5000000);
    sht_judge(&S, 5000000 + 3000000, &v);
    CHECK(v.st == BSK_SENSOR_OK && v.t_c == 2500 && v.rh_c == 4400, "3 s après la lecture : st %d, attendu OK", (int)v.st);
    v = (bsk_sensor_t){BSK_SENSOR_OK, 1, 1};
    sht_judge(&S, 5000000 + 3000001, &v);
    CHECK(v.st == BSK_SENSOR_STALE && v.t_c == 0 && v.rh_c == 0, "3 s et 1 µs après : st %d, %ld, %ld, attendu STALE, 0, 0",
          (int)v.st, (long)v.t_c, (long)v.rh_c);
    sht_judge(&S, 4000000, &v);
    CHECK(v.st == BSK_SENSOR_OK, "un instant d'avant la lecture (jugé sur une autre tâche) : st %d, attendu OK", (int)v.st);
    sht_cycle(&S, 9000000);
    sht_judge(&S, 9000000, &v);
    CHECK(v.st == BSK_SENSOR_OK && S.at_us == 9000000, "une lecture nouvelle date la valeur (%llu)", (unsigned long long)S.at_us);
    sht40_sim_present(false);
    sht_cycle(&S, 10000000);
    sht_judge(&S, 10000000, &v);
    CHECK(v.st == BSK_SENSOR_NACK && S.at_us == 9000000, "une lecture ratée ne date rien : st %d, at %llu", (int)v.st,
          (unsigned long long)S.at_us);
}

int main(void)
{
    signal(SIGALRM, too_long);
    alarm(GARDE_S);
    t_crc();
    t_normal();
    t_humidity_bounds();
    t_absent();
    t_crc_fault();
    t_stuck();
    t_judge();
    printf("le SHT40, contre le faux capteur : %d vérifications, %d échec(s)\n", checks, fails);
    return fails ? 1 : 0;
}
