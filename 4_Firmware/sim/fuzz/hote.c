/* L'hôte fou (hote.h) : le cadrage de la carte recopié à la main, la grammaire des lignes et ce que chaque ligne doit
 * recevoir. Rien ici n'appelle components/host/host.c : le banc juge host.c, il ne le consulte pas.
 *
 * LES FORMES ATTENDUES, écrites à la main, commande par commande, d'après PROTOCOL.md (§ 1 les règles, § 2 le tableau
 * des lettres et § 2.1, § 3 et § 3.1 les majuscules, § 4 Moonlite, § 5 `t`) et la spec de l'atelier § 4.5.1 (le
 * tableau d'état d'une lettre qui a besoin de l'objectif ; « une ligne que PROTOCOL.md ne connaît pas répond er nocap »).
 * « L'état » est l'état publié de la session (bsk_session_status), lu par le banc juste avant la ligne ; « coeur » les
 * lettres `f r a d c` et `m<n>`, qui rendent `nc` en OFF ; « autre » les autres, `er nolens` en OFF :
 *
 *   état d'une lettre qui a besoin de l'objectif, hors READY (PROTOCOL.md § 1 règle 5, § 2 lignes f, c, § 3.1
 *   « Conditions » ; spec § 4.5.1, le tableau) : OFF -> coeur `nc`, autre `er nolens` ; POWERING, IDENTIFYING, HOMING,
 *   RESTORING, RECOVERING -> `er busy boot` ; FAULT -> `er fault` ; `o`, `i`, `j` en FAULT : servies si l'identité est
 *   établie (un nom ou un code modèle), `er nolens` sinon.
 *
 *   v        la version passée à bsk_host_init, HOTE_VERSION (§ 2, `v`)
 *   f        READY : la position si position_valid, `er link pos` sinon ; ailleurs l'état (coeur)
 *   f…       READY : des chiffres (après `+` ou `-` pour un relatif), sinon `er range num` ; au-delà de 65535, ou un relatif
 *   m<d>…    qui sort de 0..65535 depuis la position lue, `er range 65535` ; sinon déposé : `ok`, `er busy`,
 *            `er range limits` (§ 2, f<n>, f±n, m<n>) ; ailleurs l'état (coeur), rien de déposé
 *   r        READY : `<min>-<max>`, les bornes publiées, si limits_valid, `er link limits` sinon ; ailleurs l'état (coeur)
 *   e        `y` en mouvement (READY) et dans les états de séquence, `n` sinon (§ 2, `e`)
 *   g        `ok`, `aborted`, `stall`, `unconfirmed` ou `-` (§ 2, `g`)
 *   a        READY : `<f>-<f>` avec CAP_APERTURE, `er nocap ap` sans ; ailleurs l'état (coeur)
 *   a…       READY : `ok`, `er range ap`, `er range num`, `er busy`, `er nocap ap` ; ailleurs l'état (coeur)
 *   d        READY : `-` ; ailleurs l'état (coeur)
 *   s…       `ok`, la vitesse ignorée (§ 2, `s<n>`) : le banc lit `s` suivi de n'importe quoi comme `s<n>`
 *   c        READY : `ok` ; ailleurs l'état (coeur)
 *   o        servie : `er nocap ap` sans CAP_APERTURE_READBACK ou sans plage, `er link aperture` l'ouverture inconnue
 *            (aperture_current à 0), `<f>` sinon ; ailleurs l'état (autre)
 *   l        `er nocap focal`
 *   j        servie : la marque si mark_valid, `-` sinon ; ailleurs l'état (autre)
 *   js, jx   READY : `ok`, `er store`, `er busy`, `er range limits`, `er nocap`, ou la réponse différée, `ok` ou
 *            `er store` ; ailleurs l'état (autre)
 *   jg       READY : `er range nomark` sans marque, rien de déposé ; sinon `ok`, `er busy`, `er range limits` ; ailleurs
 *            l'état (autre)
 *   i        servie : le nom publié s'il y en a un, sinon un nom de la table ou `#<code>` s'il y a un code modèle, `-`
 *            sinon ; des octets 0x20..0x7E sans `"` ; ailleurs l'état (autre)
 *   k        le plafond, que le banc tient (100 au démarrage) ; `k<n>` : `ok` pour des chiffres de valeur 0 à 100, le
 *            plafond posé, `er range 0 100` sinon (§ 2, `k`)
 *   q        HOMING : `er busy home` ; IDENTIFYING : `ok` ou `er busy home` ; READY et RESTORING : `ok`, `er busy`,
 *            `er link <jeton>`, ou la réponse différée (`ok`, `er busy`, `er link <jeton>`) ; ailleurs `ok` (§ 2, `q`)
 *   p0       un démarrage en cours : `er busy boot` ; sinon `ok` ou `er busy` (§ 2, p0/p1)
 *   p1       OFF, FAULT : `ok` ou `er busy` ; ailleurs `ok`, rien de déposé
 *   b        OFF sans maintien : `er nolens` ; sinon `ok` ou `er busy move` (§ 2, `b` : `er busy …`)
 *   z        `-` : le banc compile sim/sensor_none.c, la mesure inconnue (§ 2, `z`)
 *   x        `poweron <ms>`, le temps écoulé tronqué à la milliseconde (§ 2, `x` ; le banc passe phy_sim_now)
 *   t        les neuf clés de § 5, dans l'ordre, leurs valeurs lues dans l'instantané (last_op : sa forme seule)
 *   n w h u… p pl* pm* cm, `g`, `z`, `x`, `m`, `j` suivis d'autre chose, toute autre ligne en minuscule ou autre :
 *            `er nocap` (§ 2 ; spec § 4.5.1)
 *   DEBUG    `ok reset=poweron dropped=…`, finie par ` nvs=ok|erased|er` (§ 3) ; `DEBUG…` : `er nocap`
 *   LOG      un mot, ON, ALL ou OFF, sans casse, entre blancs : `ok log=<0|1> all=<0|1>` ; sinon
 *            `er range LOG ON|ALL|OFF` (§ 3)
 *   CUSTOM   READ, ou WRITE et deux chiffres de 0 à 2, sans casse, entre blancs : READY, `er nocap custom`, `er busy`,
 *            `er link desync`, `er refused`, `er link bus`, ou la réponse différée : `ok` et 16 octets ` HH`, ou
 *            `er link timeout|framing|aborted|bus`, `er refused`, `er busy` ; ailleurs l'état (autre) ; sinon
 *            `er range CUSTOM READ|WRITE <0-2><0-2>` (§ 3.1)
 *   autre majuscule : `er nocap` (§ 3)
 *   :GP# :GN#  4 chiffres hexadécimaux majuscules et `#`, sans fin de ligne ; :GP# la position si position_valid (§ 4)
 *   :GI#     `01#` si `e` rendrait `y`, `00#` sinon ; :GV# `10` ; :GT# `0000#` (la mesure inconnue) ; :GC# `00#` ;
 *            :GD# `02#` ; :GH# `00#` ; toute autre ligne `:…` : aucune réponse (§ 4)
 *
 * Les blancs entre les mots de LOG et de CUSTOM, l'espace ou la tabulation, ne sont pas écrits dans PROTOCOL.md : le banc
 * les lit tous deux comme blancs. LOG et CUSTOM eux-mêmes sont en majuscules (le dispatch par le premier octet, § 1 règle
 * 2) ; seuls leurs arguments sont sans casse (§ 3.1, « CUSTOM read »).
 *
 * L'ADMISSION (l'audit B4 (a) de l'atelier ; spec § 4.5.1 : une lecture ne dépose rien, une commande dépose son
 * bsk_cmd_t) : une lecture ne dépose rien ; une commande que HOTE admet elle-même (f…, m<n>, a…, js, jx, jg, CUSTOM) ne
 * dépose rien hors READY, et en READY, son texte lisible, dépose la commande de sa ligne, la réponse suivant son premier
 * accusé (ACK_ACCEPTED : `ok` ou la réponse différée ; ACK_REJECTED : `er …`). La session ne sert ces commandes qu'en
 * READY (bsk_session.h, bsk_session_command) : une ligne déposée hors READY, ou refusée pour l'état en READY, est la
 * divergence que le point (a) de B4 craint.
 *
 * LES BORNES des réponses différées (bsk_host.h, bsk_host_later ; PROTOCOL.md § 2 ligne q, § 3.1), en temps virtuel
 * depuis la ligne, les retards du pas injectés retirés, plus un tic de 1 ms (la boucle de main.c sert la ligne au pas
 * qui suit son échéance) :
 *   q        le 0x04 de la paire en cours, 1,5 ms au plus après le dépôt : 2,5 ms ;
 *   CUSTOM   la requête retenue jusqu'au 0x04 (1,5 ms), puis sa réponse ou son échéance, 1 s (PROTOCOL.md § 3.1) :
 *            1 002,5 ms ;
 *   js, jx   BSK_STORE_WRITE_MS, 2 s (bsk_store.h ; PROTOCOL.md § 2, « 2 s au plus après la ligne ») : 2 001 ms.
 * La sortie de READY finit chacune plus tôt. */
