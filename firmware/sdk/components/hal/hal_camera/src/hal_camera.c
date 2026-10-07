#include "hal_camera.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "mbedtls/base64.h"
#include "sdkconfig.h"
#include "sensecap-watcher.h"
#include "sscma_client.h"

#define TAG "HAL_CAMERA"
#define HAL_CAMERA_CONNECT_TIMEOUT_MS CONFIG_WATCHER_CAMERA_CONNECT_TIMEOUT_MS
#define HAL_CAMERA_CAPTURE_TIMEOUT_MS CONFIG_WATCHER_CAMERA_CAPTURE_TIMEOUT_MS
#define HAL_CAMERA_MAX_FPS 30
#define HAL_CAMERA_MODEL_INFO_MAX_BYTES 3000U
#define HAL_CAMERA_STREAM_TASK_STACK 6144
#define HAL_CAMERA_STREAM_TASK_PRIORITY 5
#define HAL_CAMERA_STREAM_STOP_TIMEOUT_MS (HAL_CAMERA_CAPTURE_TIMEOUT_MS + 1000)
#define HAL_CAMERA_STREAM_LOG_EVERY 30
#define HAL_CAMERA_SSCMA_PRIME_FRAMES 2
#define HAL_CAMERA_SSCMA_PRIME_SETTLE_MS 500
#define HAL_CAMERA_DEFAULT_SENSOR_ID 1
#define HAL_CAMERA_DEFAULT_QUALITY 80

#if CONFIG_WATCHER_CAMERA_VERBOSE_LOGS
#define HAL_CAMERA_VERBOSE_LOGS_ENABLED 1
#else
#define HAL_CAMERA_VERBOSE_LOGS_ENABLED 0
#endif

typedef struct {
    uint32_t invoke_call_us;
    uint32_t wait_image_us;
    uint32_t decode_us;
    uint32_t capture_total_us;
    uint32_t timestamp_total_us;
} hal_camera_capture_timing_t;

typedef struct {
    uint32_t capture_total_us;
    uint32_t invoke_call_us;
    uint32_t wait_image_us;
    uint32_t decode_us;
    uint32_t callback_us;
    uint32_t loop_total_us;
} hal_camera_stream_timing_sample_t;

typedef struct {
    int opt_id;
    int width;
    int height;
    const char *detail;
} hal_camera_sensor_option_t;

static const hal_camera_sensor_option_t s_hal_camera_sensor_options[] = {
    {.opt_id = 0, .width = 240, .height = 240, .detail = "240x240 Auto"},
    {.opt_id = 1, .width = 416, .height = 416, .detail = "416x416 Auto"},
    {.opt_id = 2, .width = 480, .height = 480, .detail = "480x480 Auto"},
    {.opt_id = 3, .width = 640, .height = 480, .detail = "640x480 Auto"},
};

typedef struct {
    sscma_client_handle_t client;
    SemaphoreHandle_t lock;
    SemaphoreHandle_t connect_sem;
    SemaphoreHandle_t capture_sem;
    TaskHandle_t stream_task;
    hal_camera_frame_cb_t stream_cb;
    void *stream_ctx;
    int stream_fps;
    bool initialized;
    bool connected;
    bool init_in_progress;
    bool init_cleanup_pending;
    bool callbacks_registered;
    bool client_started;
    bool capture_in_progress;
    bool model_maintenance_in_progress;
    bool streaming;
    bool inferencing;
    bool inference_stopping;
    bool stream_stop_requested;
    esp_err_t init_status;
    esp_err_t capture_status;
    char *capture_image;
    int capture_image_size;
    uint32_t stream_frames_ok;
    uint32_t stream_frames_err;
    hal_camera_inference_cb_t inference_cb;
    void *inference_ctx;
    hal_camera_inference_stats_t inference_stats;
    uint32_t inference_last_timestamp_ms;
    int sensor_id;
    int sensor_opt_id;
    int configured_width;
    int configured_height;
    int configured_quality;
} hal_camera_context_t;

static hal_camera_context_t s_ctx = {
    .client = NULL,
    .lock = NULL,
    .connect_sem = NULL,
    .capture_sem = NULL,
    .stream_task = NULL,
    .stream_cb = NULL,
    .stream_ctx = NULL,
    .stream_fps = 0,
    .initialized = false,
    .connected = false,
    .init_in_progress = false,
    .callbacks_registered = false,
    .client_started = false,
    .capture_in_progress = false,
    .streaming = false,
    .inferencing = false,
    .inference_stopping = false,
    .stream_stop_requested = false,
    .init_status = ESP_OK,
    .capture_status = ESP_OK,
    .capture_image = NULL,
    .capture_image_size = 0,
    .stream_frames_ok = 0,
    .stream_frames_err = 0,
    .inference_cb = NULL,
    .inference_ctx = NULL,
    .inference_stats = {0},
    .inference_last_timestamp_ms = 0,
    .sensor_id = HAL_CAMERA_DEFAULT_SENSOR_ID,
    .sensor_opt_id = 0,
    .configured_width = 240,
    .configured_height = 240,
    .configured_quality = HAL_CAMERA_DEFAULT_QUALITY,
};

static portMUX_TYPE s_capture_activity_lock = portMUX_INITIALIZER_UNLOCKED;
static hal_camera_capture_activity_cb_t s_capture_activity_cb = NULL;
static void *s_capture_activity_ctx = NULL;
static bool s_capture_activity_active = false;

static void hal_camera_publish_capture_activity(bool active) {
    hal_camera_capture_activity_cb_t cb = NULL;
    void *ctx = NULL;

    taskENTER_CRITICAL(&s_capture_activity_lock);
    if (s_capture_activity_active != active) {
        s_capture_activity_active = active;
        cb = s_capture_activity_cb;
        ctx = s_capture_activity_ctx;
    }
    taskEXIT_CRITICAL(&s_capture_activity_lock);

    if (cb != NULL) {
        cb(active, ctx);
    }
}

esp_err_t hal_camera_set_capture_activity_callback(hal_camera_capture_activity_cb_t cb, void *ctx) {
    bool active;

    taskENTER_CRITICAL(&s_capture_activity_lock);
    s_capture_activity_cb = cb;
    s_capture_activity_ctx = ctx;
    active = s_capture_activity_active;
    taskEXIT_CRITICAL(&s_capture_activity_lock);

    if (cb != NULL) {
        cb(active, ctx);
    }
    return ESP_OK;
}

static const hal_camera_sensor_option_t *hal_camera_find_sensor_option_by_opt_id(int opt_id) {
    size_t i;

    for (i = 0; i < sizeof(s_hal_camera_sensor_options) / sizeof(s_hal_camera_sensor_options[0]); ++i) {
        if (s_hal_camera_sensor_options[i].opt_id == opt_id) {
            return &s_hal_camera_sensor_options[i];
        }
    }

    return NULL;
}

static const hal_camera_sensor_option_t *hal_camera_find_sensor_option_by_size(int width, int height) {
    size_t i;

    for (i = 0; i < sizeof(s_hal_camera_sensor_options) / sizeof(s_hal_camera_sensor_options[0]); ++i) {
        if (s_hal_camera_sensor_options[i].width == width && s_hal_camera_sensor_options[i].height == height) {
            return &s_hal_camera_sensor_options[i];
        }
    }

    return NULL;
}

static int hal_camera_get_json_int(cJSON *object, const char *field_name, int fallback) {
    cJSON *field;

    if (object == NULL || field_name == NULL) {
        return fallback;
    }

    field = cJSON_GetObjectItem(object, field_name);
    if (!cJSON_IsNumber(field)) {
        return fallback;
    }

    return field->valueint;
}

static void hal_camera_update_sensor_state_locked(int sensor_id, int opt_id, int quality) {
    const hal_camera_sensor_option_t *option = hal_camera_find_sensor_option_by_opt_id(opt_id);

    if (sensor_id > 0) {
        s_ctx.sensor_id = sensor_id;
    }
    if (option != NULL) {
        s_ctx.sensor_opt_id = option->opt_id;
        s_ctx.configured_width = option->width;
        s_ctx.configured_height = option->height;
    }
    if (quality > 0) {
        s_ctx.configured_quality = quality;
    }
}

