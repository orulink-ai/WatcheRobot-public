#include "check_view.h"
#include "check_layout.h"
#include "host_mocks.h"
#include "lvgl.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef NDEBUG
#error Host regression assertions must remain enabled in Release builds
#endif
#if LV_COLOR_DEPTH != 16 || !LV_USE_SNAPSHOT
#error Real RGB565 snapshots are required
#endif

/* These tests inspect public LVGL objects and rendered pixels. No inclusion of
 * check_view.c, static-object access, replacement lvgl.h, or hardware driver. */
enum { SCREEN = 412 };
_Static_assert(LVGL_VERSION_MAJOR == 8 && LVGL_VERSION_MINOR == 4 && LVGL_VERSION_PATCH == 0,
               "The firmware uses LVGL 8.4.0");
_Static_assert(sizeof(lv_color_t) == 2, "RGB565 requires two-byte pixels");
_Static_assert(CHECK_SCREEN_SIZE == 412 && CHECK_ZONE_TOP == 288 && CHECK_ZONE_WIDTH == 178 && CHECK_ZONE_HEIGHT == 124,
               "The fixed circular navigation geometry is part of the view contract");
_Static_assert(CHECK_ZONE_RX == 112 && CHECK_ZONE_RY == 100,
               "The upper inner navigation corner is elliptical");
_Static_assert(CHECK_LABEL_LEFT == 108 && CHECK_LABEL_Y == 340 && CHECK_RESULT_Y == 188,
               "Navigation labels and visual content have fixed centers");
static struct host_queue queue;
static lv_disp_draw_buf_t draw_buffer;
static lv_color_t draw_pixels[SCREEN * 20];
static lv_disp_drv_t display_driver;
static lv_indev_drv_t pointer_driver;
static lv_point_t pointer_point;
static lv_indev_state_t pointer_state = LV_INDEV_STATE_RELEASED;
static unsigned pointer_reads;
static const char *ppm_path;
static check_view_model_t model;

#define REQUIRE(condition) do { if (!(condition)) { \
    fprintf(stderr, "%s:%d: contract failed: %s\n", __FILE__, __LINE__, #condition); \
    return false; } } while (0)

static void flush(lv_disp_drv_t *driver, const lv_area_t *area, lv_color_t *pixels) {
    (void)area; (void)pixels;
    lv_disp_flush_ready(driver);
}
static void read_pointer(lv_indev_drv_t *driver, lv_indev_data_t *data) {
    (void)driver;
    pointer_reads++;
    data->point = pointer_point;
    data->state = pointer_state;
}
static void settle(void) {
    lv_tick_inc(40);
    lv_timer_handler();
    lv_obj_update_layout(lv_scr_act());
}
static void tap(int x, int y) {
    pointer_point = (lv_point_t){(lv_coord_t)x, (lv_coord_t)y};
    pointer_state = LV_INDEV_STATE_PRESSED; settle();
    pointer_state = LV_INDEV_STATE_RELEASED; settle();
}
static void press_at(int x, int y) {
    pointer_point = (lv_point_t){(lv_coord_t)x, (lv_coord_t)y};
    pointer_state = LV_INDEV_STATE_PRESSED; settle();
}
static void release_at(int x, int y) {
    pointer_point = (lv_point_t){(lv_coord_t)x, (lv_coord_t)y};
    pointer_state = LV_INDEV_STATE_RELEASED; settle();
}
static void render(void) { check_view_render(&model); settle(); }
static void initialize(void) {
    lv_init();
    lv_disp_draw_buf_init(&draw_buffer, draw_pixels, NULL, SCREEN * 20);
    lv_disp_drv_init(&display_driver);
    display_driver.hor_res = SCREEN;
    display_driver.ver_res = SCREEN;
    display_driver.draw_buf = &draw_buffer;
    display_driver.flush_cb = flush;
    assert(lv_disp_drv_register(&display_driver));
    lv_indev_drv_init(&pointer_driver);
    pointer_driver.type = LV_INDEV_TYPE_POINTER;
    pointer_driver.read_cb = read_pointer;
    assert(lv_indev_drv_register(&pointer_driver));
    check_view_create(&queue);
    hc_flow_init(&model.flow);
    model.flow.modules = HC_HEAD | HC_BODY;
    model.flow.step = HC_SPEAKER;
    model.flow.phase = HC_IDLE;
    model.flow.attempt = 17;
    model.volume = 90;
    snprintf(model.status, sizeof(model.status), "%s", "Ready for this check");
    render();
}
static bool visible(lv_obj_t *object) {
    return object && !lv_obj_has_flag(object, LV_OBJ_FLAG_HIDDEN);
}
static lv_obj_t *find_label(lv_obj_t *root, const char *value) {
    if (lv_obj_check_type(root, &lv_label_class) && !strcmp(lv_label_get_text(root), value)) return root;
    for (uint32_t i = 0; i < lv_obj_get_child_cnt(root); i++) {
        lv_obj_t *found = find_label(lv_obj_get_child(root, (int32_t)i), value);
        if (found) return found;
    }
    return NULL;
}
static lv_obj_t *root_with_font(const lv_font_t *font) {
    lv_obj_t *screen = lv_scr_act();
    for (uint32_t i = 0; i < lv_obj_get_child_cnt(screen); i++) {
        lv_obj_t *child = lv_obj_get_child(screen, (int32_t)i);
        if (lv_obj_check_type(child, &lv_label_class) && lv_obj_get_style_text_font(child, 0) == font) return child;
    }
    return NULL;
}
static lv_obj_t *picture(void) {
    for (uint32_t i = 0; i < lv_obj_get_child_cnt(lv_scr_act()); i++) {
        lv_obj_t *child = lv_obj_get_child(lv_scr_act(), (int32_t)i);
        if (lv_obj_check_type(child, &lv_img_class)) return child;
    }
    return NULL;
}
static lv_obj_t *nav(const char *label) {
    lv_obj_t *text = find_label(lv_scr_act(), label);
    return text ? lv_obj_get_parent(text) : NULL;
}
static lv_area_t bounds(lv_obj_t *object) {
    assert(object);
    lv_area_t area;
    lv_obj_get_coords(object, &area);
    return area;
}
static bool center(lv_obj_t *object, int x, int y) {
    if (!object) return false;
    lv_area_t area = bounds(object);
    int actual_x = area.x1 + lv_area_get_width(&area) / 2;
    int actual_y = area.y1 + lv_area_get_height(&area) / 2;
    if (actual_x != x || actual_y != y) {
        fprintf(stderr, "center expected (%d,%d), actual (%d,%d), bounds (%d,%d)-(%d,%d)\n",
                x, y, actual_x, actual_y, area.x1, area.y1, area.x2, area.y2);
        return false;
    }
    return true;
}
static bool overlap(lv_area_t a, lv_area_t b) {
    return a.x1 <= b.x2 && b.x1 <= a.x2 && a.y1 <= b.y2 && b.y1 <= a.y2;
}
static bool hit(lv_obj_t *object, int x, int y) {
    lv_point_t p = {(lv_coord_t)x, (lv_coord_t)y};
    return lv_obj_hit_test(object, &p);
}
/* Independent analytic oracle, in doubled pixel-center coordinates.
 * Circle center (205.5,205.5), radius 206. Ellipse's upper inner corner
 * center (65.5,387.5), radii (112,100); the right side mirrors x -> 411-x.
 * This must agree with BOTH real draw pixels and LV_EVENT_HIT_TEST. */
