/**
 * @file mcu_link_bootstrap.h
 * @brief App-facing bootstrap helpers for the static MCU link instance.
 */

#ifndef MCU_LINK_BOOTSTRAP_H
#define MCU_LINK_BOOTSTRAP_H

#include "mcu_link.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t mcu_link_bootstrap_init(void);
esp_err_t mcu_link_bootstrap_start(void);
void mcu_link_bootstrap_stop(void);
esp_err_t mcu_link_bootstrap_poll(mcu_link_event_t *out_event);
mcu_link_t *mcu_link_bootstrap_get_link(void);
mcu_link_state_t mcu_link_bootstrap_get_state(void);

/** Temporarily hand the shared UART to the STM32 bootloader transport. */
esp_err_t mcu_link_bootstrap_begin_ota(void);

/** Restore the runtime UART, handshake, and verify the application-reported commit.
 * Must run in the task that successfully called begin_ota. All owner completion
 * paths release the UART, including invalid arguments; foreign tasks cannot
 * terminate another task's OTA session. */
esp_err_t mcu_link_bootstrap_finish_ota(const char *expected_commit, bool expected_dirty, uint32_t timeout_ms,
                                        mcu_link_peer_info_t *out_peer_info);

/** Release OTA ownership after an unrecoverable transfer failure. */
void mcu_link_bootstrap_abort_ota(void);
bool mcu_link_bootstrap_is_link_ready(void);
bool mcu_link_bootstrap_is_ready(void);
bool mcu_link_bootstrap_handshake_timed_out(uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif

#endif /* MCU_LINK_BOOTSTRAP_H */