static esp_err_t hal_camera_refresh_sensor_catalog(bool log_catalog) {
    sscma_client_reply_t reply = {0};
    esp_err_t ret;

    ret = sscma_client_request(s_ctx.client, "AT+SENSORS?\r\n", &reply, true, 2000 / portTICK_PERIOD_MS);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "sscma_client_request(AT+SENSORS?) failed: %s", esp_err_to_name(ret));
        return ret;
    }

    if (reply.payload != NULL) {
        cJSON *data = cJSON_GetObjectItem(reply.payload, "data");
        cJSON *sensor = NULL;
        int sensor_id = HAL_CAMERA_DEFAULT_SENSOR_ID;
        int opt_id = s_ctx.sensor_opt_id;

        if (cJSON_IsArray(data)) {
            sensor = cJSON_GetArrayItem(data, 0);
        } else if (cJSON_IsObject(data)) {
            sensor = cJSON_GetObjectItem(data, "sensor");
        }

        if (sensor != NULL) {
            sensor_id = hal_camera_get_json_int(sensor, "id", sensor_id);
            opt_id = hal_camera_get_json_int(sensor, "opt_id", opt_id);
            if (xSemaphoreTake(s_ctx.lock, portMAX_DELAY) == pdTRUE) {
                hal_camera_update_sensor_state_locked(sensor_id, opt_id, 0);
                xSemaphoreGive(s_ctx.lock);
            }
        }

        if (log_catalog) {
#if CONFIG_WATCHER_CAMERA_VERBOSE_LOGS
            char *json = cJSON_PrintUnformatted(reply.payload);
            if (json != NULL) {
                ESP_LOGI(TAG, "HX6538 sensors catalog: %s", json);
                free(json);
            } else {
                ESP_LOGW(TAG, "HX6538 sensors catalog print failed");
            }
#else
            (void)log_catalog;
#endif
        }
    } else if (reply.data != NULL && reply.len > 0) {
        if (log_catalog) {
#if CONFIG_WATCHER_CAMERA_VERBOSE_LOGS
            ESP_LOGI(TAG, "HX6538 sensors catalog raw: %.*s", (int)reply.len, reply.data);
#else
            (void)log_catalog;
#endif
        }
    } else if (log_catalog) {
#if CONFIG_WATCHER_CAMERA_VERBOSE_LOGS
        ESP_LOGW(TAG, "HX6538 sensors catalog reply empty");
#else
        (void)log_catalog;
#endif
    }

    sscma_client_reply_clear(&reply);
    return ESP_OK;
}

static void hal_camera_clear_capture_locked(void) {
    if (s_ctx.capture_image != NULL) {
        free(s_ctx.capture_image);
        s_ctx.capture_image = NULL;
    }
    s_ctx.capture_image_size = 0;
    s_ctx.capture_status = ESP_OK;
}

static void hal_camera_signal_connect(void) {
    s_ctx.connected = true;
    if (s_ctx.connect_sem != NULL) {
        xSemaphoreGive(s_ctx.connect_sem);
    }
}

static bool hal_camera_extract_capture(const sscma_client_reply_t *reply) {
    char *image = NULL;
    int image_size = 0;
    bool accepted = false;

    if (reply == NULL || reply->payload == NULL) {
        return false;
    }

    if (sscma_utils_fetch_image_from_reply(reply, &image, &image_size) != ESP_OK || image == NULL || image_size <= 0) {
        return false;
    }

    if (xSemaphoreTake(s_ctx.lock, portMAX_DELAY) == pdTRUE) {
        if (s_ctx.capture_in_progress && s_ctx.capture_image == NULL) {
            s_ctx.capture_image = image;
            s_ctx.capture_image_size = image_size;
            s_ctx.capture_status = ESP_OK;
            s_ctx.capture_in_progress = false;
            accepted = true;
        }
        xSemaphoreGive(s_ctx.lock);
    }

    if (!accepted) {
        free(image);
    }

    if (accepted && s_ctx.capture_sem != NULL) {
        xSemaphoreGive(s_ctx.capture_sem);
    }

    return accepted;
}

static bool hal_camera_string_contains_case_insensitive(const char *text, const char *needle) {
    size_t needle_len;

    if (text == NULL || needle == NULL || *needle == '\0') {
        return false;
    }

    needle_len = strlen(needle);
    for (; *text != '\0'; ++text) {
        size_t i = 0;
        while (i < needle_len && text[i] != '\0' &&
               tolower((unsigned char)text[i]) == tolower((unsigned char)needle[i])) {
            ++i;
        }
        if (i == needle_len) {
            return true;
        }
    }
    return false;
}

static bool hal_camera_model_has_face_class(const sscma_client_model_t *model) {
    if (model == NULL) {
        return false;
    }
    for (size_t i = 0; i < SSCMA_CLIENT_MODEL_MAX_CLASSES; ++i) {
        if (hal_camera_string_contains_case_insensitive(model->classes[i], "face")) {
            return true;
        }
    }
    return false;
}

static esp_err_t hal_camera_refresh_active_sensor(void) {
    sscma_client_sensor_t sensor = {0};
    esp_err_t ret = sscma_client_get_sensor(s_ctx.client, &sensor);

    if (ret == ESP_OK && xSemaphoreTake(s_ctx.lock, portMAX_DELAY) == pdTRUE) {
        hal_camera_update_sensor_state_locked(sensor.id, sensor.opt_id, 0);
        xSemaphoreGive(s_ctx.lock);
    }
    free(sensor.opt_detail);
    return ret;
}

static bool hal_camera_extract_inference(const sscma_client_reply_t *reply) {
    sscma_client_box_t source_boxes[HAL_CAMERA_INFERENCE_MAX_BOXES] = {0};
    hal_camera_inference_frame_t frame = {0};
    hal_camera_inference_cb_t callback = NULL;
    void *callback_ctx = NULL;
    int box_count = 0;
    esp_err_t ret;

    if (reply == NULL || reply->payload == NULL) {
        return false;
    }
    cJSON *data = cJSON_GetObjectItem(reply->payload, "data");
    if (!cJSON_IsObject(data) || !cJSON_IsArray(cJSON_GetObjectItem(data, "boxes"))) {
        return false;
    }

    if (xSemaphoreTake(s_ctx.lock, portMAX_DELAY) != pdTRUE) {
        return false;
    }
    if (!s_ctx.inferencing) {
        xSemaphoreGive(s_ctx.lock);
        return false;
    }
    xSemaphoreGive(s_ctx.lock);

    ret = sscma_utils_copy_boxes_from_reply(reply, source_boxes, HAL_CAMERA_INFERENCE_MAX_BOXES, &box_count);
    if (ret != ESP_OK) {
        if (xSemaphoreTake(s_ctx.lock, portMAX_DELAY) == pdTRUE) {
            s_ctx.inference_stats.parse_failures++;
            xSemaphoreGive(s_ctx.lock);
        }
        return false;
    }

    frame.timestamp_ms = (uint32_t)(esp_timer_get_time() / 1000ULL);
    cJSON *perf = cJSON_GetObjectItem(data, "perf");
    if (cJSON_IsArray(perf) && cJSON_GetArraySize(perf) >= 3) {
        frame.preprocess_ms = (uint16_t)cJSON_GetArrayItem(perf, 0)->valueint;
        frame.inference_ms = (uint16_t)cJSON_GetArrayItem(perf, 1)->valueint;
        frame.postprocess_ms = (uint16_t)cJSON_GetArrayItem(perf, 2)->valueint;
    }
    cJSON *image = cJSON_GetObjectItem(data, "image");
    if (cJSON_IsString(image) && image->valuestring != NULL) {
        frame.preview_image_base64 = image->valuestring;
        frame.preview_image_size = (uint32_t)strlen(image->valuestring);
    }
    frame.box_count = (uint8_t)box_count;
    for (int i = 0; i < box_count; ++i) {
        frame.boxes[i].x = source_boxes[i].x;
        frame.boxes[i].y = source_boxes[i].y;
        frame.boxes[i].width = source_boxes[i].w;
        frame.boxes[i].height = source_boxes[i].h;
        frame.boxes[i].score = source_boxes[i].score;
        frame.boxes[i].target = source_boxes[i].target;
    }

    if (xSemaphoreTake(s_ctx.lock, portMAX_DELAY) != pdTRUE) {
        return false;
    }
    if (!s_ctx.inferencing) {
        xSemaphoreGive(s_ctx.lock);
        return false;
    }

    frame.sequence = ++s_ctx.inference_stats.frame_count;
    frame.frame_width = (uint16_t)s_ctx.configured_width;
    frame.frame_height = (uint16_t)s_ctx.configured_height;
    s_ctx.inference_stats.valid_box_count += frame.box_count;
    if (s_ctx.inference_last_timestamp_ms != 0u) {
        const uint32_t interval_ms = frame.timestamp_ms - s_ctx.inference_last_timestamp_ms;
        s_ctx.inference_stats.last_interval_ms = interval_ms;
        s_ctx.inference_stats.interval_total_ms += interval_ms;
        if (interval_ms > s_ctx.inference_stats.max_interval_ms) {
            s_ctx.inference_stats.max_interval_ms = interval_ms;
        }
    }
    s_ctx.inference_last_timestamp_ms = frame.timestamp_ms;
    callback = s_ctx.inference_cb;
    callback_ctx = s_ctx.inference_ctx;
    if (callback != NULL) {
        s_ctx.inference_stats.callback_count++;
    }
    xSemaphoreGive(s_ctx.lock);

    if (callback != NULL) {
        callback(&frame, callback_ctx);
    }
    return true;
}

