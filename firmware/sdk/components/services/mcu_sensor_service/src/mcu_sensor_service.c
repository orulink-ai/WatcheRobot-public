#include "mcu_sensor_service.h"

#include "esp_log.h"

#include <string.h>

#if defined(WATCHER_STRESS_BUILD) || defined(CONFIG_WATCHER_STRESS_BUILD)
#define MCU_SENSOR_STRESS_LOGGING_DISABLED 1
#else
#define MCU_SENSOR_STRESS_LOGGING_DISABLED 0
#endif

static const char *TAG = "MCU_SENSOR";
static const char *OBS_TAG = "MCU_OBS";

typedef struct {
    bool touch_valid;
    bool mag_valid;
    bool imu_valid;
    mcu_touch_state_t touch;
    mcu_mag_state_t mag;
    mcu_imu_state_t imu;
    mcu_sensor_service_stats_t stats;
} mcu_sensor_cache_t;

static mcu_sensor_cache_t s_cache;

static uint16_t decode_u16_le(const uint8_t *src) {
    return (uint16_t)(((uint16_t)src[0]) | ((uint16_t)src[1] << 8u));
}

static uint32_t decode_u32_le(const uint8_t *src) {
    return ((uint32_t)src[0]) | ((uint32_t)src[1] << 8u) | ((uint32_t)src[2] << 16u) | ((uint32_t)src[3] << 24u);
}

static int16_t decode_i16_le(const uint8_t *src) {
    return (int16_t)decode_u16_le(src);
}

esp_err_t mcu_sensor_service_init(void) {
    memset(&s_cache, 0, sizeof(s_cache));
    return ESP_OK;
}

static esp_err_t mcu_sensor_service_apply_touch_impl(const mcu_touch_state_t *state) {
    if (state == NULL) {
        s_cache.stats.touch_invalid_count++;
        return ESP_ERR_INVALID_ARG;
    }

    if (s_cache.touch_valid) {
        s_cache.stats.touch_overwrite_count++;
    }

    s_cache.touch = *state;
    s_cache.touch_valid = true;
    s_cache.stats.touch_apply_count++;
    return ESP_OK;
}

static esp_err_t mcu_sensor_service_apply_mag_impl(const mcu_mag_state_t *state) {
    if (state == NULL) {
        s_cache.stats.mag_invalid_count++;
        return ESP_ERR_INVALID_ARG;
    }

    if (s_cache.mag_valid) {
        s_cache.stats.mag_overwrite_count++;
    }

    s_cache.mag = *state;
    s_cache.mag_valid = true;
    s_cache.stats.mag_apply_count++;
    return ESP_OK;
}

static esp_err_t mcu_sensor_service_apply_imu_impl(const mcu_imu_state_t *state) {
    if (state == NULL) {
        s_cache.stats.imu_invalid_count++;
        return ESP_ERR_INVALID_ARG;
    }

    if (s_cache.imu_valid) {
        s_cache.stats.imu_overwrite_count++;
    }

    s_cache.imu = *state;
    s_cache.imu_valid = true;
    s_cache.stats.imu_apply_count++;
    return ESP_OK;
}

esp_err_t mcu_sensor_service_apply_touch(const mcu_touch_state_t *state) {
    return mcu_sensor_service_apply_touch_impl(state);
}

esp_err_t mcu_sensor_service_apply_mag(const mcu_mag_state_t *state) {
    return mcu_sensor_service_apply_mag_impl(state);
}

esp_err_t mcu_sensor_service_apply_imu(const mcu_imu_state_t *state) {
    return mcu_sensor_service_apply_imu_impl(state);
}

esp_err_t mcu_sensor_service_apply_frame(const mcu_sensor_frame_t *frame) {
    if (frame == NULL) {
        s_cache.stats.frame_invalid_count++;
        return ESP_ERR_INVALID_ARG;
    }

    s_cache.stats.frame_apply_count++;

    switch (frame->type) {
    case MCU_SENSOR_FRAME_TOUCH:
        return mcu_sensor_service_apply_touch_impl(&frame->data.touch);
    case MCU_SENSOR_FRAME_MAG:
        return mcu_sensor_service_apply_mag_impl(&frame->data.mag);
    case MCU_SENSOR_FRAME_IMU:
        return mcu_sensor_service_apply_imu_impl(&frame->data.imu);
    default:
        s_cache.stats.frame_invalid_count++;
        return ESP_ERR_INVALID_ARG;
    }
}

esp_err_t mcu_sensor_service_update_touch(const mcu_touch_state_t *state) {
    return mcu_sensor_service_apply_touch(state);
}

esp_err_t mcu_sensor_service_update_mag(const mcu_mag_state_t *state) {
    return mcu_sensor_service_apply_mag(state);
}

esp_err_t mcu_sensor_service_update_imu(const mcu_imu_state_t *state) {
    return mcu_sensor_service_apply_imu(state);
}

