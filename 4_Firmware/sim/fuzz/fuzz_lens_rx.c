/* Cible 2 de plans/fuzzing.md (l'atelier) : la lecture des réponses, components/session/lens_rx.c — lens_rx_frame,
 * lens_rx_loop, lens_rx_stale, lens_rx_forget, et les lectures de l'init (lens_rx_full, puis lens_rx_id, lens_rx_name,
 * lens_rx_home_failed). Propriétés P1 (les sanitizers, et le contre-oracle des octets au-delà de len) et P4.
 *
 * Le contrat des appelants est tenu : une trame vient de PHY, len de 1 à BSK_MSG_MAX (bsk_phy.h, R1) ; lens_rx_id,
 * lens_rx_name et lens_rx_home_failed ne lisent qu'une réponse de leur type que lens_rx_full a jugée complète
 * (components/session/init.c, init_reply ; session.c, la réponse au 0x10 du pilote) : le type de la réponse est celui de
 * la requête, TRANSACTION la termine sur son premier octet.
 *
 * Un cas : une suite d'opérations, chacune [op][len][octets]. op % 8 : 0 à 2 une trame (lens_rx_frame), 3 lens_rx_loop,
 * 4 lens_rx_stale(0x05), 5 lens_rx_stale(0x06), 6 une réponse de l'init (len % 3 : 0x07, 0x3F, 0x10 ; son type posé),
 * 7 lens_rx_forget. Pour une trame ou une réponse, l'octet suivant op est sa longueur moins un, prise modulo BSK_MSG_MAX,
 * et viennent ensuite ses octets, type compris (octets manquants : 0).
 *
 * Le contre-oracle de P1 : ASan ne voit pas une lecture au-delà de len dans msg[], un tableau de la structure
 * (bsk_phy.h). Le cas est donc joué trois fois, les octets de msg[] au-delà de len à 0x00, à 0xFF, puis tirés : l'état
 * publié et chaque valeur rendue doivent être les mêmes après chaque opération. */
#define _POSIX_C_SOURCE 200809L

#include <string.h>

#include "../../components/session/lens_rx.h"
#include "fuzz.h"
#include "sources135.h"

const char   FZ_CIBLE[] = "lens_rx";
const size_t FZ_CAP = 32768;

enum { C_AP, C_PLAGE, C_LIM, C_POS, C_DRAP05, C_ID, C_NOM, C_AVANT, C_HORS_INIT, C_AU_DELA, C_N };

const fz_check_t FZ_CHECKS[] = {
    [C_AP] = {"P4", "ouverture", "lens_rx.h : un code d'ouverture publié est dans BSK_AP_CODE_MIN..MAX (0x0E00..0x2000, "
                                 "bsk_contract.h, écrits ici à la main) ou vaut 0 ; spec [D129] « publiée inconnue, jamais "
                                 "comme valide »"},
    [C_PLAGE] = {"P4", "plage", "lens_rx.h : have08, ses deux codes dans le domaine, le min au plus le max ; ap_min, ap_max "
                                "à 0 sans have08"},
    [C_LIM] = {"P4", "bornes", "lens_rx.h : lim implique have06 et un intervalle non vide (fmin ≤ fmax) ; fmin, fmax à 0 "
                               "sans lim"},
    [C_POS] = {"P4", "position", "lens_rx.h : pos à 0 sans 0x06 (have06) ; lens_rx_stale(0x06) : have06 et lim faux"},
    [C_DRAP05] = {"P4", "drapeaux05", "lens_rx.h : have62 et have64 oubliés avec have05 ; sans 0x05 lu, l'ouverture, les "
                                      "offsets 60 et 62 à 0 (lens_rx_stale) ; have07 implique have_id, lens_type2 à 0 "
                                      "sans have07"},
    [C_ID] = {"P4", "identite", "lens_rx.h : un 0x07 reçu hors de la réponse de l'init n'est pas lu, il ne change ni "
                                "l'identité publiée ni la clé de la marque (have_id, id, have07, lens_type2)"},
    [C_NOM] = {"P4", "nom", "lens_rx.c, name_3f : le nom publié est vide ou fait de 1 à 64 octets 0x20..0x7E sans `\"`, "
                            "sans espace de fin ; lens_rx_name rend vrai si et seulement s'il n'est pas vide"},
    [C_AVANT] = {"P4", "avant_boucle", "lens_rx.h : un 0x05 ou un 0x06 avant lens_rx_loop, ou trop court pour être lu (29 "
                                       "octets, 12), rend 0 et « ne change aucun champ publié » ([D89])"},
    [C_HORS_INIT] = {"P4", "rendu", "lens_rx.h : lens_rx_frame rend le type de son premier message, ou 0 pour un 0x05 ou un "
                                    "0x06 qui n'est pas lu"},
    [C_AU_DELA] = {"P1", "au_dela", "plans/fuzzing.md § 3, P1, contre-oracle : la même trame, octets au-delà de len "
                                    "changés, donne la même publication (bsk_phy.h : msg[] est un tableau de la structure)"},
};
const size_t FZ_N_CHECKS = C_N;

