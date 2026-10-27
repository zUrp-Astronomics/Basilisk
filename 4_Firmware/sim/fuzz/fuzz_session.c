/* F2 de plans/fuzzing.md (l'atelier) : la SESSION face à un objectif fou. Cibles 2 (la lecture des réponses, l'identité,
 * le bouton du fût), 3 (le temps et les lignes) et 5 (la NVS que la SESSION écrit et relit) du plan, § 2.
 *
 * Le banc est celui de test_session135 et de test_std : la vraie SESSION, TRANSACTION, bench_core, le journal et le
 * magasin en mémoire (sim/store_sim.c), au-dessus de la PHY simulée (sim/phy_sim.c, qui seule a le verrou de D2 et la
 * fenêtre d'écoute) et d'un faux objectif inchangé, le faux 135 (sim/lens135.c) ou le faux standard (sim/lens_std.c),
 * enveloppé par l'objectif fou (lens_fou.h), branché par l'interface sim/lens_sim.h. Le banc fait la boucle de app_main :
 * un pas par tic de 1 ms (les tics où ni PHY ni SESSION n'ont rien à faire sautés, courir), les accusés lus après chaque
 * pas et chaque dépôt, les commandes déposées entre deux pas à leur instant réel (bsk_session_real), comme HOTE. Seules des commandes qu'une ligne
 * de l'hôte peut produire sont déposées (components/host/host.c : f, f±n, q, js, jx, jg, b, p1, p0, CUSTOM, l'ouverture) ;
 * l'hôte fou est F3. bench_core ne reçoit aucun contournement : ses émissions sont observées par --wrap (programmes.sh).
 *
 * LE RETARD DU PAS. Un pas peut commencer en retard sur l'échéance qui l'appelle : sur la carte, app_main rend la main
 * un tic (1 ms, vTaskDelay(1), main/main.c) entre deux pas, une émission retient le pas jusqu'à 100 ms par trame
 * (bsk_session.h, « l'attente de sa fin, 100 ms au plus »), une écriture en flash le retient (PROTOCOL.md § 6) ; au-delà
 * de 10 s, le chien de garde redémarre la carte (plans/survie.md § 2, « Tâches et chien de garde » ; sdkconfig.defaults).
 * Un retard tiré est donc dans [0, 10 s[ : 1 ms, 2 ms (la fenêtre de la paire, txn.c), 100 ms, et jusqu'à 9,999999 s.
 *
 * Un cas, l'entrée développée (jamais une graine) : un en-tête de 8 octets, puis des événements de 12 octets.
 *   en-tête : [0] le modèle (0 le faux 135, 1 le faux standard) ; [1] le journal (0 éteint, 1 LOG ON, 2 LOG ALL) ;
 *             [2] le préfixe dont le cas part (compté au bilan, il ne change pas le jeu : ses événements sont dans le
 *             cas) ; [3..7] zéro ;
 *   événement : [dt, 4 octets : µs depuis l'instant prévu de l'événement précédent][op][a][b, 2 octets][v, 4 octets],
 *               petit-boutiste ; l'événement est joué à son instant prévu, ou dès que possible si un pas en retard l'a
 *               dépassé. Les opérations : O_* ci-dessous.
 * Après le dernier événement, le chaos est fini : les règles retirées, LENS_CS suivie, le bus rendu, D2 présent et
 * l'objectif alimenté ; la session doit atteindre READY ou FAULT en temps borné (P3), et chaque commande acceptée son
 * accusé final.
 *
 * LES ORACLES sont écrits ici, à la main, chacun avec sa source ; aucun n'appelle allowed ni listed_size de
 * bench_core.c (le modèle de la liste : liste.h). Le banc lit le firmware par ses interfaces publiques et par des
 * enveloppes --wrap d'appels entre unités de compilation (programmes.sh, fuzz_session) :
 *   bsk_phy_send, bsk_phy_poll, bsk_phy_rail, bsk_phy_lines, bsk_phy_body_cs (le fil) ; bsk_txn_samyang (la déclaration
 *   à bench_core) ; bsk_txn_held_end, session_ack_last (les deux contrats internes) ; session_ack (la file des
 *   accusés) ; bsk_journal_session (chaque transition, à son instant) ; bsk_store_request (chaque écriture déposée, et
 *   la valeur que la NVS rend à une lecture, cible 5) ; bsk_store_result (la marque relue) ; bsk_txn_ack (l'issue d'une
 *   requête lue : sa réponse jugée contre la requête émise).
 * La table des transitions, les T_max, les délais minimaux, la présence D2, le doigt du bouton (lu et réel), la réponse
 * d'une requête (PROTOCOL.md § 6) et le canal CUSTOM désynchronisé sont en tête de leurs vérifications, plus bas.
 *
 * F3 : CE MOTEUR FAIT AUSSI DEUX AUTRES HARNAIS, compilés par fuzz_hote.c (FZ_HOTE) et fuzz_commande.c (FZ_COMMANDE),
 * qui l'incluent ; leurs cas ont le même format et les mêmes oracles, plus les leurs.
 *   - fuzz_hote, l'hôte fou (cible 4 du plan) : la vraie couche HOTE (components/host/host.c) entre le banc et la
 *     session. Les octets reçus par l'USB (O_LIGNE) passent par le cadrage de la carte, recopié à la main (hote.c), et
 *     chaque ligne livrée va à bsk_host_line comme bsk_host_usb_service la donne : une ligne à la fois, après
 *     bsk_host_observe et bsk_session_real, la réponse différée (BSK_HOST_LATER) redemandée par bsk_host_later après
 *     chaque pas, aucune autre ligne prise avant elle (bsk_host.h ; main/main.c). Les commandes des scénarios (O_CMD)
 *     deviennent leur ligne. HOTE lit la file des accusés : le banc les juge à leur dépôt (session_ack), et voit ce que
 *     HOTE dépose par --wrap=bsk_session_command. La réponse de chaque ligne est jugée par hote.c (P10, P4 côté HOTE,
 *     l'admission de B4 (a)). Le magasin peut retenir ses opérations (O_MAGASIN), comme la tâche du magasin de la carte :
 *     c'est ce qui rend différées les réponses de js et jx.
 *   - fuzz_commande, le harnais secondaire : les scénarios de F2, plus des bsk_cmd_t arbitraires (O_CMDX : op hors de
 *     l'énumération, arg quelconque) déposés comme HOTE les dépose. Une trouvaille n'y est un trou que si une ligne de
 *     l'hôte y mène (plans/fuzzing.md § 2, cible 4). */
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <string.h>

#include "bsk_bench_core.h"
#include "bsk_journal.h"
#include "bsk_session.h"
#include "bsk_txn.h"
#include "fuzz.h"
#include "lens_fou.h"
#include "lens_sim_135.h"
#include "lens_sim_std.h"
#include "liste.h"
#include "phy_common.h"
#include "phy_sim.h"
#include "sources135.h"
#include "store_sim.h"
#ifdef FZ_HOTE
#include "bsk_host.h"
#include "bsk_led.h"
#include "hote.h"
#endif

#if defined(FZ_HOTE)
const char FZ_CIBLE[] = "hote";
#elif defined(FZ_COMMANDE)
const char FZ_CIBLE[] = "commande";
#else
const char FZ_CIBLE[] = "session";
#endif

#define HDR      8u
#define EV       12u
#ifdef FZ_HOTE
#define EV_MAX   1200u                                  /* une ligne de 255 octets fait 43 événements */
#else
#define EV_MAX   400u
#endif
const size_t FZ_CAP = HDR + EV * EV_MAX;
const size_t FZ_ENTETE = HDR, FZ_GRAIN = EV, FZ_NEUTRE = 4;   /* la réduction par événements (fuzz.h) */
const unsigned FZ_ESSAIS = 120;

enum {
    C_POLL, C_ETAT, C_TRANS, C_FAULT, C_DUREE, C_FIN, C_OFF, C_CIBLE, C_INST, C_IDENT, C_LISTE, C_SIGN, C_ECRIT,
    C_REFUS, C_LIGNE, C_MUET, C_DEBIT, C_RAIL, C_PRES, C_DLOG, C_DLIG, C_DBAS, C_DHAUT, C_DHOME, C_ACK, C_ACKFIN, C_HELD,
    C_LAST, C_NVS, C_DOIGT, C_APPUI, C_HAVE64, C_REPONSE, C_DESYNC, C_MARQUE,
    C_ACKPREM, C_P10FIN, C_P10TAMPON, C_P10LIGNE, C_P10LONG, C_P10OK, C_P10FORME, C_P4HOTE, C_B4, C_LATER,   /* F3 */
    C_N
};

const fz_check_t FZ_CHECKS[] = {
    [C_POLL] = {"P2", "poll", "session.c, STEP_EVENTS ; bsk_session.h (bsk_session_step) : au plus 64 événements de PHY par pas"},
    [C_ETAT] = {"P3", "etat", "PROTOCOL.md § 7 (boot_state, huit états) ; bsk_contract.h, bsk_session_state_t"},
    [C_TRANS] = {"P3", "transition", "spec § 3.1, le tableau des transitions, lignes HOTE comprises, avec [D60], [D73], "
                                     "[D85], [D129] (recopié en tête de permise, fuzz_session.c)"},
    [C_FAULT] = {"P3", "fault", "plans/survie.md § 3 : seules E_LOST et E_HOME_FAILED mènent à FAULT"},
    [C_DUREE] = {"P3", "duree", "plans/fuzzing.md § 3, P3 : aucun état de séquence tenu plus de T_max, hors retards du pas "
                                "injectés (T_max en tête de TMAX, fuzz_session.c)"},
    [C_FIN] = {"P3", "fin", "plans/fuzzing.md § 3, P3 : le chaos fini et l'objectif nominal, READY ou FAULT en temps borné"},
    [C_OFF] = {"P3", "off", "spec § 3.1 [D31], [D87] : D2 retombé plus de 2 ms, la session en OFF"},
    [C_CIBLE] = {"P3", "cible", "plans/fuzzing.md § 8 (#744) ; test_stop_goto135.c (#787) : sous un 135 nominal où seul le "
                                "moment est fou, un goto fini ACK_COMPLETED l'est sur sa cible"},
    [C_INST] = {"P4", "instantane", "bsk_contract.h, bsk_status_t ; spec § 4.4 règle 3 : ouverture dans 0x0E00..0x2000 ou 0, "
                                    "plage avec CAP_APERTURE, limits_valid => min <= max et CAP_LIMITS_REPORTED, "
                                    "position_valid => READY"},
    [C_IDENT] = {"P4", "identite", "lens_rx.h et init.c (la reconnaissance) : un 0x07 hors de l'init ne change ni l'identité "
                                   "ni la déclaration à bench_core"},
    [C_LISTE] = {"P5", "liste", "CLAUDE.md § Interdits ; bsk_bench_core.h ; PROTOCOL.md § 1 règle 7 : chaque trame émise "
                                "passe le modèle de la liste (liste.h) pour la déclaration en cours (P5 b)"},
    [C_SIGN] = {"P5", "signature", "CLAUDE.md § Interdits ; PROTOCOL.md § 2, « Samyang reconnu » : un 0x40 ne part que vers "
                                   "un objectif qui a donné sa signature dans la session (P5 c)"},
    [C_ECRIT] = {"P5", "ecriture", "PROTOCOL.md § 3.1 ; bsk_contract.h, CMD_LENS_CUSTOM : un 'P' 0x38 ne part qu'à la suite "
                                   "d'une CUSTOM WRITE déposée (F3 : d'une ligne CUSTOM WRITE acceptée), un par commande, "
                                   "son octet de données 0x30 + arg, celui de la ligne (P5 d)"},
    [C_REFUS] = {"P5", "refus", "plans/fuzzing.md § 3, P5 (e) : bsk_bench_refused() ne bouge pas"},
    [C_LIGNE] = {"P6", "ligne", "plans/fuzzing.md § 3, P6 : chaque ligne du journal tient dans 600 octets (JOURNAL_LINE de "
                                "host_usb.c)"},
    [C_MUET] = {"P6", "muet", "CLAUDE.md § Interdits ; PROTOCOL.md § 1 règle 3 (PROTOCOL.md:20) : journal éteint (l'en-tête "
                              "du cas, puis les lignes LOG passées, F3), aucune ligne `*` ; une ligne d'avant LOG OFF ne sort "
                              "pas (host_usb.c, write_line)"},
    [C_DEBIT] = {"P6", "debit", "journal.c, le plafond : par fenêtre d'une seconde, PER_S lignes du plafond et TR_PER_S "
                                "transitions, hors xdetect, std et ring ; une seconde alignée en recouvre deux fenêtres"},
    [C_RAIL] = {"P7", "rail", "plans/survie.md § 3, table A ; spec [D129] : un rail allumé ne retombe que par D2 ou p0/b"},
    [C_PRES] = {"PRESENCE", "rail", "bsk_phy.h, phy_sim.h, phy_common.h (le verrou) ; spec [D87] : un rail ne s'allume que D2 "
                                    "présent, l'insertion confirmée (300 ms), sans retombée de plus de 2 ms depuis"},
    [C_DLOG] = {"DELAI", "rail_moteur", "bsk_session.h (bsk_session_step : un délai minimal se compte depuis l'action) ; "
                                        "PROTOCOL.md § 7 ; session.c RAIL_MS : rail moteur 50 ms au moins après le logique"},
    [C_DLIG] = {"DELAI", "lignes", "bsk_session.h ; session.c RAIL_MS : les lignes 50 ms au moins après le rail moteur"},
    [C_DBAS] = {"DELAI", "cs_basse", "bsk_session.h ; session.c HS_LOW_MS : BODY_CS haute 15 ms au moins après basse"},
    [C_DHAUT] = {"DELAI", "cs_haute", "bsk_session.h ; session.c HS_HOLD_MS : BODY_CS tenue haute 3 ms au moins après "
                                      "LENS_CS vue haute"},
    [C_DHOME] = {"DELAI", "homing", "bsk_session.h (« le 0x1C du homing ») ; session.c AF_STOP_MS : le 0x10 08 100 ms au "
                                    "moins après l'entrée en HOMING"},
    [C_ACK] = {"ACCUSE", "final", "bsk_session.h, bsk_session_command : ACK_REJECTED seul, ou ACK_ACCEPTED puis un seul accusé "
                                  "final, ACK_COMPLETED ou ACK_FAILED"},
    [C_ACKFIN] = {"ACCUSE", "fin", "bsk_session.h, bsk_session_command : chaque commande acceptée reçoit son accusé final"},
    [C_HELD] = {"CONTRAT", "held_end", "backlog.md (« Trois contrats internes », 2) ; txn.c, bsk_txn_held_end : sa place "
                                       "sous BSK_TXN_HELD_MAX"},
    [C_LAST] = {"CONTRAT", "ack_last", "backlog.md (« Trois contrats internes », 3) ; session.c, session_ack_last : la file "
                                       "des accusés non vide"},
    [C_NVS] = {"P8", "nvs", "store.c en tête, main.c : la NVS ne s'écrit que sur un geste (F3 : une ligne js ou jx) ; "
                            "PROTOCOL.md § 7 : rien pendant le retour"},
    [C_DOIGT] = {"P8", "doigt", "plans/fuzzing.md § 3, P8 ; mark.c (BTN_LONG_US, BTN_HOLD_US) : une écriture du bouton suit "
                                "un relâchement, lu dans un 0x05 qui porte l'offset 64, après 1 s à 4 s d'appui commencé "
                                "en READY"},
    [C_APPUI] = {"P8", "appui", "plans/fuzzing.md § 3, P8 (« un objectif fou qui bat le bit du bouton n'écrit pas plus qu'un "
                                "doigt ») ; mark.c (BTN_LONG_US, BTN_HOLD_US) ; lens135.h (le bouton) : une écriture du "
                                "bouton suit un appui réel du faux 135 de 1 s à 4 s (100 ms de tolérance), une par appui, "
                                "quand rien d'autre ne fabrique l'offset 64"},
    [C_HAVE64] = {"P8", "have64", "lens_rx.h (« o64 n'est lu que derrière have64 ») ; backlog.md (« mark_button lit o64 sans "
                                  "have64 ») : l'appui écrit n'a été vu que dans des 0x05 qui portent l'offset 64"},
    [C_REPONSE] = {"ORDRE", "reponse", "PROTOCOL.md § 6 (#783) : la réponse qui termine une requête est du type demandé et, "
                                       "pour 0x40, 0x0B et 0x0A, de son contenu (la même commande et sous-commande, l'écho, "
                                       "aucun bit ni octet hors de la requête)"},
    [C_MARQUE] = {"P4", "marque", "bsk_mark.h ; mark.c (encode, decode : bits 0-15 la position, 16-23 le sens, « une valeur "
                                  "dont les bits 24-31 ne sont pas nuls est ignorée ») : la marque publiée est celle relue, "
                                  "ou aucune"},
    [C_DESYNC] = {"ORDRE", "custom", "bsk_session.h (CMD_LENS_CUSTOM) ; PROTOCOL.md § 3.1 (#783, #789) : après une échéance, "
                                     "ou un abandon sans réponse, CUSTOM est refusé jusqu'à la coupure des rails"},
    [C_ACKPREM] = {"ACCUSE", "premier", "bsk_session.h, bsk_session_command : la commande traitée tout de suite, ACK_REJECTED "
                                        "(immédiat) ou ACK_ACCEPTED dans l'appel, sauf CMD_HOME et CMD_RING_SET, sans accusé ; "
                                        "host.c, deposit : « la session en dépose un pour chaque commande »"},
    [C_P10FIN] = {"P10", "fin", "PROTOCOL.md § 1 règles 1 et 2, § 4 ; bsk_host.h (bsk_host_line) : une réponse par ligne finie "
                                "par \\n, Moonlite sans fin de ligne, aucune à une ligne Moonlite muette ; BSK_HOST_LATER pour "
                                "CUSTOM, q, js et jx seulement"},
    [C_P10TAMPON] = {"P10", "tampon", "bsk_host.h, BSK_HOST_REPLY_MAX : la réponse tient dans le tampon"},
    [C_P10LIGNE] = {"P10", "une_ligne", "PROTOCOL.md § 1 règle 1 : une réponse est une ligne, sans octet de contrôle"},
    [C_P10LONG] = {"P10", "longueur", "PROTOCOL.md § 1 règle 4 : au plus 15 caractères pour f r e a d s c et m<n>"},
    [C_P10OK] = {"P10", "ok", "PROTOCOL.md § 1 règle 5 : jamais la sous-chaîne ok dans une erreur"},
    [C_P10FORME] = {"P10", "forme", "PROTOCOL.md § 2 à § 5, commande par commande ; spec § 4.5.1 (le tableau d'état ; une "
                                    "ligne inconnue, er nocap) : la table en tête de hote.c"},
    [C_P4HOTE] = {"P4", "hote", "plans/fuzzing.md § 3, P4 : côté HOTE, f, r, a et o ne rendent un nombre que si son drapeau "
                                "le permet ; PROTOCOL.md § 2, lignes f, r, a, o"},
    [C_B4] = {"B4", "admission", "l'audit B4 (a) de l'atelier ; spec § 4.5.1 ; bsk_session.h (servie en READY) : une lecture "
                                 "ne dépose rien, une commande dépose celle de sa ligne, en READY, et sa réponse suit son "
                                 "premier accusé"},
    [C_LATER] = {"DIFFEREE", "borne", "bsk_host.h (bsk_host_later : l'attente est bornée) ; PROTOCOL.md § 1 règle 1, § 2 (q), "
                                      "§ 3.1 (CUSTOM) : chaque ligne a sa réponse, une différée dans sa borne (en tête de "
                                      "hote.c)"},
};
const size_t FZ_N_CHECKS = C_N;

