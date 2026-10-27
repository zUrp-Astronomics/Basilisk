/* L'arrêt suivi d'un goto, à toutes les phases d'un cycle VD, contre le faux 135 (constat G2 de l'audit des garanties
 * du 2026-10-23 ; le défaut R-01, corrigé par le ticket #680 : att_late, components/txn/txn.c).
 *
 * Un cas : un arrêt (CMD_FOCUS_STOP) déposé à la phase p d'un cycle VD, puis, d après lui, un goto vers une cible C.
 * La phase balaie le cycle de 0 à 16 600 µs par pas de 100 µs (167 phases ; la VD de la PHY simulée a pour période
 * 1 000 000 / 60 = 16 666 µs). Six scénarios, les deux sens de mouvement du goto C :
 *   - R+ : au repos en 16000, q, goto vers 16500 ; R- : au repos en 16500, q, goto vers 16000 ;
 *   - M+ : au repos en 16000, un goto vers 15000 en cours (le moteur du faux 135 tourne), q, goto vers 16500 ;
 *     M- : au repos en 16500, un goto vers 17500 en cours, q, goto vers 16000 ;
 *     ces quatre à chaque phase, avec d = 0 (le goto dans la même tâche, sans pas entre les deux) et 250 µs (le témoin
 *     de l'audit) : 1336 cas ;
 *   - L+ : au repos en 14000, q, goto vers 28000 ; L- : au repos en 28000, q, goto vers 14000 : 14 000 pas, plus de
 *     1,2 s de mouvement au faux 135 (10 217 pas/s, lens135.c), d = 16 666 µs (une trame VD) ; l'un ou l'autre à chaque
 *     phase, en alternance : 167 cas.
 * Chaque cas part de la cible du précédent (R+, R-, M+, M-, puis de nouveau R+ ; L+ puis L-) : le banc ne ramène le
 * faux 135 au départ, par un goto qu'il ne juge pas, que si le cas précédent l'a laissé ailleurs.
 *
 * L'oracle ne lit pas l'état publié de MOUVEMENT : il juge ce qui est parti sur le fil vers le faux 135 (le programme
 * est lié avec --wrap=bsk_phy_send) et la position du faux 135 (l135_position, son état interne).
 *   (a) un 0x1C part après le dépôt de l'arrêt, un 0x1D vers C part ensuite (1D 80 3E pour 16000, 1D 74 40 pour
 *       16500, 1D B0 36 pour 14000, 1D 60 6D pour 28000, calculés à la main), et aucun 0x1C ne part après ce 0x1D, dans
 *       une trame à part ou accroché au 0x04 ;
 *   (b) le goto finit ACK_COMPLETED, et à cet instant le faux 135 est en C, son moteur au repos ; 100 ms après, il y
 *       est toujours.
 * L'accusé final du goto ne sert qu'à dire quand le cas est fini, et qu'il l'est COMPLETED : un cas qui finit autrement
 * ou ne finit pas en 3 s est faux aussi (sans quoi un goto jamais fini passerait (b) sans rien prouver).
 *
 * LE CONSTAT (ticket #730), RÉGLÉ PAR LE TICKET #787 : CONSTAT est faux, chaque cas doit tenir (a) et (b). Le constat,
 * tel que #730 l'a mesuré : (a) tenait dans chaque cas, (b) dans aucun. Le faux 135 ne reprend ses
 * ordres de mise au point qu'au traitement de trame, à chaque 0x04 reçu, et y fait le 0x1D avant le 0x1C (samyang.md
 * § 2.4, § 4.1 ; lens135.c, frame_processing) :
 *   - R+, R-, M+, M- : le 0x1D part au premier 0x04 qui suit le 0x1C. Les deux sont repris au même traitement de
 *     trame : le 0x1D arme le goto, le 0x1C l'efface aussitôt, et le 0x1D est accusé 1D 00 au moteur arrêté (samyang.md
 *     § 4.5 : « probably dropped and acknowledged 1D 00 »). La carte lit cet accusé d'un goto émis une fois comme une
 *     cible bornée sur place (motion.c, mv_frame) : ARRIVED, ACK_COMPLETED, le faux 135 à la position de l'arrêt ;
 *   - L+, L- : le 0x1D part au deuxième 0x04. Le 0x1C, repris au premier, attend l'immobilité ; le goto armé au
 *     deuxième la retarde, et 1,2 s après la reprise du 0x1C l'objectif force l'arrêt (samyang.md § 4.5) : le goto
 *     finit ACK_COMPLETED à l'arrêt forcé, en deçà de C.
 * Pour que le faux 135 aille en C, quelle que soit la course, le 0x1D doit lui arriver après le traitement de trame qui
 * finit l'arrêt et l'accuse 1C 00 : au troisième 0x04 qui suit le 0x1C, au plus tôt. La correction (#787,
 * components/session/motion.c, mv_start) : un goto déposé pendant la surveillance de l'arrêt garde son 0x1D jusqu'à la
 * fin de cette surveillance, l'accusé 1C ici. CONSTAT vrai, le programme était vert si (a) tenait partout et si chaque
 * cas finissait ACK_COMPLETED hors de C ; faux, il est vert si chaque cas est juste, rouge sinon.
 *
 * Le banc est celui de test_session135 : la SESSION, TRANSACTION, bench_core au-dessus de la PHY simulée et du faux 135
 * inchangés ; le test fait la boucle de app_main (un pas, les accusés lus) et dépose les commandes entre deux pas, à
 * l'instant réel qu'il donne (bsk_session_real), comme HOTE. */
