/* SOURCE: spec de l'atelier § 1 (R1) et § 2 — la PHY de la carte (include/bsk_phy.h), sur ESP-IDF 5.3.2
 * AUTHOR: engineer
 * DATE: 2026-10-04
 * STATUS: actif — compilé par ESP-IDF seul : aucun test hôte ne le joue (seul phy_common.c l'est, dans les deux PHY) ;
 *         ticket #528 : l'ISR de l'UART en IRAM, la tâche abonnée au chien de garde, le compteur des fronts perdus, la
 *         VD refusée arrête la carte ; ticket #574 : les EN par leur polarité (pins.h), mis en sortie déjà coupés ;
 *         ticket #590 : les FAULT des rails lus par interruption, comptés, rapportés ;
 *         ticket #606 : les FAULT toujours lus, la carte v1.0 seule (pins.h)
 *
 * L'UART, les lignes, la VD, les rails, leurs FAULT et le verrou de XDETECT de la carte ; la réception en flux, la
 * présence et la lecture des FAULT viennent de phy_common.c. La fenêtre d'écoute (bsk_phy.h, bsk_phy_lines) : RXD
 * routée à l'UART lignes pilotées seulement, et la tâche de PHY qui ne rend rien de la réception fenêtre fermée ; ce que
 * RXD présente à l'objectif lignes au repos est démontré en tête de rxd_rest, d'après les sources d'ESP-IDF 5.3.2.
 *
 * Les FAULT des rails (bsk_phy.h, BSK_PHY_FAULT), démontrés ici en lecture, d'après les sources d'ESP-IDF 5.3.2 ; non
 * vérifié sur la carte :
 *   - un front n'est jamais compté deux fois. Le service d'interruption des GPIO (gpio_intr_service puis gpio_isr_loop,
 *     esp_driver_gpio/src/gpio.c) lit le statut des broches, puis, pour chacune, efface son statut avant d'appeler son
 *     interruption quand elle est armée sur un front (isr_clr_on_entry_mask, posé par gpio_set_intr_type pour
 *     GPIO_INTR_ANYEDGE, celui de fault_arm) ; un statut reposé ensuite relance le service, la source restant active.
 *     fault_isr relit le niveau par fault_settle (phy_common.h), la logique que test_phy joue contre une broche qui a ce
 *     statut ; seuls ses deux accès sont propres à la carte : fault_low, le niveau (gpio_ll_get_level, hal/esp32c3) ;
 *     fault_again, le statut de la broche (gpio_ll_get_intr_status, pcpu_int, le statut des interruptions armées vers le
 *     processeur), effacé s'il est posé (gpio_ll_clear_intr_status, status_w1tc), comme le service l'efface. La relecture
 *     du niveau ne se fait qu'après un effacement, et le statut est relu après elle : la prise que le statut d'un front vu
 *     dans le niveau relançait (le fantôme) ne l'est plus, et l'impulsion brève, deux fronts avant la prise, reste comptée ;
 *   - une rafale de fronts de FAULT ne touche pas raw_q. fault_isr n'y écrit rien : chaque prise va, une seule fois, dans
 *     fault_q (FAULT_LEN), et la tâche en fait les événements (fault_events). fault_q pleine, la prise n'est pas
 *     rapportée ; elle est comptée quand même (fault_edge, avant l'envoi), et ni raw_lost ni E_BUS n'en naissent. raw_q
 *     ne porte que LENS_CS, la VD et les rebonds de D2 : aucun de leurs fronts n'y est perdu pour un FAULT ;
 *   - ev_q, elle, reste commune : la tâche y pousse les changements d'un FAULT comme tout événement, et push y jette le
 *     plus ancien quand elle est pleine (evq_drop). Une rafale de FAULT plus rapide que la lecture de la SESSION peut donc
 *     y évincer des événements de LENS_CS ou de VD, comptés dans evq_drop. Au plus deux changements par prise ; le filtre
 *     du commutateur (5 ms au moins en surintensité) et ses cycles thermiques espacent les prises bien plus que la
 *     SESSION, qui lit la file à chaque pas (1 ms), ne tarde à la vider, hors d'un pas retenu (une écriture en flash).
 *
 * L'ordre des présences et des événements de la file que rend bsk_phy_poll, file pleine et tête jetée par push pendant la
 * lecture compris, est démontré en lecture en tête de bsk_phy_poll ; non vérifié sur la carte.
 */
#include "bsk_phy.h"

#include <stdint.h>
#include <stdlib.h>

#include "driver/gpio.h"
#include "driver/ledc.h"
#include "driver/uart.h"
#include "esp_attr.h"
#include "esp_rom_gpio.h"
#include "esp_rom_sys.h"
#include "esp_task_wdt.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
/* Exception à -Wconversion (components/phy/CMakeLists.txt), pour cet en-tête seul : les fonctions inline de
 * hal/gpio_ll.h d'ESP-IDF 5.3.2 (gpio_ll.h:187, :564, :565, et les macros REG_SET_FIELD / SET_PERI_REG_BITS de
 * soc/soc.h:87, :140 qu'elles déploient) convertissent sans cast ; on ne touche pas à ESP-IDF. Notre code qui les
 * appelle reste sous l'option. */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#include "hal/gpio_ll.h"
#pragma GCC diagnostic pop
#include "soc/gpio_pins.h"
#include "soc/io_mux_reg.h"
#include "soc/gpio_sig_map.h"
#include "soc/gpio_struct.h"

#include "phy_common.h"
#include "pins.h"

/* UART : 750 000 bauds 8N1, tampons de 4096 et 1200 octets, seuil de FIFO 8 et délai de réception de 2 symboles,
 * la configuration éprouvée sur le 135. CS_GUARD_US : la garde de BODY_CS autour des octets émis. */
#define BAUD         750000
#define CS_GUARD_US  40
#define UART_RX_BUF  4096
#define UART_TX_BUF  1200
#define UART_EV_LEN  32
#define RAW_LEN      64          /* fronts de LENS_CS et de VD, rebonds de D2, des interruptions à la tâche (raw_q) */
#define FAULT_LEN    16          /* prises des FAULT des rails, des interruptions à la tâche (fault_q) */
#define EV_LEN       32          /* événements remontés, en attente de bsk_phy_poll */
#define POLL_MS      5           /* D2 et le flux relus au moins toutes les 5 ms */

#define VD_TIMER   LEDC_TIMER_0
#define VD_CHANNEL LEDC_CHANNEL_0
#define VD_MODE    LEDC_LOW_SPEED_MODE
#define VD_RES     LEDC_TIMER_13_BIT
#define VD_LOW_US  60

/* Les niveaux des EN, de la polarité de la carte (pins.h, décision Q6 de l'humain) : toute écriture sur une broche EN
 * passe par eux, aucune n'emploie de niveau littéral ; le tirage vers « coupé » est posé par en_init. */
