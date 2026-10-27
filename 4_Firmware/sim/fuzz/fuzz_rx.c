/* Cible 1 de plans/fuzzing.md (l'atelier) : la réception en flux de components/phy/phy_common.c — rx_got, rx_next,
 * rx_deadline, rx_flush, rx_clear, et par elles fr_scan et fr_decode. Propriétés P1 (les sanitizers), P2 et P9.
 *
 * Le contrat de l'appelant (plans/fuzzing.md § 2, cible 1 ; components/phy/phy.c, on_uart, drain, bus_error) est tenu :
 *   - les octets arrivent par blocs, `k ≤ RX_CAP − n`, écrits à r->b + r->n puis rx_got, à l'instant de leur lecture ;
 *     après chaque bloc, rx_next est rappelé tant qu'il rend true, à ce même instant ;
 *   - la minuterie du début de trame périmé juge à rx_deadline (phy.c, l'échéance de la tâche) ;
 *   - une E_BUS vide le flux (rx_flush), la fermeture de la fenêtre d'écoute le vide sans rien rapporter (rx_clear) ;
 *   - les instants ne décroissent pas.
 *
 * Un cas : 8 octets, la graine du découpage B ; puis des enregistrements, chacun [tag][dt sur 18 bits (3 octets)] et,
 * pour une rafale, [longueur sur 13 bits (2 octets)][octets]. tag % 4 : 0 une rafale d'octets reçus à l'instant t + dt,
 * 1 une E_BUS (rx_flush), 2 la fenêtre fermée (rx_clear), 3 un jugement de plus (une échéance qui sonne). Des octets
 * manquants en fin de cas valent 0.
 *
 * Le même cas est joué trois fois : A, chaque rafale en un bloc (la place le permettant) ; B, en blocs de tailles tirées
 * de la graine du cas ; C, octet par octet (sim/phy_sim.c). Chaque jeu est jugé seul (P9 : chaque octet rapporté une
 * fois, chaque trame relue à la main), puis les trois sont comparés (P9 : le découpage n'y change rien). */
#define _POSIX_C_SOURCE 200809L

#include <string.h>

#include "fuzz.h"
#include "phy_common.h"
#include "sources135.h"

const char   FZ_CIBLE[] = "rx";
const size_t FZ_CAP = 65536;

enum { C_KIND, C_TRAME, C_ECART, C_RELEVE, C_BUS, C_FIN, C_BOUCLE, C_PLACE, C_DECOUPE, C_N };

const fz_check_t FZ_CHECKS[] = {
    [C_KIND] = {"P9", "nature", "bsk_phy.h, R1 : au-dessus de PHY ne remontent qu'une trame ou une erreur nommée ; le flux "
                                "ne rend que BSK_PHY_FRAME ou BSK_PHY_ERROR E_FRAMING (phy_common.h, rx_next)"},
    [C_TRAME] = {"P9", "trame", "bsk_phy.h, en tête : F0, longueur u16 de toute la trame (9 à 255), classe 1 à 3, somme des "
                                "octets 1 à longueur - 4, 55 ; ce sont les octets reçus à cette place du flux"},
    [C_ECART] = {"P9", "ecart", "bsk_phy.h, bsk_phy_raw_t : une E_FRAMING porte une suite d'octets écartés d'un seul tenant, "
                                "dans l'ordre du flux ; un octet n'est rapporté qu'une fois"},
    [C_RELEVE] = {"P9", "releve", "bsk_phy.h, bsk_phy_raw_t : b porte les len premiers des n octets, au plus 256 ; une "
                                  "E_FRAMING en porte au moins un"},
    [C_BUS] = {"P9", "bus", "phy_common.h, rx_flush : l'E_BUS porte les octets reçus pas encore rapportés, dans l'ordre, et "
                            "vide la réception"},
    [C_FIN] = {"P9", "fin", "bsk_phy.h : un octet n'est rapporté qu'une fois ; chaque octet reçu l'est, par une trame, une "
                            "E_FRAMING ou une E_BUS, sauf ceux qu'un rx_clear vide (phy_common.h)"},
    [C_BOUCLE] = {"P2", "boucle", "phy_common.h, rx_next : « à rappeler tant qu'il rend true » ; le coût d'un flux de bruit "
                                  "est linéaire : au plus 2 RX_CAP + 4 événements par jugement"},
    [C_PLACE] = {"P2", "place", "phy_common.h, rx_got : la place libre (RX_CAP − n) n'est jamais nulle après un rx_next qui "
                                "a rendu false"},
    [C_DECOUPE] = {"P9", "decoupe", "phy_common.h : « le sort d'un octet ne change pas d'un jugement à l'autre » ; le même "
                                    "flux aux mêmes instants, découpé autrement, rend les mêmes événements"},
};
const size_t FZ_N_CHECKS = C_N;

