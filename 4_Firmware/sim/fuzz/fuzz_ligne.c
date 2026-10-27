/* Cible 4 de plans/fuzzing.md (l'atelier), en harnais d'octets (F4) : la ligne de l'hôte. Les octets reçus par l'USB
 * passent par le cadrage de la carte, recopié à la main de rx_task (cadre, hote.c), et chaque ligne livrée va à la vraie
 * couche HOTE (components/host/host.c), au-dessus d'une session scriptée dont l'état et les accusés sont choisis par le
 * cas. Propriétés P1 (les sanitizers), P2 (la garde du pilote ; la réponse différée bornée), P6 muet et P10.
 *
 * POURQUOI UNE SESSION SCRIPTÉE (le choix est laissé libre par le brief de #804) : un cas doit se jouer en quelques
 * microsecondes pour que libFuzzer apprenne ; mener la vraie SESSION jusqu'à READY coûte un scénario de F2 (quelques cas
 * par seconde, F3.2). La session scriptée est celle de sim/test/test_host.c, réduite au contrat : elle ne juge rien et
 * n'est pas jugée. Ce qu'elle ne fait pas : avancer d'elle-même d'un état à l'autre selon l'objectif (ses deux états
 * alternent au rythme que le cas choisit), tenir `held` après p0 et p1 (l'en-tête le pose, en OFF seulement), publier
 * une marque rangée par js ou jx (l'en-tête la pose).
 *
 * LE CONTRAT DE L'APPELANT, tenu :
 *   - le cadrage (hote.h) : une ligne livrée attend sa réponse, rien n'est cadré avant (host_usb.c, deliver) ;
 *   - la boucle de main.c, un tour par milliseconde de temps virtuel : le pas de la session (scriptée), bsk_host_observe,
 *     puis une ligne au plus (bsk_host_usb_service) ; après BSK_HOST_LATER, bsk_host_later à chaque tour, après
 *     bsk_host_observe, et aucune autre ligne avant sa réponse (bsk_host.h) ;
 *   - la session tient bsk_session.h (bsk_session_command) : pour chaque commande, un premier accusé dans l'appel,
 *     ACK_REJECTED (final) ou ACK_ACCEPTED ; ATTACH, DETACH, CLEAR_FAULT et APERTURE_SET finissent dans l'appel ; STOP,
 *     SET_MARK et CLEAR_MARK dans l'appel ou plus tard ; GOTO, MOVE et CUSTOM restent en vol au moins un tour ; pendant un
 *     vol, toute commande sauf STOP reçoit E_BUSY, et un STOP accepté finit un GOTO ou un MOVE (E_ABORTED) ; une marque
 *     en attente refuse SET_MARK et CLEAR_MARK (E_BUSY) ; la sortie de READY finit le vol (E_ABORTED) ; chaque accusé
 *     final vient au plus 3 tours après le dépôt ; une raison est une erreur de bsk_err.h, jamais E_OK ;
 *   - l'instantané tient bsk_contract.h (bsk_status_t) : position_valid en READY seulement, limits_valid avec un
 *     intervalle et CAP_LIMITS_REPORTED, la plage d'ouverture avec CAP_APERTURE, les codes dans 0x0E00..0x2000 ou 0,
 *     held en OFF seulement, mf, oss et ring dans leur énumération, un nom de lens_rx (0x20..0x7E sans `"`, sans espace
 *     finale, 64 octets au plus) ;
 *   - le capteur tient bsk_sensor.h : t_c dans -4500..13000, rh_c dans 0..10000, BSK_SENSOR_NONE seulement avant la
 *     première mesure connue ; la mesure ne change qu'au pas ;
 *   - HOTE est compilé comme la suite le compile (sim/test/programmes.sh) : led.c, -lm, sim/stacks_sim.c, et un capteur,
 *     ici celui du cas (bsk_sensor_read, comme test_host et fuzz_climat) ; le vrai journal (journal.c), éteint au départ
 *     comme sur la carte.
 *
 * Un cas : un en-tête de 32 octets, la session, puis les octets reçus par l'USB, tous arrivés à l'instant 0.
 *   [0] l'état A, [1] l'état B (modulo 8) ; [2] la bascule : A et B alternent tous les [2] tours (0 : A seul), chaque
 *       changement d'état journalisé comme la session le journalise (bsk_journal_session) ;
 *   [3] des drapeaux : 1 position_valid, 2 limits_valid, 4 moving, 8 held, 16 mark_valid, 32 CAP_APERTURE,
 *       64 CAP_APERTURE_READBACK, 128 CAP_LIMITS_REPORTED ;
 *   [4..9] la position, le min, le max (16 bits, petit-boutiste) ; [10..15] l'ouverture courante, le min, le max (un
 *       code : 0, ou 0x0E00 + v % 0x1201) ; [16..17] la marque ; [18..19] le code modèle (0 : aucun) ;
 *   [20] le nom (NOMS, modulo) ; [21] mf (bits 0-1), oss (2-3), ring (4-5) modulo 3, et 64 : un arrêt non confirmé à
 *       chaque bascule ; [22] last_error (modulo 16) ;
 *   [23] le capteur : la mesure connue au tour t si le bit t % 8 est posé ; [24..25] t_c, [26..27] rh_c (modulo leur
 *       domaine) ;
 *   [28] DEBUG : 128 les compteurs à 0xFFFFFFFF (sinon la valeur de l'octet), l'état de la NVS (octet >> 4) % 3 ;
 *   [29..31] les accusés, l'octet b = [29 + k % 3] pour la k-ième commande déposée : 128 refusée ; sinon acceptée, finie
 *       (b >> 4) & 3 tours plus tard (un au moins pour GOTO, MOVE, CUSTOM ; dans l'appel pour ATTACH, DETACH,
 *       CLEAR_FAULT, APERTURE_SET, et pour STOP hors de READY et RESTORING), par ACK_FAILED si le bit 64 est posé et que
 *       la commande peut échouer, ACK_COMPLETED sinon ; la raison du refus ou de l'échec, la (b & 15)-ième, modulo, de
 *       celles que la vraie session donne à cette commande (raisons, plus bas).
 * Les 16 octets de CUSTOM sont les octets 0 à 15 de l'en-tête. Des octets d'en-tête manquants valent 0.
 *
 * LES ORACLES, écrits ici à la main d'après PROTOCOL.md, sans rien demander à host.c : la famille d'une ligne
 * (hote_famille), celles dont la réponse peut attendre (hote_differable) et le mode que demande une ligne LOG (hote_log)
 * sont ceux de l'hôte fou de F3 (hote.c, écrits à la main d'après PROTOCOL.md § 1 à § 4). Les formes commande par
 * commande, P4 côté HOTE et l'admission de B4 (hote_juge) ne sont pas jugées ici : elles dépendent de ce que la vraie
 * session fait de chaque commande, que la session scriptée n'imite pas ; F3 les juge (fuzz_hote). */
