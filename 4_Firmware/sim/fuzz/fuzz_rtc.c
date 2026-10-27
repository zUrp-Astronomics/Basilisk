/* Cible 6 de plans/fuzzing.md (l'atelier) : la mémoire RTC relue par restore_init (components/session/restore.c), la
 * position de reprise (bsk_session_keep_t, en RTC_NOINIT_ATTR sur la carte, main/main.c), donnée arbitraire au
 * bsk_session_init d'un démarrage tiède ou froid. Propriétés P1 (les sanitizers), P2, P3, P4 et P8.
 *
 * Le banc est celui de test_session_script (la mémoire de reprise, p->keep) : la vraie SESSION, TRANSACTION,
 * bench_core, le journal et le magasin en mémoire (sim/store_sim.c), derrière la PHY scriptée (sim/test/phy_script.c).
 * L'objectif scripté est le 135 des traces (sim/sources135.c, comme lens_135 de test_session_script) : poignée de main,
 * réponses d'init, flux après le 0x0A ; un moteur scripté derrière le flux avance de 250 pas par trame vers la cible du
 * 0x1D et accuse 1D 00 à l'arrivée, comme motor() de test_session_script. L'objectif est nominal : seule la mémoire est
 * folle. Aucun geste de l'utilisateur, aucune ligne de l'hôte.
 *
 * Un cas : l'image de la mémoire, sizeof(bsk_session_keep_t) octets (la clé, la position, le sens, l'octet de
 * remplissage, la somme), puis un octet : bit 0 le démarrage tiède (p->warm), bit 1 une marque rangée pour le 135
 * (20000, croissante), bit 2 le journal sous LOG ALL (sinon LOG ON). La mémoire n'est jamais résumée par une graine.
 *
 * Les oracles, écrits à la main :
 *   - la mémoire au bsk_session_init : relue telle quelle si le démarrage est tiède, sa somme FNV-1a juste (calculée ici
 *     d'après la définition de FNV-1a, comme test_session_script) et son sens dans bsk_approach_t (0 à 2) ; effacée (à
 *     zéro) sinon (restore.h, restore_init ; bsk_session.h, p->warm) ;
 *   - la fin du démarrage (bsk_session.h, la position de reprise : « une reprise y revient au lieu de la marque, pour la
 *     même clé » ; restore.c, restore_plan) : la mémoire relue de la clé du 135 (01030800bf087, sa clé calculée à la main
 *     dans test_session_script, KEY135 : les 13 premiers octets, puis un zéro, strcmp) et sa position dans les bornes du
 *     0x06 resserrées de 5 (13878..30733, celles de traces/ : 13873, 30738) : READY sur elle ; hors des bornes : READY sans
 *     mouvement, sans repli sur la marque ; sans mémoire de cette clé : la marque si elle est rangée, sinon READY sans
 *     mouvement (14623, la position du 0x06 des traces) ;
 *   - P3 : chaque transition observée est dans la table de la spec § 3.1 (les lignes sans HOTE, recopiées ci-dessous) ;
 *     FAULT n'a que E_LOST ou E_HOME_FAILED ; aucun état de séquence n'est tenu au-delà de son T_max (ci-dessous) ;
 *   - P4 : après chaque pas, l'instantané (bsk_contract.h, bsk_status_t ; spec § 4.4 règle 3) ;
 *   - P8 : aucune écriture déposée dans le magasin (store.c, en tête : « une écriture n'a lieu que sur un geste de
 *     l'utilisateur »), ici sans geste ;
 *   - P2 : au plus 64 bsk_phy_poll par bsk_session_step (STEP_EVENTS, session.c ; enveloppe --wrap=bsk_phy_poll), le
 *     temps virtuel qui avance. */
#define _POSIX_C_SOURCE 200809L

#include <string.h>

#include "bsk_journal.h"
#include "bsk_session.h"
#include "fuzz.h"
#include "../test/phy_script.h"
#include "sources135.h"
#include "store_sim.h"

