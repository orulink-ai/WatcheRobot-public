#include "mcu_link.h"
#include "mcu_link_uart.h"

#include <string.h>

enum {
    MCU_LINK_READ_CHUNK_SIZE = 64,
};

static uint32_t decode_u32_le(const uint8_t *src) {
    return ((uint32_t)src[0]) | ((uint32_t)src[1] << 8u) | ((uint32_t)src[2] << 16u) | ((uint32_t)src[3] << 24u);
}

static uint32_t mcu_link_alloc_tx_seq(mcu_link_t *link) {
    uint32_t seq = link->next_tx_seq;

    if (seq == 0u) {
        seq = 1u;
    }

    link->next_tx_seq = seq + 1u;
    return seq;
}

static void mcu_link_copy_payload_text(char *dst, size_t dst_size, const uint8_t *src, uint8_t src_len) {
    size_t copy_len;

    if (dst == NULL || dst_size == 0u) {
        return;
    }

    dst[0] = '\0';
    if (src == NULL || src_len == 0u) {
        return;
    }

    copy_len = src_len;
    if (copy_len >= dst_size) {
        copy_len = dst_size - 1u;
    }
    memcpy(dst, src, copy_len);
    dst[copy_len] = '\0';
}

static void mcu_link_parse_hello_rsp_peer_info(mcu_link_t *link, const mcu_frame_t *frame) {
    size_t offset = MCU_HELLO_BASE_PAYLOAD_LEN;

    if (link == NULL) {
        return;
    }

    memset(&link->peer_info, 0, sizeof(link->peer_info));
    if (frame == NULL || frame->header.payload_len < MCU_HELLO_BASE_PAYLOAD_LEN) {
        return;
    }

    link->peer_info.version_valid = true;
    link->peer_info.fw_major = frame->payload[1];
    link->peer_info.fw_minor = frame->payload[2];
    link->peer_info.fw_patch = frame->payload[3];
    link->peer_info.hw_version = frame->payload[4];
    link->peer_info.capability_bitmap = frame->payload[5];
    link->peer_info.sensor_bitmap = frame->payload[6];
    link->peer_info.default_stream_profile = frame->payload[8];

    while ((offset + 2u) <= frame->header.payload_len) {
        const uint8_t type = frame->payload[offset++];
        const uint8_t len = frame->payload[offset++];

        if ((offset + len) > frame->header.payload_len) {
            break;
        }

        switch (type) {
        case MCU_HELLO_TLV_GIT_BRANCH:
            mcu_link_copy_payload_text(link->peer_info.git_branch, sizeof(link->peer_info.git_branch),
                                       &frame->payload[offset], len);
            link->peer_info.git_valid = link->peer_info.git_branch[0] != '\0';
            break;
        case MCU_HELLO_TLV_GIT_COMMIT:
            mcu_link_copy_payload_text(link->peer_info.git_commit, sizeof(link->peer_info.git_commit),
                                       &frame->payload[offset], len);
            break;
        case MCU_HELLO_TLV_GIT_DIRTY:
            link->peer_info.git_dirty = len > 0u && frame->payload[offset] != 0u;
            break;
        default:
            break;
        }

        offset += len;
    }
}

static bool mcu_link_parse_hello_rsp_snapshot_supported(const mcu_frame_t *frame, uint8_t *out_default_profile) {
    const uint8_t *payload;

    if (frame == NULL) {
        return false;
    }

    if (frame->header.payload_len < MCU_HELLO_BASE_PAYLOAD_LEN) {
        if (out_default_profile != NULL) {
            *out_default_profile = 0u;
        }
        return false;
    }

    payload = frame->payload;

    if (out_default_profile != NULL) {
        *out_default_profile = payload[8];
    }

    return (payload[5] & MCU_CAPABILITY_RUNTIME_SNAPSHOT) != 0u;
}