#define EN_ON   (EN_ACTIVE_LOW ? 0u : 1u)
#define EN_OFF  (EN_ACTIVE_LOW ? 1u : 0u)

#define MUST(x) do { if (!(x)) abort(); } while (0)

/* RAW_BOUNCE : posé par drop_expire */
typedef enum { RAW_LENS_CS, RAW_VD, RAW_BOUNCE } raw_kind_t;

typedef struct {
    raw_kind_t kind;
    bool       level;
    int64_t    t_us;
} raw_t;

/* Une prise d'un FAULT (fault_isr, fault_arm) : ce qu'elle a vu (fault_edge), le rail, son état commandé, son instant. */
typedef struct {
    fault_ev_t k;
    bsk_rail_t rail;
    bool       on;
    int64_t    t_us;
} fault_raw_t;

static QueueHandle_t uart_q, raw_q, fault_q, ev_q;
static QueueSetHandle_t set;
/* Les fronts qu'une interruption n'a pas pu poser (file des fronts pleine), comptés par les interruptions sous raw_mux
 * (cs_isr, vd_isr et drop_expire peuvent s'interrompre l'une l'autre si leurs niveaux diffèrent) ; la tâche de PHY lit
 * le compteur d'un seul accès (un mot aligné) et rapporte une E_BUS dès qu'il a changé depuis son dernier relevé
 * (raw_seen, à elle seule) : un front perdu entre la lecture et le relevé suivant reste compté, et rapporté au tour
 * suivant (audit M7 : le booléen d'avant pouvait être remis à faux par-dessus un front perdu). */
static volatile uint32_t raw_lost;
static portMUX_TYPE raw_mux = portMUX_INITIALIZER_UNLOCKED;

/* La présence et le verrou, partagés par les interruptions, la tâche de PHY et les commandes, sous pres_mux. */
static pres_t pres;
static portMUX_TYPE pres_mux = portMUX_INITIALIZER_UNLOCKED;
static esp_timer_handle_t drop_timer;    /* l'échéance d'une retombée */

/* Les FAULT et l'état commandé des rails, indexés par bsk_rail_t, sous pres_mux aussi : fault_isr les lit ensemble,
 * bsk_phy_rail et cut écrivent rail_on avec les EN. */
static fault_t fault[2];
static bool    rail_on[2];

/* La fenêtre d'écoute (bsk_phy.h, bsk_phy_lines) : sa génération, impaire ouverte, paire fermée, avancée d'un à chaque
 * ouverture et à chaque fermeture, sous pres_mux, par bsk_phy_lines (tâche de SESSION) et par cut (interruptions). La tâche
 * de PHY la lit d'un seul accès (un mot aligné) au début de chaque jugement (heard). */
static volatile uint32_t rx_gen;

/* État de la tâche de PHY, qu'elle seule touche. */
static rx_t    rx;                       /* la réception en flux (phy_common.h) */
static uint32_t rx_seen;                 /* rx_gen au dernier jugement */
static bool    lens_cs;
static uint32_t raw_seen;                /* raw_lost au dernier relevé */

/* Les compteurs de DEBUG (bsk_phy_stats), à zéro au démarrage. evq_drop : écrit par la tâche de PHY seule (push), lu par
 * la tâche de SESSION, un mot aligné, lu et écrit d'un seul accès. tx_wait_us et tx_timeout : la tâche de SESSION seule,
 * qui émet (bsk_phy_send) et les lit. */
static volatile uint32_t evq_drop;
static uint32_t tx_wait_us, tx_timeout;
static bool     tx_late;                 /* la dernière attente de fin d'émission a échoué (bsk_phy_send) : la tâche de
                                            SESSION seule */

/* ─────────────────────────── interruptions ─────────────────────────── */
/* Tout ce qui touche au verrou de XDETECT le fait sous pres_mux, la section critique que prennent aussi les
 * interruptions :
 *   - bsk_phy_rail lit le verrou et allume le rail sous elle : aucune retombée entre les deux ;
 *   - bsk_phy_lines(true) et bsk_phy_vd(hz) passent par le pilote (uart_set_pin, ledc_*), qu'on n'appelle pas sous
 *     section critique : le verrou est lu avant (refus, rien n'est touché), puis relu après, sous elle (lock_held,
 *     lines_held) ;
 *     s'il est tombé pendant l'appel, cut() est rejouée et la commande rend false. La fenêtre où une ligne peut être
 *     pilotée après la retombée est la fin de l'appel au pilote, quelques microsecondes ;
 *   - la tâche de PHY lit D2 au moins toutes les POLL_MS pour l'insertion (pres_sample), bsk_phy_poll rend la présence
 *     tenue (pres_next). La retombée, elle, n'est pas lue par la tâche : elle passe par detect_isr. */

static void IRAM_ATTR raw_lost_one(void)
{
    portENTER_CRITICAL_ISR(&raw_mux);
    raw_lost = raw_lost + 1;
    portEXIT_CRITICAL_ISR(&raw_mux);
}

static void IRAM_ATTR raw_send(const raw_t *r)
{
    BaseType_t hp = pdFALSE;
    if (xQueueSendFromISR(raw_q, r, &hp) != pdTRUE) raw_lost_one();
    if (hp) portYIELD_FROM_ISR();
}

static void IRAM_ATTR raw_from_isr(raw_kind_t kind, bool level)
{
    raw_t r = {.kind = kind, .level = level, .t_us = esp_timer_get_time()};
    raw_send(&r);
}

static void IRAM_ATTR cs_isr(void *arg)
{
    (void)arg;
    raw_from_isr(RAW_LENS_CS, gpio_ll_get_level(&GPIO, PIN_LENS_CS) != 0);
}

static void IRAM_ATTR vd_isr(void *arg)
{
    (void)arg;
    raw_from_isr(RAW_VD, false);
}

