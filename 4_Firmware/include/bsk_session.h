/* SOURCE: spec de l'atelier § 3.1, § 4.1 et § 4.2 — SESSION
 * AUTHOR: engineer
 * DATE: 2026-10-04
 * STATUS: actif — en-tête partagé, déclarations seulement ; implémenté par components/session ; ticket #689 : `now_us` de
 *         bsk_session_step, l'instant réel des actions de la carte ; ticket #691 : bsk_session_real, l'instant réel d'une
 *         commande
 *
 * SESSION possède l'alimentation de l'objectif, la validité de la position et la politique de reprise. Elle lit PHY
 * (bsk_phy_poll), commande TRANSACTION (bsk_txn.h) et publie l'instantané (bsk_status_t, bsk_contract.h).
 *
 * Les tâches : SESSION n'attend rien d'elle-même, l'appelant la fait avancer. L'émission d'une trame la retient, sur la
 * carte (l'émission de PHY, bsk_phy.h, components/phy/phy.c) : l'écriture de ses octets vers l'UART, sans échéance, puis
 * l'attente de sa fin, 100 ms au plus, mesurée dans DEBUG (tx_wait_us, tx_timeout). Sur la carte, app_main l'appelle en
 * boucle avec esp_timer_get_time() ; en simulation, le test l'appelle aux instants où PHY ou SESSION ont quelque chose à faire
 * (bsk_session_next). bsk_session_command s'appelle sur la même tâche que bsk_session_step. */
#ifndef BSK_SESSION_H
#define BSK_SESSION_H

#include <stdbool.h>
#include <stdint.h>

#include "bsk_contract.h"
#include "bsk_store.h"

/* La position de reprise (PROTOCOL.md § 7) : la cible du déplacement en vol, sinon la dernière position lue, en READY,
 * avec la clé de la marque de l'objectif et de la focale (mark.c) et le sens de l'arrivée. Une reprise (une perte, ou un
 * redémarrage de la carte par chien de garde ou panic) y revient au lieu de la marque, pour la même clé. Sa mémoire est à
 * l'appelant, pour survivre à un redémarrage de la carte (RTC_NOINIT, main/main.c) : seule la SESSION (restore.c) en lit
 * et en écrit les champs. */
typedef struct {
    char     key[BSK_STORE_KEY_MAX];   /* "" : aucune position */
    uint16_t pos;
    uint8_t  dir;                      /* bsk_approach_t */
    uint32_t sum;                      /* la somme des trois champs (restore.c) : une écriture interrompue ne se relit pas */
} bsk_session_keep_t;

/* Les échéances, en millisecondes. */
typedef struct {
    uint32_t handshake_ms;   /* poignée de main, chaque attente de LENS_CS */
    uint32_t init_ms;        /* 0x01 (chacun de ses trois essais), 0x07, 0x0B, 0x09, 0x0D */
    uint32_t name_ms;        /* 0x3F */
    uint32_t info_ms;        /* 0x08, 0x0A (celui de l'init et celui du reset soft) */
    uint32_t init_home_ms;   /* 0x10 de l'init */
    uint32_t home_ms;        /* 0x10 du homing du pilote */
    uint32_t still_ms;       /* immobilité après le homing du pilote */
    uint32_t first05_ms;     /* premier 0x05 après l'init, sans échec */
    uint32_t probe05_ms;     /* 0x05 de la sonde standard */
    uint32_t retry_wait_ms;  /* attente entre deux essais, puis POWERING : la reprise ne coupe pas les rails */
    uint32_t lost_ms;        /* perte : READY sans 0x05 ni 0x06 ; un seul des deux arrêté, ses données invalides */
    bsk_session_keep_t *keep;  /* la mémoire de la position de reprise ; NULL : une mémoire de la SESSION */
    bool     warm;             /* ce démarrage de la carte est une reprise (main.c : chien de garde, panic) : la position
                                  de keep est relue au bsk_session_init ; faux (défaut), elle y est effacée sans être lue */
} bsk_session_params_t;

