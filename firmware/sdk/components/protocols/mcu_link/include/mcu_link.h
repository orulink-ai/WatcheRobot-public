/**
 * @file mcu_link.h
 * @brief Public umbrella header for the coprocessor UART protocol core.
 */

#ifndef MCU_LINK_H
#define MCU_LINK_H

#include "mcu_cobs.h"
#include "mcu_crc16.h"
#include "mcu_frame.h"
#include "mcu_link_fsm.h"
#include "mcu_link_stats.h"
#include "mcu_wire.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MCU_LINK_GIT_BRANCH_MAX_LEN 64u
#define MCU_LINK_GIT_COMMIT_MAX_LEN 17u

typedef struct {
    bool version_valid;
    uint8_t fw_major;
    uint8_t fw_minor;
    uint8_t fw_patch;
    uint8_t hw_version;
    uint8_t capability_bitmap;
    uint8_t sensor_bitmap;
    uint8_t default_stream_profile;
    bool git_valid;
    bool git_dirty;
    char git_branch[MCU_LINK_GIT_BRANCH_MAX_LEN];
    char git_commit[MCU_LINK_GIT_COMMIT_MAX_LEN];
} mcu_link_peer_info_t;

typedef struct {
    mcu_link_fsm_t fsm;
    mcu_link_stats_t stats;
    mcu_link_peer_info_t peer_info;
    uint32_t next_tx_seq;
    struct {
        uint8_t stream[MCU_FRAME_MAX_WIRE_SIZE];
        size_t stream_len;
        uint8_t pending[MCU_FRAME_MAX_WIRE_SIZE];
        size_t pending_len;
    } rx;
} mcu_link_t;

typedef enum {
    MCU_LINK_RX_EVENT_NONE = 0,
    MCU_LINK_RX_EVENT_HELLO_RSP,
    MCU_LINK_RX_EVENT_SNAPSHOT_RSP,
    MCU_LINK_RX_EVENT_ACK,
    MCU_LINK_RX_EVENT_NACK,
    MCU_LINK_RX_EVENT_FAULT,
    MCU_LINK_RX_EVENT_MOTION_DONE,
    MCU_LINK_RX_EVENT_SERVO_FEEDBACK_RSP,
    MCU_LINK_RX_EVENT_SERVO_FEEDBACK = MCU_LINK_RX_EVENT_SERVO_FEEDBACK_RSP,
    MCU_LINK_RX_EVENT_LED_DONE,
    MCU_LINK_RX_EVENT_TOUCH_EVENT,
    MCU_LINK_RX_EVENT_MAG_STATE,
    MCU_LINK_RX_EVENT_IMU_STATE,
} mcu_link_rx_event_type_t;

typedef struct {
    mcu_link_rx_event_type_t type;
    mcu_frame_t frame;
} mcu_link_event_t;

esp_err_t mcu_link_init(mcu_link_t *link);
esp_err_t mcu_link_reset(mcu_link_t *link);
esp_err_t mcu_link_begin_handshake(mcu_link_t *link);
esp_err_t mcu_link_on_hello_rsp(mcu_link_t *link, bool snapshot_supported);
esp_err_t mcu_link_mark_baseline_synced(mcu_link_t *link);
esp_err_t mcu_link_mark_degraded(mcu_link_t *link);
esp_err_t mcu_link_begin_recovery(mcu_link_t *link);
mcu_link_state_t mcu_link_get_state(const mcu_link_t *link);
bool mcu_link_is_link_ready(const mcu_link_t *link);
bool mcu_link_is_ready(const mcu_link_t *link);
bool mcu_link_snapshot_supported(const mcu_link_t *link);
const mcu_link_stats_t *mcu_link_get_stats(const mcu_link_t *link);
esp_err_t mcu_link_copy_stats(const mcu_link_t *link, mcu_link_stats_t *out_stats);
esp_err_t mcu_link_copy_peer_info(const mcu_link_t *link, mcu_link_peer_info_t *out_info);
esp_err_t mcu_link_record_ack_timeout(mcu_link_t *link);
esp_err_t mcu_link_record_crc_error(mcu_link_t *link);
esp_err_t mcu_link_record_reconnect(mcu_link_t *link);
esp_err_t mcu_link_record_dropped_state(mcu_link_t *link);
esp_err_t mcu_link_record_motion_done_fault(mcu_link_t *link);
esp_err_t mcu_link_send_frame(mcu_link_t *link, uint8_t msg_class, uint8_t msg_id, uint8_t flags,
                              const uint8_t *payload, uint16_t payload_len, uint32_t *out_seq, size_t *out_wire_len);
esp_err_t mcu_link_send_hello_req(mcu_link_t *link, uint32_t *out_seq, size_t *out_wire_len);
esp_err_t mcu_link_poll(mcu_link_t *link, mcu_link_event_t *out_event);

#ifdef __cplusplus
}
#endif

#endif /* MCU_LINK_H */

