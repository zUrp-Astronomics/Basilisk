/* P5 (a) de plans/fuzzing.md (l'atelier) : bsk_bench_send, sur des trames et des déclarations arbitraires, comparé à un
 * modèle de la liste blanche écrit à la main (liste.h, partagé avec fuzz_session.c). Propriétés P1 (les sanitizers) et
 * P5 (a).
 *
 * Le modèle n'appelle rien de components/bench_core/bench_core.c (aucune de ses fonctions static) : il est écrit d'après
 *   - include/bsk_bench_core.h, lignes 17-30 (la liste, l'exception du 135, « tout le reste est refusé », toute trame plus
 *     longue que BSK_MSG_MAX) et 32-38 (une déclaration hors de l'énumération ferme le canal 0x40, comme OTHER) ;
 *   - 7_Docs/PROTOCOL.md § 1, règle 7 (lignes 41-45 : les types, le canal 0x40 de 19 octets complets vers un Samyang
 *     reconnu, « une trame dont les messages ne se pavent pas exactement par leurs tailles est refusée ») et § 3.1,
 *     « L'exception de bench_core » (lignes 356-361 : 'P' 0x38 à neuf valeurs, 'P' 0xFA différent de 0x53, pour le 135
 *     déclaré seulement) ;
 *   - la spec de l'atelier, § 7.1.3, le tableau « La liste » : la taille de chaque message, octet de type compris, colonne
 *     « Lu : Tamron / Samyang » (0x01 33, 0x03 21, 0x04 14, 0x07 2, 0x08 9, 0x09 5, 0x0A 17, 0x0B 3, 0x0D 2, 0x10 2,
 *     0x1C 1, 0x1D 5, 0x3F 2, 0x40 19).
 * Une trame sans message (len 0) est refusée : aucun de ses messages n'est dans la liste (bsk_bench_core.h, ligne 29, «
 * Tout le reste est refusé »), ce que le modèle lit comme : une trame admise a au moins un message.
 *
 * Les émissions sont observées par -Wl,--wrap=bsk_phy_send (sim/test/programmes.sh), comme test_session135 : bench_core
 * ne reçoit aucun contournement. L'issue de l'émission par PHY est celle du cas (E_OK ou E_BUS).
 *
 * Un cas : une suite de trames, chacune [déclaration][issue de PHY][classe][séquence][len sur 2 octets][len octets de
 * message, au plus BSK_MSG_MAX] ; la déclaration est un octet quelconque, converti en bsk_bench_lens_t. */
#define _POSIX_C_SOURCE 200809L

#include <string.h>

#include "bsk_bench_core.h"
#include "fuzz.h"
#include "liste.h"

const char   FZ_CIBLE[] = "bench";
const size_t FZ_CAP = 65536;

enum { C_ADMISE, C_REFUSEE, C_EMISE, C_ISSUE, C_COMPTE, C_N };

const fz_check_t FZ_CHECKS[] = {
    [C_ADMISE] = {"P5", "admise", "le modèle écrit à la main (en tête de ce fichier) admet la trame : bench_core doit "
                                  "l'émettre (bsk_bench_core.h, « Émet f si chacun de ses sous-messages est dans la liste »)"},
    [C_REFUSEE] = {"P5", "refusee", "le modèle refuse la trame : bench_core doit rendre E_FORBIDDEN sans rien émettre "
                                    "(CLAUDE.md § Interdits ; bsk_bench_core.h, lignes 17-30 ; PROTOCOL.md § 1, règle 7)"},
    [C_EMISE] = {"P5", "emise", "bsk_bench_core.h : la trame émise est celle qu'on lui donne, une fois"},
    [C_ISSUE] = {"P5", "issue", "bsk_bench_core.h : rend l'issue de l'émission par PHY (E_OK, E_BUS), ou E_FORBIDDEN"},
    [C_COMPTE] = {"P5", "compte", "bsk_bench_core.h : chaque refus augmente le compteur de un (bsk_bench_refused), et lui "
                                  "seul"},
};
const size_t FZ_N_CHECKS = C_N;

