#include "hal_camera.h"

#include <stdio.h>
#include <string.h>

#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/idf_additions.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "hal_camera_jpeg_integrity.h"
#include "hx_media_protocol.h"
#include "hx_model_identity.h"
#include "hx_photo_warmup.h"
#include "mbedtls/base64.h"
#include "watcher_face_frame.h"
#include "watcher_hx_control.h"
#include "watcher_hx_ptl.h"

#define TAG "HAL_CAMERA_PTL"
#define PTL_FRAME_WIDTH ((int)HX_MEDIA_SOURCE_WIDTH)
#define PTL_FRAME_HEIGHT ((int)HX_MEDIA_SOURCE_HEIGHT)
#define PTL_READ_TIMEOUT_MS 500U
#define PTL_STOP_TIMEOUT_MS 2000U
#define PTL_TASK_STACK_SIZE 6144U
#define PTL_TASK_PRIORITY 5U
#define PTL_ONE_SHOT_STALE_DRAIN_LIMIT 3U

typedef struct {
    SemaphoreHandle_t lock;
    SemaphoreHandle_t stopped;
    SemaphoreHandle_t operation_lock;
    TaskHandle_t task;
    hal_camera_frame_cb_t frame_cb;
    void *frame_ctx;
    hal_camera_capture_activity_cb_t activity_cb;
    void *activity_ctx;
    bool initialized;
    bool streaming;
    bool capture_in_progress;
    bool deinitializing;
    bool stop_requested;
    bool preview_owned;
    bool stream_cleanup_pending;
    bool photo_prepared;
    bool inferencing, inference_stop;
    char *inference_image;
    SemaphoreHandle_t inference_stopped;
    TaskHandle_t inference_task;
    hal_camera_inference_cb_t inference_cb;
    void *inference_ctx;
    uint32_t inference_generation;
    hal_camera_inference_stats_t inference_stats;
} hal_camera_ptl_context_t;

static hal_camera_ptl_context_t s_ctx;
static unsigned s_selected_model = 4;
static bool s_catalog_valid;
static uint32_t s_model_crc[4];
/* Factory metadata was read using the official firmware. Bind its semantic
 * labels to the complete slot CRC; never call an arbitrary replacement a face. */
static const char *const s_model_names[4] = {"Person Detection", "Pet Detection", "Gesture Detection",
                                             "Face Detection"};

static void publish_activity(bool active) {
    hal_camera_capture_activity_cb_t callback;
    void *context;

    if (s_ctx.lock == NULL || xSemaphoreTake(s_ctx.lock, portMAX_DELAY) != pdTRUE) {
        return;
    }
    callback = s_ctx.activity_cb;
    context = s_ctx.activity_ctx;
    xSemaphoreGive(s_ctx.lock);
    if (callback != NULL) {
        callback(active, context);
    }
}

