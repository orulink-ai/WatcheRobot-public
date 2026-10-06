#include "mcu_link_fsm.h"

static bool is_link_ready_state(mcu_link_state_t state) {
    return state == MCU_LINK_STATE_LINK_READY || state == MCU_LINK_STATE_READY;
}

esp_err_t mcu_link_fsm_init(mcu_link_fsm_t *fsm) {
    if (fsm == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    fsm->state = MCU_LINK_STATE_DOWN;
    fsm->snapshot_supported = false;
    fsm->baseline_synced = false;
    fsm->last_transition_seq = 0u;
    return ESP_OK;
}

esp_err_t mcu_link_fsm_begin_handshake(mcu_link_fsm_t *fsm) {
    if (fsm == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    fsm->state = MCU_LINK_STATE_HANDSHAKING;
    fsm->snapshot_supported = false;
    fsm->baseline_synced = false;
    return ESP_OK;
}

esp_err_t mcu_link_fsm_on_hello_rsp(mcu_link_fsm_t *fsm, bool snapshot_supported) {
    if (fsm == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    fsm->snapshot_supported = snapshot_supported;
    fsm->state = MCU_LINK_STATE_LINK_READY;
    fsm->baseline_synced = false;
    return ESP_OK;
}

esp_err_t mcu_link_fsm_mark_baseline_synced(mcu_link_fsm_t *fsm) {
    if (fsm == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!is_link_ready_state(fsm->state) && fsm->state != MCU_LINK_STATE_RECOVERING) {
        return ESP_ERR_INVALID_STATE;
    }

    fsm->baseline_synced = true;
    fsm->state = MCU_LINK_STATE_READY;
    return ESP_OK;
}

esp_err_t mcu_link_fsm_mark_degraded(mcu_link_fsm_t *fsm) {
    if (fsm == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    fsm->state = MCU_LINK_STATE_DEGRADED;
    fsm->baseline_synced = false;
    return ESP_OK;
}

esp_err_t mcu_link_fsm_begin_recovery(mcu_link_fsm_t *fsm) {
    if (fsm == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    fsm->state = MCU_LINK_STATE_RECOVERING;
    fsm->snapshot_supported = false;
    fsm->baseline_synced = false;
    return ESP_OK;
}

mcu_link_state_t mcu_link_fsm_get_state(const mcu_link_fsm_t *fsm) {
    if (fsm == NULL) {
        return MCU_LINK_STATE_DOWN;
    }

    return fsm->state;
}

bool mcu_link_fsm_is_link_ready(const mcu_link_fsm_t *fsm) {
    return fsm != NULL && is_link_ready_state(fsm->state);
}

bool mcu_link_fsm_is_ready(const mcu_link_fsm_t *fsm) {
    return fsm != NULL && fsm->state == MCU_LINK_STATE_READY && fsm->baseline_synced;
}

