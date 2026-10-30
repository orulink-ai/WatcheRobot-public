#ifndef WATCHER_INPUT_ROUTER_H
#define WATCHER_INPUT_ROUTER_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    WATCHER_INPUT_CONTEXT_UNSPECIFIED = 0,
    WATCHER_INPUT_CONTEXT_LVGL_NAV,
    WATCHER_INPUT_CONTEXT_APP_ACTION,
    WATCHER_INPUT_CONTEXT_SYSTEM_ONLY,
    WATCHER_INPUT_CONTEXT_APP_EVENT,
} watcher_input_context_t;

typedef enum {
    WATCHER_INPUT_OWNER_NONE = 0,
    WATCHER_INPUT_OWNER_LVGL,
    WATCHER_INPUT_OWNER_APP,
    WATCHER_INPUT_OWNER_SYSTEM,
} watcher_input_owner_t;

typedef enum {
    WATCHER_INPUT_EVENT_NONE = 0,
    WATCHER_INPUT_EVENT_ROTATE,
    WATCHER_INPUT_EVENT_BUTTON_DOWN,
    WATCHER_INPUT_EVENT_SHORT_CLICK,
    WATCHER_INPUT_EVENT_LONG_HOLD,
    WATCHER_INPUT_EVENT_REJECTED,
} watcher_input_event_t;

typedef enum {
    WATCHER_INPUT_REJECT_NONE = 0,
    WATCHER_INPUT_REJECT_NOT_PRESSED,
    WATCHER_INPUT_REJECT_CONTEXT_CHANGED,
    WATCHER_INPUT_REJECT_TOO_LONG,
    WATCHER_INPUT_REJECT_SYSTEM_ONLY,
    WATCHER_INPUT_REJECT_LONG_ALREADY_FIRED,
} watcher_input_reject_t;

typedef struct {
    watcher_input_context_t context;
    uintptr_t owner_token;
} watcher_input_scope_t;

typedef struct {
    uint32_t short_click_max_ms;
} watcher_input_router_config_t;

typedef struct {
    watcher_input_event_t event;
    watcher_input_owner_t owner;
    watcher_input_reject_t reject;
    watcher_input_scope_t scope;
    uint32_t duration_ms;
    int32_t rotate_diff;
} watcher_input_result_t;

typedef struct {
    watcher_input_router_config_t config;
    watcher_input_scope_t scope;
    watcher_input_scope_t press_scope;
    watcher_input_scope_t pending_app_scope;
    watcher_input_scope_t pending_app_rotation_scope;
    uint32_t press_down_ms;
    int32_t pending_app_rotation_diff;
    uint8_t pending_app_clicks;
    bool button_pressed;
    bool long_hold_fired;
    bool lvgl_cancel_pending;
} watcher_input_router_t;

typedef watcher_input_scope_t (*watcher_input_scope_provider_t)(void *user_ctx);

watcher_input_router_config_t watcher_input_router_default_config(void);
int32_t watcher_input_encoder_count_delta(int32_t previous_count, int32_t current_count, bool wrapped_high,
                                          bool wrapped_low);
void watcher_input_router_init(watcher_input_router_t *router, const watcher_input_router_config_t *config,
                               watcher_input_scope_t initial_scope);
void watcher_input_router_set_scope(watcher_input_router_t *router, watcher_input_scope_t scope);
watcher_input_result_t watcher_input_router_on_rotate(watcher_input_router_t *router, watcher_input_scope_t scope,
                                                      int32_t diff);
watcher_input_result_t watcher_input_router_on_button_down(watcher_input_router_t *router, watcher_input_scope_t scope,
                                                           uint32_t now_ms);
watcher_input_result_t watcher_input_router_on_button_up(watcher_input_router_t *router, watcher_input_scope_t scope,
                                                         uint32_t now_ms);
watcher_input_result_t watcher_input_router_on_long_hold(watcher_input_router_t *router, watcher_input_scope_t scope,
                                                         uint32_t now_ms);
bool watcher_input_router_lvgl_button_pressed(watcher_input_router_t *router, watcher_input_scope_t scope,
                                              bool *cancelled);
bool watcher_input_router_consume_app_click(watcher_input_router_t *router, watcher_input_scope_t scope);
bool watcher_input_router_consume_app_rotation(watcher_input_router_t *router, watcher_input_scope_t scope,
                                               int32_t *out_diff);

void watcher_input_router_global_init(watcher_input_scope_t initial_scope);
void watcher_input_router_global_set_scope_provider(watcher_input_scope_provider_t provider, void *user_ctx);
watcher_input_result_t watcher_input_router_global_on_rotate(int32_t diff);
watcher_input_result_t watcher_input_router_global_on_button_down(uint32_t now_ms);
watcher_input_result_t watcher_input_router_global_on_button_up(uint32_t now_ms);
watcher_input_result_t watcher_input_router_global_on_long_hold(uint32_t now_ms);
bool watcher_input_router_global_lvgl_button_pressed(bool *cancelled);
bool watcher_input_router_global_consume_app_click(void);
bool watcher_input_router_global_consume_app_rotation(int32_t *out_diff);
void watcher_input_router_global_clear_pending(void);

const char *watcher_input_context_name(watcher_input_context_t context);
const char *watcher_input_owner_name(watcher_input_owner_t owner);
const char *watcher_input_event_name(watcher_input_event_t event);
const char *watcher_input_reject_name(watcher_input_reject_t reject);

#ifdef __cplusplus
}
#endif

#endif /* WATCHER_INPUT_ROUTER_H */