#define MS 1000u
#define S  (1000u * MS)
#define T0 (1000u * MS)

/* ─────────────────────────── les opérations d'un cas ─────────────────────────── */

enum {
    O_WAIT,     /* rien */
    O_CMD,      /* une commande de l'hôte : a son genre (K_*), b et v ses paramètres */
    O_LATE,     /* les a prochains pas en retard de v µs (v < 10 s) */
    O_D2,       /* D2 : a présent, b & 1 l'objectif alimenté ou coupé avec */
    O_POWER,    /* l'objectif alimenté (a) ou coupé, D2 inchangé */
    O_RULE,     /* une règle de l'objectif fou : a l'action (fou_act_t), b = type | n << 8, v le paramètre */
    O_CLEAR,    /* les règles retirées */
    O_INJ,      /* une émission du banc : a 0 la place b de la mémoire, 1 le gabarit b */
    O_LCS,      /* LENS_CS forcée : a (fou_cs_t) */
    O_BUSRX,    /* une E_BUS à la réception (phy_sim_bus_error) */
    O_BUSTX,    /* la prochaine émission rend E_BUS (phy_sim_send_bus_error) */
    O_FATE,     /* une trame perdue ou fausse sur le fil : a 0 vers l'objectif, 1 de lui ; b = type | n << 8 ; v 1 perdue,
                   2 fausse */
    O_RAILF,    /* le FAULT# d'un rail : a le rail, b actif */
    O_BTN,      /* le bouton du fût du faux 135 : a enfoncé */
    O_RAW,      /* des octets bruts de l'objectif : a leur nombre (1 à 4), b & 1 LENS_CS levée autour, v les octets */
    O_NVS,      /* la NVS (cible 5) : a 0 sans panne ; 1 la valeur v rangée sous la clé de la prochaine lecture ; 2 les
                   lectures en erreur ; 3 les écritures ratées ; 4 la file du magasin pleine (store_sim.h) */
    O_ALIM,     /* l'objectif resté alimenté (lens_sim.h, start_powered) : a & 1 en flux, a & 2 en mode service (135) ;
                   ce que l'enveloppé ne sait pas représenter, il le refuse, et rien ne démarre */
    O_LIGNE,    /* fuzz_hote : des octets reçus par l'USB, a leur nombre (1 à 6), puis b et v, petit-boutistes */
    O_CMDX,     /* fuzz_commande : un bsk_cmd_t arbitraire, b son op (hors de l'énumération compris), v son arg */
    O_MAGASIN,  /* fuzz_hote : le magasin, a & 1 ses opérations retenues v µs (store_sim.h, hold), comme la tâche du
                   magasin de la carte ; sinon rendues tout de suite */
    O_N
};

/* Les commandes, telles que host.c les dépose (PROTOCOL.md § 2, § 3.1). */
enum { K_GOTO, K_GOTO_SEL, K_MOVE, K_STOP, K_JS, K_JX, K_JG, K_B, K_P1, K_P0, K_CUSTOM_R, K_CUSTOM_W, K_APER, K_N };

/* ─────────────────────────── le banc ─────────────────────────── */

static fz_ctx_t *C;
static l135_t    L135;
static l135_params_t LP135;
static lstd_t    LSTD;
static lstd_params_t LPSTD;
static fou_t     F;
static bool      std_model;
static bsk_session_params_t P;
static uint8_t   journal_mode;
static bool      moment_seul;   /* un 135, et seuls le moment et le retard du pas sont fous (C_CIBLE) */
static bool      lens_on;

static lens_sim_t fou(void) { return lens_sim_fou(&F); }

/* ─────────────── D2, écrit à la main d'après phy_common.h et phy_sim.h ───────────────
 * Le verrou levé par une insertion confirmée : D2 présent 300 ms sans changement (D2_DEBOUNCE_US). Posé par une
 * retombée : objectif présent, le premier front vers « absent » arme une échéance de 2 ms (D2_DROP_US) qu'un autre front
 * ne relance pas, et D2 est relu à elle : absent, coupure ; objectif absent, la retombée est prise tout de suite. */
static struct {
    bool     pin, present, raw, pend;
    uint64_t since, pend_t;
    uint32_t cuts;              /* les coupures, objectif présent */
    uint64_t wake;              /* le banc réveille la simulation à la fin de l'anti-rebond (phy_sim.h) */
} D;

static void d2_drop(void)
{
    if (D.present) D.cuts++;
    D.present = false;
    D.raw = false;
}

static void d2_at(uint64_t t)
{
    for (;;) {
        uint64_t td = D.pend ? D.pend_t + D2_DROP_US : UINT64_MAX;
        uint64_t ti = !D.present && D.raw ? D.since + D2_DEBOUNCE_US : UINT64_MAX;
        if (td > t && ti > t) return;
        if (td <= ti) {
            D.pend = false;
            if (!D.pin) d2_drop();
        } else {
            D.present = true;
        }
    }
}

static void d2_set(uint64_t t, bool level)
{
    d2_at(t);
    if (D.pin && !level) {
        if (!D.present) d2_drop();
        else if (!D.pend) {
            D.pend = true;
            D.pend_t = t;
        }
    }
    D.pin = level;
    if (level != D.raw) {
        D.raw = level;
        D.since = t;
    }
    if (level) D.wake = t + D2_DEBOUNCE_US;
}

/* ─────────────── la session observée ─────────────── */

static uint8_t   etat;          /* le dernier état entré (bsk_journal_session) */
static uint64_t  depuis;        /* son entrée */
static uint64_t  retard_cumul, retard_entree;   /* les retards du pas injectés : au total, et à l'entrée */
static int       in_cmd = -1;   /* l'opération en cours de dépôt, -1 hors dépôt */
static uint32_t  cuts_vus;      /* les coupures de D2 déjà lues par une transition vers OFF */
static uint64_t  t_homing, t_logic, t_motor, t_cs_low, t_cs_high, t_lcs;
static bool      cs_high;
static bool      sig;           /* une signature Samyang reçue dans la session (PROTOCOL.md § 2) */
static bsk_bench_lens_t decl;   /* la déclaration à bench_core (bsk_txn_samyang) */
static unsigned  polls;
static bool      vide;          /* le dernier bsk_phy_poll du pas n'a rien rendu : la file de la PHY lue jusqu'au bout */
static bool      cut_cmd;       /* une coupure par commande (p0, b en FAULT) depuis la dernière observation */
/* Les CUSTOM WRITE déposées qui peuvent encore émettre leur 'P' 0x38 (P5 d) : leur seq et l'octet de données que la
 * trame doit porter, 0x30 + arg (F3 : celui de la ligne). Retirées à leur refus, à leur accusé final, ou à l'émission. */
static struct {
    uint32_t seq;
    int64_t  octet;
} CR[8];
static unsigned n_cr;

static void credit_retire(uint32_t seq)
{
    for (unsigned i = 0; i < n_cr; i++)
        if (CR[i].seq == seq) CR[i--] = CR[--n_cr];
}
static bool      dep_actif, dep_vu;   /* une commande en cours de dépôt, son premier accusé vu dans l'appel (C_ACKPREM) */
static uint32_t  dep_seq;
static bsk_ack_t dep_premier;
static bool      nvs_poke;      /* la valeur nvs_val rangée sous la clé de la prochaine lecture */
static uint32_t  nvs_val;
static uint8_t   marque_src;    /* d'où vient la marque : 0 aucune, 1 relue (marque_v), 2 un geste */
static uint32_t  marque_v;
static bsk_frame_t requete;     /* la dernière requête émise (classe 2, bsk_txn.h) */
static bool      desync;        /* le modèle du canal CUSTOM désynchronisé, à la main (bsk_session.h, CMD_LENS_CUSTOM) */
static bool      custom_emis, custom_repondu;   /* le CUSTOM en vol : sa requête partie, une réponse à elle reçue */
static uint32_t  cov;           /* ce que le cas a atteint (COV_*) */

#ifdef FZ_HOTE
/* ─────────────── l'hôte fou ─────────────── */
static struct {
    uint8_t  fifo[16384];       /* les octets reçus par l'USB, pas encore cadrés */
    size_t   r, w, perdus;
    cadre_t  cad;               /* le cadrage, recopié à la main (hote.c) */
    bool     vol;               /* une ligne livrée dont la réponse n'est pas écrite : rx_task l'attend (host_usb.c, deliver) */
    bool     later;             /* ... différée : bsk_host_line a rendu BSK_HOST_LATER */
    char     ligne[HOTE_LIGNE_MAX + 1];
    uint64_t t0, r0;            /* l'instant de la ligne différée, et les retards du pas injectés jusque-là */
    uint64_t servi;             /* l'instant du dernier service (une ligne ou bsk_host_later) : un par tour de boucle */
} U;
static hote_ligne_t  LC;        /* ce que le banc voit de la ligne en cours */
static bool          ligne_active;
static hote_modele_t M;         /* le journal et le plafond, tenus par le banc d'après les lignes passées */
static char          R[BSK_HOST_REPLY_MAX];   /* la taille du tampon de host_usb.c (rep) : ASan y voit un débordement */
static bool          mag_tenu;  /* le magasin retient ses opérations (O_MAGASIN) */
static uint64_t      mag_lat, mag_due;        /* leur latence ; l'échéance de celles en attente, 0 sans */
#endif

/* Ce que le bilan compte, sur toute la campagne. */
enum { COV_READY, COV_GOTO, COV_RESTORING, COV_RECOVERING, COV_FAULT, COV_MARQUE, COV_CUSTOM, COV_N };
static const char *const COV_NOM[COV_N] = {"ready", "goto en vol", "restoring", "recovering", "fault", "marque rangée",
                                           "custom en vol"};
#define N_PREFIXES 10u
static const char *const PREFIXE[N_PREFIXES] = {"froid", "ready", "goto", "marque", "perte", "custom", "retour", "arret",
                                                "faute", "homing"};
static struct {
    uint64_t cas, par_prefixe[N_PREFIXES], couverture[COV_N], fin_jugee, fin_sautee, held_end, held_end_max, ack_last,
        ack_last_vide, ecritures_bouton, appuis_coupes, appuis_juges, actions[FOU_N], injections, retards, moment_seul;
    /* F3 : les lignes servies par famille (hote.h, F_*) et par état de la session à la ligne ; les commandes que HOTE
     * admet elle-même, hors READY et en READY (B4) ; les réponses différées et la plus longue attente ; les octets reçus ;
     * les bsk_cmd_t arbitraires, hors de l'énumération, et les dépôts sans accusé. */
    uint64_t lignes_fam[64], lignes_etat[8], b4[2], differees, differee_max_us, octets_usb, cmdx, cmdx_hors, sans_accuse;
} B;

/* ─────────────── la table des transitions, à la main ───────────────
 * spec § 3.1 (l'atelier), le tableau des transitions, lu avec ses remplacements : OFF -> POWERING (D2, ou CMD_ATTACH) ;
 * POWERING -> IDENTIFYING ([D59]) ; IDENTIFYING -> HOMING, READY, RESTORING, RECOVERING, FAULT ; HOMING -> READY,
 * RESTORING, FAULT, RECOVERING ; RESTORING -> READY, RECOVERING, FAULT ; READY -> RECOVERING, FAULT ([D60]) ;
 * RECOVERING -> POWERING ([D129]) ; tout état hors OFF -> OFF (D2 retombé, [D31] ; p0, b en FAULT, (HOTE)) ; FAULT ->
 * POWERING (p1, CMD_ATTACH, (HOTE)) ; tout état hors OFF et FAULT -> POWERING (b, CMD_ATTACH, (HOTE)). Une ligne (HOTE)
 * n'est permise que pendant le dépôt de sa commande ; vers OFF sans commande, une coupure de D2 doit l'expliquer. */