/* RXD au repos, la fenêtre d'écoute fermée (bsk_phy.h, bsk_phy_lines) : l'entrée de réception de l'UART reliée au 1
 * constant de la matrice, son repos, qui ne fait ni octet ni erreur ; la broche tirée bas, son tirage haut retiré. Appelée
 * sous pres_mux, par cut en interruption comme par bsk_phy_lines(false) et bsk_phy_init : en IRAM, des registres et une
 * fonction de la ROM (esp_rom_gpio_connect_in_signal, gpio_matrix_in de la ROM du C3, esp32c3.rom.api.ld), rien en
 * flash. Le signal est nommé par sa constante (U1RXD_IN_IDX), pas par la table uart_periph_signal, et le tirage bas est
 * écrit dans le registre IO_MUX de la broche, comme gpio_ll_pulldown_dis le fait, pas par gpio_ll_pulldown_en, qui
 * l'adresse par la table GPIO_PIN_MUX_REG : deux tables en flash.
 * Lignes au repos, RXD ne présente aucun niveau haut à l'objectif. Pourquoi c'est vrai (sources d'ESP-IDF 5.3.2) :
 *   - le tirage haut vient d'uart_set_pin (esp_driver_uart/src/uart.c) : pour une broche qui n'est pas la broche par
 *     défaut de l'UART dans l'IO_MUX (uart_try_set_iomux_pin), et GPIO2 ne l'est pas (l'UART1 du C3 n'en a aucune,
 *     soc/include/soc/uart_periph.h), il appelle gpio_func_sel, gpio_set_pull_mode(rx, GPIO_PULLUP_ONLY) (le tirage bas
 *     retiré, le haut posé, esp_driver_gpio/src/gpio.c), gpio_set_direction(rx, GPIO_MODE_INPUT), puis relie l'entrée
 *     de l'UART à la broche. RXD routée, la carte tire donc la ligne vers le 3,3 V : ce firmware la routait dès
 *     bsk_phy_init et ne la détachait jamais, ce tirage était présenté à l'objectif rails coupés. Elle n'est
 *     plus routée que lignes pilotées (bsk_phy_lines(true)), et la SESSION ne pilote les lignes que les deux rails
 *     établis (bsk_phy.h) : l'objectif est alimenté ;
 *   - ici, le tirage haut est retiré et le tirage bas posé (FUN_PU et FUN_PD du registre IO_MUX de la broche), et
 *     l'entrée de l'UART reliée au 1 constant (GPIO_MATRIX_CONST_ONE_INPUT, soc/esp32c3/include/soc/gpio_pins.h) ; la
 *     sortie de RXD n'est jamais ouverte : uart_set_pin la ferme (gpio_set_direction en entrée appelle
 *     gpio_output_disable), bsk_phy_init la configure en entrée, et aucune ligne de ce fichier ne l'ouvre ;
 *   - au reset, GPIO2 n'a ni sortie ni tirage, l'entrée seule (DS C3 p. 16, relevé dans plans/carte-v1.0.md de
 *     l'atelier, § 2.3), et aucun tirage externe ; bsk_phy_init la tire bas (la configuration des lignes) avant
 *     d'installer l'UART, et rien avant lui ne la touche (le bootloader ne configure de broche que pour une console UART
 *     ou des options qui ne sont pas posées, en_init). Broche de strapping, elle n'est lue qu'au reset et ne décide pas
 *     du mode de démarrage (pins.h) : son tirage bas est sans effet sur lui.
 * Non vérifié sur la carte. */
_Static_assert(LENS_UART == UART_NUM_1, "rxd_rest nomme l'entrée de réception de l'UART1 (U1RXD_IN_IDX)");

static void IRAM_ATTR rxd_rest(void)
{
    esp_rom_gpio_connect_in_signal(GPIO_MATRIX_CONST_ONE_INPUT, U1RXD_IN_IDX, false);
    gpio_ll_pullup_dis(&GPIO, PIN_LENS_RXD);
    REG_SET_BIT(IO_MUX_GPIO0_REG + PIN_LENS_RXD * 4u, FUN_PD);
}

/* La coupure du verrou, sans tâche ni file, en IRAM, des registres seulement (gpio_ll, en ligne) et rxd_rest. Appelée
 * sous pres_mux. Les rails ; l'interruption de la VD avant sa sortie (la retombée vers le repos n'est pas un front de VD) ;
 * la sortie de TXD, BODY_CS et VD coupée, détachée de l'UART et du LEDC ; la fenêtre d'écoute fermée, RXD au repos. Le
 * tirage bas de TXD, BODY_CS et VD y est déjà, posé par bsk_phy_init et rest(), et aucune commande de ce fichier ne le
 * retire ; celui de RXD, qu'uart_set_pin remplace par un tirage haut, est reposé par rxd_rest.
 * Elle n'arrête pas le LEDC (ledc_stop n'est pas en IRAM) : il tourne, détaché de la broche et sans interruption,
 * jusqu'à ce que la SESSION arrête la VD en passant à OFF, au pas suivant (une milliseconde environ, plus une écriture
 * en flash en cours). De la retombée à la coupure : 2 ms, plus la prise de l'interruption de l'esp_timer et le parcours
 * de sa liste (quelques dizaines de microsecondes estimées), plus la plus longue section critique en cours sur le
 * cœur 0 — non mesuré. */
static void IRAM_ATTR cut(void)
{
    gpio_ll_set_level(&GPIO, PIN_EN_LOGIC, EN_OFF);
    gpio_ll_set_level(&GPIO, PIN_EN_MOTOR, EN_OFF);
    rail_on[BSK_RAIL_LOGIC] = rail_on[BSK_RAIL_MOTOR] = false;
    gpio_ll_intr_disable(&GPIO, PIN_BODY_VD);
    gpio_ll_output_disable(&GPIO, PIN_LENS_TXD);
    gpio_ll_output_disable(&GPIO, PIN_BODY_CS);
    gpio_ll_output_disable(&GPIO, PIN_BODY_VD);
    if (rx_gen & 1u) rx_gen = rx_gen + 1;
    rxd_rest();
}

/* L'échéance d'une retombée, rappelée en interruption par drop_timer : D2 relu, absent (tirage haut), la coupure ;
 * présent, le rebond vers la tâche par la file des fronts (pleine, une E_BUS comme pour un front perdu). Le réveil
 * passe par esp_timer_isr_dispatch_need_yield, comme l'exige un rappel ESP_TIMER_ISR, pas par portYIELD_FROM_ISR. */
static void IRAM_ATTR drop_expire(void *arg)
{
    uint64_t t = (uint64_t)esp_timer_get_time();
    bool absent = gpio_ll_get_level(&GPIO, PIN_LENS_DETECT) != 0;
    (void)arg;
    portENTER_CRITICAL_ISR(&pres_mux);
    if (pres_expire(&pres, absent, t)) cut();
    portEXIT_CRITICAL_ISR(&pres_mux);
    if (!absent) {
        raw_t r = {.kind = RAW_BOUNCE, .t_us = (int64_t)t};
        BaseType_t hp = pdFALSE;
        if (xQueueSendFromISR(raw_q, &r, &hp) != pdTRUE) raw_lost_one();
        if (hp) esp_timer_isr_dispatch_need_yield();
    }
}

/* La retombée de XDETECT (front montant : le tirage haut, objectif absent) : objectif présent, le premier front arme
 * l'échéance ; absent, la coupure (pres_edge). Si l'esp_timer refuse d'être armé, la coupure part tout de suite (le
 * côté sûr). */
static void IRAM_ATTR detect_isr(void *arg)
{
    uint64_t t = (uint64_t)esp_timer_get_time();
    (void)arg;
    portENTER_CRITICAL_ISR(&pres_mux);
    switch (pres_edge(&pres, t)) {
    case PRES_CUT:
        cut();
        break;
    case PRES_ARM:
        if (esp_timer_start_once(drop_timer, D2_DROP_US) != ESP_OK && pres_expire(&pres, true, t)) cut();
        break;
    case PRES_WAIT:
        break;
    }
    portEXIT_CRITICAL_ISR(&pres_mux);
}