#define _POSIX_C_SOURCE 200809L   /* alarm, la garde de durée */

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "bsk_session.h"
#include "lens_sim_135.h"
#include "phy_common.h"
#include "phy_sim.h"
#include "store_sim.h"

#define MS 1000u
#define T0 (1000u * MS)
#define VD_US 16666u       /* la période de la VD à 60 Hz dans la PHY simulée, 1 000 000 / 60 */
#define PHASE_STEP 100u
#define PHASE_END 16600u   /* la dernière phase jouée : 167 phases de 0 à 16 600 µs */
#define N_PHASES (PHASE_END / PHASE_STEP + 1)

/* Le constat de l'en-tête : vrai tant que le 0x1D pouvait partir avant la fin de l'arrêt dans l'objectif ; faux depuis le
 * ticket #787, qui l'a réglé. */
static const bool CONSTAT = false;

/* La garde de durée, en secondes : 10 ; la passe sous sanitizers de 4_Firmware/run.sh la relève (-DGARDE_S). */
#ifndef GARDE_S
#define GARDE_S 10
#endif
#define TEXTE_(x) #x
#define TEXTE(x) TEXTE_(x)

static void too_long(int sig)
{
    static const char m[] = "  ECHEC : le programme ne termine pas en " TEXTE(GARDE_S) " s (boucle sans fin)\n";
    (void)sig;
    (void)!write(1, m, sizeof m - 1);
    _exit(1);
}

static int checks, fails;

#define CHECK(c, ...)                                                   \
    do {                                                                \
        checks++;                                                       \
        if (!(c)) {                                                     \
            fails++;                                                    \
            printf("  ECHEC %s:%d : ", __FILE__, __LINE__);             \
            printf(__VA_ARGS__);                                        \
            printf("\n");                                               \
        }                                                               \
    } while (0)

/* ─────────────────────────── ce qui part sur le fil, les fronts de VD ─────────────────────────── */

typedef struct {
    uint64_t    t;
    bsk_frame_t f;
} sent_t;

#define SENT_CAP 4096u
static sent_t SENT[SENT_CAP];
static size_t n_sent;          /* remis à zéro à chaque cas ; au-delà de SENT_CAP, le cas est faux (sent_lost) */
static bool sent_lost;
static uint64_t last_vd;       /* le dernier front de VD remonté par la PHY */

bsk_err_t __real_bsk_phy_send(const bsk_frame_t *f);
bsk_err_t __wrap_bsk_phy_send(const bsk_frame_t *f);
bool __real_bsk_phy_poll(bsk_phy_event_t *ev);
bool __wrap_bsk_phy_poll(bsk_phy_event_t *ev);

bsk_err_t __wrap_bsk_phy_send(const bsk_frame_t *f)
{
    bsk_err_t e = __real_bsk_phy_send(f);
    if (e == E_OK) {           /* parti sur le fil */
        if (n_sent < SENT_CAP) SENT[n_sent++] = (sent_t){phy_sim_now(), *f};
        else sent_lost = true;
    }
    return e;
}

bool __wrap_bsk_phy_poll(bsk_phy_event_t *ev)
{
    bool got = __real_bsk_phy_poll(ev);
    if (got && ev->kind == BSK_PHY_VD) last_vd = ev->t_us;
    return got;
}

