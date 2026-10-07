#include "mcu_motion_service.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

typedef struct {
    uint8_t msg_class;
    uint8_t msg_id;
    uint8_t flags;
    uint8_t payload[128];
    uint16_t payload_len;
    uint32_t seq;
    size_t wire_len;
    unsigned send_count;
} captured_frame_t;

static mcu_link_t s_link = {0};
static bool s_ready = true;
static captured_frame_t s_captured;
static captured_frame_t s_history[8];
static unsigned s_history_count;
static uint32_t s_frame_gap_ms[8];
static unsigned s_frame_gap_count;
static mcu_motion_servo_feedback_t s_feedback;
static unsigned s_feedback_count;
static void *s_feedback_ctx;
static mcu_motion_lifecycle_event_t s_lifecycle_event;
static unsigned s_lifecycle_count;
static void *s_lifecycle_ctx;

mcu_link_t *mcu_link_bootstrap_get_link(void) {
    return &s_link;
}

bool mcu_link_bootstrap_is_ready(void) {
    return s_ready;
}

esp_err_t mcu_link_send_frame(mcu_link_t *link, uint8_t msg_class, uint8_t msg_id, uint8_t flags,
                              const uint8_t *payload, uint16_t payload_len, uint32_t *out_seq, size_t *out_wire_len) {
    assert(link == &s_link);
    assert(payload_len <= sizeof(s_captured.payload));

    s_captured.msg_class = msg_class;
    s_captured.msg_id = msg_id;
    s_captured.flags = flags;
    s_captured.payload_len = payload_len;
    if (payload_len)
        memcpy(s_captured.payload, payload, payload_len);
    s_captured.seq = 42u;
    s_captured.wire_len = payload_len + 8u;
    s_captured.send_count++;
    if (s_history_count < (sizeof(s_history) / sizeof(s_history[0]))) {
        s_history[s_history_count] = s_captured;
        s_history[s_history_count].send_count = 1u;
        s_history_count++;
    }

    if (out_seq != NULL) {
        *out_seq = (uint32_t)(42u + s_captured.send_count - 1u);
        s_captured.seq = *out_seq;
        if (s_history_count > 0u) {
            s_history[s_history_count - 1u].seq = *out_seq;
        }
    }
    if (out_wire_len != NULL) {
        *out_wire_len = s_captured.wire_len;
    }
    return ESP_OK;
}

static void reset_capture(void) {
    memset(&s_captured, 0, sizeof(s_captured));
    memset(s_history, 0, sizeof(s_history));
    memset(&s_feedback, 0, sizeof(s_feedback));
    s_history_count = 0u;
    memset(s_frame_gap_ms, 0, sizeof(s_frame_gap_ms));
    s_frame_gap_count = 0u;
    s_feedback_count = 0u;
    s_feedback_ctx = NULL;
    memset(&s_lifecycle_event, 0, sizeof(s_lifecycle_event));
    s_lifecycle_count = 0u;
    s_lifecycle_ctx = NULL;
    s_ready = true;
    assert(mcu_motion_set_servo_feedback_callback(NULL, NULL) == ESP_OK);
    assert(mcu_motion_set_lifecycle_callback(NULL, NULL) == ESP_OK);
}

void mcu_motion_test_record_frame_gap(uint32_t delay_ms) {
    if (s_frame_gap_count < (sizeof(s_frame_gap_ms) / sizeof(s_frame_gap_ms[0]))) {
        s_frame_gap_ms[s_frame_gap_count] = delay_ms;
    }
    s_frame_gap_count++;
}

static void capture_feedback(const mcu_motion_servo_feedback_t *feedback, void *ctx) {
    assert(feedback != NULL);
    s_feedback = *feedback;
    s_feedback_ctx = ctx;
    s_feedback_count++;
}

static void capture_lifecycle(const mcu_motion_lifecycle_event_t *event, void *ctx) {
    assert(event != NULL);
    s_lifecycle_event = *event;
    s_lifecycle_ctx = ctx;
    s_lifecycle_count++;
}