#define FAULT_PIN(rail) ((uint32_t)((rail) == BSK_RAIL_LOGIC ? PIN_FAULT_LOGIC : PIN_FAULT_MOTOR))

/* Les deux accès de fault_settle à la broche d'un FAULT, des registres seulement (gpio_ll, en ligne) : son niveau
 * (true : FAULT# à 0) ; son statut d'interruption, posé par un front depuis son dernier effacement, effacé s'il l'est. */
static bool IRAM_ATTR fault_low(bsk_rail_t rail) { return gpio_ll_get_level(&GPIO, FAULT_PIN(rail)) == 0; }

static bool IRAM_ATTR fault_again(bsk_rail_t rail)
{
    uint32_t st;
    gpio_ll_get_intr_status(&GPIO, 0, &st);
    if (!(st & (1u << FAULT_PIN(rail)))) return false;
    gpio_ll_clear_intr_status(&GPIO, 1u << FAULT_PIN(rail));
    return true;
}

/* Un front de FAULT# (les deux sens, arg : le rail) : le niveau relu tant qu'un front arrive pendant la prise
 * (fault_settle, en tête de ce fichier), ce qu'il dit depuis le dernier connu (fault_edge, qui compte), et l'état commandé
 * du rail, lus ensemble sous pres_mux ; puis la prise vers la tâche, par sa propre file, jamais par celle des fronts
 * (pleine : la prise n'est pas rapportée, le compte, lui, est fait). Rien n'est coupé ni décidé ici. */
static void IRAM_ATTR fault_isr(void *arg)
{
    bsk_rail_t rail = (bsk_rail_t)(uintptr_t)arg;
    fault_raw_t r = {.t_us = esp_timer_get_time(), .rail = rail};
    bool active = fault_settle(fault_low, fault_again, rail);
    BaseType_t hp = pdFALSE;
    portENTER_CRITICAL_ISR(&pres_mux);
    r.k = fault_edge(&fault[rail], active);
    r.on = rail_on[rail];
    portEXIT_CRITICAL_ISR(&pres_mux);
    (void)xQueueSendFromISR(fault_q, &r, &hp);
    if (hp) portYIELD_FROM_ISR();
}

/* Le verrou levé ? Sans rien toucher. */
static bool lock_open(void)
{
    bool open;
    portENTER_CRITICAL(&pres_mux);
    open = pres.present;
    portEXIT_CRITICAL(&pres_mux);
    return open;
}

/* Après une commande passée par le pilote : le verrou encore levé ? Sinon, la coupure rejouée. */
static bool lock_held(void)
{
    bool open;
    portENTER_CRITICAL(&pres_mux);
    open = pres.present;
    if (!open) cut();
    portEXIT_CRITICAL(&pres_mux);
    return open;
}

/* ─────────────────────────── la tâche de PHY ─────────────────────────── */

/* La file pleine (personne ne lit assez vite) : on perd le plus ancien, compté (evq_drop de DEBUG). Le seul
 * producteur est cette tâche : la place libérée est la sienne. */
static void push(const bsk_phy_event_t *e)
{
    if (xQueueSend(ev_q, e, 0) != pdTRUE) {
        bsk_phy_event_t old;
        if (xQueueReceive(ev_q, &old, 0) == pdTRUE) evq_drop = evq_drop + 1;
        (void)xQueueSend(ev_q, e, 0);
    }
}

/* La fenêtre d'écoute, lue au début de chaque jugement : true, ouverte. Une génération changée depuis le dernier
 * jugement vide la réception sans rien rapporter (rx_clear) : ce qu'elle tenait a été reçu dans une fenêtre fermée depuis.
 * La règle de la PHY simulée (sim/phy_sim.c), qui juge chaque octet à son instant. Ici, un jugement n'est pas coupé en deux
 * par une commande de la SESSION : sa tâche, celle d'app_main, a la priorité 1 (ESP_TASK_MAIN_PRIO), celle-ci 12, sur un
 * seul cœur, et un jugement n'attend rien (uart_read_bytes et la file des événements sans attente). Seule la coupure du
 * verrou, en interruption, peut tomber entre la lecture de la fenêtre et la file : l'événement jugé alors est poussé
 * après elle, fait d'octets reçus lignes pilotées, comme un événement déjà dans la file. */
static bool heard(void)
{
    uint32_t g = rx_gen;
    if (g != rx_seen) {
        rx_seen = g;
        rx_clear(&rx);
    }
    return (g & 1u) != 0;
}

/* Ce que le flux a à rendre, jugé maintenant, fenêtre d'écoute ouverte. */
static void drain(void)
{
    bsk_phy_event_t e = {.kind = BSK_PHY_FRAME};
    if (!heard()) return;
    while (rx_next(&rx, (uint64_t)esp_timer_get_time(), &e)) push(&e);
}

/* Une E_BUS. flush, une erreur du pilote UART (débordement du FIFO ou du tampon, erreur de trame, de parité, break) :
 * le flux est vidé, des octets lui manquent, sa suite est jugée comme tout octet reçu, et l'erreur porte ceux qu'il
 * n'avait pas encore rapportés (rendus par le pilote, pris par aucune trame : rx_flush) — les octets écartés en attente
 * de leur E_FRAMING, qui n'en aura pas, puis ceux qui attendaient leur jugement. Aucun autre n'est disponible : ceux que
 * uart_flush_input jette ne sont pas lus, et le pilote ne rend pas ceux qu'il a perdus. Sinon, un front de LENS_CS ou de
 * VD perdu (file des interruptions pleine) : le flux n'est pas touché, aucun octet. */
static void bus_error(bool flush)
{
    bsk_phy_event_t e = {.kind = BSK_PHY_ERROR, .t_us = (uint64_t)esp_timer_get_time()};
    e.u.err = E_BUS;
    if (flush) rx_flush(&rx, &e.u.raw);
    push(&e);
}

/* Fenêtre d'écoute fermée, rien n'est lu ni rapporté : ce que le pilote tient est jeté (RXD détachée, rien n'y entre
 * plus ; ce qui y était avant est d'une fenêtre fermée). */
static void on_uart(const uart_event_t *u)
{
    if (!heard()) {
        uart_flush_input(LENS_UART);
        return;
    }
    switch (u->type) {
    case UART_DATA: {                          /* lu dans la place libre, jamais nulle après drain, jugé à sa lecture */
        size_t n = u->size;
        while (n) {
            size_t room = RX_CAP - rx.n;
            int k = uart_read_bytes(LENS_UART, rx.b + rx.n, n < room ? n : room, 0);
            if (k <= 0) break;
            rx_got(&rx, (size_t)k, (uint64_t)esp_timer_get_time());
            n -= (size_t)k;
            drain();
        }
        break;
    }
    case UART_FIFO_OVF:
    case UART_BUFFER_FULL:
        uart_flush_input(LENS_UART);
        bus_error(true);
        break;
    case UART_FRAME_ERR:
    case UART_PARITY_ERR:
    case UART_BREAK:
        bus_error(true);
        break;
    default:                                   /* émission (UART_DATA_BREAK), rien d'autre n'est armé */
        break;
    }
}