static void hal_camera_on_connect(sscma_client_handle_t client, const sscma_client_reply_t *reply, void *user_ctx) {
    (void)client;
    (void)reply;
    (void)user_ctx;

    if (xSemaphoreTake(s_ctx.lock, portMAX_DELAY) == pdTRUE) {
        ESP_LOGI(TAG, "HX6538 connect event received");
        hal_camera_signal_connect();
        xSemaphoreGive(s_ctx.lock);
    }
}

static void hal_camera_on_response(sscma_client_handle_t client, const sscma_client_reply_t *reply, void *user_ctx) {
    (void)client;
    (void)user_ctx;
    if (!hal_camera_extract_inference(reply)) {
        (void)hal_camera_extract_capture(reply);
    }
}

static void hal_camera_on_event(sscma_client_handle_t client, const sscma_client_reply_t *reply, void *user_ctx) {
    (void)client;
    (void)user_ctx;
    if (!hal_camera_extract_inference(reply)) {
        (void)hal_camera_extract_capture(reply);
    }
}

static esp_err_t hal_camera_ensure_sync_primitives(void) {
    if (s_ctx.lock == NULL) {
        s_ctx.lock = xSemaphoreCreateMutex();
        ESP_RETURN_ON_FALSE(s_ctx.lock != NULL, ESP_ERR_NO_MEM, TAG, "create lock failed");
    }

    if (s_ctx.connect_sem == NULL) {
        s_ctx.connect_sem = xSemaphoreCreateBinary();
        ESP_RETURN_ON_FALSE(s_ctx.connect_sem != NULL, ESP_ERR_NO_MEM, TAG, "create connect semaphore failed");
    }

    if (s_ctx.capture_sem == NULL) {
        s_ctx.capture_sem = xSemaphoreCreateBinary();
        ESP_RETURN_ON_FALSE(s_ctx.capture_sem != NULL, ESP_ERR_NO_MEM, TAG, "create capture semaphore failed");
    }

    return ESP_OK;
}

static void hal_camera_drain_semaphore(SemaphoreHandle_t sem) {
    if (sem == NULL) {
        return;
    }

    while (xSemaphoreTake(sem, 0) == pdTRUE) {
    }
}

#if CONFIG_WATCHER_CAMERA_VERBOSE_LOGS
static void hal_camera_log_sensor_catalog(void) {
    (void)hal_camera_refresh_sensor_catalog(HAL_CAMERA_VERBOSE_LOGS_ENABLED != 0);
}
#endif

static esp_err_t hal_camera_log_device_info(void) {
#if !CONFIG_WATCHER_CAMERA_VERBOSE_LOGS
    return ESP_OK;
#else
    sscma_client_info_t *info = NULL;
    sscma_client_model_t *model = NULL;
    sscma_client_sensor_t sensor = {0};
    esp_err_t ret;

    ret = sscma_client_get_info(s_ctx.client, &info, false);
    if (ret == ESP_OK && info != NULL) {
        ESP_LOGI(TAG, "HX6538 id=%s name=%s hw=%s fw=%s at=%s", info->id ? info->id : "<null>",
                 info->name ? info->name : "<null>", info->hw_ver ? info->hw_ver : "<null>",
                 info->fw_ver ? info->fw_ver : "<null>", info->sw_ver ? info->sw_ver : "<null>");
    } else {
        ESP_LOGW(TAG, "sscma_client_get_info failed: %s", esp_err_to_name(ret));
    }

    ret = sscma_client_get_model(s_ctx.client, &model, false);
    if (ret == ESP_OK && model != NULL) {
        ESP_LOGI(TAG, "HX6538 model id=%d uuid=%s name=%s ver=%s", model->id, model->uuid ? model->uuid : "<null>",
                 model->name ? model->name : "<null>", model->ver ? model->ver : "<null>");
    } else {
        ESP_LOGW(TAG, "sscma_client_get_model failed: %s", esp_err_to_name(ret));
    }

    ret = sscma_client_get_sensor(s_ctx.client, &sensor);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "HX6538 sensor id=%d type=%d state=%d opt_id=%d detail=%s", sensor.id, sensor.type, sensor.state,
                 sensor.opt_id, sensor.opt_detail ? sensor.opt_detail : "<null>");
        if (xSemaphoreTake(s_ctx.lock, portMAX_DELAY) == pdTRUE) {
            hal_camera_update_sensor_state_locked(sensor.id, sensor.opt_id, 0);
            xSemaphoreGive(s_ctx.lock);
        }
        free(sensor.opt_detail);
    } else {
        ESP_LOGW(TAG, "sscma_client_get_sensor failed: %s", esp_err_to_name(ret));
    }

    hal_camera_log_sensor_catalog();

    return ESP_OK;
#endif
}

static const char *hal_camera_trim_image_string(const char *image, size_t *len) {
    const char *payload = image;
    size_t payload_len;

    if (image == NULL || len == NULL) {
        return NULL;
    }

    if (strncmp(payload, "data:", 5) == 0) {
        const char *comma = strchr(payload, ',');
        if (comma != NULL) {
            payload = comma + 1;
        }
    }

    while (*payload != '\0' && isspace((unsigned char)*payload)) {
        payload++;
    }

    payload_len = strlen(payload);
    while (payload_len > 0 && isspace((unsigned char)payload[payload_len - 1])) {
        payload_len--;
    }

    *len = payload_len;
    return payload;
}

