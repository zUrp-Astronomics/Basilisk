/* SOURCE: spec de l'atelier [D120] — le capteur de la carte v1.0 (SHT40) ; plans/sht40.md § 4 de l'atelier
 * AUTHOR: engineer
 * DATE: 2026-10-11
 * STATUS: actif — déclarations seulement ; sur la carte components/sensor/sensor_board.c, en test sim/sensor_none.c (le
 *         capteur avant sa première lecture) ou le test lui-même (sim/test/test_host.c) ; ticket #606 : la carte v1.0
 *         seule, toujours un capteur ; ticket #641 : le seuil de condensation, calculé hors de la tâche du capteur
 *
 * La température et l'humidité relative de l'air autour de la carte, lues sur son SHT40. La carte les rapporte
 * (PROTOCOL.md, `z` et `:GT#`), avec le seuil de condensation qu'elle en calcule (le point de rosée ou de givre, `z` ;
 * components/host/host.c, sur la tâche de SESSION) ; elle n'en fait rien d'autre, aucune compensation (décisions de
 * l'humain, [D120] et ticket #641).
 *
 * Invariants :
 *   - une valeur inconnue n'est jamais rendue comme un nombre : `st` le dit, et t_c, rh_c ne valent que sous
 *     BSK_SENSOR_OK ;
 *   - une valeur est inconnue avant la première lecture réussie (BSK_SENSOR_NONE),
 *     quand la dernière lecture a échoué (le capteur muet, un CRC faux, le bus bloqué), et quand la dernière lecture
 *     réussie est plus vieille que BSK_SENSOR_STALE_US (BSK_SENSOR_STALE : la lecture ne se fait plus) ;
 *   - la lecture ne prend rien au temps réel de la session : elle tourne dans une tâche de priorité 0, sous toutes les
 *     autres (components/sensor/sensor_board.c dit pourquoi cela suffit, et ce qui reste partagé).
 *
 * Les tâches : bsk_sensor_read est appelée par la tâche de SESSION (la couche HOTE) ; la lecture du capteur a sa tâche,
 * qui ne touche à rien d'autre que le bus du capteur. */
#ifndef BSK_SENSOR_H
#define BSK_SENSOR_H

#include <stdint.h>

/* Une lecture se fait toutes les BSK_SENSOR_PERIOD_MS ; la dernière lecture réussie est périmée au-delà de
 * BSK_SENSOR_STALE_US : trois périodes, deux lectures manquées et la marge d'une lecture retardée (sa tâche est sous
 * toutes les autres). Le seuil de refocalisation d'Ekos est de 1 °C ; le capteur répond en 2 s (t63, DS SHT4x p. 5). */
#define BSK_SENSOR_PERIOD_MS 1000u
#define BSK_SENSOR_STALE_US  3000000u

typedef enum {
    BSK_SENSOR_NONE,    /* aucune lecture encore */
    BSK_SENSOR_OK,      /* la dernière lecture a réussi, il y a au plus BSK_SENSOR_STALE_US */
    BSK_SENSOR_NACK,    /* la dernière lecture : le capteur n'a pas répondu (absent, ou occupé) */
    BSK_SENSOR_CRC,     /* la dernière lecture : une somme de contrôle fausse */
    BSK_SENSOR_BUS,     /* la dernière lecture : le bus bloqué, SDA tenue à 0, que le retour au repos n'a pas libéré */
    BSK_SENSOR_STALE,   /* la dernière lecture réussie est plus vieille que BSK_SENSOR_STALE_US */
} bsk_sensor_st_t;

typedef struct {
    bsk_sensor_st_t st;
    int32_t         t_c;    /* BSK_SENSOR_OK : la température, en centièmes de °C ; 0 sinon */
    int32_t         rh_c;   /* BSK_SENSOR_OK : l'humidité relative, en centièmes de %RH, bornée à 0..10000 ; 0 sinon */
} bsk_sensor_t;

/* Sur la carte : les lignes du bus au repos, puis la tâche de lecture. À appeler une fois, par app_main. */
void bsk_sensor_init(void);

/* La dernière mesure, jugée maintenant (les invariants ci-dessus). N'attend rien. */
void bsk_sensor_read(bsk_sensor_t *v);

/* La marge de pile de la tâche de lecture, en octets, pour `DEBUG` (bsk_host_stacks). La carte
 * seulement. */
uint32_t bsk_sensor_stack(void);

#endif
