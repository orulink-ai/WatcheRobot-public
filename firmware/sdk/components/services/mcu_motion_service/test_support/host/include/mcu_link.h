#ifndef TEST_MCU_LINK_H
#define TEST_MCU_LINK_H

#include "esp_err.h"

#include <stddef.h>
#include <stdint.h>

#define MCU_FRAME_CLASS_MOTION 0x02u
#define MCU_MOTION_MSG_SERVO_MOVE 0x01u
#define MCU_MOTION_MSG_SERVO_STOP 0x02u
#define MCU_MOTION_MSG_SERVO_JOG 0x05u
#define MCU_MOTION_MSG_SERVO_SEQUENCE 0x06u
#define MCU_MOTION_MSG_SERVO_SEQUENCE_BEGIN 0x07u
#define MCU_MOTION_MSG_SERVO_SEQUENCE_CHUNK 0x08u
#define MCU_MOTION_MSG_SERVO_SEQUENCE_END 0x09u
#define MCU_MOTION_MSG_SERVO_PWM_UNLOCK 0x0Au
#define MCU_MOTION_MSG_SERVO_FEEDBACK_RSP 0x0Bu
#define MCU_MOTION_MSG_SERVO_FEEDBACK MCU_MOTION_MSG_SERVO_FEEDBACK_RSP
#define MCU_MOTION_MSG_SERVO_PWM_LOCK 0x0Cu
#define MCU_MOTION_MSG_SERVO_DIRECT_TARGET 0x0Eu
#define MCU_MOTION_MSG_SERVO_FEEDBACK_REQ 0x0Fu
#define MCU_MOTION_MSG_MOTION_DONE 0x03u
#define MCU_MOTION_MSG_MOTION_STATE 0x04u
#define MCU_FRAME_FLAG_ACK_REQ 0x01u

typedef uint8_t mcu_motion_msg_id_t;

typedef struct {
    uint16_t payload_len;
    uint8_t msg_id;
} test_mcu_frame_header_t;

typedef struct mcu_link {
    int placeholder;
} mcu_link_t;

typedef struct {
    test_mcu_frame_header_t header;
    uint8_t payload[128];
} test_mcu_frame_t;

typedef enum {
    MCU_LINK_RX_EVENT_ACK,
    MCU_LINK_RX_EVENT_NACK,
    MCU_LINK_RX_EVENT_FAULT,
    MCU_LINK_RX_EVENT_MOTION_DONE,
    MCU_LINK_RX_EVENT_SERVO_FEEDBACK,
} mcu_link_rx_event_type_t;

typedef struct {
    mcu_link_rx_event_type_t type;
    test_mcu_frame_t frame;
} mcu_link_event_t;

esp_err_t mcu_link_send_frame(mcu_link_t *link,
                              uint8_t msg_class,
                              uint8_t msg_id,
                              uint8_t flags,
                              const uint8_t *payload,
                              uint16_t payload_len,
                              uint32_t *out_seq,
                              size_t *out_wire_len);

#endif /* TEST_MCU_LINK_H */

