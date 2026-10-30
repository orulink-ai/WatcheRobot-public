#ifndef MCU_SENSOR_SERVICE_H
#define MCU_SENSOR_SERVICE_H

#include "esp_err.h"
#include "mcu_link.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    MCU_TOUCH_EVENT_PRESS = 1,
    MCU_TOUCH_EVENT_RELEASE = 2,
    MCU_TOUCH_EVENT_LONG_PRESS = 3,
} mcu_touch_event_code_t;

typedef struct {
    uint8_t touch_id;
    mcu_touch_event_code_t event_code;
    bool active;
    uint32_t timestamp_ms;
} mcu_touch_state_t;

typedef struct {
    uint16_t heading_deg_x100;
    uint16_t field_norm_uT;
    uint8_t quality;
    uint8_t status_bits;
    uint32_t timestamp_ms;
} mcu_mag_state_t;

typedef struct {
    int16_t roll_deg_x100;
    int16_t pitch_deg_x100;
    int16_t yaw_deg_x100;
    uint16_t acc_norm_mg;
    uint16_t gyro_norm_dps_x10;
    uint8_t motion_flags;
    uint32_t timestamp_ms;
} mcu_imu_state_t;

typedef enum {
    MCU_SENSOR_FRAME_TOUCH = 1,
    MCU_SENSOR_FRAME_MAG = 2,
    MCU_SENSOR_FRAME_IMU = 3,
} mcu_sensor_frame_type_t;

typedef struct {
    mcu_sensor_frame_type_t type;
    union {
        mcu_touch_state_t touch;
        mcu_mag_state_t mag;
        mcu_imu_state_t imu;
    } data;
} mcu_sensor_frame_t;

typedef struct {
    uint32_t touch_apply_count;
    uint32_t touch_overwrite_count;
    uint32_t touch_invalid_count;
    uint32_t mag_apply_count;
    uint32_t mag_overwrite_count;
    uint32_t mag_invalid_count;
    uint32_t imu_apply_count;
    uint32_t imu_overwrite_count;
    uint32_t imu_invalid_count;
    uint32_t frame_apply_count;
    uint32_t frame_invalid_count;
} mcu_sensor_service_stats_t;

esp_err_t mcu_sensor_service_init(void);
esp_err_t mcu_sensor_service_apply_touch(const mcu_touch_state_t *state);
esp_err_t mcu_sensor_service_apply_mag(const mcu_mag_state_t *state);
esp_err_t mcu_sensor_service_apply_imu(const mcu_imu_state_t *state);
esp_err_t mcu_sensor_service_apply_frame(const mcu_sensor_frame_t *frame);
esp_err_t mcu_sensor_service_update_touch(const mcu_touch_state_t *state);
esp_err_t mcu_sensor_service_update_mag(const mcu_mag_state_t *state);
esp_err_t mcu_sensor_service_update_imu(const mcu_imu_state_t *state);
esp_err_t mcu_sensor_service_handle_link_event(const mcu_link_event_t *event, bool *out_overwrote_latest_state);
esp_err_t mcu_sensor_service_get_latest_touch(mcu_touch_state_t *out_state);
esp_err_t mcu_sensor_service_get_latest_mag(mcu_mag_state_t *out_state);
esp_err_t mcu_sensor_service_get_latest_imu(mcu_imu_state_t *out_state);
bool mcu_sensor_service_has_latest_touch(void);
bool mcu_sensor_service_has_latest_mag(void);
bool mcu_sensor_service_has_latest_imu(void);
esp_err_t mcu_sensor_service_get_stats(mcu_sensor_service_stats_t *out_stats);

#ifdef __cplusplus
}
#endif

#endif /* MCU_SENSOR_SERVICE_H */