const char   FZ_CIBLE[] = "rtc";
const size_t FZ_CAP = 64;

enum { C_MEMOIRE, C_ETAT, C_TRANSITION, C_FAULT, C_DUREE, C_FIN, C_CIBLE, C_INSTANTANE, C_NVS, C_POLL, C_TEMPS, C_LIGNE,
       C_N };

const fz_check_t FZ_CHECKS[] = {
    [C_MEMOIRE] = {"P4", "memoire", "restore.h, restore_init ; bsk_session.h, p->warm : relue si tiède, sa somme FNV-1a et "
                                    "son sens (0 à 2) justes ; effacée sinon"},
    [C_ETAT] = {"P3", "etat", "PROTOCOL.md § 7 (boot_state, huit états) ; bsk_contract.h, bsk_session_state_t"},
    [C_TRANSITION] = {"P3", "transition", "spec § 3.1, tableau des transitions (lignes sans HOTE) ; [D59], [D129]"},
    [C_FAULT] = {"P3", "fault", "plans/survie.md § 3 : seules E_LOST et E_HOME_FAILED mènent à FAULT"},
    [C_DUREE] = {"P3", "duree", "plans/fuzzing.md § 3, P3 : aucun état de séquence tenu plus de T_max (T_max en tête de "
                                "fuzz_rtc.c, depuis bsk_session_params_default et les échéances de motion.c)"},
    [C_FIN] = {"P3", "fin", "plans/fuzzing.md § 3, P3 : l'objectif nominal, la session atteint READY en temps borné"},
    [C_CIBLE] = {"P3", "cible", "bsk_session.h (la position de reprise) et restore.c (restore_plan) : la position de reprise "
                                "de la même clé dans les bornes, sinon rien ; sans elle, la marque"},
    [C_INSTANTANE] = {"P4", "instantane", "bsk_contract.h, bsk_status_t ; spec § 4.4 règle 3 : ouverture dans 0x0E00..0x2000 "
                                          "ou 0, plage avec CAP_APERTURE, limits_valid => min <= max et "
                                          "CAP_LIMITS_REPORTED, position_valid => READY"},
    [C_NVS] = {"P8", "nvs", "components/store/store.c, en tête : « une écriture n'a lieu que sur un geste de l'utilisateur »"},
    [C_POLL] = {"P2", "poll", "session.c, STEP_EVENTS : au plus 64 événements de PHY par pas"},
    [C_TEMPS] = {"P2", "temps", "spec § 4.4 règle 1 (« ne bloque jamais ») : le temps virtuel avance, une échéance est "
                                "traitée"},
    [C_LIGNE] = {"P6", "ligne", "plans/fuzzing.md § 3, P6 : chaque ligne du journal tient dans 600 octets (JOURNAL_LINE de "
                                "host_usb.c)"},
};
const size_t FZ_N_CHECKS = C_N;

int fz_cmd(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return -1;
}

#define MS      1000u
#define T0      (1000u * MS)
#define T_FIN   (12000u * MS)     /* le démarrage regardé : 12 s, au-delà de chaque T_max qui s'y enchaîne sans échec */
#define KEY135  "01030800bf087"   /* test_session_script, KEY135 : la clé du 135 des traces, calculée à la main */
#define FMIN    13878             /* les bornes du 0x06 des traces (13873, 30738), resserrées de 5 (lens_rx.h, [D82]) */
#define FMAX    30733
#define POS0    14623             /* la position du 0x06 des traces */
#define MARQUE  20000