int fz_cmd(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return -1;
}

#define AP_MIN 0x0E00u   /* bsk_contract.h, BSK_AP_CODE_MIN, écrit à la main */
#define AP_MAX 0x2000u   /* BSK_AP_CODE_MAX */

static bool ap_ok(uint16_t v) { return v == 0 || (v >= AP_MIN && v <= AP_MAX); }

/* Les invariants de l'état publié (P4). */
static void invariants(fz_ctx_t *c, const lens_rx_t *l, size_t op)
{
    size_t k;
    if (!ap_ok(l->ap) || !ap_ok(l->ap_ring) || !ap_ok(l->ap_min) || !ap_ok(l->ap_max)) {
        fz_fail(c, C_AP, "op %zu : ap=%04X ap_ring=%04X ap_min=%04X ap_max=%04X", op, l->ap, l->ap_ring, l->ap_min, l->ap_max);
        return;
    }
    if (l->have08 ? !l->ap_min || !l->ap_max || l->ap_min > l->ap_max : l->ap_min || l->ap_max) {
        fz_fail(c, C_PLAGE, "op %zu : have08=%d ap_min=%04X ap_max=%04X", op, l->have08, l->ap_min, l->ap_max);
        return;
    }
    if (l->lim ? !l->have06 || l->fmin > l->fmax : l->fmin || l->fmax) {
        fz_fail(c, C_LIM, "op %zu : lim=%d have06=%d fmin=%ld fmax=%ld", op, l->lim, l->have06, (long)l->fmin, (long)l->fmax);
        return;
    }
    if (!l->have06 && (l->pos || l->lim)) {
        fz_fail(c, C_POS, "op %zu : sans have06, pos=%u lim=%d", op, l->pos, l->lim);
        return;
    }
    if ((!l->have05 && (l->have62 || l->have64 || l->ap || l->move || l->o62)) || (l->have07 && !l->have_id) ||
        (!l->have07 && l->lens_type2)) {
        fz_fail(c, C_DRAP05, "op %zu : have05=%d have62=%d have64=%d ap=%04X move=%u o62=%u have07=%d have_id=%d type2=%u", op,
                l->have05, l->have62, l->have64, l->ap, l->move, l->o62, l->have07, l->have_id, l->lens_type2);
        return;
    }
    k = strnlen(l->name, sizeof l->name);
    if (k > 64 || (k && l->name[k - 1] == ' ')) {
        fz_fail(c, C_NOM, "op %zu : nom de %zu octets, ou finissant par une espace", op, k);
        return;
    }
    for (size_t i = 0; i < k; i++)
        if (l->name[i] < 0x20 || l->name[i] > 0x7E || l->name[i] == '"') {
            fz_fail(c, C_NOM, "op %zu : octet %02X à la place %zu du nom", op, (unsigned)(uint8_t)l->name[i], i);
            return;
        }
}

/* ─────────────────────────── le cas joué ─────────────────────────── */

#define OPS_MAX 4096u
static lens_rx_t obs[3][OPS_MAX];
static int       rendu[3][OPS_MAX];

