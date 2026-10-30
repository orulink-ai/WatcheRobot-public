#pragma once
#include "FreeRTOS.h"
typedef struct host_queue *QueueHandle_t;
BaseType_t xQueueSend(QueueHandle_t queue, const void *item, TickType_t ticks);