static bool permise(uint8_t de, uint8_t vers)
{
    if (vers == SESSION_OFF) {
        if (de == SESSION_OFF) return false;
        if (in_cmd == CMD_DETACH || in_cmd == CMD_CLEAR_FAULT) return true;
        if (D.cuts > cuts_vus) {
            cuts_vus = D.cuts;
            return true;
        }
        return false;
    }
    if (vers == SESSION_POWERING && in_cmd == CMD_ATTACH && de != SESSION_OFF) return true;
    switch (de) {
    case SESSION_OFF: return vers == SESSION_POWERING;
    case SESSION_POWERING: return vers == SESSION_IDENTIFYING;
    case SESSION_IDENTIFYING:
        return vers == SESSION_HOMING || vers == SESSION_READY || vers == SESSION_RESTORING || vers == SESSION_RECOVERING ||
               vers == SESSION_FAULT;
    case SESSION_HOMING:
        return vers == SESSION_READY || vers == SESSION_RESTORING || vers == SESSION_FAULT || vers == SESSION_RECOVERING;
    case SESSION_RESTORING: return vers == SESSION_READY || vers == SESSION_RECOVERING || vers == SESSION_FAULT;
    case SESSION_READY: return vers == SESSION_RECOVERING || vers == SESSION_FAULT;
    case SESSION_RECOVERING: return vers == SESSION_POWERING;
    default: return false;
    }
}

/* ─────────────── T_max, à la main, en µs de temps virtuel, retards du pas injectés retirés ───────────────
 *   - POWERING : les deux rails, 50 ms chacun (RAIL_MS), BODY_CS basse 15 ms (HS_LOW_MS), les deux attentes de LENS_CS
 *     (handshake_ms, 500 ms chacune, bsk_session_params_default) et 3 ms (HS_HOLD_MS) : 1 118 ms, 1,2 s ;
 *   - IDENTIFYING (init.c, init_reply ; spec § 3.1 [D77], [D88]) : le 0x01 trois fois (init_ms, 300 ms), le reset soft
 *     (0x0A, info_ms, 400 ms), le 0x01 trois fois encore ; 0x07, 0x3F (name_ms), 0x0B, 0x09, 0x0D trois envois chacun à
 *     300 ms, 0x08 trois envois à 400 ms ; le 0x10 (init_home_ms, 8 s) ; le 0x0A (400 ms) ; le premier 0x05
 *     (first05_ms, 500 ms) ; la sonde (probe05_ms, 300 ms) ; la lecture de la marque (MARK_WAIT_MS, 500 ms) : 17,6 s ;
 *     plus, pour chacun des 27 envois au plus, une requête retenue jusqu'au 0x04 (bsk_txn.h), moins d'une période de
 *     VD (16,7 ms) : 18,1 s ;
 *   - HOMING : le 0x1C et 100 ms (AF_STOP_MS), le 0x10 08 (home_ms, 20 s), l'immobilité (still_ms, 3 s), la marque
 *     (500 ms), une requête retenue : 23,62 s ;
 *   - RESTORING (restore.h, deux gotos au plus ; motion.c) : chacun trois envois du 0x1D à 1 s (SENDS, START_MS), puis
 *     10 s d'immobilité attendue (MOVE_STILL_MS) ; chaque 0x1D retenu jusqu'au 0x04 : 26,1 s ;
 *   - RECOVERING : retry_wait_ms, 3 s, plus 1 ms. */
static const uint64_t TMAX[8] = {0, 1200u * MS, 18100u * MS, 23620u * MS, 26100u * MS, 0, 3001u * MS, 0};

/* ─────────────── les accusés, par commande ─────────────── */

#define SEQ_MAX (EV_MAX + 8u)
enum { A_RIEN, A_REFUSEE, A_VOL, A_FINIE };
static struct {
    uint8_t  etat;
    uint8_t  op;
    bool     cible_jugee;
    int32_t  cible;
} CMDS[SEQ_MAX];
static uint32_t seq_n;
static unsigned board_vol;       /* les gotos du bouton en vol (seq 0) */
static unsigned ack_n;           /* le modèle de la file des accusés : 8 au plus (bsk_session.h) */

static void ack_juge(const bsk_ack_t *a)
{
    if (a->seq == BSK_SEQ_BOARD) {
        if (a->result == ACK_ACCEPTED) {
            if (board_vol++) fz_fail(C, C_ACK, "seq 0 : un goto du bouton accepté pendant un autre");
            cov |= 1u << COV_GOTO;
        } else if (a->result == ACK_COMPLETED || a->result == ACK_FAILED) {
            if (!board_vol) fz_fail(C, C_ACK, "seq 0 : un accusé final sans goto du bouton en vol");
            else board_vol--;
        }
        return;
    }
    if (a->seq > seq_n || a->seq >= SEQ_MAX) {
        fz_fail(C, C_ACK, "un accusé pour le seq %lu, jamais déposé", (unsigned long)a->seq);
        return;
    }
    {
        uint8_t *e = &CMDS[a->seq].etat;
        uint8_t op = CMDS[a->seq].op;
        if (a->result == ACK_REJECTED) {
            if (*e != A_RIEN) fz_fail(C, C_ACK, "seq %lu : ACK_REJECTED après un autre accusé", (unsigned long)a->seq);
            *e = A_REFUSEE;
            credit_retire(a->seq);
        } else if (a->result == ACK_ACCEPTED) {
            if (*e != A_RIEN) fz_fail(C, C_ACK, "seq %lu : ACK_ACCEPTED après un autre accusé", (unsigned long)a->seq);
            *e = A_VOL;
            if (op == CMD_LENS_CUSTOM && desync)
                fz_fail(C, C_DESYNC, "seq %lu : CUSTOM accepté, le canal désynchronisé (une échéance ou un abandon sans "
                                     "réponse, sans coupure des rails depuis)", (unsigned long)a->seq);
            if (op == CMD_LENS_CUSTOM) custom_emis = custom_repondu = false;
            if (op == CMD_FOCUS_GOTO || op == CMD_FOCUS_MOVE) cov |= 1u << COV_GOTO;
            if (op == CMD_LENS_CUSTOM) cov |= 1u << COV_CUSTOM;
            if (op == CMD_DETACH || op == CMD_CLEAR_FAULT) cut_cmd = true;
        } else {
            if (*e != A_VOL)
                fz_fail(C, C_ACK, "seq %lu : accusé final %s sans ACK_ACCEPTED seul avant", (unsigned long)a->seq,
                        a->result == ACK_COMPLETED ? "COMPLETED" : "FAILED");
            *e = A_FINIE;
            credit_retire(a->seq);
            if (op == CMD_LENS_CUSTOM && a->result == ACK_FAILED &&
                (a->reason == E_TIMEOUT || (a->reason == E_ABORTED && custom_emis && !custom_repondu)))
                desync = true;
            if (a->result == ACK_COMPLETED && op == CMD_SET_MARK) cov |= 1u << COV_MARQUE;
            if (moment_seul && a->result == ACK_COMPLETED && CMDS[a->seq].cible_jugee &&
                l135_position(&L135) != CMDS[a->seq].cible)
                fz_fail(C, C_CIBLE, "seq %lu : goto vers %ld fini ACK_COMPLETED, le faux 135 en %ld", (unsigned long)a->seq,
                        (long)CMDS[a->seq].cible, (long)l135_position(&L135));
        }
    }
}

/* Chaque accusé est jugé à son dépôt (__wrap_session_ack), dans l'ordre où la session le pose parmi ses transitions et
 * ses émissions. La file est lue ici, comme l'hôte la lit ; sous fuzz_hote, HOTE la lit (bsk_host_observe, son dépôt) :
 * rien à lire ici. */
static void lire_accuses(void)
{
#ifndef FZ_HOTE
    bsk_ack_t a;
    while (bsk_session_ack(&a))
        if (ack_n) ack_n--;
#endif
}

/* ─────────────── les enveloppes ─────────────── */

void __real_session_ack(uint32_t seq, bsk_ack_result_t r, bsk_err_t why);
void __wrap_session_ack(uint32_t seq, bsk_ack_result_t r, bsk_err_t why);
void __wrap_session_ack(uint32_t seq, bsk_ack_result_t r, bsk_err_t why)
{
    if (ack_n < 8) ack_n++;
    if (dep_actif && seq == dep_seq && !dep_vu) {
        dep_vu = true;
        dep_premier = (bsk_ack_t){.seq = seq, .result = r, .reason = why};
    }
    __real_session_ack(seq, r, why);
    {
        bsk_ack_t a = {.seq = seq, .result = r, .reason = why};
        ack_juge(&a);
    }
}

#ifdef FZ_HOTE
/* La file lue par HOTE : le modèle de la file des accusés (ack_n) suit. */
bool __real_bsk_session_ack(bsk_ack_t *a);
bool __wrap_bsk_session_ack(bsk_ack_t *a);
bool __wrap_bsk_session_ack(bsk_ack_t *a)
{
    bool got = __real_bsk_session_ack(a);
    if (got && ack_n) ack_n--;
    return got;
}
#endif

const bsk_ack_t *__real_session_ack_last(void);
const bsk_ack_t *__wrap_session_ack_last(void);
const bsk_ack_t *__wrap_session_ack_last(void)
{
    B.ack_last++;
    if (!ack_n) {
        B.ack_last_vide++;
        fz_fail(C, C_LAST, "t=%llu : session_ack_last sur une file vide", (unsigned long long)phy_sim_now());
    }
    return __real_session_ack_last();
}

bool __real_bsk_txn_held_end(uint8_t place, bsk_err_t *e);
bool __wrap_bsk_txn_held_end(uint8_t place, bsk_err_t *e);
bool __wrap_bsk_txn_held_end(uint8_t place, bsk_err_t *e)
{
    B.held_end++;
    if (place > B.held_end_max) B.held_end_max = place;
    if (place >= BSK_TXN_HELD_MAX) {
        fz_fail(C, C_HELD, "t=%llu : bsk_txn_held_end(%u)", (unsigned long long)phy_sim_now(), place);
        return false;
    }
    return __real_bsk_txn_held_end(place, e);
}

void __real_bsk_txn_samyang(bsk_bench_lens_t lens);
void __wrap_bsk_txn_samyang(bsk_bench_lens_t lens);
void __wrap_bsk_txn_samyang(bsk_bench_lens_t lens)
{
    if (lens != BSK_BENCH_OTHER && etat != SESSION_IDENTIFYING)
        fz_fail(C, C_IDENT, "t=%llu : déclaration %d à bench_core en %s", (unsigned long long)phy_sim_now(), (int)lens,
                bsk_state_name(etat));
    decl = lens;
    __real_bsk_txn_samyang(lens);
}

void __real_bsk_journal_session(uint8_t state, bsk_err_t last_error);
void __wrap_bsk_journal_session(uint8_t state, bsk_err_t last_error);
void __wrap_bsk_journal_session(uint8_t state, bsk_err_t last_error)
{
    uint64_t t = phy_sim_now();
    if (state > SESSION_FAULT) {                    /* jugé avant le journal, qui nomme l'état */
        fz_fail(C, C_ETAT, "t=%llu : état %u", (unsigned long long)t, state);
        return;
    }
    __real_bsk_journal_session(state, last_error);
    fz_tr(C, "t=%llu %s -> %s%s", (unsigned long long)t, bsk_state_name(etat), bsk_state_name(state),
          in_cmd >= 0 ? " (commande)" : "");
    d2_at(t);
    if (state != etat && !permise(etat, state))
        fz_fail(C, C_TRANS, "t=%llu : %s -> %s", (unsigned long long)t, bsk_state_name(etat), bsk_state_name(state));
    if (state == SESSION_FAULT && last_error != E_LOST && last_error != E_HOME_FAILED)
        fz_fail(C, C_FAULT, "t=%llu : FAULT sur %s", (unsigned long long)t, bsk_err_token(last_error));
    etat = state;
    depuis = t;
    retard_entree = retard_cumul;
    if (state == SESSION_POWERING) sig = false;
    if (state == SESSION_HOMING) t_homing = t;
    if (state == SESSION_READY) cov |= 1u << COV_READY;
    if (state == SESSION_RESTORING) cov |= 1u << COV_RESTORING;
    if (state == SESSION_RECOVERING) cov |= 1u << COV_RECOVERING;
    if (state == SESSION_FAULT) cov |= 1u << COV_FAULT;
}

/* Le doigt, lu à la main dans les 0x05 qui portent l'offset 64 (msg[65], bit 3 : lens_rx.c, mark.c BTN_BIT ; lens135.h,
 * le bouton) : enfoncé, relâché ; `coupe` : pendant l'appui, un 0x05 lu sans l'offset 64 (29 à 65 octets, PROTOCOL.md
 * § 7) ou plus de lost_ms sans 0x05. */
static struct {
    bool     bas, coupe, relache, rel_coupe;
    uint8_t  etat_bas;
    uint64_t t_bas, t_dernier, rel_t, rel_duree;
} DG;
static const bsk_phy_event_t *ev_courant;

/* Le doigt réel : le bouton du faux 135 (O_BTN), son dernier appui fini et s'il a déjà servi une écriture. `faux` : le
 * cas peut fabriquer l'offset 64 autrement que par le bouton (un champ 65 muté, un 0x05 allongé, doublé, échangé ou
 * rejoué, une injection) ; C_APPUI n'est alors pas jugée. */
static struct {
    bool     bas, fini, servi, faux;
    uint64_t t_bas, duree;
} DR;

static void doigt(const bsk_frame_t *f, uint64_t t)
{
    if (f->msg[0] != 0x05 || f->len < 29) return;
    if (DG.bas && t - DG.t_dernier > (uint64_t)P.lost_ms * MS) DG.coupe = true;
    DG.t_dernier = t;
    DG.relache = false;
    if (f->len < 66) {
        if (DG.bas && !DG.coupe) fz_tr(C, "t=%llu bouton : un 0x05 de %u octets, sans l'offset 64", (unsigned long long)t, f->len);
        if (DG.bas) DG.coupe = true;
        return;
    }
    if ((f->msg[65] & 0x08) && !DG.bas) {
        DG.bas = true;
        DG.coupe = false;
        DG.t_bas = t;
        DG.etat_bas = etat;
        fz_tr(C, "t=%llu bouton lu enfoncé (0x05 de %u octets)", (unsigned long long)t, f->len);
    } else if (!(f->msg[65] & 0x08) && DG.bas) {
        DG.bas = false;
        DG.relache = true;
        DG.rel_t = t;
        DG.rel_duree = t - DG.t_bas;
        DG.rel_coupe = DG.coupe;
        fz_tr(C, "t=%llu bouton lu relâché (0x05 de %u octets), après %llu µs", (unsigned long long)t, f->len,
              (unsigned long long)DG.rel_duree);
    }
}

/* La signature (PROTOCOL.md § 2) : un 0x07 dont le LensType2 (offsets 9-10) est 8, 9, 12, 13, 20, 21, 23, 0xC938,
 * 0xC93A, ou un 0x3F dont le nom (dès l'offset 1) commence par « SAMYANG » ou « LK SAMYANG ». */
static bool signature(const bsk_frame_t *f)
{
    static const uint16_t LT2[] = {8, 9, 12, 13, 20, 21, 23, 0xC938, 0xC93A};
    if (f->msg[0] == 0x07 && f->len >= 12) {
        uint16_t lt = (uint16_t)(f->msg[10] | f->msg[11] << 8);
        for (size_t i = 0; i < sizeof LT2 / sizeof LT2[0]; i++)
            if (lt == LT2[i]) return true;
    }
    if (f->msg[0] == 0x3F && f->len >= 10 && !memcmp(f->msg + 2, "SAMYANG ", 8)) return true;
    if (f->msg[0] == 0x3F && f->len >= 13 && !memcmp(f->msg + 2, "LK SAMYANG ", 11)) return true;
    return false;
}

bool __real_bsk_phy_poll(bsk_phy_event_t *ev);
bool __wrap_bsk_phy_poll(bsk_phy_event_t *ev);
bool __wrap_bsk_phy_poll(bsk_phy_event_t *ev)
{
    bool got = __real_bsk_phy_poll(ev);
    polls++;
    vide = !got;
    ev_courant = got ? ev : NULL;
    if (!got) return false;
    if (ev->kind == BSK_PHY_LENS_CS && ev->u.level && cs_high && !t_lcs) t_lcs = phy_sim_now();
    if (ev->kind == BSK_PHY_FRAME) {
        if (signature(&ev->u.frame)) sig = true;
        if (ev->u.frame.msg[0] == 0x40 && ev->u.frame.len >= 3 && requete.msg[0] == 0x40 &&
            ev->u.frame.msg[1] == requete.msg[1] && ev->u.frame.msg[2] == requete.msg[2])
            custom_repondu = true;
        doigt(&ev->u.frame, ev->t_us);
    }
    return true;
}

