/* Test assertions must execute in Release builds too. */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include "freertos/task.h"
#include "mcu_link.h"
#include "mcu_link_bootstrap.h"
#include "mcu_link_test_support.h"
#include "mcu_link_uart.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#ifdef _MSC_VER
#include <crtdbg.h>
#include <stdlib.h>
#endif

#define ARRAY_SIZE(array) (sizeof(array) / sizeof((array)[0]))
#define TEST_SERVO_AXIS_MASK_X 0x01u
#define TEST_SERVO_AXIS_MASK_Y 0x02u

typedef struct {
    bool ready;
    uint8_t rx_buffer[MCU_FRAME_MAX_WIRE_SIZE * 16u];
    size_t rx_len;
    size_t rx_offset;
    uint8_t tx_buffer[MCU_FRAME_MAX_WIRE_SIZE];
    size_t tx_len;
} fake_uart_state_t;

static fake_uart_state_t s_fake_uart;
static bool s_gate_denied;
static unsigned s_gate_depth;
static int64_t s_fake_time;
static TaskHandle_t s_fake_task = (TaskHandle_t)1;

int64_t esp_timer_get_time(void) {
    return s_fake_time;
}
void vTaskDelay(uint32_t ticks) {
    s_fake_time += (int64_t)ticks * 1000;
}
TaskHandle_t xTaskGetCurrentTaskHandle(void) {
    return s_fake_task;
}
void mcu_link_uart_deinit(void) {
    s_fake_uart.ready = false;
}
esp_err_t mcu_link_uart_wait_tx_done(uint32_t timeout_ms) {
    (void)timeout_ms;
    return ESP_OK;
}
esp_err_t mcu_link_uart_acquire_exclusive(void) {
    return mcu_link_uart_try_lock() ? ESP_OK : ESP_ERR_TIMEOUT;
}
void mcu_link_uart_release_exclusive(void) {
    mcu_link_uart_unlock();
}

bool mcu_link_uart_try_lock(void) {
    if (s_gate_denied) {
        return false;
    }
    ++s_gate_depth;
    return true;
}

void mcu_link_uart_unlock(void) {
    assert(s_gate_depth > 0u);
    --s_gate_depth;
}

static void fake_uart_reset(void) {
    memset(&s_fake_uart, 0, sizeof(s_fake_uart));
    s_fake_uart.ready = true;
}

static void fake_uart_enqueue(const uint8_t *data, size_t data_len) {
    assert(data != NULL);
    assert((s_fake_uart.rx_len + data_len) <= sizeof(s_fake_uart.rx_buffer));
    memcpy(&s_fake_uart.rx_buffer[s_fake_uart.rx_len], data, data_len);
    s_fake_uart.rx_len += data_len;
}

bool mcu_link_uart_is_ready(void) {
    return s_fake_uart.ready;
}

esp_err_t mcu_link_uart_write(const uint8_t *data, size_t data_len, size_t *out_written) {
    if (data == NULL || data_len == 0u) {
        return ESP_ERR_INVALID_ARG;
    }

    assert(data_len <= sizeof(s_fake_uart.tx_buffer));
    memcpy(s_fake_uart.tx_buffer, data, data_len);
    s_fake_uart.tx_len = data_len;

    if (out_written != NULL) {
        *out_written = data_len;
    }

    return ESP_OK;
}

esp_err_t mcu_link_uart_read(uint8_t *buffer, size_t buffer_len, uint32_t timeout_ms, size_t *out_read) {
    size_t available;
    size_t read_len;

    (void)timeout_ms;

    if (buffer == NULL || buffer_len == 0u) {
        return ESP_ERR_INVALID_ARG;
    }

    available = s_fake_uart.rx_len - s_fake_uart.rx_offset;
    read_len = (available < buffer_len) ? available : buffer_len;
    if (read_len > 0u) {
        memcpy(buffer, &s_fake_uart.rx_buffer[s_fake_uart.rx_offset], read_len);
        s_fake_uart.rx_offset += read_len;
        if (s_fake_uart.rx_offset == s_fake_uart.rx_len) {
            s_fake_uart.rx_offset = 0u;
            s_fake_uart.rx_len = 0u;
        }
    }

    if (out_read != NULL) {
        *out_read = read_len;
    }

    return ESP_OK;
}