static void test_motion_lifecycle_callback_preserves_ref_seq_and_terminal_result(void) {
    int sentinel = 0;
    mcu_link_event_t event = {
        .type = MCU_LINK_RX_EVENT_MOTION_DONE,
        .frame =
            {
                .header = {.payload_len = 11u},
                .payload =
                    {
                        0x78u,
                        0x56u,
                        0x34u,
                        0x12u,
                        MCU_MOTION_RESULT_SUCCESS,
                        0xD2u,
                        0x04u,
                        0x37u,
                        0x02u,
                        0xF4u,
                        0x01u,
                    },
            },
    };

    reset_capture();
    assert(mcu_motion_set_lifecycle_callback(capture_lifecycle, &sentinel) == ESP_OK);
    assert(mcu_motion_service_handle_link_event(&event) == ESP_OK);
    assert(s_lifecycle_count == 1u);
    assert(s_lifecycle_ctx == &sentinel);
    assert(s_lifecycle_event.type == MCU_MOTION_LIFECYCLE_DONE);
    assert(s_lifecycle_event.ref_seq == 0x12345678u);
    assert(s_lifecycle_event.result == MCU_MOTION_RESULT_SUCCESS);
    assert(s_lifecycle_event.final_x_deg_x10 == 1234);
    assert(s_lifecycle_event.final_y_deg_x10 == 567);
    assert(s_lifecycle_event.exec_ms == 500u);
}

static void test_motion_lifecycle_callback_reports_rejection(void) {
    mcu_link_event_t event = {
        .type = MCU_LINK_RX_EVENT_NACK,
        .frame =
            {
                .header = {.payload_len = 8u},
                .payload = {0x2Au, 0x00u, 0x00u, 0x00u, MCU_FRAME_CLASS_MOTION, 0x00u, 0x34u, 0x12u},
            },
    };

    reset_capture();
    assert(mcu_motion_service_init() == ESP_OK);
    assert(mcu_motion_stop(MCU_MOTION_SOURCE_WS) == ESP_OK);
    assert(mcu_motion_set_lifecycle_callback(capture_lifecycle, NULL) == ESP_OK);
    assert(mcu_motion_service_handle_link_event(&event) == ESP_OK);
    assert(s_lifecycle_count == 1u);
    assert(s_lifecycle_event.type == MCU_MOTION_LIFECYCLE_REJECTED);
    assert(s_lifecycle_event.ref_seq == 42u);
    assert(s_lifecycle_event.reason == 0x1234u);
}

static void test_motion_lifecycle_ignores_other_services_and_keeps_recent_sequences(void) {
    mcu_link_event_t event = {
        .type = MCU_LINK_RX_EVENT_ACK,
        .frame = {.header = {.payload_len = 6u}, .payload = {0x2Au, 0, 0, 0, 0, 0}},
    };
    reset_capture();
    assert(mcu_motion_service_init() == ESP_OK);
    assert(mcu_motion_stop(MCU_MOTION_SOURCE_WS) == ESP_OK); /* sequence 42 */
    assert(mcu_motion_stop(MCU_MOTION_SOURCE_WS) == ESP_OK); /* sequence 43 */
    assert(mcu_motion_set_lifecycle_callback(capture_lifecycle, NULL) == ESP_OK);
    assert(mcu_motion_service_handle_link_event(&event) == ESP_OK);
    assert(s_lifecycle_count == 1u); /* not only the most recent command */
    event.frame.payload[0] = 43;
    assert(mcu_motion_service_handle_link_event(&event) == ESP_OK);
    assert(s_lifecycle_count == 2u);
    event.frame.payload[0] = 44; /* HELLO / LED sequence, never submitted here */
    assert(mcu_motion_service_handle_link_event(&event) == ESP_OK);
    assert(s_lifecycle_count == 2u);
    event.type = MCU_LINK_RX_EVENT_NACK;
    event.frame.header.payload_len = 8;
    assert(mcu_motion_service_handle_link_event(&event) == ESP_OK);
    assert(s_lifecycle_count == 2u);
    assert(mcu_motion_service_init() == ESP_OK);
    event.frame.payload[0] = 42; /* previous runtime must not retain history */
    assert(mcu_motion_service_handle_link_event(&event) == ESP_OK);
    assert(s_lifecycle_count == 2u);
}

static void test_stop_clears_pending_motion_queue(void) {
    reset_capture();

    assert(mcu_motion_stop(MCU_MOTION_SOURCE_WS) == ESP_OK);
    assert(s_captured.send_count == 1u);
    assert(s_captured.msg_class == MCU_FRAME_CLASS_MOTION);
    assert(s_captured.msg_id == MCU_MOTION_MSG_SERVO_STOP);
    assert(s_captured.flags == MCU_FRAME_FLAG_ACK_REQ);
    assert(s_captured.payload_len == 2u);
    assert(s_captured.payload[0] == 1u);
    assert(s_captured.payload[1] == (uint8_t)MCU_MOTION_SOURCE_WS);
}