/* PROTOCOL.md § 6, écrit à la main : la réponse d'une requête est la première trame reçue dont le premier message est du
 * type demandé et, pour trois types, le contenu celui de la réponse à cette requête : 0x40, la même commande principale
 * et la même sous-commande ; 0x0B, l'écho exact ; 0x0A, aucun bit ni aucun octet hors de la requête. */
static bool repond(const bsk_frame_t *r, const bsk_frame_t *q)
{
    if (!r->len || !q->len || r->msg[0] != q->msg[0]) return false;
    if (r->msg[0] == 0x40) return r->len >= 3 && q->len >= 3 && r->msg[1] == q->msg[1] && r->msg[2] == q->msg[2];
    if (r->msg[0] == 0x0B) return r->len == q->len && !memcmp(r->msg, q->msg, q->len);
    if (r->msg[0] == 0x0A) {
        if (r->len > q->len) return false;
        for (uint16_t i = 1; i < r->len; i++)
            if (r->msg[i] & ~q->msg[i]) return false;
    }
    return true;
}

/* L'issue d'une requête lue par la SESSION (session.c, advance ; cmd.c, cmd_custom_poll) : DONE, sa réponse jugée. */
void __real_bsk_txn_ack(void);
void __wrap_bsk_txn_ack(void);
void __wrap_bsk_txn_ack(void)
{
    const bsk_frame_t *r = bsk_txn_reply();
    if (bsk_txn_state() == TXN_DONE && !repond(r, &requete))
        fz_fail(C, C_REPONSE, "t=%llu : la requête %02X %02X %02X (len %u) terminée par %02X %02X %02X (len %u)",
                (unsigned long long)phy_sim_now(), requete.msg[0], requete.msg[1], requete.msg[2], requete.len, r->msg[0],
                r->msg[1], r->msg[2], r->len);
    __real_bsk_txn_ack();
}

bsk_err_t __real_bsk_phy_send(const bsk_frame_t *f);
bsk_err_t __wrap_bsk_phy_send(const bsk_frame_t *f);
bsk_err_t __wrap_bsk_phy_send(const bsk_frame_t *f)
{
    uint64_t t = phy_sim_now();
    for (size_t i = 0; i + 19 <= f->len; i++) {
        if (f->msg[i] != 0x40) continue;
        if (!sig) fz_fail(C, C_SIGN, "t=%llu : un 0x40 '%c' %02X émis sans signature dans la session", (unsigned long long)t,
                          f->msg[i + 1], f->msg[i + 2]);
        if (f->msg[i + 1] == 'P' && f->msg[i + 2] == 0x38) {
            unsigned k = 0;
            while (k < n_cr && CR[k].octet != f->msg[i + 3]) k++;
            if (!n_cr) fz_fail(C, C_ECRIT, "t=%llu : un 'P' 0x38 émis sans CUSTOM WRITE déposée", (unsigned long long)t);
            else if (k == n_cr)
                fz_fail(C, C_ECRIT, "t=%llu : un 'P' 0x38 d'octet %02X émis, %llX demandé (0x30 + arg)", (unsigned long long)t,
                        f->msg[i + 3], (unsigned long long)CR[0].octet);
            else CR[k] = CR[--n_cr];
        }
        break;
    }
    if (!admise(f, (int)decl))
        fz_fail(C, C_LISTE, "t=%llu : type %02X len %u émis, déclaration %d", (unsigned long long)t, f->msg[0], f->len,
                (int)decl);
    if (f->cls == 2) requete = *f;
    if (f->len >= 3 && f->msg[0] == 0x40 && f->msg[1] == 'P') custom_emis = true;
    if (f->len >= 2 && f->msg[0] == 0x10 && f->msg[1] == 0x08 && etat == SESSION_HOMING && t - t_homing < 100 * MS)
        fz_fail(C, C_DHOME, "t=%llu : le 0x10 08 %llu µs après l'entrée en HOMING", (unsigned long long)t,
                (unsigned long long)(t - t_homing));
    return __real_bsk_phy_send(f);
}

/* Le verrou, vu par le modèle de D2 : le rail ne s'allume que l'insertion confirmée, sans coupure depuis. */
static void presence(uint64_t t, const char *ou)
{
    d2_at(t);
    if (!D.present) fz_fail(C, C_PRES, "t=%llu : un rail allumé (%s), D2 %s, insertion non confirmée ou retombée",
                            (unsigned long long)t, ou, D.pin ? "présent" : "absent");
}

bool __real_bsk_phy_rail(bsk_rail_t rail, bool on);
bool __wrap_bsk_phy_rail(bsk_rail_t rail, bool on);
bool __wrap_bsk_phy_rail(bsk_rail_t rail, bool on)
{
    uint64_t t = phy_sim_now();
    bool was = phy_sim_wires()->rail[rail], ok;
    if (on && rail == BSK_RAIL_MOTOR && t - t_logic < 50 * MS)
        fz_fail(C, C_DLOG, "t=%llu : rail moteur %llu µs après le logique", (unsigned long long)t,
                (unsigned long long)(t - t_logic));
    if (!on && was && in_cmd != CMD_DETACH && in_cmd != CMD_CLEAR_FAULT &&
        !(ev_courant && ev_courant->kind == BSK_PHY_PRESENCE && !ev_courant->u.present))
        fz_fail(C, C_RAIL, "t=%llu : rail %d coupé hors de p0, b et d'une retombée de D2", (unsigned long long)t, (int)rail);
    if (!on && rail == BSK_RAIL_LOGIC) desync = false;   /* les rails coupés (to_off, puis cmd_rails_cut) : plus rien
                                                            en attente de l'objectif */
    ok = __real_bsk_phy_rail(rail, on);
    if (on && rail == BSK_RAIL_LOGIC) t_logic = t;
    if (on && rail == BSK_RAIL_MOTOR) t_motor = t;
    if (on && ok) presence(t, rail == BSK_RAIL_LOGIC ? "logique" : "moteur");
    return ok;
}

bool __real_bsk_phy_lines(bool drive);
bool __wrap_bsk_phy_lines(bool drive);
bool __wrap_bsk_phy_lines(bool drive)
{
    uint64_t t = phy_sim_now();
    if (drive && t - t_motor < 50 * MS)
        fz_fail(C, C_DLIG, "t=%llu : les lignes %llu µs après le rail moteur", (unsigned long long)t,
                (unsigned long long)(t - t_motor));
    if (!drive) cs_high = false;
    return __real_bsk_phy_lines(drive);
}

void __real_bsk_phy_body_cs(bool high);
void __wrap_bsk_phy_body_cs(bool high);
void __wrap_bsk_phy_body_cs(bool high)
{
    uint64_t t = phy_sim_now();
    if (high && !cs_high && t - t_cs_low < 15 * MS)
        fz_fail(C, C_DBAS, "t=%llu : BODY_CS haute %llu µs après basse", (unsigned long long)t,
                (unsigned long long)(t - t_cs_low));
    if (!high && cs_high && t_lcs && t - (t_lcs > t_cs_high ? t_lcs : t_cs_high) < 3 * MS)
        fz_fail(C, C_DHAUT, "t=%llu : BODY_CS basse %llu µs après LENS_CS vue haute", (unsigned long long)t,
                (unsigned long long)(t - (t_lcs > t_cs_high ? t_lcs : t_cs_high)));
    if (high && !cs_high) {
        t_cs_high = t;
        t_lcs = bsk_phy_lens_cs() ? t : 0;          /* déjà haute : vue au pas qui lève BODY_CS (session.c, S_HS_UP) */
    }
    if (!high) t_cs_low = t;
    cs_high = high;
    __real_bsk_phy_body_cs(high);
}

/* P8 : une écriture déposée est une ligne js ou jx en cours de dépôt, ou le relâchement du bouton, lu dans le 0x05 que la
 * session traite (le doigt, plus haut). */
bool __real_bsk_store_request(bsk_store_op_t op, const char *key, uint32_t value);
bool __wrap_bsk_store_request(bsk_store_op_t op, const char *key, uint32_t value);
bool __wrap_bsk_store_request(bsk_store_op_t op, const char *key, uint32_t value)
{
    uint64_t t = phy_sim_now();
    bool geste = in_cmd == CMD_SET_MARK || in_cmd == CMD_CLEAR_MARK, ok;
#ifdef FZ_HOTE
    geste = geste && ligne_active && (LC.fam == F_JS || LC.fam == F_JX);   /* déposée par une ligne js ou jx */
#endif
    if (op == BSK_STORE_GET && nvs_poke) {
        store_sim_poke(key, nvs_val);
        nvs_poke = false;
    }
    if (op != BSK_STORE_GET && !geste) {
        bool rel = ev_courant && ev_courant->kind == BSK_PHY_FRAME && ev_courant->u.frame.msg[0] == 0x05 && DG.relache;
        B.ecritures_bouton++;
        if (!std_model && !DR.faux) B.appuis_juges++;
        if (!rel)
            fz_fail(C, C_NVS, "t=%llu : écriture %s déposée hors de js, jx et du relâchement du bouton", (unsigned long long)t,
                    op == BSK_STORE_PUT ? "PUT" : "DEL");
        else if (DG.rel_duree < 1000000u || DG.rel_duree >= 4000000u || DG.etat_bas != SESSION_READY)
            fz_fail(C, C_DOIGT, "t=%llu : écriture du bouton, un appui de %llu µs commencé en %s", (unsigned long long)t,
                    (unsigned long long)DG.rel_duree, bsk_state_name(DG.etat_bas));
        else if (!std_model && !DR.faux && (!DR.fini || DR.servi || DR.duree < 900000u || DR.duree >= 4100000u))
            fz_fail(C, C_APPUI, "t=%llu : écriture du bouton (un appui lu de %llu µs) ; le dernier appui réel %s%llu µs%s",
                    (unsigned long long)t, (unsigned long long)DG.rel_duree, DR.fini ? "" : "en cours depuis ",
                    (unsigned long long)(DR.fini ? DR.duree : t - DR.t_bas), DR.servi ? ", déjà servi" : "");
        else if (DG.rel_coupe) {
            B.appuis_coupes++;
            fz_fail(C, C_HAVE64, "t=%llu : écriture du bouton, un appui de %llu µs vu en partie sans l'offset 64",
                    (unsigned long long)t, (unsigned long long)DG.rel_duree);
        }
        if (DR.fini) DR.servi = true;
    }
    ok = __real_bsk_store_request(op, key, value);
#ifdef FZ_HOTE
    if (ok && g_store_sim.hold && !mag_due) mag_due = t + mag_lat;
#endif
    return ok;
}

/* La source de la marque : la dernière lecture rendue (sa valeur, ou aucune), ou un geste rangé (mark.c, on_result). */
bool __real_bsk_store_result(bsk_store_res_t *r);
bool __wrap_bsk_store_result(bsk_store_res_t *r);
bool __wrap_bsk_store_result(bsk_store_res_t *r)
{
    bool got = __real_bsk_store_result(r);
    if (got && r->op == BSK_STORE_GET) {
        marque_src = r->ok ? 1 : 0;
        marque_v = r->value;
    } else if (got && r->ok) {
        marque_src = 2;
    }
    return got;
}

/* ─────────────── les trames perdues ou fausses sur le fil (phy_sim_fault) ─────────────── */

typedef struct {
    bool     vers_objectif;
    uint8_t  type;
    uint16_t n;
    phy_sim_fate_t sort;
} fate_t;
static fate_t FATES[4];
static unsigned n_fates;

static phy_sim_fate_t sort_trame(bool to_lens, const bsk_frame_t *f)
{
    for (unsigned i = 0; i < n_fates; i++) {
        if (FATES[i].vers_objectif != to_lens || (FATES[i].type && FATES[i].type != f->msg[0])) continue;
        {
            phy_sim_fate_t s = FATES[i].sort;
            if (FATES[i].n && --FATES[i].n == 0) {
                FATES[i] = FATES[--n_fates];
            }
            return s;
        }
    }
    return PHY_SIM_PASS;
}

/* ─────────────── les gabarits de l'objectif fou ───────────────
 * Des messages du protocole, jamais des octets de firmware : les réponses des traces (sources135.c, copiées de traces/)
 * et des messages écrits ici d'après PROTOCOL.md et samyang.md (les noms, les notifications 0x40, le refus 0x02). */
#define N_GABARITS 14u
static size_t gabarit(uint8_t k, uint8_t *m)
{
    static const char *const NOMS[] = {"SAMYANG AF 135mm F1.8 FE", "LK SAMYANG AF 24mm", "TAMRON 28-75", "SAMYANG"};
    memset(m, 0, 64);
    switch (k % N_GABARITS) {
    case 0: m[0] = 0x10; return 2;                                              /* 10 00, le homing fini */
    case 1: m[0] = 0x10; m[1] = 0x01; return 2;                                 /* 10 01, le homing en échec */
    case 2: m[0] = 0x40; m[1] = 'W'; return 19;                                 /* une notification */
    case 3: m[0] = 0x40; m[1] = 'H'; return 19;
    case 4: m[0] = 0x40; m[1] = 'P'; m[2] = 0xFA; m[3] = 0x21; return 19;       /* une réponse CUSTOM */
    case 5: return sources135_msg(SRC_R07, m, BSK_MSG_MAX);                     /* l'identité du 135 */
    case 6: return sources135_msg(SRC_R01, m, BSK_MSG_MAX);
    case 7:
    case 8:
    case 9:
    case 10: {
        const char *s = NOMS[k % N_GABARITS - 7];
        m[0] = 0x3F;
        memcpy(m + 2, s, strlen(s));
        return 2 + strlen(s) + 1;
    }
    case 11: m[0] = 0x02; return 2;                                             /* un refus */
    case 12: {                                                                  /* un 0x05 long, bouton enfoncé */
        size_t n = sources135_msg(SRC_T05, m, BSK_MSG_MAX);
        if (n > 65) m[65] |= 0x08;
        return n;
    }
    default: return sources135_msg(SRC_T06, m, BSK_MSG_MAX);
    }
}

/* ─────────────── le pas, observé ─────────────── */

/* P6 débit : les lignes du plafond et les transitions comptées par seconde de leur instant (le troisième jeton, en
 * millisecondes, celui de la fenêtre du journal : journal.c, window) ; une seconde alignée en recouvre au plus deux
 * fenêtres, d'où 2 × (PER_S + TR_PER_S). Une ligne sans instant (`* lost`, `* rxflood`) compte à l'instant du pas. */
#define DEBIT_MAX (2u * (30u + 30u))
static struct {
    uint64_t sec[64];
    unsigned n[64];
} DB;

static void debit(const char *b, size_t n, uint64_t t_pas)
{
    uint64_t ms = t_pas / MS, s = 0;
    const char *p = b + 2;
    unsigned k;
    if (n < 3 || strncmp(b, "* ", 2) || !strncmp(b, "* xdetect", 9) || !strncmp(b, "* std", 5) || !strncmp(b, "* ring", 6))
        return;
    while (p < b + n && *p != ' ') p++;
    if (p < b + n && p[1] >= '0' && p[1] <= '9') {
        ms = 0;
        for (p++; p < b + n && *p >= '0' && *p <= '9'; p++) ms = ms * 10 + (uint64_t)(*p - '0');
    }
    s = ms / 1000;
    k = (unsigned)(s % 64);
    if (DB.sec[k] != s + 1) {
        DB.sec[k] = s + 1;
        DB.n[k] = 0;
    }
    if (++DB.n[k] > DEBIT_MAX)
        fz_fail(C, C_DEBIT, "t=%llu : %u lignes du plafond et transitions datées de la seconde %llu", (unsigned long long)t_pas,
                DB.n[k], (unsigned long long)s);
}

