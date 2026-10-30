#pragma once
#include "check_flow.h"
#include "demo_image.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

typedef struct {
    hc_operation_t operation;
    uint32_t attempt;
    unsigned argument, volume;
} check_media_command_t;
typedef enum { CHECK_MEDIA_INIT, CHECK_MEDIA_PROGRESS, CHECK_MEDIA_IMAGE, CHECK_MEDIA_DONE } check_media_kind_t;
typedef struct {
    check_media_kind_t kind;
    uint32_t attempt;
    esp_err_t error;
    unsigned modules, progress, peak, frames;
    char text[64];
    demo_image_t image; /* Receiver owns pixels, including stale results. */
} check_media_event_t;
/* Single media lifecycle/audio owner; a separate decoder consumes JPEG copies.
 * Init never plays/records. Cancellation takes effect between bounded chunks.
 * DONE follows stream stop, decoder fence and audio drain. */
esp_err_t check_media_start(QueueHandle_t events);
bool check_media_submit(const check_media_command_t *command);
void check_media_cancel(void);
