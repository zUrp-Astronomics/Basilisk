/* SOURCE: spec de l'atelier § 4.1 et § 4.2 — la boîte de commandes et le bouton du fût
 * AUTHOR: engineer
 * DATE: 2026-10-03
 * STATUS: actif — en-tête privé de la SESSION, inclus par session.c et cmd.c seuls
 *
 * bsk_session_command (bsk_session.h) admet ou refuse chaque commande d'après l'état de la session, rend ses accusés
 * dans la file du superviseur, délègue GOTO, MOVE et STOP à MOUVEMENT, la marque à mark.c, le rôle de la bague à ring.c,
 * et demande au superviseur les transitions (ci-dessous). Le bouton du fût dépose son goto par la même boîte. */
#ifndef BSK_SESSION_CMD_H
#define BSK_SESSION_CMD_H

#include <stdbool.h>
#include <stdint.h>

#include "bsk_session.h"

/* ─────────────────────────── fournis par session.c ─────────────────────────── */

bsk_session_state_t session_state(void);
bsk_err_t session_last_error(void);
uint64_t session_now(void);

/* La consigne d'ouverture posée telle quelle dans le 0x03 de la boucle, tenue pour chaque paire suivante. */
void session_aperture_hold(uint16_t code);

/* Le dernier accusé déposé dans la file, qui en a au moins un. */
const bsk_ack_t *session_ack_last(void);

/* Les transitions que demande la boîte : CMD_ATTACH (OFF n'est plus tenu, le compteur d'échecs à zéro, POWERING si D2 est
 * présent), CMD_DETACH (OFF, tenu), CMD_CLEAR_FAULT (OFF non tenu, depuis FAULT seulement), et q en RESTORING (READY). */
void session_attach(void);
void session_detach(void);
void session_clear_fault(void);
void session_restore_stop(void);

/* ─────────────────────────── appelés par session.c ─────────────────────────── */

/* Un 0x05 reçu à t (µs), après MOUVEMENT : le relâchement du bouton du fût, décidé par mark_button, et ce qu'il fait. */
void cmd_button(uint64_t t);

/* CMD_LENS_CUSTOM en vol, la requête finie : son accusé final, la requête lue (bsk_txn_ack). Vrai si elle l'était. À
 * n'appeler qu'en READY, où aucune autre requête n'est en vol. */
bool cmd_custom_poll(void);

/* Avec la session, après l'arrêt de la boucle (bsk_txn_loop(false)) : CMD_LENS_CUSTOM en vol finie par ACK_FAILED,
 * E_ABORTED ; émise et sans réponse, elle désynchronise le canal (cmd.c, cu). Un geste de marque en vol ne l'est pas : son
 * écriture est dans la file du magasin, son résultat le finit (cmd_mark_poll). */
void cmd_forget(void);

/* Au bsk_session_init, avant l'oubli de la session : aucun geste de marque ni CMD_LENS_CUSTOM en vol, CMD_LENS_CUSTOM
 * désynchronisée levée (le reset de la carte coupe les rails, phy.c). */
void cmd_init(void);

/* Les deux rails coupés (to_off) : CMD_LENS_CUSTOM désynchronisée levée (cmd.c). Appelée là seulement : l'oubli de la
 * session sans coupure (la reprise) ne la lève pas. */
void cmd_rails_cut(void);

/* À chaque pas, après mark_poll, dans tout état : le geste de marque en vol fini par le résultat de son écriture, ou à son
 * échéance (BSK_STORE_WRITE_MS après son dépôt) atteinte à `now` ; ACK_COMPLETED rangé, ACK_FAILED E_STORE sinon ; pour
 * l'appui long, sa ligne `* btn`. */
void cmd_mark_poll(uint64_t now);

/* L'échéance du geste de marque en vol, UINT64_MAX sans lui (bsk_session_next). */
uint64_t cmd_mark_due(void);

#endif