esp_err_t mcu_link_uart_get_buffered_bytes(size_t *out_bytes) {
    if (out_bytes == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    *out_bytes = s_fake_uart.rx_len - s_fake_uart.rx_offset;
    return ESP_OK;
}

static mcu_link_test_packet_t make_ack_packet(uint32_t seq, uint32_t ref_seq) {
    static const uint16_t status_code = 0u;
    mcu_frame_header_t header;
    uint8_t payload[6];
    mcu_link_test_packet_t packet = {0};

    mcu_frame_header_init(&header, MCU_FRAME_CLASS_SYS, MCU_SYS_MSG_ACK, MCU_FRAME_FLAG_RESPONSE, seq, sizeof(payload));
    payload[0] = (uint8_t)(ref_seq & 0xFFu);
    payload[1] = (uint8_t)((ref_seq >> 8u) & 0xFFu);
    payload[2] = (uint8_t)((ref_seq >> 16u) & 0xFFu);
    payload[3] = (uint8_t)((ref_seq >> 24u) & 0xFFu);
    payload[4] = (uint8_t)(status_code & 0xFFu);
    payload[5] = (uint8_t)((status_code >> 8u) & 0xFFu);

    assert(mcu_link_test_support_make_packet(&header, payload, &packet) == ESP_OK);
    return packet;
}

static mcu_link_test_packet_t make_hello_rsp_packet(uint32_t seq) {
    mcu_frame_header_t header;
    uint8_t payload[MCU_HELLO_BASE_PAYLOAD_LEN] = {
        0x01u,
        0x00u,
        0x01u,
        0x00u,
        0x01u,
        MCU_CAPABILITY_MOTION | MCU_CAPABILITY_LED | MCU_CAPABILITY_TOUCH | MCU_CAPABILITY_RUNTIME_SNAPSHOT |
            MCU_CAPABILITY_POWER,
        MCU_SENSOR_TOUCH,
        0x00u,
        MCU_DEFAULT_STREAM_PROFILE_V1,
    };
    mcu_link_test_packet_t packet = {0};

    mcu_frame_header_init(&header, MCU_FRAME_CLASS_SYS, MCU_SYS_MSG_HELLO_RSP, MCU_FRAME_FLAG_RESPONSE, seq,
                          sizeof(payload));
    assert(mcu_link_test_support_make_packet(&header, payload, &packet) == ESP_OK);
    return packet;
}

static mcu_link_test_packet_t make_hello_rsp_with_git_packet(uint32_t seq) {
    static const char branch[] = "stm32-dev";
    static const char commit[] = "8c509c5";
    mcu_frame_header_t header;
    uint8_t payload[MCU_FRAME_MAX_PAYLOAD_SIZE] = {
        0x01u,
        0x00u,
        0x01u,
        0x00u,
        0x01u,
        MCU_CAPABILITY_MOTION | MCU_CAPABILITY_LED | MCU_CAPABILITY_TOUCH | MCU_CAPABILITY_POWER,
        MCU_SENSOR_TOUCH,
        0x00u,
        MCU_DEFAULT_STREAM_PROFILE_V1,
    };
    size_t offset = MCU_HELLO_BASE_PAYLOAD_LEN;
    mcu_link_test_packet_t packet = {0};

    payload[offset++] = MCU_HELLO_TLV_GIT_BRANCH;
    payload[offset++] = (uint8_t)(sizeof(branch) - 1u);
    memcpy(&payload[offset], branch, sizeof(branch) - 1u);
    offset += sizeof(branch) - 1u;

    payload[offset++] = MCU_HELLO_TLV_GIT_COMMIT;
    payload[offset++] = (uint8_t)(sizeof(commit) - 1u);
    memcpy(&payload[offset], commit, sizeof(commit) - 1u);
    offset += sizeof(commit) - 1u;

    payload[offset++] = MCU_HELLO_TLV_GIT_DIRTY;
    payload[offset++] = 1u;
    payload[offset++] = 1u;

    mcu_frame_header_init(&header, MCU_FRAME_CLASS_SYS, MCU_SYS_MSG_HELLO_RSP, MCU_FRAME_FLAG_RESPONSE, seq,
                          (uint16_t)offset);
    assert(mcu_link_test_support_make_packet(&header, payload, &packet) == ESP_OK);
    return packet;
}

static mcu_link_test_packet_t make_motion_done_packet(uint32_t seq, uint32_t ref_seq) {
    mcu_frame_header_t header;
    uint8_t payload[11] = {0};
    mcu_link_test_packet_t packet = {0};

    mcu_frame_header_init(&header, MCU_FRAME_CLASS_MOTION, MCU_MOTION_MSG_MOTION_DONE, MCU_FRAME_FLAG_FINAL, seq,
                          sizeof(payload));
    payload[0] = (uint8_t)(ref_seq & 0xFFu);
    payload[1] = (uint8_t)((ref_seq >> 8u) & 0xFFu);
    payload[2] = (uint8_t)((ref_seq >> 16u) & 0xFFu);
    payload[3] = (uint8_t)((ref_seq >> 24u) & 0xFFu);
    payload[4] = 0x00u;
    payload[5] = 0x84u;
    payload[6] = 0x03u;
    payload[7] = 0xB0u;
    payload[8] = 0x04u;
    payload[9] = 0xB4u;
    payload[10] = 0x00u;

    assert(mcu_link_test_support_make_packet(&header, payload, &packet) == ESP_OK);
    return packet;
}

static mcu_link_test_packet_t make_servo_feedback_packet(uint32_t seq) {
    mcu_frame_header_t header;
    uint8_t payload[9] = {
        TEST_SERVO_AXIS_MASK_X | TEST_SERVO_AXIS_MASK_Y, 0x57u, 0x04u, 0xDEu, 0x00u, 0x05u, 0x0Du, 0xBCu, 0x01u,
    };
    mcu_link_test_packet_t packet = {0};

    mcu_frame_header_init(&header, MCU_FRAME_CLASS_MOTION, MCU_MOTION_MSG_SERVO_FEEDBACK, MCU_FRAME_FLAG_RESPONSE, seq,
                          sizeof(payload));
    assert(mcu_link_test_support_make_packet(&header, payload, &packet) == ESP_OK);
    return packet;
}

static mcu_link_test_packet_t make_imu_packet(uint32_t seq) {
    mcu_frame_header_t header;
    uint8_t payload[11] = {0};
    mcu_link_test_packet_t packet = {0};

    mcu_frame_header_init(&header, MCU_FRAME_CLASS_SENSOR, MCU_SENSOR_MSG_IMU_STATE, 0u, seq, sizeof(payload));
    payload[0] = 0x0Au;
    payload[1] = 0x00u;
    payload[2] = 0x9Bu;
    payload[3] = 0x04u;
    payload[4] = 0xB8u;
    payload[5] = 0x0Bu;
    payload[6] = 0xD4u;
    payload[7] = 0x03u;
    payload[8] = 0x78u;
    payload[9] = 0x00u;
    payload[10] = 0x01u;

    assert(mcu_link_test_support_make_packet(&header, payload, &packet) == ESP_OK);
    return packet;
}

static void expect_stats(const mcu_link_t *link, uint32_t crc_error_count) {
    mcu_link_stats_t stats = {0};

    assert(mcu_link_copy_stats(link, &stats) == ESP_OK);
    assert(stats.crc_error_count == crc_error_count);
}

static void test_poll_decodes_single_ack_frame(void) {
    mcu_link_t link = {0};
    mcu_link_event_t event = {0};
    const mcu_link_test_packet_t ack = make_ack_packet(3u, 1u);

    fake_uart_reset();
    fake_uart_enqueue(ack.wire, ack.wire_len);

    assert(mcu_link_init(&link) == ESP_OK);
    assert(mcu_link_begin_handshake(&link) == ESP_OK);
    assert(mcu_link_poll(&link, &event) == ESP_OK);
    assert(event.type == MCU_LINK_RX_EVENT_ACK);
    assert(event.frame.header.seq == 3u);
    assert(event.frame.header.payload_len == 6u);
    assert(event.frame.payload[0] == 0x01u);
    expect_stats(&link, 0u);
}

static void test_poll_keeps_second_frame_from_single_uart_read(void) {
    mcu_link_t link = {0};
    mcu_link_event_t event = {0};
    const mcu_link_test_packet_t ack = make_ack_packet(7u, 5u);
    const mcu_link_test_packet_t hello_rsp = make_hello_rsp_packet(8u);

    fake_uart_reset();
    fake_uart_enqueue(ack.wire, ack.wire_len);
    fake_uart_enqueue(hello_rsp.wire, hello_rsp.wire_len);

    assert(mcu_link_init(&link) == ESP_OK);
    assert(mcu_link_begin_handshake(&link) == ESP_OK);

    assert(mcu_link_poll(&link, &event) == ESP_OK);
    assert(event.type == MCU_LINK_RX_EVENT_ACK);
    assert(event.frame.header.seq == 7u);
    assert(mcu_link_get_state(&link) == MCU_LINK_STATE_HANDSHAKING);

    memset(&event, 0, sizeof(event));
    assert(mcu_link_poll(&link, &event) == ESP_OK);
    assert(event.type == MCU_LINK_RX_EVENT_HELLO_RSP);
    assert(event.frame.header.seq == 8u);
    assert(mcu_link_get_state(&link) == MCU_LINK_STATE_LINK_READY);
    assert(mcu_link_snapshot_supported(&link));
    expect_stats(&link, 0u);
}

static void test_poll_captures_hello_rsp_git_metadata(void) {
    mcu_link_t link = {0};
    mcu_link_event_t event = {0};
    mcu_link_peer_info_t info = {0};
    const mcu_link_test_packet_t hello_rsp = make_hello_rsp_with_git_packet(21u);

    fake_uart_reset();
    fake_uart_enqueue(hello_rsp.wire, hello_rsp.wire_len);

    assert(mcu_link_init(&link) == ESP_OK);
    assert(mcu_link_begin_handshake(&link) == ESP_OK);
    assert(mcu_link_poll(&link, &event) == ESP_OK);
    assert(event.type == MCU_LINK_RX_EVENT_HELLO_RSP);
    assert(event.frame.header.seq == 21u);
    assert(mcu_link_copy_peer_info(&link, &info) == ESP_OK);
    assert(info.version_valid);
    assert(info.fw_major == 0u);
    assert(info.fw_minor == 1u);
    assert(info.fw_patch == 0u);
    assert(info.hw_version == 1u);
    assert(info.capability_bitmap ==
           (MCU_CAPABILITY_MOTION | MCU_CAPABILITY_LED | MCU_CAPABILITY_TOUCH | MCU_CAPABILITY_POWER));
    assert(info.sensor_bitmap == MCU_SENSOR_TOUCH);
    assert(info.default_stream_profile == MCU_DEFAULT_STREAM_PROFILE_V1);
    assert(info.git_valid);
    assert(info.git_dirty);
    assert(strcmp(info.git_branch, "stm32-dev") == 0);
    assert(strcmp(info.git_commit, "8c509c5") == 0);
    expect_stats(&link, 0u);
}

static void test_poll_keeps_sensor_bitmap_authoritative_over_stream_profile(void) {
    mcu_link_t link = {0};
    mcu_link_event_t event = {0};
    mcu_link_peer_info_t info = {0};
    mcu_frame_header_t header;
    uint8_t payload[MCU_HELLO_BASE_PAYLOAD_LEN] = {
        0x01u, 0x00u, 0x01u,
        0x00u, 0x01u, MCU_CAPABILITY_MOTION | MCU_CAPABILITY_LED | MCU_CAPABILITY_POWER,
        0x00u, 0x00u, MCU_DEFAULT_STREAM_PROFILE_V1,
    };
    mcu_link_test_packet_t hello_rsp = {0};

    mcu_frame_header_init(&header, MCU_FRAME_CLASS_SYS, MCU_SYS_MSG_HELLO_RSP, MCU_FRAME_FLAG_RESPONSE, 22u,
                          sizeof(payload));
    assert(mcu_link_test_support_make_packet(&header, payload, &hello_rsp) == ESP_OK);

    fake_uart_reset();
    fake_uart_enqueue(hello_rsp.wire, hello_rsp.wire_len);

    assert(mcu_link_init(&link) == ESP_OK);
    assert(mcu_link_begin_handshake(&link) == ESP_OK);
    assert(mcu_link_poll(&link, &event) == ESP_OK);
    assert(event.type == MCU_LINK_RX_EVENT_HELLO_RSP);
    assert(mcu_link_is_link_ready(&link));
    assert(mcu_link_copy_peer_info(&link, &info) == ESP_OK);
    assert(info.default_stream_profile == MCU_DEFAULT_STREAM_PROFILE_V1);
    assert(info.sensor_bitmap == 0u);
    assert((info.capability_bitmap & (MCU_CAPABILITY_IMU | MCU_CAPABILITY_MAGNETOMETER)) == 0u);
    expect_stats(&link, 0u);
}

static void test_poll_decodes_interleaved_burst_across_uart_chunks(void) {
    mcu_link_t link = {0};
    mcu_link_event_t event = {0};
    size_t i;

    fake_uart_reset();
    assert(mcu_link_init(&link) == ESP_OK);
    assert(mcu_link_begin_handshake(&link) == ESP_OK);

    for (i = 0u; i < 8u; ++i) {
        const mcu_link_test_packet_t ack = make_ack_packet((uint32_t)(10u + (i * 3u)), (uint32_t)(100u + i));
        const mcu_link_test_packet_t motion_done =
            make_motion_done_packet((uint32_t)(11u + (i * 3u)), (uint32_t)(100u + i));
        const mcu_link_test_packet_t imu = make_imu_packet((uint32_t)(12u + (i * 3u)));

        fake_uart_enqueue(ack.wire, ack.wire_len);
        fake_uart_enqueue(motion_done.wire, motion_done.wire_len);
        fake_uart_enqueue(imu.wire, imu.wire_len);
    }

    for (i = 0u; i < 8u; ++i) {
        assert(mcu_link_poll(&link, &event) == ESP_OK);
        assert(event.type == MCU_LINK_RX_EVENT_ACK);
        assert(event.frame.header.seq == (uint32_t)(10u + (i * 3u)));

        memset(&event, 0, sizeof(event));
        assert(mcu_link_poll(&link, &event) == ESP_OK);
        assert(event.type == MCU_LINK_RX_EVENT_MOTION_DONE);
        assert(event.frame.header.seq == (uint32_t)(11u + (i * 3u)));

        memset(&event, 0, sizeof(event));
        assert(mcu_link_poll(&link, &event) == ESP_OK);
        assert(event.type == MCU_LINK_RX_EVENT_IMU_STATE);
        assert(event.frame.header.seq == (uint32_t)(12u + (i * 3u)));
    }

    expect_stats(&link, 0u);
}

static void test_poll_decodes_compact_servo_feedback_frame(void) {
    mcu_link_t link = {0};
    mcu_link_event_t event = {0};
    const mcu_link_test_packet_t feedback = make_servo_feedback_packet(13u);

    fake_uart_reset();
    fake_uart_enqueue(feedback.wire, feedback.wire_len);

    assert(mcu_link_init(&link) == ESP_OK);
    assert(mcu_link_begin_handshake(&link) == ESP_OK);
    assert(mcu_link_poll(&link, &event) == ESP_OK);
    assert(event.type == MCU_LINK_RX_EVENT_SERVO_FEEDBACK);
    assert(event.frame.header.seq == 13u);
    assert(event.frame.header.payload_len == 9u);
    assert(event.frame.payload[0] == (TEST_SERVO_AXIS_MASK_X | TEST_SERVO_AXIS_MASK_Y));
    expect_stats(&link, 0u);
}

static void test_bootstrap_recovery_without_application(void) {
    fake_uart_reset();
    assert(mcu_link_bootstrap_init() == ESP_OK);
    assert(!mcu_link_bootstrap_is_link_ready());
    assert(mcu_link_bootstrap_begin_ota() == ESP_OK);
    assert(s_gate_depth == 1u);
    assert(!s_fake_uart.ready);
    assert(mcu_link_bootstrap_get_link() == NULL);
    s_fake_task = (TaskHandle_t)2;
    mcu_link_bootstrap_abort_ota();
    assert(s_gate_depth == 1u);
    s_fake_task = (TaskHandle_t)1;
    mcu_link_bootstrap_abort_ota();
    assert(s_gate_depth == 0u);
    assert(mcu_link_bootstrap_get_link() != NULL);
    assert(mcu_link_bootstrap_begin_ota() == ESP_OK);
    assert(mcu_link_bootstrap_finish_ota(NULL, false, 30u, NULL) == ESP_ERR_INVALID_ARG);
    assert(s_gate_depth == 0u);
    assert(mcu_link_bootstrap_begin_ota() == ESP_OK);
    assert(mcu_link_bootstrap_finish_ota("deadbeef", false, 0u, NULL) == ESP_ERR_INVALID_ARG);
    assert(s_gate_depth == 0u);
    assert(mcu_link_bootstrap_begin_ota() == ESP_OK);
    mcu_link_bootstrap_abort_ota();
    assert(s_gate_depth == 0u);
    assert(mcu_link_bootstrap_begin_ota() == ESP_OK);
    /* This host configuration has no UART reinitialization. Verification must
     * fail and still release ownership, allowing another complete attempt. */
    assert(mcu_link_bootstrap_finish_ota("deadbeef", false, 30u, NULL) == ESP_ERR_INVALID_STATE);
    assert(s_gate_depth == 0u);
    assert(mcu_link_bootstrap_get_link() != NULL);
    assert(mcu_link_bootstrap_begin_ota() == ESP_OK);
    mcu_link_bootstrap_abort_ota();
    assert(s_gate_depth == 0u);
    mcu_link_bootstrap_stop();
}

static void test_exclusive_owner_prevents_runtime_mutation(void) {
    mcu_link_t link = {0};
    mcu_link_event_t event = {0};
    fake_uart_reset();
    assert(mcu_link_init(&link) == ESP_OK);
    link.next_tx_seq = 42u;
    s_gate_denied = true;
    assert(mcu_link_reset(&link) == ESP_ERR_INVALID_STATE);
    assert(mcu_link_send_hello_req(&link, NULL, NULL) == ESP_ERR_INVALID_STATE);
    assert(mcu_link_poll(&link, &event) == ESP_ERR_INVALID_STATE);
    assert(link.next_tx_seq == 42u);
    assert(link.fsm.state == MCU_LINK_STATE_DOWN);
    assert(s_fake_uart.tx_len == 0u);
    s_gate_denied = false;
    assert(mcu_link_send_hello_req(&link, NULL, NULL) == ESP_OK);
    assert(s_gate_depth == 0u);
}

int main(void) {
#ifdef _MSC_VER
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
    const struct {
        const char *name;
        void (*fn)(void);
    } tests[] = {
        {"bootstrap_recovery_without_application", test_bootstrap_recovery_without_application},
        {"exclusive_owner_blocks_mutation", test_exclusive_owner_prevents_runtime_mutation},
        {"single_ack_frame", test_poll_decodes_single_ack_frame},
        {"multi_frame_single_read", test_poll_keeps_second_frame_from_single_uart_read},
        {"hello_rsp_git_metadata", test_poll_captures_hello_rsp_git_metadata},
        {"sensor_bitmap_over_stream_profile", test_poll_keeps_sensor_bitmap_authoritative_over_stream_profile},
        {"interleaved_burst_across_chunks", test_poll_decodes_interleaved_burst_across_uart_chunks},
        {"compact_servo_feedback", test_poll_decodes_compact_servo_feedback_frame},
    };
    size_t i;

    for (i = 0u; i < ARRAY_SIZE(tests); ++i) {
        tests[i].fn();
        printf("[PASS] %s\n", tests[i].name);
    }

    return 0;
}