int fz_cmd(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return -1;
}

/* ─────────────────────────── l'émission observée ─────────────────────────── */

static unsigned      emises;
static bsk_frame_t   derniere;
static bsk_err_t     issue_phy;

bsk_err_t __wrap_bsk_phy_send(const bsk_frame_t *f);
bsk_err_t __wrap_bsk_phy_send(const bsk_frame_t *f)
{
    emises++;
    derniere = *f;
    return issue_phy;
}

static uint8_t at(const uint8_t *d, size_t n, size_t i) { return i < n ? d[i] : 0; }

void fz_run(fz_ctx_t *c, const uint8_t *d, size_t n)
{
    size_t i = 0, k = 0;
    while (i < n && c->check < 0) {
        static bsk_frame_t f;
        int decl = at(d, n, i);
        size_t len;
        bool attendu;
        uint32_t refus;
        bsk_err_t rc;
        issue_phy = at(d, n, i + 1) & 1u ? E_BUS : E_OK;
        memset(&f, 0, sizeof f);
        f.cls = at(d, n, i + 2);
        f.seq = at(d, n, i + 3);
        f.len = (uint16_t)(at(d, n, i + 4) | at(d, n, i + 5) << 8);
        i += 6;
        len = f.len < BSK_MSG_MAX ? f.len : BSK_MSG_MAX;
        for (size_t j = 0; j < len; j++) f.msg[j] = at(d, n, i + j);
        i += len;
        attendu = admise(&f, decl);
        emises = 0;
        refus = bsk_bench_refused();
        rc = bsk_bench_send(&f, (bsk_bench_lens_t)decl);
        fz_tr(c, "trame %zu : déclaration %d, len %u, type %02X : %s, rendu %d, %u émise(s)", k, decl, f.len, f.msg[0],
              attendu ? "admise" : "refusée", (int)rc, emises);
        if (attendu && emises == 0) fz_fail(c, C_ADMISE, "trame %zu (déclaration %d, len %u, type %02X) : refusée, le modèle "
                                                         "l'admet", k, decl, f.len, f.msg[0]);
        else if (!attendu && (emises || rc != E_FORBIDDEN))
            fz_fail(c, C_REFUSEE, "trame %zu (déclaration %d, len %u, type %02X) : émise ou non refusée (rendu %d), le "
                                  "modèle la refuse", k, decl, f.len, f.msg[0], (int)rc);
        else if (attendu && (emises != 1 || derniere.cls != f.cls || derniere.seq != f.seq || derniere.len != f.len ||
                             memcmp(derniere.msg, f.msg, f.len)))
            fz_fail(c, C_EMISE, "trame %zu : %u émission(s), ou une autre trame que celle donnée", k, emises);
        else if (attendu && rc != issue_phy) fz_fail(c, C_ISSUE, "trame %zu : rendu %d, PHY a rendu %d", k, (int)rc, (int)issue_phy);
        else if (bsk_bench_refused() != refus + (attendu ? 0u : 1u))
            fz_fail(c, C_COMPTE, "trame %zu : le compteur de refus passe de %lu à %lu", k, (unsigned long)refus,
                    (unsigned long)bsk_bench_refused());
        k++;
    }
}

/* ─────────────────────────── le générateur ─────────────────────────── */