static unsigned late_n;
static uint64_t late_us;
static uint16_t sets_avant;
static bool     rail_avant[2];
static uint32_t cuts_obs;
static uint32_t id_avant;
static uint32_t refus0;         /* bsk_bench_refused() au début du cas */

static void observer(uint64_t t)
{
    static char b[4096];
    bsk_status_t s;
    const phy_sim_wires_t *w = phy_sim_wires();
    uint32_t gen;
    size_t n;
    lire_accuses();
    while ((n = bsk_journal_take(b, sizeof b, &gen)) > 0) {
        if (gen != bsk_journal_gen()) continue;     /* d'avant un LOG : host_usb.c (write_line) ne l'écrit pas */
        if (n >= 600) fz_fail(C, C_LIGNE, "t=%llu : une ligne du journal de %zu octets", (unsigned long long)t, n);
        if (journal_mode == 0) fz_fail(C, C_MUET, "t=%llu : journal éteint, « %.60s »", (unsigned long long)t, b);
        if (journal_mode == 1) debit(b, n < sizeof b ? n : sizeof b, t);
    }
    bsk_session_status(&s);
    d2_at(t);
    if (bsk_bench_refused() != refus0)              /* compté depuis le démarrage du processus : la hausse dans le cas */
        fz_fail(C, C_REFUS, "t=%llu : bsk_bench_refused() = %lu, %lu au début du cas", (unsigned long long)t,
                (unsigned long)bsk_bench_refused(), (unsigned long)refus0);
    if (s.session_state != etat) fz_fail(C, C_ETAT, "t=%llu : publié %u, entré %u", (unsigned long long)t, s.session_state,
                                         etat);
    if (TMAX[etat] && (t - depuis) - (retard_cumul - retard_entree) > TMAX[etat])
        fz_fail(C, C_DUREE, "t=%llu : %s tenu %llu µs (retards du pas retirés)", (unsigned long long)t, bsk_state_name(etat),
                (unsigned long long)((t - depuis) - (retard_cumul - retard_entree)));
    if (!D.present && D.cuts && vide && etat != SESSION_OFF)
        fz_fail(C, C_OFF, "t=%llu : D2 coupé (%lu coupures), la session en %s", (unsigned long long)t, (unsigned long)D.cuts,
                bsk_state_name(etat));
    if ((s.aperture_current && (s.aperture_current < 0x0E00 || s.aperture_current > 0x2000)) ||
        ((s.capabilities & CAP_APERTURE) ? s.aperture_min < 0x0E00 || s.aperture_max > 0x2000 || s.aperture_min > s.aperture_max
                                         : s.aperture_min || s.aperture_max) ||
        (s.limits_valid ? s.focus_min > s.focus_max || !(s.capabilities & CAP_LIMITS_REPORTED) : s.focus_min || s.focus_max) ||
        (s.position_valid && s.session_state != SESSION_READY))
        fz_fail(C, C_INST, "t=%llu : état %s ouverture %04X plage %04X..%04X cap %lX bornes %d %ld..%ld position %d",
                (unsigned long long)t, bsk_state_name(s.session_state), s.aperture_current, s.aperture_min, s.aperture_max,
                (unsigned long)s.capabilities, s.limits_valid, (long)s.focus_min, (long)s.focus_max, s.position_valid);
    if (s.lens_id_product && s.lens_id_product != id_avant && etat != SESSION_IDENTIFYING)
        fz_fail(C, C_IDENT, "t=%llu : identité %lX publiée en %s", (unsigned long long)t, (unsigned long)s.lens_id_product,
                bsk_state_name(etat));
    id_avant = s.lens_id_product;
    for (int r = 0; r < 2; r++) {
        if (rail_avant[r] && !w->rail[r] && D.cuts == cuts_obs && !cut_cmd)
            fz_fail(C, C_RAIL, "t=%llu : rail %d retombé sans D2 ni p0/b", (unsigned long long)t, r);
        if (w->rail[r]) presence(t, "observé");
        rail_avant[r] = w->rail[r];
    }
    cuts_obs = D.cuts;
    cut_cmd = false;
    if (s.mark_valid && marque_src == 1 && ((marque_v >> 24) || s.mark_position != (int32_t)(marque_v & 0xFFFFu)))
        fz_fail(C, C_MARQUE, "t=%llu : marque publiée en %ld, la valeur relue %08lX", (unsigned long long)t,
                (long)s.mark_position, (unsigned long)marque_v);
    if (s.mark_sets != sets_avant) cov |= 1u << COV_MARQUE;
    sets_avant = s.mark_sets;
}

#ifdef FZ_HOTE
/* ─────────────── l'USB, servi comme host_usb.c ───────────────
 * Les octets reçus passent par le cadrage (hote.c, recopié de rx_task) jusqu'à une ligne livrée ; rx_task attend alors
 * sa réponse (deliver) et ne cadre plus rien : les octets suivants attendent. bsk_host_usb_service la donne à
 * bsk_host_line, une par tour de la boucle de main.c, puis redemande une réponse différée à bsk_host_later à chaque
 * tour, jusqu'à ce qu'elle soit rendue. */
static void avancer(void)
{
    while (!U.vol && U.r < U.w)
        if (cadre(&U.cad, U.fifo[U.r++], U.ligne)) U.vol = true;
}

static void pousser(const uint8_t *o, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        if (U.w < sizeof U.fifo) U.fifo[U.w++] = o[i];
        else U.perdus++;
    }
    B.octets_usb += n;
    avancer();
}

static void juger_reponse(const char *fin, bool later, bool finale)
{
    static const int CK[HJ_N] = {[HJ_FIN] = C_P10FIN, [HJ_TAMPON] = C_P10TAMPON, [HJ_LIGNE] = C_P10LIGNE,
                                 [HJ_LONGUEUR] = C_P10LONG, [HJ_OK] = C_P10OK, [HJ_FORME] = C_P10FORME,
                                 [HJ_P4] = C_P4HOTE, [HJ_B4] = C_B4};
    hote_verdict_t v;
    hote_juge(&v, &M, &LC, U.ligne, R, fin, later, finale);
    journal_mode = (uint8_t)M.journal;              /* LOG ON|ALL|OFF, d'après la ligne (hote_log) */
    if (v.k >= 0) fz_fail(C, CK[v.k], "t=%llu : %s", (unsigned long long)phy_sim_now(), v.why);
}

static void servir(void)
{
    const char *fin;
    U.servi = phy_sim_now() + 1;                    /* + 1 : l'instant 0 n'est pas « jamais servi » */
    if (U.later) {
        uint64_t ecoule = phy_sim_now() - U.t0 - (retard_cumul - U.r0);
        fin = bsk_host_later(R, sizeof R);
        if (fin == BSK_HOST_LATER) {
            if (ecoule > hote_borne_us(LC.fam))
                fz_fail(C, C_LATER, "t=%llu : « %s » (%s) sans réponse depuis %llu µs, retards du pas retirés (borne %llu)",
                        (unsigned long long)phy_sim_now(), hote_vis(U.ligne), HOTE_FAM[LC.fam], (unsigned long long)ecoule,
                        (unsigned long long)hote_borne_us(LC.fam));
            return;
        }
        fz_tr(C, "t=%llu réponse différée « %s » après %llu µs", (unsigned long long)phy_sim_now(), R, (unsigned long long)ecoule);
        if (ecoule > B.differee_max_us) B.differee_max_us = ecoule;
        juger_reponse(fin, false, true);
        U.later = U.vol = false;
        avancer();
        return;
    }
    if (!U.vol) return;
    memset(&LC, 0, sizeof LC);
    LC.fam = hote_famille(U.ligne);
    LC.now_us = phy_sim_now();
    bsk_session_status(&LC.st);
    B.lignes_fam[LC.fam]++;
    B.lignes_etat[LC.st.session_state & 7]++;
    if (LC.fam == F_FN || LC.fam == F_M || LC.fam == F_AX || LC.fam == F_JS || LC.fam == F_JX || LC.fam == F_JG ||
        LC.fam == F_CUSTOM)
        B.b4[LC.st.session_state == SESSION_READY]++;
    ligne_active = true;
    fin = bsk_host_line(U.ligne, R, sizeof R);
    ligne_active = false;
    if (C->trace) {
        char e[HOTE_LIGNE_MAX * 4 + 1];
        size_t k = 0;
        for (const char *p = U.ligne; *p; p++)
            k += (size_t)snprintf(e + k, sizeof e - k, (unsigned char)*p < 0x20 || (unsigned char)*p > 0x7E || *p == '\\' ? "\\x%02X" : "%c",
                                  (unsigned char)*p);
        fz_tr(C, "t=%llu ligne « %s » (%s, en %s) -> %s%s%s", (unsigned long long)phy_sim_now(), e, HOTE_FAM[LC.fam],
              bsk_state_name(LC.st.session_state), fin == BSK_HOST_LATER ? "(différée)" : fin ? "« " : "(aucune réponse)",
              fin && fin != BSK_HOST_LATER ? R : "", fin && fin != BSK_HOST_LATER ? " »" : "");
    }
    if (fin == BSK_HOST_LATER) {
        B.differees++;
        juger_reponse(fin, true, false);
        U.later = true;
        U.t0 = phy_sim_now();
        U.r0 = retard_cumul;
        return;
    }
    juger_reponse(fin, false, false);
    U.vol = false;
    avancer();
}

/* La ligne d'une commande des scénarios de F2 (O_CMD, K_*), telle qu'un pilote l'écrit, avec sa fin : la cible de
 * K_GOTO_SEL calculée sur l'instantané, comme en F2. */
static size_t ligne_de(uint8_t k, uint16_t b, uint32_t v, const bsk_status_t *s, char *o, size_t cap)
{
    static const char *const FN[] = {"1.8", "2.8", "4", "5.6", "8", "11", "16", "22"};
    int32_t lo = s->focus_min, hi = s->focus_max, p = s->focus_position, c;
    switch (k % K_N) {
    case K_GOTO: return (size_t)snprintf(o, cap, "f%lu\n", (unsigned long)(v & 0xFFFF));
    case K_GOTO_SEL:
        switch (b % 8) {
        case 0: c = lo; break;
        case 1: c = hi; break;
        case 2: c = lo - 1; break;
        case 3: c = hi + 1; break;
        case 4: c = lo + (hi - lo) / 2; break;
        case 5: c = p; break;
        case 6: c = p + (int32_t)(v % 4001) - 2000; break;
        default: c = p - lo > hi - p ? lo + 100 : hi - 100; break;
        }
        if (c < 0) c = 0;
        if (c > 0xFFFF) c = 0xFFFF;
        return (size_t)snprintf(o, cap, "f%ld\n", (long)c);
    case K_MOVE: {
        int32_t d = (int32_t)(v % 131071) - 65535;
        return (size_t)snprintf(o, cap, "f%c%ld\n", d < 0 ? '-' : '+', (long)(d < 0 ? -d : d));
    }
    case K_STOP: return (size_t)snprintf(o, cap, "q\n");
    case K_JS: return (size_t)snprintf(o, cap, "js\n");
    case K_JX: return (size_t)snprintf(o, cap, "jx\n");
    case K_JG: return (size_t)snprintf(o, cap, "jg\n");
    case K_B: return (size_t)snprintf(o, cap, "b\n");
    case K_P1: return (size_t)snprintf(o, cap, "p1\n");
    case K_P0: return (size_t)snprintf(o, cap, "p0\n");
    case K_CUSTOM_R: return (size_t)snprintf(o, cap, "CUSTOM READ\n");
    case K_CUSTOM_W: return (size_t)snprintf(o, cap, "CUSTOM WRITE %u%u\n", (unsigned)(v % 3), (unsigned)((v / 3) % 3));
    default: return (size_t)snprintf(o, cap, "a%s\n", FN[v % (sizeof FN / sizeof FN[0])]);
    }
}

/* Le magasin qui retient ses opérations (O_MAGASIN) : rendues, dans l'ordre, mag_lat après la première en attente. */
static void magasin(uint64_t t)
{
    if (mag_due && t >= mag_due) {
        store_sim_flush();
        mag_due = 0;
    }
}
#endif

static void pas(uint64_t t)
{
    if (late_n) {
        late_n--;
        t += late_us;
        retard_cumul += late_us;
        B.retards++;
    }
#ifdef FZ_HOTE
    magasin(t);
#endif
    phy_sim_run(t);
    polls = 0;
    vide = false;
    bsk_session_step(t);
    ev_courant = NULL;                              /* l'événement du pas n'existe plus (la pile de bsk_session_step) */
    if (polls > 64) fz_fail(C, C_POLL, "t=%llu : %u bsk_phy_poll dans un pas", (unsigned long long)t, polls);
#ifdef FZ_HOTE
    bsk_host_observe();                             /* main.c : après chaque pas */
#endif
    observer(t);
#ifdef FZ_HOTE
    bsk_session_real(t);                            /* main.c : l'instant réel, puis bsk_host_usb_service */
    servir();
#endif
}

/* Jusqu'à t_fin, ou jusqu'à ce que `fini` le dise. Le rythme de la carte (main/main.c) : un pas, puis un tic de 1 ms
 * rendu au système ; un tic où ni la PHY ni la SESSION n'ont rien à faire (phy_sim_next, bsk_session_next, la fin de
 * l'anti-rebond de D2 que phy_sim_next ne compte pas) ne change rien, et le banc saute au premier tic qui suit leur
 * prochaine échéance. Sous fuzz_hote, une ligne livrée attend le tic suivant (une ligne par tour de boucle), et le
 * magasin qui retient ses opérations les rend à leur échéance. */
#define TIC (1u * MS)
static void courir(uint64_t t_fin, bool (*fini)(void))
{
    while (C->check < 0 && phy_sim_now() < t_fin) {
        uint64_t now = phy_sim_now(), nx = phy_sim_next(), ts = bsk_session_next(), t = now + TIC;
        if (ts < nx) nx = ts;
        if (D.wake > now && D.wake < nx) nx = D.wake;
#ifdef FZ_HOTE
        if (U.vol && !U.later && now + TIC < nx) nx = now + TIC;
        if (mag_due && mag_due < nx) nx = mag_due;
#endif
        if (nx > t) t = nx >= UINT64_MAX - TIC ? t_fin : now + (nx - now + TIC - 1) / TIC * TIC;
        if (t > t_fin) t = t_fin;
        pas(t);
        if (fini && fini()) break;
    }
}

/* ─────────────── les commandes ─────────────── */

/* Une commande déposée, telle que HOTE la dépose (fuzz_hote : HOTE lui-même, par --wrap) : inscrite pour ses accusés,
 * le crédit d'un 'P' 0x38 et son octet (`octet38`, -1 sans), et son premier accusé attendu dans l'appel (C_ACKPREM). */
#ifdef FZ_HOTE
void __real_bsk_session_command(const bsk_cmd_t *c);
#define SESSION_COMMANDE __real_bsk_session_command
#else
#define SESSION_COMMANDE bsk_session_command
#endif
static void inscrire(const bsk_cmd_t *c, bool juger, int32_t cible, int64_t octet38)
{
    if (c->seq < SEQ_MAX) {
        if (c->seq > seq_n) seq_n = c->seq;
        CMDS[c->seq].op = (uint8_t)c->op;
        CMDS[c->seq].etat = A_RIEN;
        CMDS[c->seq].cible_jugee = juger;
        CMDS[c->seq].cible = cible;
    }
    if (c->op == CMD_LENS_CUSTOM && c->arg >= 0 && octet38 >= 0) {
        if (n_cr < sizeof CR / sizeof CR[0]) {
            CR[n_cr].seq = c->seq;
            CR[n_cr++].octet = octet38;
        }
    }
    fz_tr(C, "t=%llu commande seq %lu op %d arg %ld", (unsigned long long)phy_sim_now(), (unsigned long)c->seq, (int)c->op,
          (long)c->arg);
    dep_actif = true;
    dep_vu = false;
    dep_seq = c->seq;
    in_cmd = (int)c->op;
    SESSION_COMMANDE(c);
    in_cmd = -1;
    dep_actif = false;
    if (!dep_vu && c->op != CMD_HOME && c->op != CMD_RING_SET) {
        B.sans_accuse++;
        fz_fail(C, C_ACKPREM, "t=%llu : seq %lu op %u arg %ld : aucun accusé dans l'appel", (unsigned long long)phy_sim_now(),
                (unsigned long)c->seq, (unsigned)c->op, (long)c->arg);
    }
}

