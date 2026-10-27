/* Le pilote commun du banc de fuzzing (fuzz.h ; les commandes : README.md).
 *
 *   <harnais> campagne <graine> <n> <rép>   n cas tirés de la graine ; le premier échec de chaque vérification est
 *                                           réduit et rangé dans <rép>, nommé par sa propriété ; le bilan imprimé
 *   <harnais> rejoue <cas>                  le cas joué, ce que la cible observe imprimé, puis son verdict ; sort en 1
 *                                           s'il échoue
 *   <harnais> reduit <cas> <sortie>         le cas réduit tant que la même vérification échoue
 *   <harnais> ecrit <graine> <i> <sortie>   le i-ième cas de la graine, écrit sans être joué
 *   <harnais> premier <graine> <n> [<p> <v>]  les cas de la graine jusqu'au premier échec (de la vérification <p> <v>
 *                                           si elle est donnée), son verdict imprimé (morsures)
 *   <harnais> <commande propre> …           fz_cmd (une énumération)
 *
 * La réduction joue chaque essai dans un processus fils (fork) : une erreur d'un sanitizer (P1) ou la garde de durée
 * (P2) y tue l'essai sans tuer la réduction, et un essai retenu est celui dont le fils sort sur la même vérification.
 *
 * Compilé avec -DFZ_LIBFUZZER (F4, clang -fsanitize=fuzzer : fuzz.sh libfuzzer), le pilote ne fournit plus main ni les
 * commandes, que libFuzzer remplace : il fournit LLVMFuzzerTestOneInput, qui joue le cas par la même fz_run et le juge par
 * les mêmes FZ_CHECKS. Un échec connu (CONNUS, plus bas) est compté et passé ; tout autre échec imprime son verdict et
 * appelle abort(), que libFuzzer traite comme un plantage (le cas écrit, la campagne arrêtée). La garde de durée (P2) y
 * est l'option -timeout de libFuzzer, la mort par un sanitizer (P1) la sienne. */
#define _POSIX_C_SOURCE 200809L

#include "fuzz.h"

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#if defined(__SANITIZE_ADDRESS__)
#include <sanitizer/common_interface_defs.h>
#endif

/* La garde de durée d'un cas, en secondes de temps processeur du processus (P2 : chaque appel rend la main) ;
 * -DFZ_GARDE_S la change. Le temps processeur, pas l'horloge murale : un pod ou un runner chargé ralentit un cas sans
 * qu'il boucle, et le verdict d'un cas ne dépend pas de la charge de la machine. */
#ifndef FZ_GARDE_S
#define FZ_GARDE_S 10
#endif
#define TEXTE_(x) #x
#define TEXTE(x)  TEXTE_(x)

#define EXIT_GARDE 98
#define EXIT_SAN   99
#define EXIT_CHECK 10                                  /* + l'indice de la vérification */
#define ESSAIS_MAX 4000u                               /* les essais d'une réduction */

/* ─────────────────────────── le générateur : splitmix64 ─────────────────────────── */

uint64_t fz_next(fz_rng_t *r)
{
    uint64_t z = (r->s += 0x9E3779B97F4A7C15ull);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
}

uint32_t fz_below(fz_rng_t *r, uint32_t n) { return (uint32_t)(fz_next(r) % n); }

bool fz_chance(fz_rng_t *r, uint32_t num, uint32_t den) { return fz_below(r, den) < num; }

void fz_seed(fz_rng_t *r, uint64_t graine, uint64_t i)
{
    r->s = graine * 0xD1B54A32D192ED03ull ^ (i + 1) * 0x9E3779B97F4A7C15ull;
    (void)fz_next(r);
}

/* ─────────────────────────── les verdicts ─────────────────────────── */

bool fz_fail(fz_ctx_t *c, int check, const char *fmt, ...)
{
    va_list ap;
    if (c->check >= 0) return true;
    c->check = check;
    va_start(ap, fmt);
    vsnprintf(c->why, sizeof c->why, fmt, ap);
    va_end(ap);
    return true;
}

