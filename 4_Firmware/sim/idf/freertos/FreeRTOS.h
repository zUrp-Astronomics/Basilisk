/* L'ESP-IDF simulé de test_store (sim/test/test_store.c) : les types et constantes de FreeRTOS que
 * components/store/store.c lit. */
#ifndef SIM_IDF_FREERTOS_H
#define SIM_IDF_FREERTOS_H

#include <stdint.h>

typedef long BaseType_t;
typedef unsigned long UBaseType_t;
typedef uint32_t TickType_t;

#define pdFALSE           ((BaseType_t)0)
#define pdTRUE            ((BaseType_t)1)
#define pdPASS            pdTRUE
#define portMAX_DELAY     ((TickType_t)0xFFFFFFFFu)
#define pdMS_TO_TICKS(ms) ((TickType_t)(ms))

#endif