/* T_max de chaque état de séquence, en µs, écrit à la main :
 *   - POWERING : les rails, 50 ms chacun (bsk_phy.h, bsk_phy_lines), les deux attentes de LENS_CS de la poignée de main
 *     (handshake_ms, 500 ms chacune, bsk_session_params_default), plus 100 ms : 1,2 s ;
 *   - IDENTIFYING : l'échéance du 0x10 de l'init (init_home_ms, 8 s), neuf autres requêtes à la plus longue des leurs
 *     (info_ms, 400 ms), le premier 0x05 (first05_ms, 500 ms), la lecture de la marque (500 ms, session.c, MARK_WAIT_MS) :
 *     12,6 s ;
 *   - HOMING : home_ms (20 s) et still_ms (3 s) : 23 s ;
 *   - RESTORING : deux gotos, chacun trois démarrages manqués d'1 s puis 10 s d'immobilité attendue (motion.c, START_MS,
 *     MOVE_STILL_MS), puis un arrêt renvoyé trois fois à 1,5 s (STOP_MS) : 30,5 s ;
 *   - RECOVERING : retry_wait_ms, 3 s, plus 1 ms. */
static const uint64_t TMAX[8] = {0, 1200u * MS, 12600u * MS, 23000u * MS, 30500u * MS, 0, 3001u * MS, 0};

/* La table des transitions de la spec § 3.1, lignes sans HOTE : de -> vers. « tout état hors OFF -> OFF » (la retombée
 * de D2) en plus. */
static bool permise(uint8_t de, uint8_t vers)
{
    if (de != SESSION_OFF && vers == SESSION_OFF) return true;
    switch (de) {
    case SESSION_OFF: return vers == SESSION_POWERING;
    case SESSION_POWERING: return vers == SESSION_IDENTIFYING;
    case SESSION_IDENTIFYING:
        return vers == SESSION_HOMING || vers == SESSION_READY || vers == SESSION_RESTORING || vers == SESSION_RECOVERING ||
               vers == SESSION_FAULT;
    case SESSION_HOMING:
        return vers == SESSION_READY || vers == SESSION_RESTORING || vers == SESSION_FAULT || vers == SESSION_RECOVERING;
    case SESSION_RESTORING: return vers == SESSION_READY || vers == SESSION_RECOVERING || vers == SESSION_FAULT;
    case SESSION_READY: return vers == SESSION_RECOVERING || vers == SESSION_FAULT;
    case SESSION_RECOVERING: return vers == SESSION_POWERING;
    default: return false;
    }
}

/* FNV-1a 32 bits d'après sa définition (h = 2166136261, puis h = (h ^ octet) × 16777619), sur la clé (ses 16 octets),
 * la position (poids faible d'abord) et le sens. */
static uint32_t fnv(const bsk_session_keep_t *k)
{
    uint32_t h = 2166136261u;
    uint8_t b[BSK_STORE_KEY_MAX + 3];
    memcpy(b, k->key, BSK_STORE_KEY_MAX);
    b[BSK_STORE_KEY_MAX] = (uint8_t)k->pos;
    b[BSK_STORE_KEY_MAX + 1] = (uint8_t)(k->pos >> 8);
    b[BSK_STORE_KEY_MAX + 2] = k->dir;
    for (size_t i = 0; i < sizeof b; i++) h = (h ^ b[i]) * 16777619u;
    return h;
}

/* ─────────────────────────── le moteur scripté ─────────────────────────── */

static struct {
    int32_t pos, target;
    bool    run;
} M;

static void set_pos(uint16_t pos)
{
    g_ps.m06[3] = (uint8_t)pos;
    g_ps.m06[4] = (uint8_t)(pos >> 8);
}

/* Le 0x1D du 0x04 (offsets 14-16 du message) ; 250 pas par trame ; 1D 00 derrière le 0x06 de l'arrivée (samyang.md
 * § 4.4), comme motor() de test_session_script avec `ack`. */
static void moteur(const bsk_frame_t *f, uint64_t t)
{
    bool ack = false;
    (void)t;
    if (f->cls == 1 && f->msg[0] == 0x1C) M.run = false;
    if (!(f->cls == 1 && f->msg[0] == 0x04)) return;
    if (f->len == 19 && f->msg[14] == 0x1D) {
        M.target = f->msg[15] | f->msg[16] << 8;
        M.run = M.target != M.pos;
        ack = !M.run;
    } else if (M.run) {
        int32_t d = M.target - M.pos;
        M.pos += d > 250 ? 250 : d < -250 ? -250 : d;
        if (M.pos == M.target) {
            M.run = false;
            ack = true;
        }
    }
    set_pos((uint16_t)M.pos);
    g_ps.n06 = 40;
    if (ack) {
        g_ps.m06[40] = 0x1D;
        g_ps.m06[41] = 0x00;
        g_ps.n06 = 42;
    }
}

