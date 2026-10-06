#include "watcher_input_router.h"

#include <limits.h>
#include <stddef.h>
#include <string.h>

#if !defined(WATCHER_INPUT_ROUTER_HOST_TEST)
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
#endif

#define WATCHER_INPUT_SHORT_CLICK_MAX_MS 1200U

#if defined(WATCHER_INPUT_ROUTER_HOST_TEST)
#define ROUTER_LOCK()
#define ROUTER_UNLOCK()
#else
static portMUX_TYPE s_global_lock = portMUX_INITIALIZER_UNLOCKED;
#define ROUTER_LOCK() portENTER_CRITICAL(&s_global_lock)
#define ROUTER_UNLOCK() portEXIT_CRITICAL(&s_global_lock)
#endif

static watcher_input_router_t s_global_router;
static watcher_input_scope_provider_t s_scope_provider = NULL;
static void *s_scope_provider_ctx = NULL;
static bool s_global_initialized = false;

static bool scope_is_equal(watcher_input_scope_t lhs, watcher_input_scope_t rhs) {
    return lhs.context == rhs.context && lhs.owner_token == rhs.owner_token;
}

static watcher_input_owner_t short_click_owner(watcher_input_context_t context) {
    switch (context) {
    case WATCHER_INPUT_CONTEXT_LVGL_NAV:
        return WATCHER_INPUT_OWNER_LVGL;
    case WATCHER_INPUT_CONTEXT_APP_ACTION:
    case WATCHER_INPUT_CONTEXT_APP_EVENT:
        return WATCHER_INPUT_OWNER_APP;
    case WATCHER_INPUT_CONTEXT_SYSTEM_ONLY:
    default:
        return WATCHER_INPUT_OWNER_NONE;
    }
}

static watcher_input_result_t make_result(watcher_input_scope_t scope, watcher_input_event_t event,
                                          watcher_input_owner_t owner, watcher_input_reject_t reject,
                                          uint32_t duration_ms, int32_t rotate_diff) {
    const watcher_input_result_t result = {
        .event = event,
        .owner = owner,
        .reject = reject,
        .scope = scope,
        .duration_ms = duration_ms,
        .rotate_diff = rotate_diff,
    };
    return result;
}

watcher_input_router_config_t watcher_input_router_default_config(void) {
    const watcher_input_router_config_t config = {
        .short_click_max_ms = WATCHER_INPUT_SHORT_CLICK_MAX_MS,
    };
    return config;
}

int32_t watcher_input_encoder_count_delta(int32_t previous_count, int32_t current_count, bool wrapped_high,
                                          bool wrapped_low) {
    int64_t delta;

    if (previous_count == current_count) {
        return 0;
    }
    if (wrapped_high && !wrapped_low) {
        return 1;
    }
    if (wrapped_low && !wrapped_high) {
        return -1;
    }

    delta = (int64_t)current_count - (int64_t)previous_count;
    if (delta > INT32_MAX) {
        return INT32_MAX;
    }
    if (delta < INT32_MIN) {
        return INT32_MIN;
    }
    return (int32_t)delta;
}

void watcher_input_router_init(watcher_input_router_t *router, const watcher_input_router_config_t *config,
                               watcher_input_scope_t initial_scope) {
    if (router == NULL) {
        return;
    }

    memset(router, 0, sizeof(*router));
    router->config = config != NULL ? *config : watcher_input_router_default_config();
    router->scope = initial_scope;
}

void watcher_input_router_set_scope(watcher_input_router_t *router, watcher_input_scope_t scope) {
    if (router == NULL || scope_is_equal(router->scope, scope)) {
        return;
    }

    if (router->button_pressed && router->press_scope.context == WATCHER_INPUT_CONTEXT_LVGL_NAV) {
        router->lvgl_cancel_pending = true;
    }
    router->scope = scope;
    router->button_pressed = false;
    router->long_hold_fired = false;
    router->pending_app_clicks = 0;
    memset(&router->press_scope, 0, sizeof(router->press_scope));
    memset(&router->pending_app_scope, 0, sizeof(router->pending_app_scope));
    memset(&router->pending_app_rotation_scope, 0, sizeof(router->pending_app_rotation_scope));
    router->pending_app_rotation_diff = 0;
}