#define _POSIX_C_SOURCE 200809L

#include <string.h>

#include "bsk_bench_core.h"
#include "bsk_contract.h"
#include "bsk_host.h"
#include "bsk_journal.h"
#include "bsk_led.h"
#include "bsk_sensor.h"
#include "bsk_session.h"
#include "fuzz.h"
#include "hote.h"

const char   FZ_CIBLE[] = "ligne";
const size_t FZ_CAP = 8192;

#define ENTETE      32u
#define ATTENTE_MAX 4u   /* les appels de bsk_host_later qui rendent encore BSK_HOST_LATER : l'accusé final vient au plus 3
                            tours après le dépôt, la réponse au tour qui le lit */

enum { C_FIN, C_TAMPON, C_LIGNE, C_LONG, C_OK, C_MUET, C_DIFF, C_N };

const fz_check_t FZ_CHECKS[] = {
    [C_FIN] = {"P10", "fin", "PROTOCOL.md § 1 règles 1 et 2, § 4 ; bsk_host.h (bsk_host_line) : `\\n` après une ligne "
                             "Basilisk ou Pinefeat, la réponse d'une lecture Moonlite :G…# sans fin de ligne, rien à une "
                             "autre ligne `:…` ; BSK_HOST_LATER pour CUSTOM, q, js et jx seulement ; la réponse différée "
                             "finie par `\\n`"},
    [C_TAMPON] = {"P10", "tampon", "bsk_host.h, BSK_HOST_REPLY_MAX : la réponse tient dans le tampon"},
    [C_LIGNE] = {"P10", "une_ligne", "PROTOCOL.md § 1 règle 1 : une réponse est une ligne, sans octet de contrôle"},
    [C_LONG] = {"P10", "longueur", "PROTOCOL.md § 1 règle 4 : au plus 15 caractères pour f r e a d s c et m<n>"},
    [C_OK] = {"P10", "ok", "PROTOCOL.md § 1 règle 5 : jamais la sous-chaîne ok dans une erreur (hors `i`, qui rend un nom "
                           "d'objectif)"},
    [C_MUET] = {"P6", "muet", "CLAUDE.md § Interdits ; PROTOCOL.md § 1 règle 3 : journal éteint (au démarrage, après LOG "
                              "OFF), aucune ligne `*`"},
    [C_DIFF] = {"DIFFEREE", "borne", "bsk_host.h (bsk_host_later) : l'attente est bornée, la session finit toujours la "
                                     "commande ; la session scriptée rend l'accusé final au plus 3 tours après le dépôt"},
};
const size_t FZ_N_CHECKS = C_N;

