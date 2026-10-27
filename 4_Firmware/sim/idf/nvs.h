/* L'ESP-IDF simulé de test_store (sim/test/test_store.c) : ce que components/store/store.c lit de nvs.h. Les codes
 * d'erreur sont ceux d'ESP-IDF v5.3.2 (components/nvs_flash/include/nvs.h) ; test_store les compare à ses propres
 * valeurs, écrites à la main. Les fonctions sont celles du faux de test_store. */
#ifndef SIM_IDF_NVS_H
#define SIM_IDF_NVS_H

#include <stdint.h>

#include "esp_err.h"

#define ESP_ERR_NVS_BASE              0x1100
#define ESP_ERR_NVS_NOT_INITIALIZED   (ESP_ERR_NVS_BASE + 0x01)
#define ESP_ERR_NVS_NOT_FOUND         (ESP_ERR_NVS_BASE + 0x02)
#define ESP_ERR_NVS_TYPE_MISMATCH     (ESP_ERR_NVS_BASE + 0x03)
#define ESP_ERR_NVS_NOT_ENOUGH_SPACE  (ESP_ERR_NVS_BASE + 0x05)
#define ESP_ERR_NVS_INVALID_HANDLE    (ESP_ERR_NVS_BASE + 0x07)
#define ESP_ERR_NVS_NO_FREE_PAGES     (ESP_ERR_NVS_BASE + 0x0d)
#define ESP_ERR_NVS_PART_NOT_FOUND    (ESP_ERR_NVS_BASE + 0x0f)
#define ESP_ERR_NVS_NEW_VERSION_FOUND (ESP_ERR_NVS_BASE + 0x10)

typedef uint32_t nvs_handle_t;
typedef enum { NVS_READONLY, NVS_READWRITE } nvs_open_mode_t;

esp_err_t nvs_open(const char *ns, nvs_open_mode_t mode, nvs_handle_t *h);
esp_err_t nvs_get_u32(nvs_handle_t h, const char *key, uint32_t *v);
esp_err_t nvs_set_u32(nvs_handle_t h, const char *key, uint32_t v);
esp_err_t nvs_erase_key(nvs_handle_t h, const char *key);
esp_err_t nvs_commit(nvs_handle_t h);
void nvs_close(nvs_handle_t h);

#endif
