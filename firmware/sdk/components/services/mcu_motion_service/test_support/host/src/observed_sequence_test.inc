/* Before the API exists, use the legacy submission-only completion contract. */
#ifndef MCU_MOTION_COMPLETION_API
#define MCU_MOTION_COMPLETION_PENDING 0
#define MCU_MOTION_COMPLETION_COMPLETED 1
#define MCU_MOTION_COMPLETION_FAILED 2
typedef int mcu_motion_completion_status_t;
static esp_err_t mcu_motion_submit_observed_sequence(const mcu_motion_chunked_sequence_t *s, uint32_t *token) {
    *token = 1; return mcu_motion_submit_chunked_sequence(s);
}
static esp_err_t mcu_motion_sequence_status(uint32_t token, int *status) {
    (void)token; *status = MCU_MOTION_COMPLETION_COMPLETED; return ESP_OK;
}
static void mcu_motion_sequence_release(uint32_t token) { (void)token; }
#endif
static void observed_event(mcu_link_rx_event_type_t type, uint32_t ref, uint32_t rx, uint8_t result) {
    mcu_link_event_t e = {.type = type, .frame = {.header = {.payload_len = 11u, .seq = rx}}};
    for (unsigned i = 0; i < 4; i++) e.frame.payload[i] = (uint8_t)(ref >> (8*i));
    e.frame.payload[4] = result;
    assert(mcu_motion_service_handle_link_event(&e) == ESP_OK);
}
static void test_late_face_done_origin(unsigned invalidation) {
    reset_capture();
    assert(mcu_motion_service_init() == ESP_OK);
    mcu_motion_chunked_sequence_t action = {.sequence_id=31,
        .source=MCU_MOTION_SOURCE_FACE_TRACKING, .segment_count=1,
        .segments={{.axis_mask=3,.x_deg_x10=1200,.y_deg_x10=1100,.duration_ms=100}}};
    uint32_t motion, stop;
    int16_t x, y;
    mcu_motion_completion_status_t status;
    assert(mcu_motion_submit_observed_sequence(&action, &motion) == ESP_OK);
    uint32_t chunk = s_history[1].seq;
    if (invalidation == 0 || invalidation == 3) {
        /* Completion can arrive before B returns from its send call. */
        if (invalidation == 3) s_done_inside_send_ref = chunk;
        mcu_motion_direct_target_t other = {.source=MCU_MOTION_SOURCE_WS,
            .axis_mask=3,.x_deg_x10=900,.y_deg_x10=1100};
        assert(mcu_motion_submit_direct_target(&other) == ESP_OK);
    } else if (invalidation == 1) {
        mcu_link_event_t hello = {.type=MCU_LINK_RX_EVENT_HELLO_RSP};
        assert(mcu_motion_service_handle_link_event(&hello) == ESP_OK);
    } else {
        /* Unrelated fault retires position, not A's completion contract. */
        observed_event(MCU_LINK_RX_EVENT_FAULT, 99999, 900, 1);
    }
    observed_event(MCU_LINK_RX_EVENT_MOTION_DONE, chunk, 901, MCU_MOTION_RESULT_SUCCESS);
    assert(mcu_motion_sequence_status(motion, &status) == ESP_OK);
    assert(status == (invalidation == 1 ? MCU_MOTION_COMPLETION_FAILED : MCU_MOTION_COMPLETION_COMPLETED));
    assert(mcu_motion_submit_observed_stop(MCU_MOTION_SOURCE_FACE_TRACKING, &stop) == ESP_OK);
    observed_event(MCU_LINK_RX_EVENT_ACK, s_captured.seq, 902, 0);
    assert(mcu_motion_stop_control_origin(stop, &x, &y) == ESP_ERR_NOT_FOUND);
    mcu_motion_stop_release(stop);
    mcu_motion_sequence_release(motion);
}

static void test_stop_history_retains_32_commands(void) {
    reset_capture();
    assert(mcu_motion_service_init() == ESP_OK);
    mcu_motion_request_t move = {.source=MCU_MOTION_SOURCE_WS,.axis_mask=3,
        .x_deg_x10=900,.y_deg_x10=1100,.duration_ms=100};
    uint32_t ref;
    assert(mcu_motion_submit_with_seq(&move, &ref) == ESP_OK);
    assert(mcu_motion_set_lifecycle_callback(capture_lifecycle, NULL) == ESP_OK);
    for (unsigned i=0; i<16; ++i)
        assert(mcu_motion_stop(MCU_MOTION_SOURCE_WS) == ESP_OK);
    observed_event(MCU_LINK_RX_EVENT_ACK, ref, 950, 0);
    assert(s_lifecycle_count == 1 && s_lifecycle_event.ref_seq == ref);
    for (unsigned i=16; i<31; ++i)
        assert(mcu_motion_stop(MCU_MOTION_SOURCE_WS) == ESP_OK);
    observed_event(MCU_LINK_RX_EVENT_ACK, ref, 951, 0);
    assert(s_lifecycle_count == 2);
    assert(mcu_motion_stop(MCU_MOTION_SOURCE_WS) == ESP_OK);
    observed_event(MCU_LINK_RX_EVENT_ACK, ref, 952, 0);
    assert(s_lifecycle_count == 2); /* command 33 evicts the first */
}