watcher_input_result_t watcher_input_router_on_rotate(watcher_input_router_t *router, watcher_input_scope_t scope,
                                                      int32_t diff) {
    if (router == NULL || diff == 0) {
        return make_result(scope, WATCHER_INPUT_EVENT_NONE, WATCHER_INPUT_OWNER_NONE, WATCHER_INPUT_REJECT_NONE, 0,
                           diff);
    }

    watcher_input_router_set_scope(router, scope);
    watcher_input_owner_t owner = WATCHER_INPUT_OWNER_NONE;
    if (scope.context == WATCHER_INPUT_CONTEXT_APP_EVENT) {
        int64_t total;
        if (!scope_is_equal(router->pending_app_rotation_scope, scope)) {
            router->pending_app_rotation_diff = 0;
            router->pending_app_rotation_scope = scope;
        }
        total = (int64_t)router->pending_app_rotation_diff + diff;
        if (total > INT32_MAX) {
            total = INT32_MAX;
        } else if (total < INT32_MIN) {
            total = INT32_MIN;
        }
        router->pending_app_rotation_diff = (int32_t)total;
        owner = WATCHER_INPUT_OWNER_APP;
    } else if (scope.context == WATCHER_INPUT_CONTEXT_LVGL_NAV || scope.context == WATCHER_INPUT_CONTEXT_APP_ACTION) {
        owner = WATCHER_INPUT_OWNER_LVGL;
    }
    return make_result(scope, WATCHER_INPUT_EVENT_ROTATE, owner, WATCHER_INPUT_REJECT_NONE, 0, diff);
}

watcher_input_result_t watcher_input_router_on_button_down(watcher_input_router_t *router, watcher_input_scope_t scope,
                                                           uint32_t now_ms) {
    if (router == NULL) {
        return make_result(scope, WATCHER_INPUT_EVENT_REJECTED, WATCHER_INPUT_OWNER_NONE,
                           WATCHER_INPUT_REJECT_NOT_PRESSED, 0, 0);
    }

    watcher_input_router_set_scope(router, scope);
    router->button_pressed = true;
    router->long_hold_fired = false;
    router->press_down_ms = now_ms;
    router->press_scope = scope;
    return make_result(scope, WATCHER_INPUT_EVENT_BUTTON_DOWN, short_click_owner(scope.context),
                       WATCHER_INPUT_REJECT_NONE, 0, 0);
}

