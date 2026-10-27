/* SOURCE: spec de l'atelier § 3.1 et § 9 — RESTORING, le retour à la marque ou à la position de reprise
 * AUTHOR: engineer
 * DATE: 2026-10-04
 * STATUS: actif — C pur, sans ESP-IDF : compilé aussi par les tests hôte ; ticket #674 : la position de reprise
 *
 * Le retour en fin de démarrage (restore.h). ARRIVED sur la cible, STALLED, q, ou la clé de la marque ou les bornes du
 * 0x06 lues autres mènent à READY. Rien n'est écrit dans le magasin.
 *
 * La position de reprise (bsk_session_keep_t, décision de l'humain du 2026-10-22) : en READY, la cible de la commande de
 * mouvement en vol, sinon la dernière position lue au 0x06 (un goto, la bague, le bouton), avec la clé de la marque et le
 * sens de l'arrivée ; rien n'y est écrit sans clé ou sans 0x06 (elle garde la dernière). Elle survit à l'oubli de la session :
 * la fin du démarrage qui suit une perte la vise, pour la même clé (le même objectif, la même focale nominale), au lieu
 * de la marque. Une mise sous tension de la carte (keep effacée au bsk_session_init sans p->warm), la retombée de D2,
 * CMD_DETACH, CMD_CLEAR_FAULT et CMD_ATTACH l'effacent (restore_drop) : ces démarrages vont à la marque. */
#include "restore.h"

#include <string.h>

#include "bsk_journal.h"
#include "bsk_store.h"
#include "init.h"
#include "lens_rx.h"
#include "mark.h"
#include "motion.h"

#define X_135          200    /* le dépassement du 135, porté par son firmware (7_Docs/E-Mount/samyang.md § 5.6) */

static struct {
    bsk_session_keep_t *keep;  /* la position de reprise (restore_init) */
    bool        have;          /* restore_plan : une cible, la position de reprise ou la marque */
    bool        last;          /* ... c'est la position de reprise */
    bsk_mark_t  m;             /* ... elle, et le sens de l'arrivée */
    int32_t     x, via;        /* ... le dépassement, la cible du premier goto (la marque : le trajet direct) */
    int32_t     rs_mark;       /* RESTORING : la cible visée */
    bool        rs_via;        /* ... le goto en cours est celui du dépassement */
    char        rs_key[BSK_STORE_KEY_MAX];   /* ... la clé de la marque et les bornes du 0x06 du départ (restore_plan) */
    int32_t     rs_fmin, rs_fmax;
    const char *rs_end;        /* ... la cause de la sortie vers READY, pour le journal */
} s;

static bsk_session_keep_t own;   /* sans mémoire donnée (p->keep NULL) */

/* FNV-1a sur la clé, la position et le sens : une écriture interrompue (un redémarrage de la carte au milieu de
 * restore_note, préemptée par une tâche qui panique) mêlerait deux positions ; elle ne se relit pas. */
static uint32_t sum_of(const bsk_session_keep_t *k)
{
    uint32_t h = 2166136261u;
    uint8_t b[sizeof k->key + 3];
    memcpy(b, k->key, sizeof k->key);
    b[sizeof k->key] = (uint8_t)k->pos;
    b[sizeof k->key + 1] = (uint8_t)(k->pos >> 8);
    b[sizeof k->key + 2] = k->dir;
    for (size_t i = 0; i < sizeof b; i++) h = (h ^ b[i]) * 16777619u;
    return h;
}

/* Relue seulement après un redémarrage sans mise sous tension (warm) : sinon ce que la mémoire porte n'est jamais lu comme
 * une position. Relue, elle doit porter sa somme et un sens de bsk_approach_t : une mémoire qui passerait la somme par
 * hasard ne fait pas lire hors de la table des sens du journal (bsk_journal_restore), ce qui, s'il redémarrait la carte,
 * recommencerait à chaque redémarrage sur la même mémoire. Sa clé n'est comparée qu'à celle de mark.c, 13 caractères
 * (last_get) : strcmp s'arrête au 14e octet, dans la clé, terminée ou non. */
void restore_init(bsk_session_keep_t *keep, bool warm)
{
    s.keep = keep ? keep : &own;
    if (warm && s.keep->sum == sum_of(s.keep) && s.keep->dir <= BSK_APPROACH_DECREASING) return;
    restore_drop();
}

void restore_drop(void) { memset(s.keep, 0, sizeof *s.keep); }

/* Le sens de l'arrivée : celui de la commande en vol, de sa position de dépôt vers sa cible ; à l'arrêt, ou une cible
 * égale à cette position, le dernier changement de position vu au 0x06 (mark_dir). */
void restore_note(void)
{
    const lens_rx_t *l = lens_rx();
    const char *k = mark_key();
    bsk_session_keep_t n;
    uint16_t to, from;
    if (!k[0] || !l->have06) return;
    memset(&n, 0, sizeof n);
    memcpy(n.key, k, strlen(k));                    /* 13 caractères : la clé de mark.c */
    n.pos = l->pos;
    n.dir = mark_dir();
    if (motion_aim(&to, &from)) {
        n.pos = to;
        if (to != from) n.dir = to > from ? BSK_APPROACH_INCREASING : BSK_APPROACH_DECREASING;
    }
    n.sum = sum_of(&n);
    *s.keep = n;
}