static void test_sequence_encodes_segments_into_single_frame(void) {
    uint32_t seq = 0u;
    mcu_motion_sequence_t sequence = {
        .source = MCU_MOTION_SOURCE_BEHAVIOR,
        .segment_count = 2u,
        .segments =
            {
                {
                    .axis_mask = MCU_MOTION_AXIS_X | MCU_MOTION_AXIS_Y,
                    .x_deg_x10 = 1000,
                    .y_deg_x10 = 1200,
                    .duration_ms = 50u,
                    .motion_profile = MCU_MOTION_PROFILE_LINEAR,
                },
                {
                    .axis_mask = MCU_MOTION_AXIS_X | MCU_MOTION_AXIS_Y,
                    .x_deg_x10 = 800,
                    .y_deg_x10 = 900,
                    .duration_ms = 70u,
                    .motion_profile = MCU_MOTION_PROFILE_EASE_IN_OUT,
                },
            },
    };

    reset_capture();

    assert(mcu_motion_submit_sequence_with_seq(&sequence, &seq) == ESP_OK);
    assert(seq == 42u);
    assert(s_captured.send_count == 1u);
    assert(s_captured.msg_class == MCU_FRAME_CLASS_MOTION);
    assert(s_captured.msg_id == MCU_MOTION_MSG_SERVO_SEQUENCE);
    assert(s_captured.flags == MCU_FRAME_FLAG_ACK_REQ);
    assert(s_captured.payload_len == 18u);
    assert(s_captured.payload[0] == (uint8_t)MCU_MOTION_SOURCE_BEHAVIOR);
    assert(s_captured.payload[1] == 2u);
    assert(s_captured.payload[2] == (MCU_MOTION_AXIS_X | MCU_MOTION_AXIS_Y));
    assert(s_captured.payload[3] == 0xE8u);
    assert(s_captured.payload[4] == 0x03u);
    assert(s_captured.payload[5] == 0xB0u);
    assert(s_captured.payload[6] == 0x04u);
    assert(s_captured.payload[7] == 0x32u);
    assert(s_captured.payload[8] == 0x00u);
    assert(s_captured.payload[9] == MCU_MOTION_PROFILE_LINEAR);
    assert(s_captured.payload[10] == (MCU_MOTION_AXIS_X | MCU_MOTION_AXIS_Y));
    assert(s_captured.payload[11] == 0x20u);
    assert(s_captured.payload[12] == 0x03u);
    assert(s_captured.payload[13] == 0x84u);
    assert(s_captured.payload[14] == 0x03u);
    assert(s_captured.payload[15] == 0x46u);
    assert(s_captured.payload[16] == 0x00u);
    assert(s_captured.payload[17] == MCU_MOTION_PROFILE_EASE_IN_OUT);
}

static void test_direct_target_encodes_axis_angle_and_source(void) {
    uint32_t seq = 0u;
    mcu_motion_direct_target_t request = {
        .axis_mask = MCU_MOTION_AXIS_X | MCU_MOTION_AXIS_Y,
        .x_deg_x10 = 1230,
        .y_deg_x10 = 450,
        .source = MCU_MOTION_SOURCE_WS,
    };

    reset_capture();

    assert(mcu_motion_submit_direct_target_with_seq(&request, &seq) == ESP_OK);
    assert(seq == 42u);
    assert(s_captured.send_count == 1u);
    assert(s_captured.msg_class == MCU_FRAME_CLASS_MOTION);
    assert(s_captured.msg_id == MCU_MOTION_MSG_SERVO_DIRECT_TARGET);
    assert(s_captured.flags == 0u);
    assert(s_captured.payload_len == 6u);
    assert(s_captured.payload[0] == (MCU_MOTION_AXIS_X | MCU_MOTION_AXIS_Y));
    assert(s_captured.payload[1] == 0xCEu);
    assert(s_captured.payload[2] == 0x04u);
    assert(s_captured.payload[3] == 0xC2u);
    assert(s_captured.payload[4] == 0x01u);
    assert(s_captured.payload[5] == (uint8_t)MCU_MOTION_SOURCE_WS);
}

