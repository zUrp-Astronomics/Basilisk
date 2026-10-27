/* Les formats du journal (components/journal/journal.c), apport (e) de la relecture extérieure du plan (backlog de
 * l'atelier, rubrique « Fuzzing du firmware ») : chaque fonction publique bsk_journal_* qui écrit une ligne, appelée avec
 * chaque champ à sa borne, sous LOG ON et sous LOG ALL. Propriétés P1 (les sanitizers) et P6. C'est une énumération
 * (commande propre `enum`), pas un tirage : les %s ne reçoivent que des jetons internes, et chaque produit des bornes
 * est joué.
 *
 * Les bornes : les nombres à leurs extrêmes (0, 2^64 − 1 pour un instant, INT32_MIN et INT32_MAX, 0 et 2^32 − 1, 0 et
 * 255) ; chaque valeur de chaque énumération (états, erreurs, sens, résultats) ; les chaînes, chacune de celles que leur
 * producteur réel peut donner, la plus longue comprise :
 *   - `why` du bouton (components/session/cmd.c:126, :322-345) : NULL, chaque bsk_err_token, chaque bsk_state_name,
 *     « nomark » ;
 *   - la clé de la marque (mark.c, key_of) : 13 caractères hexa ; 15 aussi, la place de BSK_STORE_KEY_MAX, au-delà du
 *     producteur (une marge, dite comme telle) ;
 *   - `skip=` (session.c:443, :511 ; restore.c:141) : nomark, storeer, timeout, limit ;
 *   - `end=` (session.c:118 ; restore.c:172-186) : arrived, stall, stop, changed, et chaque bsk_state_name ;
 *   - `* sht40` (host.c:142-160) : NULL, nack, crc, bus, stale.
 *
 * Pour chaque fonction, le rapport donne la plus longue ligne obtenue, contre LINE_MAX (320, le tampon de vline :
 * vsnprintf puis put, journal.c:251-256) ou contre son propre tampon (gesture 112, body08 80, xdetect 48, la ligne
 * `* lost <n>` de bsk_journal_lost 24, m de lost_line), et contre 600, la ligne du transport (JOURNAL_LINE de
 * host_usb.c). Une ligne de vline de 320 octets ou plus serait tronquée par
 * vsnprintf et lue au-delà de son tampon par put : ASan l'arrêterait (P1).
 *
 * La campagne (fz_gen, fz_run) tire les mêmes appels au hasard parmi ces bornes : un cas est une suite d'appels, chacun
 * [fonction][un octet par champ, l'indice de la borne]. */
#define _POSIX_C_SOURCE 200809L

#include <string.h>

#include "bsk_contract.h"
#include "bsk_journal.h"
#include "fuzz.h"

const char   FZ_CIBLE[] = "journal";
const size_t FZ_CAP = 8192;

enum { C_LIGNE, C_TAMPON, C_TRANSPORT, C_N };

const fz_check_t FZ_CHECKS[] = {
    [C_LIGNE] = {"P1", "vline", "journal.c:249-256 : chaque format, ses champs à leur plus long, tient dans LINE_MAX (320) ; "
                                "sinon vsnprintf tronque et put lit au-delà du tampon"},
    [C_TAMPON] = {"P1", "tampon", "journal.c : gesture (112), body08 (80), xdetect (48), lost (24) et chaque morceau "
                                  "d'octets (LINE_MAX, PRE_FITS) tiennent dans leur tampon"},
    [C_TRANSPORT] = {"P6", "transport", "plans/fuzzing.md § 3, P6 : chaque ligne prise par bsk_journal_take tient dans 600 "
                                        "octets (JOURNAL_LINE de host_usb.c)"},
};
const size_t FZ_N_CHECKS = C_N;

/* ─────────────────────────── les bornes ─────────────────────────── */

static const uint64_t T64[] = {0, 999, UINT64_MAX};
static const int32_t  I32[] = {INT32_MIN, -1, 0, INT32_MAX};
static const uint32_t U32[] = {0, UINT32_MAX};
static const uint8_t  U8[] = {0, 0xFF};
static const char *const KEYS[] = {"01030800bf087", "fffffffffffff", "ffffffffffffff0", ""};   /* 13, 13, 15 (marge), vide */
static const char *const SKIP[] = {"nomark", "storeer", "timeout", "limit"};
static const char *const END[] = {"arrived", "stall", "stop", "changed"};
static const char *const SHT[] = {NULL, "nack", "crc", "bus", "stale"};
/* bsk_journal_lost : une écriture ratée, puis 2^24 (le compte sature à LOST_N, 2^24 − 1, journal.c) : le compte de
 * `* lost <n>` à son plus long. */
static const uint32_t LOSTS[] = {1, 1u << 24};