#include "hote.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "bsk_host.h"

/* ─────────────────────────── le cadrage ─────────────────────────── */

bool cadre(cadre_t *c, uint8_t o, char *ligne)
{
    if (o == '\r') return false;                                   /* host_usb.c, rx_task : `\r` ignoré */
    if (c->n == 0 && !c->jette) {
        if (o == ' ' || o == '\t' || o == '\n') return false;      /* blancs de tête sautés */
        c->moon = o == ':';
    }
    if ((c->moon && o == '#') || (!c->moon && o == '\n')) {
        bool livre = !c->jette && c->n > 0;
        if (livre) {
            memcpy(ligne, c->l, c->n);
            ligne[c->n] = '\0';
        }
        c->jette = false;
        c->n = 0;
        return livre;
    }
    if (c->moon && o == '\n') {                                    /* `:` orphelin */
        c->n = 0;
        c->jette = false;
        return false;
    }
    if (c->jette) return false;
    if (o == 0) {                                                  /* un NUL : la ligne jetée */
        c->jette = true;
        c->n = 0;
        return false;
    }
    if (c->n < HOTE_LIGNE_MAX) c->l[c->n++] = (char)o;
    else {                                                         /* trop longue : jetée */
        c->jette = true;
        c->n = 0;
    }
    return false;
}

/* ─────────────────────────── les familles ─────────────────────────── */

const char *const HOTE_FAM[F_N] = {
    [F_V] = "v", [F_F] = "f", [F_FN] = "f…", [F_M] = "m<n>", [F_R] = "r", [F_E] = "e", [F_G] = "g", [F_A] = "a",
    [F_AX] = "a…", [F_D] = "d", [F_S] = "s…", [F_C] = "c", [F_O] = "o", [F_L] = "l", [F_J] = "j", [F_JS] = "js",
    [F_JX] = "jx", [F_JG] = "jg", [F_I] = "i", [F_K] = "k", [F_KN] = "k…", [F_Q] = "q", [F_P0] = "p0", [F_P1] = "p1",
    [F_B] = "b", [F_Z] = "z", [F_X] = "x", [F_T] = "t", [F_NOCAP] = "inconnue", [F_DEBUG] = "DEBUG", [F_LOG] = "LOG",
    [F_CUSTOM] = "CUSTOM", [F_CUSTOM_MAL] = "CUSTOM mal formée", [F_MAJ] = "majuscule inconnue", [F_ML_GP] = ":GP#",
    [F_ML_GN] = ":GN#", [F_ML_GI] = ":GI#", [F_ML_GV] = ":GV#", [F_ML_GT] = ":GT#", [F_ML_GC] = ":GC#",
    [F_ML_GD] = ":GD#", [F_ML_GH] = ":GH#", [F_ML_SN] = ":SN…#", [F_ML_FG] = ":FG#", [F_ML_FQ] = ":FQ#",
    [F_ML_MUET] = ":…# muette",
};

static bool blanc(char c) { return c == ' ' || c == '\t'; }

/* `l` commence par le mot `w`, suivi de la fin ou d'un blanc. */
static bool mot_tete(const char *l, const char *w)
{
    size_t n = strlen(w);
    return !strncmp(l, w, n) && (!l[n] || blanc(l[n]));
}

/* Le mot de `n` octets en `p` est `ref` (en majuscules), sans casse ASCII. */
static bool sans_casse(const char *p, size_t n, const char *ref)
{
    if (strlen(ref) != n) return false;
    for (size_t i = 0; i < n; i++) {
        char c = p[i];
        if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
        if (c != ref[i]) return false;
    }
    return true;
}

/* Les mots de `a`, séparés de blancs : au plus `max` ; rend leur nombre (max + 1 s'il y en a plus). */
static int mots(const char *a, const char **t, size_t *n, int max)
{
    int k = 0;
    for (;;) {
        while (blanc(*a)) a++;
        if (!*a) return k;
        if (k == max) return max + 1;
        t[k] = a;
        for (n[k] = 0; a[n[k]] && !blanc(a[n[k]]); n[k]++) {}
        a += n[k];
        k++;
    }
}