watcher_input_result_t watcher_input_router_on_button_up(watcher_input_router_t *router, watcher_input_scope_t scope,
                                                         uint32_t now_ms) {
    if (router == NULL) {
        return make_result(scope, WATCHER_INPUT_EVENT_REJECTED, WATCHER_INPUT_OWNER_NONE,
                           WATCHER_INPUT_REJECT_NOT_PRESSED, 0, 0);
    }

    if (!scope_is_equal(router->scope, scope)) {
        const bool had_press = router->button_pressed;
        watcher_input_router_set_scope(router, scope);
        return make_result(scope, WATCHER_INPUT_EVENT_REJECTED, WATCHER_INPUT_OWNER_NONE,
                           had_press ? WATCHER_INPUT_REJECT_CONTEXT_CHANGED : WATCHER_INPUT_REJECT_NOT_PRESSED, 0, 0);
    }
    if (!router->button_pressed || !scope_is_equal(router->press_scope, scope)) {
        return make_result(scope, WATCHER_INPUT_EVENT_REJECTED, WATCHER_INPUT_OWNER_NONE,
                           WATCHER_INPUT_REJECT_NOT_PRESSED, 0, 0);
    }

    const uint32_t duration_ms = now_ms - router->press_down_ms;
    router->button_pressed = false;
    if (router->long_hold_fired) {
        router->long_hold_fired = false;
        return make_result(scope, WATCHER_INPUT_EVENT_REJECTED, WATCHER_INPUT_OWNER_NONE,
                           WATCHER_INPUT_REJECT_LONG_ALREADY_FIRED, duration_ms, 0);
    }
    if (duration_ms > router->config.short_click_max_ms) {
        return make_result(scope, WATCHER_INPUT_EVENT_REJECTED, WATCHER_INPUT_OWNER_NONE, WATCHER_INPUT_REJECT_TOO_LONG,
                           duration_ms, 0);
    }

    const watcher_input_owner_t owner = short_click_owner(scope.context);
    if (owner == WATCHER_INPUT_OWNER_NONE) {
        return make_result(scope, WATCHER_INPUT_EVENT_REJECTED, WATCHER_INPUT_OWNER_NONE,
                           WATCHER_INPUT_REJECT_SYSTEM_ONLY, duration_ms, 0);
    }
    if (owner == WATCHER_INPUT_OWNER_APP) {
        if (!scope_is_equal(router->pending_app_scope, scope)) {
            router->pending_app_clicks = 0;
            router->pending_app_scope = scope;
        }
        if (router->pending_app_clicks < UINT8_MAX) {
            ++router->pending_app_clicks;
        }
    }
    return make_result(scope, WATCHER_INPUT_EVENT_SHORT_CLICK, owner, WATCHER_INPUT_REJECT_NONE, duration_ms, 0);
}

watcher_input_result_t watcher_input_router_on_long_hold(watcher_input_router_t *router, watcher_input_scope_t scope,
                                                         uint32_t now_ms) {
    if (router == NULL || !scope_is_equal(router->scope, scope) || !router->button_pressed ||
        !scope_is_equal(router->press_scope, scope)) {
        if (router != NULL && !scope_is_equal(router->scope, scope)) {
            watcher_input_router_set_scope(router, scope);
        }
        return make_result(scope, WATCHER_INPUT_EVENT_REJECTED, WATCHER_INPUT_OWNER_NONE,
                           WATCHER_INPUT_REJECT_NOT_PRESSED, 0, 0);
    }
    if (router->long_hold_fired) {
        return make_result(scope, WATCHER_INPUT_EVENT_REJECTED, WATCHER_INPUT_OWNER_NONE,
                           WATCHER_INPUT_REJECT_LONG_ALREADY_FIRED, now_ms - router->press_down_ms, 0);
    }

    router->long_hold_fired = true;
    router->pending_app_clicks = 0;
    if (scope.context == WATCHER_INPUT_CONTEXT_LVGL_NAV) {
        router->lvgl_cancel_pending = true;
    }
    return make_result(scope, WATCHER_INPUT_EVENT_LONG_HOLD, WATCHER_INPUT_OWNER_SYSTEM, WATCHER_INPUT_REJECT_NONE,
                       now_ms - router->press_down_ms, 0);
}

bool watcher_input_router_lvgl_button_pressed(watcher_input_router_t *router, watcher_input_scope_t scope,
                                              bool *cancelled) {
    if (cancelled != NULL) {
        *cancelled = false;
    }
    if (router == NULL) {
        return false;
    }

    watcher_input_router_set_scope(router, scope);
    if (cancelled != NULL && router->lvgl_cancel_pending) {
        *cancelled = true;
    }
    router->lvgl_cancel_pending = false;
    return router->button_pressed && !router->long_hold_fired &&
           router->press_scope.context == WATCHER_INPUT_CONTEXT_LVGL_NAV && scope_is_equal(router->press_scope, scope);
}