void bsk_session_params_default(bsk_session_params_t *p);

/* Session en OFF, rien d'émis, compteur d'échecs à zéro ; la position de reprise relue de p->keep si p->warm, effacée
 * sinon. PHY doit être prête avant (bsk_phy_init, ou phy_sim_init en simulation). */
void bsk_session_init(const bsk_session_params_t *p);

/* Traite ce que PHY a remonté, au plus 64 événements, puis ce qui est échu, jusqu'à `now_us`. Au-delà de 64, le reste
 * attend le pas suivant, et ce qui est échu après le dernier événement traité avec lui. `now_us` est l'instant réel du
 * début du pas : un délai minimal qui suit une action de la carte sur l'objectif (un rail, BODY_CS, le 0x1C du homing) se
 * compte depuis lui, ou depuis la date de l'événement traité si elle est plus tardive, même quand le pas est en retard sur
 * l'échéance qui l'a causée, et les fenêtres de la poignée de main aussi, depuis le mouvement réel de BODY_CS ; une
 * attente de l'objectif se juge sur la date de ses événements. */
void bsk_session_step(uint64_t now_us);

/* L'instant réel des commandes déposées ensuite, jusqu'au prochain pas (bsk_session_command) : à appeler entre deux pas,
 * sur la même tâche, juste avant de déposer. Une commande qui agit sur l'objectif (CMD_ATTACH : le rail logique) compte les
 * délais qui suivent son action depuis lui, pas depuis le début du dernier pas. Sans cet appel, la commande agit à
 * l'instant du dernier pas. */
void bsk_session_real(uint64_t now_us);

/* Prochain instant où SESSION ou TRANSACTION a quelque chose à faire sans événement de PHY ;
 * UINT64_MAX s'il n'y en a pas. */
uint64_t bsk_session_next(void);

void bsk_session_status(bsk_status_t *st);

/* La boîte de commandes. La commande est traitée tout de suite : les délais qui suivent ce qu'elle fait sur l'objectif se
 * comptent depuis son instant réel (bsk_session_real), le reste de la session depuis le dernier bsk_session_step. Elle
 * rend ses accusés par bsk_session_ack : ACK_REJECTED seul (immédiat et final), ou ACK_ACCEPTED puis ACK_COMPLETED ou
 * ACK_FAILED, tout de suite ou plus tard.
 *   - une seule commande en vol, de son ACK_ACCEPTED à son accusé final : seules CMD_FOCUS_GOTO et CMD_FOCUS_MOVE
 *     (jusqu'à ARRIVED, STALLED, ABORTED) et CMD_LENS_CUSTOM (jusqu'à la réponse de l'objectif) restent en vol ; les
 *     autres finissent dans l'appel, sauf CMD_SET_MARK et CMD_CLEAR_MARK (ci-dessous). Toute commande déposée pendant un
 *     vol reçoit E_BUSY, sauf CMD_FOCUS_STOP ;
 *   - CMD_SET_MARK et CMD_CLEAR_MARK finissent quand le magasin a rendu leur écriture (bsk_store.h), au pas qui la relève,
 *     même la session quittée entre-temps : ACK_COMPLETED rangée, la marque alors publiée ; ACK_FAILED E_STORE ratée, ou
 *     pas rendue BSK_STORE_WRITE_MS après le dépôt (le pas qui atteint l'échéance). Rendue dans l'appel (le magasin
 *     des tests), elles y finissent. Tant que l'écriture attend son résultat, échéance passée comprise, une autre
 *     CMD_SET_MARK ou CMD_CLEAR_MARK, et l'appui long du bouton, reçoivent E_BUSY ; aucune autre commande n'est exclue ;
 *   - CMD_FOCUS_STOP finit par l'émission de son 0x1C, en READY et en RESTORING : ACK_COMPLETED parti, ACK_FAILED sinon.
 *     Déposée entre le 0x03 et le 0x04 de la boucle, son 0x1C est retenu jusqu'au 0x04 (bsk_txn.h) : elle finit au pas qui
 *     le fait partir, ou à l'arrêt de la boucle avant lui (E_ABORTED), sans être en vol (aucune commande n'en reçoit
 *     E_BUSY) ;
 *   - CMD_LENS_CUSTOM émet une requête au Samyang AF 135 en READY (E_NOCAP à tout autre objectif) ; finie par
 *     ACK_COMPLETED à sa réponse, ou ACK_FAILED : son échéance (E_TIMEOUT), sa réponse trop courte pour ses données
 *     (E_FRAMING), la sortie de READY (E_ABORTED) ; une autre 0x40 ne la finit pas (bsk_txn.h). Un refus de bench_core
 *     la refuse (E_FORBIDDEN), rien d'émis ; après une échéance, elle est refusée (E_DESYNC), rien d'émis, jusqu'à la
 *     coupure des rails (components/session/cmd.c) ;
 *   - CMD_HOME et CMD_RING_SET ne sont jamais déposées (aucune ligne de HOTE) : sans accusé ;
 *   - CMD_APERTURE_SET (arg : un code d'ouverture, unités objectif) est servie en READY, bornée à la plage que l'objectif
 *     publie (aperture_min, aperture_max), posée pour le prochain 0x03 ; sans plage, E_NOCAP ;
 *   - CMD_ATTACH finit tout de suite : la session publie l'issue du démarrage dans session_state et last_error.
 * La file garde 8 accusés ; l'hôte les lit après chaque dépôt (il attend le premier), si bien qu'il n'y en a jamais plus
 * de quatre : les deux de la commande, l'accusé final de celle en vol, et celui d'un mouvement qu'un CMD_FOCUS_STOP
 * arrête. Au-delà, le plus ancien serait perdu. */
