#include "mcu_power_service.h"

#include "esp_log.h"
#include "mcu_link_bootstrap.h"

#include <stdbool.h>
#include <string.h>

static const char *TAG = "MCU_POWER";
static const char *OBS_TAG = "MCU_OBS";

static uint32_t s_last_command_seq;
static bool s_command_inflight;
static bool s_last_enabled;
static mcu_power_source_t s_last_source;

static uint16_t decode_u16_le(const uint8_t *src) {
    return (uint16_t)(((uint16_t)src[0]) | ((uint16_t)src[1] << 8u));
}

static uint32_t decode_u32_le(const uint8_t *src) {
    return ((uint32_t)src[0]) | ((uint32_t)src[1] << 8u) | ((uint32_t)src[2] << 16u) | ((uint32_t)src[3] << 24u);
}

esp_err_t mcu_power_service_init(void) {
    s_last_command_seq = 0u;
    s_command_inflight = false;
    s_last_enabled = false;
    s_last_source = MCU_POWER_SOURCE_UNKNOWN;
    return ESP_OK;
}

esp_err_t mcu_power_set_5v_enabled(bool enabled, mcu_power_source_t source, uint32_t *out_seq) {
    uint8_t payload[1];
    mcu_link_t *link;
    uint32_t seq = 0u;
    size_t wire_len = 0u;
    esp_err_t ret;

    if (source < MCU_POWER_SOURCE_UNKNOWN || source > MCU_POWER_SOURCE_RECOVERY) {
        return ESP_ERR_INVALID_ARG;
    }

    link = mcu_link_bootstrap_get_link();
    if (link == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    if (!mcu_link_bootstrap_is_ready() && enabled) {
        ESP_LOGW(TAG, "MCU link not fully ready; rejecting power-on request");
        return ESP_ERR_INVALID_STATE;
    }

    if (!mcu_link_bootstrap_is_ready()) {
        ESP_LOGW(TAG, "MCU link not fully ready; sending power-off request anyway (state=%d)",
                 (int)mcu_link_bootstrap_get_state());
    }

    payload[0] = (uint8_t)source;
    ret = mcu_link_send_frame(link, MCU_FRAME_CLASS_POWER, enabled ? MCU_POWER_MSG_5V_ENABLE : MCU_POWER_MSG_5V_DISABLE,
                              MCU_FRAME_FLAG_ACK_REQ, payload, (uint16_t)sizeof(payload), &seq, &wire_len);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to queue POWER_5V_%s frame: %s", enabled ? "ENABLE" : "DISABLE", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "Queued POWER_5V_%s frame seq=%lu wire_len=%u source=%u", enabled ? "ENABLE" : "DISABLE",
             (unsigned long)seq, (unsigned)wire_len, (unsigned)source);
    ESP_LOGI(OBS_TAG, "evt=power_5v_request seq=%lu msg_class=%u msg_id=%u enabled=%u source=%u", (unsigned long)seq,
             (unsigned)MCU_FRAME_CLASS_POWER, (unsigned)(enabled ? MCU_POWER_MSG_5V_ENABLE : MCU_POWER_MSG_5V_DISABLE),
             enabled ? 1u : 0u, (unsigned)source);
    s_last_command_seq = seq;
    s_command_inflight = true;
    s_last_enabled = enabled;
    s_last_source = source;
    if (out_seq != NULL) {
        *out_seq = seq;
    }
    return ESP_OK;
}

esp_err_t mcu_power_service_handle_link_event(const mcu_link_event_t *event) {
    uint32_t ref_seq;

    if (event == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    switch (event->type) {
    case MCU_LINK_RX_EVENT_ACK:
        ref_seq = decode_u32_le(event->frame.payload);
        if (s_command_inflight && ref_seq == s_last_command_seq) {
            ESP_LOGI(TAG, "POWER ACK ref_seq=%lu status=%u enabled=%u source=%u", (unsigned long)ref_seq,
                     (unsigned)decode_u16_le(&event->frame.payload[4]), s_last_enabled ? 1u : 0u,
                     (unsigned)s_last_source);
            s_command_inflight = false;
        }
        return ESP_OK;
    case MCU_LINK_RX_EVENT_NACK:
        ref_seq = decode_u32_le(event->frame.payload);
        if (s_command_inflight && ref_seq == s_last_command_seq) {
            ESP_LOGW(TAG, "POWER NACK ref_seq=%lu reason=0x%04x", (unsigned long)ref_seq,
                     (unsigned)decode_u16_le(&event->frame.payload[6]));
            s_command_inflight = false;
        }
        return ESP_OK;
    case MCU_LINK_RX_EVENT_FAULT:
        ref_seq = decode_u32_le(event->frame.payload);
        if (event->frame.payload[4] == 0x07u &&
            (!s_command_inflight || ref_seq == s_last_command_seq || ref_seq == 0u)) {
            ESP_LOGW(TAG, "POWER FAULT ref_seq=%lu fault_code=0x%04x detail=0x%04x", (unsigned long)ref_seq,
                     (unsigned)decode_u16_le(&event->frame.payload[5]),
                     (unsigned)decode_u16_le(&event->frame.payload[7]));
            s_command_inflight = false;
        }
        return ESP_OK;
    default:
        return ESP_ERR_NOT_FOUND;
    }
}