bool watcher_input_router_consume_app_click(watcher_input_router_t *router, watcher_input_scope_t scope) {
    if (router == NULL) {
        return false;
    }

    watcher_input_router_set_scope(router, scope);
    if ((scope.context != WATCHER_INPUT_CONTEXT_APP_ACTION && scope.context != WATCHER_INPUT_CONTEXT_APP_EVENT) ||
        router->pending_app_clicks == 0 || !scope_is_equal(router->pending_app_scope, scope)) {
        router->pending_app_clicks = 0;
        return false;
    }

    --router->pending_app_clicks;
    return true;
}

bool watcher_input_router_consume_app_rotation(watcher_input_router_t *router, watcher_input_scope_t scope,
                                               int32_t *out_diff) {
    if (out_diff != NULL) {
        *out_diff = 0;
    }
    if (router == NULL || out_diff == NULL) {
        return false;
    }

    watcher_input_router_set_scope(router, scope);
    if (scope.context != WATCHER_INPUT_CONTEXT_APP_EVENT || router->pending_app_rotation_diff == 0 ||
        !scope_is_equal(router->pending_app_rotation_scope, scope)) {
        router->pending_app_rotation_diff = 0;
        return false;
    }

    *out_diff = router->pending_app_rotation_diff;
    router->pending_app_rotation_diff = 0;
    return true;
}

static watcher_input_scope_t current_global_scope(void) {
    watcher_input_scope_provider_t provider = s_scope_provider;
    if (provider != NULL) {
        return provider(s_scope_provider_ctx);
    }
    return s_global_router.scope;
}

static void ensure_global_initialized(watcher_input_scope_t scope) {
    if (!s_global_initialized) {
        watcher_input_router_init(&s_global_router, NULL, scope);
        s_global_initialized = true;
    }
}

void watcher_input_router_global_init(watcher_input_scope_t initial_scope) {
    ROUTER_LOCK();
    watcher_input_router_init(&s_global_router, NULL, initial_scope);
    s_global_initialized = true;
    ROUTER_UNLOCK();
}

void watcher_input_router_global_set_scope_provider(watcher_input_scope_provider_t provider, void *user_ctx) {
    ROUTER_LOCK();
    s_scope_provider = provider;
    s_scope_provider_ctx = user_ctx;
    ROUTER_UNLOCK();
}

watcher_input_result_t watcher_input_router_global_on_rotate(int32_t diff) {
    const watcher_input_scope_t scope = current_global_scope();
    watcher_input_result_t result;
    ROUTER_LOCK();
    ensure_global_initialized(scope);
    result = watcher_input_router_on_rotate(&s_global_router, scope, diff);
    ROUTER_UNLOCK();
    return result;
}

watcher_input_result_t watcher_input_router_global_on_button_down(uint32_t now_ms) {
    const watcher_input_scope_t scope = current_global_scope();
    watcher_input_result_t result;
    ROUTER_LOCK();
    ensure_global_initialized(scope);
    result = watcher_input_router_on_button_down(&s_global_router, scope, now_ms);
    ROUTER_UNLOCK();
    return result;
}

watcher_input_result_t watcher_input_router_global_on_button_up(uint32_t now_ms) {
    const watcher_input_scope_t scope = current_global_scope();
    watcher_input_result_t result;
    ROUTER_LOCK();
    ensure_global_initialized(scope);
    result = watcher_input_router_on_button_up(&s_global_router, scope, now_ms);
    ROUTER_UNLOCK();
    return result;
}

watcher_input_result_t watcher_input_router_global_on_long_hold(uint32_t now_ms) {
    const watcher_input_scope_t scope = current_global_scope();
    watcher_input_result_t result;
    ROUTER_LOCK();
    ensure_global_initialized(scope);
    result = watcher_input_router_on_long_hold(&s_global_router, scope, now_ms);
    ROUTER_UNLOCK();
    return result;
}