static esp_err_t hal_camera_decode_image(const char *image, uint8_t **jpeg, size_t *jpeg_size) {
    const char *payload;
    size_t payload_len = 0;
    size_t max_output = 0;
    size_t decoded_len = 0;
    uint8_t *buffer = NULL;
    int ret;

    ESP_RETURN_ON_FALSE(image != NULL && jpeg != NULL && jpeg_size != NULL, ESP_ERR_INVALID_ARG, TAG,
                        "invalid decode arguments");

    payload = hal_camera_trim_image_string(image, &payload_len);
    ESP_RETURN_ON_FALSE(payload != NULL && payload_len > 0, ESP_ERR_INVALID_ARG, TAG, "image payload empty");

    max_output = ((payload_len + 3) / 4) * 3 + 4;
    buffer = (uint8_t *)heap_caps_malloc(max_output, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (buffer == NULL) {
        buffer = (uint8_t *)heap_caps_malloc(max_output, MALLOC_CAP_8BIT);
    }
    ESP_RETURN_ON_FALSE(buffer != NULL, ESP_ERR_NO_MEM, TAG, "alloc jpeg buffer failed");

    ret = mbedtls_base64_decode(buffer, max_output, &decoded_len, (const unsigned char *)payload, payload_len);
    if (ret != 0) {
        free(buffer);
        ESP_LOGW(TAG, "base64 decode failed: -0x%04x", (unsigned int)(-ret));
        return ESP_ERR_INVALID_RESPONSE;
    }

    if (decoded_len < 2 || buffer[0] != 0xFF || buffer[1] != 0xD8) {
        free(buffer);
        ESP_LOGW(TAG, "decoded payload is not a JPEG, len=%u", (unsigned int)decoded_len);
        return ESP_ERR_INVALID_RESPONSE;
    }

    *jpeg = buffer;
    *jpeg_size = decoded_len;
    return ESP_OK;
}

static esp_err_t hal_camera_prepare_request(bool from_stream) {
    ESP_RETURN_ON_ERROR(hal_camera_init(), TAG, "hal_camera_init failed");

    if (xSemaphoreTake(s_ctx.lock, portMAX_DELAY) != pdTRUE) {
        return ESP_FAIL;
    }

    if (s_ctx.capture_in_progress || s_ctx.inferencing || s_ctx.model_maintenance_in_progress) {
        xSemaphoreGive(s_ctx.lock);
        ESP_LOGW(TAG, "capture already in progress");
        return ESP_ERR_INVALID_STATE;
    }

    if (s_ctx.streaming && !from_stream) {
        xSemaphoreGive(s_ctx.lock);
        ESP_LOGW(TAG, "capture rejected while streaming");
        return ESP_ERR_INVALID_STATE;
    }

    hal_camera_clear_capture_locked();
    s_ctx.capture_in_progress = true;
    s_ctx.capture_status = ESP_ERR_TIMEOUT;
    xSemaphoreGive(s_ctx.lock);

    hal_camera_drain_semaphore(s_ctx.capture_sem);
    return ESP_OK;
}

static void hal_camera_abort_capture(void) {
    if (xSemaphoreTake(s_ctx.lock, portMAX_DELAY) == pdTRUE) {
        s_ctx.capture_in_progress = false;
        hal_camera_clear_capture_locked();
        xSemaphoreGive(s_ctx.lock);
    }
}

static esp_err_t hal_camera_request_frame(bool from_stream) {
    (void)from_stream;
    /* The deployed SSCMA firmware returns an unexposed/stale JPEG for SAMPLE
     * after sensor setup. INVOKE is its live JPEG path and is already used by
     * continuous preview, so one-shot capture must use the same source. */
    return sscma_client_invoke(s_ctx.client, 1, false, true);
}

static esp_err_t hal_camera_take_image_string(bool from_stream, char **image, int *image_size,
                                              hal_camera_capture_timing_t *timing) {
    esp_err_t ret;
    bool one_shot_active = false;
    int64_t invoke_start_us = 0;
    int64_t invoke_return_us = 0;
    int64_t image_ready_us = 0;

    ESP_RETURN_ON_FALSE(image != NULL && image_size != NULL, ESP_ERR_INVALID_ARG, TAG, "invalid image output");
    *image = NULL;
    *image_size = 0;
    if (timing != NULL) {
        memset(timing, 0, sizeof(*timing));
    }

    ESP_RETURN_ON_ERROR(hal_camera_prepare_request(from_stream), TAG, "prepare capture failed");
    if (!from_stream) {
        hal_camera_publish_capture_activity(true);
        one_shot_active = true;
    }

    invoke_start_us = esp_timer_get_time();
    ret = hal_camera_request_frame(from_stream);
    invoke_return_us = esp_timer_get_time();
    if (ret != ESP_OK) {
        hal_camera_abort_capture();
        ESP_LOGE(TAG, "sscma invoke failed: %s", esp_err_to_name(ret));
        goto cleanup;
    }

    if (xSemaphoreTake(s_ctx.capture_sem, pdMS_TO_TICKS(HAL_CAMERA_CAPTURE_TIMEOUT_MS)) != pdTRUE) {
        hal_camera_abort_capture();
        ESP_LOGW(TAG, "capture timed out after %d ms", HAL_CAMERA_CAPTURE_TIMEOUT_MS);
        ret = ESP_ERR_TIMEOUT;
        goto cleanup;
    }
    image_ready_us = esp_timer_get_time();

    if (xSemaphoreTake(s_ctx.lock, portMAX_DELAY) != pdTRUE) {
        ret = ESP_FAIL;
        goto cleanup;
    }

    ret = s_ctx.capture_status;
    *image = s_ctx.capture_image;
    *image_size = s_ctx.capture_image_size;
    s_ctx.capture_image = NULL;
    s_ctx.capture_image_size = 0;
    s_ctx.capture_in_progress = false;
    xSemaphoreGive(s_ctx.lock);

    if (timing != NULL) {
        timing->invoke_call_us = (uint32_t)(invoke_return_us - invoke_start_us);
        timing->wait_image_us = (uint32_t)(image_ready_us - invoke_return_us);
    }
    if (!from_stream) {
        ESP_LOGI(TAG, "photo timing: request_ms=%lu reply_ms=%lu image_ready_ms=%lu request_to_image_ms=%lu",
                 (unsigned long)(invoke_start_us / 1000), (unsigned long)(invoke_return_us / 1000),
                 (unsigned long)(image_ready_us / 1000), (unsigned long)((image_ready_us - invoke_start_us) / 1000));
    }

cleanup:
    if (one_shot_active) {
        hal_camera_publish_capture_activity(false);
    }
    return ret;
}

static esp_err_t hal_camera_capture_jpeg_internal(bool from_stream, uint8_t **jpeg, size_t *jpeg_size,
                                                  uint32_t *timestamp_ms, hal_camera_capture_timing_t *timing) {
    esp_err_t ret;
    char *image = NULL;
    int image_size = 0;
    int64_t capture_start_us = 0;
    int64_t decode_start_us = 0;
    int64_t decode_done_us = 0;
    int64_t capture_done_us = 0;

    ESP_RETURN_ON_FALSE(jpeg != NULL && jpeg_size != NULL && timestamp_ms != NULL, ESP_ERR_INVALID_ARG, TAG,
                        "invalid jpeg output");

    *jpeg = NULL;
    *jpeg_size = 0;
    *timestamp_ms = 0;
    if (timing != NULL) {
        memset(timing, 0, sizeof(*timing));
    }

    capture_start_us = esp_timer_get_time();
    ret = hal_camera_take_image_string(from_stream, &image, &image_size, timing);
    ESP_GOTO_ON_ERROR(ret, cleanup, TAG, "capture failed");
    ESP_GOTO_ON_FALSE(image != NULL && image_size > 0, ESP_ERR_INVALID_RESPONSE, cleanup, TAG, "capture image missing");
    decode_start_us = esp_timer_get_time();
    ESP_GOTO_ON_ERROR(hal_camera_decode_image(image, jpeg, jpeg_size), cleanup, TAG, "image decode failed");
    decode_done_us = esp_timer_get_time();

    *timestamp_ms = (uint32_t)(esp_timer_get_time() / 1000ULL);
    capture_done_us = esp_timer_get_time();

cleanup:
    if (timing != NULL) {
        if (decode_done_us > 0 && decode_start_us > 0) {
            timing->decode_us = (uint32_t)(decode_done_us - decode_start_us);
        }
        if (capture_done_us == 0) {
            capture_done_us = esp_timer_get_time();
        }
        if (capture_start_us > 0) {
            timing->capture_total_us = (uint32_t)(capture_done_us - capture_start_us);
            timing->timestamp_total_us = timing->capture_total_us;
        }
    }
    if (image != NULL) {
        free(image);
    }
    return ret;
}

static esp_err_t hal_camera_prime_sensor(void) {
    for (int frame_index = 0; frame_index < HAL_CAMERA_SSCMA_PRIME_FRAMES; ++frame_index) {
        uint8_t *jpeg = NULL;
        size_t jpeg_size = 0;
        uint32_t timestamp_ms = 0;
        esp_err_t ret = hal_camera_capture_jpeg_internal(true, &jpeg, &jpeg_size, &timestamp_ms, NULL);

        free(jpeg);
        ESP_RETURN_ON_ERROR(ret, TAG, "camera sensor prime frame failed");
        vTaskDelay(pdMS_TO_TICKS(HAL_CAMERA_SSCMA_PRIME_SETTLE_MS));
    }

    ESP_LOGI(TAG, "camera sensor primed with %d live frames", HAL_CAMERA_SSCMA_PRIME_FRAMES);
    return ESP_OK;
}

static void hal_camera_stream_task(void *arg) {
    (void)arg;
    uint64_t capture_total_acc_us = 0;
    uint64_t invoke_call_acc_us = 0;
    uint64_t wait_image_acc_us = 0;
    uint64_t decode_acc_us = 0;
    uint64_t callback_acc_us = 0;
    uint64_t loop_total_acc_us = 0;
    uint32_t timing_count = 0;
    bool capture_activity_started = false;

    while (true) {
        uint64_t loop_start_us = (uint64_t)esp_timer_get_time();
        hal_camera_frame_cb_t frame_cb = NULL;
        void *frame_ctx = NULL;
        int fps = 0;
        uint8_t *jpeg = NULL;
        size_t jpeg_size = 0;
        uint32_t timestamp_ms = 0;
        esp_err_t ret;
        uint32_t ok_count = 0;
        uint32_t err_count = 0;
        hal_camera_capture_timing_t capture_timing = {0};
        hal_camera_stream_timing_sample_t sample = {0};
        int64_t callback_start_us = 0;
        int64_t callback_done_us = 0;

        if (xSemaphoreTake(s_ctx.lock, portMAX_DELAY) != pdTRUE) {
            break;
        }

        if (s_ctx.stream_stop_requested) {
            s_ctx.streaming = false;
            s_ctx.stream_stop_requested = false;
            s_ctx.stream_task = NULL;
            s_ctx.stream_cb = NULL;
            s_ctx.stream_ctx = NULL;
            s_ctx.stream_fps = 0;
            xSemaphoreGive(s_ctx.lock);
            break;
        }

        frame_cb = s_ctx.stream_cb;
        frame_ctx = s_ctx.stream_ctx;
        fps = s_ctx.stream_fps;
        xSemaphoreGive(s_ctx.lock);

        if (!capture_activity_started) {
            hal_camera_publish_capture_activity(true);
            capture_activity_started = true;
        }
        ret = hal_camera_capture_jpeg_internal(true, &jpeg, &jpeg_size, &timestamp_ms, &capture_timing);
        sample.capture_total_us = capture_timing.capture_total_us;
        sample.invoke_call_us = capture_timing.invoke_call_us;
        sample.wait_image_us = capture_timing.wait_image_us;
        sample.decode_us = capture_timing.decode_us;
        if (ret == ESP_OK) {
            if (frame_cb != NULL) {
                callback_start_us = esp_timer_get_time();
                frame_cb(jpeg, jpeg_size, timestamp_ms, frame_ctx);
                callback_done_us = esp_timer_get_time();
                sample.callback_us = (uint32_t)(callback_done_us - callback_start_us);
            }

            if (xSemaphoreTake(s_ctx.lock, portMAX_DELAY) == pdTRUE) {
                s_ctx.stream_frames_ok++;
                ok_count = s_ctx.stream_frames_ok;
                xSemaphoreGive(s_ctx.lock);
            }

            sample.loop_total_us = (uint32_t)((uint64_t)esp_timer_get_time() - loop_start_us);
            capture_total_acc_us += sample.capture_total_us;
            invoke_call_acc_us += sample.invoke_call_us;
            wait_image_acc_us += sample.wait_image_us;
            decode_acc_us += sample.decode_us;
            callback_acc_us += sample.callback_us;
            loop_total_acc_us += sample.loop_total_us;
            timing_count++;

            if (ok_count > 0 && (ok_count % HAL_CAMERA_STREAM_LOG_EVERY) == 0) {
                ESP_LOGI(TAG,
                         "stream frames ok=%lu err=%lu fps=%d last_jpeg=%u timing_us latest{capture=%lu invoke=%lu "
                         "wait=%lu decode=%lu callback=%lu loop=%lu} avg{capture=%lu invoke=%lu wait=%lu decode=%lu "
                         "callback=%lu loop=%lu}",
                         (unsigned long)ok_count, (unsigned long)s_ctx.stream_frames_err, fps, (unsigned int)jpeg_size,
                         (unsigned long)sample.capture_total_us, (unsigned long)sample.invoke_call_us,
                         (unsigned long)sample.wait_image_us, (unsigned long)sample.decode_us,
                         (unsigned long)sample.callback_us, (unsigned long)sample.loop_total_us,
                         (unsigned long)(capture_total_acc_us / timing_count),
                         (unsigned long)(invoke_call_acc_us / timing_count),
                         (unsigned long)(wait_image_acc_us / timing_count),
                         (unsigned long)(decode_acc_us / timing_count), (unsigned long)(callback_acc_us / timing_count),
                         (unsigned long)(loop_total_acc_us / timing_count));
            }
        } else {
            if (xSemaphoreTake(s_ctx.lock, portMAX_DELAY) == pdTRUE) {
                s_ctx.stream_frames_err++;
                err_count = s_ctx.stream_frames_err;
                xSemaphoreGive(s_ctx.lock);
            }
            ESP_LOGW(TAG, "stream frame failed #%lu: %s", (unsigned long)err_count, esp_err_to_name(ret));
        }

        if (jpeg != NULL) {
            free(jpeg);
        }

        if (fps > 0) {
            uint64_t period_us = 1000000ULL / (uint64_t)fps;
            uint64_t elapsed_us = (uint64_t)esp_timer_get_time() - loop_start_us;
            if (elapsed_us < period_us) {
                uint32_t delay_ms = (uint32_t)((period_us - elapsed_us + 999ULL) / 1000ULL);
                if (delay_ms > 0) {
                    vTaskDelay(pdMS_TO_TICKS(delay_ms));
                }
            }
        }
    }

    ESP_LOGI(TAG, "camera stream task exited");
    if (capture_activity_started) {
        hal_camera_publish_capture_activity(false);
    }
    vTaskDeleteWithCaps(NULL);
}

/* The init owner keeps the gate closed until callbacks and worker tasks have
 * been detached. Do not use hal_camera_deinit here: init waiters still own the
 * shared synchronization primitives. A failed teardown must retain its handle
 * and be retried before any new client is started. */
static esp_err_t hal_camera_abort_init(esp_err_t status) {
    esp_err_t cleanup = ESP_OK;
    if (s_ctx.client != NULL && s_ctx.callbacks_registered) {
        cleanup = sscma_client_register_callback(s_ctx.client, NULL, NULL);
    }
    if (cleanup == ESP_OK && s_ctx.client != NULL) {
        cleanup = bsp_sscma_client_deinit();
    }
    if (xSemaphoreTake(s_ctx.lock, portMAX_DELAY) == pdTRUE) {
        if (cleanup == ESP_OK) {
            s_ctx.client = NULL;
            s_ctx.callbacks_registered = false;
            s_ctx.client_started = false;
            hal_camera_clear_capture_locked();
        }
        s_ctx.initialized = false;
        s_ctx.connected = false;
        s_ctx.init_cleanup_pending = cleanup != ESP_OK;
        s_ctx.init_status = status;
        s_ctx.init_in_progress = false;
        xSemaphoreGive(s_ctx.lock);
    }
    if (cleanup != ESP_OK) {
        ESP_LOGW(TAG, "camera init cleanup deferred: %s", esp_err_to_name(cleanup));
    }
    return status;
}

esp_err_t hal_camera_init(void) {
    esp_err_t ret;
    bool need_register = false;
    bool need_client_start = false;
    TickType_t waited_ticks = 0;
    const TickType_t step_ticks = pdMS_TO_TICKS(20);
    const TickType_t timeout_ticks = pdMS_TO_TICKS(HAL_CAMERA_CONNECT_TIMEOUT_MS);
    const sscma_client_callback_t callbacks = {
        .on_connect = hal_camera_on_connect,
        .on_disconnect = NULL,
        .on_response = hal_camera_on_response,
        .on_event = hal_camera_on_event,
        .on_log = NULL,
    };

    ESP_RETURN_ON_ERROR(hal_camera_ensure_sync_primitives(), TAG, "camera sync primitive init failed");

    while (true) {
        if (xSemaphoreTake(s_ctx.lock, portMAX_DELAY) != pdTRUE) {
            return ESP_FAIL;
        }

        if (s_ctx.initialized && s_ctx.connected && s_ctx.client != NULL) {
            xSemaphoreGive(s_ctx.lock);
            return ESP_OK;
        }

        if (s_ctx.init_in_progress) {
            esp_err_t init_status = s_ctx.init_status;
            bool initialized = s_ctx.initialized;
            xSemaphoreGive(s_ctx.lock);

            if (initialized && init_status == ESP_OK) {
                return ESP_OK;
            }

            if (waited_ticks >= timeout_ticks) {
                ESP_LOGE(TAG, "HX6538 init wait timeout after %d ms", HAL_CAMERA_CONNECT_TIMEOUT_MS);
                return ESP_ERR_TIMEOUT;
            }

            vTaskDelay(step_ticks);
            waited_ticks += step_ticks;
            continue;
        }

        /* Waiters share the completed attempt's failure. Otherwise N callers
         * could serialize N full hardware resets after one protocol timeout. */
        if (waited_ticks > 0 && s_ctx.init_status != ESP_OK) {
            esp_err_t init_status = s_ctx.init_status;
            xSemaphoreGive(s_ctx.lock);
            return init_status;
        }
        s_ctx.init_in_progress = true;
        if (s_ctx.init_cleanup_pending) {
            esp_err_t previous_status = s_ctx.init_status;
            xSemaphoreGive(s_ctx.lock);
            /* This attempt only completes deferred teardown. A subsequent
             * request can start a fresh client after successful cleanup. */
            return hal_camera_abort_init(previous_status);
        }
        s_ctx.init_status = ESP_ERR_TIMEOUT;

        if (s_ctx.client == NULL) {
            s_ctx.client = bsp_sscma_client_init();
            if (s_ctx.client == NULL) {
                s_ctx.init_in_progress = false;
                s_ctx.init_status = ESP_FAIL;
                xSemaphoreGive(s_ctx.lock);
                ESP_LOGE(TAG, "bsp_sscma_client_init failed");
                return ESP_FAIL;
            }
        }

        need_register = !s_ctx.callbacks_registered;
        need_client_start = !s_ctx.client_started || !s_ctx.connected;
        s_ctx.connected = false;
        hal_camera_clear_capture_locked();
        xSemaphoreGive(s_ctx.lock);
        break;
    }

    if (need_register) {
        ret = sscma_client_register_callback(s_ctx.client, &callbacks, NULL);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "sscma register callback failed: %s", esp_err_to_name(ret));
            return hal_camera_abort_init(ret);
        }

        if (xSemaphoreTake(s_ctx.lock, portMAX_DELAY) == pdTRUE) {
            s_ctx.callbacks_registered = true;
            xSemaphoreGive(s_ctx.lock);
        }
    }

    if (need_client_start) {
        hal_camera_drain_semaphore(s_ctx.connect_sem);
        ret = sscma_client_init(s_ctx.client);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "sscma init failed: %s", esp_err_to_name(ret));
            return hal_camera_abort_init(ret);
        }

        if (xSemaphoreTake(s_ctx.lock, portMAX_DELAY) == pdTRUE) {
            s_ctx.client_started = true;
            xSemaphoreGive(s_ctx.lock);
        }

        ret = xSemaphoreTake(s_ctx.connect_sem, pdMS_TO_TICKS(HAL_CAMERA_CONNECT_TIMEOUT_MS)) == pdTRUE
                  ? ESP_OK
                  : ESP_ERR_TIMEOUT;
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "HX6538 connect timeout after %d ms", HAL_CAMERA_CONNECT_TIMEOUT_MS);
            return hal_camera_abort_init(ret);
        }
    }

    if (xSemaphoreTake(s_ctx.lock, portMAX_DELAY) == pdTRUE) {
        s_ctx.initialized = true;
        s_ctx.init_in_progress = false;
        s_ctx.init_status = ESP_OK;
        xSemaphoreGive(s_ctx.lock);
    }

    hal_camera_log_device_info();
    return ESP_OK;
}