#ifdef FZ_HOTE
/* Ce que HOTE dépose : la commande de la ligne en cours (LC), son premier accusé. Le goto du bouton (BSK_SEQ_BOARD) ne
 * vient pas de HOTE ; cmd.c le dépose dans sa propre unité, et il ne passe pas par ici. */
void __wrap_bsk_session_command(const bsk_cmd_t *c);
void __wrap_bsk_session_command(const bsk_cmd_t *c)
{
    if (!ligne_active || c->seq == BSK_SEQ_BOARD) {
        if (ligne_active) LC.depots++;
        __real_bsk_session_command(c);
        return;
    }
    LC.depots++;
    LC.cmd = *c;
    inscrire(c, false, 0, hote_custom_write(U.ligne));
    LC.premier_vu = dep_vu;
    LC.premier = dep_premier;
}
#endif

static void deposer(bsk_cmd_op_t op, int32_t arg, bool juger, int32_t cible)
{
    bsk_cmd_t c = {.seq = ++seq_n, .op = op, .arg = arg};
    if (seq_n >= SEQ_MAX) return;
    bsk_session_real(phy_sim_now());
    inscrire(&c, juger, cible, (int64_t)0x30 + arg);
    lire_accuses();
}

#ifdef FZ_HOTE
/* Les octets d'une ligne, reçus maintenant ; une ligne livrée et rien en attente : servie tout de suite (le tour de
 * boucle de ce tic, main.c), sauf si une ligne l'a déjà été à cet instant : elle attend le tour suivant (courir). */
static void recevoir(const uint8_t *o, size_t n)
{
    pousser(o, n);
    if (U.vol && !U.later && U.servi != phy_sim_now() + 1) {     /* pas déjà une ligne servie à ce tour */
        bsk_session_real(phy_sim_now());
        servir();
    }
}
#endif

static void commande(uint8_t k, uint16_t b, uint32_t v)
{
    bsk_status_t s;
    bsk_session_status(&s);
#ifdef FZ_HOTE
    {
        char o[64];
        size_t n = ligne_de(k, b, v, &s, o, sizeof o);
        recevoir((const uint8_t *)o, n);
        return;
    }
#endif
    switch (k % K_N) {
    case K_GOTO: deposer(CMD_FOCUS_GOTO, (int32_t)(v & 0xFFFF), true, (int32_t)(v & 0xFFFF)); break;
    case K_GOTO_SEL: {
        int32_t lo = s.focus_min, hi = s.focus_max, p = s.focus_position, c;
        switch (b % 8) {
        case 0: c = lo; break;
        case 1: c = hi; break;
        case 2: c = lo - 1; break;
        case 3: c = hi + 1; break;
        case 4: c = lo + (hi - lo) / 2; break;
        case 5: c = p; break;
        case 6: c = p + (int32_t)(v % 4001) - 2000; break;
        default: c = p - lo > hi - p ? lo + 100 : hi - 100; break;   /* loin de la position */
        }
        if (c < 0) c = 0;
        if (c > 0xFFFF) c = 0xFFFF;
        deposer(CMD_FOCUS_GOTO, c, true, c);
        break;
    }
    case K_MOVE: {
        int32_t d = (int32_t)(v % 131071) - 65535;
        if (s.position_valid && (s.focus_position + d < 0 || s.focus_position + d > 0xFFFF)) break;   /* er range */
        deposer(CMD_FOCUS_MOVE, d, false, 0);
        break;
    }
    case K_STOP: deposer(CMD_FOCUS_STOP, 0, false, 0); break;
    case K_JS: deposer(CMD_SET_MARK, BSK_MARK_HERE, false, 0); break;
    case K_JX: deposer(CMD_CLEAR_MARK, 0, false, 0); break;
    case K_JG:
        if (s.mark_valid) deposer(CMD_FOCUS_GOTO, s.mark_position, true, s.mark_position);
        break;
    case K_B: deposer(s.session_state == SESSION_FAULT ? CMD_CLEAR_FAULT : CMD_ATTACH, 0, false, 0); break;
    case K_P1:
        if (s.session_state == SESSION_OFF || s.session_state == SESSION_FAULT) deposer(CMD_ATTACH, 0, false, 0);
        break;
    case K_P0: deposer(CMD_DETACH, 0, false, 0); break;
    case K_CUSTOM_R: deposer(CMD_LENS_CUSTOM, -1, false, 0); break;
    case K_CUSTOM_W: deposer(CMD_LENS_CUSTOM, (int32_t)((v % 3) << 4 | (v / 3) % 3), false, 0); break;
    default: deposer(CMD_APERTURE_SET, (int32_t)(0x0E00 + v % 0x1201), false, 0); break;
    }
}

/* ─────────────── un événement ─────────────── */

static uint32_t le32(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }

static void alimenter(bool on)
{
    if (on == lens_on) return;
    lens_on = on;
    lens_sim_power(fou(), phy_sim_now(), on);
}

static void jouer(uint8_t op, uint8_t a, uint16_t b, uint32_t v)
{
    uint64_t t = phy_sim_now();
    switch (op % O_N) {
    case O_CMD: commande(a, b, v); break;
    case O_LATE:
        late_n = a ? a : 1;
        late_us = v % (10u * S);
        break;
    case O_D2:
        if (!(a & 1) && (b & 1)) alimenter(false);
        phy_sim_d2(a & 1);
        d2_set(t, a & 1);
        if ((a & 1) && (b & 1)) alimenter(true);
        break;
    case O_POWER: alimenter(a & 1); break;
    case O_RULE:
        fou_rule(&F, (fou_rule_t){.act = (uint8_t)(a % FOU_N), .type = (uint8_t)b, .n = (uint16_t)(b >> 8), .v = v});
        break;
    case O_CLEAR: fou_clear(&F); break;
    case O_INJ:
        if (a & 1) {
            uint8_t m[BSK_MSG_MAX], fr[BSK_FRAME_MAX];
            size_t n = gabarit((uint8_t)b, m);
            fou_inject(&F, fr, fr_encode(2, 0, m, n, fr, sizeof fr), true, 0);
        } else {
            (void)fou_inject_mem(&F, (uint8_t)b);
        }
        break;
    case O_LCS: fou_cs(&F, (fou_cs_t)(a % 3)); break;
    case O_BUSRX: phy_sim_bus_error(); break;
    case O_BUSTX: phy_sim_send_bus_error(); break;
    case O_FATE:
        if (n_fates < 4)
            FATES[n_fates++] = (fate_t){!(a & 1), (uint8_t)b, (uint16_t)(b >> 8), v % 2 ? PHY_SIM_LOSE : PHY_SIM_GARBLE};
        break;
    case O_RAILF: phy_sim_rail_fault((bsk_rail_t)(a & 1), b & 1); break;
    case O_BTN:
        if (std_model) break;
        L135.in.button = a & 1;
        if ((a & 1) && !DR.bas) {
            DR.bas = true;
            DR.t_bas = t;
        } else if (!(a & 1) && DR.bas) {
            DR.bas = false;
            DR.fini = true;
            DR.servi = false;
            DR.duree = t - DR.t_bas;
        }
        break;
    case O_ALIM: {
        lens_sim_powered_t st = {.flow = a & 1, .service = (a & 2) != 0, .grant = NULL};
        if (lens_sim_start_powered(fou(), t, &st)) lens_on = true;
        break;
    }
    case O_NVS:
        switch (a % 5) {
        case 0: g_store_sim.fail = g_store_sim.fail_get = g_store_sim.refuse = false; break;
        case 1: nvs_poke = true; nvs_val = v; break;
        case 2: g_store_sim.fail_get = true; break;
        case 3: g_store_sim.fail = true; break;
        default: g_store_sim.refuse = true; break;
        }
        break;
    case O_RAW: {
        uint8_t o[4] = {(uint8_t)v, (uint8_t)(v >> 8), (uint8_t)(v >> 16), (uint8_t)(v >> 24)};
        fou_inject(&F, o, a % 4 + 1, b & 1, 0);
        break;
    }
#ifdef FZ_HOTE
    case O_LIGNE: {
        uint8_t o[6] = {(uint8_t)b, (uint8_t)(b >> 8), (uint8_t)v, (uint8_t)(v >> 8), (uint8_t)(v >> 16), (uint8_t)(v >> 24)};
        recevoir(o, a > 6 ? 6 : a);
        break;
    }
    case O_MAGASIN:
        mag_tenu = a & 1;
        mag_lat = v;
        g_store_sim.hold = mag_tenu;
        if (!mag_tenu) {                            /* rendues tout de suite : celles qui attendaient aussi */
            store_sim_flush();
            mag_due = 0;
        }
        break;
#endif
#ifdef FZ_COMMANDE
    case O_CMDX: {
        B.cmdx++;
        if (b > CMD_LENS_CUSTOM) B.cmdx_hors++;
        deposer((bsk_cmd_op_t)b, (int32_t)v, false, 0);
        break;
    }
#endif
    default: break;
    }
}

/* ─────────────── le cas ─────────────── */

static bool fini_fin(void)
{
    bsk_status_t s;
    bsk_session_status(&s);
    return s.session_state == SESSION_READY || s.session_state == SESSION_FAULT || s.held;
}

/* Le cas fabrique-t-il l'offset 64 du 0x05 autrement que par le bouton (C_APPUI) : un champ 65 muté, un 0x05 allongé
 * jusqu'à lui, doublé, échangé, rangé pour être rejoué, ou une injection ? */
static bool bouton_faux(const uint8_t *d, size_t n)
{
    for (size_t i = HDR; i + EV <= n; i += EV) {
        uint8_t op = d[i + 4] % O_N, a = d[i + 5] % FOU_N, type = d[i + 6];
        uint32_t v = (uint32_t)d[i + 8] | (uint32_t)d[i + 9] << 8 | (uint32_t)d[i + 10] << 16 | (uint32_t)d[i + 11] << 24;
        if (op == O_INJ) return true;
        if (op != O_RULE || (type && type != 0x05)) continue;
        if (a == FOU_DOUBLE || a == FOU_ECHANGE || a == FOU_RANGE) return true;
        if (a == FOU_CHAMP && v >> 16 == 65) return true;
        if (a == FOU_LONGUEUR && (v & 0xFF) > 65) return true;
    }
    return false;
}

/* Les opérations où seul le moment est fou : C_CIBLE n'est jugée que là (plans/fuzzing.md § 8, #744). */
static bool moment(const uint8_t *d, size_t n)
{
    if (d[0] & 1) return false;
    for (size_t i = HDR; i + EV <= n; i += EV) {
        uint8_t op = d[i + 4] % O_N, a = d[i + 5];
        if (op == O_RULE && a % FOU_N != FOU_RETARD) return false;
        if (op != O_WAIT && op != O_CMD && op != O_LATE && op != O_RULE && op != O_CLEAR && op != O_D2) return false;
        if (op == O_D2 && (d[i + 5] & 1) == 0) return false;
    }
    return true;
}

static void raz(void)
{
    memset(&D, 0, sizeof D);
    memset(CMDS, 0, sizeof CMDS);
    memset(&DG, 0, sizeof DG);
    memset(&DB, 0, sizeof DB);
    memset(&DR, 0, sizeof DR);
    memset(FATES, 0, sizeof FATES);
    n_fates = 0;
    etat = SESSION_OFF;
    depuis = T0;
    retard_cumul = retard_entree = 0;
    in_cmd = -1;
    cuts_vus = cuts_obs = 0;
    t_homing = t_logic = t_motor = t_cs_low = t_cs_high = t_lcs = 0;
    cs_high = false;
    sig = false;
    decl = BSK_BENCH_OTHER;
    polls = 0;
    cut_cmd = false;
    n_cr = 0;
    nvs_poke = false;
    nvs_val = 0;
    marque_src = 0;
    marque_v = 0;
    memset(&requete, 0, sizeof requete);
    desync = custom_emis = custom_repondu = false;
    cov = 0;
    seq_n = 0;
    board_vol = 0;
    ack_n = 0;
    late_n = 0;
    late_us = 0;
    sets_avant = 0;
    rail_avant[0] = rail_avant[1] = false;
    id_avant = 0;
    lens_on = false;
    ev_courant = NULL;
    refus0 = bsk_bench_refused();
    dep_actif = dep_vu = false;
    dep_seq = 0;
#ifdef FZ_HOTE
    memset(&U, 0, sizeof U);
    memset(&LC, 0, sizeof LC);
    memset(&M, 0, sizeof M);
    ligne_active = false;
    mag_tenu = false;
    mag_lat = mag_due = 0;
#endif
}

void fz_run(fz_ctx_t *c, const uint8_t *d, size_t n)
{
    uint64_t t_prevu;
    bool juger_fin;
    C = c;
    raz();
    std_model = n > 0 && (d[0] & 1);
    journal_mode = n > 1 ? d[1] % 3 : 0;
    moment_seul = n >= HDR && moment(d, n);
    DR.faux = n >= HDR && bouton_faux(d, n);
    B.cas++;
    if (n > 2) B.par_prefixe[d[2] % N_PREFIXES]++;
    if (moment_seul) B.moment_seul++;
    if (std_model) {
        lstd_params_default(&LPSTD);
        lstd_init(&LSTD, &LPSTD);
        fou_init(&F, lens_sim_std(&LSTD));
    } else {
        l135_params_default(&LP135);
        l135_init(&L135, &LP135);
        fou_init(&F, lens_sim_135(&L135));
    }
    phy_sim_init(fou(), 0);
    phy_sim_fault(sort_trame);
    bsk_session_params_default(&P);
    store_sim_reset();
    bsk_journal_init(phy_sim_now);
    bsk_journal_set(journal_mode != 0, journal_mode == 2);
    bsk_session_init(&P);
#ifdef FZ_HOTE
    bsk_host_init(HOTE_VERSION, "poweron", phy_sim_now);   /* sim/test/test_host135.c, comme la carte (host_usb.c) */
    bsk_led_init(phy_sim_now());
    M.journal = journal_mode;                       /* l'en-tête du cas : comme une ligne LOG avant tout */
    M.plafond = 100;
#endif
    fz_tr(c, "modèle %s, journal %u, préfixe %s, moment seul %d", std_model ? "standard" : "135", journal_mode,
          n > 2 ? PREFIXE[d[2] % N_PREFIXES] : "-", moment_seul);
    courir(T0, NULL);
    t_prevu = T0;
    for (size_t i = HDR; i + EV <= n && c->check < 0; i += EV) {
        const uint8_t *e = d + i;
        t_prevu += le32(e);
        courir(t_prevu, NULL);
        if (c->check >= 0) break;
        fz_tr(c, "t=%llu op %u a %u b %u v %lu", (unsigned long long)phy_sim_now(), e[4] % O_N, e[5],
              (unsigned)(e[6] | e[7] << 8), (unsigned long)le32(e + 8));
        jouer(e[4], e[5], (uint16_t)(e[6] | e[7] << 8), le32(e + 8));
        lire_accuses();
    }
    if (c->check >= 0) goto bilan;
    /* le chaos fini : l'objectif nominal, D2 présent, l'objectif alimenté */
    for (unsigned k = 0; k < FOU_N; k++) B.actions[k] += F.applied[k];
    B.injections += F.injected;
    memset(F.applied, 0, sizeof F.applied);
    F.injected = 0;
    fou_clear(&F);
    n_fates = 0;
    late_n = 0;
    g_store_sim.fail = g_store_sim.fail_get = g_store_sim.refuse = false;
#ifdef FZ_HOTE
    g_store_sim.hold = mag_tenu = false;            /* le magasin rend tout ce qui attendait */
    store_sim_flush();
    mag_due = 0;
#endif
    jouer(O_BTN, 0, 0, 0);                          /* le bouton relâché, comme le doigt (DR) */
    if (!D.pin) {
        phy_sim_d2(true);
        d2_set(phy_sim_now(), true);
    }
    alimenter(true);
    courir(phy_sim_now() + 120u * S, fini_fin);
    if (c->check >= 0) goto bilan;
    {
        bsk_status_t s;
        bsk_session_status(&s);
        juger_fin = !s.held && !lens_sim_out_of_model(fou()) && !lens_sim_blocked(fou());
        if (juger_fin) B.fin_jugee++;
        else B.fin_sautee++;
        fz_tr(c, "fin : %s%s", bsk_state_name(s.session_state), juger_fin ? "" : " (non jugée : tenu, hors modèle ou bloqué)");
        if (juger_fin && s.session_state != SESSION_READY && s.session_state != SESSION_FAULT)
            fz_fail(c, C_FIN, "120 s après le chaos : %s", bsk_state_name(s.session_state));
    }
    courir(phy_sim_now() + 15u * S, NULL);
    if (c->check < 0 && juger_fin) {
        for (uint32_t k = 1; k <= seq_n && k < SEQ_MAX; k++)
            if (CMDS[k].etat == A_VOL) {
                fz_fail(c, C_ACKFIN, "seq %lu (op %u) sans accusé final, 15 s après la fin", (unsigned long)k, CMDS[k].op);
                break;
            }
        if (board_vol) fz_fail(c, C_ACKFIN, "un goto du bouton sans accusé final, 15 s après la fin");
    }
#ifdef FZ_HOTE
    if (c->check < 0 && (U.vol || U.later))         /* chaque ligne a sa réponse, objectif nominal ou non */
        fz_fail(c, C_LATER, "« %s » sans réponse, 135 s après le chaos", hote_vis(U.ligne));
#endif
bilan:
    for (unsigned k = 0; k < FOU_N; k++) B.actions[k] += F.applied[k];
    B.injections += F.injected;
    for (unsigned k = 0; k < COV_N; k++)
        if (cov & 1u << k) B.couverture[k]++;
    fz_tr(c, "atteint :%s%s%s%s%s%s%s", cov & 1u << COV_READY ? " ready" : "", cov & 1u << COV_GOTO ? " goto" : "",
          cov & 1u << COV_RESTORING ? " restoring" : "", cov & 1u << COV_RECOVERING ? " recovering" : "",
          cov & 1u << COV_FAULT ? " fault" : "", cov & 1u << COV_MARQUE ? " marque" : "", cov & 1u << COV_CUSTOM ? " custom" : "");
}