int fz_cmd(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return -1;
}

/* ─────────────────────────── la session scriptée ─────────────────────────── */

static const char *const NOMS[] = {
    "", "AF 135mm F1.8 FE", "FE 24-105mm F4 G OSS", "er ok",
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-+",   /* 64 octets */
    "~ !#$%&'()*+,-./:;<=>?@[\\]^_`{|}",
};

typedef struct {
    bool             actif;
    bsk_cmd_op_t     op;
    uint32_t         seq;
    bsk_ack_result_t r;
    bsk_err_t        why;
    uint64_t         tour;          /* le tour où l'accusé final est rendu */
} final_t;

static struct {
    uint8_t      h[ENTETE];         /* l'en-tête du cas */
    uint64_t     tour;
    uint8_t      etat;              /* l'état publié */
    uint16_t     unconfirmed;
    bool         connu;             /* une mesure connue a été rendue : BSK_SENSOR_NONE n'est plus possible */
    bsk_sensor_t mesure;
    unsigned     depots;
    bsk_ack_t    q[16];
    size_t       q_tete, q_n;
    final_t      vol;               /* GOTO, MOVE ou CUSTOM en vol */
    final_t      marque;            /* SET_MARK ou CLEAR_MARK en attente de son écriture */
    final_t      stop;              /* STOP en attente (son 0x1C retenu) */
} S;

static uint16_t mot(size_t i) { return (uint16_t)(S.h[i] | S.h[i + 1] << 8); }
static uint16_t code_ap(uint16_t v)
{
    return v ? (uint16_t)(BSK_AP_CODE_MIN + v % (BSK_AP_CODE_MAX - BSK_AP_CODE_MIN + 1u)) : 0;
}