int fz_cmd(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return -1;
}

/* ─────────────────────────── le cas lu ─────────────────────────── */

typedef struct {
    uint8_t        kind;   /* 0 rafale, 1 E_BUS, 2 rx_clear, 3 jugement */
    uint32_t       dt;
    const uint8_t *b;
    size_t         n;
} rec_t;

#define REC_MAX 8192u
static rec_t recs[REC_MAX];

static uint8_t at(const uint8_t *d, size_t n, size_t i) { return i < n ? d[i] : 0; }

static size_t parse(const uint8_t *d, size_t n, uint64_t *split)
{
    size_t i = 8, k = 0;
    *split = 0;
    for (size_t j = 0; j < 8; j++) *split |= (uint64_t)at(d, n, j) << 8 * j;
    while (i < n && k < REC_MAX) {
        rec_t *r = &recs[k++];
        r->kind = d[i] & 3u;
        r->dt = at(d, n, i + 1) | (uint32_t)at(d, n, i + 2) << 8 | (uint32_t)(at(d, n, i + 3) & 3u) << 16;
        r->b = NULL;
        r->n = 0;
        i += 4;
        if (r->kind == 0) {
            size_t len = at(d, n, i) | (size_t)(at(d, n, i + 1) & 0x1Fu) << 8;
            i += 2;
            r->b = i < n ? d + i : d;
            r->n = i < n ? (len < n - i ? len : n - i) : 0;
            i += r->n;
        }
    }
    return k;
}

/* ─────────────────────────── un jeu ─────────────────────────── */

typedef struct {
    uint8_t  kind;          /* 0 trame, 1 E_FRAMING, 2 E_BUS */
    uint64_t t;
    uint16_t a, b;          /* trame : longueur, classe << 8 | séquence ; erreur : n, len */
    uint32_t h;             /* FNV-1a du contenu */
} ev_t;

#define EV_MAX 32768u
static ev_t     evs[3][EV_MAX];
static size_t   nev[3];
static rx_t     rx;
static uint8_t  flux[1u << 17];   /* les octets reçus, dans l'ordre */
static size_t   recus, faits;     /* reçus ; rapportés ou vidés (le début du flux qui reste à rapporter) */
static fz_ctx_t *C;
static int      jeu;
static fz_rng_t decoupe;

static uint32_t fnv(const uint8_t *b, size_t n)
{
    uint32_t h = 2166136261u;
    for (size_t i = 0; i < n; i++) h = (h ^ b[i]) * 16777619u;
    return h;
}

static void note(uint8_t kind, uint64_t t, uint16_t a, uint16_t b, uint32_t h)
{
    if (nev[jeu] < EV_MAX) evs[jeu][nev[jeu]] = (ev_t){kind, t, a, b, h};
    nev[jeu]++;
}

/* La trame relue à la main contre les octets du flux à sa place (bsk_phy.h, en tête). */
static void trame(const bsk_phy_event_t *e)
{
    const bsk_frame_t *f = &e->u.frame;
    size_t L = (size_t)f->len + 8u;
    const uint8_t *b = flux + faits;
    uint32_t s = 0;
    if (faits + L > recus) {
        fz_fail(C, C_TRAME, "jeu %c : une trame de %zu octets, %zu octets reçus pas encore rapportés", 'A' + jeu, L, recus - faits);
        return;
    }
    for (size_t i = 1; i + 3 < L; i++) s += b[i];
    if (b[0] != 0xF0 || (size_t)(b[1] | b[2] << 8) != L || L < 9 || L > 255 || b[3] < 1 || b[3] > 3 || b[3] != f->cls ||
        b[4] != f->seq || memcmp(b + 5, f->msg, f->len) || b[L - 3] != (uint8_t)s || b[L - 2] != (uint8_t)(s >> 8) ||
        b[L - 1] != 0x55) {
        fz_fail(C, C_TRAME, "jeu %c : la trame rendue (classe %u, séquence %u, %u octets de message) n'est pas une trame "
                            "valide faite des octets %zu à %zu du flux",
                'A' + jeu, f->cls, f->seq, f->len, faits, faits + L - 1);
        return;
    }
    note(0, e->t_us, f->len, (uint16_t)(f->cls << 8 | f->seq), fnv(f->msg, f->len));
    fz_tr(C, "  %c t=%llu trame classe=%u seq=%u len=%u type=%02X", 'A' + jeu, (unsigned long long)e->t_us, f->cls, f->seq,
          f->len, f->msg[0]);
    faits += L;
}

