/* SOURCE: Sensirion SHT4x datasheet v2 (0_Datasheets/Sensirion_SHT4x.pdf), § 3.2 et § 4 ; NXP UM10204 — le faux SHT40
 * AUTHOR: engineer
 * DATE: 2026-10-11
 * STATUS: actif — compilé par 4_Firmware/run.sh dans test_sht40 (sim/test/programmes.sh)
 *
 * Un SHT40 à l'adresse 0x44, vu bit par bit sur ses deux lignes : il implémente les lignes de components/sensor/sht40.h
 * (sht_scl, sht_sda, sht_sda_in, sht_wait_ms) sur une horloge virtuelle, et répond au code de la carte comme le
 * capteur. Écrit d'après la DS, rien d'autre :
 *   - une condition START (SDA descend, SCL haute), puis l'adresse et le sens ; une condition STOP (SDA monte, SCL
 *     haute) ramène au repos, à tout moment ;
 *   - SDA est lue au front montant de SCL, le capteur change la sienne après le front descendant ; il acquitte au
 *     neuvième coup ce qui lui est adressé ;
 *   - 0xFD lance une mesure de 8,2 ms (le maximum, DS table 4) ; un en-tête de lecture pendant elle n'est pas acquitté
 *     (DS § 4.1) ; ses six octets (T, CRC, RH, CRC) ne se lisent qu'une fois (DS § 4.1) ; un non-acquittement du
 *     maître finit la lecture (DS § 4.1), un acquittement en appelle un autre ;
 *   - 0x94 arrête tout, efface la mesure, et le capteur ne répond plus pendant 1 ms (DS table 4, § 4.7) ;
 *   - toute autre commande est notée (sht40_sim_cmds) et n'a pas d'effet : la chauffe, en particulier, n'est pas
 *     simulée, et un test vérifie qu'elle n'est jamais demandée ;
 *   - pas d'étirement d'horloge (DS § 4.1) ;
 *   - ce que la DS ne dit pas et que ce modèle fixe : la remise à zéro accepte son adresse pendant une mesure (la DS dit
 *     seulement qu'elle l'arrête, § 4.7) ; un en-tête de lecture sans mesure faite n'est pas acquitté ; un octet après
 *     la commande n'est pas acquitté ; au-delà de ses six octets, s'il est encore acquitté, il émet des zéros ; les lignes n'ont pas de temps de montée ; le capteur absent n'a aucun effet sur
 *     elles. */
#ifndef SHT40_SIM_H
#define SHT40_SIM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Le capteur présent, au repos, sans mesure, les lignes relâchées, l'horloge à 0, aucune commande notée, aucune faute. */
void     sht40_sim_reset(void);

/* L'horloge virtuelle, en µs : sht_wait_ms seul la fait avancer. */
uint64_t sht40_sim_now_us(void);

/* Le capteur présent ou non (false : il ne répond à rien, ne tient aucune ligne). */
void     sht40_sim_present(bool on);

/* Les signaux bruts, S_T et S_RH (DS § 4.5), de chaque mesure à venir. */
void     sht40_sim_signals(uint16_t st, uint16_t srh);

/* Les `n` prochaines mesures rendues avec le CRC d'un de leurs mots faux (inversé) : `word` 0 la température, 1
 * l'humidité. */
void     sht40_sim_bad_crc(unsigned n, unsigned word);

/* Le capteur pris au milieu d'une lecture que le maître n'a pas finie (une réinitialisation de la puce, plans/sht40.md
 * § 4.2) : il émet un octet de zéros, en est à son premier bit, déjà lu (SCL remontée : la ligne relâchée au redémarrage), et
 * tient SDA basse. Le pire cas : il la tient encore sept bits, et ne la lâche qu'à l'acquittement. */
void     sht40_sim_stuck(void);

/* SDA tenue à 0 hors du capteur (un court-circuit), tant que `on`. */
void     sht40_sim_short(bool on);

/* Les commandes reçues (l'octet qui suit un en-tête d'écriture acquitté), dans l'ordre ; n au plus dans `out` ; rend
 * leur nombre total. */
size_t   sht40_sim_cmds(uint8_t *out, size_t n);

/* Le nombre de conditions START vues sur le bus. */
unsigned sht40_sim_starts(void);

/* SDA et SCL telles que le bus les porte (true : haute). */
bool     sht40_sim_sda(void);
bool     sht40_sim_scl(void);

#endif