/* La trame porte un arrêt : un 0x1C seul, ou accroché derrière les 14 octets du 0x04. */
static bool has_1c(const bsk_frame_t *f)
{
    return f->msg[0] == 0x1C || (f->msg[0] == 0x04 && f->len > 14 && f->msg[14] == 0x1C);
}

/* La trame porte un 0x1D vers la cible `lo` `hi` : accroché au 0x04, ou seul. */
static bool has_1d(const bsk_frame_t *f, uint8_t lo, uint8_t hi)
{
    const uint8_t *m = f->msg[0] == 0x04 && f->len >= 19 ? f->msg + 14 : f->msg[0] == 0x1D && f->len >= 5 ? f->msg : NULL;
    return m && m[0] == 0x1D && m[1] == lo && m[2] == hi;
}

/* ─────────────────────────── le banc ─────────────────────────── */

static l135_t L;
static l135_params_t LP;
static bsk_session_params_t P;
static uint64_t d2_due;        /* l'anti-rebond de D2, que phy_sim_next ne compte pas */
static uint32_t seq_n;

typedef struct {
    bsk_ack_t a;
    uint64_t  t;
    int32_t   pos;             /* la position du faux 135 à la lecture de l'accusé */
    bool      still;           /* son moteur au repos à cet instant : ni en mouvement, ni armé, ni demandé */
} ack_rec_t;

static ack_rec_t ACKS[64];
static size_t n_acks;

static bool lens_still(void) { return !L.moving && !L.armed && L.pending == 0; }

static void read_acks(void)
{
    bsk_ack_t a;
    while (bsk_session_ack(&a))
        if (n_acks < sizeof ACKS / sizeof ACKS[0])
            ACKS[n_acks++] = (ack_rec_t){a, phy_sim_now(), l135_position(&L), lens_still()};
}

static void run_to(uint64_t t_end)
{
    for (;;) {
        uint64_t t = phy_sim_next(), ts = bsk_session_next();
        if (ts < t) t = ts;
        if (d2_due > phy_sim_now() && d2_due < t) t = d2_due;
        if (t > t_end) t = t_end;
        phy_sim_run(t);
        bsk_session_step(t);
        read_acks();
        if (t >= t_end) break;
    }
}

static void run_for(uint64_t us) { run_to(phy_sim_now() + us); }

/* Dépose une commande à l'instant courant, entre deux pas, comme HOTE ; rend son seq. */
static uint32_t cmd(bsk_cmd_op_t op, int32_t arg)
{
    bsk_cmd_t c = {.seq = ++seq_n, .op = op, .arg = arg};
    bsk_session_real(phy_sim_now());
    bsk_session_command(&c);
    read_acks();
    return c.seq;
}

/* L'accusé final de `seq` (ni ACK_ACCEPTED), NULL s'il n'y en a pas encore. */
static const ack_rec_t *final_of(uint32_t seq)
{
    for (size_t i = 0; i < n_acks; i++)
        if (ACKS[i].a.seq == seq && ACKS[i].a.result != ACK_ACCEPTED) return &ACKS[i];
    return NULL;
}

/* Le faux 135 en `pos`, au repos, la commande finie : un goto, joué jusqu'à son accusé final. Rend faux s'il n'y arrive
 * pas. Le banc s'en sert pour poser le départ d'un cas ; il ne juge rien. */
static bool settle_at(int32_t pos)
{
    uint32_t g;
    uint64_t t_end;
    if (l135_position(&L) == pos && lens_still()) return true;
    g = cmd(CMD_FOCUS_GOTO, pos);
    t_end = phy_sim_now() + 3000 * MS;
    while (!final_of(g) && phy_sim_now() < t_end) run_for(10 * MS);
    run_for(50 * MS);
    return l135_position(&L) == pos && lens_still();
}

/* Le 135 monté, démarré à froid, la session READY. */
static bool boot(void)
{
    bsk_status_t st;
    l135_params_default(&LP);
    l135_init(&L, &LP);
    phy_sim_init(lens_sim_135(&L), 0);
    bsk_session_params_default(&P);
    store_sim_reset();
    bsk_session_init(&P);
    run_to(T0);
    l135_power(&L, phy_sim_now(), true);
    phy_sim_d2(true);
    d2_due = phy_sim_now() + D2_DEBOUNCE_US;
    run_for(4000 * MS);
    bsk_session_status(&st);
    return st.session_state == SESSION_READY;
}

/* ─────────────────────────── le balayage ─────────────────────────── */

