/* L'ESP-IDF simulé de test_store (sim/test/test_store.c) : le chien de garde des tâches, sans effet. */
#ifndef SIM_IDF_ESP_TASK_WDT_H
#define SIM_IDF_ESP_TASK_WDT_H

#include "esp_err.h"
#include "freertos/task.h"

static inline esp_err_t esp_task_wdt_add(TaskHandle_t t) { (void)t; return ESP_OK; }
static inline esp_err_t esp_task_wdt_reset(void) { return ESP_OK; }

#endif
