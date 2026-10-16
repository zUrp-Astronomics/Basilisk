/* SOURCE: Sensirion SHT4x datasheet v2 (0_Datasheets/Sensirion_SHT4x.pdf), § 4 ; plans/sht40.md § 4 de l'atelier
 * AUTHOR: engineer
 * DATE: 2026-10-11
 * STATUS: actif — en-tête privé du composant sensor : sht40.c (C pur), la carte (sensor_board.c) et la suite
 *         (sim/sht40_sim.c, sim/test/test_sht40.c)
 *
 * Le SHT40 sur un bus I2C mené à la main : le maître écrit ses deux lignes en drain ouvert et lit SDA. Le capteur ne
 * fait pas d'étirement d'horloge (DS § 4.1) : seul le maître tient SCL, et une transaction n'attend jamais une ligne.
 * Chaque opération a donc un nombre fixe de pas, et l'échéance d'une lecture ratée est celle de ses pas : deux mesures
 * et deux retours au repos, quelques dizaines de millisecondes au plus (sht_cycle). Aucun chien de garde n'en est la
 * garde.
 *
 * Ce que ce code n'émet jamais : la chauffe intégrée (0x39, 0x32, 0x2F, 0x24, 0x1E, 0x15, DS table 7 ; décision de
 * l'humain, [D120]). Il n'envoie que la mesure 0xFD et la remise à zéro 0x94, à l'adresse 0x44 (SHT40-AD1F, DS table 9). */
#ifndef SHT40_H
#define SHT40_H

#include <stdbool.h>
#include <stdint.h>

#include "bsk_sensor.h"

/* Les lignes, fournies par la carte (sensor_board.c : GPIO20 SCL, GPIO21 SDA, en drain ouvert, chaque écriture suivie
 * d'une demi-période) ou par le faux capteur (sim/sht40_sim.c). true : la ligne relâchée, le tirage la monte ; false :
 * tirée à 0. sht_wait_ms attend au moins `ms` millisecondes sans rien tenir. */
void sht_scl(bool high);
void sht_sda(bool high);
bool sht_sda_in(void);
void sht_wait_ms(uint32_t ms);

/* La somme de contrôle d'un mot de 16 bits (DS § 4.3 : CRC-8, polynôme 0x31, départ 0xFF ; CRC(0xBEEF) = 0x92). */
uint8_t sht_crc(uint8_t hi, uint8_t lo);

/* Le bus ramené au repos (DS § 4.7 ; plans/sht40.md § 4.2) : neuf coups d'horloge SDA relâchée (le « bus clear »
 * d'UM10204, § 3.1.16), pour qu'un capteur pris au milieu d'un octet, quel que soit son sens, le finisse et lâche SDA
 * (un capteur qui émet voit le non-acquittement du maître), puis une condition STOP, puis la remise à zéro 0x94 et sa
 * milliseconde (DS table 4). Le pire cas du faux capteur (sim/sht40_sim.h, sht40_sim_stuck : un octet de zéros, son
 * premier bit déjà lu) en demande sept, plus la descente de SCL qui commence STOP : les deux derniers sont la marge
 * d'UM10204, que ce modèle ne sait pas justifier ni réfuter. */
void sht_recover(void);

/* Une mesure (0xFD, haute répétabilité, 8,2 ms au plus, DS table 4), convertie (DS § 4.5), l'humidité bornée à
 * 0..10000. BSK_SENSOR_OK, ou ce qui a échoué : BSK_SENSOR_BUS (SDA basse quand le maître la relâche, avant une
 * condition START), BSK_SENSOR_NACK (l'adresse ou la commande pas acquittée), BSK_SENSOR_CRC (l'un des deux mots). */
bsk_sensor_st_t sht_measure(int32_t *t_c, int32_t *rh_c);

/* Ce que la tâche de lecture tient : la dernière lecture et l'instant de la dernière réussie. À zéro : aucune lecture
 * (BSK_SENSOR_NONE). */
typedef struct {
    bsk_sensor_st_t st;      /* l'issue de la dernière lecture : OK, NACK, CRC, BUS ; NONE avant la première */
    int32_t         t_c;     /* la dernière lecture réussie */
    int32_t         rh_c;
    uint64_t        at_us;   /* son instant */
} sht_state_t;

/* Une lecture, à l'instant `now_us` : une mesure ; ratée, le bus ramené au repos, puis une seconde mesure ; ratée
 * aussi, le bus ramené au repos encore, et l'issue de la seconde est celle de la lecture. Une lecture ratée rend la
 * valeur inconnue tout de suite. */
void sht_cycle(sht_state_t *s, uint64_t now_us);

/* La lecture `s` jugée à l'instant `now_us` (bsk_sensor.h) : périmée au-delà de BSK_SENSOR_STALE_US. */
void sht_judge(const sht_state_t *s, uint64_t now_us, bsk_sensor_t *v);

#endif
