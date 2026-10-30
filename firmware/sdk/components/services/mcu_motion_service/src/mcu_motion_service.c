#include "mcu_motion_service.h"

#include "esp_log.h"
#if defined(ESP_PLATFORM)
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "mcu_link_uart.h"
#include "motion_arbiter.h"
#endif
#include "mcu_link_bootstrap.h"

#include <stdbool.h>
#include <string.h>

static const char *TAG = "MCU_MOTION";
#define MCU_MOTION_CHUNKED_SEQUENCE_FRAME_GAP_MS 40u
#if !defined(WATCHER_STRESS_BUILD) && !defined(CONFIG_WATCHER_STRESS_BUILD)
static const char *OBS_TAG = "MCU_OBS";
#endif

static mcu_motion_request_t s_last_request;
static bool s_has_last_request;
static uint32_t s_last_command_seq;
static bool s_command_inflight;
/* SYS ACK/NACK carry no command class. Retain a bounded correlation window;
 * a reply older than the last 32 motion commands is ignored, never relabelled.
 * Typed DONE/FAULT events are not filtered, including DONE after STOP. */
#define MOTION_REPLY_HISTORY_SIZE 32u
static uint32_t s_reply_history[MOTION_REPLY_HISTORY_SIZE];
static unsigned s_reply_count;
static unsigned s_reply_next;
static mcu_motion_lifecycle_cb_t s_lifecycle_cb;
static void *s_lifecycle_ctx;
static unsigned s_lifecycle_active_callbacks;
static struct {
    uint32_t token, ref, ack_rx;
    mcu_motion_completion_status_t status;
} s_stop_observer;
static uint32_t s_stop_generation;
static bool s_stop_observing;
static struct {
    uint32_t ref;
    int16_t x, y;
    bool valid;
} s_face_jog_origin;
#if defined(ESP_PLATFORM)
static portMUX_TYPE s_lifecycle_lock = portMUX_INITIALIZER_UNLOCKED;
static StaticSemaphore_t s_submission_storage;
static SemaphoreHandle_t s_submission_mutex;
#endif

static void lifecycle_lock(void) {
#if defined(ESP_PLATFORM)
    portENTER_CRITICAL(&s_lifecycle_lock);
#endif
}

static void lifecycle_unlock(void) {
#if defined(ESP_PLATFORM)
    portEXIT_CRITICAL(&s_lifecycle_lock);
#endif
}

/* Separate from the SDK callback. STM32 reports one DONE per segment, with
 * ref_seq equal to the CHUNK that supplied that segment, not the END frame. */
#define OBSERVED_FRAME_COUNT                                                                                           \
    (2u + ((MCU_MOTION_CHUNKED_SEQUENCE_MAX_SEGMENTS + MCU_MOTION_SEQUENCE_CHUNK_MAX_SEGMENTS - 1u) /                  \
           MCU_MOTION_SEQUENCE_CHUNK_MAX_SEGMENTS))
static struct {
    uint32_t token;
    mcu_motion_completion_status_t status;
    uint32_t refs[OBSERVED_FRAME_COUNT];
    uint8_t expected[OBSERVED_FRAME_COUNT];
    uint8_t done[OBSERVED_FRAME_COUNT];
    unsigned frame_count;
    uint32_t seen[MCU_MOTION_CHUNKED_SEQUENCE_MAX_SEGMENTS];
    unsigned seen_count;
    bool submitted, origin_eligible;
    int16_t final_x, final_y;
} s_observed;
static uint32_t s_observed_generation;
static bool s_observed_sending;

static void observed_frame(uint32_t seq, uint8_t msg_id, const uint8_t *payload) {
    lifecycle_lock();
    if (s_observed_sending && s_observed.token && s_observed.frame_count < OBSERVED_FRAME_COUNT) {
        unsigned i = s_observed.frame_count++;
        s_observed.refs[i] = seq;
        s_observed.expected[i] = msg_id == MCU_MOTION_MSG_SERVO_SEQUENCE_CHUNK ? payload[3] : 0;
    }
    lifecycle_unlock();
}

/* Caller holds lifecycle_lock; never call user callbacks under this lock. */
static void observed_update(const mcu_motion_lifecycle_event_t *event) {
    if (!s_observed.token || s_observed.status != MCU_MOTION_COMPLETION_PENDING)
        return;
    for (unsigned i = 0; i < s_observed.frame_count; ++i) {
        if (s_observed.refs[i] != event->ref_seq && !(event->type == MCU_MOTION_LIFECYCLE_FAULT && event->ref_seq == 0))
            continue;
        if (event->type == MCU_MOTION_LIFECYCLE_REJECTED || event->type == MCU_MOTION_LIFECYCLE_FAULT ||
            (event->type == MCU_MOTION_LIFECYCLE_DONE && event->result != MCU_MOTION_RESULT_SUCCESS)) {
            s_observed.status = MCU_MOTION_COMPLETION_FAILED;
            return;
        }
        if (event->type == MCU_MOTION_LIFECYCLE_DONE && s_observed.expected[i]) {
            for (unsigned j = 0; j < s_observed.seen_count; ++j)
                if (s_observed.seen[j] == event->event_seq)
                    return;
            if (s_observed.done[i] >= s_observed.expected[i])
                return;
            s_observed.seen[s_observed.seen_count++] = event->event_seq;
            s_observed.done[i]++;
            s_observed.final_x = event->final_x_deg_x10;
            s_observed.final_y = event->final_y_deg_x10;
        }
        if (!s_observed.submitted)
            return;
        for (unsigned j = 0; j < s_observed.frame_count; ++j)
            if (s_observed.done[j] != s_observed.expected[j])
                return;
        s_observed.status = MCU_MOTION_COMPLETION_COMPLETED;
        if (s_observed.origin_eligible) {
            s_face_jog_origin.x = s_observed.final_x;
            s_face_jog_origin.y = s_observed.final_y;
            s_face_jog_origin.valid = true;
        }
        return;
    }
}

esp_err_t mcu_motion_sequence_status(uint32_t token, mcu_motion_completion_status_t *out_status) {
    if (!out_status)
        return ESP_ERR_INVALID_ARG;
    if (!token) {
        *out_status = MCU_MOTION_COMPLETION_COMPLETED;
        return ESP_OK;
    }
    lifecycle_lock();
    esp_err_t ret = token == s_observed.token ? ESP_OK : ESP_ERR_NOT_FOUND;
    if (ret == ESP_OK)
        *out_status = s_observed.status;
    lifecycle_unlock();
    return ret;
}

void mcu_motion_sequence_release(uint32_t token) {
    lifecycle_lock();
    if (token && token == s_observed.token)
        s_observed.token = 0;
    lifecycle_unlock();
}

static void remember_stop_sequence(uint32_t sequence) {
    lifecycle_lock();
    s_reply_history[s_reply_next] = sequence;
    s_reply_next = (s_reply_next + 1u) % MOTION_REPLY_HISTORY_SIZE;
    if (s_reply_count < MOTION_REPLY_HISTORY_SIZE)
        s_reply_count++;
    lifecycle_unlock();
}

static void remember_motion_sequence(uint32_t sequence) {
    lifecycle_lock();
    memset(&s_face_jog_origin, 0, sizeof(s_face_jog_origin));
    lifecycle_unlock();
    remember_stop_sequence(sequence);
}

