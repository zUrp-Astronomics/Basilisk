/* Le pilote commun du banc de fuzzing (README.md à côté) : un générateur à graine propre au banc, la campagne, le rejeu
 * d'un cas rangé, sa réduction. Chaque harnais (fuzz_<cible>.c) fournit sa cible, son générateur et ses vérifications ;
 * le pilote fournit main(), ou, compilé avec -DFZ_LIBFUZZER (F4), LLVMFuzzerTestOneInput, qui joue la même cible sous
 * les mêmes vérifications (fuzz.c).
 *
 * Un cas est une suite d'octets : l'entrée développée que la cible lit (des octets de flux, des opérations, une
 * mémoire), jamais une graine. Il se rejoue donc tel quel, même après un changement du générateur. Rien ici ne lit
 * l'horloge murale ni rand() : le générateur est le splitmix64 ci-dessous, le temps de chaque cible est virtuel. La
 * seule lecture de l'horloge murale est la mesure du débit d'une campagne, imprimée, jamais une entrée.
 *
 * Une cible est jugée par ses vérifications (FZ_CHECKS) : chacune nomme la propriété qu'elle tient (P1 à P11 de
 * plans/fuzzing.md § 3, l'atelier) et sa source. La première vérification qui échoue arrête le cas. Les erreurs des
 * sanitizers (P1) et la garde de durée (P2) arrêtent le processus : le cas en cours est alors écrit avant la sortie. */
#ifndef FUZZ_H
#define FUZZ_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

typedef struct {
    uint64_t s;
} fz_rng_t;

uint64_t fz_next(fz_rng_t *r);
uint32_t fz_below(fz_rng_t *r, uint32_t n);            /* dans 0..n-1, n ≥ 1 */
bool     fz_chance(fz_rng_t *r, uint32_t num, uint32_t den);
void     fz_seed(fz_rng_t *r, uint64_t graine, uint64_t i);

/* Une vérification : la propriété qu'elle tient, son nom (le lieu, pour la réduction) et sa source. */
typedef struct {
    const char *prop;
    const char *nom;
    const char *source;
} fz_check_t;

/* Le contexte d'un cas : la première vérification échouée (-1 : aucune) et son explication ; `trace` non nul au rejeu,
 * où la cible écrit ce qu'elle observe (fz_tr). */
typedef struct {
    int   check;
    char  why[400];
    FILE *trace;
} fz_ctx_t;

/* Ce que chaque harnais définit. */
extern const char       FZ_CIBLE[];
extern const fz_check_t FZ_CHECKS[];
extern const size_t     FZ_N_CHECKS;
extern const size_t     FZ_CAP;                         /* la taille maximale d'un cas */
size_t fz_gen(fz_rng_t *r, uint8_t *out, size_t cap);   /* un cas, rend sa longueur */
void   fz_run(fz_ctx_t *c, const uint8_t *d, size_t n); /* le cas joué ; tout l'état de la cible remis à zéro d'abord */
/* Une commande propre au harnais (une énumération), `argv[0]` son nom : rend le code de sortie, ou -1 si elle n'existe
 * pas. Un harnais sans commande rend -1. */
int    fz_cmd(int argc, char **argv);
/* Facultatif : ce qu'un harnais ajoute au bilan d'une campagne (fuzz_session : la couverture des états profonds). Un
 * harnais qui ne le définit pas n'imprime que le bilan du pilote. */
void   fz_bilan(void) __attribute__((weak));
/* Facultatif : un cas fait d'un en-tête de FZ_ENTETE octets puis d'événements de FZ_GRAIN octets, dont les FZ_NEUTRE
 * premiers sont l'attente (fuzz_session) ; la réduction retire alors des événements entiers, au plus FZ_ESSAIS essais.
 * Un harnais qui définit FZ_GRAIN définit les quatre. */
extern const size_t   FZ_ENTETE __attribute__((weak));
extern const size_t   FZ_GRAIN __attribute__((weak));
extern const size_t   FZ_NEUTRE __attribute__((weak));
extern const unsigned FZ_ESSAIS __attribute__((weak));

/* Échoue `check` (s'il n'a pas déjà échoué) : rend true, pour `if (cond && fz_fail(...)) return;`. */
bool fz_fail(fz_ctx_t *c, int check, const char *fmt, ...) __attribute__((format(printf, 3, 4)));
void fz_tr(fz_ctx_t *c, const char *fmt, ...) __attribute__((format(printf, 2, 3)));

/* Le cas en cours, écrit sous `rep` si le processus meurt (sanitizer, garde de durée), nommé P1-sanitizer-g<graine>-<i>.bin
 * ou P2-garde-g<graine>-<i>.bin, et la ligne qui le dit imprimée : la campagne le pose ; `rep` NULL, rien d'écrit. */
void fz_on_death(const char *rep, uint64_t graine, uint64_t i, const uint8_t *d, size_t n);

#endif