/* Une erreur : ses octets relus contre le flux à leur place. */
static void ecart(const bsk_phy_raw_t *r, uint64_t t, bool bus)
{
    size_t len = r->n < 256u ? r->n : 256u;
    if (r->len != len || (!bus && r->n == 0)) {
        fz_fail(C, C_RELEVE, "jeu %c : %s n=%u len=%u", 'A' + jeu, bus ? "E_BUS" : "E_FRAMING", r->n, r->len);
        return;
    }
    if (faits + r->n > recus || memcmp(r->b, flux + faits, r->len)) {
        fz_fail(C, bus ? C_BUS : C_ECART, "jeu %c : %s de %u octets, pas les octets %zu à %zu du flux (%zu reçus)", 'A' + jeu,
                bus ? "E_BUS" : "E_FRAMING", r->n, faits, faits + r->n - 1, recus);
        return;
    }
    note(bus ? 2 : 1, t, r->n, r->len, fnv(r->b, r->len));
    fz_tr(C, "  %c t=%llu %s n=%u", 'A' + jeu, (unsigned long long)t, bus ? "e_bus" : "e_framing", r->n);
    faits += r->n;
}

static void juger(uint64_t now)
{
    bsk_phy_event_t e;
    unsigned k = 0;
    while (C->check < 0 && rx_next(&rx, now, &e)) {
        if (++k > 2u * RX_CAP + 4u) {
            fz_fail(C, C_BOUCLE, "jeu %c : rx_next rend encore true après %u événements à t=%llu", 'A' + jeu, k - 1,
                    (unsigned long long)now);
            return;
        }
        if (e.t_us != now) fz_fail(C, C_KIND, "jeu %c : un événement daté %llu, jugé à %llu", 'A' + jeu,
                                   (unsigned long long)e.t_us, (unsigned long long)now);
        else if (e.kind == BSK_PHY_FRAME) trame(&e);
        else if (e.kind == BSK_PHY_ERROR && e.u.err == E_FRAMING) ecart(&e.u.raw, now, false);
        else fz_fail(C, C_KIND, "jeu %c : un événement de nature %d (erreur %d)", 'A' + jeu, (int)e.kind, (int)e.u.err);
    }
    if (C->check < 0 && rx.n >= RX_CAP) fz_fail(C, C_PLACE, "jeu %c : place nulle après le jugement de t=%llu", 'A' + jeu,
                                                (unsigned long long)now);
}

/* Les échéances du début de trame périmé qui sonnent jusqu'à `t` compris. */
static void echeances(uint64_t t)
{
    for (unsigned k = 0; C->check < 0 && rx_deadline(&rx) <= t; k++) {
        uint64_t dl = rx_deadline(&rx);
        if (k > 4) {
            fz_fail(C, C_BOUCLE, "jeu %c : l'échéance %llu reste due après %u jugements", 'A' + jeu, (unsigned long long)dl, k);
            return;
        }
        juger(dl);
    }
}

static void rafale(const uint8_t *b, size_t n, uint64_t t)
{
    while (n && C->check < 0) {
        size_t room = RX_CAP - rx.n, k = n < room ? n : room;
        if (!room) {
            fz_fail(C, C_PLACE, "jeu %c : aucune place pour un bloc à t=%llu", 'A' + jeu, (unsigned long long)t);
            return;
        }
        if (jeu == 1) k = 1 + fz_below(&decoupe, (uint32_t)k);
        else if (jeu == 2) k = 1;
        memcpy(rx.b + rx.n, b, k);
        memcpy(flux + recus, b, k);
        recus += k;
        rx_got(&rx, k, t);
        juger(t);
        b += k;
        n -= k;
    }
}

