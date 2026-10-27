/* SOURCE: spec de l'atelier § 3.2 — MOUVEMENT (voir motion.h)
 * AUTHOR: engineer
 * DATE: 2026-10-03
 * STATUS: actif — C pur, sans ESP-IDF : compilé aussi par les tests hôte
 *
 * Un goto sans fin explicite : démarrage constaté en 1 s (position du 0x06 ou octet de mouvement du 0x05), puis
 * l'immobilité en 10 s au plus, ou l'accusé 1D qui suit le 0x06. Aucune approche et aucune tolérance sur la position
 * finale : l'objectif borne lui-même la cible, la position où il s'arrête est publiée. */
#include "motion.h"

#include <string.h>

#include "bsk_journal.h"
#include "bsk_txn.h"
#include "drive.h"
#include "lens_rx.h"
#include "still.h"

#define MS             1000u
#define START_MS       1000u  /* démarrage d'un goto constaté */
#define MOVE_STILL_MS  10000u /* immobilité après un goto */
#define SENDS          3      /* décision de l'humain : trois envois du 0x1D (et du 0x1C) au total au plus */
#define STOP_MS        1500u  /* au-delà de la machine d'arrêt de 1,2 s du 135 (automate du 135, § 1.7.4) */

static struct {
    bsk_motion_state_t state;
    uint16_t target, pos0;
    uint16_t prev06;                  /* position du 0x06 précédent */
    bool     move_nz;                 /* l'octet de mouvement du 0x05 a été non nul depuis le dépôt */
    uint8_t  sends;                   /* les envois du 0x1D de ce goto */
    bool     wait;                    /* COMMANDED, le 0x1D pas encore déposé : il attend la fin de l'arrêt surveillé */
    uint64_t due;                     /* démarrage (1 s) ou immobilité (10 s) ; UINT64_MAX hors mouvement, et en attente */
    still_t  still;                   /* l'immobilité du mouvement, suivie à part de celle du homing */

    /* la commande en vol : CMD_FOCUS_GOTO ou CMD_FOCUS_MOVE */
    bool     busy;
    uint32_t busy_seq;

    /* la surveillance de l'arrêt, après le 0x1C de q ou de STALLED */
    uint8_t  stop_sends;              /* envois du 0x1C ; 0 : éteinte */
    bool     stop_tracked;            /* c'est l'arrêt d'un déplacement suivi (une commande en vol) ; lu allumée seulement */
    uint16_t unconfirmed;             /* les abandons de l'arrêt d'un déplacement suivi, jamais oubliés (motion_unconfirmed) */
    uint64_t stop_due;                /* UINT64_MAX éteinte */
    still_t  stop_still;              /* l'immobilité depuis le 0x1C, suivie à part */

    /* les q dont le 0x1C est retenu jusqu'au 0x04, par sa place dans la file de TRANSACTION : leur accusé final attend
     * son départ (motion_q_end) */
    bool     q_held[BSK_TXN_HELD_MAX];
    uint32_t q_seq[BSK_TXN_HELD_MAX];
} m;

/* La surveillance de l'arrêt, allumée (`sends` envois déjà faits, la fenêtre depuis `now`) ou éteinte (0). */
static void stop_watch(uint8_t sends, uint64_t now)
{
    m.stop_sends = sends;
    m.stop_due = sends ? now + (uint64_t)STOP_MS * MS : UINT64_MAX;
    m.stop_still.have = false;
}

/* Le mouvement n'est journalisé que s'il change : motion_forget le remet au repos à chaque session. Un goto n'attend qu'en
 * COMMANDED : ABORTED par q, ou la session oubliée, son 0x1D ne part plus. */
static void motion(bsk_motion_state_t st)
{
    if (m.state != st) bsk_journal_motion((uint8_t)st);
    m.state = st;
    m.wait = m.wait && st == MOTION_COMMANDED;
}

bool motion_moving(void) { return m.state == MOTION_COMMANDED || m.state == MOTION_MOVING || m.state == MOTION_SETTLING; }

/* L'accusé final de la commande en vol, s'il y en a une. */
static void cmd_end(bsk_ack_result_t r, bsk_err_t why)
{
    if (!m.busy) return;
    m.busy = false;
    session_ack(m.busy_seq, r, why);
}

static void mv_end(bsk_motion_state_t st, bsk_ack_result_t r, bsk_err_t why)
{
    motion(st);
    m.due = UINT64_MAX;
    cmd_end(r, why);
}

