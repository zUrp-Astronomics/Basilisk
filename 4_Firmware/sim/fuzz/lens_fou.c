/* L'objectif fou (lens_fou.h) : l'adaptateur qui enveloppe un faux objectif, saisit ses émissions sous les règles du
 * banc, en ajoute et force LENS_CS. */
#include "lens_fou.h"

#include <string.h>

#include "phy_common.h"

/* ─────────────────────────── la sortie, un seul fil ─────────────────────────── */

static void put(fou_t *f, uint64_t t, lens_sim_out_kind_t kind, uint8_t v)
{
    if (t < f->free) t = f->free;
    if (f->qn == FOU_Q) return;                     /* au-delà, ignoré, comme phy_sim_lens_raw au-delà de son tampon */
    f->q[(f->qh + f->qn) % FOU_Q] = (lens_sim_out_t){t, kind, v};
    f->qn++;
    f->free = t;
}

/* LENS_CS vers la carte : `force` passe outre le mode ; un niveau déjà sorti n'est pas redit. */
static void put_cs(fou_t *f, uint64_t t, bool high, bool force)
{
    if (!force && ((f->cs_mode == FOU_CS_HAUTE && !high) || (f->cs_mode == FOU_CS_BASSE && high))) return;
    if (high == f->out_cs) return;
    f->out_cs = high;
    put(f, t, LENS_SIM_LENS_CS, high ? 1 : 0);
}

/* Une émission entière à partir de `t` : LENS_CS levée (si `cs`), les octets un tous les byte_us, LENS_CS retombée
 * lens_cs_tail_us après le dernier, ou avant les `late` derniers. */
static void emit(fou_t *f, uint64_t t, const uint8_t *b, size_t n, bool cs, size_t late)
{
    uint64_t by = lens_sim_byte_us(f->inner);
    if (late > n) late = n;
    if (t < f->free) t = f->free;
    if (cs) put_cs(f, t, true, false);
    for (size_t i = 0; i < n - late; i++) {
        t += by;
        put(f, t, LENS_SIM_BYTE, b[i]);
    }
    if (cs) {
        if (!late) t += lens_sim_lens_cs_tail_us(f->inner);
        put_cs(f, t, false, false);
    }
    for (size_t i = n - late; i < n; i++) {
        t += by;
        put(f, t, LENS_SIM_BYTE, b[i]);
    }
}

/* ─────────────────────────── une émission saisie ─────────────────────────── */

static void store(fou_t *f, const uint8_t *b, size_t n)
{
    if (n > BSK_FRAME_MAX) return;
    memcpy(f->mem[f->mem_next].b, b, n);
    f->mem[f->mem_next].n = n;
    f->mem_next = (uint8_t)((f->mem_next + 1) % FOU_MEM);
}

/* La règle de l'émission (type `type`, 0 si illisible) : la première qui la vise ; FOU_RANGE range sans s'arrêter. Une
 * règle à compte le perd d'un, et part à zéro. Rend son indice, -1 sans règle. */
static int rule_for(fou_t *f, uint8_t type, const uint8_t *b, size_t n, fou_rule_t *out)
{
    for (uint8_t i = 0; i < f->n_rules; i++) {
        fou_rule_t *r = &f->rules[i];
        if (r->type && r->type != type) continue;
        *out = *r;
        if (r->n && --r->n == 0) {
            memmove(&f->rules[i], &f->rules[i + 1], (size_t)(f->n_rules - i - 1) * sizeof *r);
            f->n_rules--;
            i--;
        }
        if (out->act == FOU_RANGE) {
            store(f, b, n);
            f->applied[FOU_RANGE]++;
            continue;
        }
        return i;
    }
    return -1;
}