/* Le ou les changements d'une prise d'un FAULT, au même instant (fault_events, phy_common.c). */
static void on_fault(const fault_raw_t *r)
{
    bsk_phy_event_t e[2];
    uint8_t n = fault_events(r->k, r->rail, r->on, (uint64_t)r->t_us, e);
    for (uint8_t i = 0; i < n; i++) push(&e[i]);
}

static void on_raw(const raw_t *r)
{
    bsk_phy_event_t e = {.t_us = (uint64_t)r->t_us};
    if (r->kind == RAW_VD || r->kind == RAW_BOUNCE) {
        e.kind = r->kind == RAW_VD ? BSK_PHY_VD : BSK_PHY_BOUNCE;
        push(&e);
        return;
    }
    if (r->level == lens_cs) return;
    lens_cs = r->level;
    e.kind = BSK_PHY_LENS_CS;
    e.u.level = lens_cs;
    push(&e);
}

/* Abonnée au chien de garde (ticket #528, sdkconfig.defaults), nourri à chaque tour : sa seule attente est la file des
 * événements, bornée à POLL_MS, et rien d'autre n'y attend (le pilote UART est lu sans attente). Figée, elle ne rendrait
 * plus ni trame, ni front, ni insertion : la SESSION verrait un objectif muet, perdu, sans que rien ne redémarre ; le
 * chien de garde redémarre la carte. Un flux de bruit qui l'occupe la ralentit sans la figer : elle le nourrit encore. */
static void phy_task(void *arg)
{
    (void)arg;
    ESP_ERROR_CHECK(esp_task_wdt_add(NULL));
    for (;;) {
        QueueSetMemberHandle_t q;
        uint32_t lost;
        esp_task_wdt_reset();
        q = xQueueSelectFromSet(set, pdMS_TO_TICKS(POLL_MS));
        if (q == uart_q) {
            uart_event_t u;
            if (xQueueReceive(uart_q, &u, 0) == pdTRUE) on_uart(&u);
        } else if (q == raw_q) {
            raw_t r;
            if (xQueueReceive(raw_q, &r, 0) == pdTRUE) on_raw(&r);
        } else if (q == fault_q) {
            fault_raw_t r;
            if (xQueueReceive(fault_q, &r, 0) == pdTRUE) on_fault(&r);
        }
        lost = raw_lost;
        if (lost != raw_seen) {                /* des fronts perdus depuis le relevé : LENS_CS publiée peut être fausse */
            raw_seen = lost;
            bus_error(false);
        }
        drain();                               /* un début de trame périmé */
        {                                      /* l'insertion ; tenue dans pres, rendue par bsk_phy_poll */
            bool present = gpio_get_level(PIN_LENS_DETECT) == 0;
            uint64_t t = (uint64_t)esp_timer_get_time();
            portENTER_CRITICAL(&pres_mux);
            (void)pres_sample(&pres, present, t);
            portEXIT_CRITICAL(&pres_mux);
        }
    }
}

/* ─────────────────────────── bsk_phy.h ─────────────────────────── */

/* BODY_CS levée, garde, les octets, attente de fin d'émission, garde, BODY_CS rabaissée.
 * L'attente de fin d'émission (uart_wait_tx_done, 100 ms au plus) bloque l'appelant, la tâche de SESSION : sa durée la
 * plus longue et ses échéances atteintes sont relevées pour DEBUG (tx_wait_us, tx_timeout), deux lectures de
 * l'horloge, rien d'autre. tx_wait_us est la durée de cet appel seul, du retour d'uart_write_bytes au retour
 * d'uart_wait_tx_done (l'envoi des octets sur le fil, ce qui préempte la SESSION pendant ce temps) : ni les deux gardes
 * de BODY_CS, ni la copie dans le tampon du pilote (uart_write_bytes), ni une trame refusée sans rien écrire n'y
 * comptent. tx_timeout compte les attentes qui rendent ESP_ERR_TIMEOUT, et les trames refusées sans rien écrire (la
 * relecture du repos ratée, ci-dessous) ; une attente en autre erreur rend E_BUS sans être comptée.
 * L'échéance d'ensemble : une émission bloque la tâche de SESSION au plus 100 ms, les deux gardes et la copie dans le
 * tampon du pilote, uart_write_bytes compris. Démontré en lecture, d'après les sources d'ESP-IDF 5.3.2
 * (esp_driver_uart/src/uart.c) ; non vérifié sur la carte :
 *   - uart_write_bytes (uart_tx_all) n'attend, sans échéance (portMAX_DELAY), qu'en deux points : tx_mux, puis la place
 *     dans le tampon d'émission (xRingbufferSend, l'événement puis les données, un anneau RINGBUF_TYPE_NOSPLIT de
 *     UART_TX_BUF octets). Le pilote n'émet pas d'autre façon : le contrôle de flux est coupé (bsk_phy_init), et les
 *     octets ne dépendent que de l'horloge de l'UART ;
 *   - tx_mux n'est pris que par les fonctions d'émission du pilote, uart_write_bytes et uart_wait_tx_done ici, qui le
 *     rendent avant de rendre la main ; seule bsk_phy_send les appelle, sur la tâche de SESSION seule : il est toujours
 *     libre quand elle le demande ;
 *   - le tampon n'est jamais plein : aucune trame n'est écrite sans que l'émission d'avant ait été vue finie, l'UART au
 *     repos (uart_wait_tx_done rend ESP_OK sur uart_hal_is_tx_idle, ou sur le TX_DONE de son interruption). Après une
 *     attente échue ou en erreur (tx_late), la trame suivante relit le repos sans attendre (0 tic) ; pas au repos, elle
 *     n'écrit rien, rend E_BUS et compte une échéance (tx_timeout) : une émission ratée, comme une attente échue. Au
 *     moment d'écrire, le tampon ne tient donc que des octets que l'interruption du pilote (en IRAM,
 *     CONFIG_UART_ISR_IN_IRAM) n'a pas encore passés au FIFO quand l'UART s'est vu au repos, au plus quelques trames,
 *     qu'elle passera sans la tâche de SESSION : une trame y prend deux éléments,
 *     l'événement et ses données (au plus BSK_FRAME_MAX octets, sous la moitié de la taille d'élément la plus grande),
 *     chacun avec 8 octets d'en-tête, moins de 300 octets ; UART_TX_BUF en tient quatre.
 * Rien ne change sur le fil ni dans la durée d'une émission qui aboutit : la relecture du repos ne se fait qu'après une
 * attente ratée.
 * Pas d'attente que LENS_CS retombe avant d'émettre : quand émettre est la décision de TRANSACTION, propriétaire du bus,
 * qui voit LENS_CS en événement. Pas de mutex d'émission, ni de paire 0x03/0x04 sous un même numéro de séquence : un
 * seul émetteur, et la séquence est un champ de la trame, posé au-dessus. */
