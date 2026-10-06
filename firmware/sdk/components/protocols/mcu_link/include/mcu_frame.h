/**
 * @file mcu_frame.h
 * @brief Frame types and helpers for the STM32 coprocessor protocol.
 */

#ifndef MCU_FRAME_H
#define MCU_FRAME_H

#include "esp_err.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MCU_FRAME_MAGIC0 0xA5u
#define MCU_FRAME_MAGIC1 0x5Au
#define MCU_FRAME_PROTO_VERSION 0x01u

#define MCU_FRAME_MAX_PAYLOAD_SIZE 128u
#define MCU_FRAME_PREFIX_SIZE 12u
#define MCU_FRAME_CRC_SIZE 2u
#define MCU_FRAME_HEADER_SIZE MCU_FRAME_PREFIX_SIZE
#define MCU_FRAME_MAX_RAW_SIZE (MCU_FRAME_PREFIX_SIZE + MCU_FRAME_MAX_PAYLOAD_SIZE + MCU_FRAME_CRC_SIZE)
#define MCU_FRAME_MAX_COBS_SIZE (MCU_FRAME_MAX_RAW_SIZE + (MCU_FRAME_MAX_RAW_SIZE / 254u) + 1u)
#define MCU_FRAME_MAX_WIRE_SIZE (MCU_FRAME_MAX_COBS_SIZE + 1u)

#define MCU_HELLO_BASE_PAYLOAD_LEN 9u
#define MCU_HELLO_TLV_GIT_BRANCH 0x01u
#define MCU_HELLO_TLV_GIT_COMMIT 0x02u
#define MCU_HELLO_TLV_GIT_DIRTY 0x03u

typedef enum {
    MCU_CAPABILITY_MOTION = 1u << 0,
    MCU_CAPABILITY_LED = 1u << 1,
    MCU_CAPABILITY_TOUCH = 1u << 2,
    MCU_CAPABILITY_IMU = 1u << 3,
    MCU_CAPABILITY_MAGNETOMETER = 1u << 4,
    MCU_CAPABILITY_RUNTIME_SNAPSHOT = 1u << 5,
    MCU_CAPABILITY_POWER = 1u << 6,
} mcu_capability_t;

typedef enum {
    MCU_SENSOR_TOUCH = 1u << 0,
    MCU_SENSOR_IMU = 1u << 1,
    MCU_SENSOR_MAGNETOMETER = 1u << 2,
} mcu_sensor_t;

typedef enum {
    MCU_DEFAULT_STREAM_PROFILE_V1 = 0x01u,
} mcu_default_stream_profile_t;

typedef enum {
    MCU_FRAME_CLASS_SYS = 0x01u,
    MCU_FRAME_CLASS_MOTION = 0x02u,
    MCU_FRAME_CLASS_LED = 0x03u,
    MCU_FRAME_CLASS_SENSOR = 0x04u,
    MCU_FRAME_CLASS_POWER = 0x05u,
} mcu_frame_class_t;

typedef enum {
    MCU_FRAME_FLAG_ACK_REQ = 1u << 0,
    MCU_FRAME_FLAG_RESPONSE = 1u << 1,
    MCU_FRAME_FLAG_FINAL = 1u << 2,
} mcu_frame_flags_t;

typedef enum {
    MCU_SYS_MSG_HELLO_REQ = 0x01u,
    MCU_SYS_MSG_HELLO_RSP = 0x02u,
    MCU_SYS_MSG_HEARTBEAT = 0x03u,
    MCU_SYS_MSG_ACK = 0x04u,
    MCU_SYS_MSG_NACK = 0x05u,
    MCU_SYS_MSG_FAULT = 0x06u,
    MCU_SYS_MSG_SNAPSHOT_REQ = 0x07u,
    MCU_SYS_MSG_SNAPSHOT_RSP = 0x08u,
    MCU_SYS_MSG_OTA_ENTER = 0x09u,
} mcu_sys_msg_id_t;

