/* L'hôte fou (F3 de plans/fuzzing.md, l'atelier ; cible 4 du plan, § 2) : ce que le banc envoie sur l'USB, comment le
 * cadrage de la carte le découpe en lignes, et ce que chaque ligne doit recevoir. Utilisé par fuzz_hote (fuzz_session.c
 * compilé avec FZ_HOTE), qui donne les lignes à la vraie couche HOTE (components/host/host.c) au-dessus de la vraie
 * SESSION et de l'objectif fou.
 *
 * Rien ici n'appelle host.c : le cadrage est recopié à la main de rx_task (components/host/host_usb.c, qui n'est pas
 * compilé sur l'hôte), les formes attendues sont écrites à la main d'après PROTOCOL.md, commande par commande, en tête de
 * hote.c. */
#ifndef HOTE_H
#define HOTE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "bsk_contract.h"
#include "fuzz.h"

/* ─────────────── le cadrage ───────────────
 * rx_task (components/host/host_usb.c), recopié à la main, octet par octet : `\r` ignoré partout ; en tête de ligne,
 * blancs (espace, tabulation) et `\n` sautés ; une ligne qui commence par `:` est Moonlite et finit au `#`, un `\n`
 * l'abandonne (`:` orphelin) ; une autre finit au `\n` ; un NUL, ou un 256e octet (USB_LINE_MAX − 1 = 255 au plus),
 * jette la ligne entière jusqu'à sa fin. Une ligne vide n'est pas livrée. La ligne livrée garde son `:` de tête, sans sa
 * fin : c'est ce que bsk_host_line reçoit (bsk_host.h). */
#define HOTE_LIGNE_MAX 255u

/* La version que le banc passe à bsk_host_init, rendue par `v` (PROTOCOL.md § 2). */
#define HOTE_VERSION "1.3"

typedef struct {
    char   l[HOTE_LIGNE_MAX + 1];
    size_t n;
    bool   jette, moon;
} cadre_t;

/* Un octet reçu. Vrai : une ligne est livrée, dans `ligne` (HOTE_LIGNE_MAX + 1 octets), terminée par NUL. */
bool cadre(cadre_t *c, uint8_t o, char *ligne);

/* ─────────────── les familles de lignes ─────────────── */

typedef enum {
    F_V, F_F, F_FN, F_M, F_R, F_E, F_G, F_A, F_AX, F_D, F_S, F_C, F_O, F_L, F_J, F_JS, F_JX, F_JG, F_I, F_K, F_KN, F_Q,
    F_P0, F_P1, F_B, F_Z, F_X, F_T, F_NOCAP,                /* minuscules : une ligne que PROTOCOL.md ne connaît pas */
    F_DEBUG, F_LOG, F_CUSTOM, F_CUSTOM_MAL, F_MAJ,          /* majuscules */
    F_ML_GP, F_ML_GN, F_ML_GI, F_ML_GV, F_ML_GT, F_ML_GC, F_ML_GD, F_ML_GH, F_ML_SN, F_ML_FG, F_ML_FQ, F_ML_MUET,
    F_N
} hote_fam_t;

extern const char *const HOTE_FAM[F_N];

/* La famille d'une ligne livrée par le cadrage, lue à la main (PROTOCOL.md § 1 règle 2, § 2 à § 4). */
hote_fam_t hote_famille(const char *l);

/* Une ligne dont la réponse peut attendre (BSK_HOST_LATER, bsk_host.h) : CUSTOM, q, js, jx. Sa borne, en µs de temps
 * virtuel depuis la ligne, retards du pas injectés retirés : en tête de hote.c. */
bool     hote_differable(hote_fam_t f);
uint64_t hote_borne_us(hote_fam_t f);

/* ─────────────── le jugement ─────────────── */

enum { HJ_FIN, HJ_TAMPON, HJ_LIGNE, HJ_LONGUEUR, HJ_OK, HJ_FORME, HJ_P4, HJ_B4, HJ_N };

typedef struct {
    int  k;                     /* HJ_*, -1 : la réponse passe */
    char why[300];
} hote_verdict_t;

/* Ce que le banc tient des lignes passées, à la main : le journal (LOG), le plafond de la LED (k). */
typedef struct {
    int      journal;           /* 0 éteint, 1 LOG ON, 2 LOG ALL */
    unsigned plafond;           /* 100 au démarrage de la carte (PROTOCOL.md § 2, `k`) */
} hote_modele_t;

/* Ce que le banc a vu d'une ligne : l'instantané lu juste avant elle, et ce qu'elle a déposé (bsk_session_command,
 * par --wrap) avec le premier accusé de cette commande. */
typedef struct {
    hote_fam_t   fam;
    bsk_status_t st;
    unsigned     depots;
    bsk_cmd_t    cmd;           /* la dernière commande déposée */
    bool         premier_vu;
    bsk_ack_t    premier;       /* son premier accusé */
    uint64_t     now_us;        /* l'instant de la ligne (`x`) */
} hote_ligne_t;

/* La réponse que bsk_host_line a rendue (`fin` : son retour ; `later` : BSK_HOST_LATER), ou, `finale`, celle que
 * bsk_host_later a rendue après un BSK_HOST_LATER. Met à jour le modèle (LOG, k). */
void hote_juge(hote_verdict_t *v, hote_modele_t *m, const hote_ligne_t *h, const char *ligne, const char *rep,
               const char *fin, bool later, bool finale);

/* Une ligne lisible dans un message : un octet hors de 0x20..0x7E, ou `\`, écrit \xHH ; 60 octets de la source au plus.
 * Un tampon statique, réécrit à chaque appel. */
const char *hote_vis(const char *s);

/* LOG : le mode que la ligne demande, d'après PROTOCOL.md § 3 (0, 1, 2), ou -1 si elle n'en demande aucun. */
int hote_log(const char *l);

/* CUSTOM WRITE <h><b> bien formée : l'octet de données que la carte doit émettre, 0x30 + (h << 4 | b) (PROTOCOL.md
 * § 3.1) ; -1 sinon. */
int hote_custom_write(const char *l);

/* ─────────────── le générateur ───────────────
 * Une ligne de la grammaire de PROTOCOL.md § 2 à § 4, aux bornes, avec sa fin (`\n`, ou `#` pour Moonlite), parfois des
 * blancs en tête et des `\r` ; ou des octets arbitraires (1 à 300, NUL, `\n`, `#`, `:` compris), que le cadrage
 * découpe. Rend le nombre d'octets écrits dans b (cap ≥ 320). */
size_t hote_octets(fz_rng_t *r, uint8_t *b, size_t cap);

#endif
