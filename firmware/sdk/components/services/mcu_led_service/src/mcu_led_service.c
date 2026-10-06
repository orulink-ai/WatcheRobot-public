#include "mcu_led_service.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "mcu_link_bootstrap.h"

#include <stdbool.h>
#include <string.h>

static const char *TAG = "MCU_LED";
static const char *OBS_TAG = "MCU_OBS";

#define MCU_LED_BOOT_GREEN_RED 0u
#define MCU_LED_BOOT_GREEN_GREEN 255u
#define MCU_LED_BOOT_GREEN_BLUE 0u
#define MCU_LED_BOOT_GREEN_BRIGHTNESS 255u
#define MCU_LED_BOOT_GREEN_HOLD_MS 0u
#define MCU_LED_SIDE_ACTIVE_COUNT 4u

static mcu_led_request_t s_last_request;
static bool s_has_last_request;
static uint32_t s_last_command_seq;
static bool s_command_inflight;
static bool s_shutdown_mode;
static StaticSemaphore_t s_submit_lock_storage;
static SemaphoreHandle_t s_submit_lock;

static bool mcu_led_submit_lock(void) {
    return s_submit_lock != NULL && xSemaphoreTake(s_submit_lock, portMAX_DELAY) == pdTRUE;
}

static void mcu_led_submit_unlock(void) {
    (void)xSemaphoreGive(s_submit_lock);
}

static uint16_t decode_u16_le(const uint8_t *src) {
    return (uint16_t)(((uint16_t)src[0]) | ((uint16_t)src[1] << 8u));
}

static uint32_t decode_u32_le(const uint8_t *src) {
    return ((uint32_t)src[0]) | ((uint32_t)src[1] << 8u) | ((uint32_t)src[2] << 16u) | ((uint32_t)src[3] << 24u);
}

static uint8_t apply_brightness(uint8_t component, uint8_t brightness) {
    return (uint8_t)(((uint16_t)component * (uint16_t)brightness + 127u) / 255u);
}

static bool mcu_led_request_is_valid(const mcu_led_request_t *request) {
    if (request == NULL) {
        return false;
    }

    if (request->mode > MCU_LED_MODE_EFFECT) {
        return false;
    }

    if (request->zone > MCU_LED_ZONE_BOTTOM) {
        return false;
    }

    if (request->mode == MCU_LED_MODE_EFFECT) {
        if (request->effect_id < MCU_LED_EFFECT_BLINK || request->effect_id > MCU_LED_EFFECT_STATUS_PULSE) {
            return false;
        }

        if (request->period_ms == 0U) {
            return false;
        }
    }

    return true;
}

static esp_err_t mcu_led_submit_runtime_frame(const mcu_led_request_t *request) {
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
        if (request->mode != MCU_LED_MODE_OFF) {
            ESP_LOGW(TAG, "MCU link not ready for LED request mode=%u", (unsigned)request->mode);
            return ESP_ERR_INVALID_STATE;
        }
        if (mcu_link_bootstrap_is_link_ready()) {
            ESP_LOGW(TAG, "MCU baseline not synchronized; allowing safety LED-off request after handshake");
        } else {
            ESP_LOGW(TAG, "MCU handshake incomplete; attempting safety LED-off request anyway");
        }
    }

    switch (request->mode) {
    case MCU_LED_MODE_OFF: {
        uint8_t payload[1];

        payload[0] = (uint8_t)request->zone;
        ret = mcu_link_send_frame(link, MCU_FRAME_CLASS_LED, MCU_LED_MSG_OFF, MCU_FRAME_FLAG_ACK_REQ, payload,
                                  (uint16_t)sizeof(payload), &seq, &wire_len);
        break;
    }
    case MCU_LED_MODE_STATIC: {
        uint8_t payload[5];

        payload[0] = apply_brightness(request->primary_red, request->brightness);
        payload[1] = apply_brightness(request->primary_green, request->brightness);
        payload[2] = apply_brightness(request->primary_blue, request->brightness);
        payload[3] = MCU_LED_SIDE_ACTIVE_COUNT;
        payload[4] = (uint8_t)request->zone;
        ret = mcu_link_send_frame(link, MCU_FRAME_CLASS_LED, MCU_LED_MSG_SET_STATIC, MCU_FRAME_FLAG_ACK_REQ, payload,
                                  (uint16_t)sizeof(payload), &seq, &wire_len);
        break;
    }
    case MCU_LED_MODE_EFFECT: {
        uint8_t payload[7];

        payload[0] = (uint8_t)request->effect_id;
        payload[1] = apply_brightness(request->primary_red, request->brightness);
        payload[2] = apply_brightness(request->primary_green, request->brightness);
        payload[3] = apply_brightness(request->primary_blue, request->brightness);
        payload[4] = (uint8_t)(request->period_ms & 0xffu);
        payload[5] = (uint8_t)(request->period_ms >> 8u);
        payload[6] = (uint8_t)request->zone;
        ret = mcu_link_send_frame(link, MCU_FRAME_CLASS_LED, MCU_LED_MSG_SET_EFFECT, MCU_FRAME_FLAG_ACK_REQ, payload,
                                  (uint16_t)sizeof(payload), &seq, &wire_len);
        break;
    }
    default:
        return ESP_ERR_INVALID_STATE;
    }

    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to queue LED frame: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "Queued LED frame seq=%lu wire_len=%u mode=%u brightness=%u effect=%u", (unsigned long)seq,
             (unsigned)wire_len, (unsigned)request->mode, (unsigned)request->brightness, (unsigned)request->effect_id);
    s_last_command_seq = seq;
    s_command_inflight = true;
    return ESP_OK;
}

