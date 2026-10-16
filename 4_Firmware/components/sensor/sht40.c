/* SOURCE: Sensirion SHT4x datasheet v2 (0_Datasheets/Sensirion_SHT4x.pdf), § 4 ; NXP UM10204 (le bus I2C, auquel la DS
 *         renvoie, § 4.1) ; plans/sht40.md § 4 de l'atelier — le SHT40 (sht40.h)
 * AUTHOR: engineer
 * DATE: 2026-10-11
 * STATUS: actif — C pur, sans ESP-IDF : compilé pour la carte et par la suite (sim/test/test_sht40.c, contre le faux
 *         capteur sim/sht40_sim.c)
 *
 * Le bus mené à la main (sht40.h dit pourquoi) : une écriture de ligne par pas, le niveau de SDA lu SCL haute. SDA ne
 * change que SCL basse, sauf pour START et STOP. */
#include "sht40.h"

#define ADDR        0x44u   /* SHT40-AD1F (DS table 9) */
#define CMD_MEASURE 0xFDu   /* T et RH, haute répétabilité (DS table 7) */
#define CMD_RESET   0x94u   /* remise à zéro (DS table 7, § 4.7) */
#define MEASURE_MS  9u      /* la mesure : 8,2 ms au plus (DS table 4, à -40 °C et 1,08 V) ; sht_wait_ms attend au moins */
#define RESET_MS    1u      /* la remise à zéro : 1 ms au plus (DS table 4) */

/* Une condition START, SDA puis SCL relâchées d'abord : faux, sans rien émettre, si SDA reste basse (le bus bloqué). */
static bool start(void)
{
    sht_sda(true);
    sht_scl(true);
    if (!sht_sda_in()) return false;
    sht_sda(false);
    sht_scl(false);
    return true;
}

/* Une condition STOP : SDA monte, SCL haute. */
static void stop(void)
{
    sht_sda(false);
    sht_scl(true);
    sht_sda(true);
}

/* Un octet émis, bit de poids fort d'abord ; vrai s'il est acquitté (SDA tirée à 0 par le capteur au neuvième coup). */
static bool put_byte(uint8_t b)
{
    bool ack;
    for (int i = 7; i >= 0; i--) {
        sht_sda(((b >> i) & 1u) != 0);
        sht_scl(true);
        sht_scl(false);
    }
    sht_sda(true);
    sht_scl(true);
    ack = !sht_sda_in();
    sht_scl(false);
    return ack;
}

/* Un octet reçu ; `ack` : l'acquitter (un autre suit), sinon le non-acquittement qui finit la lecture (DS § 4.1). */
static uint8_t get_byte(bool ack)
{
    uint8_t b = 0;
    sht_sda(true);
    for (int i = 0; i < 8; i++) {
        sht_scl(true);
        b = (uint8_t)(b << 1 | (sht_sda_in() ? 1u : 0u));
        sht_scl(false);
    }
    sht_sda(!ack);
    sht_scl(true);
    sht_scl(false);
    sht_sda(true);
    return b;
}

uint8_t sht_crc(uint8_t hi, uint8_t lo)
{
    uint8_t c = 0xFF;
    const uint8_t d[2] = {hi, lo};
    for (int k = 0; k < 2; k++) {
        c = (uint8_t)(c ^ d[k]);
        for (int i = 0; i < 8; i++) {
            bool top = (c & 0x80u) != 0;
            c = (uint8_t)(c << 1);
            if (top) c = (uint8_t)(c ^ 0x31u);
        }
    }
    return c;
}

void sht_recover(void)
{
    sht_sda(true);
    for (int i = 0; i < 9; i++) {
        sht_scl(false);
        sht_scl(true);
    }
    sht_scl(false);
    stop();
    if (start()) {
        (void)(put_byte(ADDR << 1) && put_byte(CMD_RESET));
        stop();
    }
    sht_wait_ms(RESET_MS);
}

/* La conversion (DS § 4.5) en centièmes, arrondie au plus proche : T = -45 + 175 S/65535 °C, RH = -6 + 125 S/65535 %RH ;
 * les produits tiennent sur 32 bits (17500 × 65535 < 2^31). L'humidité est bornée à 0..100 %RH, comme la DS le
 * conseille. */
static int32_t centi(uint32_t span, uint16_t s) { return (int32_t)((span * s + 32767u) / 65535u); }

bsk_sensor_st_t sht_measure(int32_t *t_c, int32_t *rh_c)
{
    uint8_t d[6];
    bool ack;
    int32_t rh;
    if (!start()) return BSK_SENSOR_BUS;
    ack = put_byte(ADDR << 1) && put_byte(CMD_MEASURE);
    stop();
    if (!ack) return BSK_SENSOR_NACK;
    sht_wait_ms(MEASURE_MS);
    if (!start()) return BSK_SENSOR_BUS;
    if (!put_byte(ADDR << 1 | 1u)) {                /* le capteur occupé ou absent (DS § 4.1) */
        stop();
        return BSK_SENSOR_NACK;
    }
    for (int i = 0; i < 6; i++) d[i] = get_byte(i < 5);
    stop();
    if (sht_crc(d[0], d[1]) != d[2] || sht_crc(d[3], d[4]) != d[5]) return BSK_SENSOR_CRC;
    rh = centi(12500u, (uint16_t)(d[3] << 8 | d[4])) - 600;
    *t_c = centi(17500u, (uint16_t)(d[0] << 8 | d[1])) - 4500;
    *rh_c = rh < 0 ? 0 : rh > 10000 ? 10000 : rh;
    return BSK_SENSOR_OK;
}

void sht_cycle(sht_state_t *s, uint64_t now_us)
{
    int32_t t = 0, rh = 0;
    bsk_sensor_st_t r = sht_measure(&t, &rh);
    if (r != BSK_SENSOR_OK) {
        sht_recover();
        r = sht_measure(&t, &rh);
        if (r != BSK_SENSOR_OK) sht_recover();
    }
    s->st = r;
    if (r != BSK_SENSOR_OK) return;
    s->t_c = t;
    s->rh_c = rh;
    s->at_us = now_us;
}

void sht_judge(const sht_state_t *s, uint64_t now_us, bsk_sensor_t *v)
{
    v->st = s->st;
    v->t_c = v->rh_c = 0;
    if (s->st != BSK_SENSOR_OK) return;
    if (now_us > s->at_us && now_us - s->at_us > BSK_SENSOR_STALE_US) {
        v->st = BSK_SENSOR_STALE;
        return;
    }
    v->t_c = s->t_c;
    v->rh_c = s->rh_c;
}