esp_err_t hal_camera_deinit(void) {
    esp_err_t status = ESP_OK;
    esp_err_t ret;
    bool callbacks_registered = false;

    if (s_ctx.lock != NULL) {
        status = hal_camera_stop();
        if (status != ESP_OK) {
            ESP_LOGW(TAG, "camera deinit deferred because stream stop failed: %s", esp_err_to_name(status));
            return status;
        }
        status = hal_camera_stop_inference();
        if (status != ESP_OK) {
            ESP_LOGW(TAG, "camera deinit deferred because inference stop failed: %s", esp_err_to_name(status));
            return status;
        }
    }

    if (s_ctx.lock != NULL && xSemaphoreTake(s_ctx.lock, portMAX_DELAY) == pdTRUE) {
        callbacks_registered = s_ctx.callbacks_registered;
        xSemaphoreGive(s_ctx.lock);
    }

    if (s_ctx.client != NULL && callbacks_registered) {
        ret = sscma_client_register_callback(s_ctx.client, NULL, NULL);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "camera deinit deferred because callback unregister failed: %s", esp_err_to_name(ret));
            return ret;
        }
    }

    if (s_ctx.lock != NULL && xSemaphoreTake(s_ctx.lock, portMAX_DELAY) != pdTRUE) {
        return ESP_FAIL;
    }

    if (s_ctx.lock != NULL) {
        s_ctx.initialized = false;
        s_ctx.connected = false;
        s_ctx.init_in_progress = false;
        s_ctx.init_cleanup_pending = false;
        s_ctx.callbacks_registered = false;
        s_ctx.client_started = false;
        s_ctx.capture_in_progress = false;
        s_ctx.streaming = false;
        s_ctx.inferencing = false;
        s_ctx.inference_stopping = false;
        s_ctx.stream_stop_requested = false;
        s_ctx.stream_task = NULL;
        s_ctx.stream_cb = NULL;
        s_ctx.stream_ctx = NULL;
        s_ctx.stream_fps = 0;
        s_ctx.stream_frames_ok = 0;
        s_ctx.stream_frames_err = 0;
        s_ctx.inference_cb = NULL;
        s_ctx.inference_ctx = NULL;
        memset(&s_ctx.inference_stats, 0, sizeof(s_ctx.inference_stats));
        s_ctx.inference_last_timestamp_ms = 0;
        hal_camera_clear_capture_locked();
        xSemaphoreGive(s_ctx.lock);
    }

    if (s_ctx.client != NULL) {
        ret = bsp_sscma_client_deinit();
        if (ret != ESP_OK && status == ESP_OK) {
            status = ret;
        }
        s_ctx.client = NULL;
    }

    if (s_ctx.connect_sem != NULL) {
        vSemaphoreDelete(s_ctx.connect_sem);
        s_ctx.connect_sem = NULL;
    }
    if (s_ctx.capture_sem != NULL) {
        vSemaphoreDelete(s_ctx.capture_sem);
        s_ctx.capture_sem = NULL;
    }
    if (s_ctx.lock != NULL) {
        vSemaphoreDelete(s_ctx.lock);
        s_ctx.lock = NULL;
    }

    return status;
}