static void jouer_un(size_t nrec, uint64_t split)
{
    uint64_t t = 1000000u;
    memset(&rx, 0, sizeof rx);
    recus = faits = 0;
    nev[jeu] = 0;
    decoupe.s = split;
    for (size_t i = 0; i < nrec && C->check < 0; i++) {
        const rec_t *r = &recs[i];
        t += r->dt;
        echeances(t);
        if (C->check >= 0) return;
        if (r->kind == 0) {
            fz_tr(C, "%c t=%llu rafale de %zu octets", 'A' + jeu, (unsigned long long)t, r->n);
            rafale(r->b, r->n, t);
        } else if (r->kind == 1) {
            bsk_phy_raw_t raw;
            fz_tr(C, "%c t=%llu rx_flush (E_BUS)", 'A' + jeu, (unsigned long long)t);
            rx_flush(&rx, &raw);
            ecart(&raw, t, true);
            if (C->check < 0 && faits != recus)
                fz_fail(C, C_BUS, "jeu %c : après rx_flush, %zu octets reçus jamais rapportés", 'A' + jeu, recus - faits);
        } else if (r->kind == 2) {
            fz_tr(C, "%c t=%llu rx_clear (%zu octets vidés)", 'A' + jeu, (unsigned long long)t, recus - faits);
            rx_clear(&rx);
            faits = recus;
        } else {
            fz_tr(C, "%c t=%llu jugement", 'A' + jeu, (unsigned long long)t);
            juger(t);
        }
    }
    if (C->check >= 0) return;
    echeances(UINT64_MAX - 1);
    if (C->check < 0 && faits != recus)
        fz_fail(C, C_FIN, "jeu %c : %zu octets reçus jamais rapportés (rapportés jusqu'à %zu sur %zu)", 'A' + jeu, recus - faits,
                faits, recus);
}

void fz_run(fz_ctx_t *c, const uint8_t *d, size_t n)
{
    uint64_t split;
    size_t nrec = parse(d, n, &split);
    FILE *tr = c->trace;
    C = c;
    for (jeu = 0; jeu < 3 && c->check < 0; jeu++) {
        c->trace = jeu ? NULL : tr;                     /* au rejeu, le jeu A seul est imprimé */
        jouer_un(nrec, split);
    }
    c->trace = tr;
    if (c->check >= 0) return;
    for (int k = 1; k < 3; k++) {
        size_t m = nev[0] < nev[k] ? nev[0] : nev[k];
        if (m > EV_MAX) m = EV_MAX;
        for (size_t i = 0; i < m; i++)
            if (memcmp(&evs[0][i], &evs[k][i], sizeof(ev_t))) {
                fz_fail(c, C_DECOUPE, "événement %zu : A (nature %u, t=%llu, %u, %u) et %c (nature %u, t=%llu, %u, %u)", i,
                        evs[0][i].kind, (unsigned long long)evs[0][i].t, evs[0][i].a, evs[0][i].b, 'A' + k, evs[k][i].kind,
                        (unsigned long long)evs[k][i].t, evs[k][i].a, evs[k][i].b);
                return;
            }
        if (nev[0] != nev[k]) {
            fz_fail(c, C_DECOUPE, "A rend %zu événements, %c en rend %zu", nev[0], 'A' + k, nev[k]);
            return;
        }
    }
    fz_tr(c, "trois découpages, %zu événements identiques", nev[0]);
}

/* ─────────────────────────── le générateur ─────────────────────────── */

/* Une trame encodée ici (bsk_phy.h, en tête), sans fr_encode : la classe, la longueur et la somme peuvent être fausses
 * exprès plus bas. */
static size_t encode(uint8_t *o, uint8_t cls, uint8_t seq, const uint8_t *msg, size_t n)
{
    size_t L = n + 8;
    uint16_t s = 0;
    o[0] = 0xF0;
    o[1] = (uint8_t)L;
    o[2] = (uint8_t)(L >> 8);
    o[3] = cls;
    o[4] = seq;
    memcpy(o + 5, msg, n);
    for (size_t i = 1; i + 3 < L; i++) s = (uint16_t)(s + o[i]);
    o[L - 3] = (uint8_t)s;
    o[L - 2] = (uint8_t)(s >> 8);
    o[L - 1] = 0x55;
    return L;
}