static void test_sequence_rejects_invalid_segment_without_send(void) {
    mcu_motion_sequence_t sequence = {
        .source = MCU_MOTION_SOURCE_BEHAVIOR,
        .segment_count = 1u,
        .segments =
            {
                {
                    .axis_mask = MCU_MOTION_AXIS_X,
                    .x_deg_x10 = 1900,
                    .duration_ms = 50u,
                    .motion_profile = MCU_MOTION_PROFILE_LINEAR,
                },
            },
    };

    reset_capture();

    assert(mcu_motion_submit_sequence(&sequence) == ESP_ERR_INVALID_ARG);
    assert(s_captured.send_count == 0u);
}

static void test_sequence_accepts_max_single_frame_segments(void) {
    mcu_motion_sequence_t sequence = {
        .source = MCU_MOTION_SOURCE_BEHAVIOR,
        .segment_count = MCU_MOTION_SEQUENCE_MAX_SEGMENTS,
    };

    reset_capture();
    for (uint8_t index = 0u; index < MCU_MOTION_SEQUENCE_MAX_SEGMENTS; index++) {
        sequence.segments[index].axis_mask = MCU_MOTION_AXIS_X | MCU_MOTION_AXIS_Y;
        sequence.segments[index].x_deg_x10 = 900;
        sequence.segments[index].y_deg_x10 = 900;
        sequence.segments[index].duration_ms = 1u;
        sequence.segments[index].motion_profile = MCU_MOTION_PROFILE_LINEAR;
    }

    assert(mcu_motion_submit_sequence(&sequence) == ESP_OK);
    assert(s_captured.send_count == 1u);
    assert(s_captured.msg_id == MCU_MOTION_MSG_SERVO_SEQUENCE);
    assert(s_captured.payload_len == (uint16_t)(2u + (MCU_MOTION_SEQUENCE_MAX_SEGMENTS * 8u)));
    assert(s_captured.payload_len <= 128u);
    assert(s_captured.payload[1] == MCU_MOTION_SEQUENCE_MAX_SEGMENTS);
}

static void test_chunked_sequence_sends_begin_chunks_and_end(void) {
    uint32_t end_seq = 0u;
    mcu_motion_chunked_sequence_t sequence = {
        .sequence_id = 0x1234u,
        .source = MCU_MOTION_SOURCE_BEHAVIOR,
        .segment_count = 16u,
    };

    reset_capture();
    for (uint8_t index = 0u; index < sequence.segment_count; index++) {
        sequence.segments[index].axis_mask = MCU_MOTION_AXIS_X | MCU_MOTION_AXIS_Y;
        sequence.segments[index].x_deg_x10 = 900;
        sequence.segments[index].y_deg_x10 = 900;
        sequence.segments[index].duration_ms = 1u;
        sequence.segments[index].motion_profile = MCU_MOTION_PROFILE_LINEAR;
    }

    assert(mcu_motion_submit_chunked_sequence_with_seq(&sequence, &end_seq) == ESP_OK);
    assert(end_seq == 45u);
    assert(s_captured.send_count == 4u);
    assert(s_history_count == 4u);
    assert(s_history[0].msg_id == MCU_MOTION_MSG_SERVO_SEQUENCE_BEGIN);
    assert(s_history[0].payload_len == 4u);
    assert(s_history[0].payload[0] == 0x34u);
    assert(s_history[0].payload[1] == 0x12u);
    assert(s_history[0].payload[2] == (uint8_t)MCU_MOTION_SOURCE_BEHAVIOR);
    assert(s_history[0].payload[3] == 16u);
    assert(s_history[1].msg_id == MCU_MOTION_MSG_SERVO_SEQUENCE_CHUNK);
    assert(s_history[1].payload_len == 124u);
    assert(s_history[1].payload[2] == 0u);
    assert(s_history[1].payload[3] == MCU_MOTION_SEQUENCE_CHUNK_MAX_SEGMENTS);
    assert(s_history[2].msg_id == MCU_MOTION_MSG_SERVO_SEQUENCE_CHUNK);
    assert(s_history[2].payload_len == 12u);
    assert(s_history[2].payload[2] == MCU_MOTION_SEQUENCE_CHUNK_MAX_SEGMENTS);
    assert(s_history[2].payload[3] == 1u);
    assert(s_history[3].msg_id == MCU_MOTION_MSG_SERVO_SEQUENCE_END);
    assert(s_history[3].payload_len == 2u);
    assert(s_history[3].payload[0] == 0x34u);
    assert(s_history[3].payload[1] == 0x12u);
    assert(s_frame_gap_count == 3u);
    assert(s_frame_gap_ms[0] >= 40u);
    assert(s_frame_gap_ms[1] >= 40u);
    assert(s_frame_gap_ms[2] >= 40u);
}