esp_err_t mcu_sensor_service_get_latest_touch(mcu_touch_state_t *out_state) {
    if (out_state == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    *out_state = s_cache.touch;
    return ESP_OK;
}

esp_err_t mcu_sensor_service_get_latest_mag(mcu_mag_state_t *out_state) {
    if (out_state == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    *out_state = s_cache.mag;
    return ESP_OK;
}

esp_err_t mcu_sensor_service_get_latest_imu(mcu_imu_state_t *out_state) {
    if (out_state == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    *out_state = s_cache.imu;
    return ESP_OK;
}

bool mcu_sensor_service_has_latest_touch(void) {
    return s_cache.touch_valid;
}

bool mcu_sensor_service_has_latest_mag(void) {
    return s_cache.mag_valid;
}

bool mcu_sensor_service_has_latest_imu(void) {
    return s_cache.imu_valid;
}

esp_err_t mcu_sensor_service_get_stats(mcu_sensor_service_stats_t *out_stats) {
    if (out_stats == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    *out_stats = s_cache.stats;
    return ESP_OK;
}

esp_err_t mcu_sensor_service_handle_link_event(const mcu_link_event_t *event, bool *out_overwrote_latest_state) {
    esp_err_t ret;

    if (out_overwrote_latest_state != NULL) {
        *out_overwrote_latest_state = false;
    }

    if (event == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    switch (event->type) {
    case MCU_LINK_RX_EVENT_TOUCH_EVENT: {
        mcu_touch_state_t state = {
            .touch_id = event->frame.payload[0],
            .event_code = (mcu_touch_event_code_t)event->frame.payload[1],
            .active = event->frame.payload[1] != MCU_TOUCH_EVENT_RELEASE,
            .timestamp_ms = decode_u32_le(&event->frame.payload[2]),
        };

        if (out_overwrote_latest_state != NULL && s_cache.touch_valid) {
            *out_overwrote_latest_state = true;
        }
        ret = mcu_sensor_service_apply_touch_impl(&state);
        if (ret == ESP_OK) {
            if (MCU_SENSOR_STRESS_LOGGING_DISABLED == 0) {
                ESP_LOGI(TAG, "Touch EVENT id=%u code=%u ts=%lu active=%d", (unsigned)state.touch_id,
                         (unsigned)state.event_code, (unsigned long)state.timestamp_ms, state.active ? 1 : 0);
                ESP_LOGI(
                    OBS_TAG, "evt=touch_event msg_class=%u msg_id=%u touch_id=%u code=%u timestamp_ms=%lu active=%d",
                    (unsigned)MCU_FRAME_CLASS_SENSOR, (unsigned)MCU_SENSOR_MSG_TOUCH_EVENT, (unsigned)state.touch_id,
                    (unsigned)state.event_code, (unsigned long)state.timestamp_ms, state.active ? 1 : 0);
            }
        }
        return ret;
    }
    case MCU_LINK_RX_EVENT_MAG_STATE: {
        mcu_mag_state_t state = {
            .heading_deg_x100 = decode_u16_le(&event->frame.payload[0]),
            .field_norm_uT = decode_u16_le(&event->frame.payload[2]),
            .quality = event->frame.payload[4],
            .status_bits = event->frame.payload[5],
            .timestamp_ms = 0u,
        };

        if (out_overwrote_latest_state != NULL && s_cache.mag_valid) {
            *out_overwrote_latest_state = true;
        }
        ret = mcu_sensor_service_apply_mag_impl(&state);
        if (ret == ESP_OK) {
            if (MCU_SENSOR_STRESS_LOGGING_DISABLED == 0) {
                ESP_LOGD(TAG, "MAG STATE heading=%u field=%u quality=%u status=0x%02x",
                         (unsigned)state.heading_deg_x100, (unsigned)state.field_norm_uT, (unsigned)state.quality,
                         (unsigned)state.status_bits);
                ESP_LOGI(OBS_TAG,
                         "evt=mag_state msg_class=%u msg_id=%u heading_deg_x100=%u field_norm_uT=%u quality=%u "
                         "status=0x%02x",
                         (unsigned)MCU_FRAME_CLASS_SENSOR, (unsigned)MCU_SENSOR_MSG_MAG_STATE,
                         (unsigned)state.heading_deg_x100, (unsigned)state.field_norm_uT, (unsigned)state.quality,
                         (unsigned)state.status_bits);
            }
        }
        return ret;
    }
    case MCU_LINK_RX_EVENT_IMU_STATE: {
        mcu_imu_state_t state = {
            .roll_deg_x100 = decode_i16_le(&event->frame.payload[0]),
            .pitch_deg_x100 = decode_i16_le(&event->frame.payload[2]),
            .yaw_deg_x100 = decode_i16_le(&event->frame.payload[4]),
            .acc_norm_mg = decode_u16_le(&event->frame.payload[6]),
            .gyro_norm_dps_x10 = decode_u16_le(&event->frame.payload[8]),
            .motion_flags = event->frame.payload[10],
            .timestamp_ms = 0u,
        };

        if (out_overwrote_latest_state != NULL && s_cache.imu_valid) {
            *out_overwrote_latest_state = true;
        }
        ret = mcu_sensor_service_apply_imu_impl(&state);
        if (ret == ESP_OK) {
            if (MCU_SENSOR_STRESS_LOGGING_DISABLED == 0) {
                ESP_LOGD(TAG, "IMU STATE roll=%d pitch=%d yaw=%d flags=0x%02x", (int)state.roll_deg_x100,
                         (int)state.pitch_deg_x100, (int)state.yaw_deg_x100, (unsigned)state.motion_flags);
                ESP_LOGI(OBS_TAG,
                         "evt=imu_state msg_class=%u msg_id=%u roll_deg_x100=%d pitch_deg_x100=%d yaw_deg_x100=%d "
                         "acc_norm_mg=%u gyro_norm_dps_x10=%u motion_flags=0x%02x",
                         (unsigned)MCU_FRAME_CLASS_SENSOR, (unsigned)MCU_SENSOR_MSG_IMU_STATE, (int)state.roll_deg_x100,
                         (int)state.pitch_deg_x100, (int)state.yaw_deg_x100, (unsigned)state.acc_norm_mg,
                         (unsigned)state.gyro_norm_dps_x10, (unsigned)state.motion_flags);
            }
        }
        return ret;
    }
    default:
        return ESP_ERR_NOT_FOUND;
    }
}