static bool expected_nav(bool right, int x, int y) {
    if (right) x = SCREEN - 1 - x;
    if (x < 0 || x >= 178 || y < 288 || y >= SCREEN) return false;
    int64_t cx = 2 * x - 411, cy = 2 * y - 411;
    if (cx * cx + cy * cy > 412LL * 412) return false;
    int64_t dx = 2 * x - 131, dy = 2 * y - 775;
    return dx <= 0 || dy >= 0 ||
        dx * dx * 200LL * 200 + dy * dy * 224LL * 224 <= 224LL * 224 * 200 * 200;
}
static lv_img_dsc_t *snapshot(void) {
    settle();
    lv_img_dsc_t *image = lv_snapshot_take(lv_scr_act(), LV_IMG_CF_TRUE_COLOR);
    assert(image && image->header.w == SCREEN && image->header.h == SCREEN);
    assert(image->data_size == SCREEN * SCREEN * sizeof(lv_color_t));
    return image;
}
static lv_color_t pixel(const lv_img_dsc_t *image, int x, int y) {
    lv_color_t color;
    memcpy(&color, image->data + ((size_t)y * SCREEN + x) * sizeof(color), sizeof(color));
    return color;
}
static bool same_color(lv_color_t actual, uint32_t rgb) {
    return actual.full == lv_color_hex(rgb).full;
}
static bool save_ppm(void) {
    if (!ppm_path) return true;
    lv_img_dsc_t *image = snapshot();
    FILE *file = fopen(ppm_path, "wb");
    if (!file) { lv_snapshot_free(image); return false; }
    fprintf(file, "P6\n412 412\n255\n");
    bool okay = true;
    for (int y = 0; y < SCREEN; y++) for (int x = 0; x < SCREEN; x++) {
        lv_color32_t color = {.full = lv_color_to32(pixel(image, x, y))};
        unsigned char rgb[] = {color.ch.red, color.ch.green, color.ch.blue};
        if (fwrite(rgb, 1, 3, file) != 3) okay = false;
    }
    if (fclose(file) != 0) okay = false;
    lv_snapshot_free(image);
    return okay;
}