void bsk_session_status(bsk_status_t *st)
{
    const uint8_t f = S.h[3], e = S.etat;
    memset(st, 0, sizeof *st);
    st->session_state = e;
    st->moving = e == SESSION_READY && (f & 4);
    st->held = e == SESSION_OFF && (f & 8);
    st->position_valid = e == SESSION_READY && (f & 1);
    st->focus_position = mot(4);
    st->capabilities = (f & 32 ? CAP_APERTURE : 0u) | (f & 64 ? CAP_APERTURE_READBACK : 0u) |
                       (f & 128 ? CAP_LIMITS_REPORTED : 0u);
    if (f & 2) {
        uint16_t a = mot(6), b = mot(8);
        st->limits_valid = true;
        st->focus_min = a < b ? a : b;
        st->focus_max = a < b ? b : a;
        st->capabilities |= CAP_LIMITS_REPORTED;
    }
    st->aperture_current = code_ap(mot(10));
    if (f & 32) {
        uint16_t a = code_ap(mot(12)), b = code_ap(mot(14));
        if (!a) a = BSK_AP_CODE_MIN;
        if (!b) b = BSK_AP_CODE_MIN;
        st->aperture_min = a < b ? a : b;
        st->aperture_max = a < b ? b : a;
    }
    st->mark_valid = (f & 16) != 0;
    st->mark_position = mot(16);
    st->lens_id_product = mot(18);
    strcpy(st->lens_name, NOMS[S.h[20] % (sizeof NOMS / sizeof NOMS[0])]);
    st->mf = (uint8_t)((S.h[21] & 3u) % 3u);
    st->oss = (uint8_t)((S.h[21] >> 2 & 3u) % 3u);
    st->ring = (uint8_t)((S.h[21] >> 4 & 3u) % 3u);
    st->last_error = (bsk_err_t)(S.h[22] % 16u);
    st->stop_unconfirmed = S.unconfirmed;
}

static void pousse(uint32_t seq, bsk_ack_result_t r, bsk_err_t why)
{
    if (S.q_n == sizeof S.q / sizeof S.q[0]) return;   /* jamais : HOTE lit la file après chaque dépôt et chaque pas */
    S.q[(S.q_tete + S.q_n++) % (sizeof S.q / sizeof S.q[0])] = (bsk_ack_t){.seq = seq, .result = r, .reason = why};
}

bool bsk_session_ack(bsk_ack_t *a)
{
    if (!S.q_n) return false;
    *a = S.q[S.q_tete];
    S.q_tete = (S.q_tete + 1) % (sizeof S.q / sizeof S.q[0]);
    S.q_n--;
    return true;
}

static void finir(final_t *f)
{
    if (!f->actif) return;
    pousse(f->seq, f->r, f->why);
    f->actif = false;
}

/* Les raisons que la vraie session donne, commande par commande : le refus (le premier accusé) et l'échec (l'accusé
 * final). components/session/cmd.c (bsk_session_command, stop, focus, aperture_cmd, mark_cmd, custom_cmd,
 * cmd_custom_poll, cmd_forget), motion.c (motion_command : E_LIMIT ; mv_end, motion_leave), mark.c (mark_set,
 * mark_clear), txn.c (l'émission : E_BUS, E_FORBIDDEN ; la file des retenues pleine : E_BUSY ; la boucle arrêtée :
 * E_ABORTED). HOTE ne dépose GOTO, MOVE, APERTURE_SET, les marques et CUSTOM qu'en READY : not_ready (cmd.c) n'y vient
 * pas. ATTACH, DETACH, CLEAR_FAULT et APERTURE_SET finissent dans l'appel, ACK_COMPLETED. */
typedef struct {
    const bsk_err_t *refus;
    size_t           nr;
    const bsk_err_t *echec;         /* NULL : jamais d'échec */
    size_t           ne;
} raisons_t;

