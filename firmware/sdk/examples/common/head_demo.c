#include "head_demo.h"
#include "demo_audio.h"
#include "demo_image.h"
#include "demo_playback.h"
#include "demo_record.h"
#include "demo_speech.h"
#include "demo_state.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "watcher_hw_audio.h"
#include "watcher_hw_camera.h"
#include "watcher_hw_display.h"
#include <stdio.h>
#include <string.h>

#define SAMPLE_RATE 16000
#define CHUNK_SAMPLES 1600
#define AUDIO_CHUNKS 30
static const char *TAG = "head_demo";
static QueueHandle_t actions;
static demo_state_t state;
static lv_obj_t *buttons[5], *headline, *detail, *empty, *image, *progress, *cue, *meter;
static lv_obj_t *viewer, *full_image;
static lv_obj_t *audio_page, *volume_slider, *volume_label, *audio_hint, *boost_button, *boost_label;
static lv_obj_t *volume_down, *volume_up;
static lv_obj_t *speaker_page, *speaker_headline, *speaker_detail, *speaker_progress;
static lv_obj_t *speaker_buttons[3];
/* Preferences are protected by the display lock. Only the media owner applies
 * hardware volume; replay boost is an example policy, never an SDK policy. */
static unsigned output_volume = DEMO_OUTPUT_DEFAULT, voice_boost = 1;
static uint32_t recording_peak;
typedef struct {
    int action;
    unsigned value;
} demo_command_t;
static lv_img_dsc_t preview;
static uint8_t *preview_pixels;
/* Only the media owner touches capture buffers. LVGL uses preview_pixels until
 * replacement under its lock; never point LVGL at borrowed camera data. */