static void emission(fou_t *f, uint64_t t_end)
{
    bsk_frame_t fr;
    uint8_t b[BSK_FRAME_MAX + 256];
    size_t n = f->cap_n;
    bool ok = n <= BSK_FRAME_MAX && fr_decode(f->cap, n, &fr) == E_OK;
    fou_rule_t r;
    bool was_held = f->held;
    f->capturing = false;
    if (rule_for(f, ok ? fr.msg[0] : 0, f->cap, n, &r) < 0) {
        emit(f, t_end, f->cap, n, true, 0);
    } else {
        f->applied[r.act]++;
        switch (r.act) {
        case FOU_RETARD: emit(f, t_end + r.v, f->cap, n, true, 0); break;
        case FOU_PERTE: put_cs(f, t_end, false, false); break;
        case FOU_DOUBLE:
            emit(f, t_end, f->cap, n, true, 0);
            emit(f, f->free + r.v, f->cap, n, true, 0);
            break;
        case FOU_ECHANGE:
            if (was_held) {                         /* deux retenues de suite : la seconde part, puis la première */
                emit(f, t_end, f->cap, n, true, 0);
                break;
            }
            memcpy(f->held_b, f->cap, n);
            f->held_n = n;
            f->held = true;
            put_cs(f, t_end, false, false);
            return;
        case FOU_CHAMP:
        case FOU_LONGUEUR:
        case FOU_CLASSE: {
            uint8_t cls = ok ? fr.cls : 0;
            if (!ok) {                              /* illisible : rien à muter à la trame */
                emit(f, t_end, f->cap, n, true, 0);
                break;
            }
            if (r.act == FOU_CHAMP && (r.v >> 16) < fr.len) {
                uint16_t k = (uint16_t)(r.v >> 16);
                fr.msg[k] = (r.v & 0x100) ? (uint8_t)(fr.msg[k] ^ r.v) : (uint8_t)r.v;
            } else if (r.act == FOU_LONGUEUR) {
                uint16_t len = (uint16_t)(r.v & 0xFF);
                if (len == 0) len = 1;
                if (len > BSK_MSG_MAX) len = BSK_MSG_MAX;
                for (uint16_t i = fr.len; i < len; i++) fr.msg[i] = (uint8_t)(r.v >> 8);
                fr.len = len;
            } else if (r.act == FOU_CLASSE) {
                cls = (uint8_t)r.v;
            }
            n = fr_encode(cls, fr.seq, fr.msg, fr.len, b, sizeof b);
            emit(f, t_end, b, n, true, 0);
            break;
        }
        case FOU_BRUT:
            memcpy(b, f->cap, n);
            if (n) b[(r.v >> 8) % n] ^= (uint8_t)(r.v | 1u);
            emit(f, t_end, b, n, true, 0);
            break;
        case FOU_BOUT: emit(f, t_end, f->cap, r.v < n ? r.v : n, true, 0); break;
        case FOU_TARDIF: emit(f, t_end, f->cap, n, true, r.v ? r.v : 1); break;
        default: emit(f, t_end, f->cap, n, true, 0); break;
        }
    }
    if (was_held && f->held) {                      /* la retenue part derrière la suivante */
        f->held = false;
        emit(f, f->free, f->held_b, f->held_n, true, 0);
    }
}

/* Ce que l'enveloppé vient d'émettre. */
static void from_inner(fou_t *f, const lens_sim_out_t *o)
{
    if (o->kind == LENS_SIM_LENS_CS) {
        bool high = o->v != 0;
        f->inner_cs = high;
        if (!high && f->capturing) emission(f, o->t);
        else put_cs(f, o->t, high, false);
        return;
    }
    if (f->capturing) {
        if (f->cap_n < FOU_CAP) {
            f->cap[f->cap_n++] = o->v;
            return;
        }
        emit(f, o->t, f->cap, f->cap_n, false, 0);  /* trop long pour être saisi : il part tel quel */
        f->capturing = false;
        put(f, o->t, LENS_SIM_BYTE, o->v);
        return;
    }
    if (f->inner_cs && (f->n_rules || f->held)) {
        f->capturing = true;
        f->cap_n = 0;
        f->cap[f->cap_n++] = o->v;
        return;
    }
    put(f, o->t, LENS_SIM_BYTE, o->v);
}

static void pull(fou_t *f)
{
    lens_sim_out_t o;
    while (lens_sim_out(f->inner, &o)) from_inner(f, &o);
}

static void at(fou_t *f, uint64_t t)
{
    if (t > f->now) f->now = t;
}

/* ─────────────────────────── lens_sim.h ─────────────────────────── */

static void op_power(void *l, uint64_t t, bool on)
{
    fou_t *f = l;
    at(f, t);
    lens_sim_power(f->inner, t, on);
    if (!on) {                                      /* hors tension, plus rien ne sort : ce qui attendait est perdu */
        f->qn = 0;
        f->capturing = f->held = false;
        f->free = f->now;
        f->out_cs = true;                           /* la retombée est dite */
        put_cs(f, f->now, false, true);
    }
    pull(f);
}

