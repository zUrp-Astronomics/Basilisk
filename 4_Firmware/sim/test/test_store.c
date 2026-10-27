/* test_store : le vrai magasin de la carte (components/store/store.c), compilé sur l'hôte contre un ESP-IDF simulé
 * (sim/idf/ : les en-têtes que store.c lit) et le faux ci-dessous (la NVS, les files, la tâche). Joué : sa décision entre
 * une clé absente et une erreur à la lecture, son état de la NVS à l'init (ok, erased, er), et le résultat de ses
 * écritures (bsk_store.h). Les codes d'erreur sont écrits à la main ici, d'après ESP-IDF v5.3.2
 * (components/nvs_flash/include/nvs.h, components/esp_common/include/esp_err.h), et l'ESP-IDF simulé leur est comparé :
 * la décision est jouée contre les codes d'ESP-IDF, pas contre ceux du simulé. Les attendus sont écrits à la main
 * d'après bsk_store.h et l'API NVS d'ESP-IDF (« ESP_ERR_NVS_NOT_FOUND if namespace doesn't exist yet and mode is
 * NVS_READONLY »).
 *
 * La tâche du magasin tourne dans l'appel de serve() : elle sert les dépôts, puis, sa file vide, son attente bornée
 * (IDLE_MS) rend la main au test (longjmp) au lieu d'attendre. */
#include <setjmp.h>
#include <stdio.h>
#include <string.h>

#include "bsk_store.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "nvs.h"
#include "nvs_flash.h"

static int checks, fails;

#define CHECK(c, ...)                                                   \
    do {                                                                \
        checks++;                                                       \
        if (!(c)) {                                                     \
            fails++;                                                    \
            printf("  ECHEC %s:%d : ", __FILE__, __LINE__);             \
            printf(__VA_ARGS__);                                        \
            printf("\n");                                               \
        }                                                               \
    } while (0)

/* Les codes d'ESP-IDF v5.3.2, écrits à la main. */
#define OK               0
#define FAIL             (-1)
#define NOT_INITIALIZED  0x1101
#define NOT_FOUND        0x1102
#define TYPE_MISMATCH    0x1103
#define NOT_ENOUGH_SPACE 0x1105
#define INVALID_HANDLE   0x1107
#define NO_FREE_PAGES    0x110d
#define PART_NOT_FOUND   0x110f
#define NEW_VERSION      0x1110

/* ─────────────────────────── le faux : la NVS ─────────────────────────── */

static struct {
    esp_err_t open_ro, open_rw;   /* ce que rend nvs_open, selon le mode */
    esp_err_t get, set, commit, erase_key;
    uint32_t  value;              /* la valeur rangée, rendue par nvs_get_u32 quand il rend OK */
    esp_err_t init[2];            /* le premier nvs_flash_init, puis le second (après l'effacement) */
    unsigned  n_init;
    esp_err_t flash_erase;
    unsigned  n_flash_erase;
    unsigned  n_open, n_close;
    nvs_open_mode_t mode;         /* le mode du dernier nvs_open */
    char      ns[16];             /* l'espace du dernier nvs_open */
    char      key[16];            /* la clé de la dernière opération */
    uint32_t  set_v;
} N;

esp_err_t nvs_open(const char *ns, nvs_open_mode_t mode, nvs_handle_t *h)
{
    N.n_open++;
    N.mode = mode;
    snprintf(N.ns, sizeof N.ns, "%s", ns);
    *h = 7;
    return mode == NVS_READONLY ? N.open_ro : N.open_rw;
}

void nvs_close(nvs_handle_t h)
{
    (void)h;
    N.n_close++;
}

esp_err_t nvs_get_u32(nvs_handle_t h, const char *key, uint32_t *v)
{
    (void)h;
    snprintf(N.key, sizeof N.key, "%s", key);
    if (N.get == ESP_OK) *v = N.value;
    return N.get;
}

esp_err_t nvs_set_u32(nvs_handle_t h, const char *key, uint32_t v)
{
    (void)h;
    snprintf(N.key, sizeof N.key, "%s", key);
    N.set_v = v;
    return N.set;
}

esp_err_t nvs_erase_key(nvs_handle_t h, const char *key)
{
    (void)h;
    snprintf(N.key, sizeof N.key, "%s", key);
    return N.erase_key;
}