static void answer_src(uint8_t type, src_id_t id, uint64_t delay_us)
{
    uint8_t m[BSK_MSG_MAX];
    size_t n = sources135_msg(id, m, sizeof m);
    ps_answer(type, m, n, delay_us);
}

/* ─────────────────────────── le pas, observé ─────────────────────────── */

static unsigned polls;
bool __real_bsk_phy_poll(bsk_phy_event_t *ev);
bool __wrap_bsk_phy_poll(bsk_phy_event_t *ev);
bool __wrap_bsk_phy_poll(bsk_phy_event_t *ev)
{
    polls++;
    return __real_bsk_phy_poll(ev);
}

static fz_ctx_t *C;
static uint8_t   etat;
static uint64_t  depuis;
static bool      vu_restoring;

static void observer(uint64_t t)
{
    bsk_status_t s;
    static char b[4096];
    uint32_t gen;
    size_t n;
    bsk_session_status(&s);
    while ((n = bsk_journal_take(b, sizeof b, &gen)) > 0)
        if (n >= 600) fz_fail(C, C_LIGNE, "t=%llu : une ligne du journal de %zu octets", (unsigned long long)t, n);
    if (s.session_state > SESSION_FAULT) {
        fz_fail(C, C_ETAT, "t=%llu : état %u", (unsigned long long)t, s.session_state);
        return;
    }
    if (s.session_state != etat) {
        fz_tr(C, "t=%llu %s -> %s", (unsigned long long)t, bsk_state_name(etat), bsk_state_name(s.session_state));
        if (!permise(etat, s.session_state))
            fz_fail(C, C_TRANSITION, "t=%llu : %s -> %s", (unsigned long long)t, bsk_state_name(etat),
                    bsk_state_name(s.session_state));
        etat = s.session_state;
        depuis = t;
        if (etat == SESSION_RESTORING) vu_restoring = true;
    }
    if (etat == SESSION_FAULT && s.last_error != E_LOST && s.last_error != E_HOME_FAILED)
        fz_fail(C, C_FAULT, "t=%llu : FAULT sur %s", (unsigned long long)t, bsk_err_token(s.last_error));
    if (TMAX[etat] && t - depuis > TMAX[etat])
        fz_fail(C, C_DUREE, "t=%llu : %s tenu %llu µs", (unsigned long long)t, bsk_state_name(etat),
                (unsigned long long)(t - depuis));
    if ((s.aperture_current && (s.aperture_current < 0x0E00 || s.aperture_current > 0x2000)) ||
        ((s.capabilities & CAP_APERTURE) ? s.aperture_min < 0x0E00 || s.aperture_max > 0x2000 || s.aperture_min > s.aperture_max
                                         : s.aperture_min || s.aperture_max) ||
        (s.limits_valid ? s.focus_min > s.focus_max || !(s.capabilities & CAP_LIMITS_REPORTED) : s.focus_min || s.focus_max) ||
        (s.position_valid && s.session_state != SESSION_READY))
        fz_fail(C, C_INSTANTANE, "t=%llu : état %s ouverture %04X plage %04X..%04X cap %lX bornes %d %ld..%ld position %d",
                (unsigned long long)t, bsk_state_name(s.session_state), s.aperture_current, s.aperture_min, s.aperture_max,
                (unsigned long)s.capabilities, s.limits_valid, (long)s.focus_min, (long)s.focus_max, s.position_valid);
}