/* Le bilan d'une campagne (fuzz.h, fz_bilan) : la couverture du critère 3 et ce que les contrats ont vu. */
void fz_bilan(void)
{
    static const char *const ACT[FOU_N] = {"retard", "perte", "double", "echange", "champ", "longueur", "classe", "brut",
                                           "bout", "tardif", "range"};
    printf("COUVERTURE %llu cas :", (unsigned long long)B.cas);
    for (unsigned k = 0; k < COV_N; k++) printf(" %s %llu ;", COV_NOM[k], (unsigned long long)B.couverture[k]);
    printf("\nPREFIXES");
    for (unsigned k = 0; k < N_PREFIXES; k++) printf(" %s %llu ;", PREFIXE[k], (unsigned long long)B.par_prefixe[k]);
    printf("\nFIN jugée %llu, sautée %llu (tenu par p0, objectif hors modèle ou bloqué) ; moment seul %llu cas\n",
           (unsigned long long)B.fin_jugee, (unsigned long long)B.fin_sautee, (unsigned long long)B.moment_seul);
    printf("CONTRATS bsk_txn_held_end : %llu appels, place max %llu ; session_ack_last : %llu appels, %llu sur file vide\n",
           (unsigned long long)B.held_end, (unsigned long long)B.held_end_max, (unsigned long long)B.ack_last,
           (unsigned long long)B.ack_last_vide);
    printf("BOUTON écritures hors js/jx : %llu, dont %llu jugées sur l'appui réel ; appuis coupés écrits : %llu\n",
           (unsigned long long)B.ecritures_bouton, (unsigned long long)B.appuis_juges, (unsigned long long)B.appuis_coupes);
    printf("FOU émissions transformées :");
    for (unsigned k = 0; k < FOU_N; k++) printf(" %s %llu ;", ACT[k], (unsigned long long)B.actions[k]);
    printf(" injectées %llu ; pas en retard %llu\n", (unsigned long long)B.injections, (unsigned long long)B.retards);
#ifdef FZ_HOTE
    {
        static const char *const ETAT[8] = {"off", "powering", "identifying", "homing", "restoring", "ready", "recovering",
                                            "fault"};
        uint64_t tot = 0;
        for (unsigned k = 0; k < F_N; k++) tot += B.lignes_fam[k];
        printf("LIGNES %llu servies, %llu octets reçus par l'USB ; par famille :", (unsigned long long)tot,
               (unsigned long long)B.octets_usb);
        for (unsigned k = 0; k < F_N; k++)
            if (B.lignes_fam[k]) printf(" %s %llu ;", HOTE_FAM[k], (unsigned long long)B.lignes_fam[k]);
        printf("\nLIGNES par état de la session :");
        for (unsigned k = 0; k < 8; k++) printf(" %s %llu ;", ETAT[k], (unsigned long long)B.lignes_etat[k]);
        printf("\nB4 commandes que HOTE admet elle-même (f…, m<n>, a…, js, jx, jg, CUSTOM) : %llu hors READY, %llu en READY\n",
               (unsigned long long)B.b4[0], (unsigned long long)B.b4[1]);
        printf("DIFFEREES %llu réponses différées, la plus longue %llu µs (retards du pas retirés)\n",
               (unsigned long long)B.differees, (unsigned long long)B.differee_max_us);
    }
#endif
#ifdef FZ_COMMANDE
    printf("COMMANDES bsk_cmd_t arbitraires : %llu, dont %llu hors de l'énumération\n", (unsigned long long)B.cmdx,
           (unsigned long long)B.cmdx_hors);
#endif
    if (B.sans_accuse) printf("ACCUSE %llu dépôts sans accusé dans l'appel\n", (unsigned long long)B.sans_accuse);
}

int fz_cmd(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return -1;
}

/* ─────────────────────────── le générateur ─────────────────────────── */

static size_t pose(uint8_t *out, size_t n, size_t cap, uint32_t dt, uint8_t op, uint8_t a, uint16_t b, uint32_t v)
{
    if (n + EV > cap) return n;
    out[n] = (uint8_t)dt;
    out[n + 1] = (uint8_t)(dt >> 8);
    out[n + 2] = (uint8_t)(dt >> 16);
    out[n + 3] = (uint8_t)(dt >> 24);
    out[n + 4] = op;
    out[n + 5] = a;
    out[n + 6] = (uint8_t)b;
    out[n + 7] = (uint8_t)(b >> 8);
    out[n + 8] = (uint8_t)v;
    out[n + 9] = (uint8_t)(v >> 8);
    out[n + 10] = (uint8_t)(v >> 16);
    out[n + 11] = (uint8_t)(v >> 24);
    return n + EV;
}

/* Les préfixes, tirés des suites (test_session135.c, test_stop_goto135.c, test_session_script.c) : l'objectif monté et
 * READY en 4 s (boot de test_stop_goto135), un goto en vol, une marque posée par js, une perte (l'objectif coupé), un
 * CUSTOM en vol, un retour à la marque (js, ailleurs, b), un arrêt suivi d'un goto (#787), une faute (10 01 au 0x10), le
 * homing du pilote (l'init arrêtée avant son 0x0A : les trois réponses au 0x0B perdues, [D61], [D75]). */
static size_t prefixe(uint8_t p, uint8_t *out, size_t n, size_t cap)
{
    if (p == 8) n = pose(out, n, cap, 0, O_RULE, FOU_CHAMP, 0x10 | 1u << 8, 1u << 16 | 0x01);
    if (p == 9) {                                                  /* le 135 resté alimenté en flux, D2 seul */
        n = pose(out, n, cap, 0, O_RULE, FOU_PERTE, 0x0B | 3u << 8, 0);
        n = pose(out, n, cap, 0, O_ALIM, 1, 0, 0);
        n = pose(out, n, cap, 0, O_D2, 1, 0, 0);
        return pose(out, n, cap, 2500 * MS, O_WAIT, 0, 0, 0);
    }
    n = pose(out, n, cap, 0, O_D2, 1, 1, 0);
    if (p == 0) return n;
    if (p == 8) return pose(out, n, cap, 3 * S, O_WAIT, 0, 0, 0);
    n = pose(out, n, cap, 4 * S, O_WAIT, 0, 0, 0);
    switch (p) {
    case 2: n = pose(out, n, cap, 0, O_CMD, K_GOTO_SEL, 7, 0); break;
    case 3: n = pose(out, n, cap, 0, O_CMD, K_JS, 0, 0); n = pose(out, n, cap, 50 * MS, O_WAIT, 0, 0, 0); break;
    case 4: n = pose(out, n, cap, 0, O_POWER, 0, 0, 0); n = pose(out, n, cap, 1 * S, O_WAIT, 0, 0, 0); break;
    case 5: n = pose(out, n, cap, 0, O_CMD, K_CUSTOM_R, 0, 0); break;
    case 6:
        n = pose(out, n, cap, 0, O_CMD, K_GOTO_SEL, 4, 0);
        n = pose(out, n, cap, 3 * S, O_CMD, K_JS, 0, 0);
        n = pose(out, n, cap, 50 * MS, O_CMD, K_GOTO_SEL, 7, 0);
        n = pose(out, n, cap, 3 * S, O_CMD, K_B, 0, 0);
        n = pose(out, n, cap, 1500 * MS, O_WAIT, 0, 0, 0);
        break;
    case 7:
        n = pose(out, n, cap, 0, O_CMD, K_GOTO_SEL, 7, 0);
        n = pose(out, n, cap, 300 * MS, O_CMD, K_STOP, 0, 0);
        n = pose(out, n, cap, 0, O_CMD, K_GOTO_SEL, 4, 0);
        break;
    default: break;
    }
    return n;
}

/* Les types que les règles visent : ceux des réponses du 135 et du standard, et toute émission (0). */
static const uint8_t TYPES[] = {0, 0, 0x05, 0x06, 0x05, 0x06, 0x01, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0D, 0x10, 0x3F, 0x40, 0x02};

/* Un champ aux bords, par type : offset (type compris) << 16 | valeur, xor si 0x100 (lens_fou.h, FOU_CHAMP). Les offsets
 * sont ceux que lens_rx.c, txn.c (answers) et cmd.c (cmd_custom_poll) lisent. */
static uint32_t champ(fz_rng_t *r, uint8_t type)
{
    static const uint8_t BORDS[] = {0x00, 0x01, 0x0D, 0x0E, 0x1F, 0x20, 0x21, 0x7F, 0x80, 0xFE, 0xFF, 0x08, 0x1C, 0x1D, '"'};
    uint32_t off, val = fz_chance(r, 2, 3) ? BORDS[fz_below(r, sizeof BORDS)] : fz_below(r, 256);
    switch (type) {
    case 0x05: {
        static const uint8_t O[] = {1, 2, 18, 19, 20, 27, 28, 61, 63, 65, 65, 65, 67};
        off = O[fz_below(r, sizeof O)];
        if (off == 65 && fz_chance(r, 1, 2)) return 65u << 16 | 0x100 | 0x08;   /* le bouton, basculé */
        break;
    }
    case 0x06: {
        static const uint8_t O[] = {3, 4, 8, 9, 10, 11, 40, 41, 42, 43};
        off = O[fz_below(r, sizeof O)];
        break;
    }
    case 0x07: off = fz_chance(r, 3, 4) ? 10 + fz_below(r, 2) : fz_below(r, 12); if (off == 10 && fz_chance(r, 1, 2)) val = 8; break;
    case 0x08: off = 1 + fz_below(r, 8); break;
    case 0x10: off = 1; val = fz_below(r, 3); break;
    case 0x3F: off = 2 + fz_below(r, 12); break;
    case 0x40: {
        static const uint8_t M[] = {'W', 'H', 'L', 'P', 'F', 'V', 'M'};
        off = 1 + fz_below(r, 3);
        if (off == 1) val = M[fz_below(r, sizeof M)];
        break;
    }
    default: off = fz_below(r, 24); break;
    }
    return off << 16 | val;
}

/* Une longueur aux bords des minimums de lens_rx.c (MIN05 29, MIN06 12, have62 64, have64 66, have66 68 ; 0x08 MIN08 ;
 * 0x07 4 ; 0x10 2 ; 0x40 19, cmd_custom_poll), complétée d'un octet. */
static uint32_t longueur(fz_rng_t *r, uint8_t type)
{
    static const uint8_t FILL[] = {0x00, 0xFF, 0x08, 0x1D};
    uint32_t len;
    switch (type) {
    case 0x05: { static const uint8_t L[] = {28, 29, 30, 63, 64, 65, 66, 67, 68, 69}; len = L[fz_below(r, sizeof L)]; break; }
    case 0x06: { static const uint8_t L[] = {11, 12, 13, 39, 40, 41, 42, 43, 44}; len = L[fz_below(r, sizeof L)]; break; }
    case 0x07: len = 2 + fz_below(r, 12); break;
    case 0x08: len = 2 + fz_below(r, 9); break;
    case 0x10: len = 1 + fz_below(r, 2); break;
    case 0x40: len = 17 + fz_below(r, 4); break;
    default: len = 1 + fz_below(r, 80); break;
    }
    return FILL[fz_below(r, sizeof FILL)] << 8 | len;
}

/* Une valeur relue de la NVS (cible 5) : la forme de mark.c (position, sens), ou ses bits 24-31 non nuls, ou quelconque. */
static uint32_t valeur_nvs(fz_rng_t *r)
{
    static const uint16_t POS[] = {0, 1, 5, 14000, 20000, 30000, 65530, 65535};
    static const uint8_t DIR[] = {0, 1, 2, 3, 0xFF};
    static const uint8_t HAUT[] = {0, 0, 0, 0, 0, 0, 1, 0x80, 0xFF};
    if (fz_chance(r, 1, 8)) return (uint32_t)fz_next(r);
    return (uint32_t)(fz_chance(r, 1, 2) ? POS[fz_below(r, sizeof POS / sizeof POS[0])] : fz_below(r, 65536)) |
           (uint32_t)DIR[fz_below(r, sizeof DIR)] << 16 | (uint32_t)HAUT[fz_below(r, sizeof HAUT)] << 24;
}

static uint32_t retard(fz_rng_t *r)
{
    static const uint32_t V[] = {100, 1000, 2000, 2100, 16700, 20000, 21000, 100000, 299000, 301000, 399000, 401000,
                                 501000, 999000, 1001000, 2001000, 8001000, 20001000};
    return fz_chance(r, 2, 3) ? V[fz_below(r, sizeof V / sizeof V[0])] : fz_below(r, 3 * S);
}

static uint32_t retard_pas(fz_rng_t *r)
{
    static const uint32_t V[] = {1000, 2000, 2001, 3000, 10000, 16000, 100000, 300000, 2001000, 5000000, 9999999};
    return fz_chance(r, 3, 4) ? V[fz_below(r, sizeof V / sizeof V[0])] : fz_below(r, 10u * S);
}

static uint32_t dt_chaos(fz_rng_t *r)
{
    uint32_t q = fz_below(r, 10);
    if (q < 3) return 0;
    if (q < 6) return fz_below(r, 20 * MS);
    if (q < 9) return fz_below(r, 500 * MS);
    return fz_below(r, 5 * S);
}

