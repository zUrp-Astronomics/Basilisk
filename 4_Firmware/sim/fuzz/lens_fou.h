/* L'objectif fou de F2 (plans/fuzzing.md § 4, l'atelier) : un adaptateur de l'interface commune « faux objectif »
 * (sim/lens_sim.h) qui en enveloppe un autre, le faux 135 ou le faux standard, sans que leur code change. Ce que
 * l'objectif enveloppé émet passe tel quel, sauf ce que les règles posées par le banc saisissent ; le banc y ajoute
 * des émissions à lui (une trame rangée d'une session d'avant, un gabarit, des octets bruts) et force LENS_CS.
 *
 * Il n'imite pas un vrai objectif : il éprouve la carte face à ce que personne n'a prévu (plans/survie.md § 1 : « la
 * cohérence sémantique de ce que dit l'objectif n'est pas à nous »). Ce qu'il ne sait pas représenter, il le refuse comme
 * l'enveloppé (lens_sim.h) : l'état « resté alimenté », le blocage et le hors-modèle sont ceux de l'objectif enveloppé,
 * rendus tels quels ; un banc qui les voit ne juge pas la fin d'un cas.
 *
 * Une émission saisie : de son premier octet à la retombée de LENS_CS qui la suit (la levée de LENS_CS passe tout de
 * suite, sinon la poignée de main attendrait une émission qui ne vient qu'après elle). Une émission n'est saisie que si
 * une règle est posée ou si la ligne est encore occupée par une émission précédente ; elle part alors, transformée ou
 * non, à sa fin d'origine au plus tôt : une émission saisie est donc toujours décalée de sa propre durée, un objectif
 * sans règle ne l'est jamais. Un seul fil : tout ce qui sort est rangé dans l'ordre de l'émission, chaque sortie après
 * la précédente (la ligne occupée retarde la suivante, d'autant). La réception de la carte reçoit ainsi des instants qui
 * ne décroissent pas (le contrat de rx_t, components/phy/phy_common.h).
 *
 * Le contenu se mute à la trame, somme recalculée (fr_encode) : une mutation d'octets sans somme recalculée n'atteint
 * jamais lens_rx (plans/fuzzing.md § 4) ; les octets bruts se mutent à part (FOU_BRUT, FOU_BOUT). */
#ifndef LENS_FOU_H
#define LENS_FOU_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "bsk_phy.h"
#include "lens_sim.h"

/* Ce qu'une règle fait d'une émission saisie. `v` est le paramètre de la règle. */
typedef enum {
    FOU_RETARD,     /* le moment : partie v µs après sa fin d'origine */
    FOU_PERTE,      /* l'ordre : rien ne part (LENS_CS retombe) */
    FOU_DOUBLE,     /* l'ordre : partie deux fois, v µs entre les deux */
    FOU_ECHANGE,    /* l'ordre : retenue, partie après la suivante (ou à fou_clear) */
    FOU_CHAMP,      /* le contenu : l'octet v >> 16 du message (type compris) devient (v & 0xFF), xor si v & 0x100 ;
                       somme recalculée ; hors du message, rien */
    FOU_LONGUEUR,   /* le contenu : le message coupé ou allongé à v & 0xFF octets (complété par v >> 8 & 0xFF) */
    FOU_CLASSE,     /* le contenu : la classe de la trame devient v & 0xFF (somme recalculée) */
    FOU_BRUT,       /* le bus : l'octet (v >> 8) % n de l'émission xor (v | 1) & 0xFF, sans somme recalculée */
    FOU_BOUT,       /* le bus : seuls les v premiers octets partent, un début de trame laissé sans suite */
    FOU_TARDIF,     /* le moment : LENS_CS retombe avant les v derniers octets (le Sony, phy_sim_lens_raw_late) */
    FOU_RANGE,      /* rangée dans la mémoire (fou_inject_mem), partie telle quelle ; ne consomme pas la règle */
    FOU_N
} fou_act_t;

/* Une règle : son action, le type du premier message qu'elle vise (0 : toute émission, une émission illisible
 * comprise), le nombre d'émissions qu'elle prend encore (0 : jusqu'à fou_clear), son paramètre. */
typedef struct {
    uint8_t  act;
    uint8_t  type;
    uint16_t n;
    uint32_t v;
} fou_rule_t;

#define FOU_RULES   8u
#define FOU_MEM     8u
#define FOU_Q       16384u
#define FOU_CAP     1024u     /* une émission saisie, au plus : au-delà, elle part telle quelle */

/* LENS_CS forcée : suivie (l'enveloppé), tenue haute, tenue basse. */
typedef enum { FOU_CS_SUIT, FOU_CS_HAUTE, FOU_CS_BASSE } fou_cs_t;

typedef struct {
    lens_sim_t     inner;
    uint64_t       now;
    /* l'émission saisie */
    bool           capturing;
    uint8_t        cap[FOU_CAP];
    size_t         cap_n;
    bool           inner_cs;         /* le niveau de LENS_CS de l'enveloppé */
    bool           out_cs;           /* celui qui est sorti */
    fou_cs_t       cs_mode;
    /* l'émission retenue par FOU_ECHANGE */
    bool           held;
    uint8_t        held_b[FOU_CAP];
    size_t         held_n;
    /* la sortie, dans l'ordre */
    lens_sim_out_t q[FOU_Q];
    uint32_t       qh, qn;
    uint64_t       free;             /* l'instant de la dernière sortie rangée */
    fou_rule_t     rules[FOU_RULES];
    uint8_t        n_rules;
    struct {
        uint8_t b[BSK_FRAME_MAX];
        size_t  n;
    } mem[FOU_MEM];
    uint8_t        mem_next;
    uint32_t       applied[FOU_N];   /* les émissions transformées, par action */
    uint32_t       injected;         /* les émissions du banc */
} fou_t;

/* L'objectif fou qui enveloppe `inner` (initialisé par l'appelant), sans règle. */
void fou_init(fou_t *f, lens_sim_t inner);
lens_sim_t lens_sim_fou(fou_t *f);

/* Une règle de plus (au-delà de FOU_RULES, ignorée). */
void fou_rule(fou_t *f, fou_rule_t r);
/* Toutes les règles retirées ; l'émission retenue par FOU_ECHANGE part ; LENS_CS suivie de nouveau. */
void fou_clear(fou_t *f);
/* LENS_CS forcée ou suivie, à partir de l'instant courant. */
void fou_cs(fou_t *f, fou_cs_t mode);
/* Une émission du banc à l'instant courant, après ce qui est déjà rangé : LENS_CS levée (si `cs`), les octets, LENS_CS
 * retombée ; `late` derniers octets après la retombée (0 : aucun). */
void fou_inject(fou_t *f, const uint8_t *b, size_t n, bool cs, size_t late);
/* La trame rangée par FOU_RANGE à la place `k` (modulo FOU_MEM), émise : faux si la place est vide. */
bool fou_inject_mem(fou_t *f, uint8_t k);
/* Rien n'est en cours ni rangé : la sortie est vide, rien n'est saisi ni retenu. */
bool fou_idle(const fou_t *f);

#endif