int hote_log(const char *l)
{
    const char *t[1];
    size_t n[1];
    if (!mot_tete(l, "LOG") || mots(l + 3, t, n, 1) != 1) return -1;
    if (sans_casse(t[0], n[0], "ON")) return 1;
    if (sans_casse(t[0], n[0], "ALL")) return 2;
    if (sans_casse(t[0], n[0], "OFF")) return 0;
    return -1;
}

/* CUSTOM bien formée : READ (rend -2), ou WRITE <h><b>, h et b de 0 à 2 (rend h << 4 | b) ; -1 sinon. */
static int custom_arg(const char *l)
{
    const char *t[2];
    size_t n[2];
    int k;
    if (!mot_tete(l, "CUSTOM")) return -1;
    k = mots(l + 6, t, n, 2);
    if (k == 1 && sans_casse(t[0], n[0], "READ")) return -2;
    if (k == 2 && sans_casse(t[0], n[0], "WRITE") && n[1] == 2 && t[1][0] >= '0' && t[1][0] <= '2' && t[1][1] >= '0' &&
        t[1][1] <= '2')
        return (t[1][0] - '0') << 4 | (t[1][1] - '0');
    return -1;
}

int hote_custom_write(const char *l)
{
    int a = custom_arg(l);
    return a >= 0 ? 0x30 + a : -1;
}

hote_fam_t hote_famille(const char *l)
{
    if (l[0] == ':') {
        static const char *const G[] = {"GP", "GN", "GI", "GV", "GT", "GC", "GD", "GH"};
        static const hote_fam_t FG[] = {F_ML_GP, F_ML_GN, F_ML_GI, F_ML_GV, F_ML_GT, F_ML_GC, F_ML_GD, F_ML_GH};
        for (size_t i = 0; i < sizeof G / sizeof G[0]; i++)
            if (!strcmp(l + 1, G[i])) return FG[i];
        if (!strncmp(l + 1, "SN", 2)) return F_ML_SN;
        if (!strcmp(l + 1, "FG")) return F_ML_FG;
        if (!strcmp(l + 1, "FQ")) return F_ML_FQ;
        return F_ML_MUET;
    }
    if (l[0] >= 'A' && l[0] <= 'Z') {
        if (!strcmp(l, "DEBUG")) return F_DEBUG;
        if (mot_tete(l, "LOG")) return F_LOG;
        if (mot_tete(l, "CUSTOM")) return custom_arg(l) == -1 ? F_CUSTOM_MAL : F_CUSTOM;
        return F_MAJ;
    }
    switch (l[0]) {
    case 'v': return l[1] ? F_NOCAP : F_V;
    case 'f': return l[1] ? F_FN : F_F;
    case 'm': return l[1] >= '0' && l[1] <= '9' ? F_M : F_NOCAP;
    case 'r': return l[1] ? F_NOCAP : F_R;
    case 'e': return l[1] ? F_NOCAP : F_E;
    case 'g': return l[1] ? F_NOCAP : F_G;
    case 'a': return l[1] ? F_AX : F_A;
    case 'd': return l[1] ? F_NOCAP : F_D;
    case 's': return F_S;
    case 'c': return l[1] ? F_NOCAP : F_C;
    case 'o': return l[1] ? F_NOCAP : F_O;
    case 'l': return l[1] ? F_NOCAP : F_L;
    case 'i': return l[1] ? F_NOCAP : F_I;
    case 'k': return l[1] ? F_KN : F_K;
    case 'q': return l[1] ? F_NOCAP : F_Q;
    case 'b': return l[1] ? F_NOCAP : F_B;
    case 'z': return l[1] ? F_NOCAP : F_Z;
    case 'x': return l[1] ? F_NOCAP : F_X;
    case 't': return l[1] ? F_NOCAP : F_T;
    case 'j':
        if (!l[1]) return F_J;
        if (!strcmp(l, "js")) return F_JS;
        if (!strcmp(l, "jx")) return F_JX;
        if (!strcmp(l, "jg")) return F_JG;
        return F_NOCAP;
    case 'p':
        if (!strcmp(l, "p0")) return F_P0;
        if (!strcmp(l, "p1")) return F_P1;
        return F_NOCAP;
    default: return F_NOCAP;
    }
}

bool hote_differable(hote_fam_t f) { return f == F_CUSTOM || f == F_Q || f == F_JS || f == F_JX; }

uint64_t hote_borne_us(hote_fam_t f)
{
    if (f == F_Q) return 2500u;
    if (f == F_CUSTOM) return 1002500u;
    if (f == F_JS || f == F_JX) return 2001000u;
    return 0;
}

/* ─────────────────────────── le jugement ─────────────────────────── */

/* Des chiffres seuls, au moins un ; leur valeur, plafonnée au-delà de 65535 (pour comparer à 65535 et à 100). */
static bool chiffres(const char *s, uint32_t *v)
{
    if (!*s) return false;
    for (*v = 0; *s; s++) {
        if (*s < '0' || *s > '9') return false;
        if (*v <= 0xFFFFF) *v = *v * 10 + (uint32_t)(*s - '0');
    }
    return true;
}

/* `<d+>.<d>` (une ouverture, `o`) ; `<d+>.<d>-<d+>` (la plage, `a`) ; quatre chiffres hexadécimaux majuscules et `#`. */
static const char *entier_de(const char *s)
{
    if (*s < '0' || *s > '9') return NULL;
    while (*s >= '0' && *s <= '9') s++;
    return s;
}
static const char *fnum_de(const char *s)
{
    s = entier_de(s);
    if (!s || *s != '.' || s[1] < '0' || s[1] > '9') return NULL;
    return s + 2;
}
static bool fnum(const char *s) { s = fnum_de(s); return s && !*s; }
static bool plage_a(const char *s)
{
    s = fnum_de(s);
    if (!s || *s != '-') return false;
    s = entier_de(s + 1);
    return s && !*s;
}
static bool hex4(const char *s)
{
    for (int i = 0; i < 4; i++)
        if (!((s[i] >= '0' && s[i] <= '9') || (s[i] >= 'A' && s[i] <= 'F'))) return false;
    return s[4] == '#' && !s[5];
}
static bool nombre(const char *s) { s = entier_de(s); return s && !*s; }
static bool jeton(const char *s)
{
    if (!*s) return false;
    for (; *s; s++)
        if (!((*s >= 'a' && *s <= 'z') || *s == '_')) return false;
    return true;
}

static bool est(const char *a, const char *b) { return !strcmp(a, b); }

static bool sequence(uint8_t e)
{
    return e == SESSION_POWERING || e == SESSION_IDENTIFYING || e == SESSION_HOMING || e == SESSION_RESTORING ||
           e == SESSION_RECOVERING;
}

