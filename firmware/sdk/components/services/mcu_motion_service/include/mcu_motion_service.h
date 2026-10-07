#ifndef MCU_MOTION_SERVICE_H
#define MCU_MOTION_SERVICE_H

#include "esp_err.h"
#include "mcu_link.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    MCU_MOTION_SOURCE_UNKNOWN = 0,
    MCU_MOTION_SOURCE_BEHAVIOR = 1,
    MCU_MOTION_SOURCE_BLE = 2,
    MCU_MOTION_SOURCE_WS = 3,
    MCU_MOTION_SOURCE_RECOVERY = 4,
    MCU_MOTION_SOURCE_RELAY_W1 = 5,
    MCU_MOTION_SOURCE_FACE_TRACKING = 6,
} mcu_motion_source_t;

typedef enum {
    MCU_MOTION_PROFILE_LINEAR = 0,
    MCU_MOTION_PROFILE_EASE_IN_OUT = 1,
} mcu_motion_profile_t;

typedef enum {
    MCU_MOTION_AXIS_X = 1u << 0,
    MCU_MOTION_AXIS_Y = 1u << 1,
} mcu_motion_axis_t;

#define MCU_MOTION_SEQUENCE_MAX_SEGMENTS 15u
#define MCU_MOTION_SEQUENCE_CHUNK_MAX_SEGMENTS 15u
#define MCU_MOTION_CHUNKED_SEQUENCE_MAX_SEGMENTS 64u

typedef struct {
    uint8_t axis_mask;
    int16_t x_deg_x10;
    int16_t y_deg_x10;
    uint16_t duration_ms;
    uint8_t motion_profile;
    mcu_motion_source_t source;
} mcu_motion_request_t;

typedef struct {
    uint8_t axis_mask;
    int16_t x_velocity_deg_x10_per_sec;
    int16_t y_velocity_deg_x10_per_sec;
    uint16_t timeout_ms;
    mcu_motion_source_t source;
    uint8_t x_min_deg;
    uint8_t x_max_deg;
    uint8_t y_min_deg;
    uint8_t y_max_deg;
} mcu_motion_jog_request_t;

typedef struct {
    uint8_t axis_mask;
    int16_t x_deg_x10;
    int16_t y_deg_x10;
    mcu_motion_source_t source;
} mcu_motion_direct_target_t;

typedef struct {
    uint8_t axis_mask;
    int16_t x_deg_x10;
    int16_t y_deg_x10;
    uint16_t duration_ms;
    uint8_t motion_profile;
} mcu_motion_segment_t;

typedef struct {
    mcu_motion_source_t source;
    uint8_t segment_count;
    mcu_motion_segment_t segments[MCU_MOTION_SEQUENCE_MAX_SEGMENTS];
} mcu_motion_sequence_t;

typedef struct {
    uint8_t axis_mask;
    mcu_motion_source_t source;
} mcu_motion_pwm_unlock_request_t;

typedef struct {
    uint8_t axis_mask;
    mcu_motion_source_t source;
} mcu_motion_pwm_lock_request_t;

typedef struct {
    uint8_t axis_mask;
    uint16_t x_raw;
    int16_t x_angle_x10;
    uint16_t y_raw;
    int16_t y_angle_x10;
} mcu_motion_servo_feedback_t;

typedef void (*mcu_motion_servo_feedback_cb_t)(const mcu_motion_servo_feedback_t *feedback, void *ctx);

typedef enum {
    MCU_MOTION_LIFECYCLE_ACKED = 0,
    MCU_MOTION_LIFECYCLE_REJECTED,
    MCU_MOTION_LIFECYCLE_DONE,
    MCU_MOTION_LIFECYCLE_FAULT,
} mcu_motion_lifecycle_event_type_t;

typedef enum {
    MCU_MOTION_RESULT_SUCCESS = 0,
    MCU_MOTION_RESULT_STOPPED = 1,
    MCU_MOTION_RESULT_INTERRUPTED = 2,
    MCU_MOTION_RESULT_FAULT = 3,
} mcu_motion_result_t;

typedef struct {
    mcu_motion_lifecycle_event_type_t type;
    uint32_t ref_seq;
    uint16_t status;
    uint16_t reason;
    uint16_t fault_code;
    uint16_t detail;
    mcu_motion_result_t result;
    int16_t final_x_deg_x10;
    int16_t final_y_deg_x10;
    uint16_t exec_ms;
} mcu_motion_lifecycle_event_t;

typedef void (*mcu_motion_lifecycle_cb_t)(const mcu_motion_lifecycle_event_t *event, void *ctx);

typedef struct {
    uint16_t sequence_id;
    mcu_motion_source_t source;
    uint8_t segment_count;
    mcu_motion_segment_t segments[MCU_MOTION_CHUNKED_SEQUENCE_MAX_SEGMENTS];
} mcu_motion_chunked_sequence_t;

esp_err_t mcu_motion_service_init(void);
/* Read-only position query; response delivered via the feedback callback. */
esp_err_t mcu_motion_request_feedback(void);
esp_err_t mcu_motion_submit_with_seq(const mcu_motion_request_t *request, uint32_t *out_seq);
esp_err_t mcu_motion_submit(const mcu_motion_request_t *request);
esp_err_t mcu_motion_jog_with_seq(const mcu_motion_jog_request_t *request, uint32_t *out_seq);
esp_err_t mcu_motion_jog(const mcu_motion_jog_request_t *request);
esp_err_t mcu_motion_submit_direct_target_with_seq(const mcu_motion_direct_target_t *request, uint32_t *out_seq);
esp_err_t mcu_motion_submit_direct_target(const mcu_motion_direct_target_t *request);
esp_err_t mcu_motion_submit_sequence_with_seq(const mcu_motion_sequence_t *sequence, uint32_t *out_seq);
esp_err_t mcu_motion_submit_sequence(const mcu_motion_sequence_t *sequence);
esp_err_t mcu_motion_submit_chunked_sequence_with_seq(const mcu_motion_chunked_sequence_t *sequence,
                                                      uint32_t *out_end_seq);
esp_err_t mcu_motion_submit_chunked_sequence(const mcu_motion_chunked_sequence_t *sequence);
esp_err_t mcu_motion_pwm_unlock(const mcu_motion_pwm_unlock_request_t *request);
esp_err_t mcu_motion_pwm_lock(const mcu_motion_pwm_lock_request_t *request);
esp_err_t mcu_motion_set_servo_feedback_callback(mcu_motion_servo_feedback_cb_t cb, void *ctx);
/* The lifecycle callback runs on the MCU link task and must remain non-blocking.
 * Register/unregister it from another task, not recursively from the callback itself. */
esp_err_t mcu_motion_set_lifecycle_callback(mcu_motion_lifecycle_cb_t cb, void *ctx);
esp_err_t mcu_motion_service_get_last_request(mcu_motion_request_t *out_request);
esp_err_t mcu_motion_stop(mcu_motion_source_t source);
esp_err_t mcu_motion_service_handle_link_event(const mcu_link_event_t *event);

#ifdef __cplusplus
}
#endif

#endif /* MCU_MOTION_SERVICE_H */