#define N(a) (sizeof(a) / sizeof((a)[0]))
#define N_STATES 8u      /* SESSION_OFF..SESSION_FAULT */
#define N_MOTIONS 8u     /* MOTION_IDLE..MOTION_ABORTED */
#define N_ERRS 16u       /* E_OK..E_DESYNC */

static uint64_t horloge;
static uint64_t lire_horloge(void) { return horloge; }

/* Le `why` du bouton : NULL, 16 jetons d'erreur, 8 noms d'état, « nomark ». */
static const char *why_btn(unsigned i)
{
    if (i == 0) return NULL;
    if (i <= N_ERRS) return bsk_err_token((bsk_err_t)(i - 1));
    if (i <= N_ERRS + N_STATES) return bsk_state_name((uint8_t)(i - 1 - N_ERRS));
    return "nomark";
}
#define N_WHY_BTN (2u + N_ERRS + N_STATES)

static const char *why_end(unsigned i) { return i < N(END) ? END[i] : bsk_state_name((uint8_t)(i - N(END))); }
#define N_WHY_END (N(END) + N_STATES)

/* ─────────────────────────── les fonctions ─────────────────────────── */

/* Chaque fonction : son nom, le nombre de bornes de chacun de ses champs (le premier : LOG ON, LOG ALL ; le second :
 * l'horloge ou l'instant), et le tampon contre lequel ses lignes se mesurent. */
typedef struct {
    const char *nom;
    unsigned    nb[9];
    unsigned    k;
    unsigned    tampon;   /* LINE_MAX pour vline et les morceaux ; le tampon propre sinon */
} fn_t;

enum { F_RX, F_TX, F_REFUSED, F_ERROR, F_SESSION, F_HANDSHAKE, F_MOTION, F_LENS, F_RING, F_GESTURE, F_BODY08, F_BTN, F_MARK,
       F_RESTORE, F_SKIP, F_END, F_XDETECT, F_BOUNCE, F_RAIL, F_SHT40, F_RESEND, F_GIVEUP, F_LOST, F_N };

/* Pour une trame : la longueur du message, le remplissage, la classe et la séquence, le type, une seconde trame qui
 * diffère d'un octet sur deux (la forme différence de LOG ALL). */
static const uint16_t FLEN[] = {1, 64, 65, 128, BSK_MSG_MAX};
static const uint8_t  FTYPE[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x0A, 0xFF};

static const fn_t FN[F_N] = {
    [F_RX] = {"bsk_journal_rx", {2, N(T64), N(FLEN), 2, 2, N(FTYPE), 2}, 7, 320},
    [F_TX] = {"bsk_journal_tx", {2, N(T64), N(FLEN), 2, 2, N(FTYPE), 2}, 7, 320},
    [F_REFUSED] = {"bsk_journal_refused", {2, N(T64), N(FLEN), 2, 2}, 5, 320},
    [F_ERROR] = {"bsk_journal_error", {2, N(T64), N_ERRS, 5, 2}, 5, 320},
    [F_SESSION] = {"bsk_journal_session", {2, N(T64), N_STATES, N_ERRS}, 4, 320},
    [F_HANDSHAKE] = {"bsk_journal_handshake", {2, N(T64), 3, N(T64)}, 4, 320},
    [F_MOTION] = {"bsk_journal_motion", {2, N(T64), N_MOTIONS}, 3, 320},
    [F_LENS] = {"bsk_journal_lens", {2, N(T64), N(U8), N(U8)}, 4, 320},
    [F_RING] = {"bsk_journal_ring", {2, N(T64), 2}, 3, 320},
    [F_GESTURE] = {"bsk_journal_gesture", {2, N(T64), N(I32), N(U32), N(U32), N(U32)}, 6, 112},
    [F_BODY08] = {"bsk_journal_body08", {2, N(T64), N(U8), 2}, 4, 80},
    [F_BTN] = {"bsk_journal_btn", {2, N(T64), 3, 3, N_WHY_BTN}, 5, 320},
    [F_MARK] = {"bsk_journal_mark", {2, N(T64), 4, N(KEYS), 2, N(I32), 3}, 7, 320},
    [F_RESTORE] = {"bsk_journal_restore", {2, N(T64), N(I32), 3, 2, N(I32), 1 + N(I32)}, 7, 320},
    [F_SKIP] = {"bsk_journal_restore_skip", {2, N(T64), N(SKIP), 2, 2, N(I32)}, 6, 320},
    [F_END] = {"bsk_journal_restore_end", {2, N(T64), N_WHY_END}, 3, 320},
    [F_XDETECT] = {"bsk_journal_xdetect", {2, N(T64), 2}, 3, 48},
    [F_BOUNCE] = {"bsk_journal_xdetect_bounce", {2, N(T64)}, 2, 320},
    [F_RAIL] = {"bsk_journal_rail_fault", {2, N(T64), 2, 2, 2}, 5, 320},
    [F_SHT40] = {"bsk_journal_sht40", {2, N(T64), N(SHT)}, 3, 320},
    [F_RESEND] = {"bsk_journal_resend", {2, N(T64), N(U8), N(U8), N_ERRS}, 5, 320},
    [F_GIVEUP] = {"bsk_journal_giveup", {2, N(T64), N(U8), N_ERRS}, 4, 320},
    [F_LOST] = {"bsk_journal_lost", {2, 1, N(LOSTS)}, 3, 24},   /* sans instant ; m[24] de lost_line, journal.c */
};