static const char *etat_coeur(uint8_t e) { return e == SESSION_OFF ? "nc" : e == SESSION_FAULT ? "er fault" : "er busy boot"; }
static const char *etat_autre(uint8_t e) { return e == SESSION_OFF ? "er nolens" : e == SESSION_FAULT ? "er fault" : "er busy boot"; }

/* `o`, `i`, `j` : servies en READY, et en FAULT l'identité établie ; sinon `er nolens` (OFF, FAULT sans identité) ou
 * `er busy boot`. */
static bool identifie(const bsk_status_t *st) { return st->lens_name[0] || st->lens_id_product; }
static bool lecture_servie(const bsk_status_t *st)
{
    return st->session_state == SESSION_READY || (st->session_state == SESSION_FAULT && identifie(st));
}
static const char *lecture_refus(uint8_t e) { return sequence(e) ? "er busy boot" : "er nolens"; }

static bool e_oui(const bsk_status_t *st)
{
    return st->session_state == SESSION_READY ? st->moving : sequence(st->session_state);
}

static const char *nom_etat(uint8_t e)
{
    static const char *const N[] = {"off", "powering", "identifying", "homing", "restoring", "ready", "recovering", "fault"};
    return e < 8 ? N[e] : "?";
}

/* Les lignes du jeu figé (PROTOCOL.md § 1 règle 4) : celles qui commencent par f r e a d s c, et m<n>. */
static bool figee(const char *l) { return strchr("freadsc", l[0]) != NULL || (l[0] == 'm' && l[1] >= '0' && l[1] <= '9'); }

/* La ligne et la réponse jugées, lisibles dans un message : un octet hors de 0x20..0x7E, ou `\\`, écrit \\xHH ; 60
 * octets au plus de la source. */
static char lv[256], rv[256];
static void lisible(const char *s, char *o, size_t cap)
{
    size_t k = 0;
    for (size_t i = 0; s[i] && i < 60 && k + 5 < cap; i++) {
        unsigned char c = (unsigned char)s[i];
        k += (size_t)snprintf(o + k, cap - k, c < 0x20 || c > 0x7E || c == '\\' ? "\\x%02X" : "%c", c);
    }
    o[k] = '\0';
}

const char *hote_vis(const char *s)
{
    static char b[256];
    lisible(s, b, sizeof b);
    return b;
}

static bool echoue(hote_verdict_t *v, int k, const char *fmt, ...) __attribute__((format(printf, 3, 4)));

static bool echoue(hote_verdict_t *v, int k, const char *fmt, ...)
{
    va_list ap;
    if (v->k >= 0) return true;
    v->k = k;
    va_start(ap, fmt);
    vsnprintf(v->why, sizeof v->why, fmt, ap);
    va_end(ap);
    return true;
}

/* La réponse est l'une des chaînes de la liste (terminée par NULL). */
static bool parmi(const char *rep, const char *const *l)
{
    for (; *l; l++)
        if (est(rep, *l)) return true;
    return false;
}

/* `er link <jeton>`. */
static bool er_link(const char *rep) { return !strncmp(rep, "er link ", 8) && jeton(rep + 8); }

/* `t` (PROTOCOL.md § 5) : les neuf clés dans l'ordre, les valeurs lues dans l'instantané, last_op sa forme. */
static bool ligne_t(const char *rep, const bsk_status_t *st, char *pourquoi, size_t cap)
{
    static const char *const K[] = {"ext", "present", "boot_state", "busy", "power_off", "last_op", "mf", "oss", "ring"};
    char b[400], want[9][40];
    char *v[9];
    unsigned i = 0;
    const char *tri[3] = {"-", "0", "1"};                              /* BSK_UNKNOWN, BSK_NO, BSK_YES */
    snprintf(want[0], sizeof want[0], "1");
    snprintf(want[1], sizeof want[1], "%d", st->session_state != SESSION_OFF);
    snprintf(want[2], sizeof want[2], "%s", nom_etat(st->session_state));
    snprintf(want[3], sizeof want[3], "%s", sequence(st->session_state) ? "boot" : "-");
    snprintf(want[4], sizeof want[4], "%d", st->held);
    want[5][0] = '\0';
    snprintf(want[6], sizeof want[6], "%s", st->mf < 3 ? tri[st->mf] : "?");
    snprintf(want[7], sizeof want[7], "%s", st->oss < 3 ? tri[st->oss] : "?");
    snprintf(want[8], sizeof want[8], "%s", st->ring == RING_APERTURE ? "ap" : st->ring == RING_FOCUS ? "focus" : "-");
    if (strlen(rep) >= sizeof b) return snprintf(pourquoi, cap, "trop longue"), false;
    strcpy(b, rep);
    for (char *q = b; i < 9; i++) {
        size_t n = strlen(K[i]);
        if (strncmp(q, K[i], n) || q[n] != '=') return snprintf(pourquoi, cap, "la clé %s attendue en %u", K[i], i + 1), false;
        v[i] = q + n + 1;
        q = strchr(v[i], ' ');
        if (i < 8 && !q) return snprintf(pourquoi, cap, "%u clés au lieu de 9", i + 1), false;
        if (q) *q++ = '\0';
        if (i == 8 && q) return snprintf(pourquoi, cap, "plus de 9 clés"), false;
    }
    for (i = 0; i < 9; i++)
        if (i != 5 && strcmp(v[i], want[i]))
            return snprintf(pourquoi, cap, "%s=%.40s, attendu %.39s", K[i], v[i], want[i]), false;
    if (!est(v[5], "-:ok") && !est(v[5], "boot:ok") &&
        !(!strncmp(v[5], "boot:er:\"", 9) && strlen(v[5]) > 10 && v[5][strlen(v[5]) - 1] == '"'))
        return snprintf(pourquoi, cap, "last_op=%s", v[5]), false;
    return true;
}