static uint8_t at(const uint8_t *d, size_t n, size_t i) { return i < n ? d[i] : 0; }

/* Les champs publiés, ack1d et ack1c compris ; seul le nom est comparé jusqu'à son zéro. */
static bool meme(const lens_rx_t *a, const lens_rx_t *b)
{
    lens_rx_t x = *a, y = *b;
    memset(x.name, 0, sizeof x.name);
    memset(y.name, 0, sizeof y.name);
    memcpy(x.name, a->name, strnlen(a->name, sizeof a->name));
    memcpy(y.name, b->name, strnlen(b->name, sizeof b->name));
    return !memcmp(&x, &y, sizeof x);
}

static void jouer(fz_ctx_t *c, const uint8_t *d, size_t n, int jeu)
{
    fz_rng_t rr = {0x5EEDu};
    size_t i = 0, op = 0;
    lens_rx_forget();
    while (i < n && op < OPS_MAX && c->check < 0) {
        uint8_t o = d[i++] & 7u;
        lens_rx_t avant = *lens_rx();
        int r = -1;
        if (o <= 2 || o == 6) {
            bsk_frame_t f;
            size_t len = 1u + at(d, n, i) % BSK_MSG_MAX;
            uint8_t sub = at(d, n, i) % 3u;
            i++;
            f.cls = 1;
            f.seq = 0;
            f.len = (uint16_t)len;
            for (size_t k = 0; k < len; k++) f.msg[k] = at(d, n, i + k);
            for (size_t k = len; k < BSK_MSG_MAX; k++) f.msg[k] = jeu == 0 ? 0x00 : jeu == 1 ? 0xFF : (uint8_t)fz_next(&rr);
            i += len;
            if (o <= 2) {
                r = lens_rx_frame(&f);
                if (jeu == 0) fz_tr(c, "op %zu trame type=%02X len=%zu -> %d", op, f.msg[0], len, r);
                if (r != 0 && r != f.msg[0])
                    fz_fail(c, C_HORS_INIT, "op %zu : la trame de type %02X rend %d", op, f.msg[0], r);
                else if (r == 0 && f.msg[0] != 0x05 && f.msg[0] != 0x06 && f.msg[0] != 0)
                    fz_fail(c, C_HORS_INIT, "op %zu : une trame de type %02X rend 0", op, f.msg[0]);
                else if (r == 0 && (f.msg[0] == 0x05 || f.msg[0] == 0x06) && !meme(&avant, lens_rx()))
                    fz_fail(c, C_AVANT, "op %zu : un %02X de %zu octets non lu (rendu 0) change l'état publié (ack1d %d -> "
                                        "%d, ack1c %d -> %d)",
                            op, f.msg[0], len, avant.ack1d, lens_rx()->ack1d, avant.ack1c, lens_rx()->ack1c);
                else if (f.msg[0] == 0x07 &&
                         (avant.have_id != lens_rx()->have_id || memcmp(avant.id, lens_rx()->id, sizeof avant.id) ||
                          avant.have07 != lens_rx()->have07 || avant.lens_type2 != lens_rx()->lens_type2))
                    fz_fail(c, C_ID, "op %zu : un 0x07 de %zu octets hors de l'init change l'identité", op, len);
            } else {
                static const uint8_t TYPE[3] = {0x07, 0x3F, 0x10};
                f.msg[0] = TYPE[sub];
                if (lens_rx_full(&f)) {
                    if (sub == 0) lens_rx_id(&f);
                    else if (sub == 1) r = lens_rx_name(&f);
                    else r = lens_rx_home_failed(&f);
                    if (sub == 1 && (r != 0) != (lens_rx()->name[0] != '\0'))
                        fz_fail(c, C_NOM, "op %zu : lens_rx_name rend %d, nom de %zu octets", op, r, strlen(lens_rx()->name));
                } else {
                    r = -2;
                }
                if (jeu == 0) fz_tr(c, "op %zu réponse de l'init type=%02X len=%zu -> %d", op, f.msg[0], len, r);
            }
        } else if (o == 3) {
            lens_rx_loop();
            if (jeu == 0) fz_tr(c, "op %zu lens_rx_loop", op);
        } else if (o == 4 || o == 5) {
            lens_rx_stale(o == 4 ? 0x05 : 0x06);
            if (jeu == 0) fz_tr(c, "op %zu lens_rx_stale(%s)", op, o == 4 ? "0x05" : "0x06");
        } else {
            lens_rx_forget();
            if (jeu == 0) fz_tr(c, "op %zu lens_rx_forget", op);
        }
        if (c->check < 0) invariants(c, lens_rx(), op);
        obs[jeu][op] = *lens_rx();
        rendu[jeu][op] = r;
        if (jeu > 0 && c->check < 0 && (!meme(&obs[0][op], &obs[jeu][op]) || rendu[0][op] != rendu[jeu][op]))
            fz_fail(c, C_AU_DELA, "op %zu : octets au-delà de len à %s : publication ou valeur rendue différente (%d, %d)", op,
                    jeu == 1 ? "0xFF" : "tirés", rendu[0][op], rendu[jeu][op]);
        op++;
    }
    if (jeu == 0 && c->check < 0) {
        const lens_rx_t *l = lens_rx();
        fz_tr(c, "fin : have05=%d have06=%d have07=%d have08=%d ap=%04X ap_ring=%04X plage=%04X..%04X pos=%u lim=%d "
                 "fmin=%ld fmax=%ld rx06=%lu ack1d=%d ack1c=%d id=%02X%02X%02X%02X%02X nom=\"%s\"",
              l->have05, l->have06, l->have07, l->have08, l->ap, l->ap_ring, l->ap_min, l->ap_max, l->pos, l->lim,
              (long)l->fmin, (long)l->fmax, (unsigned long)l->rx06, l->ack1d, l->ack1c, l->id[0], l->id[1], l->id[2],
              l->id[3], l->id[4], l->name);
    }
}