bsk_err_t bsk_phy_send(const bsk_frame_t *f)
{
    uint8_t b[BSK_FRAME_MAX];
    size_t n = fr_encode(f->cls, f->seq, f->msg, f->len, b, sizeof b);
    int w;
    esp_err_t e;
    int64_t t0, dt;
    if (tx_late && uart_wait_tx_done(LENS_UART, 0) != ESP_OK) {   /* la trame d'avant pas encore partie : rien n'est écrit */
        tx_timeout++;
        return E_BUS;
    }
    tx_late = false;
    gpio_set_level(PIN_BODY_CS, 1);
    esp_rom_delay_us(CS_GUARD_US);
    w = uart_write_bytes(LENS_UART, b, n);
    t0 = esp_timer_get_time();
    e = uart_wait_tx_done(LENS_UART, pdMS_TO_TICKS(100));
    dt = esp_timer_get_time() - t0;
    if (dt > (int64_t)tx_wait_us) tx_wait_us = (uint32_t)dt;   /* 100 ms et une préemption : loin de 2^32 µs */
    if (e == ESP_ERR_TIMEOUT) tx_timeout++;
    tx_late = e != ESP_OK;
    esp_rom_delay_us(CS_GUARD_US);
    gpio_set_level(PIN_BODY_CS, 0);
    return w == (int)n && e == ESP_OK ? E_OK : E_BUS;
}

void bsk_phy_body_cs(bool high) { gpio_set_level(PIN_BODY_CS, high ? 1 : 0); }

/* La broche elle-même, pas l'état de la tâche de PHY, qui publie ses fronts. */
bool bsk_phy_lens_cs(void) { return gpio_get_level(PIN_LENS_CS) != 0; }

/* Le repos d'une ligne émise : haute impédance, tirée bas, au démarrage comme à l'arrêt. Sur la carte cible, les rails
 * de l'objectif sont commutés, et une ligne haute vers un objectif non alimenté l'alimenterait par ses diodes de
 * protection. Sur le proto, aux rails non commutés, c'est sans conséquence : le 135 jette tout octet reçu BODY_CS
 * basse, et une VD basse ne produit aucun front. gpio_set_direction en entrée coupe la
 * sortie et détache de la broche tout signal de périphérique, UART ou LEDC (gpio_output_disable ->
 * gpio_ll_output_disable, ESP-IDF 5.3.2). Le tirage d'abord : la broche ne flotte pas entre les deux. */
static void rest(gpio_num_t pin)
{
    gpio_set_pull_mode(pin, GPIO_PULLDOWN_ONLY);
    gpio_set_direction(pin, GPIO_MODE_INPUT);
}

/* L'interruption d'abord : la retombée vers le repos n'est pas un front de VD. */
static void vd_stop(void)
{
    gpio_intr_disable(PIN_BODY_VD);
    ledc_stop(VD_MODE, VD_CHANNEL, 0);
    rest(PIN_BODY_VD);
}

/* LEDC 13 bits, une impulsion basse de VD_LOW_US en tête de période ; la broche relue pour l'interruption du front
 * descendant, qui date chaque VD. La SESSION ne demande que VD_HZ ou 0, et la configuration du LEDC est constante : un
 * refus du LEDC ne peut être qu'un bug, et il arrête la carte (MUST : DEBUG dira reset=panic) au lieu de laisser la VD
 * arrêtée en silence (ticket #528, audit M2). */