/* La commande que la ligne doit déposer (son op), et son arg quand le banc le connaît (`arg_connu`). */
static bool depot_attendu(hote_fam_t f, const char *l, const bsk_status_t *st, bsk_cmd_op_t *op, int32_t *arg, bool *arg_connu)
{
    uint32_t v;
    *arg_connu = false;
    switch (f) {
    case F_FN:
    case F_M: {
        bool rel = f == F_FN && (l[1] == '+' || l[1] == '-');
        *op = rel ? CMD_FOCUS_MOVE : CMD_FOCUS_GOTO;
        if (chiffres(l + 1 + rel, &v)) {
            *arg = rel && l[1] == '-' ? -(int32_t)v : (int32_t)v;
            *arg_connu = true;
        }
        return true;
    }
    case F_AX: *op = CMD_APERTURE_SET; return true;
    case F_JS: *op = CMD_SET_MARK; *arg = BSK_MARK_HERE; *arg_connu = true; return true;
    case F_JX: *op = CMD_CLEAR_MARK; return true;
    case F_JG: *op = CMD_FOCUS_GOTO; *arg = st->mark_position; *arg_connu = true; return true;
    case F_CUSTOM: {
        int a = custom_arg(l);
        *op = CMD_LENS_CUSTOM;
        *arg = a == -2 ? -1 : a;
        *arg_connu = true;
        return true;
    }
    case F_Q:
    case F_ML_FQ: *op = CMD_FOCUS_STOP; return true;
    case F_P0: *op = CMD_DETACH; return true;
    case F_P1: *op = CMD_ATTACH; return true;
    case F_B: *op = st->session_state == SESSION_FAULT ? CMD_CLEAR_FAULT : CMD_ATTACH; return true;
    case F_ML_FG: *op = CMD_FOCUS_GOTO; return true;
    default: return false;
    }
}

/* L'admission, B4 (a) : `doit` la ligne dépose sa commande ; `peut` elle peut la déposer (:FG#, sans cible) ; sinon
 * elle ne dépose rien. */
static void admission(hote_verdict_t *v, const hote_ligne_t *h, const char *l, bool doit, bool peut)
{
    bsk_cmd_op_t op = CMD_ATTACH;
    int32_t arg = 0;
    bool arg_connu = false, a_un = depot_attendu(h->fam, l, &h->st, &op, &arg, &arg_connu);
    if (h->depots > 1) {
        echoue(v, HJ_B4, "« %s » (%s) : %u commandes déposées", lv, nom_etat(h->st.session_state), h->depots);
        return;
    }
    if (!h->depots) {
        if (doit)
            echoue(v, HJ_B4, "« %s » en %s : rien de déposé, la session l'aurait reçue (servie en READY)", lv,
                   nom_etat(h->st.session_state));
        return;
    }
    if (!doit && !peut) {
        echoue(v, HJ_B4, "« %s » en %s : la commande %d déposée, %s", lv, nom_etat(h->st.session_state), (int)h->cmd.op,
               a_un ? "hors READY ou pour un texte que HOTE refuse lui-même" : "une lecture ne dépose rien");
        return;
    }
    if (a_un && (h->cmd.op != op || (arg_connu && h->cmd.arg != arg)))
        echoue(v, HJ_B4, "« %s » : déposé op %d arg %ld, attendu op %d%s%ld", lv, (int)h->cmd.op, (long)h->cmd.arg, (int)op,
               arg_connu ? " arg " : " (arg libre) ", arg_connu ? (long)arg : 0L);
}

/* Le premier accusé de la commande déposée et la réponse immédiate : ACK_ACCEPTED -> `acc` (ou la réponse différée) ;
 * ACK_REJECTED -> une réponse en `er` (B4 : la réponse de HOTE ne contredit pas la décision de CMD). */
static void accord(hote_verdict_t *v, const hote_ligne_t *h, const char *rep, bool later,
                   const char *const *acc)
{
    if (!h->depots || !h->premier_vu) return;
    if (h->premier.result == ACK_ACCEPTED) {
        if (!later && !parmi(rep, acc))
            echoue(v, HJ_B4, "« %s » acceptée par la session (ACK_ACCEPTED), répond « %s »", lv, rv);
    } else if (later || strncmp(rep, "er", 2)) {
        echoue(v, HJ_B4, "« %s » refusée par la session (%d, raison %d), répond « %s »", lv, (int)h->premier.result,
               (int)h->premier.reason, later ? "(différée)" : rv);
    }
}

