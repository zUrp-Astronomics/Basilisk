/* L'ESP-IDF simulé de test_store (sim/test/test_store.c) : ce que components/store/store.c lit de nvs_flash.h ; les
 * fonctions sont celles du faux de test_store. */
#ifndef SIM_IDF_NVS_FLASH_H
#define SIM_IDF_NVS_FLASH_H

#include "esp_err.h"

esp_err_t nvs_flash_init(void);
esp_err_t nvs_flash_erase(void);

#endif