void fz_tr(fz_ctx_t *c, const char *fmt, ...)
{
    va_list ap;
    if (!c->trace) return;
    va_start(ap, fmt);
    vfprintf(c->trace, fmt, ap);
    va_end(ap);
    fputc('\n', c->trace);
}

#ifdef FZ_LIBFUZZER
/* ─────────────────────────── libFuzzer (F4) ─────────────────────────── */

/* Les échecs connus de ces cibles, comptés et passés : RAPPORT.txt (§ 4 de F1) et backlog.md de l'atelier, rubrique
 * « Fuzzing du firmware ». Une vérification qui n'est pas ici arrête la campagne. */
static const char *const CONNUS[][3] = {
    {"lens_rx", "P4", "avant_boucle"},   /* lens_rx_frame efface ack1d et ack1c avant de savoir si la trame est lue */
};
#define N_CONNUS (sizeof CONNUS / sizeof CONNUS[0])
static unsigned long long joues, connus[N_CONNUS];

static void bilan_lf(void)
{
    fprintf(stderr, "BILAN libFuzzer %s : %llu cas joués", FZ_CIBLE, joues);
    for (size_t k = 0; k < N_CONNUS; k++)
        if (!strcmp(CONNUS[k][0], FZ_CIBLE)) fprintf(stderr, " ; échec connu %s %s : %llu cas", CONNUS[k][1], CONNUS[k][2], connus[k]);
    fputc('\n', stderr);
}

int LLVMFuzzerInitialize(int *argc, char ***argv);
int LLVMFuzzerInitialize(int *argc, char ***argv)
{
    (void)argc;
    (void)argv;
    atexit(bilan_lf);
    return 0;
}

int LLVMFuzzerTestOneInput(const uint8_t *d, size_t n);
int LLVMFuzzerTestOneInput(const uint8_t *d, size_t n)
{
    fz_ctx_t c = {.check = -1};
    if (n > FZ_CAP) return -1;                         /* le pilote gcc ne le relirait pas (lire) : hors du corpus */
    joues++;
    fz_run(&c, d, n);
    if (c.check < 0) return 0;
    for (size_t k = 0; k < N_CONNUS; k++)
        if (!strcmp(CONNUS[k][0], FZ_CIBLE) && !strcmp(CONNUS[k][1], FZ_CHECKS[c.check].prop) &&
            !strcmp(CONNUS[k][2], FZ_CHECKS[c.check].nom)) {
            connus[k]++;
            return 0;
        }
    fprintf(stderr, "ECHEC %s %s (%s) : %s\n", FZ_CHECKS[c.check].prop, FZ_CHECKS[c.check].nom, FZ_CHECKS[c.check].source,
            c.why);
    abort();
}

#else
/* ─────────────────────────── la mort du processus ─────────────────────────── */

static char        dump_san[600], dump_garde[600], mort[200];   /* mort : la ligne que la campagne imprime en mourant */
static const uint8_t *dump_d;
static size_t      dump_n;

static void dump_to(const char *chemin)
{
    int fd;
    if (!chemin[0] || !dump_d) return;
    fd = open(chemin, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) return;
    (void)!write(fd, dump_d, dump_n);
    close(fd);
}

static void on_alarm(int sig)
{
    static const char m[] = "ECHEC P2 garde : le cas ne rend pas la main en " TEXTE(FZ_GARDE_S) " s de processeur\n";
    (void)sig;
    (void)!write(1, m, sizeof m - 1);
    (void)!write(1, mort, strlen(mort));
    dump_to(dump_garde);
    _exit(EXIT_GARDE);
}

#if defined(__SANITIZE_ADDRESS__)
static void on_san(void)
{
    static const char m[] = "ECHEC P1 sanitizer : son rapport sur la sortie d'erreur\n";
    (void)!write(1, m, sizeof m - 1);
    (void)!write(1, mort, strlen(mort));
    dump_to(dump_san);
    _exit(EXIT_SAN);
}
#endif

