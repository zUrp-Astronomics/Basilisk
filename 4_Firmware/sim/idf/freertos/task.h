/* L'ESP-IDF simulé de test_store (sim/test/test_store.c) : la création d'une tâche, celle du faux de test_store. */
#ifndef SIM_IDF_TASK_H
#define SIM_IDF_TASK_H

#include "freertos/FreeRTOS.h"

typedef void *TaskHandle_t;
typedef void (*TaskFunction_t)(void *);

BaseType_t xTaskCreatePinnedToCore(TaskFunction_t fn, const char *name, uint32_t stack, void *arg, UBaseType_t prio,
                                   TaskHandle_t *handle, BaseType_t core);

#endif