esp_err_t nvs_commit(nvs_handle_t h)
{
    (void)h;
    return N.commit;
}

esp_err_t nvs_flash_init(void) { return N.init[N.n_init++ ? 1 : 0]; }

esp_err_t nvs_flash_erase(void)
{
    N.n_flash_erase++;
    return N.flash_erase;
}

/* ─────────────────────────── le faux : les files, la tâche ─────────────────────────── */

struct sim_queue {
    size_t        len, size, head, n;
    unsigned char buf[8 * 64];
};

static struct sim_queue QS[2];
static unsigned n_q;
static TaskFunction_t task;
static jmp_buf idle;

QueueHandle_t xQueueCreate(UBaseType_t len, UBaseType_t size)
{
    struct sim_queue *q;
    if (n_q == 2 || len * size > sizeof QS[0].buf) return NULL;
    q = &QS[n_q++];
    memset(q, 0, sizeof *q);
    q->len = len;
    q->size = size;
    return q;
}

BaseType_t xQueueSend(QueueHandle_t q, const void *item, TickType_t wait)
{
    (void)wait;                                     /* la file des résultats n'est jamais pleine ici : le test la vide */
    if (q->n == q->len) return pdFALSE;
    memcpy(q->buf + (q->head + q->n++) % q->len * q->size, item, q->size);
    return pdTRUE;
}

/* Vide : sans attente, pdFALSE ; avec (la tâche du magasin, son attente d'un dépôt), la main rendue au test. */
BaseType_t xQueueReceive(QueueHandle_t q, void *item, TickType_t wait)
{
    if (!q->n) {
        if (wait) longjmp(idle, 1);
        return pdFALSE;
    }
    memcpy(item, q->buf + q->head * q->size, q->size);
    q->head = (q->head + 1) % q->len;
    q->n--;
    return pdTRUE;
}

BaseType_t xTaskCreatePinnedToCore(TaskFunction_t fn, const char *name, uint32_t stack, void *arg, UBaseType_t prio,
                                   TaskHandle_t *handle, BaseType_t core)
{
    (void)name, (void)stack, (void)arg, (void)prio, (void)handle, (void)core;
    task = fn;
    return pdPASS;
}

/* La tâche du magasin sert ce qui est déposé, jusqu'à sa file vide. */
static void serve(void)
{
    if (!setjmp(idle)) task(NULL);
}

/* Une NVS saine, rien de rangé ; le magasin initialisé dessus. */
static void fresh(void)
{
    memset(&N, 0, sizeof N);
    N.get = NOT_FOUND;
    n_q = 0;
    task = NULL;
    bsk_store_init();
}

/* Une opération déposée, servie, son résultat relevé. */
static bool op(bsk_store_op_t o, const char *key, uint32_t v, bsk_store_res_t *r)
{
    memset(r, 0xA5, sizeof *r);
    if (!bsk_store_request(o, key, v)) return false;
    serve();
    return bsk_store_result(r) && !bsk_store_result(&(bsk_store_res_t){0});
}

/* ─────────────────────────── les tests ─────────────────────────── */

static void t_codes(void)
{
    printf("l'ESP-IDF simulé : les codes d'ESP-IDF v5.3.2\n");
    CHECK(ESP_OK == OK && ESP_FAIL == FAIL, "ESP_OK, ESP_FAIL");
    CHECK(ESP_ERR_NVS_NOT_INITIALIZED == NOT_INITIALIZED && ESP_ERR_NVS_NOT_FOUND == NOT_FOUND &&
              ESP_ERR_NVS_TYPE_MISMATCH == TYPE_MISMATCH && ESP_ERR_NVS_NOT_ENOUGH_SPACE == NOT_ENOUGH_SPACE &&
              ESP_ERR_NVS_INVALID_HANDLE == INVALID_HANDLE && ESP_ERR_NVS_NO_FREE_PAGES == NO_FREE_PAGES &&
              ESP_ERR_NVS_PART_NOT_FOUND == PART_NOT_FOUND && ESP_ERR_NVS_NEW_VERSION_FOUND == NEW_VERSION,
          "les codes de la NVS");
}

/* La lecture : une valeur rangée ; une absence (l'espace `mark` pas encore créé, nvs_open en lecture seule ; la clé
 * absente) ; une erreur, tout le reste, à l'ouverture ou à la lecture. Ouverte en lecture seule, dans l'espace `mark`,
 * et refermée. */