static const bsk_err_t R_FOCUS[] = {E_BUSY, E_LIMIT}, E_FOCUS[] = {E_STALL, E_ABORTED, E_LOST};
static const bsk_err_t R_STOP[] = {E_BUSY}, E_STOP[] = {E_BUS, E_FORBIDDEN, E_BUSY, E_ABORTED};
static const bsk_err_t R_POSE[] = {E_BUSY, E_NOCAP, E_LIMIT}, R_EFFACE[] = {E_BUSY, E_NOCAP}, E_MARQUE[] = {E_STORE};
static const bsk_err_t R_CUSTOM[] = {E_BUSY, E_NOCAP, E_DESYNC, E_FORBIDDEN, E_BUS};
static const bsk_err_t E_CUSTOM[] = {E_TIMEOUT, E_FRAMING, E_ABORTED, E_BUS};
static const bsk_err_t R_OUVERTURE[] = {E_BUSY, E_NOCAP}, R_AUTRE[] = {E_BUSY};
#define N_(t) (sizeof t / sizeof t[0])

static raisons_t raisons(bsk_cmd_op_t op)
{
    switch (op) {
    case CMD_FOCUS_GOTO:
    case CMD_FOCUS_MOVE: return (raisons_t){R_FOCUS, N_(R_FOCUS), E_FOCUS, N_(E_FOCUS)};
    case CMD_FOCUS_STOP: return (raisons_t){R_STOP, N_(R_STOP), E_STOP, N_(E_STOP)};
    case CMD_SET_MARK: return (raisons_t){R_POSE, N_(R_POSE), E_MARQUE, N_(E_MARQUE)};
    case CMD_CLEAR_MARK: return (raisons_t){R_EFFACE, N_(R_EFFACE), E_MARQUE, N_(E_MARQUE)};
    case CMD_LENS_CUSTOM: return (raisons_t){R_CUSTOM, N_(R_CUSTOM), E_CUSTOM, N_(E_CUSTOM)};
    case CMD_APERTURE_SET: return (raisons_t){R_OUVERTURE, N_(R_OUVERTURE), NULL, 0};
    default: return (raisons_t){R_AUTRE, N_(R_AUTRE), NULL, 0};   /* ATTACH, DETACH, CLEAR_FAULT */
    }
}

void bsk_session_command(const bsk_cmd_t *c)
{
    const uint8_t b = S.h[29 + S.depots++ % 3];
    const raisons_t w = raisons(c->op);
    const uint64_t d = (b >> 4) & 3u;
    const bool marque = c->op == CMD_SET_MARK || c->op == CMD_CLEAR_MARK, echec = (b & 64) && w.echec;
    final_t f = {.actif = true, .op = c->op, .seq = c->seq, .r = echec ? ACK_FAILED : ACK_COMPLETED,
                 .why = echec ? w.echec[(b & 15) % w.ne] : E_OK, .tour = S.tour + d};
    if ((S.vol.actif && c->op != CMD_FOCUS_STOP) || (marque && S.marque.actif)) {
        pousse(c->seq, ACK_REJECTED, E_BUSY);
        return;
    }
    if (b & 128) {
        pousse(c->seq, ACK_REJECTED, w.refus[(b & 15) % w.nr]);
        return;
    }
    pousse(c->seq, ACK_ACCEPTED, E_OK);
    switch (c->op) {
    case CMD_FOCUS_GOTO:
    case CMD_FOCUS_MOVE:
    case CMD_LENS_CUSTOM:
        if (!d) f.tour = S.tour + 1;
        S.vol = f;
        break;
    case CMD_FOCUS_STOP:
        if (S.vol.actif && S.vol.op != CMD_LENS_CUSTOM) {   /* le mouvement arrêté : son accusé final */
            S.vol.r = ACK_FAILED;
            S.vol.why = E_ABORTED;
            finir(&S.vol);
        }
        if (S.etat != SESSION_READY && S.etat != SESSION_RESTORING) pousse(c->seq, ACK_COMPLETED, E_OK);   /* rien d'émis */
        else if (d) S.stop = f;
        else finir(&f);
        break;
    case CMD_SET_MARK:
    case CMD_CLEAR_MARK:
        if (d) S.marque = f;
        else finir(&f);
        break;
    default:                                                /* ATTACH, DETACH, CLEAR_FAULT, APERTURE_SET */
        finir(&f);
        break;
    }
}