bool bsk_phy_vd(uint16_t hz)
{
    uint32_t period_us, low_ticks;
    ledc_timer_config_t t = {
        .speed_mode = VD_MODE,
        .duty_resolution = VD_RES,
        .timer_num = VD_TIMER,
        .freq_hz = hz,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ledc_channel_config_t c = {
        .gpio_num = PIN_BODY_VD,
        .speed_mode = VD_MODE,
        .channel = VD_CHANNEL,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = VD_TIMER,
    };
    if (hz == 0) {
        vd_stop();
        return true;
    }
    if (!lock_open()) return false;
    MUST(ledc_timer_config(&t) == ESP_OK);
    period_us = 1000000u / hz;
    low_ticks = (VD_LOW_US * 8192u + period_us / 2) / period_us;   /* 29 pas à 60 Hz (VD_HZ), seule cadence */
    c.duty = 8192u - low_ticks;                                     /* durée à l'état haut */
    c.hpoint = (int)low_ticks;                                      /* l'impulsion basse en tête de période */
    MUST(ledc_channel_config(&c) == ESP_OK);
    /* relire notre propre sortie pour dater chaque front, sans toucher au routage de sortie (LEDC) */
    gpio_ll_input_enable(&GPIO, PIN_BODY_VD);
    gpio_set_intr_type(PIN_BODY_VD, GPIO_INTR_NEGEDGE);
    gpio_intr_enable(PIN_BODY_VD);
    return lock_held();
}

void bsk_phy_stats(bsk_phy_stats_t *st)
{
    st->evq_drop = evq_drop;
    st->tx_wait_us = tx_wait_us;
    st->tx_timeout = tx_timeout;
    portENTER_CRITICAL(&pres_mux);
    st->fault_motor = fault[BSK_RAIL_MOTOR].n;
    st->fault_logic = fault[BSK_RAIL_LOGIC].n;
    portEXIT_CRITICAL(&pres_mux);
}

/* L'événement retiré de la file et pas encore rendu, à la tâche de SESSION seule (bsk_phy_poll) : push ne le voit pas. */
static bsk_phy_event_t held;
static bool held_ok;

/* La présence tenue d'abord, si elle précède l'événement retenu ; sinon l'événement, rendu tel qu'il a été comparé.
 * Pourquoi l'événement est retiré avant d'être comparé, et non regardé en tête de file puis retiré (le défaut O14 de
 * l'audit, ce que faisait ce code) : file pleine, push, sur la tâche de PHY (priorité 12, qui préempte la SESSION,
 * priorité 1), jette la tête pour faire place au nouveau ; entre le regard et le retrait, l'événement retiré n'était plus
 * celui qui avait été comparé, et un événement plus récent qu'une retombée pouvait être rendu avant elle. Ici :
 *   - l'événement comparé est `held`, une copie hors de la file que seule cette fonction touche : celui qui est rendu
 *     est celui qui a été comparé, quoi que push fasse de la file pendant ce temps. Comparé à une présence plus ancienne,
 *     il est gardé pour l'appel suivant, où il est comparé de nouveau ;
 *   - un événement rendu `e` est plus ancien que toute retombée pas encore rendue : pres_next, sous pres_mux, n'en tenait
 *     aucune datée au plus de e.t_us ; une retombée posée ensuite l'est par une interruption (detect_isr, drop_expire)
 *     qui lit son instant en entrant, et une interruption qui a commencé finit avant que la SESSION ne reprenne (un seul
 *     cœur) : elle a commencé après la section critique, donc après la création de `e`, qui date son instant ;
 *   - rien n'est perdu de plus : push compte dans evq_drop chaque événement qu'il jette de la file pleine, comme avant ;
 *     `held` n'est plus dans la file, et ne peut pas être jeté. La présence ne passe pas par la file (pres_next) ;
 *   - rien n'attend : la file est lue sans attente, et pres_mux n'entoure que pres_next, sans appel à FreeRTOS. */
bool bsk_phy_poll(bsk_phy_event_t *ev)
{
    bool got;
    if (!held_ok) held_ok = xQueueReceive(ev_q, &held, 0) == pdTRUE;
    portENTER_CRITICAL(&pres_mux);
    got = pres_next(&pres, held_ok ? held.t_us : UINT64_MAX, ev);
    portEXIT_CRITICAL(&pres_mux);
    if (got || !held_ok) return got;
    *ev = held;
    held_ok = false;
    return true;
}

bool bsk_phy_rail(bsk_rail_t rail, bool on)
{
    bool ok;
    portENTER_CRITICAL(&pres_mux);
    ok = !on || pres.present;
    if (ok) {
        gpio_ll_set_level(&GPIO, rail == BSK_RAIL_LOGIC ? PIN_EN_LOGIC : PIN_EN_MOTOR, on ? EN_ON : EN_OFF);
        rail_on[rail] = on;
    }
    portEXIT_CRITICAL(&pres_mux);
    return ok;
}

/* Après le routage des lignes : le verrou encore levé, la fenêtre d'écoute ouverte (déjà ouverte, la reprise, elle le
 * reste) ; sinon, la coupure rejouée. */
static bool lines_held(void)
{
    bool open;
    portENTER_CRITICAL(&pres_mux);
    open = pres.present;
    if (!open) cut();
    else if (!(rx_gen & 1u)) rx_gen = rx_gen + 1;
    portEXIT_CRITICAL(&pres_mux);
    return open;
}

/* Pilotées : TXD rattachée à l'UART (uart_set_pin la pose haute avant de la router), RXD aussi (tirée haut, rxd_rest),
 * BODY_CS basse avant que sa sortie ne soit ouverte, puis la fenêtre d'écoute ouverte. Relâchées : la fenêtre fermée et
 * RXD au repos d'abord, puis TXD et BODY_CS. La VD n'est pas touchée : bsk_phy_vd la démarre. */
bool bsk_phy_lines(bool drive)
{
    if (!drive) {
        portENTER_CRITICAL(&pres_mux);
        if (rx_gen & 1u) rx_gen = rx_gen + 1;
        rxd_rest();
        portEXIT_CRITICAL(&pres_mux);
        rest(PIN_LENS_TXD);
        rest(PIN_BODY_CS);
        return true;
    }
    if (!lock_open()) return false;
    ESP_ERROR_CHECK(uart_set_pin(LENS_UART, PIN_LENS_TXD, PIN_LENS_RXD, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    gpio_set_level(PIN_BODY_CS, 0);
    gpio_set_direction(PIN_BODY_CS, GPIO_MODE_OUTPUT);
    return lines_held();
}

/* Les EN mis en sortie sans jamais sortir ni être tirés vers « allumé » (ticket #574) : du reset jusqu'à la demande
 * de la SESSION (bsk_phy_rail), aucune écriture de ce firmware ne va vers « allumé ». Pourquoi c'est vrai :
 *   - au reset, la sortie de GPIO7 et GPIO8 du C3 est coupée (DS C3, table 2-1) : R1 et R4 tiennent les /EN hauts,
 *     rails coupés. Ouvrir une sortie sort ce que contient le registre de sortie du GPIO, que rien ici n'a écrit : sur
 *     le C3, 0 est « allumé ». D'où le niveau écrit avant que la sortie ne soit ouverte ;
 *   - gpio_config (ESP-IDF 5.3.2, lu dans ses sources) ouvre la sortie, pose les tirages, puis seulement choisit la
 *     fonction GPIO de la broche, et n'écrit jamais le niveau. D'où trois temps : gpio_config en mode DISABLE (la
 *     sortie fermée, le tirage vers « coupé », la fonction GPIO) ; le niveau « coupé » écrit, sortie toujours fermée ;
 *     la sortie ouverte (gpio_set_direction : l'entrée et le drain ouvert fermés, la sortie ouverte, rien d'autre).
 *     Ouverte, la broche sort le niveau déjà écrit ;
 *   - le tirage vers « coupé » (haut sur le C3) va dans le sens de R1 et R4 : il ne les combat pas ;
 *   - avant bsk_phy_init, rien ne touche les EN : le bootloader et le démarrage d'ESP-IDF 5.3.2 ne nomment ni GPIO7 ni
 *     GPIO8 du C3 (lu dans leurs sources ; le bootloader ne configure de broches que pour une console UART ou les
 *     options BOOTLOADER_FACTORY_RESET et BOOTLOADER_APP_TEST, aucune posée), et rien n'est appelé avant bsk_phy_init
 *     dans app_main (main/main.c). Non vérifié sur la carte.
 * Ce que la puce fait seule à la mise sous tension n'en relève pas : le créneau bas de 5 ns de GPIO7 (MTDO) est du
 * silicium, avant tout code, et l'humain l'a jugé sans effet. */
static void en_init(void)
{
    const gpio_config_t en = {
        .pin_bit_mask = (1ULL << PIN_EN_LOGIC) | (1ULL << PIN_EN_MOTOR),
        .mode = GPIO_MODE_DISABLE,
        .pull_up_en = EN_ACTIVE_LOW ? GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE,
        .pull_down_en = EN_ACTIVE_LOW ? GPIO_PULLDOWN_DISABLE : GPIO_PULLDOWN_ENABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&en));
    gpio_set_level(PIN_EN_LOGIC, EN_OFF);
    gpio_set_level(PIN_EN_MOTOR, EN_OFF);
    ESP_ERROR_CHECK(gpio_set_direction(PIN_EN_LOGIC, GPIO_MODE_OUTPUT));
    ESP_ERROR_CHECK(gpio_set_direction(PIN_EN_MOTOR, GPIO_MODE_OUTPUT));
}

/* Un FAULT armé : son niveau lu d'abord, puis son interruption sur les deux fronts. Actif, c'est une activation, rail
 * coupé (rien ne l'a encore allumé), rapportée par la tâche de PHY, pas encore créée : seule productrice de la file des
 * événements, elle n'y entre pas en concurrence. Un front entre la lecture et l'armement, quelques microsecondes, rails
 * coupés, ne serait pas vu ; le prochain l'est, mais fault_edge le jugerait par rapport au niveau lu avant. Non vérifié
 * sur la carte. */
static void fault_arm(bsk_rail_t rail, gpio_num_t pin)
{
    if (gpio_get_level(pin) == 0) {
        fault_raw_t r = {.k = fault_edge(&fault[rail], true), .rail = rail, .on = false, .t_us = esp_timer_get_time()};
        on_fault(&r);
    }
    ESP_ERROR_CHECK(gpio_set_intr_type(pin, GPIO_INTR_ANYEDGE));
    ESP_ERROR_CHECK(gpio_isr_handler_add(pin, fault_isr, (void *)(uintptr_t)rail));
    ESP_ERROR_CHECK(gpio_intr_enable(pin));
}

void bsk_phy_init(void)
{
    const gpio_config_t lines = {   /* au repos (rest) : TXD et RXD ne sont pas routées, VD arrêtée */
        .pin_bit_mask = (1ULL << PIN_LENS_TXD) | (1ULL << PIN_LENS_RXD) | (1ULL << PIN_BODY_CS) | (1ULL << PIN_BODY_VD),
        .mode = GPIO_MODE_INPUT,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
    };
    const gpio_config_t detect = {   /* la retombée (front montant, tirage haut) par interruption */
        .pin_bit_mask = 1ULL << PIN_LENS_DETECT,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .intr_type = GPIO_INTR_POSEDGE,
    };
    const gpio_config_t cs = {
        .pin_bit_mask = 1ULL << PIN_LENS_CS,
        .mode = GPIO_MODE_INPUT,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_ANYEDGE,
    };
    /* Les FAULT# : entrées, jamais sorties, tirées haut comme R3 et R6 et comme après le reset (DS C3, table 2-1 : le
     * tirage haut de GPIO9, celui de GPIO6 selon l'eFuse du JTAG) ; aucun tirage bas. GPIO9 est la broche de strapping
     * du mode de démarrage : rien ici ne la tire vers 0. L'interruption est armée par fault_arm, après la lecture du
     * niveau. */
    const gpio_config_t faults = {
        .pin_bit_mask = (1ULL << PIN_FAULT_MOTOR) | (1ULL << PIN_FAULT_LOGIC),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    const uart_config_t ucfg = {
        .baud_rate = BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    /* rien n'est émis : lignes au repos, rails coupés */
    ESP_ERROR_CHECK(gpio_config(&lines));
    en_init();
    ESP_ERROR_CHECK(gpio_config(&detect));
    ESP_ERROR_CHECK(gpio_config(&cs));
    ESP_ERROR_CHECK(gpio_config(&faults));

    MUST((raw_q = xQueueCreate(RAW_LEN, sizeof(raw_t))) != NULL);
    MUST((fault_q = xQueueCreate(FAULT_LEN, sizeof(fault_raw_t))) != NULL);
    MUST((ev_q = xQueueCreate(EV_LEN, sizeof(bsk_phy_event_t))) != NULL);
    MUST((set = xQueueCreateSet(RAW_LEN + FAULT_LEN + UART_EV_LEN)) != NULL);
    MUST(xQueueAddToSet(raw_q, set) == pdPASS);
    MUST(xQueueAddToSet(fault_q, set) == pdPASS);

    ESP_ERROR_CHECK(uart_param_config(LENS_UART, &ucfg));
    /* ESP_INTR_FLAG_IRAM, avec CONFIG_UART_ISR_IN_IRAM (sdkconfig.defaults) : l'ISR de l'UART vide le FIFO de
     * réception pendant une écriture de la marque en NVS, cache de la flash coupé (ticket #528, audit M3) */
    ESP_ERROR_CHECK(uart_driver_install(LENS_UART, UART_RX_BUF, UART_TX_BUF, UART_EV_LEN, &uart_q, ESP_INTR_FLAG_IRAM));
    MUST(xQueueAddToSet(uart_q, set) == pdPASS);   /* vide : les broches ne sont pas routées */
    /* TXD et RXD au repos jusqu'à bsk_phy_lines(true) : l'entrée de l'UART sur le 1 constant, la fenêtre d'écoute fermée
     * (rx_gen à zéro) */
    portENTER_CRITICAL(&pres_mux);
    rxd_rest();
    portEXIT_CRITICAL(&pres_mux);
    ESP_ERROR_CHECK(uart_set_rx_full_threshold(LENS_UART, 8));
    ESP_ERROR_CHECK(uart_set_rx_timeout(LENS_UART, 2));

    lens_cs = gpio_get_level(PIN_LENS_CS) != 0;
    /* Avant l'interruption qui l'arme. ESP_TIMER_ISR exige CONFIG_ESP_TIMER_SUPPORTS_ISR_DISPATCH_METHOD
     * (sdkconfig.defaults). Pourquoi un esp_timer : son interruption est allouée avec ESP_INTR_FLAG_IRAM et, l'option
     * posée, son traitement des rappels en interruption est en IRAM, comme esp_timer_start_once (lu dans les sources
     * d'ESP-IDF 5.3.2, non vérifié sur la carte). Il compte le temps réel, ni en VD ni en tick FreeRTOS. */
    {
        const esp_timer_create_args_t drop = {
            .callback = drop_expire,
            .dispatch_method = ESP_TIMER_ISR,
            .name = "xdetect",
        };
        ESP_ERROR_CHECK(esp_timer_create(&drop, &drop_timer));
    }
    /* ESP_INTR_FLAG_IRAM : les interruptions tournent en IRAM, cache de la flash coupé compris (une écriture de la
     * marque en NVS) ; un front n'est pas perdu pendant une écriture en flash */
    ESP_ERROR_CHECK(gpio_install_isr_service(ESP_INTR_FLAG_IRAM));
    ESP_ERROR_CHECK(gpio_isr_handler_add(PIN_LENS_CS, cs_isr, NULL));
    ESP_ERROR_CHECK(gpio_isr_handler_add(PIN_BODY_VD, vd_isr, NULL));
    /* le verrou est posé (pres à zéro, « absent ») avant que l'interruption ne soit armée ; une insertion se confirme
     * par la tâche, 300 ms après */
    ESP_ERROR_CHECK(gpio_isr_handler_add(PIN_LENS_DETECT, detect_isr, NULL));
    fault_arm(BSK_RAIL_MOTOR, PIN_FAULT_MOTOR);
    fault_arm(BSK_RAIL_LOGIC, PIN_FAULT_LOGIC);

    /* le cœur 0, comme toutes les tâches (décision de l'humain) : la carte v1.0 est un ESP32-C3, à un seul cœur (le
     * cœur 0). */
    MUST(xTaskCreatePinnedToCore(phy_task, "phy", 4096, NULL, 12, NULL, 0) == pdPASS);
}
