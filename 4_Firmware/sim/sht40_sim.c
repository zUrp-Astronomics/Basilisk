/* SOURCE: Sensirion SHT4x datasheet v2 (0_Datasheets/Sensirion_SHT4x.pdf), § 3.2 et § 4 ; NXP UM10204 — le faux SHT40
 *         (voir sht40_sim.h)
 * AUTHOR: engineer
 * DATE: 2026-10-11
 * STATUS: actif — compilé par 4_Firmware/run.sh dans test_sht40 (sim/test/programmes.sh)
 *
 * Les lignes de components/sensor/sht40.h au-dessus d'un capteur vu bit par bit. Le bus : SDA est basse dès qu'un des
 * trois la tire (le maître, le capteur, un court-circuit) ; SCL est celle du maître (pas d'étirement). Le capteur ne
 * change SDA que SCL basse ; il ne réagit qu'aux changements du maître. */
#include "sht40_sim.h"

#include <string.h>

#include "sht40.h"

#define ADDR      0x44u
#define MEAS_US   8200u     /* DS table 4, le maximum en haute répétabilité */
#define RESET_US  1000u     /* DS table 4 */

typedef enum { M_IDLE, M_RX, M_TX } smode_t;   /* M_IDLE : attend START (ou STOP) */

static struct {
    uint64_t now;
    bool     m_scl, m_sda;     /* ce que tient le maître : true relâchée */
    bool     s_sda;            /* ce que tient le capteur */
    bool     shorted;
    bool     present;
    smode_t  mode;
    int      bit;              /* M_RX : les bits reçus de l'octet (0 à 8), 9 dans l'acquittement */
    uint8_t  shift;
    unsigned rx;               /* M_RX : les octets reçus depuis START */
    bool     write;            /* l'en-tête reçu est une écriture */
    bool     to_tx;            /* l'en-tête de lecture acquitté : l'émission commence à la fin de l'acquittement */
    bool     ack;              /* M_RX : ce que le capteur rend au neuvième coup */
    int      tx_bit;           /* M_TX : le bit émis (7 à 0), -1 l'acquittement du maître */
    bool     m_ack;            /* M_TX : le maître a acquitté */
    uint8_t  tx[7];            /* les six octets d'une mesure, puis des zéros (sht40_sim.h) */
    unsigned tx_i;
    bool     meas;             /* une mesure lancée, pas encore lue */
    uint64_t meas_end, reset_end;
    uint16_t st, srh;
    unsigned bad, bad_word;
    uint8_t  cmds[64];
    size_t   n_cmds;
    unsigned starts;
} g;

/* Le CRC de la DS (§ 4.3), écrit autrement que sht_crc : le reste de la division du mot, sa départ 0xFF ajouté à son
 * premier octet, par x^8 + x^5 + x^4 + 1 (0x131). */
static uint8_t crc(uint16_t w)
{
    uint32_t r = (uint32_t)(w ^ 0xFF00u) << 8;
    for (int i = 23; i >= 8; i--)
        if (r & (1u << i)) r ^= 0x131u << (i - 8);
    return (uint8_t)r;
}

static bool bus_sda(void) { return g.m_sda && g.s_sda && !g.shorted; }

static void drive_bit(void) { g.s_sda = ((g.tx[g.tx_i] >> g.tx_bit) & 1u) != 0; }

/* L'octet reçu, au front descendant qui suit son huitième bit : ce que le capteur en fait, et s'il l'acquitte. */
static bool take(uint8_t b)
{
    if (g.rx++ == 0) {                                  /* l'en-tête */
        if ((b >> 1) != ADDR || g.now < g.reset_end) return false;
        g.write = (b & 1u) == 0;
        if (g.write) return true;
        if (!g.meas || g.now < g.meas_end) return false; /* occupé, ou rien à lire */
        g.tx[0] = (uint8_t)(g.st >> 8);
        g.tx[1] = (uint8_t)g.st;
        g.tx[2] = crc(g.st);
        g.tx[3] = (uint8_t)(g.srh >> 8);
        g.tx[4] = (uint8_t)g.srh;
        g.tx[5] = crc(g.srh);
        g.tx[6] = 0;
        if (g.bad) {
            g.tx[g.bad_word ? 5 : 2] = (uint8_t)~g.tx[g.bad_word ? 5 : 2];
            g.bad--;
        }
        g.meas = false;                                 /* lue une fois (DS § 4.1) */
        g.to_tx = true;
        return true;
    }
    if (!g.write || g.rx != 2) return false;            /* une commande, un octet */
    if (g.n_cmds < sizeof g.cmds) g.cmds[g.n_cmds] = b;
    g.n_cmds++;
    if (b == 0xFD) {
        g.meas = true;
        g.meas_end = g.now + MEAS_US;
    } else if (b == 0x94) {
        g.meas = false;
        g.reset_end = g.now + RESET_US;
    }
    return true;
}