/* COMMANDED -> MOVING : l'immobilité est attendue 10 s au plus, et son suivi repart d'ici : ce qui a été suivi avant le
 * démarrage ne compte pas. */
static void mv_moving(uint64_t now)
{
    motion(MOTION_MOVING);
    m.due = now + (uint64_t)MOVE_STILL_MS * MS;
    m.still.have = false;
}

/* -> SETTLING : l'objectif a déclaré l'arrêt ; ARRIVED à l'immobilité. */
static void mv_settling(uint64_t due)
{
    motion(MOTION_SETTLING);
    m.due = due;
}

/* Le 0x1D de la cible, déposé pour la prochaine trame 0x04 : le démarrage attendu 1 s, suivi depuis la position lue. */
static void mv_send(uint64_t now)
{
    m.wait = false;
    drive_goto(m.target);
    m.sends = 1;
    m.pos0 = m.prev06 = lens_rx()->pos;
    m.move_nz = false;
    m.still.have = false;
    m.due = now + (uint64_t)START_MS * MS;
}

/* La surveillance finie, l'arrêt confirmé ou abandonné : le goto qui attendait part (mv_start). */
static void stop_end(uint64_t now)
{
    stop_watch(0, now);
    if (m.wait) mv_send(now);
}

/* PLANNING -> COMMANDED : la cible telle quelle. PLANNING n'est jamais publié : la recette d'approche du retour à la
 * marque est calculée par restore.c. Une cible égale à la position mène à ARRIVED dès l'immobilité, bien avant 1 s.
 * Ticket #787 : un arrêt surveillé (le 0x1C de q ou de STALLED, motion_stop, motion_expire) n'est peut-être pas fini dans
 * l'objectif. Le 135 reprend 0x1D et 0x1C au même traitement de trame, le 0x1D d'abord (samyang.md § 2.4, § 4.1) : le
 * 0x1C efface le goto que le 0x1D vient d'armer, ou le goto retarde l'arrêt jusqu'à l'arrêt forcé de 1,2 s (§ 4.5). Le
 * 0x1D attend donc la fin de la surveillance (stop_end) : l'arrêt confirmé, ou le 0x1C abandonné après son troisième
 * envoi, 4,5 s au plus. Jusque-là rien de ce goto n'est émis, ni échu (m.due, UINT64_MAX hors mouvement, le reste) : il
 * n'est ni renvoyé ni STALLED pour avoir attendu ; un 0x1C renvoyé ne peut pas l'évincer, il n'est pas parti. */
static void mv_start(uint16_t target, uint64_t now)
{
    motion(MOTION_COMMANDED);
    m.target = target;
    m.pos0 = lens_rx()->pos;          /* la position au dépôt (motion_aim), relue au départ du 0x1D */
    if (m.stop_sends) m.wait = true;
    else mv_send(now);
}

/* À chaque 0x05 ou 0x06 ; `acked` : un accusé 1D suit le 0x06 dans la trame.
 * Piège : un 0x1D évincé par un nouveau reçoit `1D 00` chez Samyang (automate du 135, § 1.7.4), qu'un goto en COMMANDED
 * lit « borné sur place ». Après un renvoi, cet accusé est donc celui de l'évincé, pas une arrivée : un 135 bloqué reçoit
 * le 0x1D sans bouger ni l'accuser, et le renvoi fabriquait une fausse arrivée. Un renvoi, c'est celui d'ici (m.sends), ou
 * le report par TRANSACTION d'un 0x1D dont le 0x04 a été tenté et a raté, peut-être parti quand même (drive_goto_failed) :
 * l'arrivée se juge alors sur l'état, la position égale à la cible et l'immobilité, ou c'est le renvoi, puis STALLED
 * (décision de l'humain du 2026-10-09). Un report sans émission tentée compte comme une émission unique. Une cible égale à
 * la position arrive toujours par l'immobilité ; une cible que l'objectif bornerait n'est pas émise (bornes du 0x06
 * resserrées). */