static bool op_start_powered(void *l, uint64_t t, const lens_sim_powered_t *st)
{
    fou_t *f = l;
    bool ok;
    at(f, t);
    ok = lens_sim_start_powered(f->inner, t, st);
    pull(f);
    return ok;
}

static void op_body_cs(void *l, uint64_t t, bool high)
{
    fou_t *f = l;
    at(f, t);
    lens_sim_body_cs(f->inner, t, high);
    pull(f);
}

static void op_byte(void *l, uint64_t t, uint8_t b)
{
    fou_t *f = l;
    at(f, t);
    lens_sim_byte(f->inner, t, b);
    pull(f);
}

static void op_vd(void *l, uint64_t t)
{
    fou_t *f = l;
    at(f, t);
    lens_sim_vd(f->inner, t);
    pull(f);
}

static void op_advance(void *l, uint64_t t)
{
    fou_t *f = l;
    at(f, t);
    lens_sim_advance(f->inner, t);
    pull(f);
}

static uint64_t op_next_event(const void *l)
{
    const fou_t *f = l;
    uint64_t t = lens_sim_next_event(f->inner);
    if (f->qn && f->q[f->qh].t < t) t = f->q[f->qh].t;
    return t;
}

static bool op_out(void *l, lens_sim_out_t *o)
{
    fou_t *f = l;
    pull(f);
    if (!f->qn || f->q[f->qh].t > f->now) return false;
    *o = f->q[f->qh];
    f->qh = (f->qh + 1) % FOU_Q;
    f->qn--;
    return true;
}

static uint64_t op_byte_us(const void *l) { return lens_sim_byte_us(((const fou_t *)l)->inner); }
static uint64_t op_tail_us(const void *l) { return lens_sim_lens_cs_tail_us(((const fou_t *)l)->inner); }
static bool op_blocked(const void *l) { return lens_sim_blocked(((const fou_t *)l)->inner); }
static bool op_oom(const void *l) { return lens_sim_out_of_model(((const fou_t *)l)->inner); }
static uint32_t op_unmodelled(const void *l, uint8_t type) { return lens_sim_unmodelled(((const fou_t *)l)->inner, type); }
static int32_t op_position(const void *l) { return lens_sim_position(((const fou_t *)l)->inner); }

static const lens_sim_ops_t OPS = {
    "objectif fou", op_power, op_start_powered, op_body_cs, op_byte, op_vd, op_advance, op_next_event, op_out,
    op_byte_us, op_tail_us, op_blocked, op_oom, op_unmodelled, op_position,
};

lens_sim_t lens_sim_fou(fou_t *f) { return (lens_sim_t){&OPS, f}; }

/* ─────────────────────────── le banc ─────────────────────────── */

void fou_init(fou_t *f, lens_sim_t inner)
{
    memset(f, 0, sizeof *f);
    f->inner = inner;
}

void fou_rule(fou_t *f, fou_rule_t r)
{
    if (f->n_rules < FOU_RULES && r.act < FOU_N) f->rules[f->n_rules++] = r;
}

void fou_clear(fou_t *f)
{
    f->n_rules = 0;
    if (f->held) {
        f->held = false;
        emit(f, f->now, f->held_b, f->held_n, true, 0);
    }
    fou_cs(f, FOU_CS_SUIT);
}

void fou_cs(fou_t *f, fou_cs_t mode)
{
    f->cs_mode = mode;
    put_cs(f, f->now, mode == FOU_CS_HAUTE ? true : mode == FOU_CS_BASSE ? false : f->inner_cs, true);
}

void fou_inject(fou_t *f, const uint8_t *b, size_t n, bool cs, size_t late)
{
    f->injected++;
    emit(f, f->now, b, n, cs, late);
}

bool fou_inject_mem(fou_t *f, uint8_t k)
{
    k %= FOU_MEM;
    if (!f->mem[k].n) return false;
    fou_inject(f, f->mem[k].b, f->mem[k].n, true, 0);
    return true;
}

bool fou_idle(const fou_t *f) { return !f->qn && !f->capturing && !f->held; }