void fz_run(fz_ctx_t *c, const uint8_t *d, size_t n)
{
    for (int jeu = 0; jeu < 3 && c->check < 0; jeu++) jouer(c, d, n, jeu);
}

/* ─────────────────────────── le générateur ─────────────────────────── */

static size_t pick(fz_rng_t *r, const uint16_t *v, size_t nv) { return v[fz_below(r, (uint32_t)nv)]; }

static void put16(uint8_t *m, size_t off, uint16_t v)
{
    m[off] = (uint8_t)v;
    m[off + 1] = (uint8_t)(v >> 8);
}

static uint16_t bord16(fz_rng_t *r)
{
    static const uint16_t B[] = {0, 1, 4, 5, 6, 0x0DFF, 0x0E00, 0x0E01, 0x1FFF, 0x2000, 0x2001, 13873, 30738,
                                 65529, 65530, 65531, 65535};
    return fz_chance(r, 3, 4) ? B[fz_below(r, sizeof B / sizeof B[0])] : (uint16_t)fz_next(r);
}

/* Le message d'une trame dans m (type compris), rend sa longueur. */
static size_t message(fz_rng_t *r, uint8_t *m)
{
    static const uint16_t L05[] = {1, 2, 28, 29, 30, 61, 62, 63, 64, 65, 66, 67, 68, 70, BSK_MSG_MAX};
    static const uint16_t L06[] = {1, 11, 12, 13, 39, 40, 41, 42, 43, 44, 46, BSK_MSG_MAX};
    static const uint16_t L08[] = {1, 2, 3, 5, 6, 7, 202};
    static const uint16_t L07[] = {1, 2, 3, 4, 5, 11, 12, 13, 35};
    static const uint16_t L3F[] = {1, 2, 3, 10, 65, 66, 67, BSK_MSG_MAX};
    static const uint8_t TYPES[] = {0x05, 0x06, 0x08, 0x07, 0x3F, 0x10, 0x02, 0x40};
    uint8_t t = fz_chance(r, 9, 10) ? TYPES[fz_below(r, sizeof TYPES)] : (uint8_t)fz_next(r);
    size_t n;
    if (fz_chance(r, 1, 5)) {                                   /* un message des traces du 135 (sim/sources135.c) */
        static const src_id_t S[] = {SRC_T05, SRC_T06, SRC_R07, SRC_R08, SRC_R10, SRC_R01, SRC_R40_V};
        n = sources135_msg(S[fz_below(r, sizeof S / sizeof S[0])], m, BSK_MSG_MAX);
        if (n) {
            if (fz_chance(r, 1, 2)) n = 1 + fz_below(r, (uint32_t)n);
            for (uint32_t k = fz_below(r, 4); k; k--) m[fz_below(r, (uint32_t)n)] = (uint8_t)fz_next(r);
            return n;
        }
    }
    n = t == 0x05 ? pick(r, L05, sizeof L05 / 2) : t == 0x06 ? pick(r, L06, sizeof L06 / 2) : t == 0x08 ? pick(r, L08, sizeof L08 / 2)
      : t == 0x07 ? pick(r, L07, sizeof L07 / 2) : t == 0x3F ? pick(r, L3F, sizeof L3F / 2) : 1 + fz_below(r, BSK_MSG_MAX);
    if (fz_chance(r, 1, 6)) n = 1 + fz_below(r, BSK_MSG_MAX);
    for (size_t k = 0; k < n; k++) m[k] = (uint8_t)fz_next(r);
    m[0] = t;
    if (t == 0x05 && n >= 21) {
        put16(m, 1, bord16(r));
        put16(m, 18, bord16(r));
        m[20] = (uint8_t)fz_below(r, 4);
    }
    if (t == 0x06 && n >= 12) {
        put16(m, 3, bord16(r));
        put16(m, 8, bord16(r));
        put16(m, 10, bord16(r));
        if (fz_chance(r, 1, 4)) {                               /* des bornes qui, resserrées de 5 et 5, se croisent ou non */
            uint16_t lo = (uint16_t)fz_next(r);
            put16(m, 8, lo);
            put16(m, 10, (uint16_t)(lo + 8 + fz_below(r, 5)));
        }
        for (size_t k = 40; k + 1 < n; k += 2) {
            uint32_t q = fz_below(r, 4);
            m[k] = q == 0 ? 0x1D : q == 1 ? 0x1C : m[k];
        }
    }
    if (t == 0x08 && n >= 5) {
        put16(m, 1, bord16(r));
        put16(m, 3, bord16(r));
    }
    if (t == 0x3F) {
        for (size_t k = 2; k < n; k++) {
            uint32_t q = fz_below(r, 40);
            m[k] = q == 0 ? '"' : q == 1 ? 0x7F : q == 2 ? 0x1F : q == 3 ? 0 : q < 10 ? ' ' : (uint8_t)(0x20 + fz_below(r, 0x5F));
        }
    }
    return n;
}