static void t_get(void)
{
    static const struct {
        const char *why;
        esp_err_t   open, get;
        bool        ok, err;
    } C[] = {
        {"rangée", OK, OK, true, false},
        {"l'espace absent (nvs_open, lecture seule)", NOT_FOUND, OK, false, false},
        {"la clé absente", OK, NOT_FOUND, false, false},
        {"la NVS en panne (nvs_open)", NOT_INITIALIZED, OK, false, true},
        {"la partition absente (nvs_open)", PART_NOT_FOUND, OK, false, true},
        {"ESP_FAIL (nvs_open)", FAIL, OK, false, true},
        {"un autre type sous la clé", OK, TYPE_MISMATCH, false, true},
        {"un descripteur invalide", OK, INVALID_HANDLE, false, true},
        {"la NVS en panne (nvs_get_u32)", OK, NOT_INITIALIZED, false, true},
        {"ESP_FAIL (nvs_get_u32)", OK, FAIL, false, true},
    };
    bsk_store_res_t r;
    printf("lecture : rangée, absente (l'espace, la clé), en erreur (tout le reste)\n");
    for (size_t i = 0; i < sizeof C / sizeof C[0]; i++) {
        fresh();
        N.open_ro = C[i].open;
        N.open_rw = FAIL;                           /* une lecture n'ouvre jamais en écriture */
        N.get = C[i].get;
        N.value = 0x14E20;
        CHECK(op(BSK_STORE_GET, "01030800bf087", 0, &r), "%s : un résultat, un seul", C[i].why);
        CHECK(r.op == BSK_STORE_GET && !strcmp(r.key, "01030800bf087") && r.ok == C[i].ok && r.err == C[i].err,
              "%s : ok=%d err=%d (attendu %d %d)", C[i].why, r.ok, r.err, C[i].ok, C[i].err);
        CHECK(!C[i].ok || r.value == 0x14E20, "%s : la valeur 0x14E20 (0x%X)", C[i].why, (unsigned)r.value);
        CHECK(N.n_open == 1 && N.mode == NVS_READONLY && !strcmp(N.ns, "mark"), "%s : ouverte en lecture seule, l'espace "
              "mark", C[i].why);
        CHECK(N.n_close == (C[i].open == OK), "%s : refermée si ouverte (%u)", C[i].why, N.n_close);
    }
}

/* L'écriture et l'effacement : faits (rangés, ok), ou ratés ; l'effacement d'une clé absente est fait (bsk_store.h). */
static void t_put_del(void)
{
    static const struct {
        const char    *why;
        bsk_store_op_t op;
        esp_err_t      open, act, commit;
        bool           ok;
    } C[] = {
        {"PUT rangée", BSK_STORE_PUT, OK, OK, OK, true},
        {"PUT, nvs_open en échec", BSK_STORE_PUT, NOT_INITIALIZED, OK, OK, false},
        {"PUT, plus de place", BSK_STORE_PUT, OK, NOT_ENOUGH_SPACE, OK, false},
        {"PUT, nvs_commit en échec", BSK_STORE_PUT, OK, OK, FAIL, false},
        {"DEL faite", BSK_STORE_DEL, OK, OK, OK, true},
        {"DEL d'une clé absente", BSK_STORE_DEL, OK, NOT_FOUND, OK, true},
        {"DEL, nvs_open en échec", BSK_STORE_DEL, NOT_INITIALIZED, OK, OK, false},
        {"DEL, un descripteur invalide", BSK_STORE_DEL, OK, INVALID_HANDLE, OK, false},
        {"DEL, nvs_commit en échec", BSK_STORE_DEL, OK, OK, FAIL, false},
    };
    bsk_store_res_t r;
    printf("écriture, effacement : faits, ratés ; une clé absente est effacée\n");
    for (size_t i = 0; i < sizeof C / sizeof C[0]; i++) {
        fresh();
        N.open_ro = FAIL;                           /* une écriture n'ouvre jamais en lecture seule */
        N.open_rw = C[i].open;
        N.set = N.erase_key = C[i].act;
        N.commit = C[i].commit;
        CHECK(op(C[i].op, "01030800bf087", 0x14E20, &r), "%s : un résultat, un seul", C[i].why);
        CHECK(r.op == C[i].op && !strcmp(r.key, "01030800bf087") && r.ok == C[i].ok && !r.err, "%s : ok=%d err=%d (attendu %d 0)",
              C[i].why, r.ok, r.err, C[i].ok);
        CHECK(N.mode == NVS_READWRITE && !strcmp(N.ns, "mark") && N.n_close == (C[i].open == OK), "%s : ouverte en écriture, "
              "l'espace mark, refermée si ouverte", C[i].why);
        CHECK(C[i].op != BSK_STORE_PUT || C[i].open != OK || N.set_v == 0x14E20, "%s : 0x14E20 écrite", C[i].why);
    }
}

