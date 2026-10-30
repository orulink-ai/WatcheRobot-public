/**
 * @file mcu_link_fsm.h
 * @brief Small link-state machine for the coprocessor protocol.
 */

#ifndef MCU_LINK_FSM_H
#define MCU_LINK_FSM_H

#include "esp_err.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    MCU_LINK_STATE_DOWN = 0,
    MCU_LINK_STATE_HANDSHAKING,
    MCU_LINK_STATE_LINK_READY,
    MCU_LINK_STATE_READY,
    MCU_LINK_STATE_DEGRADED,
    MCU_LINK_STATE_RECOVERING,
} mcu_link_state_t;

typedef struct {
    mcu_link_state_t state;
    bool snapshot_supported;
    bool baseline_synced;
    uint32_t last_transition_seq;
} mcu_link_fsm_t;

esp_err_t mcu_link_fsm_init(mcu_link_fsm_t *fsm);
esp_err_t mcu_link_fsm_begin_handshake(mcu_link_fsm_t *fsm);
esp_err_t mcu_link_fsm_on_hello_rsp(mcu_link_fsm_t *fsm, bool snapshot_supported);
esp_err_t mcu_link_fsm_mark_baseline_synced(mcu_link_fsm_t *fsm);
esp_err_t mcu_link_fsm_mark_degraded(mcu_link_fsm_t *fsm);
esp_err_t mcu_link_fsm_begin_recovery(mcu_link_fsm_t *fsm);
mcu_link_state_t mcu_link_fsm_get_state(const mcu_link_fsm_t *fsm);
bool mcu_link_fsm_is_link_ready(const mcu_link_fsm_t *fsm);
bool mcu_link_fsm_is_ready(const mcu_link_fsm_t *fsm);

#ifdef __cplusplus
}
#endif

#endif /* MCU_LINK_FSM_H */