void bsk_session_command(const bsk_cmd_t *c);

/* Ce que la carte compte pour `DEBUG` (PROTOCOL.md § 3) : des mesures ; compter n'y change rien. À zéro au démarrage de
 * la carte (bsk_phy_init, bsk_session_init). La SESSION les remonte de la PHY et de TRANSACTION, dont HOTE ne dépend
 * pas. */
typedef struct {
    uint32_t evq_drop;       /* PHY : les événements jetés, sa file pleine (bsk_phy_stats) */
    uint32_t ev_back;        /* SESSION : les événements de PHY reçus datés avant le précédent */
    uint32_t ev_back_us;     /*   le plus grand écart vu, en µs */
    uint32_t tx_wait_us;     /* PHY : la plus longue attente de fin d'émission, en µs (bsk_phy_stats) */
    uint32_t tx_timeout;     /*   ses échéances de 100 ms atteintes */
    uint32_t pair_late;      /* TRANSACTION : les paires jetées, une de leurs trames plus de 2 ms après son échéance */
    uint32_t pair_late_us;   /*   le plus grand retard constaté en les jetant, en µs (bsk_txn_late) */
    uint32_t fault_motor;    /* PHY : les activations du FAULT du rail moteur (bsk_phy_stats) */
    uint32_t fault_logic;    /*   et du rail logique */
    bsk_store_nvs_t nvs;     /* le magasin : l'état de la NVS depuis le démarrage de la carte (bsk_store_nvs) ; un état,
                                pas un compte */
} bsk_session_debug_t;

void bsk_session_debug(bsk_session_debug_t *d);

/* Le prochain accusé, dans l'ordre ; false s'il n'y en a pas. */
bool bsk_session_ack(bsk_ack_t *a);

/* Les 16 octets de données de la réponse de l'objectif à la dernière CMD_LENS_CUSTOM finie par ACK_COMPLETED, tels qu'il
 * les a rendus (rien n'y est interprété). */
#define BSK_CUSTOM_DATA 16u
void bsk_session_custom_data(uint8_t data[BSK_CUSTOM_DATA]);

#endif
