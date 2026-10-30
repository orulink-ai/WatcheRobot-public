#include "check_media.h"
#include "demo_audio.h"
#include "demo_playback.h"
#include "demo_record.h"
#include "demo_speech.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "watcher_hw_audio.h"
#include "watcher_hw_camera.h"
#include "lvgl.h"
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>

enum { RATE = 16000, CHUNK = 320, RECORD_SAMPLES = RATE * 3 };
/* This example's replay policy, not an SDK gain or microphone setting. */
enum { REPLAY_BOOST = 32 };
enum { CANCELLED_BY_USER = 0x7001 };
static const char *TAG = "check_media";
static QueueHandle_t commands, notifications, jpeg_frames;
static SemaphoreHandle_t decoder_drained;
static atomic_bool cancelled, stream_accept;
static unsigned modules;
static atomic_uint frames;
static int16_t *recording, *scratch;
static bool recording_valid;
static uint32_t recording_peak;
static esp_err_t audio_cleanup_error;
static check_media_command_t current;
static int64_t stream_started;
typedef struct { uint8_t *data; size_t bytes; uint32_t attempt; } jpeg_frame_t;

static void send_event(check_media_kind_t kind, esp_err_t error, const char *text, unsigned percent, unsigned peak) {
    check_media_event_t event = {.kind = kind, .attempt = current.attempt, .error = error, .modules = modules,
                                .progress = percent, .peak = peak, .frames = frames};
    snprintf(event.text, sizeof(event.text), "%s", text ? text : "");
    /* Only progress may be dropped. Critical ownership/results remain reliable. */
    xQueueSend(notifications, &event, kind == CHECK_MEDIA_PROGRESS ? 0 : portMAX_DELAY);
}
static void publish_image(demo_image_t *image, uint32_t attempt) {
    check_media_event_t event = {.kind = CHECK_MEDIA_IMAGE, .attempt = attempt,
                                .image = *image, .frames = atomic_load(&frames) + 1};
    if (xQueueSend(notifications, &event, 0) != pdTRUE) demo_image_release(image);
    else atomic_fetch_add(&frames, 1);
    memset(image, 0, sizeof(*image));
}
static bool delay_cancel(unsigned ms) {
    for (unsigned done = 0; done < ms && !atomic_load(&cancelled); done += 20) vTaskDelay(pdMS_TO_TICKS(20));
    return !atomic_load(&cancelled);
}
static bool countdown(void) {
    for (int n = 3; n > 0; n--) {
        char text[2] = {(char)('0' + n), 0};
        send_event(CHECK_MEDIA_PROGRESS, ESP_OK, text, (unsigned)(3 - n) * 33, 0);
        if (!delay_cancel(1000)) return false;
    }
    return true;
}
static void copy_jpeg(const uint8_t *jpeg, size_t bytes, uint32_t timestamp, void *context) {
    (void)timestamp; (void)context;
    if (!atomic_load(&stream_accept) || !demo_jpeg_valid(jpeg, bytes)) return;
    jpeg_frame_t frame = {.bytes = bytes, .attempt = current.attempt};
    frame.data = heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!frame.data) return;
    memcpy(frame.data, jpeg, bytes);
    if (xQueueSend(jpeg_frames, &frame, 0) != pdTRUE) heap_caps_free(frame.data);
}
static void decoder(void *ctx) {
    (void)ctx;
    for (;;) {
        jpeg_frame_t frame;
        if (xQueueReceive(jpeg_frames, &frame, portMAX_DELAY) != pdTRUE) continue;
        if (!frame.data) { xSemaphoreGive(decoder_drained); continue; }
        demo_image_t image;
        esp_err_t ret = demo_image_decode(frame.data, frame.bytes, LV_COLOR_16_SWAP, &image);
        heap_caps_free(frame.data);
        if (ret == ESP_OK) publish_image(&image, frame.attempt);
        else ESP_LOGW(TAG, "stream decode=%s", esp_err_to_name(ret));
    }
}
static void empty_frames(void) {
    jpeg_frame_t frame;
    while (xQueueReceive(jpeg_frames, &frame, 0) == pdTRUE) heap_caps_free(frame.data);
}
static int read_pcm(void *ctx, int16_t *pcm, size_t bytes) {
    (void)ctx;
    if (atomic_load(&cancelled)) return CANCELLED_BY_USER;
    return watcher_hw_audio_read(pcm, bytes);
}
static void record_progress(void *ctx, size_t done, size_t total, uint32_t peak) {
    (void)ctx;
    char text[64];
    snprintf(text, sizeof(text), "REC %.1fs | %d dBFS", (total - done) / (double)RATE, demo_peak_dbfs(peak));
    send_event(CHECK_MEDIA_PROGRESS, ESP_OK, text, (unsigned)(done * 100 / total), peak);
}
static int write_pcm(void *ctx, int16_t *pcm, size_t bytes) {
    (void)ctx;
    if (atomic_load(&cancelled)) return CANCELLED_BY_USER;
    return watcher_hw_audio_write(pcm, bytes);
}
static int drain_pcm(void *ctx) {
    (void)ctx;
    audio_cleanup_error = watcher_hw_audio_drain(1000);
    return audio_cleanup_error;
}
static void playback_progress(void *ctx, size_t done, size_t total) {
    (void)ctx;
    unsigned progress = current.operation == HC_OP_COMBINED ? (unsigned)((esp_timer_get_time() - stream_started) / 200000) :
        (unsigned)(done * 100 / total);
    char text[32] = "Playing";
    if (current.operation == HC_OP_REPLAY)
        snprintf(text, sizeof(text), "Playing | %ux", demo_safe_boost(REPLAY_BOOST, recording_peak));
    send_event(CHECK_MEDIA_PROGRESS, ESP_OK, text, progress, 0);
}
static esp_err_t play(bool tone, bool replay) {
    if (!scratch || (replay && !recording_valid)) return ESP_ERR_INVALID_STATE;
    unsigned boost = replay ? demo_safe_boost(REPLAY_BOOST, recording_peak) : 1;
    demo_playback_source_t source = {
        .pcm = tone ? NULL : replay ? recording : demo_speech_pcm,
        .samples = tone ? RATE : replay ? RECORD_SAMPLES : DEMO_SPEECH_SAMPLES,
        .boost = boost, .tone_rate = tone ? RATE : 0};
    demo_playback_sink_t sink = {NULL, write_pcm, drain_pcm, playback_progress};
    demo_playback_result_t result;
    esp_err_t ret = (esp_err_t)demo_playback_run(&source, &sink, scratch, CHUNK, &result);
    ESP_LOGI(TAG, "op=%u attempt=%lu output=%u boost=%u samples=%u clipped=%u error=%s",
             current.operation, (unsigned long)current.attempt, current.volume, boost,
             (unsigned)result.submitted, (unsigned)result.clipped, esp_err_to_name(ret));
    return ret;
}
static esp_err_t run_stream(bool combined) {
    empty_frames();
    atomic_store(&stream_accept, true);
    esp_err_t ret = watcher_hw_camera_stream(4, copy_jpeg, NULL);
    bool started = ret == ESP_OK;
    int64_t start = esp_timer_get_time(), duration = (combined ? 20 : 5) * 1000000LL;
    stream_started = start;
    if (started && combined) ret = play(false, false);
    while (ret == ESP_OK && !atomic_load(&cancelled) && esp_timer_get_time() - start < duration) {
        char text[64];
        snprintf(text, sizeof(text), "%u frames | %llds left", frames,
                 (long long)((duration - (esp_timer_get_time() - start)) / 1000000 + 1));
        send_event(CHECK_MEDIA_PROGRESS, ESP_OK, text,
                   (unsigned)((esp_timer_get_time() - start) * 100 / duration), 0);
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    /* Also stop after a failed start: it may have partially acquired resources. */
    atomic_store(&stream_accept, false);
    esp_err_t cleanup = watcher_hw_camera_stop();
    empty_frames();
    /* FIFO fence: DONE cannot overtake the last borrowed/decoded frame.
     * Decoder never touches audio or camera lifecycle. */
    (void)xSemaphoreTake(decoder_drained, 0);
    const jpeg_frame_t fence = {0};
    if (xQueueSend(jpeg_frames, &fence, pdMS_TO_TICKS(2000)) != pdTRUE ||
        xSemaphoreTake(decoder_drained, pdMS_TO_TICKS(2000)) != pdTRUE) cleanup = ESP_ERR_TIMEOUT;
    if (cleanup != ESP_OK) { modules &= ~HC_CAMERA_MODULE; ret = cleanup; }
    if (ret == ESP_OK && !atomic_load(&cancelled) && frames < 2) ret = ESP_ERR_INVALID_RESPONSE;
    ESP_LOGI(TAG, "stream attempt=%lu frames=%u cleanup=%s", (unsigned long)current.attempt, frames,
             esp_err_to_name(cleanup));
    return ret;
}
static esp_err_t execute(void) {
    frames = 0;
    switch (current.operation) {
    case HC_OP_PHOTO: {
        if (!countdown()) return ESP_OK;
        send_event(CHECK_MEDIA_PROGRESS, ESP_OK, "Capturing", 40, 0);
        demo_image_t image;
        esp_err_t ret = demo_image_capture(LV_COLOR_16_SWAP, &image);
        if (ret == ESP_OK && !atomic_load(&cancelled)) publish_image(&image, current.attempt);
        if (ret == ESP_OK && !atomic_load(&cancelled) && !atomic_load(&frames)) ret = ESP_ERR_NO_MEM;
        demo_image_release(&image);
        ESP_LOGI(TAG, "photo attempt=%lu jpeg_decode=%s frames=%u", (unsigned long)current.attempt,
                 esp_err_to_name(ret), frames);
        return ret;
    }
    case HC_OP_STREAM: return run_stream(false);
    case HC_OP_COMBINED: return run_stream(true);
    case HC_OP_RECORD: {
        recording_valid = false;
        if (!recording) return ESP_ERR_NO_MEM;
        if (!countdown()) return ESP_OK;
        demo_record_sink_t sink = {NULL, read_pcm, record_progress};
        demo_pcm_stats_t stats;
        esp_err_t ret = (esp_err_t)demo_record_run(recording, RECORD_SAMPLES, CHUNK, &sink, &stats);
        recording_valid = ret == ESP_OK && !atomic_load(&cancelled) && stats.samples == RECORD_SAMPLES;
        recording_peak = stats.peak;
        ESP_LOGI(TAG, "record attempt=%lu samples=%u peak=%u mean_abs=%u complete=%d error=%s",
                 (unsigned long)current.attempt, (unsigned)stats.samples, (unsigned)stats.peak,
                 demo_pcm_mean(&stats), recording_valid, esp_err_to_name(ret));
        return ret;
    }
    case HC_OP_VOICE: return play(false, false);
    case HC_OP_TONE: return play(true, false);
    case HC_OP_REPLAY: return play(false, true);
    case HC_OP_VOLUME: return watcher_hw_audio_volume((uint8_t)current.volume);
    default: return ESP_ERR_INVALID_ARG;
    }
}
static void owner(void *ctx) {
    (void)ctx;
    esp_err_t camera = watcher_hw_camera_init();
    if (camera == ESP_OK) modules |= HC_CAMERA_MODULE;
    esp_err_t audio = watcher_hw_audio_init(RATE, DEMO_OUTPUT_DEFAULT);
    if (audio == ESP_OK) {
        recording = heap_caps_malloc(RECORD_SAMPLES * sizeof(*recording), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        scratch = heap_caps_malloc(CHUNK * sizeof(*scratch), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (recording && scratch) modules |= HC_AUDIO;
        else {
            heap_caps_free(recording); heap_caps_free(scratch); recording = scratch = NULL;
            (void)watcher_hw_audio_close(); audio = ESP_ERR_NO_MEM;
        }
    }
    ESP_LOGI(TAG, "init camera=%s audio=%s", esp_err_to_name(camera), esp_err_to_name(audio));
    send_event(CHECK_MEDIA_INIT, ESP_OK, "Ready", 0, 0);
    for (;;) {
        if (xQueueReceive(commands, &current, portMAX_DELAY) != pdTRUE) continue;
        audio_cleanup_error = ESP_OK;
        esp_err_t ret = execute();
        if (ret == CANCELLED_BY_USER) ret = ESP_OK;
        if (audio_cleanup_error != ESP_OK) {
            ret = audio_cleanup_error;
            modules &= ~HC_AUDIO;
            ESP_LOGE(TAG, "drain failed=%s close=%s", esp_err_to_name(ret), esp_err_to_name(watcher_hw_audio_close()));
        }
        send_event(CHECK_MEDIA_DONE, ret, ret == ESP_OK ? "Ready to review" : esp_err_to_name(ret), 100, 0);
    }
}
esp_err_t check_media_start(QueueHandle_t events) {
    if (commands || !events) return ESP_ERR_INVALID_STATE;
    notifications = events;
    commands = xQueueCreate(1, sizeof(check_media_command_t));
    jpeg_frames = xQueueCreate(2, sizeof(jpeg_frame_t));
    decoder_drained = xSemaphoreCreateBinary();
    if (!commands || !jpeg_frames || !decoder_drained) goto fail;
    TaskHandle_t decoder_task = NULL;
    if (xTaskCreate(decoder, "check_decode", 6144, NULL, 3, &decoder_task) != pdPASS) goto fail;
    if (xTaskCreate(owner, "check_media", 8192, NULL, 4, NULL) != pdPASS) { vTaskDelete(decoder_task); goto fail; }
    return ESP_OK;
fail:
    if (commands) vQueueDelete(commands);
    if (jpeg_frames) vQueueDelete(jpeg_frames);
    if (decoder_drained) vSemaphoreDelete(decoder_drained);
    commands = jpeg_frames = NULL;
    return ESP_ERR_NO_MEM;
}
bool check_media_submit(const check_media_command_t *command) {
    if (!commands || !command) return false;
    atomic_store(&cancelled, false);
    return xQueueSend(commands, command, 0) == pdTRUE;
}
void check_media_cancel(void) { atomic_store(&cancelled, true); }