void hote_juge(hote_verdict_t *v, hote_modele_t *m, const hote_ligne_t *h, const char *l, const char *rep,
               const char *fin, bool later, bool finale)
{
    static const char *const OK[] = {"ok", NULL};
    const bsk_status_t *st = &h->st;
    const uint8_t e = st->session_state;
    const bool ready = e == SESSION_READY;
    const bool moon = l[0] == ':';
    const bool getter = h->fam >= F_ML_GP && h->fam <= F_ML_GH;
    char w[160];
    size_t n;
    v->k = -1;
    lisible(l, lv, sizeof lv);
    lisible(rep, rv, sizeof rv);
#define RATE(...) do { echoue(v, __VA_ARGS__); return; } while (0)

    /* ── la fin (PROTOCOL.md § 1 règles 1 et 2, § 4 ; bsk_host.h) ── */
    if (later) {
        if (!hote_differable(h->fam)) echoue(v, HJ_FIN, "« %s » : BSK_HOST_LATER pour une ligne qui n'attend rien", lv);
        else if (!h->depots || !h->premier_vu || h->premier.result != ACK_ACCEPTED)
            echoue(v, HJ_B4, "« %s » : la réponse attend une commande que la session n'a pas acceptée", lv);
        return;
    }
    if (finale) {
        if (!fin || strcmp(fin, "\n")) RATE(HJ_FIN, "« %s » : la réponse différée finie sans `\\n`", lv);
    } else if (moon) {
        if (getter && (!fin || *fin))
            RATE(HJ_FIN, "« %s » : %s, attendu la réponse sans fin de ligne", lv, fin ? "une fin" : "aucune réponse");
        if (!getter && fin) RATE(HJ_FIN, "« %s » : une réponse « %s », attendu aucune", lv, rv);
    } else if (!fin || strcmp(fin, "\n")) {
        RATE(HJ_FIN, "« %s » : %s, attendu une ligne finie par `\\n`", lv, fin ? "une autre fin" : "aucune réponse");
    }

    /* ── ce que la ligne dépose (B4 (a), spec § 4.5.1) ── */
    if (!finale) {
        switch (h->fam) {
        case F_FN:
        case F_M: {
            uint32_t x;
            bool rel = h->fam == F_FN && (l[1] == '+' || l[1] == '-'), lisible = chiffres(l + 1 + rel, &x);
            int64_t c = rel ? (int64_t)st->focus_position + (l[1] == '-' ? -(int64_t)x : (int64_t)x) : 0;
            bool texte_ok = lisible && x <= 0xFFFF && !(rel && st->position_valid && (c < 0 || c > 0xFFFF));
            admission(v, h, l, ready && texte_ok, false);
            break;
        }
        case F_AX:
            admission(v, h, l, false, ready && (st->capabilities & CAP_APERTURE));   /* le texte : jugé par HOTE (strtod) */
            if (h->depots == 0 && ready && (est(rep, "ok") || est(rep, "er busy")))
                echoue(v, HJ_B4, "« %s » : « %s » sans rien de déposé", lv, rv);
            break;
        case F_JS:
        case F_JX: admission(v, h, l, ready, false); break;
        case F_JG: admission(v, h, l, ready && st->mark_valid, false); break;
        case F_CUSTOM: admission(v, h, l, ready, false); break;
        case F_Q:
        case F_P0:
        case F_ML_FQ: admission(v, h, l, true, false); break;
        case F_P1: admission(v, h, l, e == SESSION_OFF || e == SESSION_FAULT, false); break;
        case F_B: admission(v, h, l, !(e == SESSION_OFF && !st->held), false); break;
        case F_ML_FG: admission(v, h, l, false, true); break;
        default: admission(v, h, l, false, false); break;
        }
        if (v->k >= 0) return;
    }
    if (!fin) return;                                              /* rien d'écrit */

    /* ── la réponse écrite (PROTOCOL.md § 1 règles 1, 4 et 5 ; bsk_host.h) ── */
    n = strlen(rep);
    if (n >= BSK_HOST_REPLY_MAX - 1)
        RATE(HJ_TAMPON, "« %s » : une réponse de %zu octets, le tampon plein", lv, n);
    for (size_t i = 0; i < n; i++)
        if ((unsigned char)rep[i] < 0x20 || rep[i] == 0x7F)
            RATE(HJ_LIGNE, "« %s » : l'octet %02X dans la réponse « %s »", lv, (unsigned char)rep[i], rv);
    if (figee(l) && n > 15) RATE(HJ_LONGUEUR, "« %s » : « %s », %zu caractères", lv, rv, n);
    if (!strncmp(rep, "er", 2) && strstr(rep, "ok") && h->fam != F_I)
        RATE(HJ_OK, "« %s » : « %s »", lv, rv);

    /* ── la forme, commande par commande (en tête de ce fichier) ── */
#define FORME(cond, ...) do { if (!(cond)) RATE(HJ_FORME, __VA_ARGS__); } while (0)
#define ATTENDU(s) FORME(est(rep, (s)), "« %s » en %s : « %s », attendu « %s »", lv, nom_etat(e), rv, (s))
#define DANS(...) do { static const char *const L_[] = {__VA_ARGS__, NULL}; \
        FORME(parmi(rep, L_), "« %s » en %s : « %s », hors de la liste de PROTOCOL.md", lv, nom_etat(e), rv); } while (0)
    switch (h->fam) {
    case F_V: ATTENDU(HOTE_VERSION); break;
    case F_F:
        if (!ready) { ATTENDU(etat_coeur(e)); break; }
        if (nombre(rep) && !st->position_valid) RATE(HJ_P4, "f : « %s » sans position valide", rv);
        if (!st->position_valid) { ATTENDU("er link pos"); break; }
        snprintf(w, sizeof w, "%ld", (long)st->focus_position);
        ATTENDU(w);
        break;
    case F_FN:
    case F_M: {
        uint32_t x;
        bool rel = h->fam == F_FN && (l[1] == '+' || l[1] == '-');
        int64_t c;
        if (!ready) { ATTENDU(etat_coeur(e)); break; }
        if (!chiffres(l + 1 + rel, &x)) { ATTENDU("er range num"); break; }
        c = (int64_t)st->focus_position + (l[1] == '-' ? -(int64_t)x : (int64_t)x);
        if (x > 0xFFFF || (rel && st->position_valid && (c < 0 || c > 0xFFFF))) { ATTENDU("er range 65535"); break; }
        DANS("ok", "er busy", "er range limits");
        accord(v, h, rep, false, OK);
        break;
    }
    case F_R:
        if (!ready) { ATTENDU(etat_coeur(e)); break; }
        if (!st->limits_valid) {
            if (strchr(rep, '-') && rep[0] >= '0' && rep[0] <= '9')
                RATE(HJ_P4, "r : « %s » sans bornes valides", rv);
            ATTENDU("er link limits");
            break;
        }
        snprintf(w, sizeof w, "%ld-%ld", (long)st->focus_min, (long)st->focus_max);
        ATTENDU(w);
        break;
    case F_E: ATTENDU(e_oui(st) ? "y" : "n"); break;
    case F_G: DANS("ok", "aborted", "stall", "unconfirmed", "-"); break;
    case F_A:
        if (!ready) { ATTENDU(etat_coeur(e)); break; }
        if (plage_a(rep) && !(st->capabilities & CAP_APERTURE))
            RATE(HJ_P4, "a : « %s » sans CAP_APERTURE", rv);
        if (!(st->capabilities & CAP_APERTURE)) { ATTENDU("er nocap ap"); break; }
        FORME(plage_a(rep), "a : « %s », attendu <f>-<f>", rv);
        break;
    case F_AX:
        if (!ready) { ATTENDU(etat_coeur(e)); break; }
        if (!(st->capabilities & CAP_APERTURE)) { ATTENDU("er nocap ap"); break; }
        DANS("ok", "er range ap", "er range num", "er busy", "er nocap ap");
        accord(v, h, rep, false, OK);
        break;
    case F_D: ATTENDU(ready ? "-" : etat_coeur(e)); break;
    case F_S: ATTENDU("ok"); break;
    case F_C: ATTENDU(ready ? "ok" : etat_coeur(e)); break;
    case F_O:
        if (!lecture_servie(st)) { ATTENDU(lecture_refus(e)); break; }
        if (fnum(rep) && (!st->aperture_current || !(st->capabilities & CAP_APERTURE_READBACK)))
            RATE(HJ_P4, "o : « %s », l'ouverture %04X, cap %lX", rv, st->aperture_current,
                                (unsigned long)st->capabilities);
        if (!(st->capabilities & CAP_APERTURE_READBACK) || !(st->capabilities & CAP_APERTURE)) { ATTENDU("er nocap ap"); break; }
        if (!st->aperture_current) { ATTENDU("er link aperture"); break; }
        FORME(fnum(rep), "o : « %s », attendu <f>", rv);
        break;
    case F_L: ATTENDU("er nocap focal"); break;
    case F_J:
        if (!lecture_servie(st)) { ATTENDU(lecture_refus(e)); break; }
        if (!st->mark_valid) { ATTENDU("-"); break; }
        snprintf(w, sizeof w, "%ld", (long)st->mark_position);
        ATTENDU(w);
        break;
    case F_JS:
    case F_JX:
        if (finale) { DANS("ok", "er store"); break; }
        if (!ready) { ATTENDU(etat_autre(e)); break; }
        DANS("ok", "er store", "er busy", "er range limits", "er nocap");
        {
            static const char *const ACC[] = {"ok", "er store", NULL};
            accord(v, h, rep, false, ACC);
        }
        break;
    case F_JG:
        if (!ready) { ATTENDU(etat_autre(e)); break; }
        if (!st->mark_valid) { ATTENDU("er range nomark"); break; }
        DANS("ok", "er busy", "er range limits");
        accord(v, h, rep, false, OK);
        break;
    case F_I:
        if (!lecture_servie(st)) { ATTENDU(lecture_refus(e)); break; }
        if (st->lens_name[0]) { ATTENDU(st->lens_name); break; }
        if (!st->lens_id_product) { ATTENDU("-"); break; }
        snprintf(w, sizeof w, "#%lu", (unsigned long)st->lens_id_product);
        FORME(est(rep, w) || (rep[0] && rep[0] != '#' && !strchr(rep, '"') && !est(rep, "-")),
              "i : « %s », attendu un nom ou %s", rv, w);
        break;
    case F_K:
        snprintf(w, sizeof w, "%u", m->plafond);
        ATTENDU(w);
        break;
    case F_KN: {
        uint32_t x;
        if (chiffres(l + 1, &x) && x <= 100) {
            ATTENDU("ok");
            m->plafond = x;
        } else {
            ATTENDU("er range 0 100");
        }
        break;
    }
    case F_Q:
        if (finale) {
            FORME(est(rep, "ok") || est(rep, "er busy") || er_link(rep), "q (différée) : « %s »", rv);
            break;
        }
        if (e == SESSION_HOMING) { ATTENDU("er busy home"); break; }
        if (e == SESSION_IDENTIFYING) { DANS("ok", "er busy home"); break; }
        if (e != SESSION_READY && e != SESSION_RESTORING) { ATTENDU("ok"); break; }
        FORME(est(rep, "ok") || est(rep, "er busy") || er_link(rep), "q en %s : « %s »", nom_etat(e), rv);
        if (h->premier_vu && h->premier.result == ACK_REJECTED)
            echoue(v, HJ_B4, "q en %s refusé par la session (raison %d), hors d'un homing", nom_etat(e), (int)h->premier.reason);
        break;
    case F_P0:
        if (sequence(e)) { ATTENDU("er busy boot"); break; }
        DANS("ok", "er busy");
        accord(v, h, rep, false, OK);
        break;
    case F_P1:
        if (e != SESSION_OFF && e != SESSION_FAULT) { ATTENDU("ok"); break; }
        DANS("ok", "er busy");
        accord(v, h, rep, false, OK);
        break;
    case F_B:
        if (e == SESSION_OFF && !st->held) { ATTENDU("er nolens"); break; }
        DANS("ok", "er busy move");
        accord(v, h, rep, false, OK);
        break;
    case F_Z: ATTENDU("-"); break;
    case F_X:
        snprintf(w, sizeof w, "poweron %llu", (unsigned long long)(h->now_us / 1000u));
        ATTENDU(w);
        break;
    case F_T: {
        char pq[120];
        FORME(ligne_t(rep, st, pq, sizeof pq), "t : %s, « %s »", pq, rv);
        break;
    }
    case F_NOCAP:
    case F_MAJ: ATTENDU("er nocap"); break;
    case F_DEBUG: {
        const char *nv = strstr(rep, " nvs=");
        FORME(!strncmp(rep, "ok reset=poweron dropped=", 25) && nv && (est(nv, " nvs=ok") || est(nv, " nvs=erased") || est(nv, " nvs=er")),
              "DEBUG : « %s »", rv);
        break;
    }
    case F_LOG: {
        int mode = hote_log(l);
        if (mode < 0) { ATTENDU("er range LOG ON|ALL|OFF"); break; }
        snprintf(w, sizeof w, "ok log=%d all=%d", mode != 0, mode == 2);
        m->journal = mode;
        ATTENDU(w);
        break;
    }
    case F_CUSTOM_MAL: ATTENDU("er range CUSTOM READ|WRITE <0-2><0-2>"); break;
    case F_CUSTOM:
        if (finale) {
            bool donnees = n == 2 + 16 * 3 && !strncmp(rep, "ok", 2);
            for (size_t i = 2; donnees && i < n; i += 3)
                donnees = rep[i] == ' ' && isxdigit((unsigned char)rep[i + 1]) && isxdigit((unsigned char)rep[i + 2]) &&
                          !islower((unsigned char)rep[i + 1]) && !islower((unsigned char)rep[i + 2]);
            if (!donnees) DANS("er link timeout", "er link framing", "er link aborted", "er link bus", "er refused", "er busy");
            break;
        }
        if (!ready) { ATTENDU(etat_autre(e)); break; }
        DANS("er nocap custom", "er busy", "er link desync", "er refused", "er link bus");
        accord(v, h, rep, false, OK);
        break;
    case F_ML_GP:
        FORME(hex4(rep), ":GP# : « %s »", rv);
        if (st->position_valid) {
            snprintf(w, sizeof w, "%04X#", (unsigned)(st->focus_position & 0xFFFF));
            ATTENDU(w);
        }
        break;
    case F_ML_GN: FORME(hex4(rep), ":GN# : « %s »", rv); break;
    case F_ML_GT: ATTENDU("0000#"); break;
    case F_ML_GI: ATTENDU(e_oui(st) ? "01#" : "00#"); break;
    case F_ML_GV: ATTENDU("10"); break;
    case F_ML_GC: ATTENDU("00#"); break;
    case F_ML_GD: ATTENDU("02#"); break;
    case F_ML_GH: ATTENDU("00#"); break;
    default: break;
    }
