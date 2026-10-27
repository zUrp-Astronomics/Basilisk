/* Le modèle de la liste blanche de bench_core, écrit à la main (P5 de plans/fuzzing.md, l'atelier) : partagé par
 * fuzz_bench.c (P5 a, bsk_bench_send comparé au modèle) et fuzz_session.c (P5 b, chaque trame émise par la SESSION
 * passe le modèle pour la déclaration en cours). Ses sources sont citées en tête de fuzz_bench.c ; il n'appelle rien de
 * components/bench_core/bench_core.c. */
#ifndef LISTE_H
#define LISTE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "bsk_phy.h"

/* ─────────────────────────── le modèle, à la main ─────────────────────────── */

/* La taille de chaque message de la liste, type compris : spec § 7.1.3, colonne « Lu : Tamron / Samyang ». */
static inline unsigned taille(uint8_t type)
{
    if (type == 0x01) return 33;
    if (type == 0x03) return 21;
    if (type == 0x04) return 14;
    if (type == 0x07) return 2;
    if (type == 0x08) return 9;
    if (type == 0x09) return 5;
    if (type == 0x0A) return 17;
    if (type == 0x0B) return 3;
    if (type == 0x0D) return 2;
    if (type == 0x10) return 2;
    if (type == 0x1C) return 1;
    if (type == 0x1D) return 5;
    if (type == 0x3F) return 2;
    if (type == 0x40) return 19;
    return 0;
}

/* Un message 0x40 de 19 octets [0x40][Main][Sub][données…], pour la déclaration `d` (sa valeur brute). */
static inline bool canal(const uint8_t *m, int d)
{
    uint8_t main = m[1], sub = m[2], data = m[3];
    if (d != 1 && d != 2) return false;                                    /* bsk_bench_core.h:32-38 : OTHER et hors énumération */
    if (main == 'V' && sub == 0x00) return true;                           /* bsk_bench_core.h:20, PROTOCOL.md:356 */
    if (main == 'F' && (sub == 0xFA || sub == 0xFB || sub == 0x32)) return true;
    if (main == 'M' && (sub == 0x00 || sub == 0x31)) return true;
    if (d != 2 || main != 'P') return false;                               /* l'exception : le 135 déclaré seul */
    if (sub == 0xFA) return data != 0x53;                                  /* PROTOCOL.md:359 */
    if (sub == 0x38)                                                       /* PROTOCOL.md:358 : 0x30 + (h << 4 | b), h, b de 0 à 2 */
        return data == 0x30 || data == 0x31 || data == 0x32 || data == 0x40 || data == 0x41 || data == 0x42 || data == 0x50 ||
               data == 0x51 || data == 0x52;
    return false;
}

static inline bool admise(const bsk_frame_t *f, int d)
{
    size_t i = 0;
    if (f->len == 0 || f->len > 247) return false;                         /* BSK_MSG_MAX = 255 − 8 (bsk_phy.h) */
    while (i < f->len) {
        unsigned t = taille(f->msg[i]);
        if (t == 0 || i + t > f->len) return false;                         /* hors liste, ou ne pave pas exactement */
        if (f->msg[i] == 0x40 && !canal(f->msg + i, d)) return false;
        i += t;
    }
    return true;
}

#endif