esp_err_t hal_camera_configure(int width, int height, int quality, int *applied_width, int *applied_height) {
    const hal_camera_sensor_option_t *option = NULL;
    int sensor_id = HAL_CAMERA_DEFAULT_SENSOR_ID;
    int current_quality = HAL_CAMERA_DEFAULT_QUALITY;
    esp_err_t ret;

    if (applied_width != NULL) {
        *applied_width = 0;
    }
    if (applied_height != NULL) {
        *applied_height = 0;
    }

    ESP_RETURN_ON_ERROR(hal_camera_init(), TAG, "hal_camera_init failed");

    if (width > 0 || height > 0) {
        ESP_RETURN_ON_FALSE(width > 0 && height > 0, ESP_ERR_INVALID_ARG, TAG, "width/height must both be set");
        option = hal_camera_find_sensor_option_by_size(width, height);
        ESP_RETURN_ON_FALSE(option != NULL, ESP_ERR_NOT_SUPPORTED, TAG, "unsupported resolution %dx%d", width, height);
    }

    if (xSemaphoreTake(s_ctx.lock, portMAX_DELAY) != pdTRUE) {
        return ESP_FAIL;
    }

    if (s_ctx.streaming || s_ctx.capture_in_progress || s_ctx.inferencing || s_ctx.model_maintenance_in_progress) {
        xSemaphoreGive(s_ctx.lock);
        ESP_LOGW(TAG, "camera configure rejected while busy");
        return ESP_ERR_INVALID_STATE;
    }

    sensor_id = s_ctx.sensor_id > 0 ? s_ctx.sensor_id : HAL_CAMERA_DEFAULT_SENSOR_ID;
    current_quality = s_ctx.configured_quality;

    if (option == NULL) {
        const hal_camera_sensor_option_t *current = hal_camera_find_sensor_option_by_opt_id(s_ctx.sensor_opt_id);
        option = current != NULL ? current : hal_camera_find_sensor_option_by_opt_id(0);
    }

    if (quality > 0) {
        current_quality = quality;
    }

    xSemaphoreGive(s_ctx.lock);

    ret = sscma_client_set_sensor(s_ctx.client, sensor_id, option->opt_id, true);
    ESP_RETURN_ON_ERROR(ret, TAG, "sscma_client_set_sensor failed");

    if (xSemaphoreTake(s_ctx.lock, portMAX_DELAY) == pdTRUE) {
        hal_camera_update_sensor_state_locked(sensor_id, option->opt_id, current_quality);
        xSemaphoreGive(s_ctx.lock);
    }

    /* A freshly configured HX6538 returns dark/stale JPEGs for its first live
     * requests while exposure settles. Prime the SSCMA sensor before the SDK
     * starts the photo behavior so the shutter cue still selects the final
     * frame and applications do not need backend-specific warm-up logic. */
    ESP_RETURN_ON_ERROR(hal_camera_prime_sensor(), TAG, "camera sensor prime failed");

    (void)hal_camera_refresh_sensor_catalog(HAL_CAMERA_VERBOSE_LOGS_ENABLED != 0);
    ESP_LOGI(TAG, "camera sensor profile applied: sensor=%d opt_id=%d detail=%s quality_hint=%d", sensor_id,
             option->opt_id, option->detail, current_quality);

    if (applied_width != NULL) {
        *applied_width = option->width;
    }
    if (applied_height != NULL) {
        *applied_height = option->height;
    }

    if (quality > 0) {
        ESP_LOGI(TAG, "camera quality hint stored=%d (current SSCMA path does not expose writable JPEG quality)",
                 quality);
    }

    return ESP_OK;
}

