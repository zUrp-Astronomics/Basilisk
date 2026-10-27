/* SOURCE: spec de l'atelier § 3.2 — MOUVEMENT
 * AUTHOR: engineer
 * DATE: 2026-10-04
 * STATUS: actif — en-tête privé de la SESSION, inclus par session.c, motion.c, cmd.c et restore.c
 *
 * MOUVEMENT : ses états (bsk_motion_state_t), le démarrage constaté, l'immobilité, STALLED, ABORTED, et la commande de
 * mouvement en vol, tenue jusqu'à son accusé final. Il lit lens_rx.h, ne décode aucune trame, et émet par drive.h. */

/* Ordre d'appels : la boîte de commandes (cmd.c) garde l'admission (une commande en vol, q passe toujours) et n'appelle
 * motion_command et motion_stop qu'en READY (motion_stop aussi en RESTORING) ; le superviseur (session.c) signale la
 * sortie de READY par motion_leave. */
#ifndef BSK_MOTION_H
#define BSK_MOTION_H

#include <stdbool.h>
#include <stdint.h>

#include "bsk_session.h"

/* Fourni par session.c : la file des accusés (bsk_session_ack), où MOUVEMENT rend ceux de ses commandes. */
void session_ack(uint32_t seq, bsk_ack_result_t r, bsk_err_t why);

/* Tout à zéro, rien en vol : au bsk_session_init, avant le premier motion_forget. */
void motion_init(void);

/* Une nouvelle session : IDLE (journalisé s'il change), plus d'échéance, la consigne et le bit AF oubliés (drive_forget).
 * La commande en vol a été close avant, la boucle arrêtée. */
void motion_forget(void);

/* L'état publié ; une commande de mouvement en vol, jusqu'à son accusé final. */
bsk_motion_state_t motion_state(void);
bool motion_busy(void);

/* Un mouvement en cours : COMMANDED, MOVING ou SETTLING, la seule liste (PLANNING n'est jamais publié, mv_start). Publié
 * dans l'instantané (bsk_status_t, moving), où la couche HOTE le lit pour `e`. */
bool motion_moving(void);

/* La commande de mouvement en vol (motion_busy) : vrai, sa cible et la position lue à son dépôt, pour la position de
 * reprise (restore.c) ; faux sans elle, rien d'écrit. */
bool motion_aim(uint16_t *target, uint16_t *from);

/* Les arrêts non confirmés d'un déplacement suivi, modulo 2^16 (bsk_status_t, stop_unconfirmed) : l'abandon du 0x1C qui
 * a fini une commande de mouvement en vol, par q (motion_stop, et un q suivant pendant sa surveillance) ou par STALLED
 * (motion_expire). Ni un q sans commande en vol, ni STALLED du retour à la marque. Jamais remis à zéro avec la session. */
uint16_t motion_unconfirmed(void);

/* L'admission d'une cible de goto, la seule : les bornes du 0x06 connues (lens_rx, lim) et la cible dedans, bornes
 * comprises. Consultée par motion_command pour chaque goto direct, et par restore.c pour la cible du retour avant de le
 * planifier (restore_plan). */
bool motion_admit(int64_t target);

/* READY, rien en vol : CMD_FOCUS_GOTO ou CMD_FOCUS_MOVE, refusée si motion_admit ne l'admet pas (E_LIMIT), sinon acceptée
 * et déposée, COMMANDED : son 0x1D part à la prochaine trame 0x04, ou, l'arrêt surveillé (motion_stop, motion_expire), une
 * fois la surveillance finie, l'arrêt confirmé ou son 0x1C abandonné ; d'ici là, aucune échéance du goto ne court. */
void motion_command(const bsk_cmd_t *c, uint64_t now);

/* RESTORING : un goto que la carte fait elle-même, la cible calculée par restore.c, déposé comme celui d'une commande
 * (COMMANDED … ARRIVED, STALLED), sans commande en vol ni accusé. restore.c lit sa fin dans motion_state. Pas d'admission
 * ici : la cible du retour a été admise au départ (motion_admit, restore_plan), le dépassement est borné par la recette,
 * et des bornes changées en route abandonnent le retour avant le goto suivant (restore.c, changed). */
void motion_restore(uint16_t target, uint64_t now);

/* READY, RESTORING : le 0x1C, à now (pour CMD_FOCUS_STOP, motion_q ; l'abandon du retour à la marque, restore.c) ; un
 * mouvement en cours est ABORTED, que le 0x1C soit parti ou non.
 * Rend le résultat de drive_stop. E_OK (parti, ou retenu jusqu'au 0x04 : bsk_txn.h), l'arrêt est surveillé (motion.c) :
 * sans accusé 1C ni immobilité en 1,5 s, le même 0x1C, trois envois au plus ; la session oubliée l'éteint. Un goto déposé
 * pendant elle attend sa fin (motion_command) : aucun 0x1C ne part après son 0x1D. */
bsk_err_t motion_stop(uint64_t now);

/* CMD_FOCUS_STOP (seq, déjà ACK_ACCEPTED) en READY ou en RESTORING : motion_stop, puis l'accusé final de q, l'issue du
 * premier envoi du 0x1C : ACK_COMPLETED parti, ACK_FAILED sinon (l'échec de son émission ; E_BUSY, la file des retenues
 * pleine), l'arrêt surveillé s'il est parti. Retenu entre le 0x03 et le 0x04 (bsk_txn.h), l'accusé final et la
 * surveillance attendent son départ (motion_q_end). */
void motion_q(uint32_t seq, uint64_t now);

/* Après chaque bsk_txn_tick, à son instant : chaque q dont le 0x1C retenu est parti, ou pas, reçoit son accusé final, comme
 * s'il l'avait été au dépôt (motion_q), et l'arrêt est surveillé depuis `now` s'il est parti ; pas parti, un goto déposé
 * après lui, qui attendait, part. Aussi à l'oubli de la session
 * (motion_forget), la boucle arrêtée : le 0x1C retenu n'est pas parti, ACK_FAILED, E_ABORTED. */
void motion_q_end(uint64_t now);

/* Sortie de READY : la commande en vol, s'il y en a une, rend ACK_FAILED avec why (E_ABORTED ou E_LOST). */
void motion_leave(bsk_err_t why);

/* Une trame reçue, décodée par lens_rx : son type ; now, l'instant de la SESSION. */
void motion_frame(uint8_t type, uint64_t now);

/* L'échéance du mouvement (UINT64_MAX hors mouvement, et pour un goto qui attend la fin de l'arrêt), ou de la surveillance
 * de l'arrêt ; échue à now : vrai, et le
 * mouvement est STALLED, le 0x1C émis et surveillé comme celui de motion_stop, ou, en COMMANDED avant son troisième envoi,
 * le même 0x1D renvoyé avec une nouvelle échéance ; ou, l'arrêt surveillé encore non confirmé, le même 0x1C, ou
 * l'abandon, qui fait partir le goto qui attendait. L'arrêt confirmé (accusé 1C ou immobilité) se juge à la réception d'un
 * 0x05 ou d'un 0x06, dans motion_frame, qui éteint la surveillance et fait partir ce goto : rien ici. */
uint64_t motion_due(void);
bool motion_expire(uint64_t now);

#endif