static void controls(void) {
    for (unsigned i = 0; i < 5; ++i) {
        bool ready = i == 0 ? state.camera_ready : state.audio_ready;
        if (i == 2)
            ready = ready && state.recording_ready;
        if (ready && !demo_busy(&state))
            lv_obj_clear_state(buttons[i], LV_STATE_DISABLED);
        else
            lv_obj_add_state(buttons[i], LV_STATE_DISABLED);
    }
    lv_obj_t *audio_controls[] = {volume_slider, volume_down, volume_up, boost_button};
    for (unsigned i = 0; i < 4; ++i) {
        if (state.audio_ready && !demo_busy(&state))
            lv_obj_clear_state(audio_controls[i], LV_STATE_DISABLED);
        else
            lv_obj_add_state(audio_controls[i], LV_STATE_DISABLED);
    }
    for (unsigned i = 0; i < 3; ++i) {
        if (state.audio_ready && !demo_busy(&state))
            lv_obj_clear_state(speaker_buttons[i], LV_STATE_DISABLED);
        else
            lv_obj_add_state(speaker_buttons[i], LV_STATE_DISABLED);
    }
}
static void audio_settings_locked(const char *hint) {
    char text[64];
    snprintf(text, sizeof(text), "Output: %u / 95%s", output_volume, output_volume ? "" : " (muted)");
    lv_label_set_text(volume_label, text);
    lv_slider_set_value(volume_slider, (int)output_volume, LV_ANIM_OFF);
    snprintf(text, sizeof(text), "Replay boost: %ux", voice_boost);
    lv_label_set_text(boost_label, text);
    lv_label_set_text(audio_hint, hint);
}
static void show_locked(const char *title, const char *message, int percent) {
    lv_label_set_text(headline, title);
    lv_label_set_text(detail, message);
    lv_obj_set_style_text_color(headline, lv_color_hex(state.phase == DEMO_ERROR ? 0xb34132 : 0x247568), 0);
    lv_bar_set_value(progress, percent, LV_ANIM_OFF);
    lv_label_set_text(speaker_headline, title);
    lv_label_set_text(speaker_detail, message);
    lv_obj_set_style_text_color(speaker_headline, lv_color_hex(state.phase == DEMO_ERROR ? 0xb34132 : 0x247568), 0);
    lv_bar_set_value(speaker_progress, percent, LV_ANIM_OFF);
    const bool busy = demo_busy(&state);
    if (busy) {
        lv_obj_add_flag(image, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(empty, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(cue, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(cue, state.phase == DEMO_RECORDING ? "REC" : state.phase == DEMO_PLAYING ? "PLAY" : "...");
    } else {
        lv_obj_add_flag(cue, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(meter, LV_OBJ_FLAG_HIDDEN);
        if (preview_pixels)
            lv_obj_clear_flag(image, LV_OBJ_FLAG_HIDDEN);
        else
            lv_obj_clear_flag(empty, LV_OBJ_FLAG_HIDDEN);
    }
    controls();
}
/* UI lock is never held across media, countdown delays, or decoding. */
static void show(demo_phase_t phase, const char *title, const char *message, int percent) {
    if (!watcher_hw_display_lock(0))
        return;
    if (phase == DEMO_COMPLETE || phase == DEMO_ERROR)
        demo_finish(&state, phase == DEMO_COMPLETE);
    else
        state.phase = phase;
    show_locked(title, message, percent);
    watcher_hw_display_unlock();
}
static void failed(const char *operation, esp_err_t error, const char *hint) {
    char message[112];
    snprintf(message, sizeof(message), "%s: %s\n%s", operation, esp_err_to_name(error), hint);
    ESP_LOGE(TAG, "%s", message);
    show(DEMO_ERROR, "Could not finish", message, 0);
}
static void clicked(lv_event_t *event) {
    int action = (int)(intptr_t)lv_event_get_user_data(event);
    demo_command_t command = {.action = action};
    demo_state_t previous = state;
    /* LVGL invokes this with its lock held. Claim busy before enqueueing. */
    if (!demo_begin(&state, action))
        return;
    if (xQueueSend(actions, &command, 0) != pdTRUE) {
        state = previous;
        ESP_LOGW(TAG, "Action queue busy");
        show_locked("Please try again", "No action was queued", 0);
        return;
    }
    ESP_LOGI(TAG, "UX5 click action=%d", action);
    show_locked(action <= DEMO_RECORD ? "Get ready..." : "Playing...",
                action <= DEMO_RECORD ? "Watch the 3, 2, 1 countdown" : "Listen to the speaker", 0);
}
static void open_audio(lv_event_t *event) {
    (void)event;
    if (!state.audio_ready || demo_busy(&state))
        return;
    audio_settings_locked(output_volume ? "Adjust, then tap Test speaker." : "Output muted. Raise the volume.");
    lv_obj_move_foreground(audio_page);
    lv_obj_clear_flag(audio_page, LV_OBJ_FLAG_HIDDEN);
}
static void request_volume(lv_event_t *event) {
    int delta = (int)(intptr_t)lv_event_get_user_data(event);
    unsigned requested = delta ? demo_volume_step(output_volume, delta)
                               : demo_volume_step((unsigned)lv_slider_get_value(volume_slider), 0);
    demo_state_t previous = state;
    if (!demo_begin(&state, DEMO_VOLUME))
        return;
    demo_command_t command = {.action = DEMO_VOLUME, .value = requested};
    if (xQueueSend(actions, &command, 0) != pdTRUE) {
        state = previous;
        audio_settings_locked("No change applied. Try again.");
    } else {
        audio_settings_locked("Applying volume...");
    }
    controls();
}
static void change_boost(lv_event_t *event) {
    (void)event;
    if (!state.audio_ready || demo_busy(&state))
        return;
    voice_boost = demo_boost_next(voice_boost);
    audio_settings_locked("Recorded voice only, not speaker tests.\nBoost also raises background noise.");
}
static void open_speaker(lv_event_t *event) {
    (void)event;
    if (!state.audio_ready || demo_busy(&state))
        return;
    char message[96];
    snprintf(message, sizeof(message), "Output %u/95%s\nChoose voice or a 1-second tone", output_volume,
             output_volume ? "" : " (muted)");
    lv_label_set_text(speaker_headline, "Ready to test");
    lv_label_set_text(speaker_detail, message);
    lv_obj_set_style_text_color(speaker_headline, lv_color_hex(0x247568), 0);
    lv_bar_set_value(speaker_progress, 0, LV_ANIM_OFF);
    lv_obj_move_foreground(speaker_page);
    lv_obj_clear_flag(speaker_page, LV_OBJ_FLAG_HIDDEN);
}
static void close_page(lv_event_t *event) {
    lv_obj_add_flag(lv_event_get_user_data(event), LV_OBJ_FLAG_HIDDEN);
}
static void view_photo(lv_event_t *event) {
    (void)event;
    if (!preview_pixels || demo_busy(&state))
        return;
    lv_img_set_src(full_image, &preview);
    lv_img_set_zoom(full_image, demo_image_zoom(preview.header.w, preview.header.h, 280, 260));
    lv_obj_align(full_image, LV_ALIGN_TOP_MID, 0, 64);
    lv_obj_clear_flag(viewer, LV_OBJ_FLAG_HIDDEN);
}
static void close_viewer(lv_event_t *event) {
    (void)event;
    lv_obj_add_flag(viewer, LV_OBJ_FLAG_HIDDEN);
}
static lv_obj_t *label(lv_obj_t *parent, const char *text, int y, int width) {
    lv_obj_t *obj = lv_label_create(parent);
    lv_label_set_text(obj, text);
    lv_obj_set_width(obj, width);
    lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(obj, LV_ALIGN_TOP_MID, 0, y);
    return obj;
}
static void style_button(lv_obj_t *button) {
    lv_obj_set_style_bg_color(button, lv_color_hex(0x247568), 0);
    lv_obj_set_style_text_color(button, lv_color_hex(0xf4f1ec), 0);
    lv_obj_set_style_bg_color(button, lv_color_hex(0xc9cfca), LV_STATE_DISABLED);
    lv_obj_set_style_text_color(button, lv_color_hex(0x53605a), LV_STATE_DISABLED);
}
static void build_ui(void) {
    lv_obj_t *screen = lv_scr_act();
    lv_obj_set_style_bg_color(screen, lv_color_hex(0xf4f1ec), 0);
    lv_obj_set_style_text_color(screen, lv_color_hex(0x293732), 0);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *title = label(screen, "Hardware lab / UX5", 26, 260);
#if LV_FONT_MONTSERRAT_20
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
#else
    lv_obj_set_style_text_font(title, LV_FONT_DEFAULT, 0);
#endif
    headline = label(screen, "Starting hardware...", 58, 280);
    lv_obj_t *area = lv_obj_create(screen);
    lv_obj_set_size(area, 240, 116);
    lv_obj_align(area, LV_ALIGN_TOP_MID, 0, 82);
    lv_obj_set_style_bg_color(area, lv_color_hex(0xe6eae5), 0);
    lv_obj_set_style_border_width(area, 0, 0);
    lv_obj_set_style_pad_all(area, 0, 0);
    lv_obj_clear_flag(area, LV_OBJ_FLAG_SCROLLABLE);
    empty = lv_label_create(area);
    lv_label_set_text(empty, "Photo: countdown + preview\nRecord first, then tap Play");
    lv_obj_set_style_text_align(empty, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(empty);
    image = lv_img_create(area);
    lv_img_set_size_mode(image, LV_IMG_SIZE_MODE_REAL);
    lv_obj_add_flag(image, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(image, view_photo, LV_EVENT_CLICKED, NULL);
    lv_obj_center(image);
    lv_obj_add_flag(image, LV_OBJ_FLAG_HIDDEN);
    cue = lv_label_create(area);
#if LV_FONT_MONTSERRAT_48
    lv_obj_set_style_text_font(cue, &lv_font_montserrat_48, 0);
#elif LV_FONT_MONTSERRAT_20
    lv_obj_set_style_text_font(cue, &lv_font_montserrat_20, 0);
#endif
    lv_obj_set_style_text_color(cue, lv_color_hex(0x247568), 0);
    lv_obj_center(cue);
    lv_obj_add_flag(cue, LV_OBJ_FLAG_HIDDEN);
    meter = lv_bar_create(area);
    lv_obj_set_size(meter, 180, 8);
    lv_obj_align(meter, LV_ALIGN_BOTTOM_MID, 0, -8);
    lv_obj_set_style_bg_color(meter, lv_color_hex(0x247568), LV_PART_INDICATOR);
    lv_obj_add_flag(meter, LV_OBJ_FLAG_HIDDEN);
    detail = label(screen, "Checking camera and audio", 202, 280);
    lv_obj_set_height(detail, 44);
    lv_label_set_long_mode(detail, LV_LABEL_LONG_DOT);
    progress = lv_bar_create(screen);
    lv_obj_set_size(progress, 220, 6);
    lv_obj_align(progress, LV_ALIGN_TOP_MID, 0, 252);
    lv_obj_set_style_bg_color(progress, lv_color_hex(0xe6eae5), 0);
    lv_obj_set_style_bg_color(progress, lv_color_hex(0x247568), LV_PART_INDICATOR);
    const char *labels[] = {"Photo", "Record", "Play", "Test speaker", "Audio"};
    for (int i = 0; i < 5; ++i) {
        buttons[i] = lv_btn_create(screen);
        lv_obj_set_size(buttons[i], i >= 3 ? 124 : 84, i >= 3 ? 44 : 48);
        lv_obj_set_style_pad_hor(buttons[i], 8, 0);
        lv_obj_align(buttons[i], LV_ALIGN_TOP_MID, i >= 3 ? (i == 3 ? -68 : 68) : (i - 1) * 94, i >= 3 ? 326 : 268);
        style_button(buttons[i]);
        lv_obj_t *text = lv_label_create(buttons[i]);
        lv_label_set_text(text, labels[i]);
#if LV_FONT_MONTSERRAT_16
        lv_obj_set_style_text_font(text, &lv_font_montserrat_16, 0);
#else
        lv_obj_set_style_text_font(text, LV_FONT_DEFAULT, 0);
#endif
        lv_obj_center(text);
        lv_obj_add_event_cb(buttons[i],
                            i == 4   ? open_audio
                            : i == 3 ? open_speaker
                                     : clicked,
                            LV_EVENT_CLICKED, (void *)(intptr_t)(i + 1));
    }
    viewer = lv_obj_create(screen);
    lv_obj_set_size(viewer, 412, 412);
    lv_obj_center(viewer);
    lv_obj_set_style_bg_color(viewer, lv_color_hex(0xf4f1ec), 0);
    lv_obj_set_style_border_width(viewer, 0, 0);
    lv_obj_set_style_pad_all(viewer, 0, 0);
    lv_obj_clear_flag(viewer, LV_OBJ_FLAG_SCROLLABLE);
    full_image = lv_img_create(viewer);
    lv_img_set_size_mode(full_image, LV_IMG_SIZE_MODE_REAL);
    lv_obj_t *back = lv_btn_create(viewer);
    style_button(back);
    lv_obj_set_size(back, 124, 46);
    lv_obj_align(back, LV_ALIGN_TOP_MID, 0, 346);
    lv_obj_t *back_label = lv_label_create(back);
    lv_label_set_text(back_label, "Back");
    lv_obj_center(back_label);
    lv_obj_add_event_cb(back, close_viewer, LV_EVENT_CLICKED, NULL);
    lv_obj_add_flag(viewer, LV_OBJ_FLAG_HIDDEN);

    audio_page = lv_obj_create(screen);
    lv_obj_set_size(audio_page, 412, 412);
    lv_obj_center(audio_page);
    lv_obj_set_style_bg_color(audio_page, lv_color_hex(0xf4f1ec), 0);
    lv_obj_set_style_border_width(audio_page, 0, 0);
    lv_obj_set_style_pad_all(audio_page, 0, 0);
    lv_obj_clear_flag(audio_page, LV_OBJ_FLAG_SCROLLABLE);
    label(audio_page, "Audio settings", 48, 250);
    volume_label = label(audio_page, "", 92, 300);
    volume_slider = lv_slider_create(audio_page);
    lv_obj_set_style_bg_color(volume_slider, lv_color_hex(0x247568), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(volume_slider, lv_color_hex(0x247568), LV_PART_KNOB);
    lv_obj_set_size(volume_slider, 150, 16);
    lv_obj_align(volume_slider, LV_ALIGN_TOP_MID, 0, 146);
    lv_obj_set_ext_click_area(volume_slider, 16);
    lv_slider_set_range(volume_slider, 0, DEMO_OUTPUT_MAX);
    lv_obj_add_event_cb(volume_slider, request_volume, LV_EVENT_RELEASED, NULL);
    volume_down = lv_btn_create(audio_page);
    volume_up = lv_btn_create(audio_page);
    lv_obj_t *step_buttons[] = {volume_down, volume_up};
    for (int i = 0; i < 2; ++i) {
        style_button(step_buttons[i]);
        lv_obj_set_size(step_buttons[i], 48, 48);
        lv_obj_align(step_buttons[i], LV_ALIGN_TOP_MID, i ? 116 : -116, 130);
        lv_obj_t *text = lv_label_create(step_buttons[i]);
        lv_label_set_text(text, i ? "+" : "-");
        lv_obj_center(text);
        lv_obj_add_event_cb(step_buttons[i], request_volume, LV_EVENT_CLICKED, (void *)(intptr_t)(i ? 5 : -5));
    }
    audio_hint = label(audio_page, "", 190, 280);
    boost_button = lv_btn_create(audio_page);
    style_button(boost_button);
    lv_obj_set_size(boost_button, 232, 48);
    lv_obj_align(boost_button, LV_ALIGN_TOP_MID, 0, 248);
    boost_label = lv_label_create(boost_button);
    lv_obj_center(boost_label);
    lv_obj_add_event_cb(boost_button, change_boost, LV_EVENT_CLICKED, NULL);
    label(audio_page, "Output controls all speaker sound.\nReplay boost changes Play only.", 308, 290);
    lv_obj_t *audio_back = lv_btn_create(audio_page);
    style_button(audio_back);
    lv_obj_set_size(audio_back, 124, 44);
    lv_obj_align(audio_back, LV_ALIGN_TOP_MID, 0, 352);
    lv_obj_t *audio_back_text = lv_label_create(audio_back);
    lv_label_set_text(audio_back_text, "Back");
    lv_obj_center(audio_back_text);
    lv_obj_add_event_cb(audio_back, close_page, LV_EVENT_CLICKED, audio_page);
    audio_settings_locked("Adjust output, then test the speaker.");
    lv_obj_add_flag(audio_page, LV_OBJ_FLAG_HIDDEN);

    speaker_page = lv_obj_create(screen);
    lv_obj_set_size(speaker_page, 412, 412);
    lv_obj_center(speaker_page);
    lv_obj_set_style_bg_color(speaker_page, lv_color_hex(0xf4f1ec), 0);
    lv_obj_set_style_border_width(speaker_page, 0, 0);
    lv_obj_set_style_pad_all(speaker_page, 0, 0);
    lv_obj_clear_flag(speaker_page, LV_OBJ_FLAG_SCROLLABLE);
    label(speaker_page, "Speaker test", 44, 250);
    speaker_headline = label(speaker_page, "Ready to test", 82, 280);
    speaker_detail = label(speaker_page, "Choose a voice or tone", 116, 280);
    lv_obj_set_height(speaker_detail, 52);
    lv_label_set_long_mode(speaker_detail, LV_LABEL_LONG_DOT);
    const char *speaker_labels[] = {"Test voice", "Test tone"};
    const int speaker_actions[] = {DEMO_SPEAKER_TEST, DEMO_TONE_TEST};
    for (int i = 0; i < 2; i++) {
        speaker_buttons[i] = lv_btn_create(speaker_page);
        style_button(speaker_buttons[i]);
        lv_obj_set_size(speaker_buttons[i], 210, 52);
        lv_obj_align(speaker_buttons[i], LV_ALIGN_TOP_MID, 0, 178 + i * 64);
        lv_obj_t *text = lv_label_create(speaker_buttons[i]);
        lv_label_set_text(text, speaker_labels[i]);
        lv_obj_center(text);
        lv_obj_add_event_cb(speaker_buttons[i], clicked, LV_EVENT_CLICKED, (void *)(intptr_t)speaker_actions[i]);
    }
    speaker_progress = lv_bar_create(speaker_page);
    lv_obj_set_size(speaker_progress, 220, 6);
    lv_obj_align(speaker_progress, LV_ALIGN_TOP_MID, 0, 310);
    lv_obj_set_style_bg_color(speaker_progress, lv_color_hex(0xe6eae5), 0);
    lv_obj_set_style_bg_color(speaker_progress, lv_color_hex(0x247568), LV_PART_INDICATOR);
    lv_obj_t *speaker_back = lv_btn_create(speaker_page);
    speaker_buttons[2] = lv_btn_create(speaker_page);
    lv_obj_t *speaker_footer[] = {speaker_back, speaker_buttons[2]};
    for (int i = 0; i < 2; i++) {
        style_button(speaker_footer[i]);
        lv_obj_set_size(speaker_footer[i], 110, 44);
        lv_obj_align(speaker_footer[i], LV_ALIGN_TOP_MID, i ? 60 : -60, 328);
        lv_obj_t *text = lv_label_create(speaker_footer[i]);
        lv_label_set_text(text, i ? "Audio" : "Back");
        lv_obj_center(text);
        lv_obj_add_event_cb(speaker_footer[i], i ? open_audio : close_page, LV_EVENT_CLICKED, speaker_page);
    }
    lv_obj_add_flag(speaker_page, LV_OBJ_FLAG_HIDDEN);
    controls();
}
static esp_err_t preview_photo(demo_image_t *capture) {
    if (!watcher_hw_display_lock(1000)) return ESP_ERR_TIMEOUT;
    lv_img_cache_invalidate_src(&preview);
    uint8_t *previous = preview_pixels;
    preview = (lv_img_dsc_t){.header = {.cf = LV_IMG_CF_TRUE_COLOR, .w = capture->width, .h = capture->height},
                             .data_size = capture->bytes, .data = capture->pixels};
    preview_pixels = capture->pixels;
    capture->pixels = NULL; /* LVGL now owns this application's retained preview. */
    lv_img_set_src(image, &preview);
    lv_img_set_zoom(image, demo_image_zoom(preview.header.w, preview.header.h, 224, 112));
    lv_obj_center(image);
    lv_obj_clear_flag(image, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(empty, LV_OBJ_FLAG_HIDDEN);
    heap_caps_free(previous);
    watcher_hw_display_unlock();
    return ESP_OK;
}
static void countdown(const char *title, const char *hint) {
    for (int seconds = 3; seconds > 0; --seconds) {
        char count[2] = {(char)('0' + seconds), 0};
        if (watcher_hw_display_lock(0)) {
            state.phase = DEMO_PREPARING;
            show_locked(title, hint, (3 - seconds) * 33);
            lv_label_set_text(cue, count);
            lv_obj_center(cue);
            watcher_hw_display_unlock();
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
static void take_photo(void) {
    countdown("Photo in...", "Hold still; wait for the countdown");
    show(DEMO_CAPTURING, "Taking photo...", "Keep the camera steady", 25);
    demo_image_t capture;
    esp_err_t ret = demo_image_capture(LV_COLOR_16_SWAP, &capture);
    ESP_LOGI(TAG, "photo: %s", esp_err_to_name(ret));
    if (ret == ESP_OK) {
        show(DEMO_DECODING, "Opening photo...", "Checking and decoding JPEG", 60);
        ret = preview_photo(&capture);
    }
    demo_image_release(&capture);
    if (ret != ESP_OK) {
        failed("Photo", ret,
               preview_pixels ? "Previous photo kept; check PTL pairing" : "Check PTL firmware; tap Photo to retry");
        return;
    }
    show(DEMO_COMPLETE, "Photo ready", "Tap the photo to enlarge; Back to return", 100);
}
static int read_record(void *context, int16_t *pcm, size_t bytes) {
    (void)context;
    return watcher_hw_audio_read(pcm, bytes);
}
static void record_progress(void *context, size_t done, size_t count, uint32_t peak) {
    (void)context;
    char message[80];
    snprintf(message, sizeof(message), "%.1f s left | Mic peak %d dBFS\nSpeak 15-30 cm from the mic",
             (count - done) / (double)SAMPLE_RATE, demo_peak_dbfs(peak));
    show(DEMO_RECORDING, "Recording...", message, (int)(done * 100 / count));
    if (watcher_hw_display_lock(0)) {
        lv_obj_clear_flag(meter, LV_OBJ_FLAG_HIDDEN);
        lv_bar_set_value(meter, (int)demo_mic_meter(peak), LV_ANIM_OFF);
        watcher_hw_display_unlock();
    }
}
static void record_audio(int16_t *pcm) {
    countdown("Record in...", "Wait for REC, then speak for 3 seconds");
    show(DEMO_RECORDING, "Recording...", "Speak now; watch the mic level", 0);
    demo_pcm_stats_t total;
    const demo_record_sink_t sink = {NULL, read_record, record_progress};
    esp_err_t ret = (esp_err_t)demo_record_run(pcm, CHUNK_SAMPLES * AUDIO_CHUNKS, CHUNK_SAMPLES, &sink, &total);
    if (ret != ESP_OK) { failed("Record", ret, "No complete recording; check microphone"); return; }
    unsigned mean = demo_pcm_mean(&total);
    recording_peak = total.peak;
    ESP_LOGI(TAG, "UX5 record: samples=%u peak=%u peak_dbfs=%d mean_abs=%u", (unsigned)total.samples,
             (unsigned)total.peak, demo_peak_dbfs(total.peak), mean);
    char result[112];
    snprintf(result, sizeof(result), "Mic peak %d dBFS | Tap Play\n%s", demo_peak_dbfs(total.peak),
             mean < 32 ? "Quiet input; speak closer or use replay boost" : "Use Audio to adjust output / replay boost");
    if (watcher_hw_display_lock(0)) {
        demo_recorded(&state, true);
        demo_finish(&state, true);
        show_locked(mean < 32 ? "Recorded: very quiet" : "Recording ready", result, 100);
        watcher_hw_display_unlock();
    }
}
typedef struct {
    const char *title;
    const char *message;
} playback_ui_t;
static int write_output(void *context, int16_t *scratch, size_t bytes) {
    (void)context;
    return watcher_hw_audio_write(scratch, bytes);
}
static int drain_output(void *context) {
    (void)context;
    return watcher_hw_audio_drain(1000);
}
static void playback_progress(void *context, size_t submitted, size_t total) {
    const playback_ui_t *ui = context;
    show(DEMO_PLAYING, ui->title, ui->message, (int)(submitted * 100 / total));
}
static esp_err_t play_source(const demo_playback_source_t *source, int16_t *scratch, playback_ui_t *ui,
                             demo_playback_result_t *result) {
    const demo_playback_sink_t sink = {ui, write_output, drain_output, playback_progress};
    int ret = demo_playback_run(source, &sink, scratch, CHUNK_SAMPLES, result);
    return ret == -1 ? ESP_ERR_INVALID_ARG : (esp_err_t)ret;
}
static void play_audio(const int16_t *pcm, int16_t *scratch) {
    unsigned volume, requested_boost;
    if (!watcher_hw_display_lock(0)) {
        failed("Playback", ESP_ERR_INVALID_STATE, "Try again");
        return;
    }
    volume = output_volume;
    requested_boost = voice_boost;
    watcher_hw_display_unlock();
    unsigned boost = demo_safe_boost(requested_boost, recording_peak);
    char message[96];
    snprintf(message, sizeof(message), "Output %u/95 | Replay %ux%s\n%s", volume, boost,
             boost < requested_boost ? " (limited)" : "",
             volume ? "Listen for your voice" : "Muted; raise Output in Audio");
    const demo_playback_source_t source = {pcm, CHUNK_SAMPLES * AUDIO_CHUNKS, boost, 0};
    playback_ui_t ui = {"Playing recording...", message};
    demo_playback_result_t result;
    esp_err_t ret = play_source(&source, scratch, &ui, &result);
    ESP_LOGI(TAG, "UX5 playback: %s volume=%u requested_boost=%u effective_boost=%u samples=%u clipped=%u",
             esp_err_to_name(ret), volume, requested_boost, boost, (unsigned)result.submitted,
             (unsigned)result.clipped);
    if (ret != ESP_OK) {
        failed("Playback", ret, "Recording kept; check speaker and retry");
        return;
    }
    show(DEMO_COMPLETE, "Playback finished", "No sound? Tap Test speaker to check output", 100);
}
static void test_speaker(int16_t *scratch, bool tone) {
    unsigned volume;
    if (!watcher_hw_display_lock(0)) {
        failed("Speaker test", ESP_ERR_INVALID_STATE, "Try again");
        return;
    }
    volume = output_volume;
    watcher_hw_display_unlock();
    char message[80];
    snprintf(message, sizeof(message), "Output %u/95 | %s\n%s", volume, tone ? "660 Hz" : "English voice",
             volume ? (tone ? "Listen for a 1-second tone" : "Listen: speaker test, one two three")
                    : "Muted; raise Output in Audio");
    const demo_playback_source_t source = {tone ? NULL : demo_speech_pcm, tone ? SAMPLE_RATE : DEMO_SPEECH_SAMPLES, 1,
                                           tone ? SAMPLE_RATE : 0};
    playback_ui_t ui = {tone ? "Playing test tone..." : "Playing test voice...", message};
    demo_playback_result_t result;
    esp_err_t ret = play_source(&source, scratch, &ui, &result);
    ESP_LOGI(TAG, "UX5 speaker %s: %s rate=%u volume=%u samples=%u", tone ? "tone" : "voice", esp_err_to_name(ret),
             SAMPLE_RATE, volume, (unsigned)result.submitted);
    if (ret != ESP_OK) {
        failed("Speaker test", ret, "Check output; retry after sound stops");
        return;
    }
    snprintf(message, sizeof(message), "Output %u/95%s\n%s", volume, volume ? "" : " (muted)",
             volume ? "Heard it? If not, check Audio output." : "Muted; raise Output in Audio");
    show(DEMO_COMPLETE, tone ? "Tone test finished" : "Voice test finished", message, 100);
}
static void apply_volume(unsigned requested) {
    esp_err_t ret = watcher_hw_audio_volume((uint8_t)requested);
    ESP_LOGI(TAG, "UX5 volume: requested=%u result=%s", requested, esp_err_to_name(ret));
    if (!watcher_hw_display_lock(0))
        return;
    if (ret == ESP_OK)
        output_volume = requested;
    demo_finish(&state, ret == ESP_OK);
    show_locked(ret == ESP_OK ? "Output updated" : "Volume change failed",
                ret == ESP_OK ? "Tap Test speaker to check the sound" : "Setting not confirmed; retry or restart", 0);
    audio_settings_locked(
        ret == ESP_OK ? (output_volume ? "Applied. Back, then Test speaker." : "Muted. Raise Output to hear sound.")
                      : "Setting not confirmed. Retry / restart.");
    watcher_hw_display_unlock();
}
static void media_task(void *unused) {
    (void)unused;
    esp_err_t camera = watcher_hw_camera_init();
    esp_err_t audio = watcher_hw_audio_init(SAMPLE_RATE, DEMO_OUTPUT_DEFAULT);
    int16_t *pcm = NULL, *scratch = NULL;
    if (audio == ESP_OK) {
        pcm = heap_caps_malloc(CHUNK_SAMPLES * AUDIO_CHUNKS * sizeof(int16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        scratch = heap_caps_malloc(CHUNK_SAMPLES * sizeof(int16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (!pcm || !scratch) {
            heap_caps_free(pcm);
            heap_caps_free(scratch);
            pcm = scratch = NULL;
            (void)watcher_hw_audio_close();
            audio = ESP_ERR_NO_MEM;
        }
    }
    ESP_LOGI(TAG, "camera: %s", esp_err_to_name(camera));
    ESP_LOGI(TAG, "audio: %s", esp_err_to_name(audio));
    if (watcher_hw_display_lock(0)) {
        char message[112];
        state.camera_ready = camera == ESP_OK;
        state.audio_ready = audio == ESP_OK;
        state.phase = camera == ESP_OK && audio == ESP_OK ? DEMO_IDLE : DEMO_ERROR;
        snprintf(message, sizeof(message), "Camera: %s\nAudio: %s%s", esp_err_to_name(camera), esp_err_to_name(audio),
                 state.phase == DEMO_ERROR ? "\nCheck wiring / pairing; restart" : "");
        show_locked(state.phase == DEMO_IDLE ? "Choose an experiment" : "Some hardware unavailable", message, 0);
        watcher_hw_display_unlock();
    }
    for (;;) {
        demo_command_t command;
        if (xQueueReceive(actions, &command, portMAX_DELAY) == pdTRUE) {
            int action = command.action;
            if (action == DEMO_PHOTO)
                take_photo();
            else if (action == DEMO_RECORD && pcm)
                record_audio(pcm);
            else if (action == DEMO_PLAY && pcm && scratch)
                play_audio(pcm, scratch);
            else if ((action == DEMO_SPEAKER_TEST || action == DEMO_TONE_TEST) && scratch)
                test_speaker(scratch, action == DEMO_TONE_TEST);
            else if (action == DEMO_VOLUME)
                apply_volume(command.value);
        }
    }
}
esp_err_t head_demo_start(void) {
    if (actions)
        return ESP_ERR_INVALID_STATE;
    esp_err_t ret = watcher_hw_display_init();
    ESP_LOGI(TAG, "Hardware lab UX5: Photo / Record / Play / speaker voice+tone / Audio, display: %s",
             esp_err_to_name(ret));
    if (ret != ESP_OK)
        return ret;
    actions = xQueueCreate(1, sizeof(demo_command_t));
    if (!actions)
        return ESP_ERR_NO_MEM;
    if (!watcher_hw_display_lock(1000)) {
        vQueueDelete(actions);
        actions = NULL;
        return ESP_ERR_TIMEOUT;
    }
    build_ui();
    watcher_hw_display_unlock();
    if (xTaskCreate(media_task, "head_media", 8192, NULL, 4, NULL) != pdPASS) {
        show(DEMO_ERROR, "Could not start", "Not enough memory; restart the example", 0);
        vQueueDelete(actions);
        actions = NULL;
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}