void bsk_session_debug(bsk_session_debug_t *d)
{
    const uint8_t b = S.h[28];
    const uint32_t v = b & 128 ? UINT32_MAX : b;
    *d = (bsk_session_debug_t){v, v, v, v, v, v, v, v, v, (bsk_store_nvs_t)((b >> 4) % 3u)};
}

uint32_t bsk_bench_refused(void) { return S.h[28] & 128 ? UINT32_MAX : S.h[28]; }

void bsk_session_custom_data(uint8_t data[BSK_CUSTOM_DATA]) { memcpy(data, S.h, BSK_CUSTOM_DATA); }

void bsk_sensor_read(bsk_sensor_t *v) { *v = S.mesure; }

static uint64_t horloge(void) { return S.tour * 1000u; }

/* Le pas de la session : les accusés finaux échus, la bascule de l'état, la mesure du capteur. */
static void pas(void)
{
    const uint8_t k = S.h[2];
    uint8_t e = (uint8_t)((k && (S.tour / k) % 2u ? S.h[1] : S.h[0]) % 8u);
    if (S.vol.actif && S.vol.tour <= S.tour) finir(&S.vol);
    if (S.marque.actif && S.marque.tour <= S.tour) finir(&S.marque);
    if (S.stop.actif && S.stop.tour <= S.tour) finir(&S.stop);
    if (e != S.etat) {
        if (S.etat == SESSION_READY && S.vol.actif) {       /* la sortie de READY finit le vol */
            S.vol.r = ACK_FAILED;
            S.vol.why = E_ABORTED;
            finir(&S.vol);
        }
        S.etat = e;
        if (S.h[21] & 64) S.unconfirmed++;
        bsk_journal_session(e, e == SESSION_FAULT ? (bsk_err_t)(S.h[22] % 16u) : E_OK);
    }
    if (S.h[23] >> (S.tour % 8u) & 1u) {
        S.mesure = (bsk_sensor_t){.st = BSK_SENSOR_OK, .t_c = (int32_t)(mot(24) % 17501u) - 4500,
                                  .rh_c = (int32_t)(mot(26) % 10001u)};
        S.connu = true;
    } else {
        S.mesure = (bsk_sensor_t){.st = S.connu ? (bsk_sensor_st_t)(BSK_SENSOR_NACK + S.tour % 4u) : BSK_SENSOR_NONE};
    }
}

/* ─────────────────────────── l'USB, servi comme host_usb.c ─────────────────────────── */

static struct {
    const uint8_t *d;
    size_t         r, n;            /* les octets reçus ; r : le prochain à cadrer */
    cadre_t        cad;
    bool           vol;             /* une ligne livrée dont la réponse n'est pas écrite */
    bool           later;           /* ... différée */
    unsigned       attente;         /* les bsk_host_later qui ont rendu BSK_HOST_LATER */
    hote_fam_t     fam;
    char           ligne[HOTE_LIGNE_MAX + 1];
    int            journal;         /* le mode que les lignes LOG ont demandé (hote_log) : 0 éteint */
    unsigned       lignes, differees, etoiles;
} U;
static char      R[BSK_HOST_REPLY_MAX];   /* la taille du tampon de host_usb.c : ASan y voit un débordement */
static fz_ctx_t *C;

static void avancer(void)
{
    while (!U.vol && U.r < U.n)
        if (cadre(&U.cad, U.d[U.r++], U.ligne)) U.vol = true;
}

/* Les lignes f r e a d s c et m<n> (PROTOCOL.md § 1 règle 4). */
static bool figee(const char *l) { return (l[0] && strchr("freadsc", l[0])) || (l[0] == 'm' && l[1] >= '0' && l[1] <= '9'); }

