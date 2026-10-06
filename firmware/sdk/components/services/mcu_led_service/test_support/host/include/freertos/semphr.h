#ifndef TEST_SEMPHR_H
#define TEST_SEMPHR_H

#include "freertos/FreeRTOS.h"

typedef struct {
    BaseType_t locked;
} StaticSemaphore_t;

typedef StaticSemaphore_t *SemaphoreHandle_t;

static inline SemaphoreHandle_t xSemaphoreCreateMutexStatic(StaticSemaphore_t *storage) {
    if (storage != NULL) {
        storage->locked = pdFALSE;
    }
    return storage;
}

static inline BaseType_t xSemaphoreTake(SemaphoreHandle_t semaphore, TickType_t timeout) {
    (void)timeout;
    if (semaphore == NULL || semaphore->locked == pdTRUE) {
        return pdFALSE;
    }
    semaphore->locked = pdTRUE;
    return pdTRUE;
}

static inline BaseType_t xSemaphoreGive(SemaphoreHandle_t semaphore) {
    if (semaphore == NULL || semaphore->locked == pdFALSE) {
        return pdFALSE;
    }
    semaphore->locked = pdFALSE;
    return pdTRUE;
}

#endif /* TEST_SEMPHR_H */