static void courir(uint64_t t_fin)
{
    uint64_t prev = UINT64_MAX;
    unsigned meme = 0;
    while (C->check < 0) {
        uint64_t t = ps_next(), ts = bsk_session_next();
        if (ts < t) t = ts;
        if (t > t_fin) t = t_fin;
        ps_run(t);
        polls = 0;
        bsk_session_step(t);
        if (polls > 64) fz_fail(C, C_POLL, "t=%llu : %u bsk_phy_poll dans un pas", (unsigned long long)t, polls);
        observer(t);
        if (t >= t_fin) break;
        meme = t == prev ? meme + 1 : 0;
        prev = t;
        if (meme > 100000) fz_fail(C, C_TEMPS, "le temps n'avance plus à %llu µs", (unsigned long long)t);
    }
}

/* ─────────────────────────── le cas ─────────────────────────── */

void fz_run(fz_ctx_t *c, const uint8_t *d, size_t n)
{
    static bsk_session_keep_t K;
    static bsk_session_params_t P;
    bsk_session_keep_t lu;
    uint8_t flags = n > sizeof K ? d[sizeof K] : 0;
    bool tiede = flags & 1u, relue, cle;
    bsk_status_t s;
    int32_t attendu;
    C = c;
    memset(&K, 0, sizeof K);
    memcpy(&K, d, n < sizeof K ? n : sizeof K);
    lu = K;
    relue = tiede && K.sum == fnv(&K) && K.dir <= 2;
    cle = relue && !memcmp(K.key, KEY135, 13) && K.key[13] == '\0';
    attendu = cle ? (K.pos >= FMIN && K.pos <= FMAX ? K.pos : POS0) : (flags & 2u) ? MARQUE : POS0;

    ps_init(T0);
    store_sim_reset();
    if (flags & 2u) store_sim_poke(KEY135, MARQUE | 1u << 16);
    bsk_session_params_default(&P);
    P.keep = &K;
    P.warm = tiede;
    bsk_journal_init(ps_now);
    bsk_journal_set(true, (flags & 4u) != 0);
    bsk_session_init(&P);
    if (c->trace) {
        char h[2 * sizeof lu.key + 1];
        for (size_t i = 0; i < sizeof lu.key; i++) snprintf(h + 2 * i, 3, "%02X", (unsigned)(uint8_t)lu.key[i]);
        fz_tr(c, "mémoire : clé %s", h);
    }
    fz_tr(c, "mémoire : pos=%u dir=%u somme=%08lX (FNV %08lX), %s, marque %s : %s ; attendu READY en %ld",
          lu.pos, lu.dir, (unsigned long)lu.sum, (unsigned long)fnv(&lu), tiede ? "tiède" : "froid",
          (flags & 2u) ? "rangée" : "absente", relue ? (cle ? "relue, clé du 135" : "relue, autre clé") : "effacée",
          (long)attendu);
    {
        static const bsk_session_keep_t ZERO;
        if (relue ? memcmp(&K, &lu, sizeof K) : memcmp(&K, &ZERO, sizeof K)) {
            fz_fail(c, C_MEMOIRE, "au bsk_session_init : %s attendue, %s", relue ? "relue telle quelle" : "effacée",
                    memcmp(&K, &ZERO, sizeof K) ? "pas à zéro" : "effacée");
            return;
        }
    }
    answer_src(0x01, SRC_R01, 5 * MS);
    answer_src(0x07, SRC_R07, 5 * MS);
    answer_src(0x08, SRC_R08, 5 * MS);
    answer_src(0x0B, SRC_R0B, 5 * MS);
    answer_src(0x09, SRC_R09, 5 * MS);
    answer_src(0x0D, SRC_R0D, 5 * MS);
    answer_src(0x10, SRC_R10, 700 * MS);
    answer_src(0x0A, SRC_R0A, 5 * MS);
    g_ps.n05 = (uint16_t)sources135_msg(SRC_T05, g_ps.m05, sizeof g_ps.m05);
    g_ps.n06 = (uint16_t)sources135_msg(SRC_T06, g_ps.m06, sizeof g_ps.m06);
    g_ps.flow_on_0a = true;
    g_ps.handshake = true;
    g_ps.on_send = moteur;
    memset(&M, 0, sizeof M);
    M.pos = g_ps.m06[3] | g_ps.m06[4] << 8;
    etat = SESSION_OFF;
    depuis = T0;
    vu_restoring = false;
    ps_presence(true);
    courir(T0 + T_FIN);
    if (c->check >= 0) return;
    bsk_session_status(&s);
    fz_tr(c, "fin : %s, position %ld (valide %d), moteur en %ld, %lu écriture(s) au magasin", bsk_state_name(s.session_state),
          (long)s.focus_position, s.position_valid, (long)M.pos, (unsigned long)g_store_sim.writes);
    if (s.session_state != SESSION_READY) fz_fail(c, C_FIN, "à %u s : %s, pas READY", T_FIN / 1000000u, bsk_state_name(s.session_state));
    else if (s.focus_position != attendu || M.pos != attendu)
        fz_fail(c, C_CIBLE, "READY en %ld (le moteur en %ld), attendu %ld (%s)", (long)s.focus_position, (long)M.pos,
                (long)attendu, cle ? "la position de reprise" : (flags & 2u) ? "la marque" : "sans mouvement");
    else if (g_store_sim.writes) fz_fail(c, C_NVS, "%lu écriture(s) déposée(s) sans geste", (unsigned long)g_store_sim.writes);
}