typedef enum {
    MCU_MOTION_MSG_SERVO_MOVE = 0x01u,
    MCU_MOTION_MSG_SERVO_STOP = 0x02u,
    MCU_MOTION_MSG_MOTION_DONE = 0x03u,
    MCU_MOTION_MSG_MOTION_STATE = 0x04u,
    MCU_MOTION_MSG_SERVO_JOG = 0x05u,
    MCU_MOTION_MSG_SERVO_SEQUENCE = 0x06u,
    MCU_MOTION_MSG_SERVO_SEQUENCE_BEGIN = 0x07u,
    MCU_MOTION_MSG_SERVO_SEQUENCE_CHUNK = 0x08u,
    MCU_MOTION_MSG_SERVO_SEQUENCE_END = 0x09u,
    MCU_MOTION_MSG_SERVO_PWM_UNLOCK = 0x0Au,
    MCU_MOTION_MSG_SERVO_FEEDBACK_RSP = 0x0Bu,
    MCU_MOTION_MSG_SERVO_FEEDBACK = MCU_MOTION_MSG_SERVO_FEEDBACK_RSP,
    MCU_MOTION_MSG_SERVO_PWM_LOCK = 0x0Cu,
    MCU_MOTION_MSG_SERVO_DIRECT_TARGET = 0x0Eu,
    MCU_MOTION_MSG_SERVO_FEEDBACK_REQ = 0x0Fu,
} mcu_motion_msg_id_t;

typedef enum {
    MCU_LED_MSG_SET_STATIC = 0x01u,
    MCU_LED_MSG_SET_EFFECT = 0x02u,
    MCU_LED_MSG_OFF = 0x03u,
    MCU_LED_MSG_DONE = 0x04u,
    MCU_LED_MSG_STATE = 0x05u,
} mcu_led_msg_id_t;

typedef enum {
    MCU_SENSOR_MSG_TOUCH_EVENT = 0x01u,
    MCU_SENSOR_MSG_MAG_STATE = 0x02u,
    MCU_SENSOR_MSG_MAG_EVENT = 0x03u,
    MCU_SENSOR_MSG_IMU_STATE = 0x04u,
    MCU_SENSOR_MSG_IMU_EVENT = 0x05u,
    MCU_SENSOR_MSG_SENSOR_HEALTH = 0x06u,
} mcu_sensor_msg_id_t;

typedef enum {
    MCU_POWER_MSG_5V_ENABLE = 0x01u,
    MCU_POWER_MSG_5V_DISABLE = 0x02u,
} mcu_power_msg_id_t;

typedef struct {
    uint8_t magic0;
    uint8_t magic1;
    uint8_t proto_version;
    uint8_t msg_class;
    uint8_t msg_id;
    uint8_t flags;
    uint32_t seq;
    uint16_t payload_len;
} mcu_frame_header_t;

typedef struct {
    mcu_frame_header_t header;
    const uint8_t *payload;
} mcu_frame_view_t;

typedef struct {
    mcu_frame_header_t header;
    uint16_t crc16;
    uint8_t payload[MCU_FRAME_MAX_PAYLOAD_SIZE];
} mcu_frame_t;

void mcu_frame_header_init(mcu_frame_header_t *header, uint8_t msg_class, uint8_t msg_id, uint8_t flags, uint32_t seq,
                           uint16_t payload_len);
bool mcu_frame_header_is_valid(const mcu_frame_header_t *header);
uint16_t mcu_frame_compute_crc(const mcu_frame_header_t *header, const uint8_t *payload);
esp_err_t mcu_frame_pack(const mcu_frame_header_t *header, const uint8_t *payload, uint8_t *buffer, size_t buffer_len,
                         size_t *encoded_len);
esp_err_t mcu_frame_unpack(const uint8_t *buffer, size_t buffer_len, mcu_frame_t *frame, size_t *payload_len);

#ifdef __cplusplus
}
#endif

#endif /* MCU_FRAME_H */