esp_err_t mcu_led_service_init(void) {
    if (s_submit_lock == NULL) {
        s_submit_lock = xSemaphoreCreateMutexStatic(&s_submit_lock_storage);
        if (s_submit_lock == NULL) {
            return ESP_ERR_NO_MEM;
        }
    }
    if (!mcu_led_submit_lock()) {
        return ESP_ERR_INVALID_STATE;
    }

    memset(&s_last_request, 0, sizeof(s_last_request));
    s_has_last_request = false;
    s_last_command_seq = 0u;
    s_command_inflight = false;
    s_shutdown_mode = false;
    mcu_led_submit_unlock();
    return ESP_OK;
}

esp_err_t mcu_led_service_enter_shutdown_mode(void) {
    if (!mcu_led_submit_lock()) {
        return ESP_ERR_INVALID_STATE;
    }

    s_shutdown_mode = true;
    mcu_led_submit_unlock();
    return ESP_OK;
}

esp_err_t mcu_led_submit_boot_green_baseline(void) {
    const mcu_led_request_t request = {
        .mode = MCU_LED_MODE_STATIC,
        .zone = MCU_LED_ZONE_ALL,
        .primary_red = MCU_LED_BOOT_GREEN_RED,
        .primary_green = MCU_LED_BOOT_GREEN_GREEN,
        .primary_blue = MCU_LED_BOOT_GREEN_BLUE,
        .brightness = MCU_LED_BOOT_GREEN_BRIGHTNESS,
        .period_ms = MCU_LED_BOOT_GREEN_HOLD_MS,
    };

    return mcu_led_submit(&request);
}

esp_err_t mcu_led_submit(const mcu_led_request_t *request) {
    esp_err_t ret;

    if (!mcu_led_request_is_valid(request)) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!mcu_led_submit_lock()) {
        return ESP_ERR_INVALID_STATE;
    }
    if (s_shutdown_mode && request->mode != MCU_LED_MODE_OFF) {
        ESP_LOGW(TAG, "Rejecting LED relight request during shutdown mode=%u", (unsigned)request->mode);
        mcu_led_submit_unlock();
        return ESP_ERR_INVALID_STATE;
    }

    ret = mcu_led_submit_runtime_frame(request);
    if (ret == ESP_OK) {
        s_last_request = *request;
        s_has_last_request = true;
    }
    mcu_led_submit_unlock();

    return ret;
}

esp_err_t mcu_led_service_get_last_request(mcu_led_request_t *out_request) {
    if (out_request == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!mcu_led_submit_lock()) {
        return ESP_ERR_INVALID_STATE;
    }
    if (!s_has_last_request) {
        mcu_led_submit_unlock();
        return ESP_ERR_NOT_FOUND;
    }

    *out_request = s_last_request;
    mcu_led_submit_unlock();
    return ESP_OK;
}

esp_err_t mcu_led_service_handle_link_event(const mcu_link_event_t *event) {
    uint32_t ref_seq;
    esp_err_t ret = ESP_OK;

    if (event == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!mcu_led_submit_lock()) {
        return ESP_ERR_INVALID_STATE;
    }

    switch (event->type) {
    case MCU_LINK_RX_EVENT_ACK:
        ref_seq = decode_u32_le(event->frame.payload);
        if (s_command_inflight && ref_seq == s_last_command_seq) {
            ESP_LOGI(TAG, "LED ACK ref_seq=%lu status=%u", (unsigned long)ref_seq,
                     (unsigned)decode_u16_le(&event->frame.payload[4]));
        }
        break;
    case MCU_LINK_RX_EVENT_NACK:
        ref_seq = decode_u32_le(event->frame.payload);
        if (s_command_inflight && ref_seq == s_last_command_seq) {
            ESP_LOGW(TAG, "LED NACK ref_seq=%lu reason=0x%04x", (unsigned long)ref_seq,
                     (unsigned)decode_u16_le(&event->frame.payload[6]));
            s_command_inflight = false;
        }
        break;
    case MCU_LINK_RX_EVENT_FAULT:
        ref_seq = decode_u32_le(event->frame.payload);
        if (event->frame.payload[4] == 0x02u &&
            (!s_command_inflight || ref_seq == s_last_command_seq || ref_seq == 0u)) {
            ESP_LOGW(TAG, "LED FAULT ref_seq=%lu fault_code=0x%04x detail=0x%04x", (unsigned long)ref_seq,
                     (unsigned)decode_u16_le(&event->frame.payload[5]),
                     (unsigned)decode_u16_le(&event->frame.payload[7]));
            s_command_inflight = false;
        }
        break;
    case MCU_LINK_RX_EVENT_LED_DONE:
        ref_seq = decode_u32_le(event->frame.payload);
        if (!s_command_inflight || ref_seq == s_last_command_seq) {
            ESP_LOGI(TAG, "LED DONE ref_seq=%lu result=%u", (unsigned long)ref_seq, (unsigned)event->frame.payload[4]);
            ESP_LOGI(OBS_TAG, "evt=led_done ref_seq=%lu msg_class=%u msg_id=%u result=%u", (unsigned long)ref_seq,
                     (unsigned)MCU_FRAME_CLASS_LED, (unsigned)MCU_LED_MSG_DONE, (unsigned)event->frame.payload[4]);
            s_command_inflight = false;
        }
        break;
    default:
        ret = ESP_ERR_NOT_FOUND;
        break;
    }

    mcu_led_submit_unlock();
    return ret;
}

