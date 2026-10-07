#pragma once
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef int uart_port_t;
#define UART_NUM_MAX 3
#define UART_DATA_8_BITS 8
#define UART_PARITY_DISABLE 0
#define UART_STOP_BITS_1 1
#define UART_HW_FLOWCTRL_DISABLE 0
#define UART_SCLK_DEFAULT 0
#define UART_PIN_NO_CHANGE -1
#define ESP_INTR_FLAG_SHARED 1
typedef struct {
    int baud_rate, data_bits, parity, stop_bits, flow_ctrl, source_clk;
} uart_config_t;
esp_err_t uart_param_config(uart_port_t port, const uart_config_t *config);
esp_err_t uart_set_pin(uart_port_t port, int tx, int rx, int rts, int cts);
esp_err_t uart_driver_install(uart_port_t port, int rx, int tx, int queue_size, void *queue, int flags);
esp_err_t uart_driver_delete(uart_port_t port);
int uart_write_bytes(uart_port_t port, const void *data, size_t length);
int uart_read_bytes(uart_port_t port, void *data, size_t length, TickType_t ticks);
esp_err_t uart_get_buffered_data_len(uart_port_t port, size_t *length);
esp_err_t uart_wait_tx_done(uart_port_t port, TickType_t ticks);
esp_err_t uart_flush_input(uart_port_t port);
#ifdef __cplusplus
}
#endif