/* Un événement de chaos. L'ordre des poids suit l'ordre du brief (§ 8, « si ça coince ») : le moment et le retard du
 * pas, l'ordre, le contenu, D2 et LENS_CS, le type annoncé, le bus. `seul_moment` : rien que le moment. */
static size_t chaos(fz_rng_t *r, uint8_t *out, size_t n, size_t cap, bool seul_moment)
{
    uint32_t dt = dt_chaos(r), q = fz_below(r, 100);
    uint8_t type = TYPES[fz_below(r, sizeof TYPES)];
    uint16_t nb = (uint16_t)(fz_chance(r, 1, 4) ? 0 : 1 + fz_below(r, 3));
    if (seul_moment) q = q % 3 == 0 ? 0 : q % 3 == 1 ? 12 : 36;
    if (q < 12) return pose(out, n, cap, dt, O_RULE, FOU_RETARD, (uint16_t)(type | nb << 8), retard(r));
    if (q < 22) return pose(out, n, cap, dt, O_LATE, (uint8_t)(1 + fz_below(r, fz_chance(r, 1, 2) ? 3 : 60)), 0, retard_pas(r));
    if (q < 25) return pose(out, n, cap, dt, O_RULE, FOU_TARDIF, (uint16_t)(type | nb << 8), 1 + fz_below(r, 4));
    if (q < 28) return pose(out, n, cap, dt, O_RULE, FOU_DOUBLE, (uint16_t)(type | nb << 8), fz_below(r, 30 * MS));
    if (q < 31) return pose(out, n, cap, dt, O_RULE, FOU_ECHANGE, (uint16_t)(type | 1u << 8), 0);
    if (q < 34) return pose(out, n, cap, dt, O_RULE, FOU_PERTE, (uint16_t)(type | nb << 8), 0);
    if (q < 36) return pose(out, n, cap, dt, O_RULE, FOU_RANGE, (uint16_t)type, 0);
    if (q < 44) return pose(out, n, cap, dt, O_CMD, (uint8_t)fz_below(r, K_N), (uint16_t)fz_below(r, 8), (uint32_t)fz_next(r));
    if (q < 47) return pose(out, n, cap, dt, O_INJ, 0, (uint16_t)fz_below(r, FOU_MEM), 0);
    if (q < 52) return pose(out, n, cap, dt, O_INJ, 1, (uint16_t)fz_below(r, N_GABARITS), 0);
    if (q < 60) return pose(out, n, cap, dt, O_RULE, FOU_CHAMP, (uint16_t)(type | nb << 8), champ(r, type));
    if (q < 65) return pose(out, n, cap, dt, O_RULE, FOU_LONGUEUR, (uint16_t)(type | nb << 8), longueur(r, type));
    if (q < 66) return pose(out, n, cap, dt, O_RULE, FOU_CLASSE, (uint16_t)(type | nb << 8), fz_below(r, 5));
    if (q < 68) return pose(out, n, cap, dt, O_CLEAR, 0, 0, 0);
    if (q < 74) {                                                  /* D2 : un rebond sous 2 ms, une retombée */
        uint32_t w = fz_chance(r, 1, 2) ? fz_below(r, 2 * MS) : fz_chance(r, 1, 2) ? 2 * MS + fz_below(r, 400 * MS)
                                                                                    : 295 * MS + fz_below(r, 10 * MS);
        bool pw = fz_chance(r, 1, 3);
        n = pose(out, n, cap, dt, O_D2, 0, pw, 0);
        return pose(out, n, cap, w, O_D2, 1, pw, 0);
    }
    if (q < 78) return pose(out, n, cap, dt, O_LCS, (uint8_t)fz_below(r, 3), 0, 0);
    if (q < 79) return pose(out, n, cap, dt, O_POWER, (uint8_t)fz_below(r, 2), 0, 0);
    if (q < 80) return pose(out, n, cap, dt, O_ALIM, (uint8_t)fz_below(r, 4), 0, 0);
    if (q < 83) {                                                  /* un appui du bouton (P8) */
        static const uint32_t D[] = {300000, 999000, 1001000, 1500000, 2500000, 3999000, 4001000};
        uint32_t duree = fz_chance(r, 3, 4) ? D[fz_below(r, sizeof D / sizeof D[0])] : fz_below(r, 5 * S);
        uint32_t k = fz_below(r, 4);
        n = pose(out, n, cap, dt, O_BTN, 1, 0, 0);
        if (k == 1)                                                /* des 0x05 courts pendant l'appui */
            n = pose(out, n, cap, fz_below(r, duree / 2 + 1), O_RULE, FOU_LONGUEUR,
                     (uint16_t)(0x05 | (1 + fz_below(r, 60)) << 8), 29 + fz_below(r, 37));
        else if (k == 2)                                           /* le 0x05 perdu pendant l'appui, le 0x06 courant */
            n = pose(out, n, cap, fz_below(r, duree / 2 + 1), O_RULE, FOU_PERTE, (uint16_t)(0x05 | (100 + fz_below(r, 100)) << 8), 0);
        return pose(out, n, cap, k ? duree / 2 : duree, O_BTN, 0, 0, 0);
    }
    if (q < 85) return pose(out, n, cap, dt, O_RAILF, (uint8_t)fz_below(r, 2), (uint16_t)fz_below(r, 2), 0);
    if (q < 88) return pose(out, n, cap, dt, O_RULE, FOU_BRUT, (uint16_t)(type | nb << 8), (uint32_t)fz_next(r));
    if (q < 90) return pose(out, n, cap, dt, O_RULE, FOU_BOUT, (uint16_t)(type | nb << 8), 1 + fz_below(r, 12));
    if (q < 93) return pose(out, n, cap, dt, O_FATE, (uint8_t)fz_below(r, 2), (uint16_t)(type | (1 + fz_below(r, 2)) << 8),
                            1 + fz_below(r, 2));
    if (q < 94) return pose(out, n, cap, dt, O_BUSRX, 0, 0, 0);
    if (q < 95) return pose(out, n, cap, dt, O_NVS, (uint8_t)fz_below(r, 5), 0, valeur_nvs(r));
    if (q < 97) return pose(out, n, cap, dt, O_BUSTX, 0, 0, 0);
    {
        uint32_t v = (uint32_t)fz_next(r);
        if (fz_chance(r, 1, 2)) v = (v & ~0xFFu) | 0xF0;           /* un début de trame */
        return pose(out, n, cap, dt, O_RAW, (uint8_t)fz_below(r, 4), (uint16_t)fz_below(r, 2), v);
    }
}

#if defined(FZ_HOTE)
/* Des octets reçus par l'USB, en événements O_LIGNE de 6 octets au plus : le premier à `dt`, les suivants aussitôt, ou
 * parfois quelques millisecondes plus tard (des octets arrivés en plusieurs paquets). */
static size_t octets(fz_rng_t *r, uint8_t *out, size_t n, size_t cap, uint32_t dt, const uint8_t *o, size_t k)
{
    for (size_t i = 0; i < k; i += 6) {
        uint8_t c[6] = {0};
        size_t m = k - i < 6 ? k - i : 6;
        memcpy(c, o + i, m);
        n = pose(out, n, cap, i ? (fz_chance(r, 1, 8) ? fz_below(r, 3 * MS) : 0) : dt, O_LIGNE, (uint8_t)m,
                 (uint16_t)(c[0] | c[1] << 8), le32(c + 2));
    }
    return n;
}

static size_t ligne(fz_rng_t *r, uint8_t *out, size_t n, size_t cap, uint32_t dt)
{
    uint8_t o[320];
    return octets(r, out, n, cap, dt, o, hote_octets(r, o, sizeof o));
}

/* La latence du magasin qui retient ses opérations : autour de BSK_STORE_WRITE_MS (2 s), l'échéance de js et jx. */
static uint32_t latence(fz_rng_t *r)
{
    static const uint32_t L[] = {0, 1000, 20000, 500000, 1999000, 2000000, 2001000, 3000000};
    return fz_chance(r, 3, 4) ? L[fz_below(r, sizeof L / sizeof L[0])] : fz_below(r, 3 * S);
}

/* Un cas de l'hôte fou : l'en-tête et les préfixes de F2 (leurs commandes deviennent leurs lignes), puis des lignes de
 * la grammaire et des octets arbitraires (hote.c) mêlés au chaos de F2 ; des rafales : une ligne qui attend sa réponse
 * (CUSTOM, q, js, jx), suivie aussitôt d'autres, que le cadrage retient jusqu'à elle ; et le sondage d'un pilote, des
 * lectures à 5 Hz (PROTOCOL.md § 1 règle 6 : « l'app lit f/e à 5 Hz »), pendant que l'objectif fait n'importe quoi. Le
 * journal part éteint deux fois sur trois, comme la carte : LOG l'allume. */
size_t fz_gen(fz_rng_t *r, uint8_t *out, size_t cap)
{
    static const char *const DIF[] = {"CUSTOM READ\n", "CUSTOM WRITE 10\n", "CUSTOM WRITE 21\n", "q\n", "js\n", "jx\n"};
    static const char *const SONDE[] = {"f\n", "e\n", "f\n", "e\n", "r\n", "o\n", "a\n", "j\n", "i\n", "t\n", "g\n", ":GP#",
                                        ":GI#", "c\n", "d\n"};
    uint8_t p = (uint8_t)fz_below(r, N_PREFIXES);
    size_t n = HDR;
    uint32_t k = 1 + fz_below(r, fz_chance(r, 1, 2) ? 12 : 60);
    memset(out, 0, HDR);
    out[0] = (uint8_t)(fz_chance(r, 1, 4) ? 1 : 0);
    if (p == 5 || p == 7 || p == 9) out[0] = 0;
    out[1] = (uint8_t)(fz_chance(r, 2, 3) ? 0 : 1 + fz_below(r, 2));
    out[2] = p;
    if (fz_chance(r, 1, 2)) n = pose(out, n, cap, 0, O_MAGASIN, 1, 0, latence(r));
    if (fz_chance(r, 1, 3))
        n = pose(out, n, cap, 0, O_NVS, (uint8_t)(fz_chance(r, 3, 4) ? 1 : 2 + fz_below(r, 3)), 0, valeur_nvs(r));
    n = prefixe(p, out, n, cap);
    for (uint32_t i = 0; i < k; i++) {
        uint32_t q = fz_below(r, 100);
        if (q < 40) {
            n = ligne(r, out, n, cap, dt_chaos(r));
        } else if (q < 50) {
            const char *d = DIF[fz_below(r, sizeof DIF / sizeof DIF[0])];
            uint32_t j = 1 + fz_below(r, 4);
            n = octets(r, out, n, cap, dt_chaos(r), (const uint8_t *)d, strlen(d));
            while (j--) n = ligne(r, out, n, cap, fz_chance(r, 1, 2) ? 0 : fz_below(r, 5 * MS));
        } else if (q < 53) {
            n = pose(out, n, cap, dt_chaos(r), O_MAGASIN, (uint8_t)fz_below(r, 2), 0, latence(r));
        } else if (q < 63) {
            uint32_t j = 3 + fz_below(r, 20), pas_us = fz_chance(r, 3, 4) ? 200 * MS : fz_below(r, 300 * MS);
            for (uint32_t m = 0; m < j; m++) {
                const char *d = SONDE[fz_below(r, sizeof SONDE / sizeof SONDE[0])];
                n = octets(r, out, n, cap, m ? pas_us : dt_chaos(r), (const uint8_t *)d, strlen(d));
            }
        } else {
            n = chaos(r, out, n, cap, false);
        }
    }
    return n;
}
#elif defined(FZ_COMMANDE)
/* Un bsk_cmd_t arbitraire : op de l'énumération, ou hors d'elle une fois sur quarante ; arg aux bornes (positions, l'octet
 * 0x30 + arg de CUSTOM, les extrêmes d'un int32_t) ou quelconque. CUSTOM a un arg de sa ligne une fois sur deux ; son
 * arg au-delà de INT32_MAX − 0x30 (où 0x30 + arg déborde un int, components/session/cmd.c, custom_cmd) est gardé une fois
 * sur deux. Une trouvaille de P1 arrête le processus : la campagne se joue par lots (RAPPORT.txt, F3). */
static size_t cmdx(fz_rng_t *r, uint8_t *out, size_t n, size_t cap, uint32_t dt)
{
    static const uint16_t HORS[] = {12, 13, 15, 16, 255, 256, 0x7FFF, 0xFFFF};
    static const int32_t ARG[] = {INT32_MIN, INT32_MIN + 1, -65536, -65535, -2, -1, 0, 1, 2, 0x10, 0x12, 0x20, 0x21, 0x22,
                                  0x23, 0x2F, 0x30, 0x40, 0x7F, 0x80, 0xCF, 0xD0, 0xFF, 0x100, 0x110, 0x122, 13723, 13877,
                                  13878, 16384, 20000, 30983, 30984, 65535, 65536, INT32_MAX - 0x30, INT32_MAX - 0x2F,
                                  INT32_MAX};
    uint16_t op = fz_chance(r, 1, 40) ? HORS[fz_below(r, sizeof HORS / sizeof HORS[0])] : (uint16_t)fz_below(r, CMD_LENS_CUSTOM + 1);
    int32_t arg = fz_chance(r, 3, 4) ? ARG[fz_below(r, sizeof ARG / sizeof ARG[0])] : (int32_t)fz_next(r);
    if (op == CMD_LENS_CUSTOM) {
        if (fz_chance(r, 1, 2)) arg = fz_chance(r, 1, 3) ? -1 : (int32_t)(fz_below(r, 3) << 4 | fz_below(r, 3));
        else if (arg > INT32_MAX - 0x30 && fz_chance(r, 1, 2)) arg = 0x110;
    }
    return pose(out, n, cap, dt, O_CMDX, 0, op, (uint32_t)arg);
}

/* Un cas du harnais secondaire : un cas de F2, des bsk_cmd_t arbitraires mêlés à son chaos. */
size_t fz_gen(fz_rng_t *r, uint8_t *out, size_t cap)
{
    uint8_t p = (uint8_t)fz_below(r, N_PREFIXES);
    size_t n = HDR;
    uint32_t k = 1 + fz_below(r, fz_chance(r, 1, 2) ? 8 : 40);
    memset(out, 0, HDR);
    out[0] = (uint8_t)(fz_chance(r, 1, 4) ? 1 : 0);
    if (p == 5 || p == 7 || p == 9) out[0] = 0;
    out[1] = (uint8_t)fz_below(r, 3);
    out[2] = p;
    n = prefixe(p, out, n, cap);
    for (uint32_t i = 0; i < k; i++)
        n = fz_chance(r, 1, 4) ? cmdx(r, out, n, cap, dt_chaos(r)) : chaos(r, out, n, cap, false);
    return n;
}
#else
size_t fz_gen(fz_rng_t *r, uint8_t *out, size_t cap)
{
    uint8_t p = (uint8_t)fz_below(r, N_PREFIXES);
    size_t n = HDR;
    uint32_t k = 1 + fz_below(r, fz_chance(r, 1, 2) ? 8 : 40);
    bool seul_moment;
    memset(out, 0, HDR);
    out[0] = (uint8_t)(fz_chance(r, 1, 4) ? 1 : 0);
    if (p == 5 || p == 7 || p == 9) out[0] = 0;                    /* CUSTOM, l'arrêt suivi d'un goto, homing : le 135 */
    out[1] = (uint8_t)fz_below(r, 3);
    out[2] = p;
    seul_moment = !out[0] && (p == 7 || fz_chance(r, 1, 8));
    if (!seul_moment && fz_chance(r, 1, 3))                        /* la NVS relue au démarrage (cible 5) */
        n = pose(out, n, cap, 0, O_NVS, (uint8_t)(fz_chance(r, 3, 4) ? 1 : 2 + fz_below(r, 3)), 0, valeur_nvs(r));
    n = prefixe(p, out, n, cap);
    for (uint32_t i = 0; i < k; i++) n = chaos(r, out, n, cap, seul_moment);
    return n;
}
#endif