bool watcher_input_router_global_lvgl_button_pressed(bool *cancelled) {
    const watcher_input_scope_t scope = current_global_scope();
    bool pressed;
    ROUTER_LOCK();
    ensure_global_initialized(scope);
    pressed = watcher_input_router_lvgl_button_pressed(&s_global_router, scope, cancelled);
    ROUTER_UNLOCK();
    return pressed;
}

bool watcher_input_router_global_consume_app_click(void) {
    const watcher_input_scope_t scope = current_global_scope();
    bool consumed;
    ROUTER_LOCK();
    ensure_global_initialized(scope);
    consumed = watcher_input_router_consume_app_click(&s_global_router, scope);
    ROUTER_UNLOCK();
    return consumed;
}

bool watcher_input_router_global_consume_app_rotation(int32_t *out_diff) {
    const watcher_input_scope_t scope = current_global_scope();
    bool consumed;
    ROUTER_LOCK();
    ensure_global_initialized(scope);
    consumed = watcher_input_router_consume_app_rotation(&s_global_router, scope, out_diff);
    ROUTER_UNLOCK();
    return consumed;
}

void watcher_input_router_global_clear_pending(void) {
    const watcher_input_scope_t scope = current_global_scope();
    ROUTER_LOCK();
    ensure_global_initialized(scope);
    watcher_input_router_set_scope(&s_global_router, scope);
    s_global_router.pending_app_clicks = 0;
    s_global_router.pending_app_rotation_diff = 0;
    ROUTER_UNLOCK();
}

const char *watcher_input_context_name(watcher_input_context_t context) {
    switch (context) {
    case WATCHER_INPUT_CONTEXT_UNSPECIFIED:
        return "unspecified";
    case WATCHER_INPUT_CONTEXT_LVGL_NAV:
        return "lvgl_nav";
    case WATCHER_INPUT_CONTEXT_APP_ACTION:
        return "app_action";
    case WATCHER_INPUT_CONTEXT_SYSTEM_ONLY:
        return "system_only";
    case WATCHER_INPUT_CONTEXT_APP_EVENT:
        return "app_event";
    default:
        return "unknown";
    }
}

const char *watcher_input_owner_name(watcher_input_owner_t owner) {
    switch (owner) {
    case WATCHER_INPUT_OWNER_NONE:
        return "none";
    case WATCHER_INPUT_OWNER_LVGL:
        return "lvgl";
    case WATCHER_INPUT_OWNER_APP:
        return "app";
    case WATCHER_INPUT_OWNER_SYSTEM:
        return "system";
    default:
        return "unknown";
    }
}

const char *watcher_input_event_name(watcher_input_event_t event) {
    switch (event) {
    case WATCHER_INPUT_EVENT_NONE:
        return "none";
    case WATCHER_INPUT_EVENT_ROTATE:
        return "rotate";
    case WATCHER_INPUT_EVENT_BUTTON_DOWN:
        return "button_down";
    case WATCHER_INPUT_EVENT_SHORT_CLICK:
        return "short_click";
    case WATCHER_INPUT_EVENT_LONG_HOLD:
        return "long_hold";
    case WATCHER_INPUT_EVENT_REJECTED:
        return "rejected";
    default:
        return "unknown";
    }
}

const char *watcher_input_reject_name(watcher_input_reject_t reject) {
    switch (reject) {
    case WATCHER_INPUT_REJECT_NONE:
        return "none";
    case WATCHER_INPUT_REJECT_NOT_PRESSED:
        return "not_pressed";
    case WATCHER_INPUT_REJECT_CONTEXT_CHANGED:
        return "context_changed";
    case WATCHER_INPUT_REJECT_TOO_LONG:
        return "too_long";
    case WATCHER_INPUT_REJECT_SYSTEM_ONLY:
        return "system_only";
    case WATCHER_INPUT_REJECT_LONG_ALREADY_FIRED:
        return "long_already_fired";
    default:
        return "unknown";
    }
}