static void mv_frame(uint8_t type, bool acked, uint64_t now)
{
    const lens_rx_t *l = lens_rx();
    if (type == 0x05 && l->move != 0) m.move_nz = true;
    if (m.state == MOTION_COMMANDED) {
        if ((type == 0x06 && l->pos != m.pos0) || (type == 0x05 && l->move != 0)) mv_moving(now);
        else if (acked && m.sends == 1 && !drive_goto_failed())
            mv_settling(now + (uint64_t)MOVE_STILL_MS * MS);   /* bornée sur place : 1D 00 */
        else if (l->pos == m.target && still(&m.still, now)) mv_end(MOTION_ARRIVED, ACK_COMPLETED, E_OK);
    }
    if (m.state == MOTION_MOVING) {
        if (acked || (type == 0x05 && m.move_nz && l->move == 0) ||
            (type == 0x06 && !m.move_nz && l->pos == m.prev06))
            mv_settling(m.due);
    } else if (m.state == MOTION_SETTLING && still(&m.still, now)) {
        mv_end(MOTION_ARRIVED, ACK_COMPLETED, E_OK);
    }
    if (type == 0x06) m.prev06 = l->pos;
}

/* ─────────────────────────── motion.h ─────────────────────────── */

void motion_init(void) { memset(&m, 0, sizeof m); }

void motion_forget(void)
{
    drive_forget();                   /* la boucle vient d'être arrêtée */
    motion(MOTION_IDLE);
    m.due = UINT64_MAX;
    stop_watch(0, 0);
    motion_q_end(0);                  /* un q retenu : son 0x1C abandonné avec la boucle, ACK_FAILED, E_ABORTED */
}

bsk_motion_state_t motion_state(void) { return m.state; }

bool motion_busy(void) { return m.busy; }

bool motion_aim(uint16_t *target, uint16_t *from)
{
    if (!m.busy) return false;
    *target = m.target;
    *from = m.pos0;
    return true;
}

uint16_t motion_unconfirmed(void) { return m.unconfirmed; }

/* Sans bornes connues (lim : sans 0x06, ou un intervalle invalide), toute cible est refusée. */
bool motion_admit(int64_t target)
{
    const lens_rx_t *l = lens_rx();
    return l->lim && target >= l->fmin && target <= l->fmax;
}

/* CMD_FOCUS_GOTO (arg : la cible) et CMD_FOCUS_MOVE (arg : ±n depuis la position lue). */
void motion_command(const bsk_cmd_t *c, uint64_t now)
{
    int64_t target = c->op == CMD_FOCUS_MOVE ? (int64_t)lens_rx()->pos + c->arg : (int64_t)c->arg;
    if (!motion_admit(target)) {
        session_ack(c->seq, ACK_REJECTED, E_LIMIT);
        return;
    }
    session_ack(c->seq, ACK_ACCEPTED, E_OK);
    m.busy = true;
    m.busy_seq = c->seq;
    mv_start((uint16_t)target, now);
}

void motion_restore(uint16_t target, uint64_t now) { mv_start(target, now); }

/* Le 0x1C part même sans mouvement. Le mouvement en cours est ABORTED même si le 0x1C n'est pas parti : la consigne est
 * retirée. L'accusé de q est le résultat du premier envoi. */
bsk_err_t motion_stop(uint64_t now)
{
    m.stop_tracked = m.busy || (m.stop_sends && m.stop_tracked);   /* un second q reste l'arrêt du goto qu'il surveille */
    bsk_err_t e = drive_stop();
    if (motion_moving()) mv_end(MOTION_ABORTED, ACK_FAILED, E_ABORTED);
    stop_watch(e == E_OK ? 1 : 0, now);   /* pas parti, q le dit (ACK_FAILED) : rien à surveiller */
    return e;
}

/* L'issue du premier envoi du 0x1C de q, à `now` : son accusé final, et la surveillance s'il est parti. Pas parti, q le dit
 * (ACK_FAILED) : rien à surveiller, et un goto déposé après lui, qui attendait (mv_start), part. Aucun 0x1C ne part
 * après le 0x1D d'un goto : un goto dont le 0x1D est parti (la surveillance finie pendant la retenue) ne la rallume
 * pas. */
static void q_sent(uint32_t seq, bsk_err_t e, uint64_t now)
{
    if (!motion_moving() || m.wait) {
        if (e == E_OK) stop_watch(1, now);
        else stop_end(now);
    }
    session_ack(seq, e == E_OK ? ACK_COMPLETED : ACK_FAILED, e);
}

void motion_q(uint32_t seq, uint64_t now)
{
    uint8_t k;
    bsk_err_t e = motion_stop(now);
    if (e == E_OK && drive_stop_held(&k)) {   /* retenu jusqu'au 0x04 : accusé, et surveillé de nouveau, à son départ */
        m.q_held[k] = true;
        m.q_seq[k] = seq;
        return;
    }
    q_sent(seq, e, now);
}