#undef FORME
#undef ATTENDU
#undef DANS
#undef RATE
}

/* ─────────────────────────── le générateur ─────────────────────────── */

static const char *tire(fz_rng_t *r, const char *const *t, size_t n) { return t[fz_below(r, (uint32_t)n)]; }
#define TIRE(t) tire(r, t, sizeof t / sizeof t[0])

/* Un nombre aux bornes : de 0..65535 et au-delà, les bornes du 135 (13723..30988, resserrées à 13878..30983), les
 * chiffres en trop, et ce qui n'en est pas. */
static const char *const NOMBRES[] = {
    "0", "1", "5", "9", "10", "100", "101", "255", "256", "1000", "13723", "13877", "13878", "13879", "14620", "16384",
    "20000", "25000", "30982", "30983", "30984", "30988", "32767", "32768", "65534", "65535", "65536", "70000", "99999",
    "100000", "999999", "4294967295", "4294967296", "18446744073709551616", "00000", "065535", "0000000000000000016384",
    "", "x", "1x", "1 ", " 1", "1.5", "0x10", "-", "+", "1e3", "+5", "-5",
};
static const char *const FNUM[] = {"1.8", "1.83", "1.75", "1.85", "2", "2.8", "4", "5.6", "8", "11", "16", "22", "22.05",
                                   "22.1", "0.5", "256", "0", "-1", "+1", "-0.3", "+0.3", "+100", "-100", "nan", "inf",
                                   "-inf", "1e308", "1e-308", ".5", "5.", "0x1p3", "1,8", "", "+", "-", " ", "5.6x"};
