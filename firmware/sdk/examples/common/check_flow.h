#pragma once
#include <stdbool.h>
#include <stdint.h>

/* Application-owned pure state. No SDK, RTOS or LVGL calls. */
typedef enum { HC_SCREEN, HC_CAMERA, HC_SPEAKER, HC_MIC, HC_TOUCH, HC_LIGHTS, HC_MOTION, HC_COMBINED, HC_STEP_COUNT } hc_step_t;
typedef enum { HC_WELCOME, HC_IDLE, HC_RUNNING, HC_CANCELLING, HC_REVIEW, HC_PASSED, HC_ISSUE, HC_SKIPPED, HC_UNAVAILABLE } hc_phase_t;
typedef enum { HC_UNTESTED, HC_PASS, HC_FAIL, HC_SKIP } hc_result_t;
typedef enum { HC_OP_SCREEN, HC_OP_PHOTO, HC_OP_STREAM, HC_OP_VOICE, HC_OP_TONE, HC_OP_RECORD, HC_OP_REPLAY,
               HC_OP_TOUCH, HC_OP_LIGHT, HC_OP_MOVE, HC_OP_STOP_TEST, HC_OP_COMBINED, HC_OP_VOLUME } hc_operation_t;
enum { HC_DISPLAY = 1, HC_CAMERA_MODULE = 2, HC_AUDIO = 4, HC_BODY = 8, HC_HEAD = 7 };
typedef struct {
    const char *title, *action, *instruction, *question;
    hc_operation_t operation;
    unsigned argument, modules;
} hc_item_t;
typedef struct {
    unsigned step, item, modules;
    uint32_t attempt, confirmed[HC_STEP_COUNT], failed[HC_STEP_COUNT], skipped[HC_STEP_COUNT];
    hc_phase_t phase;
    hc_result_t results[HC_STEP_COUNT];
    int last_error;
    bool body_fault;
} hc_flow_t;
void hc_flow_init(hc_flow_t *flow);
const char *hc_step_name(unsigned step);
const hc_item_t *hc_current_item(const hc_flow_t *flow);
unsigned hc_item_count(unsigned step);
bool hc_flow_busy(const hc_flow_t *flow);
/* UI intentions carry the page/phase that was actually visible when tapped.
 * In particular, a double tap on Ready cannot also confirm a new safety page. */
bool hc_flow_accept_intent(const hc_flow_t *flow, uint32_t attempt, hc_phase_t phase, unsigned page, unsigned current_page);
bool hc_flow_available(const hc_flow_t *flow);
void hc_flow_set_modules(hc_flow_t *flow, unsigned modules, bool body_fault);
bool hc_flow_select(hc_flow_t *flow, unsigned step);
bool hc_flow_select_item(hc_flow_t *flow, unsigned step, unsigned item);
void hc_flow_restart(hc_flow_t *flow);
bool hc_flow_begin(hc_flow_t *flow, bool explicit_motion_confirmation);
bool hc_flow_cancel(hc_flow_t *flow);
/* Cancellation does not release busy until the actual owner reports cleanup. */
bool hc_flow_finish(hc_flow_t *flow, uint32_t attempt, int error, bool cancelled);
bool hc_flow_confirm(hc_flow_t *flow);
bool hc_flow_reject(hc_flow_t *flow);
bool hc_flow_skip(hc_flow_t *flow);
bool hc_flow_retry(hc_flow_t *flow);
bool hc_flow_next(hc_flow_t *flow);
/* Retain a failed/skipped item's result and advance without executing anything. */
bool hc_flow_continue(hc_flow_t *flow);