/* La position de reprise de la clé courante. */
static bool last_get(bsk_mark_t *m)
{
    const char *k = mark_key();
    if (!k[0] || strcmp(k, s.keep->key)) return false;
    m->position = s.keep->pos;
    m->approach_dir = s.keep->dir;
    return true;
}

/* La cible : la position de reprise de la clé courante, sinon la marque. La recette, la même pour l'une et l'autre :
 * arriver sur la cible du côté de son sens (rangé avec la marque, celui de l'arrivée pour la position de reprise), en
 * décroissant s'il est inconnu ; si le trajet direct arriverait de l'autre côté, un goto à la cible ± X de ce côté-là,
 * borné aux bornes du 0x06, puis un goto à la cible ; sinon un seul goto. Déjà sur la cible, le trajet direct n'arrive de
 * ce côté que si le dernier sens vu au 0x06 (mark_dir) est celui-là : un autre, ou inconnu, et le jeu n'est pas repris ;
 * le dépassement, alors. X : 200 pas sur le 135 reconnu par l'init (init_lens), sinon 1 % de la course du 0x06, au
 * moins 1 — le 24 mm aussi, bien que la spec de l'atelier § 9 le dise à 50 (décision prise ainsi). La cible passe ici,
 * une fois, l'admission des gotos (motion_admit) : refusée, pas de retour. Une position de reprise refusée suit la règle
 * de la marque (décision de l'architecte), sans repli sur la marque. */
bool restore_plan(void)
{
    const lens_rx_t *l = lens_rx();
    bsk_mark_t m;
    int32_t x, via;
    bool inc, taken;
    s.last = last_get(&m);
    s.have = s.last || mark_get(&m);
    if (!s.have) return false;
    s.m = m;
    if (!motion_admit(m.position)) return false;
    x = init_lens() == BSK_BENCH_SAMYANG135 ? X_135 : (l->fmax - l->fmin) / 100;
    if (x < 1) x = 1;
    inc = m.approach_dir == BSK_APPROACH_INCREASING;   /* inconnu ou invalide : en décroissant (samyang.md § 5.6) */
    taken = l->pos == m.position && mark_dir() == (inc ? BSK_APPROACH_INCREASING : BSK_APPROACH_DECREASING);
    via = m.position;
    if (inc && l->pos >= m.position && !taken) via = m.position - x < l->fmin ? l->fmin : m.position - x;
    if (!inc && l->pos <= m.position && !taken) via = m.position + x > l->fmax ? l->fmax : m.position + x;
    /* via == la cible : le trajet direct, ou un dépassement borné qui tombe sur elle (une cible à une borne) : un goto */
    s.x = x;
    s.via = via;
    memcpy(s.rs_key, mark_key(), sizeof s.rs_key);
    s.rs_fmin = l->fmin;
    s.rs_fmax = l->fmax;
    return true;
}

void restore_skip(const char *nomark)
{
    if (!s.have) bsk_journal_restore_skip(nomark, false, NULL);
    else bsk_journal_restore_skip("limit", s.last, &s.m);
}

void restore_start(uint64_t now)
{
    bsk_journal_restore(&s.m, s.last, s.x, s.via == s.m.position ? -1 : s.via);
    s.rs_mark = s.m.position;
    s.rs_via = s.via != s.m.position;
    motion_restore((uint16_t)s.via, now);
}

/* La clé de la marque ou les bornes du 0x06 lues autres que celles du départ : la cible visée n'est plus celle de la
 * focale courante, ou plus dans ses bornes. Une clé vidée ("" : le 0x05 arrêté) ou des bornes oubliées (have06
 * faux : le 0x06 arrêté) n'en sont pas : cette perte a sa règle, la position et les bornes inconnues pendant le retour
 * (PROTOCOL.md § 2) ; revenues les mêmes, rien n'a changé. Des bornes lues invalides, le 0x06 courant (lim faux), en
 * sont : publiées 0 et 0, elles diffèrent toujours de celles du départ, valides ; le retour est abandonné. */
static bool changed(void)
{
    const lens_rx_t *l = lens_rx();
    const char *k = mark_key();
    return (k[0] && strcmp(k, s.rs_key)) || (l->have06 && (l->fmin != s.rs_fmin || l->fmax != s.rs_fmax));
}

/* La ligne `* restore` de la sortie est écrite par le superviseur, avec la cause posée ici. Un changement (changed)
 * abandonne le retour à tout moment, avant tout autre goto, comme q (cmd.c:stop) : le 0x1C, READY qu'il soit parti ou non ;
 * aucun trajet n'est recalculé pour la focale nouvelle. */
restore_step_t restore_track(uint64_t now)
{
    bsk_motion_state_t m = motion_state();
    if (changed()) {
        (void)motion_stop(now);
        s.rs_end = "changed";
        return RESTORE_END;
    }
    if (m == MOTION_ARRIVED && s.rs_via) {
        s.rs_via = false;
        motion_restore((uint16_t)s.rs_mark, now);
        return RESTORE_NEXT;
    }
    if (m == MOTION_ARRIVED) s.rs_end = "arrived";
    else if (m == MOTION_STALLED) s.rs_end = "stall";
    else return RESTORE_WAIT;
    return RESTORE_END;
}

void restore_stop(void) { s.rs_end = "stop"; }

const char *restore_cause(void) { return s.rs_end; }