/* La réponse rendue avec sa fin (`finale` : celle de bsk_host_later après BSK_HOST_LATER). */
static void juger(const char *fin, bool finale)
{
    const char *l = U.ligne;
    const bool moon = l[0] == ':', getter = U.fam >= F_ML_GP && U.fam <= F_ML_GH;
    size_t n;
    if (finale || !moon) {
        if (!fin || strcmp(fin, "\n")) {
            fz_fail(C, C_FIN, "« %s »%s : %s, attendu une ligne finie par `\\n`", hote_vis(l), finale ? " (différée)" : "",
                    fin ? "une autre fin" : "aucune réponse");
            return;
        }
    } else if (getter && (!fin || *fin)) {
        fz_fail(C, C_FIN, "« %s » : %s, attendu la réponse sans fin de ligne", hote_vis(l), fin ? "une fin" : "aucune réponse");
        return;
    } else if (!getter && fin) {
        fz_fail(C, C_FIN, "« %s » : une réponse, attendu aucune", hote_vis(l));
        return;
    }
    if (!fin) return;
    n = strlen(R);
    if (n >= BSK_HOST_REPLY_MAX - 1) {
        fz_fail(C, C_TAMPON, "« %s » : une réponse de %zu octets, le tampon plein", hote_vis(l), n);
        return;
    }
    for (size_t i = 0; i < n; i++)
        if ((unsigned char)R[i] < 0x20 || R[i] == 0x7F) {
            fz_fail(C, C_LIGNE, "« %s » : l'octet %02X à la place %zu de la réponse", hote_vis(l), (unsigned char)R[i], i);
            return;
        }
    if (figee(l) && n > 15) {
        fz_fail(C, C_LONG, "« %s » : %zu caractères", hote_vis(l), n);
        return;
    }
    if (!strncmp(R, "er", 2) && strstr(R, "ok") && U.fam != F_I) fz_fail(C, C_OK, "« %s » : « %s »", hote_vis(l), R);
}

static void tracer(const char *fin)
{
    char rv[256];
    if (!C->trace) return;
    snprintf(rv, sizeof rv, "%s", hote_vis(R));
    fz_tr(C, "tour %llu %s « %s » (%s) -> %s%s%s", (unsigned long long)S.tour, U.later ? "réponse différée de" : "ligne",
          hote_vis(U.ligne), HOTE_FAM[U.fam], fin == BSK_HOST_LATER ? "(différée)" : fin ? "« " : "(aucune réponse)",
          fin && fin != BSK_HOST_LATER ? rv : "", fin && fin != BSK_HOST_LATER ? (*fin ? " » \\n" : " »") : "");
}

static void servir(void)
{
    const char *fin;
    if (U.later) {
        R[0] = '\0';
        fin = bsk_host_later(R, sizeof R);
        if (fin == BSK_HOST_LATER) {
            if (++U.attente > ATTENTE_MAX)
                fz_fail(C, C_DIFF, "« %s » (%s) : toujours sans réponse après %u tours", hote_vis(U.ligne), HOTE_FAM[U.fam],
                        U.attente);
            return;
        }
        tracer(fin);
        juger(fin, true);
        U.later = U.vol = false;
        avancer();
        return;
    }
    if (!U.vol) return;
    U.fam = hote_famille(U.ligne);
    U.lignes++;
    R[0] = '\0';
    fin = bsk_host_line(U.ligne, R, sizeof R);
    tracer(fin);
    if (fin == BSK_HOST_LATER) {
        if (!hote_differable(U.fam))
            fz_fail(C, C_FIN, "« %s » (%s) : BSK_HOST_LATER pour une ligne qui n'attend rien", hote_vis(U.ligne),
                    HOTE_FAM[U.fam]);
        U.later = true;
        U.attente = 0;
        U.differees++;
        return;
    }
    juger(fin, false);
    if (U.fam == F_LOG && hote_log(U.ligne) >= 0) U.journal = hote_log(U.ligne);
    U.vol = false;
    avancer();
}

