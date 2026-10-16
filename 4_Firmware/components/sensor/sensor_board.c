/* SOURCE: spec de l'atelier [D120] ; plans/sht40.md § 3.2 et § 4 de l'atelier — le SHT40 sur la carte (bsk_sensor.h)
 * AUTHOR: engineer
 * DATE: 2026-10-11
 * STATUS: actif — ESP-IDF seulement : aucun test hôte ne le compile (le code du capteur, sht40.c, est joué par
 *         sim/test/test_sht40.c) ; ticket #606 : la carte v1.0 seule, son bus toujours déclaré (pins.h)
 *
 * Les lignes du bus (sht40.h) sur GPIO20 et GPIO21, la tâche de lecture, et la dernière lecture partagée avec la tâche de
 * SESSION.
 *
 * Pourquoi la lecture ne prend rien au temps réel de la session (bsk_sensor.h) :
 *   - la tâche est de priorité 0, celle de la tâche de repos, sous toutes les autres (la SESSION est à 1, ESP_TASK_MAIN_PRIO,
 *     main/main.c) : elle ne tourne que quand toutes les autres attendent, et la SESSION la préempte au tick où son
 *     vTaskDelay finit, comme elle préempterait la tâche de repos. Un pas de SESSION ne commence donc pas plus tard
 *     parce qu'une lecture est en cours : `pair_late` de DEBUG le mesurerait (PROTOCOL.md § 6) ;
 *   - le bus est mené à la main : aucune interruption, aucune section critique pendant une lecture ; une préemption au
 *     milieu ne fait qu'allonger un coup d'horloge, ce que l'I2C admet (le maître seul tient SCL, DS § 4.1). L'attente
 *     d'une mesure (9 ms au moins) se fait en vTaskDelay, sans occuper le processeur ; le reste, environ 1 ms d'attente active
 *     (esp_rom_delay_us) par lecture, n'est pris qu'à la tâche de repos ;
 *   - ce qui reste partagé : la copie de la dernière lecture, sous une section critique de quelques dizaines d'octets
 *     (bsk_sensor_read, sur la tâche de SESSION ; la tâche de lecture, à la fin de chaque lecture).
 *
 * Le chien de garde (sdkconfig.defaults) : la tâche n'y est pas abonnée. Une lecture n'attend aucune ligne (sht40.h) : sa
 * durée est bornée par ses pas, quelques dizaines de millisecondes au plus. Une tâche de lecture qui ne tournerait plus
 * (affamée, ou figée) ne ferait que laisser la dernière lecture vieillir : elle devient inconnue au bout de
 * BSK_SENSOR_STALE_US, ce que la ligne `* sht40` du journal dit ; redémarrer la carte pour elle couperait l'objectif.
 * Affamée, elle le serait avec la tâche de repos, que le chien de garde surveille déjà. */
#include "bsk_sensor.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_rom_sys.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "pins.h"
#include "sht40.h"

#define MUST(x) do { if (!(x)) abort(); } while (0)

#define HALF_US 5u      /* une demi-période d'horloge : 100 kHz, le mode standard (DS § 4.1) */
#define STACK   2048u
#define PRIO    0u      /* tskIDLE_PRIORITY : sous toutes les autres tâches (ci-dessus) */

static portMUX_TYPE  mux = portMUX_INITIALIZER_UNLOCKED;
static sht_state_t   shared;    /* la dernière lecture, sous mux ; à zéro : aucune (BSK_SENSOR_NONE) */
static TaskHandle_t  task;

/* Chaque écriture de ligne est suivie d'une demi-période : la ligne a le temps de monter par son tirage (10 kΩ), et le
 * niveau lu après un front montant de SCL est celui que le capteur tient. */
void sht_scl(bool high)
{
    gpio_set_level(PIN_SHT_SCL, high ? 1u : 0u);
    esp_rom_delay_us(HALF_US);
}

void sht_sda(bool high)
{
    gpio_set_level(PIN_SHT_SDA, high ? 1u : 0u);
    esp_rom_delay_us(HALF_US);
}

bool sht_sda_in(void) { return gpio_get_level(PIN_SHT_SDA) != 0; }

/* Au moins `ms` : un vTaskDelay de n ticks peut finir jusqu'à un tick plus tôt (le tick en cours compte). */
void sht_wait_ms(uint32_t ms) { vTaskDelay(pdMS_TO_TICKS(ms) + 1); }

/* Le bus ramené au repos au démarrage (plans/sht40.md § 4.2 : un capteur laissé au milieu d'une lecture), puis une
 * lecture par période, publiée à sa fin. L'instant d'une lecture est pris avant elle : la valeur en paraît un peu plus
 * vieille qu'elle n'est, jamais plus jeune. */
static void sensor_task(void *arg)
{
    sht_state_t mine = {.st = BSK_SENSOR_NONE};
    (void)arg;
    sht_recover();
    for (;;) {
        sht_cycle(&mine, (uint64_t)esp_timer_get_time());
        portENTER_CRITICAL(&mux);
        shared = mine;
        portEXIT_CRITICAL(&mux);
        vTaskDelay(pdMS_TO_TICKS(BSK_SENSOR_PERIOD_MS));
    }
}

/* Les deux lignes relâchées avant d'être des sorties (le registre de sortie d'abord) : leur mise en drain ouvert ne tire
 * rien à 0, pas de START parasite. gpio_config leur donne la fonction GPIO : l'UART0 de la ROM n'y est plus. */
void bsk_sensor_init(void)
{
    const gpio_config_t bus = {
        .pin_bit_mask = (1ULL << PIN_SHT_SCL) | (1ULL << PIN_SHT_SDA),
        .mode = GPIO_MODE_INPUT_OUTPUT_OD,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_set_level(PIN_SHT_SCL, 1u));
    ESP_ERROR_CHECK(gpio_set_level(PIN_SHT_SDA, 1u));
    ESP_ERROR_CHECK(gpio_config(&bus));
    /* le cœur 0, comme toutes les tâches (décision de l'humain) */
    MUST(xTaskCreatePinnedToCore(sensor_task, "sensor", STACK, NULL, PRIO, &task, 0) == pdPASS);
}

void bsk_sensor_read(bsk_sensor_t *v)
{
    sht_state_t s;
    portENTER_CRITICAL(&mux);
    s = shared;
    portEXIT_CRITICAL(&mux);
    sht_judge(&s, (uint64_t)esp_timer_get_time(), v);
}

/* ESP-IDF compte la pile en octets. bsk_sensor_init précède la boucle de app_main, seule à appeler ceci. */
uint32_t bsk_sensor_stack(void) { return (uint32_t)uxTaskGetStackHighWaterMark(task); }