void fz_on_death(const char *rep, uint64_t graine, uint64_t i, const uint8_t *d, size_t n)
{
    dump_d = d;
    dump_n = n;
    dump_san[0] = dump_garde[0] = mort[0] = '\0';
    if (!rep) return;
    snprintf(dump_san, sizeof dump_san, "%s/P1-sanitizer-g%llu-%llu.bin", rep, (unsigned long long)graine,
             (unsigned long long)i);
    snprintf(dump_garde, sizeof dump_garde, "%s/P2-garde-g%llu-%llu.bin", rep, (unsigned long long)graine,
             (unsigned long long)i);
    snprintf(mort, sizeof mort, "   cas %llu de la graine %llu, écrit non réduit ; BILAN %s graine=%llu interrompu : %llu cas joués\n",
             (unsigned long long)i, (unsigned long long)graine, FZ_CIBLE, (unsigned long long)graine, (unsigned long long)i);
}

static void arm(void)
{
    struct sigaction sa;
    memset(&sa, 0, sizeof sa);
    sa.sa_handler = on_alarm;
    sigaction(SIGPROF, &sa, NULL);
#if defined(__SANITIZE_ADDRESS__)
    __sanitizer_set_death_callback(on_san);
#endif
}

/* ─────────────────────────── fichiers ─────────────────────────── */

static uint8_t *lire(const char *chemin, size_t *n)
{
    FILE *f = fopen(chemin, "rb");
    uint8_t *b = malloc(FZ_CAP + 1);
    if (!f || !b) {
        fprintf(stderr, "%s : illisible (%s)\n", chemin, strerror(errno));
        exit(2);
    }
    *n = fread(b, 1, FZ_CAP + 1, f);
    fclose(f);
    if (*n > FZ_CAP) {
        fprintf(stderr, "%s : plus de %zu octets\n", chemin, FZ_CAP);
        exit(2);
    }
    return b;
}

static void ecrire(const char *chemin, const uint8_t *d, size_t n)
{
    FILE *f = fopen(chemin, "wb");
    if (!f || fwrite(d, 1, n, f) != n || fclose(f)) {
        fprintf(stderr, "%s : écriture impossible\n", chemin);
        exit(2);
    }
}

/* ─────────────────────────── un cas ─────────────────────────── */

/* La garde armée pour `s` secondes de processeur (0 : désarmée) ; échue, SIGPROF (on_alarm). */
static void garde(unsigned s)
{
    struct itimerval v;
    memset(&v, 0, sizeof v);
    v.it_value.tv_sec = s;
    setitimer(ITIMER_PROF, &v, NULL);
}

static fz_ctx_t jouer(const uint8_t *d, size_t n, FILE *trace)
{
    fz_ctx_t c = {.check = -1, .trace = trace};
    garde(FZ_GARDE_S);
    fz_run(&c, d, n);
    garde(0);
    return c;
}

/* Le verdict d'un essai dans un fils : 0 passe, EXIT_CHECK + vérification, EXIT_GARDE, EXIT_SAN, 200 + signal. */
static int verdict_fils(const uint8_t *d, size_t n)
{
    int st;
    pid_t p;
    fflush(NULL);
    p = fork();
    if (p == 0) {
        int nul = open("/dev/null", O_WRONLY);
        fz_ctx_t c;
        if (nul >= 0) {
            dup2(nul, 1);
            dup2(nul, 2);
        }
        fz_on_death(NULL, 0, 0, d, n);
        c = jouer(d, n, NULL);
        _exit(c.check < 0 ? 0 : EXIT_CHECK + c.check);
    }
    if (p < 0 || waitpid(p, &st, 0) != p) return -1;
    if (WIFEXITED(st)) return WEXITSTATUS(st);
    return 200 + WTERMSIG(st);
}

/* Coupe le cas au plus court préfixe qui échoue encore, retire des morceaux (moitiés, quarts… jusqu'à l'octet), puis met
 * des octets à zéro, tant que le verdict reste `v` : la même vérification, au même lieu. Rend la longueur réduite. Le
 * préfixe n'est pas monotone en général (un préfixe plus court peut échouer autrement) : la dichotomie ne garde que des
 * longueurs vérifiées, au pire elle ne réduit rien. */