static esp_err_t mcu_link_handle_frame(mcu_link_t *link, const mcu_frame_t *frame, mcu_link_event_t *out_event) {
    if (link == NULL || frame == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (out_event != NULL) {
        memset(out_event, 0, sizeof(*out_event));
        out_event->frame = *frame;
    }

    link->fsm.last_transition_seq = frame->header.seq;

    switch ((mcu_frame_class_t)frame->header.msg_class) {
    case MCU_FRAME_CLASS_SYS:
        switch ((mcu_sys_msg_id_t)frame->header.msg_id) {
        case MCU_SYS_MSG_HELLO_RSP: {
            bool snapshot_supported = false;
            uint8_t default_stream_profile = 0u;
            mcu_link_state_t previous_state = mcu_link_get_state(link);

            if (frame->header.payload_len < MCU_HELLO_BASE_PAYLOAD_LEN) {
                return ESP_ERR_NOT_FOUND;
            }

            mcu_link_parse_hello_rsp_peer_info(link, frame);
            snapshot_supported = mcu_link_parse_hello_rsp_snapshot_supported(frame, &default_stream_profile);
            if (default_stream_profile != MCU_DEFAULT_STREAM_PROFILE_V1) {
                (void)mcu_link_mark_degraded(link);
                return ESP_ERR_NOT_SUPPORTED;
            }

            if (previous_state == MCU_LINK_STATE_RECOVERING || previous_state == MCU_LINK_STATE_DEGRADED) {
                mcu_link_record_reconnect(link);
            }

            if (previous_state != MCU_LINK_STATE_READY) {
                (void)mcu_link_on_hello_rsp(link, snapshot_supported);
            }

            if (out_event != NULL) {
                out_event->type = MCU_LINK_RX_EVENT_HELLO_RSP;
            }
            return ESP_OK;
        }
        case MCU_SYS_MSG_SNAPSHOT_RSP:
            if (out_event != NULL) {
                out_event->type = MCU_LINK_RX_EVENT_SNAPSHOT_RSP;
            }
            return ESP_OK;
        case MCU_SYS_MSG_ACK:
            if (frame->header.payload_len < 6u) {
                return ESP_ERR_NOT_FOUND;
            }
            if (out_event != NULL) {
                out_event->type = MCU_LINK_RX_EVENT_ACK;
            }
            return ESP_OK;
        case MCU_SYS_MSG_NACK:
            if (frame->header.payload_len < 8u) {
                return ESP_ERR_NOT_FOUND;
            }
            if (mcu_link_get_state(link) == MCU_LINK_STATE_HANDSHAKING ||
                mcu_link_get_state(link) == MCU_LINK_STATE_RECOVERING) {
                (void)mcu_link_mark_degraded(link);
            }
            if (out_event != NULL) {
                out_event->type = MCU_LINK_RX_EVENT_NACK;
            }
            return ESP_OK;
        case MCU_SYS_MSG_FAULT: {
            uint8_t fault_source = 0u;
            uint32_t ref_seq = 0u;

            if (frame->header.payload_len < 9u) {
                return ESP_ERR_NOT_FOUND;
            }

            ref_seq = decode_u32_le(frame->payload);
            fault_source = frame->payload[4];

            if (fault_source == 0x01u || ref_seq != 0u) {
                mcu_link_record_motion_done_fault(link);
            }

            if (fault_source == 0x06u) {
                (void)mcu_link_mark_degraded(link);
            }

            if (out_event != NULL) {
                out_event->type = MCU_LINK_RX_EVENT_FAULT;
            }
            return ESP_OK;
        }
        default:
            break;
        }
        break;
    case MCU_FRAME_CLASS_MOTION:
        if ((mcu_motion_msg_id_t)frame->header.msg_id == MCU_MOTION_MSG_MOTION_DONE) {
            if (frame->header.payload_len < 11u) {
                return ESP_ERR_NOT_FOUND;
            }
            if (out_event != NULL) {
                out_event->type = MCU_LINK_RX_EVENT_MOTION_DONE;
            }
            return ESP_OK;
        }
        if ((mcu_motion_msg_id_t)frame->header.msg_id == MCU_MOTION_MSG_MOTION_STATE) {
            if (frame->header.payload_len < 13u) {
                return ESP_ERR_NOT_FOUND;
            }
            if (out_event != NULL) {
                out_event->type = MCU_LINK_RX_EVENT_SERVO_FEEDBACK;
            }
            return ESP_OK;
        }
        if ((mcu_motion_msg_id_t)frame->header.msg_id == MCU_MOTION_MSG_SERVO_FEEDBACK_RSP) {
            if (frame->header.payload_len < 9u) {
                return ESP_ERR_NOT_FOUND;
            }
            if (out_event != NULL) {
                out_event->type = MCU_LINK_RX_EVENT_SERVO_FEEDBACK_RSP;
            }
            return ESP_OK;
        }
        break;
    case MCU_FRAME_CLASS_LED:
        if ((mcu_led_msg_id_t)frame->header.msg_id == MCU_LED_MSG_DONE) {
            if (frame->header.payload_len < 5u) {
                return ESP_ERR_NOT_FOUND;
            }
            if (out_event != NULL) {
                out_event->type = MCU_LINK_RX_EVENT_LED_DONE;
            }
            return ESP_OK;
        }
        break;
    case MCU_FRAME_CLASS_SENSOR:
        switch ((mcu_sensor_msg_id_t)frame->header.msg_id) {
        case MCU_SENSOR_MSG_TOUCH_EVENT:
            if (frame->header.payload_len < 6u) {
                return ESP_ERR_NOT_FOUND;
            }
            if (out_event != NULL) {
                out_event->type = MCU_LINK_RX_EVENT_TOUCH_EVENT;
            }
            return ESP_OK;
        case MCU_SENSOR_MSG_MAG_STATE:
            if (frame->header.payload_len < 6u) {
                return ESP_ERR_NOT_FOUND;
            }
            if (out_event != NULL) {
                out_event->type = MCU_LINK_RX_EVENT_MAG_STATE;
            }
            return ESP_OK;
        case MCU_SENSOR_MSG_IMU_STATE:
            if (frame->header.payload_len < 11u) {
                return ESP_ERR_NOT_FOUND;
            }
            if (out_event != NULL) {
                out_event->type = MCU_LINK_RX_EVENT_IMU_STATE;
            }
            return ESP_OK;
        default:
            break;
        }
        break;
    default:
        break;
    }

    return ESP_ERR_NOT_FOUND;
}

static void mcu_link_store_pending_bytes(mcu_link_t *link, const uint8_t *bytes, size_t byte_count) {
    if (link == NULL) {
        return;
    }

    if (bytes == NULL || byte_count == 0u) {
        link->rx.pending_len = 0u;
        return;
    }

    if (byte_count > sizeof(link->rx.pending)) {
        byte_count = sizeof(link->rx.pending);
    }

    memmove(link->rx.pending, bytes, byte_count);
    link->rx.pending_len = byte_count;
}

static esp_err_t mcu_link_process_frame_candidate(mcu_link_t *link, mcu_link_event_t *out_event, bool *out_has_event) {
    uint8_t raw[MCU_FRAME_MAX_RAW_SIZE];
    mcu_frame_t frame;
    size_t raw_len = 0u;
    size_t payload_len = 0u;
    esp_err_t decode_ret;

    if (link == NULL || out_has_event == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    *out_has_event = false;
    if (link->rx.stream_len == 0u) {
        return ESP_OK;
    }

    link->rx.stream[link->rx.stream_len] = 0u;
    decode_ret = mcu_wire_decode_raw(link->rx.stream, link->rx.stream_len + 1u, raw, sizeof(raw), &raw_len);
    if (decode_ret != ESP_OK) {
        mcu_link_record_crc_error(link);
        link->rx.stream_len = 0u;
        return ESP_OK;
    }

    decode_ret = mcu_frame_unpack(raw, raw_len, &frame, &payload_len);
    if (decode_ret != ESP_OK) {
        mcu_link_record_crc_error(link);
        link->rx.stream_len = 0u;
        return ESP_OK;
    }

    frame.header.payload_len = (uint16_t)payload_len;
    decode_ret = mcu_link_handle_frame(link, &frame, out_event);
    link->rx.stream_len = 0u;
    if (decode_ret == ESP_OK) {
        *out_has_event = true;
    }

    return ESP_OK;
}

static esp_err_t mcu_link_process_bytes(mcu_link_t *link, const uint8_t *bytes, size_t byte_count, size_t *out_consumed,
                                        mcu_link_event_t *out_event, bool *out_has_event) {
    size_t i;

    if (link == NULL || bytes == NULL || out_consumed == NULL || out_has_event == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    *out_consumed = 0u;
    *out_has_event = false;

    for (i = 0u; i < byte_count; ++i) {
        const uint8_t byte = bytes[i];

        if (byte == 0u) {
            esp_err_t ret = mcu_link_process_frame_candidate(link, out_event, out_has_event);
            if (ret != ESP_OK) {
                return ret;
            }

            *out_consumed = i + 1u;
            if (*out_has_event) {
                return ESP_OK;
            }
            continue;
        }

        if (link->rx.stream_len >= (sizeof(link->rx.stream) - 1u)) {
            mcu_link_record_crc_error(link);
            link->rx.stream_len = 0u;
        }

        link->rx.stream[link->rx.stream_len++] = byte;
        *out_consumed = i + 1u;
    }

    return ESP_OK;
}

static esp_err_t mcu_link_init_locked(mcu_link_t *link) {
    if (link == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    mcu_link_stats_init(&link->stats);
    memset(&link->peer_info, 0, sizeof(link->peer_info));
    link->next_tx_seq = 1u;
    link->rx.stream_len = 0u;
    link->rx.pending_len = 0u;
    return mcu_link_fsm_init(&link->fsm);
}

static esp_err_t mcu_link_reset_locked(mcu_link_t *link) {
    if (link == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    link->next_tx_seq = 1u;
    memset(&link->peer_info, 0, sizeof(link->peer_info));
    link->rx.stream_len = 0u;
    link->rx.pending_len = 0u;
    return mcu_link_fsm_init(&link->fsm);
}

static esp_err_t mcu_link_begin_handshake_locked(mcu_link_t *link) {
    if (link == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    return mcu_link_fsm_begin_handshake(&link->fsm);
}

static esp_err_t mcu_link_on_hello_rsp_locked(mcu_link_t *link, bool snapshot_supported) {
    if (link == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    return mcu_link_fsm_on_hello_rsp(&link->fsm, snapshot_supported);
}

static esp_err_t mcu_link_mark_baseline_synced_locked(mcu_link_t *link) {
    if (link == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    return mcu_link_fsm_mark_baseline_synced(&link->fsm);
}

static esp_err_t mcu_link_mark_degraded_locked(mcu_link_t *link) {
    if (link == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    return mcu_link_fsm_mark_degraded(&link->fsm);
}

static esp_err_t mcu_link_begin_recovery_locked(mcu_link_t *link) {
    if (link == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    return mcu_link_fsm_begin_recovery(&link->fsm);
}

static mcu_link_state_t mcu_link_get_state_locked(const mcu_link_t *link) {
    if (link == NULL) {
        return MCU_LINK_STATE_DOWN;
    }

    return mcu_link_fsm_get_state(&link->fsm);
}

static bool mcu_link_is_link_ready_locked(const mcu_link_t *link) {
    return link != NULL && mcu_link_fsm_is_link_ready(&link->fsm);
}

static bool mcu_link_is_ready_locked(const mcu_link_t *link) {
    return link != NULL && mcu_link_fsm_is_ready(&link->fsm);
}

static bool mcu_link_snapshot_supported_locked(const mcu_link_t *link) {
    return link != NULL && link->fsm.snapshot_supported;
}

const mcu_link_stats_t *mcu_link_get_stats(const mcu_link_t *link) {
    if (link == NULL) {
        return NULL;
    }

    return &link->stats;
}

static esp_err_t mcu_link_copy_stats_locked(const mcu_link_t *link, mcu_link_stats_t *out_stats) {
    if (link == NULL || out_stats == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    mcu_link_stats_copy(out_stats, &link->stats);
    return ESP_OK;
}

static esp_err_t mcu_link_copy_peer_info_locked(const mcu_link_t *link, mcu_link_peer_info_t *out_info) {
    if (link == NULL || out_info == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    *out_info = link->peer_info;
    return ESP_OK;
}

static esp_err_t mcu_link_record_ack_timeout_locked(mcu_link_t *link) {
    if (link == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    mcu_link_stats_record_ack_timeout(&link->stats);
    return ESP_OK;
}

static esp_err_t mcu_link_record_crc_error_locked(mcu_link_t *link) {
    if (link == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    mcu_link_stats_record_crc_error(&link->stats);
    return ESP_OK;
}

static esp_err_t mcu_link_record_reconnect_locked(mcu_link_t *link) {
    if (link == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    mcu_link_stats_record_reconnect(&link->stats);
    return ESP_OK;
}

static esp_err_t mcu_link_record_dropped_state_locked(mcu_link_t *link) {
    if (link == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    mcu_link_stats_record_dropped_state(&link->stats);
    return ESP_OK;
}

static esp_err_t mcu_link_record_motion_done_fault_locked(mcu_link_t *link) {
    if (link == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    mcu_link_stats_record_motion_done_fault(&link->stats);
    return ESP_OK;
}

static esp_err_t mcu_link_send_frame_locked(mcu_link_t *link, uint8_t msg_class, uint8_t msg_id, uint8_t flags,
                                            const uint8_t *payload, uint16_t payload_len, uint32_t *out_seq,
                                            size_t *out_wire_len) {
    mcu_frame_header_t header;
    uint8_t wire[MCU_FRAME_MAX_WIRE_SIZE];
    size_t wire_len = 0u;
    size_t written = 0u;
    uint32_t seq;
    esp_err_t ret;

    if (link == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (payload_len > MCU_FRAME_MAX_PAYLOAD_SIZE) {
        return ESP_ERR_INVALID_SIZE;
    }

    if (!mcu_link_uart_is_ready()) {
        return ESP_ERR_INVALID_STATE;
    }

    seq = mcu_link_alloc_tx_seq(link);
    mcu_frame_header_init(&header, msg_class, msg_id, flags, seq, payload_len);

    ret = mcu_wire_encode_frame(&header, payload, wire, sizeof(wire), &wire_len);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = mcu_link_uart_write(wire, wire_len, &written);
    if (ret != ESP_OK) {
        return ret;
    }

    if (written != wire_len) {
        return ESP_ERR_INVALID_SIZE;
    }

    if (out_seq != NULL) {
        *out_seq = seq;
    }

    if (out_wire_len != NULL) {
        *out_wire_len = wire_len;
    }

    return ESP_OK;
}

static esp_err_t mcu_link_send_hello_req_locked(mcu_link_t *link, uint32_t *out_seq, size_t *out_wire_len) {
    esp_err_t ret;

    if (link == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    ret = mcu_link_begin_handshake(link);
    if (ret != ESP_OK) {
        return ret;
    }

    return mcu_link_send_frame(link, MCU_FRAME_CLASS_SYS, MCU_SYS_MSG_HELLO_REQ, MCU_FRAME_FLAG_ACK_REQ, NULL, 0u,
                               out_seq, out_wire_len);
}

static esp_err_t mcu_link_poll_locked(mcu_link_t *link, mcu_link_event_t *out_event) {
    uint8_t read_buf[MCU_LINK_READ_CHUNK_SIZE];
    size_t buffered = 0u;
    size_t read_len = 0u;
    esp_err_t ret;

    if (link == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (out_event != NULL) {
        memset(out_event, 0, sizeof(*out_event));
    }

    if (!mcu_link_uart_is_ready()) {
        return ESP_ERR_INVALID_STATE;
    }

    /* Bound each poll even when a noisy peer continuously supplies garbage. */
    for (unsigned chunks = 0; chunks < 16U; ++chunks) {
        if (link->rx.pending_len > 0u) {
            size_t consumed = 0u;
            bool has_event = false;

            ret =
                mcu_link_process_bytes(link, link->rx.pending, link->rx.pending_len, &consumed, out_event, &has_event);
            if (ret != ESP_OK) {
                return ret;
            }

            if (has_event) {
                mcu_link_store_pending_bytes(link, &link->rx.pending[consumed], link->rx.pending_len - consumed);
                return ESP_OK;
            }

            link->rx.pending_len = 0u;
            continue;
        }

        ret = mcu_link_uart_get_buffered_bytes(&buffered);
        if (ret != ESP_OK) {
            return ret;
        }

        if (buffered == 0u) {
            break;
        }

        read_len = 0u;
        ret = mcu_link_uart_read(read_buf, (buffered < sizeof(read_buf)) ? buffered : sizeof(read_buf), 0u, &read_len);
        if (ret != ESP_OK) {
            return ret;
        }

        if (read_len == 0u) {
            break;
        }

        {
            size_t consumed = 0u;
            bool has_event = false;

            ret = mcu_link_process_bytes(link, read_buf, read_len, &consumed, out_event, &has_event);
            if (ret != ESP_OK) {
                return ret;
            }

            if (has_event) {
                mcu_link_store_pending_bytes(link, &read_buf[consumed], read_len - consumed);
                return ESP_OK;
            }
        }
    }

    return ESP_ERR_NOT_FOUND;
}

/* Serialize whole protocol operations, including parser/FSM updates, with UART
 * ownership. Locking only individual driver calls leaves a read/reset race. */
esp_err_t mcu_link_init(mcu_link_t *link) {
    if (!mcu_link_uart_try_lock()) {
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t result = mcu_link_init_locked(link);
    mcu_link_uart_unlock();
    return result;
}

esp_err_t mcu_link_reset(mcu_link_t *link) {
    if (!mcu_link_uart_try_lock()) {
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t result = mcu_link_reset_locked(link);
    mcu_link_uart_unlock();
    return result;
}

esp_err_t mcu_link_begin_handshake(mcu_link_t *link) {
    if (!mcu_link_uart_try_lock()) {
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t result = mcu_link_begin_handshake_locked(link);
    mcu_link_uart_unlock();
    return result;
}

esp_err_t mcu_link_on_hello_rsp(mcu_link_t *link, bool snapshot_supported) {
    if (!mcu_link_uart_try_lock()) {
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t result = mcu_link_on_hello_rsp_locked(link, snapshot_supported);
    mcu_link_uart_unlock();
    return result;
}

esp_err_t mcu_link_mark_baseline_synced(mcu_link_t *link) {
    if (!mcu_link_uart_try_lock()) {
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t result = mcu_link_mark_baseline_synced_locked(link);
    mcu_link_uart_unlock();
    return result;
}

esp_err_t mcu_link_mark_degraded(mcu_link_t *link) {
    if (!mcu_link_uart_try_lock()) {
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t result = mcu_link_mark_degraded_locked(link);
    mcu_link_uart_unlock();
    return result;
}

esp_err_t mcu_link_begin_recovery(mcu_link_t *link) {
    if (!mcu_link_uart_try_lock()) {
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t result = mcu_link_begin_recovery_locked(link);
    mcu_link_uart_unlock();
    return result;
}

mcu_link_state_t mcu_link_get_state(const mcu_link_t *link) {
    if (!mcu_link_uart_try_lock()) {
        return MCU_LINK_STATE_DOWN;
    }
    mcu_link_state_t result = mcu_link_get_state_locked(link);
    mcu_link_uart_unlock();
    return result;
}

bool mcu_link_is_link_ready(const mcu_link_t *link) {
    if (!mcu_link_uart_try_lock()) {
        return false;
    }
    bool result = mcu_link_is_link_ready_locked(link);
    mcu_link_uart_unlock();
    return result;
}

bool mcu_link_is_ready(const mcu_link_t *link) {
    if (!mcu_link_uart_try_lock()) {
        return false;
    }
    bool result = mcu_link_is_ready_locked(link);
    mcu_link_uart_unlock();
    return result;
}

bool mcu_link_snapshot_supported(const mcu_link_t *link) {
    if (!mcu_link_uart_try_lock()) {
        return false;
    }
    bool result = mcu_link_snapshot_supported_locked(link);
    mcu_link_uart_unlock();
    return result;
}

esp_err_t mcu_link_copy_stats(const mcu_link_t *link, mcu_link_stats_t *out_stats) {
    if (!mcu_link_uart_try_lock()) {
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t result = mcu_link_copy_stats_locked(link, out_stats);
    mcu_link_uart_unlock();
    return result;
}

esp_err_t mcu_link_copy_peer_info(const mcu_link_t *link, mcu_link_peer_info_t *out_info) {
    if (!mcu_link_uart_try_lock()) {
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t result = mcu_link_copy_peer_info_locked(link, out_info);
    mcu_link_uart_unlock();
    return result;
}

esp_err_t mcu_link_record_ack_timeout(mcu_link_t *link) {
    if (!mcu_link_uart_try_lock()) {
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t result = mcu_link_record_ack_timeout_locked(link);
    mcu_link_uart_unlock();
    return result;
}

esp_err_t mcu_link_record_crc_error(mcu_link_t *link) {
    if (!mcu_link_uart_try_lock()) {
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t result = mcu_link_record_crc_error_locked(link);
    mcu_link_uart_unlock();
    return result;
}

esp_err_t mcu_link_record_reconnect(mcu_link_t *link) {
    if (!mcu_link_uart_try_lock()) {
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t result = mcu_link_record_reconnect_locked(link);
    mcu_link_uart_unlock();
    return result;
}

esp_err_t mcu_link_record_dropped_state(mcu_link_t *link) {
    if (!mcu_link_uart_try_lock()) {
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t result = mcu_link_record_dropped_state_locked(link);
    mcu_link_uart_unlock();
    return result;
}

esp_err_t mcu_link_record_motion_done_fault(mcu_link_t *link) {
    if (!mcu_link_uart_try_lock()) {
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t result = mcu_link_record_motion_done_fault_locked(link);
    mcu_link_uart_unlock();
    return result;
}

esp_err_t mcu_link_send_frame(mcu_link_t *link, uint8_t msg_class, uint8_t msg_id, uint8_t flags,
                              const uint8_t *payload, uint16_t payload_len, uint32_t *out_seq, size_t *out_wire_len) {
    if (!mcu_link_uart_try_lock()) {
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t result =
        mcu_link_send_frame_locked(link, msg_class, msg_id, flags, payload, payload_len, out_seq, out_wire_len);
    mcu_link_uart_unlock();
    return result;
}

esp_err_t mcu_link_send_hello_req(mcu_link_t *link, uint32_t *out_seq, size_t *out_wire_len) {
    if (!mcu_link_uart_try_lock()) {
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t result = mcu_link_send_hello_req_locked(link, out_seq, out_wire_len);
    mcu_link_uart_unlock();
    return result;
}

esp_err_t mcu_link_poll(mcu_link_t *link, mcu_link_event_t *out_event) {
    if (!mcu_link_uart_try_lock()) {
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t result = mcu_link_poll_locked(link, out_event);
    mcu_link_uart_unlock();
    return result;
}

