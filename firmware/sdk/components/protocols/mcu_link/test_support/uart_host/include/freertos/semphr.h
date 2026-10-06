#pragma once
#include "FreeRTOS.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef void *SemaphoreHandle_t;
SemaphoreHandle_t xSemaphoreCreateRecursiveMutex(void);
int xSemaphoreTakeRecursive(SemaphoreHandle_t semaphore, TickType_t ticks);
int xSemaphoreGiveRecursive(SemaphoreHandle_t semaphore);
void vSemaphoreDelete(SemaphoreHandle_t semaphore);
#ifdef __cplusplus
}
#endif