static size_t reduire_octets(uint8_t *d, size_t n, int v)
{
    uint8_t *t = malloc(FZ_CAP);
    unsigned essais = 0;
    bool progres = true;
    if (!t) exit(2);
    {                                                  /* le plus court préfixe qui échoue encore, par dichotomie */
        size_t lo = 0, hi = n;                         /* hi échoue ; lo, non (le cas vide passe, ou on le suppose) */
        while (hi - lo > 1 && essais < ESSAIS_MAX) {
            size_t m = lo + (hi - lo) / 2;
            essais++;
            if (verdict_fils(d, m) == v) hi = m;
            else lo = m;
        }
        n = hi;
    }
    while (progres && essais < ESSAIS_MAX) {
        progres = false;
        for (size_t k = n / 2 ? n / 2 : 1; k >= 1 && essais < ESSAIS_MAX; k /= 2) {
            for (size_t a = 0; a + k <= n && essais < ESSAIS_MAX;) {
                memcpy(t, d, a);
                memcpy(t + a, d + a + k, n - a - k);
                essais++;
                if (verdict_fils(t, n - k) == v) {
                    memcpy(d, t, n - k);
                    n -= k;
                    progres = true;
                } else {
                    a += k;
                }
            }
            if (k == 1) break;
        }
    }
    for (size_t a = 0; a < n && essais < ESSAIS_MAX; a++) {
        uint8_t o = d[a];
        if (!o) continue;
        d[a] = 0;
        essais++;
        if (verdict_fils(d, n) != v) d[a] = o;
    }
    free(t);
    return n;
}

/* La réduction d'un cas fait d'événements (FZ_GRAIN octets après un en-tête de FZ_ENTETE ; fuzz_session) : le plus
 * court préfixe d'événements qui échoue encore, puis des événements entiers retirés (moitiés, quarts… jusqu'à un), puis
 * chacun neutralisé (ses octets après les FZ_NEUTRE premiers mis à zéro : l'attente gardée, l'opération devenue « rien »),
 * tant que le verdict reste `v`, au plus FZ_ESSAIS essais : un essai y coûte un scénario entier. */
static size_t reduire_ev(uint8_t *d, size_t n, int v)
{
    const size_t h = FZ_ENTETE, g = FZ_GRAIN;
    uint8_t *t = malloc(FZ_CAP);
    unsigned essais = 0;
    size_t m = (n - h) / g;                            /* les événements entiers */
    bool progres = true;
    if (!t) exit(2);
    {
        size_t lo = 0, hi = m;                         /* hi échoue ; lo, non (ou on le suppose) */
        while (hi - lo > 1 && essais < FZ_ESSAIS) {
            size_t mi = lo + (hi - lo) / 2;
            essais++;
            if (verdict_fils(d, h + mi * g) == v) hi = mi;
            else lo = mi;
        }
        m = hi;
    }
    while (progres && essais < FZ_ESSAIS) {
        progres = false;
        for (size_t k = m / 2 ? m / 2 : 1; k >= 1 && essais < FZ_ESSAIS; k /= 2) {
            for (size_t a = 0; a + k <= m && essais < FZ_ESSAIS;) {
                memcpy(t, d, h + a * g);
                memcpy(t + h + a * g, d + h + (a + k) * g, (m - a - k) * g);
                essais++;
                if (verdict_fils(t, h + (m - k) * g) == v) {
                    memcpy(d, t, h + (m - k) * g);
                    m -= k;
                    progres = true;
                } else {
                    a += k;
                }
            }
            if (k == 1) break;
        }
    }
    for (size_t a = 0; a < m && essais < FZ_ESSAIS && g <= 64; a++) {
        uint8_t *e = d + h + a * g, o[64];
        bool nul = true;
        for (size_t i = FZ_NEUTRE; i < g; i++) nul = nul && !e[i];
        if (nul) continue;
        memcpy(o, e, g);
        memset(e + FZ_NEUTRE, 0, g - FZ_NEUTRE);
        essais++;
        if (verdict_fils(d, h + m * g) != v) memcpy(e, o, g);
    }
    free(t);
    return h + m * g;
}

/* La réduction d'un cas : par événements si le harnais les déclare (FZ_GRAIN), par octets sinon. */
static size_t reduire(uint8_t *d, size_t n, int v)
{
    if (&FZ_GRAIN && FZ_GRAIN && n >= FZ_ENTETE) return reduire_ev(d, n, v);
    return reduire_octets(d, n, v);
}

