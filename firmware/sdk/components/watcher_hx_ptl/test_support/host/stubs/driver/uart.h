#pragma once
#include <stddef.h>
#define UART_DATA_8_BITS 8
#define UART_PARITY_DISABLE 0
#define UART_STOP_BITS_1 1
#define UART_HW_FLOWCTRL_DISABLE 0
#define UART_SCLK_DEFAULT 0
typedef struct {
    int baud_rate, data_bits, parity, stop_bits, flow_ctrl, source_clk;
} uart_config_t;
int uart_driver_install(int port, int rx, int tx, int queue, void *handle, int flags);
int uart_driver_delete(int port);
int uart_param_config(int port, const uart_config_t *config);
int uart_set_pin(int port, int tx, int rx, int rts, int cts);
int uart_write_bytes(int port, const void *data, size_t size);
int uart_read_bytes(int port, void *data, size_t size, unsigned timeout);