static bool is_motion_sequence(uint32_t sequence) {
    bool found = false;
    lifecycle_lock();
    for (unsigned i = 0; i < s_reply_count; ++i) {
        if (s_reply_history[i] == sequence) {
            found = true;
            break;
        }
    }
    lifecycle_unlock();
    return found;
}

static bool mcu_motion_source_has_access(mcu_motion_source_t source) {
#if defined(ESP_PLATFORM)
    motion_arbiter_owner_t owner;
    switch (source) {
    case MCU_MOTION_SOURCE_BEHAVIOR:
        owner = MOTION_ARBITER_OWNER_BEHAVIOR;
        break;
    case MCU_MOTION_SOURCE_FACE_TRACKING:
        owner = MOTION_ARBITER_OWNER_FACE_TRACKING;
        break;
    case MCU_MOTION_SOURCE_RECOVERY:
        owner = MOTION_ARBITER_OWNER_RECOVERY;
        break;
    case MCU_MOTION_SOURCE_BLE:
    case MCU_MOTION_SOURCE_WS:
    case MCU_MOTION_SOURCE_RELAY_W1:
    case MCU_MOTION_SOURCE_UNKNOWN:
    default:
        owner = MOTION_ARBITER_OWNER_MANUAL;
        break;
    }
    return motion_arbiter_can_submit(owner);
#elif defined(MCU_MOTION_HOST_TEST)
    extern bool mcu_motion_test_source_has_access(mcu_motion_source_t source);
    return mcu_motion_test_source_has_access(source);
#else
    (void)source;
    return true;
#endif
}

/* Serialize only motion writers across BEGIN/CHUNK/END. The RX callback and
 * audio/LED paths never take this mutex. No ACK wait is performed here. */
static bool motion_submission_lock(void) {
#if defined(ESP_PLATFORM)
    return s_submission_mutex != NULL && xSemaphoreTake(s_submission_mutex, pdMS_TO_TICKS(300)) == pdTRUE;
#else
    return true;
#endif
}

static void motion_submission_unlock(void) {
#if defined(ESP_PLATFORM)
    xSemaphoreGive(s_submission_mutex);
#endif
}

static esp_err_t motion_submission_begin(mcu_motion_source_t source, bool moves) {
    if (!motion_submission_lock()) {
        return ESP_ERR_TIMEOUT;
    }
    if (!mcu_motion_source_has_access(source)) {
        motion_submission_unlock();
        return ESP_ERR_INVALID_STATE;
    }
    if (moves) {
        /* Revoke before sending: RX can complete the old observer inside
         * send_frame or a frame gap. Its completion contract stays intact. */
        lifecycle_lock();
        s_observed.origin_eligible = false;
        memset(&s_face_jog_origin, 0, sizeof(s_face_jog_origin));
        lifecycle_unlock();
    }
    return ESP_OK;
}

static uint16_t decode_u16_le(const uint8_t *src) {
    return (uint16_t)(((uint16_t)src[0]) | ((uint16_t)src[1] << 8u));
}

static uint32_t decode_u32_le(const uint8_t *src) {
    return ((uint32_t)src[0]) | ((uint32_t)src[1] << 8u) | ((uint32_t)src[2] << 16u) | ((uint32_t)src[3] << 24u);
}

static int16_t decode_i16_le(const uint8_t *src) {
    return (int16_t)decode_u16_le(src);
}

static void publish_lifecycle_event(const mcu_motion_lifecycle_event_t *event) {
    mcu_motion_lifecycle_cb_t callback = NULL;
    void *callback_context = NULL;

    if (event == NULL) {
        return;
    }
    lifecycle_lock();
    if (event->type == MCU_MOTION_LIFECYCLE_FAULT)
        s_observed.origin_eligible = false;
    observed_update(event);
    if (event->type == MCU_MOTION_LIFECYCLE_FAULT ||
        (s_face_jog_origin.ref && event->ref_seq == s_face_jog_origin.ref &&
         event->type == MCU_MOTION_LIFECYCLE_REJECTED)) {
        memset(&s_face_jog_origin, 0, sizeof(s_face_jog_origin));
    } else if (s_face_jog_origin.ref && event->ref_seq == s_face_jog_origin.ref &&
               event->type == MCU_MOTION_LIFECYCLE_DONE) {
        s_face_jog_origin.valid = event->result == MCU_MOTION_RESULT_STOPPED;
        s_face_jog_origin.x = event->final_x_deg_x10;
        s_face_jog_origin.y = event->final_y_deg_x10;
    }
    if (s_stop_observer.token && s_stop_observer.status == MCU_MOTION_COMPLETION_PENDING &&
        (event->ref_seq == s_stop_observer.ref || (event->type == MCU_MOTION_LIFECYCLE_FAULT && event->ref_seq == 0))) {
        if (event->type == MCU_MOTION_LIFECYCLE_ACKED) {
            s_stop_observer.status =
                event->status == 0 ? MCU_MOTION_COMPLETION_COMPLETED : MCU_MOTION_COMPLETION_FAILED;
            s_stop_observer.ack_rx = event->event_seq;
        } else if (event->type == MCU_MOTION_LIFECYCLE_REJECTED || event->type == MCU_MOTION_LIFECYCLE_FAULT) {
            s_stop_observer.status = MCU_MOTION_COMPLETION_FAILED;
        }
    }
    callback = s_lifecycle_cb;
    callback_context = s_lifecycle_ctx;
    if (callback != NULL) {
        s_lifecycle_active_callbacks++;
    }
    lifecycle_unlock();
    if (callback != NULL) {
        callback(event, callback_context);
        lifecycle_lock();
        s_lifecycle_active_callbacks--;
        lifecycle_unlock();
    }
}

static void encode_u16_le(uint8_t *dst, uint16_t value) {
    dst[0] = (uint8_t)(value & 0xFFu);
    dst[1] = (uint8_t)((value >> 8) & 0xFFu);
}

static void encode_i16_le(uint8_t *dst, int16_t value) {
    encode_u16_le(dst, (uint16_t)value);
}