/* L'état de la NVS à l'init (bsk_store_nvs) : saine ; effacée sur NO_FREE_PAGES et NEW_VERSION_FOUND seulement, puis
 * saine ; en panne si l'init, l'effacement ou la réinit échouent. */
static void t_init(void)
{
    static const struct {
        const char     *why;
        esp_err_t       init, erase, reinit;
        bsk_store_nvs_t want;
        unsigned        erased;
    } C[] = {
        {"saine", OK, OK, OK, BSK_STORE_NVS_OK, 0},
        {"plus de page libre, effacée", NO_FREE_PAGES, OK, OK, BSK_STORE_NVS_ERASED, 1},
        {"une autre version, effacée", NEW_VERSION, OK, OK, BSK_STORE_NVS_ERASED, 1},
        {"plus de page libre, l'effacement en échec", NO_FREE_PAGES, FAIL, OK, BSK_STORE_NVS_BROKEN, 1},
        {"une autre version, la réinit en échec", NEW_VERSION, OK, PART_NOT_FOUND, BSK_STORE_NVS_BROKEN, 1},
        {"la partition absente", PART_NOT_FOUND, OK, OK, BSK_STORE_NVS_BROKEN, 0},
        {"ESP_FAIL", FAIL, OK, OK, BSK_STORE_NVS_BROKEN, 0},
        {"NOT_FOUND à l'init", NOT_FOUND, OK, OK, BSK_STORE_NVS_BROKEN, 0},
    };
    printf("init : la NVS saine, effacée (deux codes), en panne\n");
    for (size_t i = 0; i < sizeof C / sizeof C[0]; i++) {
        memset(&N, 0, sizeof N);
        N.init[0] = C[i].init;
        N.init[1] = C[i].reinit;
        N.flash_erase = C[i].erase;
        n_q = 0;
        task = NULL;
        bsk_store_init();
        CHECK(bsk_store_nvs() == C[i].want, "%s : l'état %d (attendu %d)", C[i].why, bsk_store_nvs(), C[i].want);
        CHECK(N.n_flash_erase == C[i].erased && N.n_init == 1u + (C[i].erased && C[i].erase == OK ? 1u : 0u), "%s : %u effacement(s), %u "
              "init", C[i].why, N.n_flash_erase, N.n_init);
        CHECK(task != NULL && n_q == 2, "%s : la tâche et ses deux files créées, la carte continue", C[i].why);
    }
}

/* Les opérations dans l'ordre de leur dépôt, un résultat chacune ; la file des dépôts pleine refuse, rien de déposé ;
 * une clé trop longue pour la NVS refusée. */
static void t_order(void)
{
    bsk_store_res_t r;
    unsigned n = 0;
    bool in_order = true;
    printf("les dépôts : dans l'ordre, la file pleine refuse, une clé trop longue refusée\n");
    fresh();
    for (unsigned i = 0; i < 8; i++) CHECK(bsk_store_request(i & 1 ? BSK_STORE_PUT : BSK_STORE_GET, "k", i), "dépôt %u", i);
    CHECK(!bsk_store_request(BSK_STORE_GET, "k", 0), "le neuvième, la file pleine : refusé");
    CHECK(!bsk_store_request(BSK_STORE_GET, "0123456789abcdef", 0), "une clé de 16 caractères : refusée");
    serve();
    while (bsk_store_result(&r)) in_order = in_order && r.op == (n++ & 1 ? BSK_STORE_PUT : BSK_STORE_GET);
    CHECK(n == 8 && in_order, "huit résultats, dans l'ordre (%u)", n);
}

int main(void)
{
    t_codes();
    t_get();
    t_put_del();
    t_init();
    t_order();
    printf("magasin de la carte : %d vérifications, %d échec(s)\n", checks, fails);
    return fails ? 1 : 0;
}
