#pragma once
#include "FreeRTOS.h"
typedef void *TaskHandle_t;
BaseType_t xTaskCreate(void (*function)(void *), const char *name, uint32_t stack, void *ctx, unsigned priority,
                       TaskHandle_t *task);
void vTaskDelete(TaskHandle_t task);
void vTaskSuspend(TaskHandle_t task);
void vTaskDelay(TickType_t ticks);