static void test_motion_state_feedback_uses_stm32_payload_layout(void) {
    int sentinel = 0;
    mcu_link_event_t event = {
        .type = MCU_LINK_RX_EVENT_SERVO_FEEDBACK,
        .frame =
            {
                .header =
                    {
                        .payload_len = 13u,
                        .msg_id = MCU_MOTION_MSG_MOTION_STATE,
                    },
                .payload =
                    {
                        0x78u,
                        0x56u,
                        0x34u,
                        0x12u,
                        MCU_MOTION_AXIS_X | MCU_MOTION_AXIS_Y,
                        0xD2u,
                        0x04u,
                        0x37u,
                        0x02u,
                        0xDCu,
                        0x05u,
                        0xC4u,
                        0x09u,
                    },
            },
    };

    reset_capture();

    assert(mcu_motion_set_servo_feedback_callback(capture_feedback, &sentinel) == ESP_OK);
    assert(mcu_motion_service_handle_link_event(&event) == ESP_OK);
    assert(s_feedback_count == 1u);
    assert(s_feedback_ctx == &sentinel);
    assert(s_feedback.axis_mask == (MCU_MOTION_AXIS_X | MCU_MOTION_AXIS_Y));
    assert(s_feedback.x_angle_x10 == 1234);
    assert(s_feedback.y_angle_x10 == 567);
    assert(s_feedback.x_raw == 2500u);
    assert(s_feedback.y_raw == 1500u);
}

static void test_servo_feedback_rsp_uses_compact_payload_layout(void) {
    int sentinel = 0;
    mcu_link_event_t event = {
        .type = MCU_LINK_RX_EVENT_SERVO_FEEDBACK,
        .frame =
            {
                .header =
                    {
                        .payload_len = 9u,
                        .msg_id = MCU_MOTION_MSG_SERVO_FEEDBACK,
                    },
                .payload =
                    {
                        MCU_MOTION_AXIS_X | MCU_MOTION_AXIS_Y,
                        0x57u,
                        0x04u,
                        0xDEu,
                        0x00u,
                        0x05u,
                        0x0Du,
                        0xBCu,
                        0x01u,
                    },
            },
    };

    reset_capture();

    assert(mcu_motion_set_servo_feedback_callback(capture_feedback, &sentinel) == ESP_OK);
    assert(mcu_motion_service_handle_link_event(&event) == ESP_OK);
    assert(s_feedback_count == 1u);
    assert(s_feedback_ctx == &sentinel);
    assert(s_feedback.axis_mask == (MCU_MOTION_AXIS_X | MCU_MOTION_AXIS_Y));
    assert(s_feedback.x_angle_x10 == 444);
    assert(s_feedback.y_angle_x10 == 222);
    assert(s_feedback.x_raw == 3333u);
    assert(s_feedback.y_raw == 1111u);
}

int main(void) {
    reset_capture();
    s_ready = false;
    assert(mcu_motion_request_feedback() == ESP_ERR_INVALID_STATE);
    assert(s_captured.send_count == 0);
    s_ready = true;
    assert(mcu_motion_request_feedback() == ESP_OK);
    assert(s_captured.msg_id == MCU_MOTION_MSG_SERVO_FEEDBACK_REQ);
    assert(s_captured.payload_len == 0);
    test_stop_clears_pending_motion_queue();
    test_sequence_encodes_segments_into_single_frame();
    test_direct_target_encodes_axis_angle_and_source();
    test_sequence_rejects_invalid_segment_without_send();
    test_sequence_accepts_max_single_frame_segments();
    test_chunked_sequence_sends_begin_chunks_and_end();
    test_motion_lifecycle_callback_preserves_ref_seq_and_terminal_result();
    test_motion_lifecycle_callback_reports_rejection();
    test_motion_lifecycle_ignores_other_services_and_keeps_recent_sequences();
    test_motion_state_feedback_uses_stm32_payload_layout();
    test_servo_feedback_rsp_uses_compact_payload_layout();
    return 0;
}