typedef struct {
    const char *name;
    int32_t     start;         /* la position de départ, au repos */
    int32_t     first;         /* 0 : au repos au q ; sinon la cible du goto en cours au q */
    int32_t     target;        /* C, la cible du goto déposé après le q */
    uint8_t     lo, hi;        /* C, petit-boutiste, écrite à la main */
} scen_t;

/* Dans l'ordre où ils s'enchaînent : chacun part de la cible du précédent. */
static const scen_t SCEN[] = {
    {"R+", 16000, 0, 16500, 0x74, 0x40},
    {"R-", 16500, 0, 16000, 0x80, 0x3E},
    {"M+", 16000, 15000, 16500, 0x74, 0x40},
    {"M-", 16500, 17500, 16000, 0x80, 0x3E},
    {"L+", 14000, 0, 28000, 0x60, 0x6D},
    {"L-", 28000, 0, 14000, 0xB0, 0x36},
};

#define N_SCEN (sizeof SCEN / sizeof SCEN[0])
#define N_SHORT 4u                                  /* R+, R-, M+, M- ; L+ et L- ensuite */
static const uint32_t DELAYS[] = {0, 250};          /* R+, R-, M+, M- */
#define N_DELAYS (sizeof DELAYS / sizeof DELAYS[0])
#define LONG_DELAY 16666u                           /* L+, L- : une trame VD */

/* Les issues d'un cas, la première qui le rend faux. */
typedef enum {
    C_OK,
    C_SETUP,         /* le départ n'a pas pu être posé, ou le moteur ne tourne pas au q en M+ / M- */
    C_GOTO_REFUSED,  /* le goto C n'a pas été accepté */
    C_NO_1C,         /* aucun 0x1C parti après le dépôt de l'arrêt */
    C_NO_1D,         /* aucun 0x1D vers C parti après le dépôt de l'arrêt */
    C_1C_AFTER_1D,   /* (a) un 0x1C parti après le 0x1D vers C */
    C_NOT_COMPLETED, /* le goto n'a pas fini ACK_COMPLETED en 3 s */
    C_WRONG_POS,     /* (b) ACK_COMPLETED, le faux 135 ailleurs qu'en C, ou son moteur pas au repos */
    C_MOVED_AFTER,   /* (b) 100 ms après ACK_COMPLETED, le faux 135 plus en C */
    C_N
} outcome_t;

static const char *const WHY[C_N] = {
    "juste", "départ non posé", "goto refusé", "aucun 0x1C", "aucun 0x1D vers la cible",
    "un 0x1C après le 0x1D (a)", "pas ACK_COMPLETED", "ACK_COMPLETED hors de la cible (b)", "parti de la cible après (b)",
};

typedef struct {
    outcome_t why;
    int32_t   pos;             /* la position du faux 135 à l'accusé final, ou à la fin du cas */
} result_t;

static result_t one(const scen_t *s, uint32_t phase, uint32_t delay)
{
    uint32_t g;
    uint64_t t_min, v, t_end;
    long i1c = -1, i1d = -1;
    const ack_rec_t *fin;
    if (!settle_at(s->start)) return (result_t){C_SETUP, l135_position(&L)};
    t_min = phy_sim_now();
    if (s->first) {
        (void)cmd(CMD_FOCUS_GOTO, s->first);
        t_min += 3 * VD_US;    /* le 0x1D au 0x04 qui suit, le moteur parti au front suivant (lens135.c, S2) */
    }
    for (v = last_vd; v + phase < t_min; v += VD_US) {}
    run_to(v + phase);
    if (s->first && !L.moving) return (result_t){C_SETUP, l135_position(&L)};
    n_sent = 0;
    sent_lost = false;
    n_acks = 0;
    (void)cmd(CMD_FOCUS_STOP, 0);
    if (delay) run_to(phy_sim_now() + delay);
    g = cmd(CMD_FOCUS_GOTO, s->target);
    if (!n_acks || ACKS[n_acks - 1].a.seq != g || ACKS[n_acks - 1].a.result != ACK_ACCEPTED)
        return (result_t){C_GOTO_REFUSED, l135_position(&L)};
    t_end = phy_sim_now() + 3000 * MS;
    while (!final_of(g) && phy_sim_now() < t_end) run_for(1 * MS);
    run_for(100 * MS);
    fin = final_of(g);
    for (size_t i = 0; i < n_sent; i++) {
        if (i1c < 0 && has_1c(&SENT[i].f)) i1c = (long)i;
        if (i1d < 0 && has_1d(&SENT[i].f, s->lo, s->hi)) i1d = (long)i;
    }
    if (sent_lost || i1c < 0) return (result_t){C_NO_1C, l135_position(&L)};
    if (i1d < 0) return (result_t){C_NO_1D, l135_position(&L)};
    for (size_t i = (size_t)i1d + 1; i < n_sent; i++)
        if (has_1c(&SENT[i].f)) return (result_t){C_1C_AFTER_1D, fin ? fin->pos : l135_position(&L)};
    if (!fin || fin->a.result != ACK_COMPLETED) return (result_t){C_NOT_COMPLETED, l135_position(&L)};
    if (fin->pos != s->target || !fin->still) return (result_t){C_WRONG_POS, fin->pos};
    if (l135_position(&L) != s->target) return (result_t){C_MOVED_AFTER, l135_position(&L)};
    return (result_t){C_OK, fin->pos};
}