static bsk_frame_t trame(const unsigned *v, bool autre)
{
    bsk_frame_t f;
    memset(&f, 0, sizeof f);
    f.len = FLEN[v[2]];
    f.cls = v[3] ? 3 : 1;
    f.seq = v[3] ? 0xFF : 0;
    for (size_t i = 0; i < f.len; i++) f.msg[i] = v[4] ? 0xFF : 0x00;
    if (autre)
        for (size_t i = 1; i < f.len; i += 2) f.msg[i] ^= 0x5A;
    return f;
}

/* L'appel de la fonction `f` aux bornes `v` (v[0] : LOG ALL ; v[1] : l'horloge ou l'instant). */
static void appeler(unsigned f, const unsigned *v)
{
    uint64_t t = T64[v[1]];
    bsk_mark_t m;
    horloge = t;
    bsk_journal_set(true, v[0] != 0);
    switch (f) {
    case F_RX:
    case F_TX: {
        bsk_frame_t a = trame(v, false), b;
        a.msg[0] = FTYPE[v[5]];
        b = trame(v, true);
        b.msg[0] = a.msg[0];
        b.seq = (uint8_t)(a.seq + 7);
        if (f == F_RX) bsk_journal_rx(&a, t);
        else bsk_journal_tx(&a);
        if (v[6] && f == F_RX) bsk_journal_rx(&b, t);
        else if (v[6]) bsk_journal_tx(&b);
        break;
    }
    case F_REFUSED: {
        bsk_frame_t a = trame(v, false);
        bsk_journal_refused(&a);
        break;
    }
    case F_ERROR: {
        static const uint16_t RN[] = {0, 1, 256, 257, 65535};
        bsk_phy_raw_t r;
        r.n = RN[v[3]];
        r.len = r.n < 256 ? r.n : 256;
        memset(r.b, v[4] ? 0xFF : 0, sizeof r.b);
        bsk_journal_error((bsk_err_t)v[2], &r, t);
        break;
    }
    case F_SESSION: bsk_journal_session((uint8_t)v[2], (bsk_err_t)v[3]); break;
    case F_HANDSHAKE: bsk_journal_handshake((bsk_hs_t)v[2], T64[v[3]]); break;
    case F_MOTION: bsk_journal_motion((uint8_t)v[2]); break;
    case F_LENS: bsk_journal_lens(U8[v[2]], U8[v[3]]); break;
    case F_RING: bsk_journal_ring(v[2] != 0); break;
    case F_GESTURE: bsk_journal_gesture(I32[v[2]], U32[v[3]], U32[v[4]], U32[v[5]]); break;
    case F_BODY08: bsk_journal_body08(U8[v[2]], v[3] != 0); break;
    case F_BTN: bsk_journal_btn((bsk_btn_t)v[2], (bsk_btn_result_t)v[3], why_btn(v[4])); break;
    case F_MARK:
        m = (bsk_mark_t){.position = I32[v[5]], .approach_dir = (bsk_approach_t)v[6]};
        bsk_journal_mark((bsk_mark_ev_t)v[2], KEYS[v[3]], v[4] ? &m : NULL);
        break;
    case F_RESTORE:
        m = (bsk_mark_t){.position = I32[v[2]], .approach_dir = (bsk_approach_t)v[3]};
        bsk_journal_restore(&m, v[4] != 0, I32[v[5]], v[6] ? I32[v[6] - 1] : -1);
        break;
    case F_SKIP:
        m = (bsk_mark_t){.position = I32[v[5]], .approach_dir = BSK_APPROACH_UNKNOWN};
        bsk_journal_restore_skip(SKIP[v[2]], v[3] != 0, v[4] ? &m : NULL);
        break;
    case F_END: bsk_journal_restore_end(why_end(v[2])); break;
    case F_XDETECT: bsk_journal_xdetect(v[2] != 0, t); break;
    case F_BOUNCE: bsk_journal_xdetect_bounce(t); break;
    case F_RAIL: bsk_journal_rail_fault(v[2] ? BSK_RAIL_MOTOR : BSK_RAIL_LOGIC, v[3] != 0, v[4] != 0, t); break;
    case F_SHT40: bsk_journal_sht40(SHT[v[2]]); break;
    case F_RESEND: bsk_journal_resend(U8[v[2]], U8[v[3]], (bsk_err_t)v[4]); break;
    case F_GIVEUP: bsk_journal_giveup(U8[v[2]], (bsk_err_t)v[3]); break;
    case F_LOST:                                    /* la ligne `* lost <n>`, sous LOG ALL seulement (bsk_journal.h) */
        for (uint32_t k = 0; k < LOSTS[v[2]]; k++) bsk_journal_lost();
        break;
    default: break;
    }
}