static bool test_config(void) {
    printf("Actual LVGL %d.%d.%d, RGB565, snapshot, headless\n",
           LVGL_VERSION_MAJOR, LVGL_VERSION_MINOR, LVGL_VERSION_PATCH);
    const lv_font_t *fonts[] = {&lv_font_montserrat_14, &lv_font_montserrat_16,
        &lv_font_montserrat_18, &lv_font_montserrat_20, &lv_font_montserrat_24, &lv_font_montserrat_48};
    const unsigned sizes[] = {14, 16, 18, 20, 24, 48};
    for (unsigned i = 0; i < 6; i++) {
        lv_font_glyph_dsc_t glyph;
        /* Nominal px size is NOT LVGL's line_height (18px has a 21px line).
         * Check the selected real font and its actual glyph implementation. */
        REQUIRE((unsigned)fonts[i]->line_height >= sizes[i]);
        REQUIRE((unsigned)fonts[i]->line_height <= sizes[i] + 8);
        REQUIRE(lv_font_get_glyph_dsc(fonts[i], &glyph, 'A', 0));
        REQUIRE(glyph.box_w > 0 && glyph.box_h > 0);
    }
    lv_img_dsc_t *image = snapshot();
    REQUIRE(same_color(pixel(image, 206, 20), 0xf4f1ec));
    lv_snapshot_free(image);
    return true;
}
static bool test_geometry_oracle(void) {
    for (int y = -1; y <= SCREEN; y++) {
        int first = -1, last = -1;
        bool span = check_layout_zone_span(y, &first, &last);
        for (int x = -1; x <= SCREEN; x++) {
            bool left = expected_nav(false, x, y), right = expected_nav(true, x, y);
            REQUIRE(check_layout_zone_hit(false, x, y) == left);
            REQUIRE(check_layout_zone_hit(true, x, y) == right);
            REQUIRE(left == (span && x >= first && x <= last));
            REQUIRE(!left || !right);
        }
    }
    REQUIRE(check_layout_zone_span(340, NULL, NULL));
    return true;
}
static bool test_nav_geometry(void) {
    lv_obj_t *left = nav("Back"), *right = nav("Test");
    REQUIRE(left && right);
    lv_area_t a = bounds(left), b = bounds(right);
    REQUIRE(a.x1 == 0 && a.y1 == 288 && lv_area_get_width(&a) == 178 && lv_area_get_height(&a) == 124);
    REQUIRE(b.x1 == 234 && b.y1 == 288 && lv_area_get_width(&b) == 178 && lv_area_get_height(&b) == 124);
    REQUIRE(lv_obj_has_flag(left, LV_OBJ_FLAG_ADV_HITTEST));
    REQUIRE(lv_obj_has_flag(right, LV_OBJ_FLAG_ADV_HITTEST));
    unsigned area = 0;
    for (int y = 0; y < SCREEN; y++) for (int x = 0; x < SCREEN; x++) {
        bool expected_left = expected_nav(false, x, y), expected_right = expected_nav(true, x, y);
        REQUIRE(hit(left, x, y) == expected_left);
        REQUIRE(hit(right, x, y) == expected_right);
        if (y >= 288) {
            lv_point_t p = {(lv_coord_t)x, (lv_coord_t)y};
            lv_hit_test_info_t info = {.point = &p, .res = true};
            REQUIRE(lv_event_send(left, LV_EVENT_HIT_TEST, &info) == LV_RES_OK);
            REQUIRE(info.res == expected_left);
            info.res = true;
            REQUIRE(lv_event_send(right, LV_EVENT_HIT_TEST, &info) == LV_RES_OK);
            REQUIRE(info.res == expected_right);
        }
        area += expected_left;
    }
    REQUIRE(area == 11043);
    REQUIRE(hit(left, 108, 340) && hit(right, 304, 340));
    REQUIRE(!hit(left, 174, 290) && !hit(right, 237, 290));
    REQUIRE(!hit(left, 206, 340) && !hit(right, 206, 340));
    REQUIRE(!hit(left, 5, 400) && !hit(right, 406, 400));
    REQUIRE(!hit(left, -1, 340) && !hit(right, 412, 340));
    tap(174, 290); tap(237, 290); tap(206, 340); tap(5, 400);
    REQUIRE(queue.count == 0);
    tap(108, 340); tap(304, 340);
    REQUIRE(queue.count == 2 && queue.items[0].kind == CHECK_LEFT && queue.items[1].kind == CHECK_RIGHT);
    return true;
}
static bool test_nav_pixels(void) {
    lv_obj_t *left_label = find_label(lv_scr_act(), "Back"), *right_label = find_label(lv_scr_act(), "Test");
    REQUIRE(left_label && right_label);
    /* Remove only glyphs to verify every nav-area pixel, including ellipse and
     * circle boundaries. Optional PPM is saved after restoring labels. */
    lv_obj_add_flag(left_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(right_label, LV_OBJ_FLAG_HIDDEN);
    lv_img_dsc_t *image = snapshot();
    unsigned mismatches = 0;
    for (int y = 288; y < SCREEN; y++) for (int x = 0; x < SCREEN; x++) {
        uint32_t color = expected_nav(false, x, y) ? 0xe6eae5 :
                         expected_nav(true, x, y) ? 0xcbe4d8 : 0xf4f1ec;
        if (!same_color(pixel(image, x, y), color)) {
            if (!mismatches) fprintf(stderr, "first shape/color mismatch (%d,%d), actual RGB565=%04x expected=%04x\n",
                x, y, (unsigned)pixel(image, x, y).full, (unsigned)lv_color_hex(color).full);
            mismatches++;
        }
    }
    lv_snapshot_free(image);
    lv_obj_clear_flag(left_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(right_label, LV_OBJ_FLAG_HIDDEN);
    fprintf(stdout, "nav draw/predicate mismatches: %u\n", mismatches);
    REQUIRE(mismatches == 0);
    return true;
}
static bool test_labels(void) {
    lv_obj_t *left = find_label(lv_scr_act(), "Back"), *right = find_label(lv_scr_act(), "Test");
    REQUIRE(center(left, 108, 340));
    REQUIRE(center(right, 304, 340));
    REQUIRE(lv_obj_get_style_text_font(left, 0) == &lv_font_montserrat_18);
    REQUIRE(lv_obj_get_style_text_font(right, 0) == &lv_font_montserrat_18);
    REQUIRE(same_color(lv_obj_get_style_text_color(right, 0), 0x247568));
    model.flow.phase = HC_REVIEW; render();
    REQUIRE(center(find_label(lv_scr_act(), "Not OK"), 108, 340));
    REQUIRE(center(find_label(lv_scr_act(), "Next"), 304, 340));
    return true;
}
static bool test_content(void) {
    for (unsigned step = 0; step < HC_STEP_COUNT; step++) {
        model.flow.step = step; model.flow.item = 0; render();
        lv_obj_t *heading = root_with_font(&lv_font_montserrat_24);
        lv_obj_t *visual = root_with_font(&lv_font_montserrat_48);
        lv_obj_t *status = find_label(lv_scr_act(), model.status);
        REQUIRE(visible(heading) && visible(visual) && visible(status));
        lv_area_t h = bounds(heading), s = bounds(status), v = bounds(visual);
        REQUIRE(h.y1 == 80 && h.x1 + lv_area_get_width(&h) / 2 == 206);
        REQUIRE(center(visual, 206, 188));
        REQUIRE(s.y1 == 260 && s.y2 <= 280);
        REQUIRE(!overlap(h, v) && !overlap(v, s));
        REQUIRE(s.y2 < 288);
    }
    return true;
}
static bool test_status(void) {
    /* Actual check_app/check_media messages, including their longest hints,
     * exercise LVGL wrapping and the y260/bottom<=280 rendering contract. */
    static const char *const messages[] = {
        "Clear screen? Taps worked?", "Clear, new photo?", "Did the picture update?",
        "Heard it clearly?", "Level followed your voice?", "Clear voice, no squeal?",
        "Events match your touches?", "Correct zone and effect?", "Did movement STOP?",
        "Small, safe movement?", "Picture, sound, touch OK?", "Tap, release; hold, release",
        "Locked. Restart resets results", "Module not ready; Next skips", "Clear space. Keep hands away",
        "No motion result. Check logs", "Setting failed. Retry / reboot", "REC 2.5s | -12 dBFS",
        "123 frames | 4s left", "Target X -180 / Y 180 deg", "ID 65535 | Press and hold"
    };
    for (unsigned i = 0; i < sizeof(messages) / sizeof(messages[0]); i++) {
        model.flow.step = HC_CAMERA; model.flow.item = 0; model.flow.phase = HC_REVIEW;
        snprintf(model.status, sizeof(model.status), "%s", messages[i]);
        render();
        lv_obj_t *status = find_label(lv_scr_act(), model.status);
        REQUIRE(visible(status));
        lv_area_t a = bounds(status);
        REQUIRE(a.y1 == 260 && a.y2 <= 280);
        REQUIRE(!overlap(a, bounds(nav("Next"))));
    }
    model.flow.step = HC_CAMERA; model.flow.item = 0; model.flow.phase = HC_RUNNING;
    snprintf(model.status, sizeof(model.status), "%s", "3"); render();
    REQUIRE(center(root_with_font(&lv_font_montserrat_48), 206, 188));
    lv_obj_t *status = find_label(lv_scr_act(), "Hold still");
    REQUIRE(visible(status) && bounds(status).y1 == 260 && bounds(status).y2 <= 280);
    return true;
}
static bool test_running_content(void) {
    model.flow.phase = HC_RUNNING;
    for (unsigned step = 0; step < HC_STEP_COUNT; step++) {
        model.flow.step = step;
        snprintf(model.status, sizeof(model.status), "%s", step == HC_SCREEN ? "Tap 1, then 2, then 3" : "Working");
        render();
        lv_obj_t *status = find_label(lv_scr_act(), model.status); REQUIRE(visible(status));
        lv_area_t s = bounds(status); REQUIRE(s.y1 == 260 && s.y2 <= 280);
        for (uint32_t i = 0; i < lv_obj_get_child_cnt(lv_scr_act()); i++) {
            lv_obj_t *child = lv_obj_get_child(lv_scr_act(), (int32_t)i);
            if (child == status || !visible(child)) continue;
            if (overlap(s, bounds(child))) {
                lv_area_t a = bounds(child);
                fprintf(stderr, "running step=%u: status (%d,%d)-(%d,%d) overlaps object (%d,%d)-(%d,%d)\n",
                        step, s.x1, s.y1, s.x2, s.y2, a.x1, a.y1, a.x2, a.y2);
            }
            REQUIRE(!overlap(s, bounds(child)));
        }
    }
    return true;
}
static bool check_report_rows(void) {
    lv_obj_t *heading = root_with_font(&lv_font_montserrat_24);
    REQUIRE(visible(heading));
    REQUIRE(!visible(root_with_font(&lv_font_montserrat_48)));
    REQUIRE(!visible(find_label(lv_scr_act(), model.status)));
    REQUIRE(!visible(picture()));
    unsigned total = model.page == CHECK_REPORT_PAGE ? HC_STEP_COUNT : hc_item_count(model.detail_step);
    unsigned offset = (model.page == CHECK_REPORT_PAGE ? model.report_page : model.detail_page) * 4;
    unsigned expected_count = total - offset < 4 ? total - offset : 4;
    int expected_top = 114 + (164 - (int)expected_count * 41) / 2;
    lv_area_t previous = bounds(heading);
    unsigned count = 0;
    for (uint32_t i = 0; i < lv_obj_get_child_cnt(lv_scr_act()); i++) {
        lv_obj_t *row = lv_obj_get_child(lv_scr_act(), (int32_t)i);
        if (!visible(row) || lv_obj_get_child_cnt(row) != 1) continue;
        lv_obj_t *label = lv_obj_get_child(row, 0);
        if (!lv_obj_check_type(label, &lv_label_class) ||
            lv_obj_get_style_text_font(label, 0) != &lv_font_montserrat_14) continue;
        if (!strstr(lv_label_get_text(label), "  |  ")) continue;
        lv_area_t a = bounds(row), text = bounds(label);
        if (a.y1 != expected_top + (int)count * 41) {
            fprintf(stderr, "report/detail step=%u page=%u row=%u: expected y=%d, actual y=%d\n",
                    model.detail_step, model.detail_page, count, expected_top + (int)count * 41, a.y1);
        }
        REQUIRE(lv_area_get_height(&a) == 41 && a.y1 == expected_top + (int)count * 41);
        REQUIRE(a.y1 > previous.y2 && a.y2 < 288);
        REQUIRE(text.x1 >= a.x1 && text.x2 <= a.x2 && text.y1 >= a.y1 && text.y2 <= a.y2);
        REQUIRE(!overlap(a, bounds(nav("Back"))));
        previous = a; count++;
    }
    REQUIRE(count == expected_count);
    return true;
}
static bool test_report(void) {
    model.page = CHECK_REPORT_PAGE;
    for (unsigned page = 0; page < 2; page++) {
        model.report_page = page; render(); REQUIRE(check_report_rows());
        queue.count = 0; tap(108, 340); tap(304, 340);
        REQUIRE(queue.count == 2 && queue.items[0].navigation == CHECK_REPORT_PAGE * 32 + page);
    }
    model.page = CHECK_DETAIL_PAGE;
    for (unsigned step = 0; step < HC_STEP_COUNT; step++) {
        model.detail_step = step;
        for (unsigned page = 0; page * 4 < hc_item_count(step); page++) {
            model.detail_page = page; render(); REQUIRE(check_report_rows());
        }
    }
    return true;
}
static bool test_detail_tail(void) {
    model.page = CHECK_DETAIL_PAGE; model.detail_step = HC_LIGHTS; model.detail_page = 3;
    render();
    REQUIRE(hc_item_count(HC_LIGHTS) == 14);
    /* Exactly two rows: y155..195 and y196..236, list centered in 114..277. */
    REQUIRE(check_report_rows());
    return true;
}
static bool test_stop(void) {
    model.flow.step = HC_MOTION; model.flow.phase = HC_RUNNING; render();
    lv_obj_t *button = nav("STOP"), *label = find_label(lv_scr_act(), "STOP");
    REQUIRE(button && label && !lv_obj_has_state(button, LV_STATE_DISABLED));
    REQUIRE(lv_obj_get_style_text_font(label, 0) == &lv_font_montserrat_20);
    REQUIRE(center(label, 304, 340));
    lv_img_dsc_t *image = snapshot();
    bool red = same_color(pixel(image, 304, 370), 0xb34132);
    lv_snapshot_free(image); REQUIRE(red);
    queue.count = 0; press_at(304, 340);
    REQUIRE(lv_obj_has_state(button, LV_STATE_PRESSED));
    image = snapshot();
    bool dark_red = same_color(pixel(image, 304, 370), 0x8b3026);
    lv_snapshot_free(image); REQUIRE(dark_red);
    release_at(304, 340);
    REQUIRE(queue.count == 1 && queue.items[0].kind == CHECK_RIGHT && queue.items[0].phase == HC_RUNNING);
    model.flow.phase = HC_CANCELLING; render();
    REQUIRE(button && lv_obj_has_state(button, LV_STATE_DISABLED));
    REQUIRE(!hit(button, 304, 340));
    image = snapshot(); red = same_color(pixel(image, 304, 370), 0xb34132);
    lv_snapshot_free(image); REQUIRE(red);
    queue.count = 0; tap(304, 340); tap(108, 340); REQUIRE(queue.count == 0);
    model.flow.phase = HC_IDLE; render();
    image = snapshot(); bool green = same_color(pixel(image, 304, 370), 0xcbe4d8);
    lv_snapshot_free(image); REQUIRE(green);
    REQUIRE(lv_obj_get_style_text_font(find_label(lv_scr_act(), "Test"), 0) == &lv_font_montserrat_18);
    return true;
}
static bool test_stopping(void) {
    model.flow.step = HC_MOTION; model.flow.item = 2; model.flow.phase = HC_RUNNING;
    model.stopping = false; render();
    lv_obj_t *right = nav("STOP"), *left = nav("Back");
    REQUIRE(right && left && !lv_obj_has_state(right, LV_STATE_DISABLED));
    queue.count = 0; tap(304, 340); REQUIRE(queue.count == 1 && queue.items[0].kind == CHECK_RIGHT);
    model.stopping = true;
    snprintf(model.status, sizeof(model.status), "%s", "Stop requested; waiting"); render();
    REQUIRE(model.flow.phase == HC_RUNNING);
    REQUIRE(nav("STOP") == right && lv_obj_has_state(right, LV_STATE_DISABLED));
    /* Back is already disabled by running; stopping adds ONLY the right-side
     * disable condition and must not manufacture CANCELLING or change captions. */
    REQUIRE(lv_obj_has_state(left, LV_STATE_DISABLED));
    lv_obj_t *status = find_label(lv_scr_act(), "Stopping | waiting for result");
    REQUIRE(visible(status));
    REQUIRE(bounds(status).y1 == 260 && bounds(status).y2 <= 280);
    REQUIRE(lv_obj_get_style_text_font(find_label(lv_scr_act(), "STOP"), 0) == &lv_font_montserrat_20);
    lv_img_dsc_t *image = snapshot();
    bool red = same_color(pixel(image, 304, 370), 0xb34132);
    lv_snapshot_free(image); REQUIRE(red);
    REQUIRE(!hit(right, 304, 340));
    queue.count = 0; tap(304, 340); tap(304, 340); tap(108, 340); REQUIRE(queue.count == 0);
    model.stopping = false; model.flow.phase = HC_REVIEW; render();
    REQUIRE(nav("Next") == right && !lv_obj_has_state(right, LV_STATE_DISABLED));
    REQUIRE(!lv_obj_has_state(left, LV_STATE_DISABLED));
    return true;
}
static bool test_pressed(void) {
    lv_obj_t *right = nav("Test"); REQUIRE(right);
    press_at(304, 340); REQUIRE(lv_obj_has_state(right, LV_STATE_PRESSED));
    lv_img_dsc_t *image = snapshot();
    lv_color_t color = pixel(image, 304, 370);
    /* Ordinary pressed fill stays a light green surface (not dark ink). */
    lv_color32_t rgb = {.full = lv_color_to32(color)};
    bool light_green = rgb.ch.red >= 140 && rgb.ch.green >= 170 && rgb.ch.blue >= 140 &&
                       rgb.ch.green > rgb.ch.red && rgb.ch.green > rgb.ch.blue && !same_color(color, 0xcbe4d8);
    lv_snapshot_free(image); REQUIRE(light_green);
    release_at(304, 340);
    image = snapshot(); bool restored = same_color(pixel(image, 304, 370), 0xcbe4d8);
    lv_snapshot_free(image); REQUIRE(restored);
    REQUIRE(queue.count == 1 && queue.items[0].kind == CHECK_RIGHT);
    return true;
}
static bool test_busy(void) {
    for (unsigned step = 0; step < HC_STEP_COUNT; step++) {
        model.flow.step = step; model.flow.phase = HC_RUNNING; model.setting_pending = false; render();
        lv_obj_t *left = nav("Back"), *right = nav(step == HC_MOTION ? "STOP" : "Stop");
        REQUIRE(left && right && lv_obj_has_state(left, LV_STATE_DISABLED));
        REQUIRE(!lv_obj_has_state(right, LV_STATE_DISABLED));
        queue.count = 0; tap(108, 340); REQUIRE(queue.count == 0);
        model.flow.phase = HC_CANCELLING; render();
        REQUIRE(lv_obj_has_state(right, LV_STATE_DISABLED));
        queue.count = 0; tap(108, 340); tap(304, 340); REQUIRE(queue.count == 0);
    }
    model.flow.step = HC_SPEAKER; model.flow.phase = HC_IDLE; model.setting_pending = true; render();
    REQUIRE(lv_obj_has_state(nav("Back"), LV_STATE_DISABLED));
    REQUIRE(lv_obj_has_state(nav("Test"), LV_STATE_DISABLED));
    queue.count = 0; tap(108, 340); tap(304, 340); REQUIRE(queue.count == 0);
    lv_img_dsc_t *image = snapshot();
    bool disabled = same_color(pixel(image, 108, 370), 0xebeee8) && same_color(pixel(image, 304, 370), 0xebeee8);
    lv_snapshot_free(image); REQUIRE(disabled);
    model.page = CHECK_AUDIO_PAGE; render();
    for (uint32_t i = 0; i < lv_obj_get_child_cnt(lv_scr_act()); i++) {
        lv_obj_t *child = lv_obj_get_child(lv_scr_act(), (int32_t)i);
        if (lv_obj_check_type(child, &lv_slider_class)) REQUIRE(lv_obj_has_state(child, LV_STATE_DISABLED));
    }
    model.setting_pending = false; model.page = CHECK_TEST_PAGE; model.flow.phase = HC_WELCOME; render();
    REQUIRE(lv_obj_has_state(nav("Back"), LV_STATE_DISABLED));
    return true;
}
static bool test_native_input(void) {
    lv_obj_t *right = nav("Test"), *left = nav("Back"); REQUIRE(right && left);
    REQUIRE(!lv_obj_has_flag(right, LV_OBJ_FLAG_PRESS_LOCK) && !lv_obj_has_flag(left, LV_OBJ_FLAG_PRESS_LOCK));
    queue.count = 0;
    /* A press that leaves either curved region cannot click through its box. */
    press_at(304, 340); press_at(237, 290); release_at(237, 290); REQUIRE(queue.count == 0);
    press_at(108, 340); press_at(174, 290); release_at(174, 290); REQUIRE(queue.count == 0);
    press_at(304, 340); press_at(206, 340); release_at(206, 340); REQUIRE(queue.count == 0);
    press_at(108, 340); press_at(5, 400); release_at(5, 400); REQUIRE(queue.count == 0);
    press_at(304, 340);
    model.setting_pending = true; render();
    release_at(304, 340); REQUIRE(queue.count == 0);
    tap(304, 340); tap(108, 340); REQUIRE(queue.count == 0);
    model.setting_pending = false; render();
    tap(304, 340); tap(108, 340);
    REQUIRE(queue.count == 2 && queue.items[0].kind == CHECK_RIGHT && queue.items[1].kind == CHECK_LEFT);
    REQUIRE(queue.items[0].attempt == model.flow.attempt && queue.items[0].phase == HC_IDLE);
    return true;
}
static bool test_release_outside(void) {
    lv_obj_t *right = nav("Test"); REQUIRE(right);
    const lv_point_t outside[] = {{237, 290}, {206, 340}, {406, 400}};
    for (unsigned i = 0; i < sizeof(outside) / sizeof(outside[0]); i++) {
        REQUIRE(!hit(right, outside[i].x, outside[i].y));
        unsigned reads = pointer_reads;
        press_at(304, 340); REQUIRE(pointer_reads == reads + 1);
        /* Exactly the next sample is RELEASED at a different coordinate.
         * There is NO intermediate PRESSED/move sample to cancel capture. */
        release_at(outside[i].x, outside[i].y);
        REQUIRE(pointer_reads == reads + 2);
        if (queue.count) fprintf(stderr, "direct outside release (%d,%d) incorrectly queued %u intent(s)\n",
                                 outside[i].x, outside[i].y, queue.count);
        REQUIRE(queue.count == 0);
    }
    tap(304, 340);
    REQUIRE(queue.count == 1 && queue.items[0].kind == CHECK_RIGHT);
    return true;
}
static bool test_release_disabled(void) {
    for (unsigned mode = 0; mode < 3; mode++) {
        model.setting_pending = false; model.stopping = false;
        model.flow.step = mode == 2 ? HC_MOTION : HC_SPEAKER;
        model.flow.item = mode == 2 ? 2 : 0;
        model.flow.phase = mode ? HC_RUNNING : HC_IDLE; render();
        lv_obj_t *right = nav(mode == 2 ? "STOP" : mode == 1 ? "Stop" : "Test"); REQUIRE(right);
        queue.count = 0;
        unsigned reads = pointer_reads;
        press_at(304, 340);
        REQUIRE(pointer_reads == reads + 1 && lv_obj_has_state(right, LV_STATE_PRESSED));
        if (mode == 0) model.setting_pending = true;
        else if (mode == 1) model.flow.phase = HC_CANCELLING;
        else model.stopping = true;
        /* Do not call render()/settle(): it would insert an extra PRESSED input
         * sample AFTER disabling and conceal the direct-release defect. */
        check_view_render(&model);
        lv_obj_update_layout(lv_scr_act());
        REQUIRE(pointer_reads == reads + 1);
        REQUIRE(lv_obj_has_state(right, LV_STATE_DISABLED));
        release_at(304, 340);
        REQUIRE(pointer_reads == reads + 2);
        if (queue.count) fprintf(stderr, "direct disabled release mode=%u incorrectly queued %u intent(s)\n", mode, queue.count);
        REQUIRE(queue.count == 0);
    }
    return true;
}
static uint8_t *install_image(uint32_t attempt) {
    demo_image_t source = {.width = 320, .height = 180, .bytes = 320 * 180 * 2};
    source.pixels = malloc(source.bytes);
    assert(source.pixels);
    lv_color_t *colors = (lv_color_t *)source.pixels;
    for (unsigned i = 0; i < 320 * 180; i++) colors[i] = lv_color_hex(0xec00ef);
    uint8_t *pixels = source.pixels;
    check_view_image(&source, attempt);
    assert(!source.pixels && !source.width && !source.height && !source.bytes);
    return pixels;
}
static bool check_image_extent(int width, int height) {
    lv_obj_t *image = picture();
    REQUIRE(visible(image));
    REQUIRE(center(image, 206, 188));
    lv_area_t transformed;
    lv_point_t pivot;
    lv_img_get_pivot(image, &pivot);
    _lv_img_buf_get_transformed_area(&transformed, lv_obj_get_width(image), lv_obj_get_height(image),
                                   lv_img_get_angle(image), lv_img_get_zoom(image), &pivot);
    /* LVGL adds a two-pixel sampling margin on each side for non-unit zoom.
     * Verify real visible pixels too, rather than confuse raw object dimensions
     * (320x180 source) with its thumbnail dimensions. */
    REQUIRE(abs(lv_area_get_width(&transformed) - width) <= 6);
    REQUIRE(abs(lv_area_get_height(&transformed) - height) <= 6);
    lv_area_t expected = {(lv_coord_t)(206 - width / 2), (lv_coord_t)(188 - height / 2),
                         (lv_coord_t)(206 + width / 2 - 1), (lv_coord_t)(188 + height / 2 - 1)};
    lv_img_dsc_t *snap = snapshot();
    lv_area_t colored = {SCREEN, SCREEN, -1, -1};
    for (int y = 90; y < 288; y++) for (int x = 35; x < 377; x++) {
        if (!same_color(pixel(snap, x, y), 0xec00ef)) continue;
        if (x < colored.x1) colored.x1 = (lv_coord_t)x;
        if (x > colored.x2) colored.x2 = (lv_coord_t)x;
        if (y < colored.y1) colored.y1 = (lv_coord_t)y;
        if (y > colored.y2) colored.y2 = (lv_coord_t)y;
    }
    lv_snapshot_free(snap);
    REQUIRE(abs(colored.x1 - expected.x1) <= 1 && abs(colored.x2 - expected.x2) <= 1);
    REQUIRE(abs(colored.y1 - expected.y1) <= 1 && abs(colored.y2 - expected.y2) <= 1);
    REQUIRE(expected.y2 < 288 && expected.x1 >= 0 && expected.x2 < SCREEN);
    REQUIRE(!overlap(expected, bounds(root_with_font(&lv_font_montserrat_14))));
    if (visible(root_with_font(&lv_font_montserrat_24))) REQUIRE(!overlap(expected, bounds(root_with_font(&lv_font_montserrat_24))));
    lv_obj_t *status = find_label(lv_scr_act(), model.status);
    if (visible(status)) REQUIRE(!overlap(expected, bounds(status)));
    return true;
}
static bool photo_contract(unsigned item) {
    model.flow.step = HC_CAMERA; model.flow.item = item; model.flow.phase = HC_REVIEW; render();
    uint8_t *owned = install_image(model.flow.attempt); render();
    const lv_img_dsc_t *descriptor = lv_img_get_src(picture());
    REQUIRE(descriptor && descriptor->data == owned);
    REQUIRE(check_image_extent(224, 126));
    REQUIRE(visible(root_with_font(&lv_font_montserrat_24)));
    REQUIRE(!visible(root_with_font(&lv_font_montserrat_48)));
    queue.count = 0; tap(206, 188);
    REQUIRE(queue.count == 1 && queue.items[0].kind == CHECK_IMAGE);
    REQUIRE(queue.items[0].attempt == model.flow.attempt && queue.items[0].phase == HC_REVIEW);
    model.full_image = true; render();
    REQUIRE(check_image_extent(320, 180));
    REQUIRE(!visible(root_with_font(&lv_font_montserrat_24)));
    REQUIRE(visible(find_label(lv_scr_act(), item ? "Live view clear?" : "New photo clear?")));
    REQUIRE(!visible(find_label(lv_scr_act(), model.status)));
    REQUIRE(center(find_label(lv_scr_act(), "Back"), 108, 340));
    REQUIRE(center(find_label(lv_scr_act(), "Next"), 304, 340));
    descriptor = lv_img_get_src(picture()); REQUIRE(descriptor->data == owned && host_image_releases == 0);
    queue.count = 0; tap(108, 340); tap(304, 340);
    REQUIRE(queue.count == 2 && queue.items[0].kind == CHECK_LEFT && queue.items[1].kind == CHECK_RIGHT);
    REQUIRE(queue.items[0].attempt == model.flow.attempt && queue.items[1].attempt == model.flow.attempt);
    REQUIRE(queue.items[0].navigation == check_view_navigation(&model));
    REQUIRE(queue.items[1].phase == HC_REVIEW && queue.items[1].navigation == check_view_navigation(&model));
    model.full_image = false; render(); REQUIRE(check_image_extent(224, 126));
    REQUIRE(visible(root_with_font(&lv_font_montserrat_24)));
    REQUIRE(visible(find_label(lv_scr_act(), model.status)));
    REQUIRE(visible(find_label(lv_scr_act(), "Not OK")) && visible(find_label(lv_scr_act(), "Next")));
    model.full_image = true; render();
    return true;
}
static bool test_photo(void) { return photo_contract(0); }
static bool test_live_image(void) { return photo_contract(1); }
static bool test_stale_image(void) {
    model.flow.step = HC_CAMERA; model.flow.phase = HC_REVIEW; model.full_image = true; render();
    REQUIRE(!visible(picture()));
    REQUIRE(visible(root_with_font(&lv_font_montserrat_24)));
    REQUIRE(visible(find_label(lv_scr_act(), model.status)));
    install_image(model.flow.attempt); render(); REQUIRE(visible(picture()));
    model.flow.attempt++; render();
    REQUIRE(!visible(picture()));
    REQUIRE(visible(root_with_font(&lv_font_montserrat_24)));
    REQUIRE(visible(find_label(lv_scr_act(), model.status)));
    queue.count = 0; tap(206, 188); REQUIRE(queue.count == 0);
    install_image(model.flow.attempt); render(); REQUIRE(visible(picture()) && host_image_releases == 1);
    REQUIRE(check_image_extent(320, 180));
    model.flow.step = HC_SPEAKER; render(); REQUIRE(!visible(picture()));
    REQUIRE(visible(root_with_font(&lv_font_montserrat_24)));
    model.flow.step = HC_COMBINED; model.flow.phase = HC_RUNNING; render();
    REQUIRE(visible(picture()));
    return true;
}
static bool test_image_arrival_attempt(void) {
    model.flow.step = HC_CAMERA; model.flow.phase = HC_REVIEW; render();
    uint32_t old_attempt = model.flow.attempt, new_attempt = old_attempt + 1;
    /* A new decoded frame arrives before the new model has been rendered.
     * Ownership is attached to the event attempt, never to stale 'shown'. */
    uint8_t *owned = install_image(new_attempt);
    REQUIRE(model.flow.attempt == old_attempt);
    model.flow.attempt = new_attempt; render();
    REQUIRE(visible(picture()));
    const lv_img_dsc_t *descriptor = lv_img_get_src(picture());
    REQUIRE(descriptor && descriptor->data == owned);
    REQUIRE(check_image_extent(224, 126));
    queue.count = 0; tap(206, 188);
    REQUIRE(queue.count == 1 && queue.items[0].kind == CHECK_IMAGE && queue.items[0].attempt == new_attempt);
    model.full_image = true; render(); REQUIRE(check_image_extent(320, 180));
    /* Late data for the old attempt must not become the current photo either. */
    install_image(old_attempt); render();
    REQUIRE(!visible(picture()) && host_image_releases == 1);
    REQUIRE(visible(root_with_font(&lv_font_montserrat_24)));
    return true;
}

int main(int argc, char **argv) {
    const struct { const char *name; bool (*run)(void); } cases[] = {
        {"config", test_config}, {"geometry_oracle", test_geometry_oracle},
        {"nav_geometry", test_nav_geometry}, {"nav_pixels", test_nav_pixels},
        {"labels", test_labels}, {"content", test_content}, {"status", test_status},
        {"running_content", test_running_content}, {"report", test_report}, {"detail_tail", test_detail_tail},
        {"stop", test_stop}, {"stopping", test_stopping},
        {"pressed", test_pressed}, {"busy", test_busy}, {"native_input", test_native_input},
        {"release_outside", test_release_outside}, {"release_disabled", test_release_disabled},
        {"photo", test_photo}, {"live_image", test_live_image}, {"stale_image", test_stale_image},
        {"image_arrival_attempt", test_image_arrival_attempt}
    };
    if (argc != 2 && argc != 4) return 2;
    if (argc == 4) {
        if (strcmp(argv[2], "--ppm")) return 2;
        ppm_path = argv[3];
    }
    for (unsigned i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        if (strcmp(argv[1], cases[i].name)) continue;
        initialize();
        bool okay = cases[i].run();
        /* Preserve a diagnostic snapshot even on assertion failure. */
        if (!save_ppm()) { fprintf(stderr, "Unable to write software PPM\n"); okay = false; }
        printf("%s: %s (real LVGL %d.%d.%d software; no LCD evidence)\n", cases[i].name,
               okay ? "PASS" : "FAIL", LVGL_VERSION_MAJOR, LVGL_VERSION_MINOR, LVGL_VERSION_PATCH);
        return okay ? 0 : 1;
    }
    return 2;
}