/* ─────────────────────────── le générateur ─────────────────────────── */

size_t fz_gen(fz_rng_t *r, uint8_t *out, size_t cap)
{
    static const uint16_t POS[] = {0, 1, FMIN - 1, FMIN, FMIN + 1, POS0, POS0 + 1, MARQUE, FMAX - 1, FMAX, FMAX + 1, 30738,
                                   65535};
    bsk_session_keep_t k;
    uint32_t q = fz_below(r, 100);
    uint8_t *b = (uint8_t *)&k;
    (void)cap;
    for (size_t i = 0; i < sizeof k; i++) b[i] = (uint8_t)fz_next(r);   /* le remplissage aussi : la RAM non initialisée */
    if (q < 60) {                                       /* la clé du 135, terminée */
        memset(k.key, 0, sizeof k.key);
        memcpy(k.key, KEY135, 13);
        if (fz_chance(r, 1, 4))                         /* des octets après le zéro */
            for (size_t i = 14; i < sizeof k.key; i++) k.key[i] = (char)(1 + fz_below(r, 255));
    } else if (q < 70) {                                /* la clé du 135, non terminée */
        memcpy(k.key, KEY135, 13);
        for (size_t i = 13; i < sizeof k.key; i++) k.key[i] = (char)(1 + fz_below(r, 255));
    } else if (q < 80) {                                /* une autre clé de 13 caractères */
        memset(k.key, 0, sizeof k.key);
        memcpy(k.key, KEY135, 13);
        k.key[fz_below(r, 13)] ^= (char)(1 + fz_below(r, 7));
    }
    k.pos = fz_chance(r, 1, 2) ? POS[fz_below(r, sizeof POS / sizeof POS[0])] : (uint16_t)fz_next(r);
    {
        static const uint8_t HORS[] = {3, 4, 0x7F, 0x80, 0xFF};    /* hors de bsk_approach_t */
        uint32_t qd = fz_below(r, 10);
        k.dir = (uint8_t)(qd < 7 ? fz_below(r, 3) : qd < 9 ? HORS[fz_below(r, sizeof HORS)] : fz_next(r));
    }
    if (fz_chance(r, 3, 4)) k.sum = fnv(&k);            /* la somme juste */
    else if (fz_chance(r, 1, 2)) k.sum = fnv(&k) ^ (1u << fz_below(r, 32));
    memcpy(out, &k, sizeof k);
    out[sizeof k] = (uint8_t)((fz_chance(r, 6, 7) ? 1u : 0u) | (fz_chance(r, 1, 2) ? 2u : 0u) | (fz_chance(r, 1, 4) ? 4u : 0u));
    return sizeof k + 1;
}