/* Les comptes par scénario, et les issues de tout le balayage. */
static unsigned tot[N_SCEN], bad[N_SCEN], a_ok[N_SCEN], n[C_N], shown;
static outcome_t expect;

static void play(size_t k, uint32_t phase, uint32_t delay)
{
    result_t r = one(&SCEN[k], phase, delay);
    n[r.why]++;
    tot[k]++;
    a_ok[k] += r.why == C_OK || r.why >= C_NOT_COMPLETED;   /* (a) jugé et tenu */
    bad[k] += r.why != C_OK;
    if (r.why != expect && shown++ < 20)
        printf("    %s, phase %5u µs, goto %5u µs après q : %s (le faux 135 en %d, la cible %d)\n", SCEN[k].name,
               (unsigned)phase, (unsigned)delay, WHY[r.why], (int)r.pos, (int)SCEN[k].target);
}

int main(int argc, char **argv)
{
    unsigned cases = 0, want = N_PHASES * (N_SHORT * N_DELAYS + 1), i = 0;
    (void)argv;
    if (argc != 1) {
        fprintf(stderr, "usage : test_stop_goto135 (sans argument)\n");
        return 2;
    }
    signal(SIGALRM, too_long);
    alarm(GARDE_S);
    expect = CONSTAT ? C_WRONG_POS : C_OK;
    printf("q puis goto à chaque phase du cycle VD, contre le faux 135 : le 0x1C jamais après le 0x1D, le goto fini en sa cible\n");
    if (CONSTAT)
        printf("  constat (ticket #730) : le 0x1D part avant la fin de l'arrêt dans l'objectif, chaque goto finit ACK_COMPLETED"
               " hors de sa cible (en tête du programme) ; attendu en échec sur (b), (a) tenu partout\n");
    CHECK(boot(), "le faux 135 démarré, READY");
    for (uint32_t p = 0; p <= PHASE_END; p += PHASE_STEP)
        for (size_t d = 0; d < N_DELAYS; d++)
            for (size_t k = 0; k < N_SHORT; k++) play(k, p, DELAYS[d]);
    for (uint32_t p = 0; p <= PHASE_END; p += PHASE_STEP) play(N_SHORT + i++ % 2, p, LONG_DELAY);
    for (size_t k = 0; k < N_SCEN; k++) {
        cases += tot[k];
        printf("  %s (%s%d, goto vers %d) : %u cas, (a) tenu dans %u, faux %u\n", SCEN[k].name,
               SCEN[k].first ? "goto en cours vers " : "au repos en ", (int)(SCEN[k].first ? SCEN[k].first : SCEN[k].start),
               (int)SCEN[k].target, tot[k], a_ok[k], bad[k]);
    }
    printf("  balayage : %u cas", cases);
    for (int w = 0; w < C_N; w++)
        if (n[w]) printf(", %s %u", WHY[w], n[w]);
    printf("\n");
    CHECK(cases == want, "%u cas joués, %u attendus", cases, want);
    CHECK(n[expect] == cases, CONSTAT ? "%u cas sur %u finissent comme le constat le dit, (a) tenu et ACK_COMPLETED hors de la"
                                        " cible ; tout autre résultat est un défaut nouveau, ou, un cas juste, le constat"
                                        " réglé (CONSTAT à faux)"
                                      : "%u cas sur %u justes",
          n[expect], cases);
    printf("q puis goto, balayage de phase contre le faux 135 : %d vérifications, %d échec(s)\n", checks, fails);
    return fails ? 1 : 0;
}