/* Les lignes que l'appel a produites, prises toutes ; la plus longue rendue, jugée. */
static size_t prendre(fz_ctx_t *c, unsigned f, size_t *nl)
{
    static char b[4096];
    uint32_t gen;
    size_t n, max = 0;
    *nl = 0;
    while ((n = bsk_journal_take(b, sizeof b, &gen)) > 0) {
        (*nl)++;
        if (n > max) max = n;
        b[n < sizeof b ? n : sizeof b - 1] = '\0';
        fz_tr(c, "  %zu : %s", n, b);
        if (n >= 600) fz_fail(c, C_TRANSPORT, "%s : une ligne de %zu octets", FN[f].nom, n);
        else if (n >= FN[f].tampon) fz_fail(c, FN[f].tampon == 320 ? C_LIGNE : C_TAMPON, "%s : une ligne de %zu octets, "
                                            "tampon de %u", FN[f].nom, n, FN[f].tampon);
    }
    return max;
}

static void initialiser(void)
{
    bsk_journal_init(lire_horloge);
    bsk_journal_set(false, false);
    {
        static char b[4096];
        uint32_t gen;
        while (bsk_journal_take(b, sizeof b, &gen)) {}
    }
}

/* ─────────────────────────── la campagne ─────────────────────────── */

void fz_run(fz_ctx_t *c, const uint8_t *d, size_t n)
{
    size_t i = 0;
    initialiser();
    while (i < n && c->check < 0) {
        unsigned f = d[i++] % F_N, v[9] = {0}, k;
        size_t nl;
        for (k = 0; k < FN[f].k; k++) v[k] = (i < n ? d[i++] : 0) % FN[f].nb[k];
        fz_tr(c, "%s", FN[f].nom);
        appeler(f, v);
        (void)prendre(c, f, &nl);
    }
}

size_t fz_gen(fz_rng_t *r, uint8_t *out, size_t cap)
{
    uint32_t nc = 1 + fz_below(r, 40);
    size_t n = 0;
    for (uint32_t j = 0; j < nc && n + 10 <= cap; j++) {
        unsigned f = fz_below(r, F_N);
        out[n++] = (uint8_t)f;
        for (unsigned k = 0; k < FN[f].k; k++)    /* les 2^24 appels de bsk_journal_lost : l'énumération seule */
            out[n++] = (uint8_t)(f == F_LOST && k == 2 ? 0 : fz_below(r, FN[f].nb[k]));
    }
    return n;
}

/* ─────────────────────────── l'énumération ─────────────────────────── */

int fz_cmd(int argc, char **argv)
{
    fz_ctx_t c = {.check = -1};
    unsigned long total = 0;
    if (argc != 1 || strcmp(argv[0], "enum")) return -1;
    initialiser();
    printf("%-28s %8s %7s %9s %8s\n", "fonction", "appels", "lignes", "plus_long", "tampon");
    for (unsigned f = 0; f < F_N; f++) {
        unsigned v[9] = {0};
        unsigned long appels = 0, lignes = 0;
        size_t max = 0;
        for (;;) {
            size_t nl, m;
            unsigned k;
            appeler(f, v);
            m = prendre(&c, f, &nl);
            if (m > max) max = m;
            lignes += nl;
            appels++;
            for (k = 0; k < FN[f].k && ++v[k] == FN[f].nb[k]; k++) v[k] = 0;
            if (k == FN[f].k) break;
        }
        total += appels;
        printf("%-28s %8lu %7lu %9zu %8u\n", FN[f].nom, appels, lignes, max, FN[f].tampon);
    }
    if (c.check >= 0) printf("ECHEC %s %s : %s\n", FZ_CHECKS[c.check].prop, FZ_CHECKS[c.check].nom, c.why);
    printf("BILAN journal enum : %lu appels, %s\n", total, c.check < 0 ? "chaque ligne dans son tampon" : "un écart");
    return c.check < 0 ? 0 : 1;
}