static void stream_task(void *arg) {
    (void)arg;
    publish_activity(true);
    for (;;) {
        watcher_hx_ptl_frame_t frame;
        hal_camera_frame_cb_t callback;
        void *context;
        bool stop;

        if (xSemaphoreTake(s_ctx.lock, portMAX_DELAY) != pdTRUE) {
            break;
        }
        stop = s_ctx.stop_requested;
        callback = s_ctx.frame_cb;
        context = s_ctx.frame_ctx;
        xSemaphoreGive(s_ctx.lock);
        if (stop) {
            break;
        }

        esp_err_t ret = watcher_hx_ptl_read(&frame, PTL_READ_TIMEOUT_MS);
        if (ret == ESP_OK && callback != NULL) {
            if (hal_camera_jpeg_dimensions_match(frame.jpeg, frame.jpeg_size, HX_MEDIA_SOURCE_WIDTH,
                                                 HX_MEDIA_SOURCE_HEIGHT)) {
                callback(frame.jpeg, frame.jpeg_size, frame.timestamp_ms, context);
            } else {
                ESP_LOGW(TAG, "Reject JPEG whose SOF dimensions are not %ux%u", (unsigned)HX_MEDIA_SOURCE_WIDTH,
                         (unsigned)HX_MEDIA_SOURCE_HEIGHT);
            }
        } else if (ret != ESP_ERR_TIMEOUT && ret != ESP_ERR_INVALID_CRC && ret != ESP_ERR_INVALID_RESPONSE) {
            ESP_LOGW(TAG, "PTL frame read failed: %s", esp_err_to_name(ret));
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }

    publish_activity(false);
    if (xSemaphoreTake(s_ctx.lock, portMAX_DELAY) == pdTRUE) {
        s_ctx.frame_cb = NULL;
        s_ctx.frame_ctx = NULL;
        xSemaphoreGive(s_ctx.lock);
    }
    xSemaphoreGive(s_ctx.stopped);
    /* No more callbacks, transport access or locks after the acknowledgement.
     * The operation owner deletes us using our retained handle. Self deletion
     * with Caps would allocate an internal-RAM cleanup task and abort on OOM.
     * The owner may preempt us before this suspension; external deletion is
     * safe in either case and synchronizes with the other CPU in IDF. */
    for (;;) {
        vTaskSuspend(NULL);
    }
}

esp_err_t hal_camera_set_capture_activity_callback(hal_camera_capture_activity_cb_t cb, void *ctx) {
    bool active;

    if (s_ctx.lock == NULL) {
        s_ctx.activity_cb = cb;
        s_ctx.activity_ctx = ctx;
        if (cb != NULL) {
            cb(false, ctx);
        }
        return ESP_OK;
    }
    xSemaphoreTake(s_ctx.lock, portMAX_DELAY);
    s_ctx.activity_cb = cb;
    s_ctx.activity_ctx = ctx;
    active = s_ctx.streaming || s_ctx.capture_in_progress;
    xSemaphoreGive(s_ctx.lock);
    if (cb != NULL) {
        cb(active, ctx);
    }
    return ESP_OK;
}

esp_err_t hal_camera_init(void) {
    if (s_ctx.initialized) {
        return ESP_OK;
    }
    if (s_ctx.lock == NULL) {
        s_ctx.lock = xSemaphoreCreateMutex();
    }
    if (s_ctx.stopped == NULL) {
        s_ctx.stopped = xSemaphoreCreateBinary();
    }
    if (s_ctx.operation_lock == NULL) {
        s_ctx.operation_lock = xSemaphoreCreateMutex();
    }
    ESP_RETURN_ON_FALSE(s_ctx.lock != NULL && s_ctx.stopped != NULL && s_ctx.operation_lock != NULL, ESP_ERR_NO_MEM,
                        TAG, "Create PTL camera synchronization");
    ESP_RETURN_ON_ERROR(watcher_hx_ptl_init(), TAG, "Initialize HX binary PTL transport");
    esp_err_t control = watcher_hx_control_init();
    if (control != ESP_OK) {
        (void)watcher_hx_ptl_deinit();
        return control;
    }
    s_ctx.initialized = true;
    ESP_LOGI(TAG, "Custom HX PTL camera backend selected (%dx%d)", PTL_FRAME_WIDTH, PTL_FRAME_HEIGHT);
    return ESP_OK;
}

esp_err_t hal_camera_deinit(void) {
    esp_err_t inference_stop = hal_camera_stop_inference();
    if (inference_stop != ESP_OK)
        return inference_stop;
    if (s_ctx.lock == NULL) {
        return ESP_OK;
    }
    ESP_RETURN_ON_FALSE(xSemaphoreTake(s_ctx.lock, portMAX_DELAY) == pdTRUE, ESP_FAIL, TAG,
                        "Lock PTL camera state for deinit");
    if (!s_ctx.initialized) {
        xSemaphoreGive(s_ctx.lock);
        return ESP_OK;
    }
    if (s_ctx.deinitializing) {
        xSemaphoreGive(s_ctx.lock);
        return ESP_ERR_INVALID_STATE;
    }
    s_ctx.deinitializing = true;
    xSemaphoreGive(s_ctx.lock);

    esp_err_t stop_result = hal_camera_stop();
    if (stop_result != ESP_OK) {
        xSemaphoreTake(s_ctx.lock, portMAX_DELAY);
        s_ctx.deinitializing = false;
        xSemaphoreGive(s_ctx.lock);
        ESP_LOGE(TAG, "PTL camera stop timed out; retaining transport and synchronization resources");
        return stop_result;
    }

    if (xSemaphoreTake(s_ctx.operation_lock, portMAX_DELAY) != pdTRUE) {
        xSemaphoreTake(s_ctx.lock, portMAX_DELAY);
        s_ctx.deinitializing = false;
        xSemaphoreGive(s_ctx.lock);
        ESP_LOGE(TAG, "Lock PTL camera operation for deinit failed");
        return ESP_FAIL;
    }
    watcher_hx_control_deinit();
    esp_err_t transport_result = watcher_hx_ptl_deinit();
    xSemaphoreTake(s_ctx.lock, portMAX_DELAY);
    /* watcher_hx_ptl_deinit() always tears down the singleton before reporting
     * a late SPI/bus cleanup error, so the HAL must not keep advertising an
     * initialized transport after any completed teardown attempt. */
    s_ctx.initialized = false;
    s_ctx.deinitializing = false;
    xSemaphoreGive(s_ctx.lock);
    xSemaphoreGive(s_ctx.operation_lock);
    return transport_result;
}

esp_err_t hal_camera_configure(int width, int height, int quality, int *applied_width, int *applied_height) {
    (void)quality;
    ESP_RETURN_ON_FALSE(width == PTL_FRAME_WIDTH && height == PTL_FRAME_HEIGHT, ESP_ERR_NOT_SUPPORTED, TAG,
                        "Custom HX image must output %dx%d JPEG", PTL_FRAME_WIDTH, PTL_FRAME_HEIGHT);
    if (applied_width != NULL) {
        *applied_width = PTL_FRAME_WIDTH;
    }
    if (applied_height != NULL) {
        *applied_height = PTL_FRAME_HEIGHT;
    }
    return ESP_OK;
}

static esp_err_t start_locked(int fps, hal_camera_frame_cb_t cb, void *ctx) {
    ESP_RETURN_ON_FALSE(s_ctx.initialized, ESP_ERR_INVALID_STATE, TAG, "Camera not initialized");
    ESP_RETURN_ON_FALSE(fps > 0 && fps <= 30 && cb != NULL, ESP_ERR_INVALID_ARG, TAG, "Invalid stream settings");
    xSemaphoreTake(s_ctx.lock, portMAX_DELAY);
    if (s_ctx.streaming || s_ctx.task || s_ctx.photo_prepared || s_ctx.capture_in_progress || s_ctx.deinitializing ||
        s_ctx.inferencing) {
        xSemaphoreGive(s_ctx.lock);
        return ESP_ERR_INVALID_STATE;
    }
    if (s_ctx.preview_owned) {
        if (!s_ctx.stream_cleanup_pending) {
            xSemaphoreGive(s_ctx.lock);
            return ESP_ERR_INVALID_STATE;
        }
        /* Only retry an orphan from our own failed stream start, never a
         * photo/other owner's preview. One bounded control attempt per call. */
        xSemaphoreGive(s_ctx.lock);
        esp_err_t cleanup = watcher_hx_control_mode(0, 0);
        xSemaphoreTake(s_ctx.lock, portMAX_DELAY);
        if (cleanup != ESP_OK) {
            xSemaphoreGive(s_ctx.lock);
            return cleanup;
        }
        s_ctx.preview_owned = false;
        s_ctx.stream_cleanup_pending = false;
    }
    esp_err_t mode = watcher_hx_control_mode(1, 0);
    if (mode != ESP_OK) {
        xSemaphoreGive(s_ctx.lock);
        return mode;
    }
    s_ctx.preview_owned = true;
    (void)xSemaphoreTake(s_ctx.stopped, 0);
    s_ctx.stop_requested = false;
    s_ctx.frame_cb = cb;
    s_ctx.frame_ctx = ctx;
    s_ctx.streaming = true;
    /* Like inference_task, the stream worker reads PTL in task context with
     * cache enabled. Keep its stack out of scarce internal/DMA RAM; the PTL
     * transport already handles external receive buffers. */
    BaseType_t created = xTaskCreateWithCaps(stream_task, "hx_ptl_stream", PTL_TASK_STACK_SIZE, NULL, PTL_TASK_PRIORITY,
                                             &s_ctx.task, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (created != pdPASS) {
        s_ctx.streaming = false;
        s_ctx.frame_cb = NULL;
        s_ctx.frame_ctx = NULL;
        s_ctx.task = NULL;
        xSemaphoreGive(s_ctx.lock);
        /* MODE1 succeeded, but there is no worker to stop the sensor. Roll
         * back here: callers must not need a successful start to clean up.
         * operation_lock remains held by hal_camera_start throughout. */
        esp_err_t stopped = watcher_hx_control_mode(0, 0);
        xSemaphoreTake(s_ctx.lock, portMAX_DELAY);
        s_ctx.preview_owned = stopped != ESP_OK;
        s_ctx.stream_cleanup_pending = stopped != ESP_OK;
        xSemaphoreGive(s_ctx.lock);
        ESP_LOGE(TAG, "Stream task creation failed: %s; sensor rollback: %s", esp_err_to_name(ESP_ERR_NO_MEM),
                 esp_err_to_name(stopped));
        ESP_LOGE(TAG, "Task memory: internal free=%u largest=%u; PSRAM free=%u largest=%u",
                 (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
                 (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
                 (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT),
                 (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
        return ESP_ERR_NO_MEM;
    }
    xSemaphoreGive(s_ctx.lock);
    return ESP_OK;
}

esp_err_t hal_camera_start(int fps, hal_camera_frame_cb_t cb, void *ctx) {
    if (!s_ctx.operation_lock)
        return ESP_ERR_INVALID_STATE;
    xSemaphoreTake(s_ctx.operation_lock, portMAX_DELAY);
    esp_err_t result = start_locked(fps, cb, ctx);
    xSemaphoreGive(s_ctx.operation_lock);
    return result;
}
static esp_err_t stop_locked(void) {
    s_ctx.photo_prepared = false;
    if (s_ctx.lock == NULL) {
        return ESP_OK;
    }
    xSemaphoreTake(s_ctx.lock, portMAX_DELAY);
    TaskHandle_t task = s_ctx.task;
    s_ctx.stop_requested = true;
    xSemaphoreGive(s_ctx.lock);
    if (task != NULL) {
        if (xSemaphoreTake(s_ctx.stopped, pdMS_TO_TICKS(PTL_STOP_TIMEOUT_MS)) != pdTRUE) {
            return ESP_ERR_TIMEOUT;
        }
        /* No allocation: externally delete the acknowledged worker before
         * publishing idle or permitting transport teardown / another start. */
        vTaskDeleteWithCaps(task);
        xSemaphoreTake(s_ctx.lock, portMAX_DELAY);
        s_ctx.task = NULL;
        s_ctx.streaming = false;
        s_ctx.frame_cb = NULL;
        s_ctx.frame_ctx = NULL;
        xSemaphoreGive(s_ctx.lock);
    }
    if (!s_ctx.preview_owned)
        return ESP_OK;
    esp_err_t result = watcher_hx_control_mode(0, 0);
    if (result == ESP_OK) {
        s_ctx.preview_owned = false;
        s_ctx.stream_cleanup_pending = false;
    }
    return result;
}
esp_err_t hal_camera_stop(void) {
    if (!s_ctx.operation_lock)
        return ESP_OK;
    xSemaphoreTake(s_ctx.operation_lock, portMAX_DELAY);
    esp_err_t result = stop_locked();
    xSemaphoreGive(s_ctx.operation_lock);
    return result;
}

static esp_err_t hal_camera_discard_pending_frames(uint32_t *discarded_frames) {
    watcher_hx_ptl_frame_t pending_frame;
    uint32_t discarded = 0U;

    ESP_RETURN_ON_FALSE(discarded_frames != NULL, ESP_ERR_INVALID_ARG, TAG, "Discard counter is null");

    while (discarded < PTL_ONE_SHOT_STALE_DRAIN_LIMIT) {
        const esp_err_t ret = watcher_hx_ptl_read(&pending_frame, 0U);
        if (ret == ESP_ERR_TIMEOUT) {
            break;
        }
        if (ret != ESP_OK && ret != ESP_ERR_INVALID_CRC && ret != ESP_ERR_INVALID_RESPONSE) {
            return ret;
        }
        ++discarded;
    }

    *discarded_frames = discarded;
    return ESP_OK;
}

/* Caller owns operation_lock throughout setup or capture. A prepared photo
 * retains exclusive ownership across the animation, but publishes no frame. */
static esp_err_t prepare_capture_locked(void) {
    if (!s_ctx.initialized || s_ctx.streaming || s_ctx.capture_in_progress || s_ctx.deinitializing || s_ctx.inferencing)
        return ESP_ERR_INVALID_STATE;
    if (s_ctx.photo_prepared)
        return ESP_OK;
    if (s_ctx.preview_owned)
        return ESP_ERR_INVALID_STATE;
    watcher_hx_ptl_frame_t frame;
    const int64_t capture_deadline_us =
        esp_timer_get_time() + (int64_t)CONFIG_WATCHER_CAMERA_CAPTURE_TIMEOUT_MS * 1000LL;
    esp_err_t stopped;
    esp_err_t ret = watcher_hx_control_mode(1, 0);
    if (ret != ESP_OK)
        return ret;
    s_ctx.preview_owned = true;
    /* A mode switch resets sensor AE. Three stale-frame discards aren't a
     * warm-up: the measured first JPEG was dark and AE settled ~0.8s later.
     * Wait for time AND valid frames, then take a new frame after the shutter. */
    hx_photo_warmup_t warmup = {.started_us = (uint64_t)esp_timer_get_time()};
    bool ready = false;
    while (esp_timer_get_time() < capture_deadline_us) {
        ret = watcher_hx_ptl_read(&frame, 100);
        bool valid = ret == ESP_OK && hal_camera_jpeg_dimensions_match(frame.jpeg, frame.jpeg_size,
                                                                       HX_MEDIA_SOURCE_WIDTH, HX_MEDIA_SOURCE_HEIGHT);
        if (hx_photo_warmup_observe(&warmup, (uint64_t)esp_timer_get_time(), valid)) {
            ready = true;
            break;
        }
        if (ret != ESP_OK && ret != ESP_ERR_TIMEOUT && ret != ESP_ERR_INVALID_CRC && ret != ESP_ERR_INVALID_RESPONSE)
            goto prepare_failed;
    }
    if (!ready) {
        ret = ESP_ERR_TIMEOUT;
        goto prepare_failed;
    }
    ESP_LOGI(TAG, "PTL photo warm-up ready after %llu us, valid_frames=%u",
             (unsigned long long)((uint64_t)esp_timer_get_time() - warmup.started_us), warmup.valid_frames);
    s_ctx.photo_prepared = true;
    return ESP_OK;
prepare_failed:
    stopped = watcher_hx_control_mode(0, 0);
    if (stopped == ESP_OK)
        s_ctx.preview_owned = false;
    else
        ret = stopped; /* Keep ownership until a later stop succeeds. */
    return ret;
}

esp_err_t hal_camera_prepare_capture(void) {
    if (!s_ctx.operation_lock)
        return ESP_ERR_INVALID_STATE;
    xSemaphoreTake(s_ctx.operation_lock, portMAX_DELAY);
    esp_err_t result = prepare_capture_locked();
    xSemaphoreGive(s_ctx.operation_lock);
    return result;
}

esp_err_t hal_camera_capture_once(hal_camera_frame_cb_t cb, void *ctx) {
    watcher_hx_ptl_frame_t frame;
    int64_t capture_deadline_us;
    uint32_t rejected_frames = 0U;
    uint32_t discarded_frames = 0U;
    bool activity_published = false;
    esp_err_t ret = ESP_ERR_TIMEOUT;

    ESP_RETURN_ON_FALSE(cb != NULL, ESP_ERR_INVALID_ARG, TAG, "Capture callback is null");
    ESP_RETURN_ON_FALSE(s_ctx.lock != NULL && s_ctx.operation_lock != NULL, ESP_ERR_INVALID_STATE, TAG,
                        "Camera not initialized");
    ESP_RETURN_ON_FALSE(xSemaphoreTake(s_ctx.operation_lock, portMAX_DELAY) == pdTRUE, ESP_FAIL, TAG,
                        "Lock PTL camera capture operation");
    ret = prepare_capture_locked();
    if (ret != ESP_OK) {
        xSemaphoreGive(s_ctx.operation_lock);
        return ret;
    }
    if (xSemaphoreTake(s_ctx.lock, portMAX_DELAY) != pdTRUE) {
        xSemaphoreGive(s_ctx.operation_lock);
        return ESP_FAIL;
    }
    if (!s_ctx.initialized || s_ctx.streaming || s_ctx.capture_in_progress || s_ctx.deinitializing ||
        s_ctx.inferencing) {
        xSemaphoreGive(s_ctx.lock);
        xSemaphoreGive(s_ctx.operation_lock);
        return ESP_ERR_INVALID_STATE;
    }
    capture_deadline_us = esp_timer_get_time() + (int64_t)CONFIG_WATCHER_CAMERA_CAPTURE_TIMEOUT_MS * 1000LL;
    s_ctx.capture_in_progress = true;
    xSemaphoreGive(s_ctx.lock);
    /* HX produces frames continuously. A frame can be waiting on SYNC since
     * camera initialization, so returning the first readable packet can show
     * the user's pose from seconds before the shutter. Drain only packets that
     * are already pending, then light the system indicator and wait for the
     * next frame generated by the live sensor. */
    ret = hal_camera_discard_pending_frames(&discarded_frames);
    if (ret != ESP_OK) {
        goto capture_done;
    }
    if (discarded_frames > 0U) {
        ESP_LOGI(TAG, "Discarded %u pre-shutter PTL frame(s)", (unsigned)discarded_frames);
    }
    publish_activity(true);
    activity_published = true;

    while (esp_timer_get_time() < capture_deadline_us) {
        const int64_t remaining_us = capture_deadline_us - esp_timer_get_time();
        const uint32_t remaining_ms = (uint32_t)((remaining_us + 999LL) / 1000LL);
        ret = watcher_hx_ptl_read(&frame, remaining_ms);
        if (ret == ESP_OK) {
            if (hal_camera_jpeg_dimensions_match(frame.jpeg, frame.jpeg_size, HX_MEDIA_SOURCE_WIDTH,
                                                 HX_MEDIA_SOURCE_HEIGHT)) {
                ESP_LOGI(
                    TAG, "PTL shutter fresh frame after %lld us, timestamp_ms=%lu",
                    (long long)(esp_timer_get_time() -
                                (capture_deadline_us - (int64_t)CONFIG_WATCHER_CAMERA_CAPTURE_TIMEOUT_MS * 1000LL)),
                    (unsigned long)frame.timestamp_ms);
                cb(frame.jpeg, frame.jpeg_size, frame.timestamp_ms, ctx);
                break;
            }
            ret = ESP_ERR_INVALID_RESPONSE;
        }
        if (ret == ESP_ERR_INVALID_CRC || ret == ESP_ERR_INVALID_RESPONSE) {
            ++rejected_frames;
            continue;
        }
        break;
    }
    if (ret != ESP_OK && rejected_frames > 0U) {
        ESP_LOGW(TAG, "One-shot capture exhausted after rejecting %u PTL frame(s): %s", (unsigned)rejected_frames,
                 esp_err_to_name(ret));
    }
capture_done:
    s_ctx.photo_prepared = false;
    {
        esp_err_t stopped = watcher_hx_control_mode(0, 0);
        if (stopped == ESP_OK)
            s_ctx.preview_owned = false;
        if (stopped != ESP_OK)
            ret = stopped;
    }
    if (activity_published) {
        publish_activity(false);
    }
    xSemaphoreTake(s_ctx.lock, portMAX_DELAY);
    s_ctx.capture_in_progress = false;
    xSemaphoreGive(s_ctx.lock);
    xSemaphoreGive(s_ctx.operation_lock);
    return ret;
}

esp_err_t hal_camera_get_model_info(hal_camera_model_info_t *out_info) {
    if (!out_info)
        return ESP_ERR_INVALID_ARG;
    ESP_RETURN_ON_ERROR(hal_camera_init(), TAG, "Initialize model control");
    xSemaphoreTake(s_ctx.operation_lock, portMAX_DELAY);
    esp_err_t result = ESP_OK;
    if (!s_catalog_valid) {
        if (s_ctx.streaming || s_ctx.inferencing || s_ctx.capture_in_progress)
            result = ESP_ERR_INVALID_STATE;
        else {
            result = watcher_hx_control_catalog(s_model_crc);
            s_catalog_valid = result == ESP_OK;
        }
    }
    if (result == ESP_OK) {
        memset(out_info, 0, sizeof(*out_info));
        out_info->model_id = (int)s_selected_model;
        bool verified = hx_model_identity_verified(s_selected_model, s_model_crc[s_selected_model - 1]);
        out_info->verified = verified;
        out_info->contains_face_class = verified && s_selected_model == 4;
        snprintf(out_info->name, sizeof(out_info->name), "%s",
                 verified ? s_model_names[s_selected_model - 1] : "Unverified model");
    }
    xSemaphoreGive(s_ctx.operation_lock);
    return result;
}

esp_err_t hal_camera_list_models(hal_camera_model_info_t *models, size_t capacity, size_t *count) {
    if (!models || !count || capacity < 4)
        return ESP_ERR_INVALID_ARG;
    hal_camera_model_info_t selected;
    ESP_RETURN_ON_ERROR(hal_camera_get_model_info(&selected), TAG, "Read catalog");
    xSemaphoreTake(s_ctx.operation_lock, portMAX_DELAY);
    for (unsigned i = 0; i < 4; ++i) {
        memset(&models[i], 0, sizeof(models[i]));
        models[i].model_id = (int)i + 1;
        models[i].verified = hx_model_identity_verified(i + 1, s_model_crc[i]);
        models[i].contains_face_class = models[i].verified && i == 3;
        snprintf(models[i].name, sizeof(models[i].name), "%s",
                 models[i].verified ? s_model_names[i] : "Unverified model");
    }
    *count = 4;
    xSemaphoreGive(s_ctx.operation_lock);
    return ESP_OK;
}

esp_err_t hal_camera_set_active_model(int model_id) {
    if (model_id < 1 || model_id > 4)
        return ESP_ERR_INVALID_ARG;
    hal_camera_model_info_t info;
    ESP_RETURN_ON_ERROR(hal_camera_get_model_info(&info), TAG, "Read model catalog");
    xSemaphoreTake(s_ctx.operation_lock, portMAX_DELAY);
    xSemaphoreTake(s_ctx.lock, portMAX_DELAY);
    esp_err_t result = ESP_ERR_INVALID_STATE;
    if (!s_ctx.streaming && !s_ctx.inferencing && !s_ctx.capture_in_progress && !s_ctx.photo_prepared &&
        !s_ctx.deinitializing) {
        s_selected_model = (unsigned)model_id;
        result = ESP_OK;
    }
    xSemaphoreGive(s_ctx.lock);
    xSemaphoreGive(s_ctx.operation_lock);
    return result;
}

esp_err_t hal_camera_set_model_info(const char *model_info) {
    (void)model_info;
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t hal_camera_get_diagnostics(hal_camera_diagnostics_t *out_diagnostics) {
    ESP_RETURN_ON_FALSE(out_diagnostics != NULL, ESP_ERR_INVALID_ARG, TAG, "Diagnostics output is null");
    memset(out_diagnostics, 0, sizeof(*out_diagnostics));
    out_diagnostics->backend = "ptl";
    out_diagnostics->supports_capture = true;
    out_diagnostics->supports_preview = true;
    out_diagnostics->supports_inference = true;
    out_diagnostics->supports_model_info = true;
    out_diagnostics->supports_model_management = true;
    out_diagnostics->status = ESP_OK;
    out_diagnostics->model_status = s_catalog_valid ? ESP_OK : ESP_ERR_INVALID_STATE;

    if (s_ctx.lock != NULL && xSemaphoreTake(s_ctx.lock, portMAX_DELAY) == pdTRUE) {
        out_diagnostics->initialized = s_ctx.initialized;
        out_diagnostics->connected = s_ctx.initialized;
        out_diagnostics->streaming = s_ctx.streaming;
        out_diagnostics->inferencing = s_ctx.inferencing;
        out_diagnostics->busy =
            s_ctx.streaming || s_ctx.capture_in_progress || s_ctx.deinitializing || s_ctx.inferencing;
        if (s_catalog_valid) {
            bool verified = hx_model_identity_verified(s_selected_model, s_model_crc[s_selected_model - 1]);
            out_diagnostics->model_available = true;
            out_diagnostics->model.model_id = (int)s_selected_model;
            out_diagnostics->model.verified = verified;
            out_diagnostics->model.contains_face_class = verified && s_selected_model == 4;
            snprintf(out_diagnostics->model.name, sizeof(out_diagnostics->model.name), "%s",
                     verified ? s_model_names[s_selected_model - 1] : "Unverified model");
        }
        xSemaphoreGive(s_ctx.lock);
    }
    /* RTC owns a separate source object. Report the coprocessor's confirmed
     * mode instead of mistaking this HAL object's local idle flag for IDLE. */
    watcher_hx_state_t state;
    esp_err_t control_status = watcher_hx_control_status(&state);
    if (control_status == ESP_OK) {
        out_diagnostics->initialized = true;
        out_diagnostics->connected = true;
        out_diagnostics->streaming = state.mode == 1;
        out_diagnostics->inferencing = state.mode == 2;
        out_diagnostics->busy = state.mode != 0;
        out_diagnostics->status = state.mode == 4 || state.error ? ESP_FAIL : ESP_OK;
    } else if (control_status != ESP_ERR_INVALID_STATE) {
        out_diagnostics->connected = false;
        out_diagnostics->status = control_status;
    }
    return ESP_OK;
}

esp_err_t hal_camera_start_inference(hal_camera_inference_cb_t cb, void *ctx) {
    return hal_camera_start_inference_ex(cb, ctx, NULL);
}

static void inference_task(void *arg) {
    (void)arg;
    uint32_t last_sequence = 0;
    uint32_t preview_reads = 0, preview_rejected = 0;
    int64_t preview_log_us = 0;
    /* Camera cold starts affect inference exposure just as they affect photos.
     * Reuse the tested valid-frame/time gate before delivering motion inputs. */
    hx_photo_warmup_t warmup = {.started_us = (uint64_t)esp_timer_get_time()};
    for (;;) {
        xSemaphoreTake(s_ctx.lock, portMAX_DELAY);
        bool stop = s_ctx.inference_stop;
        xSemaphoreGive(s_ctx.lock);
        if (stop)
            break;
        watcher_hx_result_t result = {0};
        size_t image_size = 0;
        esp_err_t received;
        if (s_ctx.inference_image) {
            watcher_hx_ptl_frame_t jpeg;
            wff_frame_t paired;
            received = watcher_hx_ptl_read(&jpeg, 200);
            if (received == ESP_OK) {
                ++preview_reads;
                if (!wff_unpack(jpeg.jpeg, jpeg.jpeg_size, &paired) ||
                    !hal_camera_jpeg_dimensions_match(jpeg.jpeg, jpeg.jpeg_size, 640, 480)) {
                    received = ESP_ERR_INVALID_RESPONSE;
                    ++preview_rejected;
                } else {
                    result.generation = paired.generation;
                    result.sequence = paired.sequence;
                    result.width = paired.width;
                    result.height = paired.height;
                    result.count = paired.count;
                    for (unsigned i = 0; i < paired.count; ++i) {
                        const wff_box_t *b = &paired.boxes[i];
                        result.boxes[i] =
                            (watcher_hx_box_t){b->x, b->y, b->width, b->height, (uint8_t)b->score, (uint8_t)b->target};
                    }
                    if (mbedtls_base64_encode((unsigned char *)s_ctx.inference_image, 175000, &image_size, jpeg.jpeg,
                                              jpeg.jpeg_size) != 0)
                        received = ESP_ERR_INVALID_SIZE;
                }
            }
            const int64_t now_us = esp_timer_get_time();
            if (now_us - preview_log_us > 3000000) {
                ESP_LOGI(TAG, "Face preview read=%lu rejected=%lu err=%d bytes=%u seq=%lu gen=%lu/%lu",
                         (unsigned long)preview_reads, (unsigned long)preview_rejected, received, (unsigned)image_size,
                         (unsigned long)result.sequence, (unsigned long)result.generation,
                         (unsigned long)s_ctx.inference_generation);
                preview_log_us = now_us;
            }
        } else {
            received = watcher_hx_control_boxes(&result);
        }
        hal_camera_inference_frame_t frame = {0};
        hal_camera_inference_cb_t callback = NULL;
        void *context = NULL;
        xSemaphoreTake(s_ctx.lock, portMAX_DELAY);
        if (received == ESP_OK && result.generation == s_ctx.inference_generation && result.sequence != last_sequence &&
            !s_ctx.inference_stop) {
            last_sequence = result.sequence;
            frame.sequence = result.sequence;
            frame.timestamp_ms = (uint32_t)(esp_timer_get_time() / 1000);
            frame.frame_width = result.width;
            frame.frame_height = result.height;
            frame.preview_image_base64 = image_size ? s_ctx.inference_image : NULL;
            frame.preview_image_size = (uint32_t)image_size;
            frame.box_count = result.count;
            for (unsigned i = 0; i < result.count; ++i) {
                const watcher_hx_box_t *b = &result.boxes[i];
                frame.boxes[i] = (hal_camera_inference_box_t){b->x, b->y, b->width, b->height, b->score, b->target};
            }
            ++s_ctx.inference_stats.frame_count;
            if (hx_photo_warmup_observe(&warmup, (uint64_t)esp_timer_get_time(), true)) {
                s_ctx.inference_stats.valid_box_count += result.count;
                ++s_ctx.inference_stats.callback_count;
                callback = s_ctx.inference_cb;
                context = s_ctx.inference_ctx;
            }
        } else if (received != ESP_OK)
            ++s_ctx.inference_stats.parse_failures;
        xSemaphoreGive(s_ctx.lock);
        if (callback)
            callback(&frame, context);
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    xSemaphoreTake(s_ctx.lock, portMAX_DELAY);
    s_ctx.inference_task = NULL;
    xSemaphoreGive(s_ctx.lock);
    xSemaphoreGive(s_ctx.inference_stopped);
    vTaskDeleteWithCaps(NULL);
}

esp_err_t hal_camera_start_inference_ex(hal_camera_inference_cb_t cb, void *ctx,
                                        const hal_camera_inference_options_t *options) {
    if (!cb)
        return ESP_ERR_INVALID_ARG;
    ESP_RETURN_ON_ERROR(hal_camera_init(), TAG, "Initialize inference control");
    xSemaphoreTake(s_ctx.operation_lock, portMAX_DELAY);
    xSemaphoreTake(s_ctx.lock, portMAX_DELAY);
    if (s_ctx.inferencing || s_ctx.streaming || s_ctx.capture_in_progress || s_ctx.deinitializing ||
        s_ctx.preview_owned) {
        xSemaphoreGive(s_ctx.lock);
        xSemaphoreGive(s_ctx.operation_lock);
        return ESP_ERR_INVALID_STATE;
    }
    if (!s_ctx.inference_stopped)
        s_ctx.inference_stopped = xSemaphoreCreateBinary();
    if (!s_ctx.inference_stopped) {
        xSemaphoreGive(s_ctx.lock);
        xSemaphoreGive(s_ctx.operation_lock);
        return ESP_ERR_NO_MEM;
    }
    if (options && options->include_preview_image && !s_ctx.inference_image)
        s_ctx.inference_image = heap_caps_malloc(175000, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (options && options->include_preview_image && !s_ctx.inference_image) {
        xSemaphoreGive(s_ctx.lock);
        xSemaphoreGive(s_ctx.operation_lock);
        return ESP_ERR_NO_MEM;
    }
    xSemaphoreGive(s_ctx.lock);
    /* Keep the camera indicator on until the sensor is confirmed stopped. */
    publish_activity(true);
    esp_err_t result = watcher_hx_control_mode_preview(2, s_selected_model, s_ctx.inference_image != NULL);
    if (result == ESP_OK) {
        xSemaphoreTake(s_ctx.lock, portMAX_DELAY);
        s_ctx.inferencing = true;
        s_ctx.inference_stop = false;
        s_ctx.inference_cb = cb;
        s_ctx.inference_ctx = ctx;
        s_ctx.inference_generation = watcher_hx_control_generation();
        memset(&s_ctx.inference_stats, 0, sizeof(s_ctx.inference_stats));
        (void)xSemaphoreTake(s_ctx.inference_stopped, 0);
        if (xTaskCreateWithCaps(inference_task, "hx_inference", 8192, NULL, 5, &s_ctx.inference_task,
                                MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT) != pdPASS)
            result = ESP_ERR_NO_MEM;
        xSemaphoreGive(s_ctx.lock);
    }
    if (result != ESP_OK) {
        esp_err_t stopped = watcher_hx_control_mode(0, 0);
        xSemaphoreTake(s_ctx.lock, portMAX_DELAY);
        s_ctx.inferencing = stopped != ESP_OK;
        xSemaphoreGive(s_ctx.lock);
        if (stopped == ESP_OK) {
            free(s_ctx.inference_image);
            s_ctx.inference_image = NULL;
            publish_activity(false);
        }
    }
    xSemaphoreGive(s_ctx.operation_lock);
    return result;
}

esp_err_t hal_camera_stop_inference(void) {
    if (!s_ctx.operation_lock)
        return ESP_OK;
    xSemaphoreTake(s_ctx.operation_lock, portMAX_DELAY);
    xSemaphoreTake(s_ctx.lock, portMAX_DELAY);
    if (!s_ctx.inferencing) {
        xSemaphoreGive(s_ctx.lock);
        xSemaphoreGive(s_ctx.operation_lock);
        return ESP_OK;
    }
    s_ctx.inference_stop = true;
    bool has_task = s_ctx.inference_task != NULL;
    xSemaphoreGive(s_ctx.lock);
    esp_err_t result = ESP_OK;
    if (has_task && xSemaphoreTake(s_ctx.inference_stopped, pdMS_TO_TICKS(2000)) != pdTRUE)
        result = ESP_ERR_TIMEOUT;
    if (result == ESP_OK)
        result = watcher_hx_control_mode(0, 0);
    if (result == ESP_OK) {
        xSemaphoreTake(s_ctx.lock, portMAX_DELAY);
        s_ctx.inferencing = false;
        s_ctx.inference_cb = NULL;
        s_ctx.inference_ctx = NULL;
        free(s_ctx.inference_image);
        s_ctx.inference_image = NULL;
        xSemaphoreGive(s_ctx.lock);
        publish_activity(false);
    }
    xSemaphoreGive(s_ctx.operation_lock);
    return result;
}

bool hal_camera_is_inferencing(void) {
    if (!s_ctx.lock)
        return false;
    xSemaphoreTake(s_ctx.lock, portMAX_DELAY);
    bool active = s_ctx.inferencing;
    xSemaphoreGive(s_ctx.lock);
    return active;
}

esp_err_t hal_camera_get_inference_stats(hal_camera_inference_stats_t *out_stats) {
    ESP_RETURN_ON_FALSE(out_stats != NULL, ESP_ERR_INVALID_ARG, TAG, "Stats output is null");
    if (!s_ctx.lock)
        return ESP_ERR_INVALID_STATE;
    xSemaphoreTake(s_ctx.lock, portMAX_DELAY);
    *out_stats = s_ctx.inference_stats;
    xSemaphoreGive(s_ctx.lock);
    return ESP_OK;
}

bool hal_camera_is_streaming(void) {
    bool streaming = false;

    if (s_ctx.lock != NULL && xSemaphoreTake(s_ctx.lock, portMAX_DELAY) == pdTRUE) {
        streaming = s_ctx.streaming;
        xSemaphoreGive(s_ctx.lock);
    }
    return streaming;
}