/* Un morceau de rafale dans `o` (au moins 300 octets libres, 4200 pour un bruit long). */
static size_t morceau(fz_rng_t *r, uint8_t *o, size_t libre)
{
    uint8_t msg[BSK_MSG_MAX];
    size_t n, L;
    uint32_t k = fz_below(r, 100);
    if (k < 25) {                                       /* une trame des traces du 135 (sim/sources135.c) */
        n = sources135_frame((src_id_t)fz_below(r, SRC_N), o, libre);
        if (n) return n;
    }
    if (k < 55) {                                       /* une trame valide, aux bornes de longueur */
        uint32_t q = fz_below(r, 6);
        n = q == 0 ? 1 : q == 1 ? BSK_MSG_MAX : q == 2 ? BSK_MSG_MAX - 1 : 1 + fz_below(r, BSK_MSG_MAX);
        for (size_t i = 0; i < n; i++) msg[i] = (uint8_t)fz_next(r);
        return encode(o, (uint8_t)(1 + fz_below(r, 3)), (uint8_t)fz_next(r), msg, n);
    }
    if (k < 75) {                                       /* une trame abîmée d'une façon */
        n = 1 + fz_below(r, 64);
        for (size_t i = 0; i < n; i++) msg[i] = (uint8_t)fz_next(r);
        L = encode(o, (uint8_t)(1 + fz_below(r, 3)), (uint8_t)fz_next(r), msg, n);
        switch (fz_below(r, 7)) {
        case 0: o[fz_below(r, (uint32_t)L)] ^= (uint8_t)(1u << fz_below(r, 8)); break;   /* un bit */
        case 1: return 1 + fz_below(r, (uint32_t)L - 1);                                  /* tronquée */
        case 2:                                                                           /* classe hors 1..3, somme juste */
            return encode(o, (uint8_t)(fz_chance(r, 1, 3) ? 0 : fz_chance(r, 1, 2) ? 4 : 4 + fz_below(r, 252)), o[4], msg, n);
        case 3: o[L - 1] = (uint8_t)fz_next(r); break;                                    /* fin */
        case 4: {                                                                         /* longueur annoncée */
            static const uint16_t BORD[] = {0, 1, 7, 8, 9, 254, 255, 256, 0x0109, 0xFFFF};
            uint16_t v = BORD[fz_below(r, sizeof BORD / sizeof BORD[0])];
            o[1] = (uint8_t)v;
            o[2] = (uint8_t)(v >> 8);
            break;
        }
        case 5: o[L - 3] ^= 0x01; break;                                                  /* somme */
        default: return L;
        }
        return L;
    }
    if (k < 85) {                                       /* un début de trame seul */
        o[0] = 0xF0;
        n = 1 + fz_below(r, 3);
        for (size_t i = 1; i < n; i++) o[i] = (uint8_t)(fz_chance(r, 1, 2) ? fz_below(r, 260) : fz_next(r));
        return n;
    }
    if (k < 97) {                                       /* du bruit, des F0 et des 55 */
        n = 1 + fz_below(r, 64);
        for (size_t i = 0; i < n; i++) {
            uint32_t q = fz_below(r, 4);
            o[i] = q == 0 ? 0xF0 : q == 1 ? 0x55 : (uint8_t)fz_next(r);
        }
        return n;
    }
    n = libre < 4200 ? libre : 4090 + fz_below(r, 110); /* un bruit long, autour de RX_CAP */
    for (size_t i = 0; i < n; i++) o[i] = (uint8_t)(fz_chance(r, 1, 8) ? 0xF0 : fz_next(r));
    return n;
}

static uint32_t delai(fz_rng_t *r)
{
    static const uint32_t BORD[] = {0, 1, 19999, 20000, 20001, 20002, 40002, 262143};
    switch (fz_below(r, 5)) {
    case 0: return 0;
    case 1: return fz_below(r, 2000);
    case 2: return BORD[fz_below(r, sizeof BORD / sizeof BORD[0])];
    case 3: return fz_below(r, 100000);
    default: return 50 + fz_below(r, 500);
    }
}

size_t fz_gen(fz_rng_t *r, uint8_t *out, size_t cap)
{
    size_t n = 8;
    uint32_t nrec = 1 + fz_below(r, 14);
    for (size_t i = 0; i < 8; i++) out[i] = (uint8_t)fz_next(r);
    for (uint32_t k = 0; k < nrec && n + 6 + 8191 <= cap; k++) {
        uint32_t q = fz_below(r, 100), dt = delai(r);
        uint8_t kind = q < 78 ? 0 : q < 88 ? 3 : q < 94 ? 1 : 2;
        out[n] = (uint8_t)(kind | (uint8_t)fz_below(r, 64) << 2);
        out[n + 1] = (uint8_t)dt;
        out[n + 2] = (uint8_t)(dt >> 8);
        out[n + 3] = (uint8_t)(dt >> 16);
        n += 4;
        if (kind == 0) {
            size_t len = 0, h = n;
            uint32_t m = 1 + fz_below(r, 8);
            n += 2;
            for (uint32_t j = 0; j < m && len + 4200 <= 8191; j++) len += morceau(r, out + n + len, 8191 - len);
            out[h] = (uint8_t)len;
            out[h + 1] = (uint8_t)(len >> 8);
            n += len;
        }
    }
    return n;
}