static const char *nom_verdict(int v, char *b, size_t cap)
{
    if (v == 0) snprintf(b, cap, "passe");
    else if (v == EXIT_GARDE) snprintf(b, cap, "P2 garde de durée");
    else if (v == EXIT_SAN) snprintf(b, cap, "P1 sanitizer");
    else if (v >= EXIT_CHECK && (size_t)(v - EXIT_CHECK) < FZ_N_CHECKS)
        snprintf(b, cap, "%s %s", FZ_CHECKS[v - EXIT_CHECK].prop, FZ_CHECKS[v - EXIT_CHECK].nom);
    else snprintf(b, cap, "issue %d", v);
    return b;
}

/* ─────────────────────────── les commandes ─────────────────────────── */

static double secondes(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

static int campagne(uint64_t graine, uint64_t n, const char *rep)
{
    uint8_t *d = malloc(FZ_CAP);
    unsigned *vus = calloc(FZ_N_CHECKS, sizeof *vus);
    uint64_t echecs = 0;
    double t0 = secondes(), dt;
    if (!d || !vus) exit(2);
    for (uint64_t i = 0; i < n; i++) {
        fz_rng_t r;
        size_t len;
        fz_ctx_t c;
        fz_seed(&r, graine, i);
        len = fz_gen(&r, d, FZ_CAP);
        fz_on_death(rep, graine, i, d, len);
        c = jouer(d, len, NULL);
        if (c.check < 0) continue;
        echecs++;
        if (vus[c.check]++) continue;
        {
            char chemin[600], b[120];
            size_t m;
            int v = verdict_fils(d, len);
            printf("ECHEC %s %s : cas %llu de la graine %llu : %s\n", FZ_CHECKS[c.check].prop, FZ_CHECKS[c.check].nom,
                   (unsigned long long)i, (unsigned long long)graine, c.why);
            if (v != EXIT_CHECK + c.check) {
                printf("   non reproduit dans un fils (%s) : non rangé\n", nom_verdict(v, b, sizeof b));
                continue;
            }
            m = reduire(d, len, v);
            snprintf(chemin, sizeof chemin, "%s/%s-%s-g%llu-%llu.bin", rep, FZ_CHECKS[c.check].prop, FZ_CHECKS[c.check].nom,
                     (unsigned long long)graine, (unsigned long long)i);
            ecrire(chemin, d, m);
            printf("   réduit de %zu à %zu octets : %s\n", len, m, chemin);
        }
    }
    dt = secondes() - t0;
    printf("BILAN %s graine=%llu cas=%llu echecs=%llu duree_s=%.2f cas_par_s=%.0f\n", FZ_CIBLE, (unsigned long long)graine,
           (unsigned long long)n, (unsigned long long)echecs, dt, dt > 0 ? (double)n / dt : 0.0);
    for (size_t k = 0; k < FZ_N_CHECKS; k++)
        if (vus[k]) printf("   %s %s : %u cas\n", FZ_CHECKS[k].prop, FZ_CHECKS[k].nom, vus[k]);
    if (fz_bilan) fz_bilan();
    free(d);
    free(vus);
    return 0;
}

/* Les cas de la graine, joués jusqu'au premier qui échoue (sur la vérification `prop` `nom` si elles sont données ; les
 * autres échecs sont alors comptés et passés) : son verdict imprimé, rien de rangé ni de réduit. La commande des morsures
 * (fuzz.sh mord) : une faute mord dès qu'un cas la voit, sans payer le reste de la campagne, et sans buter sur une
 * trouvaille connue du code réel. */
static int premier(uint64_t graine, uint64_t n, const char *prop, const char *nom)
{
    uint64_t autres = 0;
    uint8_t *d = malloc(FZ_CAP);
    if (!d) exit(2);
    for (uint64_t i = 0; i < n; i++) {
        fz_rng_t r;
        size_t len;
        fz_ctx_t c;
        fz_seed(&r, graine, i);
        len = fz_gen(&r, d, FZ_CAP);
        fz_on_death(NULL, 0, 0, d, len);
        c = jouer(d, len, NULL);
        if (c.check < 0) continue;
        if (prop && (strcmp(FZ_CHECKS[c.check].prop, prop) || strcmp(FZ_CHECKS[c.check].nom, nom))) {
            autres++;
            continue;
        }
        printf("ECHEC %s %s : cas %llu de la graine %llu : %s\n", FZ_CHECKS[c.check].prop, FZ_CHECKS[c.check].nom,
               (unsigned long long)i, (unsigned long long)graine, c.why);
        free(d);
        return 1;
    }
    printf("aucun échec%s%s%s en %llu cas de la graine %llu (%llu autres échecs passés)\n", prop ? " " : "",
           prop ? prop : "", prop ? " visé" : "", (unsigned long long)n, (unsigned long long)graine,
           (unsigned long long)autres);
    free(d);
    return 0;
}

static int rejoue(const char *chemin)
{
    size_t n;
    uint8_t *d = lire(chemin, &n);
    fz_ctx_t c;
    fz_on_death(NULL, 0, 0, d, n);
    printf("rejeu %s : %zu octets\n", FZ_CIBLE, n);
    c = jouer(d, n, stdout);
    if (c.check < 0) printf("VERDICT passe\n");
    else printf("VERDICT ECHEC %s %s (%s) : %s\n", FZ_CHECKS[c.check].prop, FZ_CHECKS[c.check].nom, FZ_CHECKS[c.check].source,
                c.why);
    free(d);
    return c.check < 0 ? 0 : 1;
}

static int reduit(const char *chemin, const char *sortie)
{
    size_t n, m;
    uint8_t *d = lire(chemin, &n);
    char b[120];
    int v = verdict_fils(d, n);
    if (v == 0) {
        fprintf(stderr, "%s : le cas passe, rien à réduire\n", chemin);
        return 1;
    }
    m = reduire(d, n, v);
    ecrire(sortie, d, m);
    printf("%s : %s, réduit de %zu à %zu octets\n", sortie, nom_verdict(v, b, sizeof b), n, m);
    free(d);
    return 0;
}

static int ecrit(uint64_t graine, uint64_t i, const char *sortie)
{
    uint8_t *d = malloc(FZ_CAP);
    fz_rng_t r;
    size_t n;
    if (!d) exit(2);
    fz_seed(&r, graine, i);
    n = fz_gen(&r, d, FZ_CAP);
    ecrire(sortie, d, n);
    free(d);
    return 0;
}

static uint64_t nombre(const char *s)
{
    char *e;
    unsigned long long v = strtoull(s, &e, 0);
    if (!*s || *e) {
        fprintf(stderr, "« %s » n'est pas un nombre\n", s);
        exit(2);
    }
    return v;
}

int main(int argc, char **argv)
{
    int rc;
    setvbuf(stdout, NULL, _IOLBF, 0);
    arm();
    if (argc == 5 && !strcmp(argv[1], "campagne")) return campagne(nombre(argv[2]), nombre(argv[3]), argv[4]);
    if (argc == 3 && !strcmp(argv[1], "rejoue")) return rejoue(argv[2]);
    if (argc == 4 && !strcmp(argv[1], "reduit")) return reduit(argv[2], argv[3]);
    if (argc == 4 && !strcmp(argv[1], "premier")) return premier(nombre(argv[2]), nombre(argv[3]), NULL, NULL);
    if (argc == 6 && !strcmp(argv[1], "premier")) return premier(nombre(argv[2]), nombre(argv[3]), argv[4], argv[5]);
    if (argc == 5 && !strcmp(argv[1], "ecrit")) return ecrit(nombre(argv[2]), nombre(argv[3]), argv[4]);
    if (argc >= 2 && (rc = fz_cmd(argc - 1, argv + 1)) >= 0) return rc;
    fprintf(stderr, "usage : %s campagne <graine> <n> <rép> | rejoue <cas> | reduit <cas> <sortie> | ecrit <graine> <i> "
                    "<sortie> | premier <graine> <n> [<propriété> <vérification>] (et les commandes du harnais, README.md)\n", argv[0]);
    return 2;
}
#endif