void motion_q_end(uint64_t now)
{
    bsk_err_t e;
    for (uint8_t k = 0; k < BSK_TXN_HELD_MAX; k++) {
        if (!m.q_held[k] || !drive_stop_end(k, &e)) continue;
        m.q_held[k] = false;
        q_sent(m.q_seq[k], e, now);
    }
}

void motion_leave(bsk_err_t why) { cmd_end(ACK_FAILED, why); }

/* L'arrêt confirmé éteint la surveillance : un accusé 1C de toute valeur (1C 00 du 135, 1C 01 du Tamron), ou l'immobilité,
 * pour un objectif qui s'arrête sans accuser (le Sony, peut-être : son accusé du 0x1C n'est pas établi). Seuls les 0x1C de q
 * (motion_stop) et de STALLED (motion_expire) sont surveillés, pas celui du homing de démarrage. */
static bool stopped(uint64_t now) { return lens_rx()->ack1c || still(&m.stop_still, now); }

/* Le goto qui attend ne lit pas la trame qui finit l'arrêt : son 0x1D n'est pas parti, l'accusé 1D qu'elle peut porter
 * est celui du goto arrêté (le 135 l'accuse 1D 00 au moteur arrêté, dans le 0x06 de l'accusé 1C). */
void motion_frame(uint8_t type, uint64_t now)
{
    if (type != 0x05 && type != 0x06) return;
    if (motion_moving() && !m.wait) mv_frame(type, lens_rx()->ack1d, now);
    if (m.stop_sends && stopped(now)) stop_end(now);
}

uint64_t motion_due(void) { return m.stop_due < m.due ? m.stop_due : m.due; }

/* La fenêtre de l'arrêt échue sans accusé 1C ni immobilité : le même 0x1C, ou l'abandon (« arrêt non confirmé »), compté
 * si c'est l'arrêt d'un déplacement suivi (motion_unconfirmed). */
static void stop_expire(uint64_t now)
{
    if (m.stop_sends < SENDS) {
        uint8_t n = (uint8_t)(m.stop_sends + 1);
        bsk_journal_resend(0x1C, n, E_TIMEOUT);
        (void)drive_stop();           /* le résultat de l'envoi ne change rien : la fenêtre repart */
        m.stop_sends = n;             /* l'immobilité reste suivie depuis le premier 0x1C */
        m.stop_due = now + (uint64_t)STOP_MS * MS;
    } else {
        bsk_journal_giveup(0x1C, E_TIMEOUT);
        if (m.stop_tracked) m.unconfirmed++;
        stop_end(now);
    }
}

/* -> STALLED : ni démarrage ni accusé en 1 s, trois fois, ou pas immobile en 10 s. Le 0x1C part, surveillé comme celui de
 * q : l'état du moteur est inconnu (un 135 bloqué peut se débloquer et finir le goto). Seule différence : un premier 0x1C
 * qui n'est pas parti est surveillé aussi, comme un renvoi (stop_expire) ; q, lui, le dit à l'hôte (ACK_FAILED), STALLED
 * n'a personne à qui le dire. Un 0x1D encore accroché (aucune paire partie depuis le dernier envoi, bsk_txn_attach) est
 * retiré par drive_stop : il ne part pas après l'arrêt.
 * Le même 0x1D n'est renvoyé que là où la carte conclurait à un échec : ni démarrage ni accusé en START_MS. Un objectif
 * qui l'a reçu bouge, ou l'accuse, bien avant ; le renvoyer plus tôt à un 135 qui a reçu le premier fabriquerait une
 * fausse arrivée (mv_frame). */
bool motion_expire(uint64_t now)
{
    if (now >= m.stop_due) {          /* jamais avec un 0x1D parti : un goto déposé attend la fin (mv_start) */
        stop_expire(now);
        return true;
    }
    if (now < m.due) return false;
    if (m.state == MOTION_COMMANDED && m.sends < SENDS) {
        m.sends++;
        bsk_journal_resend(0x1D, m.sends, E_TIMEOUT);
        drive_goto(m.target);
        m.due = now + (uint64_t)START_MS * MS;
        return true;
    }
    if (m.state == MOTION_COMMANDED) bsk_journal_giveup(0x1D, E_TIMEOUT);
    (void)drive_stop();               /* l'objectif arrêté ; parti ou non, la surveillance renvoie (stop_expire) */
    stop_watch(1, now);
    m.stop_tracked = m.busy;          /* un goto de l'hôte ou du bouton ; pas le retour à la marque */
    mv_end(MOTION_STALLED, ACK_FAILED, E_STALL);
    return true;
}