static const char *const SUFFIXES[] = {"", "", "", "", " ", "x", "m", "0", "1", "\t", "+", "s"};
static const char *const SIMPLES[] = {"v", "f", "r", "e", "g", "a", "d", "c", "o", "l", "i", "n", "w", "h", "z", "x",
                                      "t", "u", "ux", "uf", "ua", "j", "js", "jx", "jg", "jz", "jss", "q", "p", "p0", "p1",
                                      "p2", "pl0", "pl1", "pm0", "pm1", "b", "cm", "ms", "mx", "mg", "m", "k", "s", "y", "0",
                                      "!", "~", "\x7f", "\x80", "\xff", "é"};
static const char *const MOTS_LOG[] = {"ON", "ALL", "OFF", "on", "All", "oFf", "ONN", "", "ON OFF", "1", "O N"};
static const char *const MOTS_CUSTOM[] = {"READ", "read", "Read", "WRITE 10", "WRITE 00", "WRITE 22", "WRITE 21",
                                          "write 12", "WRITE 3", "WRITE 1", "WRITE 100", "WRITE 23", "WRITE 30", "WRITE 1a",
                                          "WRITE", "", "READ 1", "WRITE  01", "WRITE\t20", "READX"};
static const char *const BLANCS[] = {" ", " ", " ", "\t", "  ", " \t ", ""};
static const char *const FINS[] = {"", "", "", " ", "\t", " x", "  "};
static const char *const MAJ[] = {"DEBUG", "DEBUG ", "DEBUGX", "DEBUG x", "Debug", "X", "HELP", "OK", "ERR", "LOGON",
                                  "CUSTOMREAD", "L", "D"};
static const char *const MOON[] = {"GP", "GN", "GI", "GV", "GT", "GC", "GD", "GH", "FG", "FQ", "SP", "SD", "SH", "SF",
                                   "SC", "PO", "C", "+", "-", "", "G", "GPX", "GP ", "gp", "FG ", "S"};
static const char *const HEX[] = {"0000", "FFFF", "10000", "3E80", "3e80", "", "zz", "-1", "+10", "0x10", " 3E80",
                                  "  ", "5000", "6000", "4000", "FFFFFFFFFFFFFFFFFF", "35A6"};

static size_t ecrit(uint8_t *b, size_t n, size_t cap, const char *s)
{
    size_t k = strlen(s);
    if (n + k > cap) k = cap - n;
    memcpy(b + n, s, k);
    return n + k;
}

size_t hote_octets(fz_rng_t *r, uint8_t *b, size_t cap)
{
    size_t n = 0;
    uint32_t q = fz_below(r, 100);
    bool moon = false;
    if (q < 18) {                                                  /* des octets arbitraires, que le cadrage découpe */
        static const uint8_t SPEC[] = {0, '\n', '\r', '#', ':', ' ', '\t', 0x7F, 0x80, 0xFF, 'A', 'f', '1', 'j', 'q'};
        uint32_t k = fz_chance(r, 7, 8) ? 1 + fz_below(r, 40) : 1 + fz_below(r, 300);
        for (uint32_t i = 0; i < k && n < cap; i++)
            b[n++] = fz_chance(r, 1, 3) ? SPEC[fz_below(r, sizeof SPEC)] : (uint8_t)fz_next(r);
        if (n < cap && fz_chance(r, 3, 4)) b[n++] = fz_chance(r, 1, 4) ? '#' : '\n';
        return n;
    }
    if (q < 21) {                                                  /* une ligne aux bornes du cadrage : 254 à 257 octets */
        uint32_t k = 254 + fz_below(r, 4);
        b[n++] = fz_chance(r, 1, 2) ? 'f' : ':';
        moon = b[0] == ':';
        while (n < k && n < cap) b[n++] = moon ? 'G' : '0';
        if (n < cap) b[n++] = moon ? '#' : '\n';
        return n;
    }
    if (fz_chance(r, 1, 10)) n = ecrit(b, n, cap, TIRE(BLANCS));     /* des blancs en tête, que le cadrage saute */
    if (q < 45) {                                                  /* les lettres simples, et leurs suffixes */
        n = ecrit(b, n, cap, TIRE(SIMPLES));
        if (fz_chance(r, 1, 4)) n = ecrit(b, n, cap, TIRE(SUFFIXES));
    } else if (q < 60) {                                           /* f, m, et leurs nombres */
        static const char *const P[] = {"f", "f", "f+", "f-", "m", "f++", "f+-", "f +", "m+", "m-"};
        n = ecrit(b, n, cap, TIRE(P));
        n = ecrit(b, n, cap, TIRE(NOMBRES));
    } else if (q < 66) {
        n = ecrit(b, n, cap, "a");
        n = ecrit(b, n, cap, TIRE(FNUM));
    } else if (q < 70) {
        static const char *const P[] = {"k", "s"};
        n = ecrit(b, n, cap, TIRE(P));
        n = ecrit(b, n, cap, TIRE(NOMBRES));
    } else if (q < 77) {
        n = ecrit(b, n, cap, fz_chance(r, 9, 10) ? "LOG" : "Log");
        n = ecrit(b, n, cap, TIRE(BLANCS));
        n = ecrit(b, n, cap, TIRE(MOTS_LOG));
        n = ecrit(b, n, cap, TIRE(FINS));
    } else if (q < 84) {
        n = ecrit(b, n, cap, "CUSTOM");
        n = ecrit(b, n, cap, TIRE(BLANCS));
        n = ecrit(b, n, cap, TIRE(MOTS_CUSTOM));
        n = ecrit(b, n, cap, TIRE(FINS));
    } else if (q < 87) {
        n = ecrit(b, n, cap, TIRE(MAJ));
    } else {                                                       /* Moonlite */
        moon = true;
        n = ecrit(b, n, cap, ":");
        if (fz_chance(r, 1, 4)) {
            n = ecrit(b, n, cap, fz_chance(r, 1, 4) ? "SN " : "SN");
            n = ecrit(b, n, cap, TIRE(HEX));
        } else {
            n = ecrit(b, n, cap, TIRE(MOON));
        }
    }
    if (n < cap && fz_chance(r, 1, 12)) b[n++] = '\r';               /* retiré par le cadrage */
    if (n < cap) b[n++] = moon && fz_chance(r, 15, 16) ? '#' : '\n';   /* un `:` fini par `\n` : orphelin */
    return n;
}
