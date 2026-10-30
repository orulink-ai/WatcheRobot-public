#include "check_view.h"
#include "check_layout.h"
#include "demo_audio.h"
#include "demo_state.h"
#include "lvgl.h"
#include "esp_heap_caps.h"
#include "watcher_hw_display.h"
#include <stdio.h>
#include <string.h>

/* Fixed circular-safe geometry. No hardware calls or workflow decisions here. */
static QueueHandle_t inputs;
static check_view_model_t shown;
static lv_obj_t *heading, *eyebrow, *status, *visual, *picture, *buttons[2], *button_text[2], *bar, *meter;
static lv_obj_t *targets[3], *rows[4], *row_text[4], *slider, *volume_text, *replay_note;
static demo_image_t retained;
static lv_img_dsc_t descriptor;
static uint32_t image_attempt;
static const char *const titles[] = {"Screen", "Camera", "Speaker", "Microphone", "Body touch", "Lights", "Motion", "Together"};
/* Precomputed once, no per-frame allocation or ellipse math under the display lock. */
static struct { int16_t first, last; } zone_spans[CHECK_ZONE_HEIGHT];

static void zone_event(lv_event_t *event) {
    bool right = (uintptr_t)lv_event_get_user_data(event) != 0;
    lv_obj_t *obj = lv_event_get_target(event);
    lv_area_t bounds;
    lv_obj_get_coords(obj, &bounds);
    if (lv_event_get_code(event) == LV_EVENT_HIT_TEST) {
        lv_hit_test_info_t *hit = lv_event_get_hit_test_info(event);
        int x = hit->point->x - bounds.x1 + (right ? CHECK_SCREEN_SIZE - CHECK_ZONE_WIDTH : 0);
        int y = hit->point->y - bounds.y1 + CHECK_ZONE_TOP;
        hit->res = check_layout_zone_hit(right, x, y);
    } else if (lv_event_get_code(event) == LV_EVENT_DRAW_MAIN) {
        lv_draw_ctx_t *ctx = lv_event_get_draw_ctx(event);
        lv_draw_rect_dsc_t fill;
        lv_draw_rect_dsc_init(&fill);
        fill.bg_color = lv_obj_get_style_bg_color(obj, LV_PART_MAIN);
        for (unsigned row = 0; row < CHECK_ZONE_HEIGHT; row++) {
            int first = zone_spans[row].first, last = zone_spans[row].last;
            if (first < 0) continue;
            lv_area_t line = {
                .x1 = bounds.x1 + (right ? CHECK_ZONE_WIDTH - 1 - last : first),
                .x2 = bounds.x1 + (right ? CHECK_ZONE_WIDTH - 1 - first : last),
                .y1 = bounds.y1 + (int)row, .y2 = bounds.y1 + (int)row
            };
            lv_draw_rect(ctx, &fill, &line);
        }
        if (lv_obj_has_state(obj, LV_STATE_FOCUS_KEY)) {
            /* Inset focus ring around the label, not a new small touch target. */
            int x = right ? CHECK_ZONE_WIDTH - CHECK_LABEL_LEFT : CHECK_LABEL_LEFT;
            lv_area_t ring = {.x1 = bounds.x1 + x - 42, .x2 = bounds.x1 + x + 42,
                              .y1 = bounds.y1 + CHECK_LABEL_Y - CHECK_ZONE_TOP - 20,
                              .y2 = bounds.y1 + CHECK_LABEL_Y - CHECK_ZONE_TOP + 20};
            fill.bg_opa = LV_OPA_TRANSP;
            fill.border_width = 2;
            fill.border_color = lv_obj_get_style_text_color(obj, LV_PART_MAIN);
            fill.radius = 8;
            lv_draw_rect(ctx, &fill, &ring);
        }
    }
}
static void input(lv_event_t *event) {
    lv_obj_t *obj = lv_event_get_target(event);
    if (lv_obj_has_state(obj, LV_STATE_DISABLED)) return;
    uintptr_t encoded = (uintptr_t)lv_event_get_user_data(event);
    check_input_t message = {.kind = (check_input_kind_t)(encoded >> 8), .value = encoded & 255,
                             .attempt = shown.flow.attempt, .phase = shown.flow.phase,
                             .navigation = check_view_navigation(&shown)};
    if (message.kind == CHECK_LEFT || message.kind == CHECK_RIGHT) {
        /* LVGL 8 can send CLICKED to a captured object on a RELEASED sample
         * without a preceding move sample. Validate the final visible zone,
         * not only the point where the press began. Keep keypad/encoder input. */
        lv_indev_t *indev = lv_event_get_indev(event);
        if (indev && lv_indev_get_type(indev) == LV_INDEV_TYPE_POINTER) {
            lv_point_t point;
            lv_indev_get_point(indev, &point);
            if (!lv_obj_hit_test(obj, &point)) return;
        }
    }
    if (message.kind == CHECK_VOLUME) message.value = (unsigned)lv_slider_get_value(slider);
    xQueueSend(inputs, &message, 0); /* Dropped intent does not execute later. */
}
static void bind(lv_obj_t *obj, check_input_kind_t kind, unsigned value, lv_event_code_t code) {
    lv_obj_add_event_cb(obj, input, code, (void *)(uintptr_t)(((unsigned)kind << 8) | value));
}
static void visible(lv_obj_t *obj, bool show) {
    if (show) lv_obj_clear_flag(obj, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
}
static void enabled(lv_obj_t *obj, bool enable) {
    if (enable) lv_obj_clear_state(obj, LV_STATE_DISABLED);
    else lv_obj_add_state(obj, LV_STATE_DISABLED);
}
static lv_obj_t *text(lv_obj_t *parent, int y, int width, const lv_font_t *font) {
    lv_obj_t *label = lv_label_create(parent);
    lv_obj_set_width(label, width);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(0x293732), 0);
    lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
    lv_obj_align(label, LV_ALIGN_TOP_MID, 0, y);
    return label;
}
static void style_button(lv_obj_t *obj, bool primary) {
    lv_obj_set_style_radius(obj, 14, 0);
    lv_obj_set_style_shadow_width(obj, 0, 0);
    lv_obj_set_style_border_width(obj, primary ? 0 : 1, 0);
    lv_obj_set_style_border_color(obj, lv_color_hex(0xaab6ad), 0);
    lv_obj_set_style_bg_color(obj, lv_color_hex(primary ? 0x247568 : 0xe6eae5), 0);
    lv_obj_set_style_text_color(obj, lv_color_hex(primary ? 0xf4f1ec : 0x293732), 0);
    lv_obj_set_style_bg_color(obj, lv_color_hex(0xe6eae5), LV_STATE_DISABLED);
    lv_obj_set_style_text_color(obj, lv_color_hex(0x6a736c), LV_STATE_DISABLED);
    lv_obj_set_style_pad_all(obj, 0, 0);
}
static void style_zone(lv_obj_t *obj, bool primary, bool danger) {
    uint32_t surface = danger ? 0xb34132 : primary ? 0xcbe4d8 : 0xe6eae5;
    uint32_t ink = danger ? 0xf4f1ec : primary ? 0x247568 : 0x293732;
    lv_obj_set_style_bg_color(obj, lv_color_hex(surface), 0);
    lv_obj_set_style_text_color(obj, lv_color_hex(ink), 0);
    lv_obj_set_style_bg_color(obj, lv_color_hex(danger ? 0x8b3026 : primary ? 0xb0d3c2 : 0xd4dbd2), LV_STATE_PRESSED);
    lv_obj_set_style_bg_color(obj, lv_color_hex(danger ? 0xb34132 : 0xebeee8), LV_STATE_DISABLED);
    lv_obj_set_style_text_color(obj, lv_color_hex(danger ? 0xf4f1ec : 0x7a827b), LV_STATE_DISABLED);
}
void check_view_create(QueueHandle_t queue) {
    inputs = queue;
    lv_obj_t *screen = lv_scr_act();
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(screen, lv_color_hex(0xf4f1ec), 0);
    eyebrow = text(screen, 48, 256, &lv_font_montserrat_14);
    heading = text(screen, 80, 280, &lv_font_montserrat_24);
    visual = text(screen, 156, 256, &lv_font_montserrat_48);
    status = text(screen, 260, 280, &lv_font_montserrat_16);
    lv_label_set_long_mode(status, LV_LABEL_LONG_CLIP);
    picture = lv_img_create(screen);
    lv_obj_add_flag(picture, LV_OBJ_FLAG_CLICKABLE);
    bind(picture, CHECK_IMAGE, 0, LV_EVENT_CLICKED);
    bar = lv_bar_create(screen);
    lv_obj_set_size(bar, 176, 4); lv_obj_align(bar, LV_ALIGN_TOP_MID, 0, 246);
    lv_obj_set_style_bg_color(bar, lv_color_hex(0xe6eae5), 0);
    lv_obj_set_style_bg_color(bar, lv_color_hex(0x247568), LV_PART_INDICATOR);
    meter = lv_bar_create(screen);
    lv_obj_set_size(meter, 176, 10); lv_obj_align(meter, LV_ALIGN_TOP_MID, 0, 218);
    lv_obj_set_style_bg_color(meter, lv_color_hex(0xe6eae5), 0);
    lv_obj_set_style_bg_color(meter, lv_color_hex(0x247568), LV_PART_INDICATOR);
    for (unsigned row = 0; row < CHECK_ZONE_HEIGHT; row++) {
        int first, last;
        bool exists = check_layout_zone_span(CHECK_ZONE_TOP + (int)row, &first, &last);
        zone_spans[row].first = exists ? first : -1;
        zone_spans[row].last = exists ? last : -1;
    }
    for (unsigned i = 0; i < 2; i++) {
        buttons[i] = lv_btn_create(screen);
        lv_obj_remove_style_all(buttons[i]);
        lv_obj_set_size(buttons[i], CHECK_ZONE_WIDTH, CHECK_ZONE_HEIGHT);
        lv_obj_set_pos(buttons[i], i ? CHECK_SCREEN_SIZE - CHECK_ZONE_WIDTH : 0, CHECK_ZONE_TOP);
        lv_obj_clear_flag(buttons[i], LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_PRESS_LOCK);
        lv_obj_add_flag(buttons[i], LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_ADV_HITTEST);
        /* Standard rectangle painting is transparent; only the exact pixel shape is drawn. */
        lv_obj_set_style_bg_opa(buttons[i], LV_OPA_TRANSP, 0);
        style_zone(buttons[i], i == 1, false);
        lv_obj_add_event_cb(buttons[i], zone_event, LV_EVENT_HIT_TEST, (void *)(uintptr_t)i);
        lv_obj_add_event_cb(buttons[i], zone_event, LV_EVENT_DRAW_MAIN, (void *)(uintptr_t)i);
        button_text[i] = lv_label_create(buttons[i]);
        lv_obj_set_style_text_font(button_text[i], &lv_font_montserrat_18, 0);
        bind(buttons[i], i ? CHECK_RIGHT : CHECK_LEFT, 0, LV_EVENT_CLICKED);
    }
    const int x[] = {104, 256, 180}, y[] = {152, 152, 198};
    for (unsigned i = 0; i < 3; i++) {
        targets[i] = lv_btn_create(screen); style_button(targets[i], true);
        lv_obj_set_size(targets[i], 52, 52); lv_obj_set_pos(targets[i], x[i], y[i]);
        lv_obj_t *label = lv_label_create(targets[i]);
        lv_label_set_text_fmt(label, "%u", i + 1); lv_obj_center(label);
        bind(targets[i], CHECK_TARGET, i, LV_EVENT_CLICKED);
    }
    for (unsigned i = 0; i < 4; i++) {
        rows[i] = lv_btn_create(screen); style_button(rows[i], false);
        lv_obj_set_size(rows[i], 244, 41); lv_obj_align(rows[i], LV_ALIGN_TOP_MID, 0, 114 + (int)i * 41);
        lv_obj_set_style_radius(rows[i], 8, 0);
        row_text[i] = lv_label_create(rows[i]);
        lv_obj_set_style_text_font(row_text[i], &lv_font_montserrat_14, 0);
        bind(rows[i], CHECK_ROW, i, LV_EVENT_CLICKED);
    }
    volume_text = text(screen, 155, 240, &lv_font_montserrat_16);
    slider = lv_slider_create(screen); lv_obj_set_size(slider, 190, 14);
    lv_obj_align(slider, LV_ALIGN_TOP_MID, 0, 198); lv_slider_set_range(slider, 0, DEMO_OUTPUT_MAX);
    lv_obj_set_ext_click_area(slider, 14);
    lv_obj_set_style_bg_color(slider, lv_color_hex(0x247568), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(slider, lv_color_hex(0x247568), LV_PART_KNOB);
    bind(slider, CHECK_VOLUME, 0, LV_EVENT_RELEASED);
    replay_note = text(screen, 233, 240, &lv_font_montserrat_16);
    lv_label_set_text(replay_note, "Replay: 32x (limited)");
}
void check_view_image(demo_image_t *image, uint32_t attempt) {
    lv_img_cache_invalidate_src(&descriptor);
    demo_image_t previous = retained;
    retained = *image; memset(image, 0, sizeof(*image));
    descriptor = (lv_img_dsc_t){.header = {.cf = LV_IMG_CF_TRUE_COLOR, .w = retained.width, .h = retained.height},
                               .data_size = retained.bytes, .data = retained.pixels};
    lv_img_set_src(picture, &descriptor);
    image_attempt = attempt;
    demo_image_release(&previous);
}
void check_view_snapshot(void) {
    if (!watcher_hw_display_lock(1000)) return;
    uint32_t bytes = lv_snapshot_buf_size_needed(lv_scr_act(), LV_IMG_CF_TRUE_COLOR);
    uint8_t *pixels = heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    lv_img_dsc_t snap = {0};
    lv_res_t result = pixels ? lv_snapshot_take_to_buf(lv_scr_act(), LV_IMG_CF_TRUE_COLOR, &snap, pixels, bytes) : LV_RES_INV;
    lv_area_t status_bounds, button_bounds;
    lv_obj_get_coords(status, &status_bounds); lv_obj_get_coords(buttons[0], &button_bounds);
    printf("HC_LAYOUT axis=206 status_bottom=%d footer_top=%d overlap=%u\n", status_bounds.y2,
           button_bounds.y1, !lv_obj_has_flag(status, LV_OBJ_FLAG_HIDDEN) && status_bounds.y2 >= button_bounds.y1);
    watcher_hw_display_unlock();
    if (result == LV_RES_OK) {
        printf("HC_SNAPSHOT width=%u height=%u step=2 swap=%u\n", snap.header.w, snap.header.h, LV_COLOR_16_SWAP);
        const char hex[] = "0123456789abcdef";
        char row[4 * 206 + 1];
        /* Downsample for bounded UART diagnostic transfer, never on the
         * controller task or under the LVGL lock. No framebuffer persistence. */
        for (unsigned y = 0; y < snap.header.h && snap.header.w == 412; y += 2) {
            unsigned n = 0;
            for (unsigned x = 0; x < snap.header.w; x += 2) {
                unsigned offset = (y * snap.header.w + x) * 2;
                for (unsigned j = 0; j < 2; j++) {
                    row[n++] = hex[pixels[offset + j] >> 4]; row[n++] = hex[pixels[offset + j] & 15];
                }
            }
            row[n] = 0;
            printf("HC_PIX y=%u %s\n", y, row);
        }
        printf("HC_SNAPSHOT end\n");
    } else printf("HC_SNAPSHOT failed\n");
    heap_caps_free(pixels);
}
static const char *item_status(const hc_flow_t *flow, unsigned step, unsigned item) {
    unsigned bit = 1u << item;
    return flow->failed[step] & bit ? "Issue" : flow->skipped[step] & bit ? "Skip" :
        flow->confirmed[step] & bit ? "Pass" : "--";
}
void check_view_render(const check_view_model_t *m) {
    shown = *m;
    const hc_flow_t *f = &m->flow;
    const hc_item_t *item = hc_current_item(f);
    bool busy = hc_flow_busy(f), test = m->page == CHECK_TEST_PAGE;
    bool pending = f->phase == HC_CANCELLING || m->stopping;
    const char *left = "Back", *right = "Test", *title = titles[f->step], *cue = LV_SYMBOL_SETTINGS;
    char top[64];
    const char *subtitle = item->title;
    if (item->operation == HC_OP_STREAM) subtitle = "Live view";
    if (item->operation == HC_OP_VOICE) subtitle = "Voice";
    if (item->operation == HC_OP_TONE) subtitle = "Tone";
    if (item->operation == HC_OP_RECORD) subtitle = "Record";
    snprintf(top, sizeof(top), "%u / 8 | %s %u/%u", f->step + 1, subtitle, f->item + 1, hc_item_count(f->step));
    if (f->phase == HC_WELCOME) { title = "Hardware check"; snprintf(top, sizeof(top), "8 TESTS"); cue = LV_SYMBOL_OK; }
    else if (m->page == CHECK_SAFETY_PAGE) { title = "Clear space?"; cue = LV_SYMBOL_WARNING; }
    else if (m->page == CHECK_AUDIO_PAGE) { title = "Audio setup"; snprintf(top, sizeof(top), "%s", item->title); }
    else if (m->page == CHECK_RESTART_PAGE) { title = "New check?"; cue = LV_SYMBOL_REFRESH; }
    else if (m->page == CHECK_REPORT_PAGE || m->page == CHECK_DETAIL_PAGE) {
        title = m->page == CHECK_REPORT_PAGE ? "Results" : titles[m->detail_step];
        snprintf(top, sizeof(top), m->page == CHECK_REPORT_PAGE ? "PAGE %u / 2" : "DETAILS | PAGE %u",
                 (m->page == CHECK_REPORT_PAGE ? m->report_page : m->detail_page) + 1);
        right = "Next";
        if (m->page == CHECK_DETAIL_PAGE && (m->detail_page + 1) * 4 >= hc_item_count(m->detail_step)) right = "Test";
    } else if (busy) { cue = LV_SYMBOL_REFRESH; right = f->step == HC_MOTION ? "STOP" : "Stop"; }
    else if (f->phase == HC_REVIEW) { cue = LV_SYMBOL_OK; left = "Not OK"; right = "Next"; }
    else if (f->phase == HC_PASSED) { cue = f->results[f->step] == HC_PASS ? LV_SYMBOL_OK : LV_SYMBOL_WARNING; right = "Next"; }
    else if (f->phase == HC_ISSUE || f->phase == HC_SKIPPED || f->phase == HC_UNAVAILABLE) { cue = LV_SYMBOL_WARNING; right = "Next"; }
    else {
        const char *symbols[] = {LV_SYMBOL_HOME, LV_SYMBOL_IMAGE, LV_SYMBOL_VOLUME_MAX, LV_SYMBOL_AUDIO,
                                 LV_SYMBOL_EDIT, LV_SYMBOL_EYE_OPEN, LV_SYMBOL_SHUFFLE, LV_SYMBOL_LOOP};
        cue = symbols[f->step];
    }
    bool countdown = test && busy && strlen(m->status) == 1 && m->status[0] >= '1' && m->status[0] <= '3';
    if (countdown) cue = m->status;
    bool has_image = test && retained.pixels && image_attempt == f->attempt &&
        (f->step == HC_CAMERA || f->step == HC_COMBINED);
    bool zoom = has_image && m->full_image;
    if (zoom) snprintf(top, sizeof(top), "%s", item->operation == HC_OP_PHOTO ? "New photo clear?" : "Live view clear?");
    lv_label_set_text(eyebrow, top); lv_label_set_text(heading, title);
    visible(heading, !zoom);
    lv_label_set_text(visual, cue);
    lv_obj_align(visual, LV_ALIGN_CENTER, 0, CHECK_RESULT_Y - CHECK_SCREEN_SIZE / 2 -
                 (test && busy && f->step == HC_MIC && f->item == 0 && !countdown ? 20 : 0));
    lv_label_set_text(status, pending && f->step == HC_MOTION ? "Stopping | waiting for result" :
                     countdown ? f->step == HC_CAMERA ? "Hold still" : "Speak after REC" : m->status);
    lv_obj_set_style_text_color(status, lv_color_hex(pending ? 0x293732 : 0x58665e), 0);
    visible(picture, has_image);
    if (has_image) {
        lv_img_set_zoom(picture, demo_image_zoom(retained.width, retained.height, zoom ? 320 : 224,
                                               zoom ? 180 : 126));
        lv_obj_align(picture, LV_ALIGN_CENTER, 0, CHECK_RESULT_Y - CHECK_SCREEN_SIZE / 2);
    }
    visible(visual, !has_image && m->page != CHECK_AUDIO_PAGE && m->page != CHECK_REPORT_PAGE && m->page != CHECK_DETAIL_PAGE);
    visible(status, !zoom && m->page != CHECK_REPORT_PAGE && m->page != CHECK_DETAIL_PAGE);
    visible(bar, test && busy && !has_image && f->step != HC_SCREEN);
    lv_bar_set_value(bar, (int)m->progress, LV_ANIM_OFF);
    visible(meter, test && busy && f->step == HC_MIC && f->item == 0 && strlen(m->status) > 1);
    lv_bar_set_value(meter, (int)m->meter, LV_ANIM_OFF);
    for (unsigned i = 0; i < 3; i++) {
        visible(targets[i], test && f->step == HC_SCREEN && f->phase == HC_RUNNING);
        enabled(targets[i], i == m->targets);
    }
    if (f->step == HC_SCREEN && f->phase == HC_RUNNING) visible(visual, false);
    bool report = m->page == CHECK_REPORT_PAGE, detail = m->page == CHECK_DETAIL_PAGE;
    unsigned count = report ? 4 : detail ? hc_item_count(m->detail_step) - m->detail_page * 4 : 0;
    if (count > 4) count = 4;
    for (unsigned i = 0; i < 4; i++) {
        unsigned index = (report ? m->report_page : m->detail_page) * 4 + i;
        bool show = report ? index < HC_STEP_COUNT : detail && index < hc_item_count(m->detail_step);
        visible(rows[i], show);
        if (show) {
            lv_obj_align(rows[i], LV_ALIGN_TOP_MID, 0, 114 + (int)(4 - count) * 41 / 2 + (int)i * 41);
            static const char *const results[] = {"--", "Pass", "Issue", "Skip"};
            hc_flow_t temp = *f; temp.step = m->detail_step; temp.item = index;
            lv_label_set_text_fmt(row_text[i], "%s  |  %s", report ? titles[index] : hc_current_item(&temp)->title,
                                 report ? results[f->results[index]] : item_status(f, temp.step, index));
            lv_obj_center(row_text[i]);
        }
    }
    bool audio_page = m->page == CHECK_AUDIO_PAGE;
    visible(slider, audio_page); visible(volume_text, audio_page); visible(replay_note, audio_page && f->step == HC_MIC && f->item == 1);
    lv_label_set_text_fmt(volume_text, m->volume ? "Output %u / 95" : "Muted (%u / 95)", m->volume);
    if (!lv_obj_has_state(slider, LV_STATE_PRESSED)) lv_slider_set_value(slider, (int)m->volume, LV_ANIM_OFF);
    enabled(slider, !m->setting_pending);
    if (m->full_image) left = "Back";
    if (test && f->phase == HC_UNAVAILABLE && f->body_fault && (item->modules & HC_BODY)) { left = "Restart"; right = "Skip"; }
    for (unsigned i = 0; i < 2; i++) {
        bool danger = i == 1 && busy && f->step == HC_MOTION;
        style_zone(buttons[i], i == 1, danger);
        lv_obj_set_style_text_font(button_text[i], danger ? &lv_font_montserrat_20 : &lv_font_montserrat_18, 0);
        lv_label_set_text(button_text[i], i ? right : left);
        lv_obj_align(button_text[i], LV_ALIGN_CENTER,
                     (i ? CHECK_ZONE_WIDTH - CHECK_LABEL_LEFT : CHECK_LABEL_LEFT) - CHECK_ZONE_WIDTH / 2,
                     CHECK_LABEL_Y - CHECK_ZONE_TOP - CHECK_ZONE_HEIGHT / 2);
        enabled(buttons[i], i ? !m->setting_pending && !pending : !busy && !m->setting_pending);
    }
    if (f->phase == HC_WELCOME && m->page == CHECK_TEST_PAGE) enabled(buttons[0], false);
}
