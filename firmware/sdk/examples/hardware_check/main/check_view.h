#pragma once
#include "check_flow.h"
#include "demo_image.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

typedef enum { CHECK_LEFT, CHECK_RIGHT, CHECK_TARGET, CHECK_IMAGE, CHECK_ROW, CHECK_VOLUME } check_input_kind_t;
typedef struct { check_input_kind_t kind; unsigned value, navigation; uint32_t attempt; hc_phase_t phase; } check_input_t;
typedef enum { CHECK_TEST_PAGE, CHECK_SAFETY_PAGE, CHECK_AUDIO_PAGE, CHECK_REPORT_PAGE, CHECK_DETAIL_PAGE,
               CHECK_RESTART_PAGE } check_page_t;
typedef struct {
    hc_flow_t flow;
    check_page_t page;
    unsigned report_page, detail_step, detail_page, targets, progress, meter, frames, volume;
    bool full_image, setting_pending, stopping;
    char status[64];
} check_view_model_t;
static inline unsigned check_view_navigation(const check_view_model_t *view) {
    return (unsigned)view->page * 32 + (view->page == CHECK_REPORT_PAGE ? view->report_page :
        view->page == CHECK_DETAIL_PAGE ? view->detail_page : 0);
}
/* Controller calls under display lock. LVGL callbacks only enqueue intentions. */
void check_view_create(QueueHandle_t inputs);
void check_view_render(const check_view_model_t *model);
/* Takes image ownership; attempt is validated by the controller, not the last rendered model. */
void check_view_image(demo_image_t *image, uint32_t attempt);
/* Software-rendered LVGL snapshot, not proof of physical LCD visibility. */
void check_view_snapshot(void);
