#ifndef MCU_LINK_HOST_TEST_UART_H
#define MCU_LINK_HOST_TEST_UART_H

#include "esp_err.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

bool mcu_link_uart_is_ready(void);
bool mcu_link_uart_try_lock(void);
void mcu_link_uart_unlock(void);
esp_err_t mcu_link_uart_acquire_exclusive(void);
void mcu_link_uart_release_exclusive(void);
void mcu_link_uart_deinit(void);
esp_err_t mcu_link_uart_wait_tx_done(uint32_t timeout_ms);
esp_err_t mcu_link_uart_write(const uint8_t *data, size_t data_len, size_t *out_written);
esp_err_t mcu_link_uart_read(uint8_t *buffer, size_t buffer_len, uint32_t timeout_ms, size_t *out_read);
esp_err_t mcu_link_uart_get_buffered_bytes(size_t *out_bytes);

#ifdef __cplusplus
}
#endif

#endif /* MCU_LINK_HOST_TEST_UART_H */