size_t fz_gen(fz_rng_t *r, uint8_t *out, size_t cap)
{
    size_t n = 0;
    uint32_t nops = 1 + fz_below(r, 48);
    for (uint32_t k = 0; k < nops && n + 2 + BSK_MSG_MAX <= cap; k++) {
        uint32_t q = fz_below(r, 100);
        uint8_t o = q < 55 ? (uint8_t)fz_below(r, 3) : q < 70 ? 3 : q < 76 ? 4 : q < 82 ? 5 : q < 96 ? 6 : 7;
        out[n++] = (uint8_t)(o | (uint8_t)fz_below(r, 32) << 3);
        if (o <= 2 || o == 6) {
            uint8_t m[BSK_MSG_MAX] = {0};
            size_t len = message(r, m);
            if (o == 6) {                                       /* la sous-opération : len % 3 */
                uint32_t sub = fz_below(r, 3);
                static const uint16_t LI[3][6] = {{1, 3, 4, 5, 11, 12}, {1, 2, 3, 10, 66, 67}, {1, 2, 3, 2, 2, 5}};
                len = LI[sub][fz_below(r, 6)];
                while ((len - 1) % 3 != sub) len++;
            }
            out[n++] = (uint8_t)(len - 1);
            memcpy(out + n, m, len);
            n += len;
        }
    }
    return n;
}