static void on_start(void)
{
    g.starts++;
    if (!g.present) return;
    g.mode = M_RX;
    g.bit = 0;
    g.shift = 0;
    g.rx = 0;
    g.to_tx = false;
    g.s_sda = true;
}

static void on_stop(void)
{
    g.mode = M_IDLE;
    g.s_sda = true;
}

static void on_rise(bool sda)
{
    if (g.mode == M_RX && g.bit < 8) {
        g.shift = (uint8_t)(g.shift << 1 | (sda ? 1u : 0u));
        g.bit++;
    } else if (g.mode == M_TX && g.tx_bit < 0) {
        g.m_ack = !sda;
    }
}

static void on_fall(void)
{
    if (g.mode == M_RX && g.bit == 8) {                 /* l'octet entier : l'acquittement */
        g.ack = take(g.shift);
        g.s_sda = !g.ack;
        g.bit = 9;
    } else if (g.mode == M_RX && g.bit == 9) {          /* la fin de l'acquittement */
        g.s_sda = true;
        if (!g.ack) {
            g.mode = M_IDLE;
        } else if (g.to_tx) {
            g.mode = M_TX;
            g.tx_i = 0;
            g.tx_bit = 7;
            drive_bit();
        } else {
            g.bit = 0;
            g.shift = 0;
        }
    } else if (g.mode == M_TX && g.tx_bit > 0) {
        g.tx_bit--;
        drive_bit();
    } else if (g.mode == M_TX && g.tx_bit == 0) {       /* l'acquittement du maître */
        g.tx_bit = -1;
        g.s_sda = true;
    } else if (g.mode == M_TX) {                        /* après lui : l'octet suivant, ou la fin */
        if (g.m_ack) {
            if (g.tx_i + 1 < sizeof g.tx) g.tx_i++;
            g.tx_bit = 7;
            drive_bit();
        } else {
            g.mode = M_IDLE;
            g.s_sda = true;
        }
    }
}

/* Le maître change une ligne : START et STOP d'abord (SDA change, SCL haute), puis les fronts de SCL. */
static void master(bool scl, bool sda)
{
    bool o_scl = g.m_scl, o_sda = bus_sda(), n_sda;
    g.m_scl = scl;
    g.m_sda = sda;
    n_sda = bus_sda();
    if (o_scl && scl && o_sda != n_sda) {
        if (n_sda) on_stop();
        else on_start();
        return;
    }
    if (!g.present) return;
    if (!o_scl && scl) on_rise(n_sda);
    else if (o_scl && !scl) on_fall();
}

/* ─────────────────────────── les lignes de sht40.h ─────────────────────────── */

void sht_scl(bool high) { master(high, g.m_sda); }
void sht_sda(bool high) { master(g.m_scl, high); }
bool sht_sda_in(void) { return bus_sda(); }
void sht_wait_ms(uint32_t ms) { g.now += (uint64_t)ms * 1000u; }

/* ─────────────────────────── le banc ─────────────────────────── */

void sht40_sim_reset(void)
{
    memset(&g, 0, sizeof g);
    g.m_scl = g.m_sda = g.s_sda = true;
    g.present = true;
}

uint64_t sht40_sim_now_us(void) { return g.now; }

void sht40_sim_present(bool on)
{
    g.present = on;
    g.mode = M_IDLE;
    g.s_sda = true;
}

void sht40_sim_signals(uint16_t st, uint16_t srh)
{
    g.st = st;
    g.srh = srh;
}

void sht40_sim_bad_crc(unsigned n, unsigned word)
{
    g.bad = n;
    g.bad_word = word;
}

void sht40_sim_stuck(void)
{
    g.mode = M_TX;
    g.tx[0] = 0x00;
    g.tx_i = 0;
    g.tx_bit = 7;
    drive_bit();
}

void sht40_sim_short(bool on) { g.shorted = on; }

size_t sht40_sim_cmds(uint8_t *out, size_t n)
{
    size_t k = g.n_cmds < sizeof g.cmds ? g.n_cmds : sizeof g.cmds;
    memcpy(out, g.cmds, k < n ? k : n);
    return g.n_cmds;
}

unsigned sht40_sim_starts(void) { return g.starts; }

bool sht40_sim_sda(void) { return bus_sda(); }
bool sht40_sim_scl(void) { return g.m_scl; }
