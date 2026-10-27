/* L'ESP-IDF simulé de test_store (sim/test/test_store.c) : ce que components/store/store.c lit d'esp_err.h, rien
 * d'autre. Les valeurs sont celles d'ESP-IDF v5.3.2 (components/esp_common/include/esp_err.h) ; test_store les compare
 * à ses propres valeurs, écrites à la main. */
#ifndef SIM_IDF_ESP_ERR_H
#define SIM_IDF_ESP_ERR_H

#include <stdlib.h>

typedef int esp_err_t;

#define ESP_OK   0
#define ESP_FAIL -1

/* Comme celui d'ESP-IDF : un échec arrête la carte. */
#define ESP_ERROR_CHECK(x)            \
    do {                              \
        if ((x) != ESP_OK) abort();   \
    } while (0)

#endif
