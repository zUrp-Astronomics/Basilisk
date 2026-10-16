/* SOURCE: include/bsk_sensor.h (bsk_sensor_read) — ticket #594
 * AUTHOR: engineer
 * DATE: 2026-10-11
 * STATUS: actif — compilé par les programmes de la suite qui compilent host.c, hors test_host (sim/test/programmes.sh) ;
 *         ticket #606 : plus de carte sans capteur, la mesure d'avant la première lecture
 *
 * Le capteur avant sa première lecture : la mesure reste inconnue (BSK_SENSOR_NONE), `z` rend `-`, `:GT#` `0000#`, et
 * le journal n'en dit rien. test_host pose les siennes pour jouer le capteur ; le capteur lui-même est joué
 * par test_sht40, contre le faux SHT40 (sim/sht40_sim.c). */
#include "bsk_sensor.h"

void bsk_sensor_read(bsk_sensor_t *v) { *v = (bsk_sensor_t){.st = BSK_SENSOR_NONE}; }
