#pragma once
#include "esp_err.h"
esp_err_t mcu_link_uart_wait_tx_done(uint32_t timeout);
bool mcu_link_uart_try_lock(void);
void mcu_link_uart_unlock(void);