/* Les messages de la liste, à leur taille ; un 0x40 tiré parmi des commandes voisines de la liste. */
static size_t message(fz_rng_t *r, uint8_t *m)
{
    static const uint8_t TYPES[] = {0x01, 0x03, 0x04, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0D, 0x10, 0x1C, 0x1D, 0x3F, 0x40,
                                    0x40, 0x40, 0x40, 0x0C, 0x14, 0x15, 0x16, 0x02, 0x05, 0x00, 0xFF};
    static const uint8_t MAINS[] = {'V', 'F', 'M', 'P', 'X', 'Z', 'I', 'J', 'K', 'W', 'H', 'L', 'v', 0};
    static const uint8_t SUBS[] = {0x00, 0xFA, 0xFB, 0x32, 0x31, 0x38, 0x34, 0x11, 0x33, 0x37, 0x39, 0x01, 0xFF};
    static const uint8_t DATA[] = {0x30, 0x31, 0x32, 0x33, 0x40, 0x41, 0x42, 0x43, 0x50, 0x51, 0x52, 0x53, 0x60, 0x2F,
                                   0x00, 0xFF, 0x3F, 0x4F, 0x5F, 0x20};
    uint8_t t = TYPES[fz_below(r, sizeof TYPES)];
    size_t n = taille(t);
    if (!n || fz_chance(r, 1, 30)) n = 1 + fz_below(r, 40);              /* hors liste, ou une taille fausse */
    for (size_t k = 0; k < n; k++) m[k] = (uint8_t)fz_next(r);
    m[0] = t;
    if (t == 0x40 && n >= 4 && fz_chance(r, 1, 2)) {                   /* une commande de la liste ou de l'exception */
        static const uint8_t PAIRES[][2] = {{'V', 0x00}, {'F', 0xFA}, {'F', 0xFB}, {'F', 0x32}, {'M', 0x00}, {'M', 0x31},
                                            {'P', 0xFA}, {'P', 0x38}, {'P', 0x38}, {'P', 0x38}};
        uint32_t q = fz_below(r, sizeof PAIRES / sizeof PAIRES[0]);
        m[1] = PAIRES[q][0];
        m[2] = PAIRES[q][1];
        m[3] = fz_chance(r, 3, 4) ? DATA[fz_below(r, sizeof DATA)] : (uint8_t)fz_next(r);
    } else if (t == 0x40 && n >= 4) {
        m[1] = MAINS[fz_below(r, sizeof MAINS)];
        m[2] = SUBS[fz_below(r, sizeof SUBS)];
        m[3] = fz_chance(r, 3, 4) ? DATA[fz_below(r, sizeof DATA)] : (uint8_t)fz_next(r);
    }
    return n;
}

size_t fz_gen(fz_rng_t *r, uint8_t *out, size_t cap)
{
    static const uint8_t DECL[] = {0, 1, 2, 2, 2, 1, 3, 255, 255, 255};   /* OTHER, SAMYANG, SAMYANG135, hors énumération */
    size_t n = 0;
    uint32_t nf = 1 + fz_below(r, 24);
    for (uint32_t k = 0; k < nf && n + 6 + 300 <= cap; k++) {
        uint8_t m[400] = {0};
        size_t len = 0, w;
        uint32_t nm = fz_chance(r, 4, 5) ? 1 + fz_below(r, 3) : 1 + fz_below(r, 12);
        uint8_t decl = DECL[fz_below(r, sizeof DECL)];
        uint16_t lf;
        out[n] = fz_chance(r, 1, 8) ? (uint8_t)fz_next(r) : decl;
        out[n + 1] = (uint8_t)fz_below(r, 4);
        out[n + 2] = (uint8_t)fz_below(r, 4);
        out[n + 3] = (uint8_t)fz_next(r);
        for (uint32_t j = 0; j < nm && len + 40 <= sizeof m; j++) len += message(r, m + len);
        if (fz_chance(r, 1, 10)) len = fz_below(r, (uint32_t)len + 1);   /* tronquée */
        if (len > BSK_MSG_MAX && fz_chance(r, 3, 4)) len = BSK_MSG_MAX - fz_below(r, 3);
        lf = (uint16_t)len;
        if (fz_chance(r, 1, 20)) lf = (uint16_t)(BSK_MSG_MAX + 1 + fz_below(r, 0xFF00));   /* longueur annoncée hors borne */
        out[n + 4] = (uint8_t)lf;
        out[n + 5] = (uint8_t)(lf >> 8);
        n += 6;
        w = lf < BSK_MSG_MAX ? lf : BSK_MSG_MAX;                         /* ce que fz_run lit */
        memcpy(out + n, m, w);
        n += w;
    }
    return n;
}
