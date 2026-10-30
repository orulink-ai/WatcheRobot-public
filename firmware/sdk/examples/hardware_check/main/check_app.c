#include "check_app.h"
#include "check_body.h"
#include "check_media.h"
#include "check_view.h"
#include "demo_audio.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/task.h"
#include "watcher_hw_display.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "hardware_check";
static QueueHandle_t inputs, media_events, diagnostics;
static check_view_model_t view;
static check_view_model_t rendered;
static hc_body_t body;
static unsigned head_modules, combined_touches;
static unsigned pending_volume;
static bool body_owner, media_owner, return_report;
static int64_t screen_deadline, last_light, last_status;
typedef enum { DIAG_STATUS, DIAG_PHOTO, DIAG_STREAM, DIAG_RECORD, DIAG_CANCEL, DIAG_RESET } diagnostic_t;

static void status(const char *text) {
    snprintf(view.status, sizeof(view.status), "%s", text);
}
static void report(void) {
    view.page = CHECK_REPORT_PAGE;
    view.report_page = 0;
    view.full_image = false;
    status("Tap a test to inspect");
    ESP_LOGI(TAG, "report results=%u,%u,%u,%u,%u,%u,%u,%u (human confirmation only)", view.flow.results[0],
             view.flow.results[1], view.flow.results[2], view.flow.results[3], view.flow.results[4],
             view.flow.results[5], view.flow.results[6], view.flow.results[7]);
}
static const char *ready_hint(void) {
    const hc_item_t *item = hc_current_item(&view.flow);
    switch (item->operation) {
    case HC_OP_SCREEN:
        return "Tap 1, then 2, then 3";
    case HC_OP_PHOTO:
        return "Point at an object";
    case HC_OP_STREAM:
        return "Live view for 5 seconds";
    case HC_OP_VOICE:
        return "Listen for one, two, three";
    case HC_OP_TONE:
        return "One short tone";
    case HC_OP_RECORD:
        return "Wait for REC, then speak";
    case HC_OP_REPLAY:
        return "Listen to your recording";
    case HC_OP_TOUCH:
        return "Tap, release; hold, release";
    case HC_OP_LIGHT:
        return item->title;
    case HC_OP_MOVE:
        return item->argument ? "Small Y move (3 degrees)" : "Small X move (3 degrees)";
    case HC_OP_STOP_TEST:
        return "Slow move; tap STOP";
    default:
        return "20 seconds, no motor";
    }
}
static const char *review_hint(void) {
    switch (hc_current_item(&view.flow)->operation) {
    case HC_OP_SCREEN:
        return "Clear screen? Taps worked?";
    case HC_OP_PHOTO:
        return "Clear, new photo?";
    case HC_OP_STREAM:
        return "Did the picture update?";
    case HC_OP_VOICE:
    case HC_OP_TONE:
        return "Heard it clearly?";
    case HC_OP_RECORD:
        return "Level followed your voice?";
    case HC_OP_REPLAY:
        return "Clear voice, no squeal?";
    case HC_OP_TOUCH:
        return "Events match your touches?";
    case HC_OP_LIGHT:
        return "Correct zone and effect?";
    case HC_OP_STOP_TEST:
        return "Did movement STOP?";
    case HC_OP_MOVE:
        return "Small, safe movement?";
    default:
        return "Picture, sound, touch OK?";
    }
}
static void refresh_hint(void) {
    if (view.flow.phase == HC_IDLE)
        status(ready_hint());
    else if (view.flow.phase == HC_UNAVAILABLE)
        status(view.flow.body_fault ? "Locked. Restart resets results" : "Module not ready; Next skips");
    else if (view.flow.phase == HC_REVIEW)
        status(review_hint());
    else if (view.flow.phase == HC_ISSUE)
        status("Issue kept. Back to retry");
    else if (view.flow.phase == HC_PASSED)
        status(view.flow.results[view.flow.step] == HC_PASS ? "Confirmed. Next test" : "Issue / skip kept. Next test");
}
static esp_err_t clear_lights(void) {
    esp_err_t ret = hc_body_lights_off(&body);
    if (ret != ESP_OK) {
        view.flow.failed[view.flow.step] |= 1u << view.flow.item;
        view.flow.results[view.flow.step] = HC_FAIL;
        view.flow.body_fault = true;
        ESP_LOGE(TAG, "cleanup light=%s; body controls locked", esp_err_to_name(ret));
    }
    return ret;
}
static void finish(uint32_t attempt, esp_err_t error, bool cancelled) {
    if (attempt != view.flow.attempt || !hc_flow_busy(&view.flow))
        return;
    if (view.flow.step == HC_COMBINED || error || cancelled) {
        esp_err_t cleanup = clear_lights();
        if (error == ESP_OK)
            error = cleanup;
    }
    hc_flow_finish(&view.flow, attempt, error, cancelled);
    view.progress = 100;
    refresh_hint();
    ESP_LOGI(TAG, "test=%u item=%u attempt=%lu phase=%u error=%s cancelled=%d", view.flow.step, view.flow.item,
             (unsigned long)attempt, view.flow.phase, esp_err_to_name(error), cancelled);
    if (error && view.flow.phase == HC_ISSUE) {
        if (error == ESP_ERR_TIMEOUT)
            status(view.flow.step == HC_MOTION ? "No motion result. Check logs" : "Timed out. Back to retry");
        else if (view.flow.step == HC_MIC && view.flow.item == 1)
            status("Record first, then Replay");
        else
            status("Failed. Back to retry; see logs");
    }
}
static void cancel(void) {
    if (view.flow.phase != HC_RUNNING)
        return;
    if (body_owner && body.operation == HC_OP_STOP_TEST && body.moving) {
        if (body.stop_submitted) {
            status("Stop requested; waiting");
            return;
        }
        esp_err_t ret = hc_body_stop_test(&body);
        status(ret == ESP_OK ? "Stop requested; waiting" : "Stop delivery failed");
        return; /* STOP test remains running until matched STOPPED. */
    }
    hc_flow_cancel(&view.flow);
    status("Cleaning up");
    if (media_owner)
        check_media_cancel();
    else if (body_owner)
        hc_body_cancel(&body);
    else
        finish(view.flow.attempt, ESP_OK, true);
}
static void begin(bool motion_confirmed) {
    if (media_owner || body_owner || view.setting_pending || !hc_flow_begin(&view.flow, motion_confirmed))
        return;
    const hc_item_t *item = hc_current_item(&view.flow);
    view.page = CHECK_TEST_PAGE;
    view.full_image = false;
    view.progress = view.targets = view.frames = view.meter = 0;
    status("Starting");
    ESP_LOGI(TAG, "begin test=%u item=%u attempt=%lu op=%u", view.flow.step, view.flow.item,
             (unsigned long)view.flow.attempt, item->operation);
    if (item->operation == HC_OP_SCREEN) {
        screen_deadline = esp_timer_get_time() + 30000000;
        status("Tap 1, then 2, then 3");
    } else if (item->operation == HC_OP_TOUCH || item->operation == HC_OP_LIGHT || item->operation == HC_OP_MOVE ||
               item->operation == HC_OP_STOP_TEST) {
        body_owner = true;
        esp_err_t ret = hc_body_start(&body, item->operation, item->argument, view.flow.attempt, esp_timer_get_time());
        if (ret != ESP_OK) {
            body_owner = false;
            finish(view.flow.attempt, ret, false);
        } else
            status(item->operation == HC_OP_TOUCH   ? "Tap, release; hold, release"
                   : item->operation == HC_OP_LIGHT ? item->title
                                                    : "Reference command X90 / Y120 (no sensor)");
    } else {
        if (item->operation == HC_OP_COMBINED) {
            combined_touches = 0;
            last_light = esp_timer_get_time();
            esp_err_t ret = hc_body_start(&body, HC_OP_LIGHT, 1, view.flow.attempt, last_light);
            if (ret != ESP_OK) {
                finish(view.flow.attempt, ret, false);
                return;
            }
        }
        check_media_command_t command = {item->operation, view.flow.attempt, item->argument, view.volume};
        media_owner = check_media_submit(&command);
        if (!media_owner)
            finish(view.flow.attempt, ESP_ERR_NO_MEM, false);
    }
}
static void body_event(const watcher_hw_event_t *event, void *context) {
    (void)context;
    if (event->type == WATCHER_HW_EVENT_TOUCH && event->size == sizeof(mcu_touch_state_t)) {
        mcu_touch_state_t touch;
        memcpy(&touch, event->payload, sizeof(touch));
        ESP_LOGI(TAG, "touch sensor=%u event=%u seq=%lu", touch.touch_id, touch.event_code,
                 (unsigned long)event->sequence);
        if (view.flow.phase == HC_RUNNING && view.flow.step == HC_COMBINED)
            combined_touches++;
    }
    if (event->type == WATCHER_HW_EVENT_OVERFLOW) {
        view.flow.body_fault = true;
        ESP_LOGE(TAG, "event overflow; body controls locked until explicit reboot");
    }
    if (body_owner)
        hc_body_event(&body, event, esp_timer_get_time());
    /* LINK re-read is handled by tick, so a reconnect never replays intent. */
}
static void handle_media(check_media_event_t *event) {
    if (event->kind == CHECK_MEDIA_INIT) {
        head_modules = event->modules | HC_DISPLAY;
        return;
    }
    if (event->attempt != view.flow.attempt) {
        demo_image_release(&event->image);
        return;
    }
    if (event->kind == CHECK_MEDIA_IMAGE) {
        view.frames = event->frames;
        if (watcher_hw_display_lock(1000)) {
            check_view_image(&event->image, event->attempt);
            watcher_hw_display_unlock();
        }
        demo_image_release(&event->image);
    } else if (event->kind == CHECK_MEDIA_PROGRESS) {
        if (view.flow.phase == HC_RUNNING) {
            status(event->text);
            view.progress = event->progress;
            view.frames = event->frames;
            view.meter = demo_mic_meter(event->peak);
            if (view.flow.step == HC_COMBINED)
                snprintf(view.status, sizeof(view.status), "%uf | %ut | %us left", event->frames, combined_touches,
                         20 * (100 - (event->progress > 100 ? 100 : event->progress)) / 100);
        }
    } else if (event->kind == CHECK_MEDIA_DONE) {
        head_modules = event->modules | HC_DISPLAY;
        if (view.setting_pending) {
            view.setting_pending = false;
            if (event->error != ESP_OK) {
                status("Setting failed. Retry / reboot");
                view.page = CHECK_TEST_PAGE;
            } else {
                view.volume = pending_volume;
                status(view.volume ? "Set output, then Test" : "Muted. Raise output");
            }
        } else {
            media_owner = false;
            finish(event->attempt, event->error, view.flow.phase == HC_CANCELLING);
        }
    }
}
static void advance(void) {
    if (return_report) {
        return_report = false;
        report();
        return;
    }
    if (!hc_flow_next(&view.flow))
        report();
    else {
        view.page = CHECK_TEST_PAGE;
        refresh_hint();
    }
}
static void input(const check_input_t *event) {
    if (!hc_flow_accept_intent(&view.flow, event->attempt, event->phase, event->navigation,
                               check_view_navigation(&view)))
        return;
    hc_flow_t *f = &view.flow;
    if (event->kind == CHECK_TARGET && f->phase == HC_RUNNING && f->step == HC_SCREEN) {
        if (event->value == view.targets && ++view.targets == 3)
            finish(f->attempt, ESP_OK, false);
        return;
    }
    if (event->kind == CHECK_IMAGE && f->phase == HC_REVIEW && f->step == HC_CAMERA) {
        view.full_image = !view.full_image;
        return;
    }
    if (hc_flow_busy(f)) {
        if (event->kind == CHECK_RIGHT)
            cancel();
        return;
    }
    if (event->kind == CHECK_ROW) {
        if (view.page == CHECK_REPORT_PAGE) {
            view.detail_step = view.report_page * 4 + event->value;
            view.detail_page = 0;
            view.page = CHECK_DETAIL_PAGE;
            status("Tap an item to retest");
        } else if (view.page == CHECK_DETAIL_PAGE) {
            unsigned item = view.detail_page * 4 + event->value;
            if (hc_flow_select_item(f, view.detail_step, item)) {
                return_report = true;
                view.page = CHECK_TEST_PAGE;
                refresh_hint();
            }
        }
        return;
    }
    if (view.page == CHECK_AUDIO_PAGE && event->kind == CHECK_VOLUME && !view.setting_pending) {
        check_media_command_t command = {HC_OP_VOLUME, f->attempt, 0, event->value};
        if (check_media_submit(&command)) {
            view.setting_pending = true;
            pending_volume = event->value;
            status("Applying output");
        }
        return;
    }
    if (view.setting_pending)
        return;
    if (event->kind != CHECK_LEFT && event->kind != CHECK_RIGHT)
        return;
    bool right = event->kind == CHECK_RIGHT;
    if (view.full_image && !right) {
        view.full_image = false;
        return;
    }
    if (view.page == CHECK_REPORT_PAGE) {
        if (right && !view.report_page)
            view.report_page = 1;
        else if (!right && view.report_page)
            view.report_page = 0;
        else {
            view.page = CHECK_RESTART_PAGE;
            status("Test clears previous results");
        }
        return;
    }
    if (view.page == CHECK_DETAIL_PAGE) {
        if (!right) {
            if (view.detail_page)
                view.detail_page--;
            else
                report();
        } else if ((view.detail_page + 1) * 4 < hc_item_count(view.detail_step))
            view.detail_page++;
        else {
            hc_flow_select_item(f, view.detail_step, 0);
            f->confirmed[f->step] = f->failed[f->step] = f->skipped[f->step] = 0;
            f->results[f->step] = HC_UNTESTED;
            return_report = false;
            view.page = CHECK_TEST_PAGE;
            refresh_hint();
        }
        return;
    }
    if (view.page == CHECK_RESTART_PAGE) {
        if (!right)
            report();
        else {
            (void)clear_lights();
            hc_flow_restart(f);
            view.page = CHECK_TEST_PAGE;
            status("One test at a time");
        }
        return;
    }
    if (view.page == CHECK_SAFETY_PAGE || view.page == CHECK_AUDIO_PAGE) {
        bool motion = view.page == CHECK_SAFETY_PAGE;
        view.page = CHECK_TEST_PAGE;
        if (right)
            begin(motion);
        else
            refresh_hint();
        return;
    }
    if (f->phase == HC_WELCOME) {
        if (right) {
            hc_flow_select(f, HC_SCREEN);
            refresh_hint();
        }
        return;
    }
    if (f->phase == HC_REVIEW) {
        (void)clear_lights();
        view.full_image = false;
        if (right) {
            hc_flow_confirm(f);
            if (return_report) {
                return_report = false;
                report();
            } else
                refresh_hint();
        } else {
            hc_flow_reject(f);
            refresh_hint();
        }
    } else if (f->phase == HC_ISSUE) {
        if (right) {
            hc_flow_continue(f);
            if (f->phase == HC_PASSED)
                advance();
            else
                refresh_hint();
        } else {
            hc_flow_retry(f);
            refresh_hint();
        }
    } else if (f->phase == HC_UNAVAILABLE) {
        if (right) {
            hc_flow_skip(f);
            hc_flow_continue(f);
            while (f->phase == HC_UNAVAILABLE) {
                hc_flow_skip(f);
                hc_flow_continue(f);
            }
            advance();
        } else if (f->body_fault && (hc_current_item(f)->modules & HC_BODY))
            esp_restart();
        else if (return_report)
            report();
        else if (f->step) {
            hc_flow_select(f, f->step - 1);
            refresh_hint();
        }
    } else if (f->phase == HC_PASSED || f->phase == HC_SKIPPED) {
        if (right)
            advance();
        else {
            hc_flow_retry(f);
            refresh_hint();
        }
    } else if (f->phase == HC_IDLE) {
        if (!right) {
            (void)clear_lights();
            if (return_report) {
                return_report = false;
                report();
            } else if (f->step) {
                hc_flow_select(f, f->step - 1);
                refresh_hint();
            } else {
                view.page = CHECK_RESTART_PAGE;
                status("Test clears previous results");
            }
        } else if (f->step == HC_MOTION) {
            view.page = CHECK_SAFETY_PAGE;
            status("Clear space. Keep hands away");
        } else if (f->step == HC_SPEAKER || f->step == HC_MIC) {
            view.page = CHECK_AUDIO_PAGE;
            status("Set output, then Test");
        } else
            begin(false);
    }
}
static void tick(void) {
    int64_t now = esp_timer_get_time();
    watcher_hw_status_t state;
    unsigned modules = head_modules;
    if (watcher_hw_body_status(&state) == ESP_OK && state.state == WATCHER_HW_READY)
        modules |= HC_BODY;
    hc_flow_set_modules(&view.flow, modules, view.flow.body_fault);
    if (view.flow.phase == HC_CANCELLING) {
        if (media_owner)
            check_media_cancel();
        if (body_owner && !body.cancelled)
            hc_body_cancel(&body);
    }
    if (body_owner) {
        hc_body_tick(&body, now);
        if (body.finished) {
            body_owner = false;
            if (body.error != ESP_OK && (body.operation == HC_OP_MOVE || body.operation == HC_OP_STOP_TEST))
                view.flow.body_fault = true;
            finish(body.attempt, body.error, body.cancelled);
        } else if (body.operation == HC_OP_TOUCH) {
            static const char *const hints[] = {"Tap body sensor", "Release", "Press and hold",
                                                "Keep holding",    "Release", "All events received"};
            if (body.touch_stage)
                snprintf(view.status, sizeof(view.status), "ID %u | %s", body.touch_id,
                         hints[body.touch_stage < 6 ? body.touch_stage : 0]);
            else
                status(hints[0]);
            view.progress = body.touch_stage * 20;
        } else if (body.moving && !body.stop_submitted)
            snprintf(view.status, sizeof(view.status), "Target X %d / Y %d deg", body.x, body.y);
    }
    if (view.flow.phase == HC_RUNNING && view.flow.step == HC_SCREEN && now >= screen_deadline)
        finish(view.flow.attempt, ESP_ERR_TIMEOUT, false);
    if (view.flow.phase == HC_RUNNING && view.flow.step == HC_COMBINED && now - last_light >= 1500000) {
        last_light = now;
        esp_err_t ret = hc_body_start(&body, HC_OP_LIGHT, view.frames % 2 ? 1 : 8, view.flow.attempt, now);
        if (ret != ESP_OK) {
            hc_flow_cancel(&view.flow);
            view.flow.failed[HC_COMBINED] = 1;
            view.flow.results[HC_COMBINED] = HC_FAIL;
            check_media_cancel();
        }
    }
    if (view.flow.phase == HC_UNAVAILABLE)
        refresh_hint();
    /* Presentation fact only: STOP_TEST remains RUNNING until matching STOPPED. */
    view.stopping = view.flow.phase == HC_CANCELLING || (body_owner && body.stop_submitted && !body.finished);
    if (now - last_status >= 10000000) {
        last_status = now;
        ESP_LOGI(TAG, "state modules=%u body_fault=%d test=%u item=%u phase=%u frames=%u touch=%u free=%lu", modules,
                 view.flow.body_fault, view.flow.step, view.flow.item, view.flow.phase, view.frames, combined_touches,
                 (unsigned long)esp_get_free_heap_size());
    }
}
static void diagnostic(diagnostic_t command) {
    if (command == DIAG_STATUS) {
        last_status = 0;
        return;
    }
    if (command == DIAG_CANCEL) {
        cancel();
        return;
    }
    if (hc_flow_busy(&view.flow) || view.setting_pending)
        return;
    if (command == DIAG_RESET) {
        hc_flow_restart(&view.flow);
        view.page = CHECK_TEST_PAGE;
        status("One test at a time");
        return;
    }
    unsigned step = command == DIAG_RECORD ? HC_MIC : HC_CAMERA;
    unsigned item = command == DIAG_STREAM ? 1 : 0;
    hc_flow_select_item(&view.flow, step, item);
    begin(false); /* Diagnostic whitelist: no sound, lights or motor operations. */
}
static void controller(void *ctx) {
    (void)ctx;
    esp_err_t ret = watcher_hw_body_init();
    ESP_LOGI(TAG, "body init=%s", esp_err_to_name(ret));
    if (ret == ESP_OK)
        ESP_LOGI(TAG, "subscribe=%s", esp_err_to_name(watcher_hw_body_subscribe(body_event, NULL)));
    for (;;) {
        uint32_t count;
        (void)watcher_hw_body_dispatch_events(32, &count);
        check_media_event_t media;
        while (xQueueReceive(media_events, &media, 0) == pdTRUE)
            handle_media(&media);
        check_input_t event;
        while (xQueueReceive(inputs, &event, 0) == pdTRUE)
            input(&event);
        diagnostic_t command;
        if (xQueueReceive(diagnostics, &command, 0) == pdTRUE)
            diagnostic(command);
        tick();
        if (memcmp(&rendered, &view, sizeof(view)) && watcher_hw_display_lock(20)) {
            check_view_render(&view);
            rendered = view;
            watcher_hw_display_unlock();
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}
static void console(void *ctx) {
    (void)ctx;
    char line[40];
    unsigned used = 0;
    for (;;) {
        int c = getchar();
        if (c == EOF) {
            clearerr(stdin);
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }
        if (c == '\n' || c == '\r') {
            line[used] = 0;
            used = 0;
            if (!strcmp(line, "diag snapshot")) {
                check_view_snapshot();
                continue;
            }
            static const char *const names[] = {"diag status", "diag photo",  "diag stream",
                                                "diag record", "diag cancel", "diag reset"};
            for (unsigned i = 0; i < sizeof(names) / sizeof(names[0]); i++) {
                if (!strcmp(line, names[i])) {
                    diagnostic_t command = (diagnostic_t)i;
                    xQueueSend(diagnostics, &command, 0);
                    break;
                }
            }
        } else if (used + 1 < sizeof(line))
            line[used++] = (char)c;
        else
            used = 0;
    }
}
esp_err_t check_app_start(void) {
    if (inputs)
        return ESP_ERR_INVALID_STATE;
    hc_flow_init(&view.flow);
    hc_body_init(&body);
    view.volume = DEMO_OUTPUT_DEFAULT;
    status("One test at a time");
    esp_err_t ret = watcher_hw_display_init();
    if (ret != ESP_OK)
        return ret;
    head_modules = HC_DISPLAY;
    inputs = xQueueCreate(12, sizeof(check_input_t));
    media_events = xQueueCreate(12, sizeof(check_media_event_t));
    diagnostics = xQueueCreate(2, sizeof(diagnostic_t));
    if (!inputs || !media_events || !diagnostics)
        return ESP_ERR_NO_MEM;
    if (!watcher_hw_display_lock(1000))
        return ESP_ERR_TIMEOUT;
    check_view_create(inputs);
    check_view_render(&view);
    rendered = view;
    watcher_hw_display_unlock();
    if (xTaskCreate(controller, "check_control", 6144, NULL, 5, NULL) != pdPASS)
        return ESP_ERR_NO_MEM;
    ret = check_media_start(media_events);
    ESP_LOGI(TAG, "Hardware check v2 | curved zones | 8 groups / 26 items | media=%s | boot has no actuators",
             esp_err_to_name(ret));
    if (xTaskCreate(console, "check_console", 6144, NULL, 2, NULL) != pdPASS)
        ESP_LOGW(TAG, "diagnostic console unavailable");
    return ret;
}
