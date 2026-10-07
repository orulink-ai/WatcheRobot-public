/**
 * @file mcu_link_uart.h
 * @brief Minimal UART transport scaffold for the STM32 coprocessor link.
 */

#ifndef MCU_LINK_UART_H
#define MCU_LINK_UART_H

#include "driver/uart.h"
#include "esp_err.h"
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uart_port_t port;
    int tx_io_num;
    int rx_io_num;
    int baud_rate;
    int rx_buffer_size;
    int tx_buffer_size;
} mcu_link_uart_config_t;

esp_err_t mcu_link_uart_init(const mcu_link_uart_config_t *config);
void mcu_link_uart_deinit(void);
bool mcu_link_uart_is_ready(void);
uart_port_t mcu_link_uart_get_port(void);
esp_err_t mcu_link_uart_write(const uint8_t *data, size_t data_len, size_t *out_written);
esp_err_t mcu_link_uart_read(uint8_t *buffer, size_t buffer_len, uint32_t timeout_ms, size_t *out_read);
esp_err_t mcu_link_uart_get_buffered_bytes(size_t *out_bytes);
esp_err_t mcu_link_uart_wait_tx_done(uint32_t timeout_ms);
esp_err_t mcu_link_uart_flush_input(void);
esp_err_t mcu_link_uart_acquire_exclusive(void);
void mcu_link_uart_release_exclusive(void);
/* Recursive task gate shared by protocol operations and UART lifecycle.
 * Runtime calls fail fast while OTA holds the gate; no blocking I/O occurs
 * inside a FreeRTOS critical section. */
bool mcu_link_uart_try_lock(void);
void mcu_link_uart_unlock(void);

#ifdef __cplusplus
}
#endif

#endif /* MCU_LINK_UART_H */

