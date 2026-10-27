/* L'ESP-IDF simulé de test_store (sim/test/test_store.c) : les files de FreeRTOS, celles du faux de test_store. */
#ifndef SIM_IDF_QUEUE_H
#define SIM_IDF_QUEUE_H

#include "freertos/FreeRTOS.h"

typedef struct sim_queue *QueueHandle_t;

QueueHandle_t xQueueCreate(UBaseType_t len, UBaseType_t size);
BaseType_t xQueueSend(QueueHandle_t q, const void *item, TickType_t wait);
BaseType_t xQueueReceive(QueueHandle_t q, void *item, TickType_t wait);

#endif
