#pragma once
#define BSP_SSCMA_FLASHER_UART_NUM 1
#define BSP_SSCMA_FLASHER_UART_TX 17
#define BSP_SSCMA_FLASHER_UART_RX 18
#define BSP_SSCMA_CLIENT_RST 128
int bsp_exp_io_set_level(unsigned mask, unsigned level);