/* Le journal, vidé comme host_usb.c le vide : une ligne d'une génération passée (d'avant un LOG OFF) n'est pas écrite. */
static void vider(void)
{
    static char b[4096];
    uint32_t gen;
    size_t k;
    while ((k = bsk_journal_take(b, sizeof b - 1, &gen)) > 0) {
        b[k < sizeof b - 1 ? k : sizeof b - 1] = '\0';   /* la ligne rendue sans terminateur (bsk_journal.h) */
        if (gen != bsk_journal_gen()) continue;
        U.etoiles++;
        if (U.journal == 0)
            fz_fail(C, C_MUET, "tour %llu : journal éteint, « %.60s »", (unsigned long long)S.tour, hote_vis(b));
    }
}

void fz_run(fz_ctx_t *c, const uint8_t *d, size_t n)
{
    C = c;
    memset(&S, 0, sizeof S);
    memset(&U, 0, sizeof U);
    memcpy(S.h, d, n < ENTETE ? n : ENTETE);
    S.etat = S.h[0] % 8u;
    U.d = d;
    U.r = n < ENTETE ? n : ENTETE;
    U.n = n;
    bsk_journal_init(horloge);
    bsk_journal_set(false, false);                  /* la carte démarre journal éteint */
    vider();
    bsk_led_init(0);
    bsk_host_init(HOTE_VERSION, "poweron", horloge);
    fz_tr(c, "session : A %s, B %s, bascule %u, drapeaux %02X, accusés %02X %02X %02X ; %zu octets reçus",
          bsk_state_name(S.h[0] % 8u), bsk_state_name(S.h[1] % 8u), S.h[2], S.h[3], S.h[29], S.h[30], S.h[31], U.n - U.r);
    avancer();
    while (c->check < 0 && (U.vol || U.later)) {
        S.tour++;
        pas();
        bsk_host_observe();
        vider();
        servir();
    }
    fz_tr(c, "fin : %u lignes, %u différées, %u lignes du journal écrites, %llu tours", U.lignes, U.differees, U.etoiles,
          (unsigned long long)S.tour);
}

/* ─────────────────────────── le générateur ─────────────────────────── */

static uint16_t bord16(fz_rng_t *r)
{
    static const uint16_t B[] = {0, 1, 2, 9, 10, 99, 100, 13723, 13878, 20000, 30983, 30988, 32767, 32768, 65534, 65535};
    return fz_chance(r, 2, 3) ? B[fz_below(r, sizeof B / sizeof B[0])] : (uint16_t)fz_next(r);
}

size_t fz_gen(fz_rng_t *r, uint8_t *out, size_t cap)
{
    static const uint8_t BASCULE[] = {0, 0, 0, 1, 2, 3, 5, 8, 20, 255};
    size_t n = ENTETE;
    uint32_t nl = 1 + fz_below(r, 40);
    for (size_t i = 0; i < ENTETE; i++) out[i] = (uint8_t)fz_next(r);
    out[0] = (uint8_t)(fz_chance(r, 1, 2) ? SESSION_READY : fz_below(r, 8));
    out[1] = (uint8_t)(fz_chance(r, 1, 4) ? SESSION_READY : fz_below(r, 8));
    out[2] = BASCULE[fz_below(r, sizeof BASCULE)];
    for (size_t i = 4; i < 20; i += 2) {
        uint16_t v = bord16(r);
        out[i] = (uint8_t)v;
        out[i + 1] = (uint8_t)(v >> 8);
    }
    out[18] = out[19] = fz_chance(r, 1, 3) ? 0 : out[18];
    for (size_t i = 29; i < 32; i++)
        if (fz_chance(r, 2, 3)) out[i] &= 0x7F;             /* acceptée deux fois sur trois */
    for (uint32_t k = 0; k < nl && n + 320 <= cap; k++) n += hote_octets(r, out + n, cap - n);
    return n;
}
