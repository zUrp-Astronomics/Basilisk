/* SOURCE: composant phy — les broches de la carte, une table par cible ESP-IDF
 * AUTHOR: engineer
 * DATE: 2026-10-10
 * STATUS: actif — inclus par components/phy/phy.c et components/led/led_board.c ; ticket #574 : la table de la carte
 *         v1.0 (ESP32-C3), la polarité des EN dans chaque table
 *
 * La table est choisie par la cible du build (CONFIG_IDF_TARGET_*), pas par une option : une carte est un brochage.
 * Deux tables : la XIAO ESP32-S3 du banc, la carte v1.0 (ESP32-C3FH4 nu, pas une XIAO). Une cible sans table ne
 * compile pas. Chaque table déclare la polarité de ses EN (EN_ACTIVE_LOW) et de sa LED (LED_ACTIVE_LOW), sans valeur
 * par défaut hors des tables : une polarité fait partie du brochage. Elles sont employées dans des expressions C, pas
 * dans des #if : une table qui en oublierait une ne compilerait pas. */
#ifndef PINS_H
#define PINS_H

#include "sdkconfig.h"

#if CONFIG_IDF_TARGET_ESP32S3
/* Seeed XIAO ESP32S3 */
#define PIN_EN_LOGIC    GPIO_NUM_1   /* D0  : rail 3,3 V de l'objectif (optionnel) */
#define PIN_EN_MOTOR    GPIO_NUM_2   /* D1  : rail moteur 5 V (optionnel) */
#define EN_ACTIVE_LOW   0            /* ... rails allumés à l'état haut */
#define PIN_LENS_DETECT GPIO_NUM_3   /* D2  : XDETECT, tirage haut, 0 = présent */
#define PIN_BODY_VD     GPIO_NUM_4   /* D3  : VD, sortie LEDC */
#define PIN_LENS_CS     GPIO_NUM_44  /* D7  : LENS_CS, entrée */
#define PIN_LENS_TXD    GPIO_NUM_7   /* D8  : UART1 TX, carte -> objectif */
#define PIN_LENS_RXD    GPIO_NUM_8   /* D9  : UART1 RX, objectif -> carte */
#define PIN_BODY_CS     GPIO_NUM_9   /* D10 : BODY_CS, sortie */
#define LENS_UART       UART_NUM_1
#define PIN_LED         GPIO_NUM_21  /* la LED utilisateur de la XIAO */
#define LED_ACTIVE_LOW  1            /* ... allumée à l'état bas */
#elif CONFIG_IDF_TARGET_ESP32C3
/* Carte Basilisk v1.0 : ESP32-C3FH4 nu (4 Mo de flash intégrée), relevé du schéma v1.0 recoupé avec la netlist du
 * Gerber (1_Board/). Aucun GPIO libre : GPIO0 à 10 et 18 à 21 sont tous câblés ; 11 à 17 sont VDD_SPI et la flash
 * intégrée. Ce que chaque broche a de particulier, et pourquoi c'est sans objet pour le firmware :
 *   - GPIO0, GPIO1 : les broches du quartz de 32 kHz, qui n'est pas monté ; l'horloge RTC reste l'oscillateur RC
 *     interne (sdkconfig.defaults.esp32c3), elles servent de GPIO ;
 *   - GPIO2 (RXD) : broche de strapping, sans tirage sur la carte. Elle n'est lue qu'au reset, avant le firmware, et ne
 *     décide pas du mode de démarrage (DS C3, table 3-3) ; le firmware n'en fait qu'une entrée de l'UART ;
 *   - GPIO4 à GPIO7 : les broches du JTAG sur les pastilles (MTMS, MTDI, MTCK, MTDO) ; gpio_config leur donne la
 *     fonction GPIO (GPIO6, jamais configurée, garde son état de reset) ;
 *   - GPIO7 (EN_MOTOR) : un créneau bas de 5 ns à la mise sous tension, fait du silicium, avant tout firmware ; jugé
 *     sans effet par l'humain (un commutateur d'alimentation ne s'allume pas en 5 ns) ;
 *   - GPIO8 (EN_LOGIC) : broche de strapping, tenue haute par R4 au reset ; « coupé », le niveau que le firmware y
 *     écrit, est ce même niveau haut ;
 *   - GPIO9 (FAULT_LOGIC) : la broche de strapping du mode de démarrage, tirée haut par R6 : voulu (l'humain). Le
 *     firmware ne la configure pas ;
 *   - GPIO18, GPIO19 : l'USB Serial/JTAG, la ligne de PROTOCOL.md ;
 *   - GPIO20, GPIO21 : SCL et SDA du capteur SHT40 (U0RXD, U0TXD), absents de cette table : le firmware n'y touche pas.
 * Les deux FAULT (FAULT# des TPS2552, drain ouvert, tirés haut par R3 et R6, actifs à 0) sont déclarés ici et ne sont
 * pas lus : aucun code ne configure ces broches, qui gardent leur état de reset (entrées). */
#define PIN_LENS_DETECT GPIO_NUM_0   /* XDetect : R17 au 3,3 V, 0 = présent */
#define PIN_LENS_TXD    GPIO_NUM_1   /* TXD : UART1 TX, carte -> objectif (R14 série, contact 8) */
#define PIN_LENS_RXD    GPIO_NUM_2   /* RXD : UART1 RX, objectif -> carte (R15 série, contact 7) */
#define PIN_LENS_CS     GPIO_NUM_3   /* Lens_CS : entrée, R13 à la masse */
#define PIN_BODY_VD     GPIO_NUM_4   /* Body_VD : VD, sortie LEDC */
#define PIN_BODY_CS     GPIO_NUM_5   /* Body_CS : sortie, R16 à la masse */
#define PIN_FAULT_MOTOR GPIO_NUM_6   /* Fault_Motor : FAULT# de U2, rail moteur, actif à 0 (non lu) */
#define PIN_EN_MOTOR    GPIO_NUM_7   /* EN_Motor : /EN de U2, rail moteur 5 V, R1 au 3,3 V */
#define PIN_EN_LOGIC    GPIO_NUM_8   /* EN_Logic : /EN de U3, rail logique 3,3 V, R4 au 3,3 V */
#define EN_ACTIVE_LOW   1            /* ... rails allumés à l'état bas (« Enable when EN=0 ») */
#define PIN_FAULT_LOGIC GPIO_NUM_9   /* Fault_Logic : FAULT# de U3, rail logique, actif à 0 (non lu) */
#define LENS_UART       UART_NUM_1
#define PIN_LED         GPIO_NUM_10  /* LED1 et LED2, chacune par 220 Ω vers la masse */
#define LED_ACTIVE_LOW  0            /* ... allumées à l'état haut */
#else
#error "firmware2 : aucune table de broches pour cette cible ESP-IDF : ajouter la sienne dans components/phy/pins.h"
#endif

#endif
