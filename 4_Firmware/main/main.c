/* SOURCE: composant main — app_main
 * AUTHOR: engineer
 * DATE: 2026-10-04
 * STATUS: actif — ticket #528 : la tâche de SESSION abonnée au chien de garde ; ticket #594 : le capteur SHT40 ;
 *         ticket #674 : la position de reprise en mémoire RTC ; ticket #691 : l'instant réel d'une commande
 *
 * Prépare la PHY, l'USB, le magasin de la marque, la SESSION et le capteur, puis avance la SESSION en boucle sur cette
 * tâche. */
#include <stdint.h>

#include "esp_attr.h"
#include "esp_err.h"
#include "esp_system.h"
#include "esp_task_wdt.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "bsk_host.h"
#include "bsk_led.h"
#include "bsk_phy.h"
#include "bsk_sensor.h"
#include "bsk_session.h"
#include "bsk_store.h"

/* Ordre imposé :
 *   - bsk_phy_init d'abord : tant que D2 ne dit pas « présent », la carte n'émet rien (TXD, BODY_CS et VD au repos,
 *     rails coupés) ;
 *   - le magasin prêt avant la SESSION, la LED après elle (son fixe « ON » dit la SESSION prête) ;
 *   - le capteur n'attend rien et rien ne l'attend (bsk_sensor.h) : après la LED, avant la boucle, qui seule lit sa
 *     marge de pile (DEBUG) ;
 *   - dans la boucle, après chaque pas : bsk_host_observe (accusés et instantané : last_op, move_rc), puis l'instant
 *     réel donné à la session (bsk_session_real) juste avant bsk_host_usb_service (la ligne en attente, déposée sur la
 *     tâche de bsk_session_step comme bsk_session.h l'exige) : `p1` ou `b` allument le rail logique entre deux pas, et
 *     le rail moteur se compte depuis cet instant, pas depuis le début du pas (la tâche peut avoir été préemptée entre
 *     les deux), puis la LED, qui lit l'instantané, rendu sur cette tâche seulement.
 * Rien ici n'écrit sur la console : elle est muette (sdkconfig.defaults). bench_core n'a rien à initialiser : son seul
 * état, le compteur de refus, part de zéro.
 *
 * La boucle rend la main un tick (1 ms) entre deux pas : le 0x03 et le 0x04 sont servis par le pas qui suit leurs
 * échéances, VD + 8,6 ms et VD + 10,1 ms ; un pas qui commence plus de 2 ms après l'échéance jette la paire (bsk_txn.h),
 * ce que DEBUG compte (pair_late, pair_late_us). La fenêtre se juge au début du pas, pas à l'émission : une trame part
 * plus tard du temps que le pas a passé avant elle, sans borne, dont l'attente de fin d'émission qui retient un pas,
 * relevée par DEBUG (tx_wait_us, tx_timeout).
 *
 * Le chien de garde (ticket #528, sdkconfig.defaults) : cette tâche s'abonne après les initialisations (celle de la NVS
 * peut effacer la partition) et le nourrit à chaque tour. Un tour attend l'émission de chaque trame : seule l'attente de
 * sa fin est bornée, à 100 ms (uart_wait_tx_done, components/phy/phy.c) ; l'écriture de ses octets vers l'UART
 * (uart_write_bytes) n'a pas d'échéance. Il attend aussi une écriture en flash du magasin, qui coupe le cache. Une
 * SESSION qui reste bloquée là, ou ailleurs, plus que les 10 s du chien de garde redémarre la carte, et DEBUG le dit
 * (reset=task_wdt). */

/* La position de reprise (bsk_session.h), en mémoire RTC FAST : RTC_NOINIT_ATTR, « Any variable marked with this
 * attribute will keep its value after restart or during a deep sleep / wake cycle » (esp_attr.h d'ESP-IDF 5.3.2), dans
 * la section .rtc_noinit, « data that should not be initialized at power up » (ld/esp32c3/sections.ld.in). Rien n'en
 * est écrit en NVS : la flash ne s'écrit que sur un geste de l'utilisateur (components/store/store.c). */
static RTC_NOINIT_ATTR bsk_session_keep_t keep;

/* Une reprise de la carte : un redémarrage par panic ou par un chien de garde. Sur l'esp32c3 (ESP-IDF 5.3.2,
 * port/soc/esp32c3/reset_reason.c), panic, task_wdt et int_wdt sont un redémarrage logiciel du processeur
 * (esp_restart_noos), wdt un reset par un chien de garde matériel ; aucun n'est une mise sous tension (si l'un d'eux
 * remettait aussi le domaine RTC, ce qui resterait en mémoire ne serait relu que s'il passe sa somme, restore.c ; sinon,
 * la marque). Toute autre cause
 * est un démarrage à froid, poweron et brownout d'abord (une coupure de courant ramène à la marque, règle de l'humain) :
 * la mémoire est alors effacée sans être lue (bsk_session_init), jamais prise pour une position. */
static bool warm_start(void)
{
    switch (esp_reset_reason()) {
    case ESP_RST_PANIC: case ESP_RST_INT_WDT: case ESP_RST_TASK_WDT: case ESP_RST_WDT: return true;
    default: return false;
    }
}

void app_main(void)
{
    bsk_session_params_t p;
    bsk_phy_init();
    bsk_host_usb_init();
    bsk_store_init();
    bsk_session_params_default(&p);
    p.keep = &keep;
    p.warm = warm_start();
    bsk_session_init(&p);
    bsk_led_board_init();
    bsk_sensor_init();
    ESP_ERROR_CHECK(esp_task_wdt_add(NULL));
    for (;;) {
        esp_task_wdt_reset();
        bsk_session_step((uint64_t)esp_timer_get_time());
        bsk_host_observe();
        bsk_session_real((uint64_t)esp_timer_get_time());
        bsk_host_usb_service();
        bsk_led_board_service((uint64_t)esp_timer_get_time());
        vTaskDelay(1);
    }
}