esp_err_t hal_camera_start(int fps, hal_camera_frame_cb_t cb, void *ctx) {
    BaseType_t task_ret;

    ESP_RETURN_ON_FALSE(cb != NULL, ESP_ERR_INVALID_ARG, TAG, "frame callback required");
    ESP_RETURN_ON_FALSE(fps > 0 && fps <= HAL_CAMERA_MAX_FPS, ESP_ERR_INVALID_ARG, TAG, "fps out of range");
    ESP_RETURN_ON_ERROR(hal_camera_init(), TAG, "hal_camera_init failed");

    if (xSemaphoreTake(s_ctx.lock, portMAX_DELAY) != pdTRUE) {
        return ESP_FAIL;
    }

    if (s_ctx.streaming || s_ctx.stream_task != NULL || s_ctx.inferencing || s_ctx.model_maintenance_in_progress) {
        xSemaphoreGive(s_ctx.lock);
        ESP_LOGW(TAG, "camera already streaming");
        return ESP_ERR_INVALID_STATE;
    }

    if (s_ctx.capture_in_progress || s_ctx.inferencing) {
        xSemaphoreGive(s_ctx.lock);
        ESP_LOGW(TAG, "camera busy with one-shot capture");
        return ESP_ERR_INVALID_STATE;
    }

    s_ctx.streaming = true;
    s_ctx.stream_stop_requested = false;
    s_ctx.stream_cb = cb;
    s_ctx.stream_ctx = ctx;
    s_ctx.stream_fps = fps;
    s_ctx.stream_frames_ok = 0;
    s_ctx.stream_frames_err = 0;
    xSemaphoreGive(s_ctx.lock);

    /* JPEG capture performs no flash writes. Keep its stack in PSRAM, as the
     * PTL reader does, so streaming leaves internal RAM for Wi-Fi and DMA. */
    task_ret =
        xTaskCreateWithCaps(hal_camera_stream_task, "hal_camera_stream", HAL_CAMERA_STREAM_TASK_STACK, NULL,
                            HAL_CAMERA_STREAM_TASK_PRIORITY, &s_ctx.stream_task, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (task_ret != pdPASS) {
        if (xSemaphoreTake(s_ctx.lock, portMAX_DELAY) == pdTRUE) {
            s_ctx.streaming = false;
            s_ctx.stream_cb = NULL;
            s_ctx.stream_ctx = NULL;
            s_ctx.stream_fps = 0;
            s_ctx.stream_task = NULL;
            xSemaphoreGive(s_ctx.lock);
        }
        ESP_LOGE(TAG, "failed to create stream task");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "camera stream started, target_fps=%d", fps);
    return ESP_OK;
}

esp_err_t hal_camera_stop(void) {
    TickType_t waited_ticks = 0;
    const TickType_t step_ticks = pdMS_TO_TICKS(20);
    const TickType_t timeout_ticks = pdMS_TO_TICKS(HAL_CAMERA_STREAM_STOP_TIMEOUT_MS);

    if (xSemaphoreTake(s_ctx.lock, portMAX_DELAY) != pdTRUE) {
        return ESP_FAIL;
    }

    if (!s_ctx.streaming && s_ctx.stream_task == NULL) {
        xSemaphoreGive(s_ctx.lock);
        return ESP_OK;
    }

    s_ctx.stream_stop_requested = true;
    xSemaphoreGive(s_ctx.lock);

    while (waited_ticks < timeout_ticks) {
        bool done = false;

        if (xSemaphoreTake(s_ctx.lock, portMAX_DELAY) == pdTRUE) {
            done = (!s_ctx.streaming && s_ctx.stream_task == NULL);
            xSemaphoreGive(s_ctx.lock);
        }

        if (done) {
            ESP_LOGI(TAG, "camera stream stopped");
            return ESP_OK;
        }

        vTaskDelay(step_ticks);
        waited_ticks += step_ticks;
    }

    ESP_LOGW(TAG, "camera stream stop timed out after %d ms", HAL_CAMERA_STREAM_STOP_TIMEOUT_MS);
    return ESP_ERR_TIMEOUT;
}

esp_err_t hal_camera_prepare_capture(void) {
    /* SSCMA configure already primes exposure before returning. */
    return hal_camera_init();
}

esp_err_t hal_camera_capture_once(hal_camera_frame_cb_t cb, void *ctx) {
    uint8_t *jpeg = NULL;
    size_t jpeg_size = 0;
    uint32_t timestamp_ms;
    esp_err_t ret;

    ESP_RETURN_ON_FALSE(cb != NULL, ESP_ERR_INVALID_ARG, TAG, "frame callback required");
    ret = hal_camera_capture_jpeg_internal(false, &jpeg, &jpeg_size, &timestamp_ms, NULL);
    ESP_GOTO_ON_ERROR(ret, cleanup, TAG, "capture failed");
    cb(jpeg, jpeg_size, timestamp_ms, ctx);

cleanup:
    if (jpeg != NULL) {
        free(jpeg);
    }
    return ret;
}

esp_err_t hal_camera_get_model_info(hal_camera_model_info_t *out_info) {
    sscma_client_model_t *model = NULL;
    esp_err_t ret;

    ESP_RETURN_ON_FALSE(out_info != NULL, ESP_ERR_INVALID_ARG, TAG, "model output required");
    memset(out_info, 0, sizeof(*out_info));
    ESP_RETURN_ON_ERROR(hal_camera_init(), TAG, "hal_camera_init failed");

    ret = sscma_client_get_model(s_ctx.client, &model, false);
    ESP_RETURN_ON_ERROR(ret, TAG, "sscma_client_get_model failed");
    ESP_RETURN_ON_FALSE(model != NULL, ESP_ERR_NOT_FOUND, TAG, "active model missing");

    out_info->model_id = model->id;
    out_info->contains_face_class = hal_camera_model_has_face_class(model);
    if (model->name != NULL) {
        strncpy(out_info->name, model->name, sizeof(out_info->name) - 1u);
    }
    return ESP_OK;
}

static esp_err_t hal_camera_begin_model_maintenance(void) {
    ESP_RETURN_ON_ERROR(hal_camera_init(), TAG, "hal_camera_init failed");
    if (xSemaphoreTake(s_ctx.lock, portMAX_DELAY) != pdTRUE) {
        return ESP_FAIL;
    }
    if (s_ctx.streaming || s_ctx.stream_task != NULL || s_ctx.capture_in_progress || s_ctx.inferencing ||
        s_ctx.inference_stopping || s_ctx.model_maintenance_in_progress) {
        xSemaphoreGive(s_ctx.lock);
        ESP_LOGW(TAG, "model maintenance rejected while camera is busy");
        return ESP_ERR_INVALID_STATE;
    }
    s_ctx.model_maintenance_in_progress = true;
    xSemaphoreGive(s_ctx.lock);
    return ESP_OK;
}

static void hal_camera_end_model_maintenance(void) {
    if (s_ctx.lock != NULL && xSemaphoreTake(s_ctx.lock, portMAX_DELAY) == pdTRUE) {
        s_ctx.model_maintenance_in_progress = false;
        xSemaphoreGive(s_ctx.lock);
    }
}

esp_err_t hal_camera_set_active_model(int model_id) {
    esp_err_t ret;

    ESP_RETURN_ON_FALSE(model_id > 0 && model_id <= UINT8_MAX, ESP_ERR_INVALID_ARG, TAG, "model id out of range");
    ESP_RETURN_ON_ERROR(hal_camera_begin_model_maintenance(), TAG, "model selection unavailable");
    ret = sscma_client_set_model(s_ctx.client, model_id);
    hal_camera_end_model_maintenance();
    ESP_RETURN_ON_ERROR(ret, TAG, "sscma_client_set_model failed");
    ESP_LOGI(TAG, "active SSCMA model selected: id=%d", model_id);
    return ESP_OK;
}

esp_err_t hal_camera_set_model_info(const char *model_info) {
    esp_err_t ret;
    size_t model_info_len;

    ESP_RETURN_ON_FALSE(model_info != NULL, ESP_ERR_INVALID_ARG, TAG, "model info required");
    model_info_len = strnlen(model_info, HAL_CAMERA_MODEL_INFO_MAX_BYTES + 1U);
    ESP_RETURN_ON_FALSE(model_info_len > 0U && model_info_len <= HAL_CAMERA_MODEL_INFO_MAX_BYTES, ESP_ERR_INVALID_SIZE,
                        TAG, "model info size invalid");
    ESP_RETURN_ON_ERROR(hal_camera_begin_model_maintenance(), TAG, "model metadata update unavailable");
    ret = sscma_client_set_model_info(s_ctx.client, model_info);
    hal_camera_end_model_maintenance();
    ESP_RETURN_ON_ERROR(ret, TAG, "sscma_client_set_model_info failed");
    ESP_LOGI(TAG, "active SSCMA model metadata updated: bytes=%u", (unsigned)model_info_len);
    return ESP_OK;
}

esp_err_t hal_camera_get_diagnostics(hal_camera_diagnostics_t *out_diagnostics) {
    esp_err_t init_status;

    ESP_RETURN_ON_FALSE(out_diagnostics != NULL, ESP_ERR_INVALID_ARG, TAG, "diagnostics output required");
    memset(out_diagnostics, 0, sizeof(*out_diagnostics));
    out_diagnostics->backend = "sscma";
    out_diagnostics->supports_capture = true;
    out_diagnostics->supports_preview = true;
    out_diagnostics->supports_inference = true;
    out_diagnostics->supports_model_info = true;
    out_diagnostics->supports_model_management = false;
    out_diagnostics->model_status = ESP_ERR_NOT_FOUND;

    init_status = hal_camera_init();
    out_diagnostics->status = init_status;
    if (init_status != ESP_OK || s_ctx.lock == NULL) {
        return ESP_OK;
    }
    if (xSemaphoreTake(s_ctx.lock, portMAX_DELAY) != pdTRUE) {
        out_diagnostics->status = ESP_FAIL;
        return ESP_OK;
    }
    out_diagnostics->initialized = s_ctx.initialized;
    out_diagnostics->connected = s_ctx.connected;
    out_diagnostics->streaming = s_ctx.streaming;
    out_diagnostics->inferencing = s_ctx.inferencing || s_ctx.inference_stopping;
    out_diagnostics->busy = s_ctx.capture_in_progress || s_ctx.streaming || s_ctx.inferencing ||
                            s_ctx.inference_stopping || s_ctx.stream_task != NULL ||
                            s_ctx.model_maintenance_in_progress;
    xSemaphoreGive(s_ctx.lock);

    if (out_diagnostics->busy) {
        out_diagnostics->model_status = ESP_ERR_INVALID_STATE;
        return ESP_OK;
    }
    out_diagnostics->model_status = hal_camera_get_model_info(&out_diagnostics->model);
    out_diagnostics->model_available = out_diagnostics->model_status == ESP_OK;
    return ESP_OK;
}

esp_err_t hal_camera_start_inference(hal_camera_inference_cb_t cb, void *ctx) {
    const hal_camera_inference_options_t options = {
        .include_preview_image = false,
    };
    return hal_camera_start_inference_ex(cb, ctx, &options);
}

esp_err_t hal_camera_start_inference_ex(hal_camera_inference_cb_t cb, void *ctx,
                                        const hal_camera_inference_options_t *options) {
    hal_camera_model_info_t model = {0};
    esp_err_t ret;
    const bool include_preview_image = options != NULL && options->include_preview_image;

    ESP_RETURN_ON_FALSE(cb != NULL, ESP_ERR_INVALID_ARG, TAG, "inference callback required");
    ESP_RETURN_ON_ERROR(hal_camera_get_model_info(&model), TAG, "active model query failed");
    ESP_RETURN_ON_ERROR(hal_camera_refresh_active_sensor(), TAG, "active sensor query failed");

    if (xSemaphoreTake(s_ctx.lock, portMAX_DELAY) != pdTRUE) {
        return ESP_FAIL;
    }
    if (s_ctx.inferencing || s_ctx.inference_stopping || s_ctx.streaming || s_ctx.stream_task != NULL ||
        s_ctx.capture_in_progress || s_ctx.model_maintenance_in_progress) {
        xSemaphoreGive(s_ctx.lock);
        ESP_LOGW(TAG, "inference start rejected while camera is busy");
        return ESP_ERR_INVALID_STATE;
    }

    s_ctx.inferencing = true;
    s_ctx.inference_cb = cb;
    s_ctx.inference_ctx = ctx;
    memset(&s_ctx.inference_stats, 0, sizeof(s_ctx.inference_stats));
    s_ctx.inference_last_timestamp_ms = 0u;
    xSemaphoreGive(s_ctx.lock);

    hal_camera_publish_capture_activity(true);
    ret = sscma_client_invoke(s_ctx.client, -1, false, include_preview_image);
    if (ret != ESP_OK) {
        if (xSemaphoreTake(s_ctx.lock, portMAX_DELAY) == pdTRUE) {
            s_ctx.inferencing = false;
            s_ctx.inference_cb = NULL;
            s_ctx.inference_ctx = NULL;
            xSemaphoreGive(s_ctx.lock);
        }
        hal_camera_publish_capture_activity(false);
        ESP_LOGE(TAG, "continuous inference invoke failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "continuous face inference started: model_id=%d name=%s sensor=%dx%d preview=%s", model.model_id,
             model.name, s_ctx.configured_width, s_ctx.configured_height,
             include_preview_image ? "enabled" : "disabled");
    return ESP_OK;
}

esp_err_t hal_camera_stop_inference(void) {
    esp_err_t ret = ESP_OK;

    if (s_ctx.lock == NULL) {
        return ESP_OK;
    }
    if (xSemaphoreTake(s_ctx.lock, portMAX_DELAY) != pdTRUE) {
        return ESP_FAIL;
    }
    if (s_ctx.inference_stopping) {
        xSemaphoreGive(s_ctx.lock);
        return ESP_ERR_INVALID_STATE;
    }
    if (!s_ctx.inferencing) {
        xSemaphoreGive(s_ctx.lock);
        return ESP_OK;
    }
    s_ctx.inference_stopping = true;
    xSemaphoreGive(s_ctx.lock);

    if (s_ctx.client != NULL) {
        ret = sscma_client_break(s_ctx.client);
    }

    if (xSemaphoreTake(s_ctx.lock, portMAX_DELAY) != pdTRUE) {
        return ESP_FAIL;
    }
    s_ctx.inference_stopping = false;
    if (ret == ESP_OK) {
        s_ctx.inferencing = false;
        s_ctx.inference_cb = NULL;
        s_ctx.inference_ctx = NULL;
    }
    xSemaphoreGive(s_ctx.lock);

    if (ret == ESP_OK) {
        hal_camera_publish_capture_activity(false);
        ESP_LOGI(TAG, "continuous face inference stopped");
    } else {
        ESP_LOGW(TAG, "continuous inference break failed; camera remains busy for retry: %s", esp_err_to_name(ret));
    }
    return ret;
}

bool hal_camera_is_inferencing(void) {
    bool inferencing = false;

    if (s_ctx.lock != NULL && xSemaphoreTake(s_ctx.lock, portMAX_DELAY) == pdTRUE) {
        inferencing = s_ctx.inferencing || s_ctx.inference_stopping;
        xSemaphoreGive(s_ctx.lock);
    }
    return inferencing;
}

esp_err_t hal_camera_get_inference_stats(hal_camera_inference_stats_t *out_stats) {
    ESP_RETURN_ON_FALSE(out_stats != NULL, ESP_ERR_INVALID_ARG, TAG, "stats output required");
    ESP_RETURN_ON_FALSE(s_ctx.lock != NULL, ESP_ERR_INVALID_STATE, TAG, "camera not initialized");
    if (xSemaphoreTake(s_ctx.lock, portMAX_DELAY) != pdTRUE) {
        return ESP_FAIL;
    }
    *out_stats = s_ctx.inference_stats;
    xSemaphoreGive(s_ctx.lock);
    return ESP_OK;
}

bool hal_camera_is_streaming(void) {
    bool streaming = false;

    if (s_ctx.lock == NULL || !s_ctx.initialized) {
        return false;
    }

    if (xSemaphoreTake(s_ctx.lock, portMAX_DELAY) == pdTRUE) {
        streaming = s_ctx.streaming;
        xSemaphoreGive(s_ctx.lock);
    }

    return streaming;
}

esp_err_t hal_camera_list_models(hal_camera_model_info_t *models, size_t capacity, size_t *count) {
    (void)models;
    (void)capacity;
    (void)count;
    return ESP_ERR_NOT_SUPPORTED;
}