static void test_observed_sequence(void) {
    mcu_motion_chunked_sequence_t s = {.sequence_id = 17, .source = MCU_MOTION_SOURCE_BEHAVIOR, .segment_count = 64};
    for (unsigned i = 0; i < 64; i++) s.segments[i] = (mcu_motion_segment_t){.axis_mask=3, .x_deg_x10=900, .y_deg_x10=900, .duration_ms=100};
    reset_capture();
    int sentinel;
    assert(mcu_motion_set_lifecycle_callback(capture_lifecycle, &sentinel) == ESP_OK);
    uint32_t token;
    mcu_motion_completion_status_t status;
    assert(mcu_motion_submit_observed_sequence(&s, &token) == ESP_OK && token != 0);
    assert(mcu_motion_sequence_status(token, &status) == ESP_OK);
    assert(status == MCU_MOTION_COMPLETION_PENDING);
    uint32_t refused = 99;
    assert(mcu_motion_submit_observed_sequence(&s, &refused) == ESP_ERR_INVALID_STATE && refused == 0);
    observed_event(MCU_LINK_RX_EVENT_MOTION_DONE, 99999, 45, 0);
    assert(mcu_motion_sequence_status(token, &status) == ESP_OK && status == MCU_MOTION_COMPLETION_PENDING);
    for (unsigned chunk = 1; chunk <= 5; chunk++) {
        unsigned count = chunk == 5 ? 4 : 15;
        observed_event(MCU_LINK_RX_EVENT_ACK, s_history[chunk].seq, 80+chunk, 0);
        for (unsigned i=0; i<count; i++) {
            uint32_t rx = 100 + chunk*16 + i;
            observed_event(MCU_LINK_RX_EVENT_MOTION_DONE, s_history[chunk].seq, rx, 0);
            observed_event(MCU_LINK_RX_EVENT_MOTION_DONE, s_history[chunk].seq, rx, 0);
            assert(mcu_motion_sequence_status(token, &status) == ESP_OK);
            assert(status == ((chunk==5 && i==3) ? MCU_MOTION_COMPLETION_COMPLETED : MCU_MOTION_COMPLETION_PENDING));
        }
    }
    assert(s_lifecycle_count == 134 && s_lifecycle_ctx == &sentinel);
    mcu_motion_sequence_release(token);
    assert(mcu_motion_sequence_status(token, &status) == ESP_ERR_NOT_FOUND);
    reset_capture();
    assert(mcu_motion_submit_observed_sequence(&s, &token) == ESP_OK);
    observed_event(MCU_LINK_RX_EVENT_MOTION_DONE, s_history[1].seq, 999, MCU_MOTION_RESULT_INTERRUPTED);
    assert(mcu_motion_sequence_status(token, &status) == ESP_OK && status == MCU_MOTION_COMPLETION_FAILED);
    mcu_motion_sequence_release(token);
    /* Events received in the send gaps, before submission returns, survive. */
    reset_capture();
    s.segment_count = 1;
    s_inject_early_done = true;
    assert(mcu_motion_submit_observed_sequence(&s, &token) == ESP_OK);
    s_inject_early_done = false;
    assert(mcu_motion_sequence_status(token, &status) == ESP_OK && status == MCU_MOTION_COMPLETION_COMPLETED);
    mcu_motion_sequence_release(token);
    reset_capture();
    s_inject_early_nack = true;
    assert(mcu_motion_submit_observed_sequence(&s, &token) == ESP_OK);
    s_inject_early_nack = false;
    assert(mcu_motion_sequence_status(token, &status) == ESP_OK && status == MCU_MOTION_COMPLETION_FAILED);
    mcu_motion_sequence_release(token);
    for (unsigned msg = 0; msg < 3; msg++) {
        const uint8_t ids[] = {MCU_MOTION_MSG_SERVO_SEQUENCE_BEGIN, MCU_MOTION_MSG_SERVO_SEQUENCE_CHUNK, MCU_MOTION_MSG_SERVO_SEQUENCE_END};
        reset_capture();
        s_fail_msg_id = ids[msg];
        token = 123;
        assert(mcu_motion_submit_observed_sequence(&s, &token) != ESP_OK && token == 0);
        s_fail_msg_id = 0;
        assert(mcu_motion_submit_observed_sequence(&s, &token) == ESP_OK);
        mcu_motion_sequence_release(token);
    }
    assert(mcu_motion_sequence_status(0, &status) == ESP_OK && status == MCU_MOTION_COMPLETION_COMPLETED);
}
static void test_stop_handoff(void) {
    reset_capture();
    uint32_t token = 0, ack = 0;
    mcu_motion_completion_status_t status;
    assert(mcu_motion_submit_observed_stop(MCU_MOTION_SOURCE_FACE_TRACKING, &token) == ESP_OK);
    assert(s_captured.payload[1] == MCU_MOTION_SOURCE_UNKNOWN);
    assert(mcu_motion_stop_status(token, &status, &ack) == ESP_OK && status == MCU_MOTION_COMPLETION_PENDING);
    observed_event(MCU_LINK_RX_EVENT_ACK, 99999, 800, 0);
    assert(mcu_motion_stop_status(token, &status, &ack) == ESP_OK && status == MCU_MOTION_COMPLETION_PENDING);
    observed_event(MCU_LINK_RX_EVENT_ACK, s_captured.seq, 801, 0);
    assert(mcu_motion_stop_status(token, &status, &ack) == ESP_OK && status == MCU_MOTION_COMPLETION_COMPLETED && ack == 801);
    mcu_motion_stop_release(token);
    assert(mcu_motion_stop_status(token, &status, &ack) == ESP_ERR_NOT_FOUND);
    assert(mcu_motion_submit_observed_stop(MCU_MOTION_SOURCE_FACE_TRACKING, &token) == ESP_OK);
    observed_event(MCU_LINK_RX_EVENT_NACK, s_captured.seq, 802, 0);
    assert(mcu_motion_stop_status(token, &status, &ack) == ESP_OK && status == MCU_MOTION_COMPLETION_FAILED);
    mcu_motion_stop_release(token);
    mcu_motion_chunked_sequence_t relative = {.sequence_id=22, .source=MCU_MOTION_SOURCE_FACE_TRACKING, .segment_count=1,
        .segments={{.axis_mask=3, .x_deg_x10=900, .y_deg_x10=1080, .duration_ms=300}}};
    reset_capture();
    assert(mcu_motion_submit_observed_sequence(&relative, &token) == ESP_OK);
    assert(s_history[0].payload[2] == MCU_MOTION_SOURCE_UNKNOWN);
    mcu_motion_sequence_release(token);

}
static void test_stopped_jog_control_origin(void) {
    reset_capture();
    assert(mcu_motion_service_init() == ESP_OK);
    mcu_motion_jog_request_t jog = {.source=MCU_MOTION_SOURCE_FACE_TRACKING, .axis_mask=3,
        .x_velocity_deg_x10_per_sec=100, .y_velocity_deg_x10_per_sec=100, .timeout_ms=200,
        .x_min_deg=30,.x_max_deg=150,.y_min_deg=100,.y_max_deg=130};
    uint32_t ref, token; int16_t x,y;
    assert(mcu_motion_jog_with_seq(&jog,&ref) == ESP_OK);
    assert(mcu_motion_submit_observed_stop(MCU_MOTION_SOURCE_FACE_TRACKING,&token) == ESP_OK);
    uint32_t stop_ref=s_captured.seq;
    assert(mcu_motion_stop_control_origin(token,&x,&y) != ESP_OK);
    observed_event(MCU_LINK_RX_EVENT_ACK,stop_ref,100,0);
    assert(mcu_motion_stop_control_origin(token,&x,&y) != ESP_OK);
    mcu_link_event_t done={.type=MCU_LINK_RX_EVENT_MOTION_DONE,
        .frame={.header={.payload_len=11,.seq=101}}};
    for(unsigned i=0;i<4;i++)done.frame.payload[i]=(uint8_t)(ref>>(i*8));
    done.frame.payload[4]=MCU_MOTION_RESULT_STOPPED;
    done.frame.payload[5]=0xb0;done.frame.payload[6]=4; /* 1200 */
    done.frame.payload[7]=0x4c;done.frame.payload[8]=4; /* 1100 */
    done.frame.payload[0]++; /* unrelated/stale DONE cannot supply origin */
    assert(mcu_motion_service_handle_link_event(&done)==ESP_OK);
    assert(mcu_motion_stop_control_origin(token,&x,&y)!=ESP_OK);
    done.frame.payload[0]--;
    assert(mcu_motion_service_handle_link_event(&done)==ESP_OK);
    assert(mcu_motion_stop_control_origin(token,&x,&y)==ESP_OK && x==1200 && y==1100);
    mcu_motion_stop_release(token);
    assert(mcu_motion_stop_control_origin(token,&x,&y)!=ESP_OK);
    /* A watchdog-completed jog remains usable if nothing else has moved. */
    assert(mcu_motion_submit_observed_stop(MCU_MOTION_SOURCE_FACE_TRACKING,&token)==ESP_OK);
    observed_event(MCU_LINK_RX_EVENT_ACK,s_captured.seq,102,0);
    assert(mcu_motion_stop_control_origin(token,&x,&y)==ESP_OK);
    mcu_motion_stop_release(token);
    /* A new jog must not reuse the previous position. */
    assert(mcu_motion_jog_with_seq(&jog,&ref)==ESP_OK);
    assert(mcu_motion_submit_observed_stop(MCU_MOTION_SOURCE_FACE_TRACKING,&token)==ESP_OK);
    observed_event(MCU_LINK_RX_EVENT_ACK,s_captured.seq,103,0);
    assert(mcu_motion_stop_control_origin(token,&x,&y)!=ESP_OK);
    mcu_motion_stop_release(token);
    /* Completed relative actions also provide a confirmed origin, even if
     * a centred face has not required a new jog between commands. */
    mcu_motion_chunked_sequence_t action={.sequence_id=23,
        .source=MCU_MOTION_SOURCE_FACE_TRACKING,.segment_count=1,
        .segments={{.axis_mask=3,.x_deg_x10=1200,.y_deg_x10=1100,.duration_ms=100}}};
    uint32_t motion;
    reset_capture();
    assert(mcu_motion_submit_observed_sequence(&action,&motion)==ESP_OK);
    uint32_t chunk=s_history[1].seq;
    for(unsigned i=0;i<4;i++)done.frame.payload[i]=(uint8_t)(chunk>>(i*8));
    done.frame.header.seq=200;done.frame.payload[4]=MCU_MOTION_RESULT_SUCCESS;
    assert(mcu_motion_service_handle_link_event(&done)==ESP_OK);
    mcu_motion_sequence_release(motion);
    assert(mcu_motion_submit_observed_stop(MCU_MOTION_SOURCE_FACE_TRACKING,&token)==ESP_OK);
    observed_event(MCU_LINK_RX_EVENT_ACK,s_captured.seq,201,0);
    assert(mcu_motion_stop_control_origin(token,&x,&y)==ESP_OK && x==1200 && y==1100);
    mcu_link_event_t hello={.type=MCU_LINK_RX_EVENT_HELLO_RSP};
    assert(mcu_motion_service_handle_link_event(&hello)==ESP_OK);
    assert(mcu_motion_stop_control_origin(token,&x,&y)!=ESP_OK);
    mcu_motion_stop_release(token);
    /* A new completed action restores position after link recovery. */
    reset_capture();
    assert(mcu_motion_submit_observed_sequence(&action,&motion)==ESP_OK);
    assert(mcu_motion_service_handle_link_event(&done)==ESP_OK);
    mcu_motion_sequence_release(motion);
    assert(mcu_motion_submit_observed_stop(MCU_MOTION_SOURCE_FACE_TRACKING,&token)==ESP_OK);
    observed_event(MCU_LINK_RX_EVENT_ACK,s_captured.seq,201,0);
    assert(mcu_motion_stop_control_origin(token,&x,&y)==ESP_OK);
    /* Other producers retire the confirmed position. */
    mcu_motion_direct_target_t other={.source=MCU_MOTION_SOURCE_WS,.axis_mask=3,
        .x_deg_x10=900,.y_deg_x10=1100};
    assert(mcu_motion_submit_direct_target(&other)==ESP_OK);
    assert(mcu_motion_stop_control_origin(token,&x,&y)!=ESP_OK);
    mcu_motion_stop_release(token);
    /* Faults invalidate even previously confirmed stopped positions. */
    assert(mcu_motion_jog_with_seq(&jog,&ref)==ESP_OK);
    for(unsigned i=0;i<4;i++)done.frame.payload[i]=(uint8_t)(ref>>(i*8));
    done.frame.header.seq=300;done.frame.payload[4]=MCU_MOTION_RESULT_STOPPED;
    assert(mcu_motion_service_handle_link_event(&done)==ESP_OK);
    assert(mcu_motion_submit_observed_stop(MCU_MOTION_SOURCE_FACE_TRACKING,&token)==ESP_OK);
    observed_event(MCU_LINK_RX_EVENT_ACK,s_captured.seq,301,0);
    assert(mcu_motion_stop_control_origin(token,&x,&y)==ESP_OK);
    observed_event(MCU_LINK_RX_EVENT_FAULT,0,302,1);
    assert(mcu_motion_stop_control_origin(token,&x,&y)!=ESP_OK);
    mcu_motion_stop_release(token);
}