static esp_err_t mcu_motion_submit_runtime_frame(const mcu_motion_request_t *request, uint32_t *out_seq) {
    uint8_t payload[9];
    mcu_link_t *link;
    uint32_t seq = 0u;
    size_t wire_len = 0u;
    esp_err_t ret;

    if (request == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    link = mcu_link_bootstrap_get_link();
    if (link == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    if (!mcu_link_bootstrap_is_ready()) {
        ESP_LOGW(TAG, "MCU link not fully ready; rejecting motion request");
        return ESP_ERR_INVALID_STATE;
    }

    payload[0] = request->axis_mask;
    encode_i16_le(&payload[1], request->x_deg_x10);
    encode_i16_le(&payload[3], request->y_deg_x10);
    encode_u16_le(&payload[5], request->duration_ms);
    payload[7] = request->motion_profile;
    payload[8] = (uint8_t)request->source;

    ret = mcu_link_send_frame(link, MCU_FRAME_CLASS_MOTION, MCU_MOTION_MSG_SERVO_MOVE, MCU_FRAME_FLAG_ACK_REQ, payload,
                              (uint16_t)sizeof(payload), &seq, &wire_len);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to queue SERVO_MOVE frame: %s", esp_err_to_name(ret));
        return ret;
    }

#if !defined(WATCHER_STRESS_BUILD) && !defined(CONFIG_WATCHER_STRESS_BUILD)
    ESP_LOGD(TAG, "Queued SERVO_MOVE frame seq=%lu wire_len=%u axis_mask=0x%02x duration_ms=%u", (unsigned long)seq,
             (unsigned)wire_len, request->axis_mask, (unsigned)request->duration_ms);
#endif
    remember_motion_sequence(seq);
    s_last_command_seq = seq;
    s_command_inflight = true;
    if (out_seq != NULL) {
        *out_seq = seq;
    }
    return ESP_OK;
}

static esp_err_t mcu_motion_submit_stop_frame(mcu_motion_source_t source) {
    uint8_t payload[2];
    mcu_link_t *link;
    uint32_t seq = 0u;
    size_t wire_len = 0u;
    esp_err_t ret;

    link = mcu_link_bootstrap_get_link();
    if (link == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    if (!mcu_link_bootstrap_is_ready()) {
        ESP_LOGW(TAG, "MCU link not fully ready; rejecting stop request");
        return ESP_ERR_INVALID_STATE;
    }

    payload[0] = 1u; /* all_pending: hal_servo_cancel_all() must clear the STM32 queue */
    /* Legacy STOP accepts tags 0..5. Arbitration still uses FACE_TRACKING;
     * this is wire attribution only, not a permission downgrade. */
    payload[1] = source == MCU_MOTION_SOURCE_FACE_TRACKING ? MCU_MOTION_SOURCE_UNKNOWN : (uint8_t)source;
#if defined(ESP_PLATFORM)
    if (!mcu_link_uart_try_lock())
        return ESP_ERR_INVALID_STATE;
#endif
    ret = mcu_link_send_frame(link, MCU_FRAME_CLASS_MOTION, MCU_MOTION_MSG_SERVO_STOP, MCU_FRAME_FLAG_ACK_REQ, payload,
                              (uint16_t)sizeof(payload), &seq, &wire_len);
    if (ret == ESP_OK) {
        remember_stop_sequence(seq);
        lifecycle_lock();
        if (s_stop_observing)
            s_stop_observer.ref = seq;
        lifecycle_unlock();
    }
#if defined(ESP_PLATFORM)
    mcu_link_uart_unlock();
#endif
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to queue SERVO_STOP frame: %s", esp_err_to_name(ret));
        return ret;
    }

#if !defined(WATCHER_STRESS_BUILD) && !defined(CONFIG_WATCHER_STRESS_BUILD)
    ESP_LOGI(TAG, "Queued SERVO_STOP frame seq=%lu wire_len=%u scope=all_pending source=%u", (unsigned long)seq,
             (unsigned)wire_len, (unsigned)source);
#endif
    s_last_command_seq = seq;
    s_command_inflight = true;
    return ESP_OK;
}

static esp_err_t mcu_motion_submit_pwm_unlock_frame(const mcu_motion_pwm_unlock_request_t *request) {
    uint8_t payload[2];
    mcu_link_t *link;
    uint32_t seq = 0u;
    size_t wire_len = 0u;
    esp_err_t ret;

    if (request == NULL || request->axis_mask == 0u ||
        (request->axis_mask & (uint8_t) ~(MCU_MOTION_AXIS_X | MCU_MOTION_AXIS_Y)) != 0u) {
        return ESP_ERR_INVALID_ARG;
    }

    link = mcu_link_bootstrap_get_link();
    if (link == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    if (!mcu_link_bootstrap_is_ready()) {
        ESP_LOGW(TAG, "MCU link not fully ready; rejecting PWM unlock request");
        return ESP_ERR_INVALID_STATE;
    }

    payload[0] = request->axis_mask;
    payload[1] = (uint8_t)request->source;
    ret = mcu_link_send_frame(link, MCU_FRAME_CLASS_MOTION, MCU_MOTION_MSG_SERVO_PWM_UNLOCK, MCU_FRAME_FLAG_ACK_REQ,
                              payload, (uint16_t)sizeof(payload), &seq, &wire_len);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to queue SERVO_PWM_UNLOCK frame: %s", esp_err_to_name(ret));
        return ret;
    }

#if !defined(WATCHER_STRESS_BUILD) && !defined(CONFIG_WATCHER_STRESS_BUILD)
    ESP_LOGD(TAG, "Queued SERVO_PWM_UNLOCK frame seq=%lu wire_len=%u axis_mask=0x%02x source=%u", (unsigned long)seq,
             (unsigned)wire_len, request->axis_mask, (unsigned)request->source);
#endif
    remember_motion_sequence(seq);
    s_last_command_seq = seq;
    s_command_inflight = true;
    return ESP_OK;
}

static esp_err_t mcu_motion_submit_pwm_lock_frame(const mcu_motion_pwm_lock_request_t *request) {
    uint8_t payload[2];
    uint32_t seq = 0u;
    size_t wire_len = 0u;
    esp_err_t ret;
    mcu_link_t *link;

    if (request == NULL || request->axis_mask == 0u ||
        (request->axis_mask & (uint8_t) ~(MCU_MOTION_AXIS_X | MCU_MOTION_AXIS_Y)) != 0u) {
        return ESP_ERR_INVALID_ARG;
    }

    link = mcu_link_bootstrap_get_link();
    if (link == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    if (!mcu_link_bootstrap_is_ready()) {
        ESP_LOGW(TAG, "MCU link not fully ready; rejecting PWM lock request");
        return ESP_ERR_INVALID_STATE;
    }

    payload[0] = request->axis_mask;
    payload[1] = (uint8_t)request->source;
    ret = mcu_link_send_frame(link, MCU_FRAME_CLASS_MOTION, MCU_MOTION_MSG_SERVO_PWM_LOCK, MCU_FRAME_FLAG_ACK_REQ,
                              payload, (uint16_t)sizeof(payload), &seq, &wire_len);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to queue SERVO_PWM_LOCK frame: %s", esp_err_to_name(ret));
        return ret;
    }

    remember_motion_sequence(seq);
    s_last_command_seq = seq;
    s_command_inflight = true;
    ESP_LOGD(TAG, "Queued SERVO_PWM_LOCK frame seq=%lu wire_len=%u axis_mask=0x%02x source=%u", (unsigned long)seq,
             (unsigned int)wire_len, request->axis_mask, (unsigned)request->source);
    return ESP_OK;
}

static esp_err_t mcu_motion_submit_jog_frame(const mcu_motion_jog_request_t *request, uint32_t *out_seq) {
    uint8_t payload[12];
    mcu_link_t *link;
    uint32_t seq = 0u;
    size_t wire_len = 0u;
    esp_err_t ret;

    if (request == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    link = mcu_link_bootstrap_get_link();
    if (link == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    if (!mcu_link_bootstrap_is_ready()) {
        ESP_LOGW(TAG, "MCU link not fully ready; rejecting jog request");
        return ESP_ERR_INVALID_STATE;
    }

    payload[0] = request->axis_mask;
    encode_i16_le(&payload[1], request->x_velocity_deg_x10_per_sec);
    encode_i16_le(&payload[3], request->y_velocity_deg_x10_per_sec);
    encode_u16_le(&payload[5], request->timeout_ms);
    payload[7] = (uint8_t)request->source;
    payload[8] = request->x_min_deg;
    payload[9] = request->x_max_deg;
    payload[10] = request->y_min_deg;
    payload[11] = request->y_max_deg;

#if defined(ESP_PLATFORM)
    if (!mcu_link_uart_try_lock())
        return ESP_ERR_INVALID_STATE;
#endif
    ret = mcu_link_send_frame(link, MCU_FRAME_CLASS_MOTION, MCU_MOTION_MSG_SERVO_JOG, MCU_FRAME_FLAG_ACK_REQ, payload,
                              (uint16_t)sizeof(payload), &seq, &wire_len);
    if (ret == ESP_OK) {
        remember_motion_sequence(seq);
        lifecycle_lock();
        if (request->source == MCU_MOTION_SOURCE_FACE_TRACKING)
            s_face_jog_origin.ref = seq;
        lifecycle_unlock();
    }
#if defined(ESP_PLATFORM)
    mcu_link_uart_unlock();
#endif
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to queue SERVO_JOG frame: %s", esp_err_to_name(ret));
        return ret;
    }

#if !defined(WATCHER_STRESS_BUILD) && !defined(CONFIG_WATCHER_STRESS_BUILD)
    if (request->source == MCU_MOTION_SOURCE_FACE_TRACKING) {
        ESP_LOGD(TAG, "Queued face tracking SERVO_JOG seq=%lu axis_mask=0x%02x timeout_ms=%u", (unsigned long)seq,
                 request->axis_mask, (unsigned)request->timeout_ms);
    } else {
        ESP_LOGD(TAG, "Queued SERVO_JOG frame seq=%lu wire_len=%u axis_mask=0x%02x timeout_ms=%u", (unsigned long)seq,
                 (unsigned)wire_len, request->axis_mask, (unsigned)request->timeout_ms);
    }
#endif
    s_last_command_seq = seq;
    s_command_inflight = true;
    if (out_seq != NULL) {
        *out_seq = seq;
    }
    return ESP_OK;
}

static esp_err_t mcu_motion_submit_direct_target_frame(const mcu_motion_direct_target_t *request, uint32_t *out_seq) {
    uint8_t payload[6];
    mcu_link_t *link;
    uint32_t seq = 0u;
    size_t wire_len = 0u;
    esp_err_t ret;

    if (request == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    link = mcu_link_bootstrap_get_link();
    if (link == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    if (!mcu_link_bootstrap_is_ready()) {
        ESP_LOGW(TAG, "MCU link not fully ready; rejecting direct target request");
        return ESP_ERR_INVALID_STATE;
    }

    payload[0] = request->axis_mask;
    encode_i16_le(&payload[1], request->x_deg_x10);
    encode_i16_le(&payload[3], request->y_deg_x10);
    payload[5] = (uint8_t)request->source;

    ret = mcu_link_send_frame(link, MCU_FRAME_CLASS_MOTION, MCU_MOTION_MSG_SERVO_DIRECT_TARGET, 0u, payload,
                              (uint16_t)sizeof(payload), &seq, &wire_len);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to queue SERVO_DIRECT_TARGET frame: %s", esp_err_to_name(ret));
        return ret;
    }

#if !defined(WATCHER_STRESS_BUILD) && !defined(CONFIG_WATCHER_STRESS_BUILD)
    ESP_LOGD(TAG, "Queued SERVO_DIRECT_TARGET frame seq=%lu wire_len=%u axis_mask=0x%02x", (unsigned long)seq,
             (unsigned)wire_len, request->axis_mask);
#endif
    lifecycle_lock();
    memset(&s_face_jog_origin, 0, sizeof(s_face_jog_origin));
    lifecycle_unlock();
    if (out_seq != NULL) {
        *out_seq = seq;
    }
    return ESP_OK;
}

static esp_err_t mcu_motion_submit_sequence_frame(const mcu_motion_sequence_t *sequence, uint32_t *out_seq) {
    uint8_t payload[2u + (MCU_MOTION_SEQUENCE_MAX_SEGMENTS * 8u)];
    mcu_link_t *link;
    uint32_t seq = 0u;
    size_t wire_len = 0u;
    uint16_t payload_len;
    esp_err_t ret;

    if (sequence == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    link = mcu_link_bootstrap_get_link();
    if (link == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    if (!mcu_link_bootstrap_is_ready()) {
        ESP_LOGW(TAG, "MCU link not fully ready; rejecting motion sequence request");
        return ESP_ERR_INVALID_STATE;
    }

    payload[0] = (uint8_t)sequence->source;
    payload[1] = sequence->segment_count;
    for (uint8_t index = 0u; index < sequence->segment_count; index++) {
        uint16_t offset = (uint16_t)(2u + ((uint16_t)index * 8u));
        const mcu_motion_segment_t *segment = &sequence->segments[index];

        payload[offset] = segment->axis_mask;
        encode_i16_le(&payload[offset + 1u], segment->x_deg_x10);
        encode_i16_le(&payload[offset + 3u], segment->y_deg_x10);
        encode_u16_le(&payload[offset + 5u], segment->duration_ms);
        payload[offset + 7u] = segment->motion_profile;
    }
    payload_len = (uint16_t)(2u + ((uint16_t)sequence->segment_count * 8u));

    ret = mcu_link_send_frame(link, MCU_FRAME_CLASS_MOTION, MCU_MOTION_MSG_SERVO_SEQUENCE, MCU_FRAME_FLAG_ACK_REQ,
                              payload, payload_len, &seq, &wire_len);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to queue SERVO_SEQUENCE frame: %s", esp_err_to_name(ret));
        return ret;
    }

#if !defined(WATCHER_STRESS_BUILD) && !defined(CONFIG_WATCHER_STRESS_BUILD)
    ESP_LOGD(TAG, "Queued SERVO_SEQUENCE frame seq=%lu wire_len=%u segments=%u source=%u", (unsigned long)seq,
             (unsigned)wire_len, (unsigned)sequence->segment_count, (unsigned)sequence->source);
#endif
    remember_motion_sequence(seq);
    s_last_command_seq = seq;
    s_command_inflight = true;
    if (out_seq != NULL) {
        *out_seq = seq;
    }
    return ESP_OK;
}

static esp_err_t mcu_motion_send_chunked_sequence_control_frame(mcu_link_t *link, uint8_t msg_id,
                                                                const uint8_t *payload, uint16_t payload_len,
                                                                uint32_t *out_seq) {
    size_t wire_len = 0u;

    uint32_t seq = 0;
#if defined(ESP_PLATFORM)
    /* Keep the correlation entry visible before RX can dispatch an early NACK. */
    if (!mcu_link_uart_try_lock()) {
        return ESP_ERR_INVALID_STATE;
    }
#endif
    esp_err_t ret = mcu_link_send_frame(link, MCU_FRAME_CLASS_MOTION, msg_id, MCU_FRAME_FLAG_ACK_REQ, payload,
                                        payload_len, &seq, &wire_len);
    if (ret == ESP_OK) {
        remember_motion_sequence(seq);
        observed_frame(seq, msg_id, payload);
        if (out_seq != NULL)
            *out_seq = seq;
    } else {
        ESP_LOGW(TAG, "Sequence frame 0x%02x failed: %s", msg_id, esp_err_to_name(ret));
    }
#if defined(ESP_PLATFORM)
    mcu_link_uart_unlock();
#endif
    return ret;
}

static void mcu_motion_chunked_sequence_frame_gap(void) {
#if defined(MCU_MOTION_HOST_TEST)
    extern void mcu_motion_test_record_frame_gap(uint32_t delay_ms);
    mcu_motion_test_record_frame_gap(MCU_MOTION_CHUNKED_SEQUENCE_FRAME_GAP_MS);
#elif defined(ESP_PLATFORM)
    vTaskDelay(pdMS_TO_TICKS(MCU_MOTION_CHUNKED_SEQUENCE_FRAME_GAP_MS));
#endif
}

static esp_err_t mcu_motion_submit_chunked_sequence_frames(const mcu_motion_chunked_sequence_t *sequence,
                                                           uint32_t *out_end_seq,
                                                           mcu_motion_sequence_receipt_t *out_receipt) {
    uint8_t payload[4u + (MCU_MOTION_SEQUENCE_CHUNK_MAX_SEGMENTS * 8u)];
    mcu_link_t *link;
    uint8_t next_index = 0u;
    uint32_t end_seq = 0u;
    esp_err_t ret;

    if (sequence == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    link = mcu_link_bootstrap_get_link();
    if (link == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    if (!mcu_link_bootstrap_is_ready()) {
        ESP_LOGW(TAG, "MCU link not fully ready; rejecting chunked motion sequence request");
        return ESP_ERR_INVALID_STATE;
    }

    encode_u16_le(&payload[0], sequence->sequence_id);
    /* Legacy sequence BEGIN accepts 0..4; keep local FACE ownership intact. */
    payload[2] =
        sequence->source == MCU_MOTION_SOURCE_FACE_TRACKING ? MCU_MOTION_SOURCE_UNKNOWN : (uint8_t)sequence->source;
    payload[3] = sequence->segment_count;
    ret = mcu_motion_send_chunked_sequence_control_frame(link, MCU_MOTION_MSG_SERVO_SEQUENCE_BEGIN, payload, 4u,
                                                         out_receipt != NULL ? &out_receipt->begin_seq : NULL);
    if (ret != ESP_OK) {
        return ret;
    }
    mcu_motion_chunked_sequence_frame_gap();

    while (next_index < sequence->segment_count) {
        if (!mcu_motion_source_has_access(sequence->source)) {
            return ESP_ERR_INVALID_STATE;
        }
        uint8_t chunk_count = (uint8_t)(sequence->segment_count - next_index);
        uint16_t payload_len;

        if (chunk_count > MCU_MOTION_SEQUENCE_CHUNK_MAX_SEGMENTS) {
            chunk_count = MCU_MOTION_SEQUENCE_CHUNK_MAX_SEGMENTS;
        }

        encode_u16_le(&payload[0], sequence->sequence_id);
        payload[2] = next_index;
        payload[3] = chunk_count;
        for (uint8_t index = 0u; index < chunk_count; index++) {
            uint16_t offset = (uint16_t)(4u + ((uint16_t)index * 8u));
            const mcu_motion_segment_t *segment = &sequence->segments[next_index + index];

            payload[offset] = segment->axis_mask;
            encode_i16_le(&payload[offset + 1u], segment->x_deg_x10);
            encode_i16_le(&payload[offset + 3u], segment->y_deg_x10);
            encode_u16_le(&payload[offset + 5u], segment->duration_ms);
            payload[offset + 7u] = segment->motion_profile;
        }
        payload_len = (uint16_t)(4u + ((uint16_t)chunk_count * 8u));
        ret = mcu_motion_send_chunked_sequence_control_frame(link, MCU_MOTION_MSG_SERVO_SEQUENCE_CHUNK, payload,
                                                             payload_len,
                                                             out_receipt != NULL ? &out_receipt->chunk_seq : NULL);
        if (ret != ESP_OK) {
            return ret;
        }
        mcu_motion_chunked_sequence_frame_gap();
        next_index = (uint8_t)(next_index + chunk_count);
    }

    encode_u16_le(&payload[0], sequence->sequence_id);
    if (!mcu_motion_source_has_access(sequence->source)) {
        return ESP_ERR_INVALID_STATE;
    }
    ret =
        mcu_motion_send_chunked_sequence_control_frame(link, MCU_MOTION_MSG_SERVO_SEQUENCE_END, payload, 2u, &end_seq);
    if (ret != ESP_OK) {
        return ret;
    }

#if !defined(WATCHER_STRESS_BUILD) && !defined(CONFIG_WATCHER_STRESS_BUILD)
    ESP_LOGD(TAG, "Queued chunked SERVO_SEQUENCE seq=%lu id=%u segments=%u source=%u", (unsigned long)end_seq,
             (unsigned)sequence->sequence_id, (unsigned)sequence->segment_count, (unsigned)sequence->source);
#endif
    s_last_command_seq = out_receipt != NULL ? out_receipt->chunk_seq : end_seq;
    s_command_inflight = true;
    if (out_end_seq != NULL) {
        *out_end_seq = end_seq;
    }
    if (out_receipt != NULL) {
        out_receipt->end_seq = end_seq;
        out_receipt->segment_count = sequence->segment_count;
    }
    return ESP_OK;
}

static bool mcu_motion_request_is_valid(const mcu_motion_request_t *request) {
    if (request == NULL) {
        return false;
    }

    if (request->axis_mask == 0U) {
        return false;
    }

    if ((request->axis_mask & ~(MCU_MOTION_AXIS_X | MCU_MOTION_AXIS_Y)) != 0U) {
        return false;
    }

    if (request->duration_ms == 0U) {
        return false;
    }

    if (request->motion_profile != MCU_MOTION_PROFILE_LINEAR &&
        request->motion_profile != MCU_MOTION_PROFILE_EASE_IN_OUT) {
        return false;
    }

    if (request->source < MCU_MOTION_SOURCE_UNKNOWN || request->source > MCU_MOTION_SOURCE_FACE_TRACKING) {
        return false;
    }

    return true;
}

static bool mcu_motion_segment_is_valid(const mcu_motion_segment_t *segment) {
    if (segment == NULL) {
        return false;
    }
    if (segment->axis_mask == 0U || (segment->axis_mask & ~(MCU_MOTION_AXIS_X | MCU_MOTION_AXIS_Y)) != 0U) {
        return false;
    }
    if (segment->duration_ms == 0U) {
        return false;
    }
    if (segment->motion_profile != MCU_MOTION_PROFILE_LINEAR &&
        segment->motion_profile != MCU_MOTION_PROFILE_EASE_IN_OUT) {
        return false;
    }
    if ((segment->axis_mask & MCU_MOTION_AXIS_X) != 0U && (segment->x_deg_x10 < 0 || segment->x_deg_x10 > 1800)) {
        return false;
    }
    if ((segment->axis_mask & MCU_MOTION_AXIS_Y) != 0U && (segment->y_deg_x10 < 0 || segment->y_deg_x10 > 1800)) {
        return false;
    }
    return true;
}

static bool mcu_motion_sequence_is_valid(const mcu_motion_sequence_t *sequence) {
    if (sequence == NULL) {
        return false;
    }
    if (sequence->source < MCU_MOTION_SOURCE_UNKNOWN || sequence->source > MCU_MOTION_SOURCE_FACE_TRACKING) {
        return false;
    }
    if (sequence->segment_count == 0U || sequence->segment_count > MCU_MOTION_SEQUENCE_MAX_SEGMENTS) {
        return false;
    }
    for (uint8_t index = 0u; index < sequence->segment_count; index++) {
        if (!mcu_motion_segment_is_valid(&sequence->segments[index])) {
            return false;
        }
    }
    return true;
}

static bool mcu_motion_chunked_sequence_is_valid(const mcu_motion_chunked_sequence_t *sequence) {
    if (sequence == NULL) {
        return false;
    }
    if (sequence->source < MCU_MOTION_SOURCE_UNKNOWN || sequence->source > MCU_MOTION_SOURCE_FACE_TRACKING) {
        return false;
    }
    if (sequence->segment_count == 0U || sequence->segment_count > MCU_MOTION_CHUNKED_SEQUENCE_MAX_SEGMENTS) {
        return false;
    }
    for (uint8_t index = 0u; index < sequence->segment_count; index++) {
        if (!mcu_motion_segment_is_valid(&sequence->segments[index])) {
            return false;
        }
    }
    return true;
}

static bool mcu_motion_jog_request_is_valid(const mcu_motion_jog_request_t *request) {
    if (request == NULL) {
        return false;
    }
    if (request->axis_mask == 0U || (request->axis_mask & ~(MCU_MOTION_AXIS_X | MCU_MOTION_AXIS_Y)) != 0U) {
        return false;
    }
    if (request->timeout_ms == 0U) {
        return false;
    }
    if (request->x_min_deg > request->x_max_deg || request->y_min_deg > request->y_max_deg) {
        return false;
    }
    if ((request->axis_mask & MCU_MOTION_AXIS_X) != 0U && request->x_velocity_deg_x10_per_sec == 0) {
        return false;
    }
    if ((request->axis_mask & MCU_MOTION_AXIS_Y) != 0U && request->y_velocity_deg_x10_per_sec == 0) {
        return false;
    }
    if (request->source < MCU_MOTION_SOURCE_UNKNOWN || request->source > MCU_MOTION_SOURCE_FACE_TRACKING) {
        return false;
    }
    return true;
}

static bool mcu_motion_direct_target_is_valid(const mcu_motion_direct_target_t *request) {
    if (request == NULL) {
        return false;
    }
    if (request->axis_mask == 0U || (request->axis_mask & ~(MCU_MOTION_AXIS_X | MCU_MOTION_AXIS_Y)) != 0U) {
        return false;
    }
    if ((request->axis_mask & MCU_MOTION_AXIS_X) != 0U && (request->x_deg_x10 < 0 || request->x_deg_x10 > 1800)) {
        return false;
    }
    if ((request->axis_mask & MCU_MOTION_AXIS_Y) != 0U && (request->y_deg_x10 < 0 || request->y_deg_x10 > 1800)) {
        return false;
    }
    if (request->source < MCU_MOTION_SOURCE_UNKNOWN || request->source > MCU_MOTION_SOURCE_FACE_TRACKING) {
        return false;
    }
    return true;
}

esp_err_t mcu_motion_service_init(void) {
#if defined(ESP_PLATFORM)
    if (s_submission_mutex == NULL) {
        s_submission_mutex = xSemaphoreCreateMutexStatic(&s_submission_storage);
        if (s_submission_mutex == NULL) {
            return ESP_ERR_NO_MEM;
        }
    }
#endif
    memset(&s_last_request, 0, sizeof(s_last_request));
    s_has_last_request = false;
    memset(s_reply_history, 0, sizeof(s_reply_history));
    s_reply_count = 0;
    s_reply_next = 0;
    s_last_command_seq = 0u;
    s_command_inflight = false;
    s_lifecycle_cb = NULL;
    s_lifecycle_ctx = NULL;
    s_lifecycle_active_callbacks = 0U;
    memset(&s_stop_observer, 0, sizeof(s_stop_observer));
    memset(&s_face_jog_origin, 0, sizeof(s_face_jog_origin));
    s_stop_observing = false;
    return ESP_OK;
}

esp_err_t mcu_motion_submit(const mcu_motion_request_t *request) {
    return mcu_motion_submit_with_seq(request, NULL);
}

esp_err_t mcu_motion_jog(const mcu_motion_jog_request_t *request) {
    return mcu_motion_jog_with_seq(request, NULL);
}

esp_err_t mcu_motion_submit_direct_target(const mcu_motion_direct_target_t *request) {
    return mcu_motion_submit_direct_target_with_seq(request, NULL);
}

esp_err_t mcu_motion_submit_sequence(const mcu_motion_sequence_t *sequence) {
    return mcu_motion_submit_sequence_with_seq(sequence, NULL);
}

esp_err_t mcu_motion_submit_chunked_sequence(const mcu_motion_chunked_sequence_t *sequence) {
    return mcu_motion_submit_chunked_sequence_with_seq(sequence, NULL);
}

esp_err_t mcu_motion_pwm_unlock(const mcu_motion_pwm_unlock_request_t *request) {
    if (request == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (request->source < MCU_MOTION_SOURCE_UNKNOWN || request->source > MCU_MOTION_SOURCE_RECOVERY) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t guard = motion_submission_begin(request->source, true);
    if (guard != ESP_OK) {
        return guard;
    }
    esp_err_t ret = mcu_motion_submit_pwm_unlock_frame(request);
    motion_submission_unlock();
    return ret;
}

esp_err_t mcu_motion_pwm_lock(const mcu_motion_pwm_lock_request_t *request) {
    if (request == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (request->source < MCU_MOTION_SOURCE_UNKNOWN || request->source > MCU_MOTION_SOURCE_RECOVERY) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t guard = motion_submission_begin(request->source, true);
    if (guard != ESP_OK) {
        return guard;
    }
    esp_err_t ret = mcu_motion_submit_pwm_lock_frame(request);
    motion_submission_unlock();
    return ret;
}

esp_err_t mcu_motion_set_servo_feedback_callback(mcu_motion_servo_feedback_cb_t cb, void *ctx) {
    (void)ctx;
    return cb == NULL ? ESP_OK : ESP_ERR_NOT_SUPPORTED;
#if 0 /* Retired: current robot has no physical servo feedback. */
    s_servo_feedback_cb = cb;
    s_servo_feedback_ctx = ctx;
    return ESP_OK;
#endif
}

esp_err_t mcu_motion_request_feedback(void) {
    return ESP_ERR_NOT_SUPPORTED;
#if 0 /* Retired: current robot has no physical servo feedback. */
    mcu_link_t *link = mcu_link_bootstrap_get_link();
    if (link == NULL || !mcu_link_bootstrap_is_ready())
        return ESP_ERR_INVALID_STATE;
    return mcu_link_send_frame(link, MCU_FRAME_CLASS_MOTION, MCU_MOTION_MSG_SERVO_FEEDBACK_REQ, 0, NULL, 0, NULL, NULL);
#endif
}

esp_err_t mcu_motion_set_lifecycle_callback(mcu_motion_lifecycle_cb_t cb, void *ctx) {
    bool callback_active;

    lifecycle_lock();
    s_lifecycle_cb = NULL;
    s_lifecycle_ctx = NULL;
    lifecycle_unlock();
    do {
        lifecycle_lock();
        callback_active = s_lifecycle_active_callbacks > 0U;
        lifecycle_unlock();
#if defined(ESP_PLATFORM)
        if (callback_active) {
            vTaskDelay(pdMS_TO_TICKS(1));
        }
#endif
    } while (callback_active);
    lifecycle_lock();
    s_lifecycle_ctx = ctx;
    s_lifecycle_cb = cb;
    lifecycle_unlock();
    return ESP_OK;
}

esp_err_t mcu_motion_submit_with_seq(const mcu_motion_request_t *request, uint32_t *out_seq) {
    if (!mcu_motion_request_is_valid(request)) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t guard = motion_submission_begin(request->source, true);
    if (guard != ESP_OK) {
        return guard;
    }

    {
        esp_err_t ret = mcu_motion_submit_runtime_frame(request, out_seq);
        motion_submission_unlock();
        if (ret != ESP_OK) {
            return ret;
        }

        s_last_request = *request;
        s_has_last_request = true;
        return ESP_OK;
    }
}

esp_err_t mcu_motion_jog_with_seq(const mcu_motion_jog_request_t *request, uint32_t *out_seq) {
    if (!mcu_motion_jog_request_is_valid(request)) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t guard = motion_submission_begin(request->source, true);
    if (guard != ESP_OK) {
        return guard;
    }

    esp_err_t ret = mcu_motion_submit_jog_frame(request, out_seq);
    motion_submission_unlock();
    return ret;
}

esp_err_t mcu_motion_submit_direct_target_with_seq(const mcu_motion_direct_target_t *request, uint32_t *out_seq) {
    if (!mcu_motion_direct_target_is_valid(request)) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t guard = motion_submission_begin(request->source, true);
    if (guard != ESP_OK) {
        return guard;
    }

    esp_err_t ret = mcu_motion_submit_direct_target_frame(request, out_seq);
    motion_submission_unlock();
    return ret;
}

esp_err_t mcu_motion_submit_sequence_with_seq(const mcu_motion_sequence_t *sequence, uint32_t *out_seq) {
    if (!mcu_motion_sequence_is_valid(sequence)) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t guard = motion_submission_begin(sequence->source, true);
    if (guard != ESP_OK) {
        return guard;
    }

    esp_err_t ret = mcu_motion_submit_sequence_frame(sequence, out_seq);
    motion_submission_unlock();
    return ret;
}

esp_err_t mcu_motion_submit_chunked_sequence_with_seq(const mcu_motion_chunked_sequence_t *sequence,
                                                      uint32_t *out_end_seq) {
    if (!mcu_motion_chunked_sequence_is_valid(sequence)) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t guard = motion_submission_begin(sequence->source, true);
    if (guard != ESP_OK) {
        return guard;
    }

    esp_err_t ret = mcu_motion_submit_chunked_sequence_frames(sequence, out_end_seq, NULL);
    motion_submission_unlock();
    return ret;
}

esp_err_t mcu_motion_submit_observed_sequence(const mcu_motion_chunked_sequence_t *sequence, uint32_t *out_token) {
    if (!out_token)
        return ESP_ERR_INVALID_ARG;
    *out_token = 0;
    if (!mcu_motion_chunked_sequence_is_valid(sequence))
        return ESP_ERR_INVALID_ARG;
    esp_err_t ret = motion_submission_begin(sequence->source, true);
    if (ret != ESP_OK)
        return ret;
    lifecycle_lock();
    if (s_observed.token) {
        lifecycle_unlock();
        motion_submission_unlock();
        return ESP_ERR_INVALID_STATE;
    }
    memset(&s_observed, 0, sizeof(s_observed));
    if (++s_observed_generation == 0)
        ++s_observed_generation;
    s_observed.token = s_observed_generation;
    s_observed.origin_eligible = sequence->source == MCU_MOTION_SOURCE_FACE_TRACKING;
    s_observed_sending = true;
    lifecycle_unlock();
    ret = mcu_motion_submit_chunked_sequence_frames(sequence, NULL, NULL);
    lifecycle_lock();
    s_observed_sending = false;
    if (ret == ESP_OK) {
        s_observed.submitted = true;
        bool done = true;
        for (unsigned i = 0; i < s_observed.frame_count; ++i)
            if (s_observed.done[i] != s_observed.expected[i])
                done = false;
        if (done && s_observed.status == MCU_MOTION_COMPLETION_PENDING) {
            s_observed.status = MCU_MOTION_COMPLETION_COMPLETED;
            if (s_observed.origin_eligible) {
                s_face_jog_origin.x = s_observed.final_x;
                s_face_jog_origin.y = s_observed.final_y;
                s_face_jog_origin.valid = true;
            }
        }
        *out_token = s_observed.token;
    } else
        s_observed.token = 0;
    lifecycle_unlock();
    motion_submission_unlock();
    return ret;
}

esp_err_t mcu_motion_submit_tracked_sequence(const mcu_motion_chunked_sequence_t *sequence,
                                             mcu_motion_sequence_receipt_t *out_receipt) {
    if (out_receipt == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    *out_receipt = (mcu_motion_sequence_receipt_t){0};
    if (!mcu_motion_chunked_sequence_is_valid(sequence) ||
        sequence->segment_count > MCU_MOTION_SEQUENCE_CHUNK_MAX_SEGMENTS) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t guard = motion_submission_begin(sequence->source, true);
    if (guard != ESP_OK) {
        return guard;
    }
    esp_err_t ret = mcu_motion_submit_chunked_sequence_frames(sequence, NULL, out_receipt);
    motion_submission_unlock();
    return ret;
}

esp_err_t mcu_motion_service_get_last_request(mcu_motion_request_t *out_request) {
    if (out_request == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!s_has_last_request) {
        return ESP_ERR_NOT_FOUND;
    }

    *out_request = s_last_request;
    return ESP_OK;
}

esp_err_t mcu_motion_stop(mcu_motion_source_t source) {
    if (source < MCU_MOTION_SOURCE_UNKNOWN || source > MCU_MOTION_SOURCE_FACE_TRACKING) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t guard = motion_submission_begin(source, false);
    if (guard != ESP_OK) {
        return guard;
    }

    esp_err_t ret = mcu_motion_submit_stop_frame(source);
    motion_submission_unlock();
    return ret;
}

esp_err_t mcu_motion_submit_observed_stop(mcu_motion_source_t source, uint32_t *out_token) {
    if (!out_token || source < MCU_MOTION_SOURCE_UNKNOWN || source > MCU_MOTION_SOURCE_FACE_TRACKING)
        return ESP_ERR_INVALID_ARG;
    *out_token = 0;
    esp_err_t ret = motion_submission_begin(source, false);
    if (ret != ESP_OK)
        return ret;
    lifecycle_lock();
    if (s_stop_observer.token) {
        lifecycle_unlock();
        motion_submission_unlock();
        return ESP_ERR_INVALID_STATE;
    }
    if (++s_stop_generation == 0)
        ++s_stop_generation;
    s_stop_observer.token = s_stop_generation;
    s_stop_observer.ref = 0;
    s_stop_observer.ack_rx = 0;
    s_stop_observer.status = MCU_MOTION_COMPLETION_PENDING;
    s_stop_observing = true;
    lifecycle_unlock();
    ret = mcu_motion_submit_stop_frame(source);
    lifecycle_lock();
    s_stop_observing = false;
    if (ret == ESP_OK)
        *out_token = s_stop_observer.token;
    else
        s_stop_observer.token = 0;
    lifecycle_unlock();
    motion_submission_unlock();
    return ret;
}

esp_err_t mcu_motion_stop_status(uint32_t token, mcu_motion_completion_status_t *status, uint32_t *ack_rx_seq) {
    if (!status || !ack_rx_seq)
        return ESP_ERR_INVALID_ARG;
    lifecycle_lock();
    esp_err_t ret = token && token == s_stop_observer.token ? ESP_OK : ESP_ERR_NOT_FOUND;
    if (ret == ESP_OK) {
        *status = s_stop_observer.status;
        *ack_rx_seq = s_stop_observer.ack_rx;
    }
    lifecycle_unlock();
    return ret;
}

esp_err_t mcu_motion_stop_control_origin(uint32_t token, int16_t *x, int16_t *y) {
    if (!x || !y)
        return ESP_ERR_INVALID_ARG;
    lifecycle_lock();
    bool valid = token && token == s_stop_observer.token && s_stop_observer.status == MCU_MOTION_COMPLETION_COMPLETED &&
                 s_face_jog_origin.valid;
    if (valid) {
        *x = s_face_jog_origin.x;
        *y = s_face_jog_origin.y;
    }
    lifecycle_unlock();
    return valid ? ESP_OK : ESP_ERR_NOT_FOUND;
}

void mcu_motion_stop_release(uint32_t token) {
    lifecycle_lock();
    if (token && token == s_stop_observer.token)
        s_stop_observer.token = 0;
    lifecycle_unlock();
}

esp_err_t mcu_motion_service_handle_link_event(const mcu_link_event_t *event) {
    uint32_t ref_seq;

    if (event == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    switch (event->type) {
    case MCU_LINK_RX_EVENT_HELLO_RSP:
        /* A new MCU baseline invalidates all old position evidence. */
        lifecycle_lock();
        s_observed.origin_eligible = false;
        memset(&s_face_jog_origin, 0, sizeof(s_face_jog_origin));
        if (s_stop_observer.token)
            s_stop_observer.status = MCU_MOTION_COMPLETION_FAILED;
        if (s_observed.token)
            s_observed.status = MCU_MOTION_COMPLETION_FAILED;
        lifecycle_unlock();
        return ESP_OK;
    case MCU_LINK_RX_EVENT_ACK: {
        mcu_motion_lifecycle_event_t lifecycle = {.type = MCU_MOTION_LIFECYCLE_ACKED};
        if (event->frame.header.payload_len < 6u) {
            return ESP_ERR_INVALID_SIZE;
        }
        ref_seq = decode_u32_le(event->frame.payload);
        if (!is_motion_sequence(ref_seq))
            return ESP_OK;
        lifecycle.ref_seq = ref_seq;
        lifecycle.event_seq = event->frame.header.seq;
        lifecycle.status = decode_u16_le(&event->frame.payload[4]);
        publish_lifecycle_event(&lifecycle);
        if (s_command_inflight && ref_seq == s_last_command_seq) {
#if !defined(WATCHER_STRESS_BUILD) && !defined(CONFIG_WATCHER_STRESS_BUILD)
            ESP_LOGD(TAG, "Motion ACK ref_seq=%lu status=%u", (unsigned long)ref_seq,
                     (unsigned)decode_u16_le(&event->frame.payload[4]));
#endif
        }
        return ESP_OK;
    }
    case MCU_LINK_RX_EVENT_NACK: {
        mcu_motion_lifecycle_event_t lifecycle = {.type = MCU_MOTION_LIFECYCLE_REJECTED};
        if (event->frame.header.payload_len < 8u) {
            return ESP_ERR_INVALID_SIZE;
        }
        ref_seq = decode_u32_le(event->frame.payload);
        if (!is_motion_sequence(ref_seq))
            return ESP_OK;
        lifecycle.ref_seq = ref_seq;
        lifecycle.reason = decode_u16_le(&event->frame.payload[6]);
        lifecycle.event_seq = event->frame.header.seq;
        publish_lifecycle_event(&lifecycle);
        if (s_command_inflight && ref_seq == s_last_command_seq) {
            ESP_LOGW(TAG, "Motion NACK ref_seq=%lu reason=0x%04x", (unsigned long)ref_seq,
                     (unsigned)decode_u16_le(&event->frame.payload[6]));
            s_command_inflight = false;
        }
        return ESP_OK;
    }
    case MCU_LINK_RX_EVENT_FAULT: {
        mcu_motion_lifecycle_event_t lifecycle = {.type = MCU_MOTION_LIFECYCLE_FAULT};
        if (event->frame.header.payload_len < 9u) {
            return ESP_ERR_INVALID_SIZE;
        }
        ref_seq = decode_u32_le(event->frame.payload);
        lifecycle.ref_seq = ref_seq;
        lifecycle.fault_code = decode_u16_le(&event->frame.payload[5]);
        lifecycle.event_seq = event->frame.header.seq;
        lifecycle.detail = decode_u16_le(&event->frame.payload[7]);
        if (event->frame.payload[4] == 0x01u) {
            publish_lifecycle_event(&lifecycle);
        }
        if (event->frame.payload[4] == 0x01u &&
            (!s_command_inflight || ref_seq == s_last_command_seq || ref_seq == 0u)) {
            ESP_LOGW(TAG, "Motion FAULT ref_seq=%lu fault_code=0x%04x detail=0x%04x", (unsigned long)ref_seq,
                     (unsigned)decode_u16_le(&event->frame.payload[5]),
                     (unsigned)decode_u16_le(&event->frame.payload[7]));
            s_command_inflight = false;
        }
        return ESP_OK;
    }
    case MCU_LINK_RX_EVENT_MOTION_DONE: {
        mcu_motion_lifecycle_event_t lifecycle = {.type = MCU_MOTION_LIFECYCLE_DONE};
        if (event->frame.header.payload_len < 11u) {
            return ESP_ERR_INVALID_SIZE;
        }
        ref_seq = decode_u32_le(event->frame.payload);
        lifecycle.ref_seq = ref_seq;
        lifecycle.result = (mcu_motion_result_t)event->frame.payload[4];
        lifecycle.event_seq = event->frame.header.seq;
        lifecycle.final_x_deg_x10 = decode_i16_le(&event->frame.payload[5]);
        lifecycle.final_y_deg_x10 = decode_i16_le(&event->frame.payload[7]);
        lifecycle.exec_ms = decode_u16_le(&event->frame.payload[9]);
        publish_lifecycle_event(&lifecycle);
        if (!s_command_inflight || ref_seq == s_last_command_seq) {
#if !defined(WATCHER_STRESS_BUILD) && !defined(CONFIG_WATCHER_STRESS_BUILD)
            ESP_LOGD(TAG, "Motion DONE ref_seq=%lu result=%u final=(%d,%d) exec_ms=%u", (unsigned long)ref_seq,
                     (unsigned)event->frame.payload[4], (int)decode_i16_le(&event->frame.payload[5]),
                     (int)decode_i16_le(&event->frame.payload[7]), (unsigned)decode_u16_le(&event->frame.payload[9]));
            ESP_LOGI(OBS_TAG,
                     "evt=motion_done ref_seq=%lu msg_class=%u msg_id=%u result=%u final_x=%d final_y=%d "
                     "exec_ms=%u",
                     (unsigned long)ref_seq, (unsigned)MCU_FRAME_CLASS_MOTION, (unsigned)MCU_MOTION_MSG_MOTION_DONE,
                     (unsigned)event->frame.payload[4], (int)decode_i16_le(&event->frame.payload[5]),
                     (int)decode_i16_le(&event->frame.payload[7]), (unsigned)decode_u16_le(&event->frame.payload[9]));
#endif
            s_command_inflight = false;
        }
        return ESP_OK;
    }
    case MCU_LINK_RX_EVENT_SERVO_FEEDBACK:
        /* Legacy frames are not measurements on the current open-loop hardware. */
        return ESP_OK;
    default:
        return ESP_ERR_NOT_FOUND;
    }
}
