#include "mcu_link_uart.h"

#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "MCU_LINK_UART";

typedef struct {
    bool ready;
    mcu_link_uart_config_t config;
} mcu_link_uart_state_t;

static mcu_link_uart_state_t s_uart;
static portMUX_TYPE s_gate_init_lock = portMUX_INITIALIZER_UNLOCKED;
static SemaphoreHandle_t s_gate;

static SemaphoreHandle_t uart_gate(void) {
    taskENTER_CRITICAL(&s_gate_init_lock);
    SemaphoreHandle_t gate = s_gate;
    taskEXIT_CRITICAL(&s_gate_init_lock);
    if (gate != NULL) {
        return gate;
    }
    SemaphoreHandle_t candidate = xSemaphoreCreateRecursiveMutex();
    if (candidate == NULL) {
        return NULL;
    }
    taskENTER_CRITICAL(&s_gate_init_lock);
    if (s_gate == NULL) {
        s_gate = candidate;
        candidate = NULL;
    }
    gate = s_gate;
    taskEXIT_CRITICAL(&s_gate_init_lock);
    if (candidate != NULL) {
        vSemaphoreDelete(candidate);
    }
    return gate;
}

bool mcu_link_uart_try_lock(void) {
    SemaphoreHandle_t gate = uart_gate();
    return gate != NULL && xSemaphoreTakeRecursive(gate, 0) == pdTRUE;
}

void mcu_link_uart_unlock(void) {
    (void)xSemaphoreGiveRecursive(s_gate);
}

esp_err_t mcu_link_uart_acquire_exclusive(void) {
    SemaphoreHandle_t gate = uart_gate();
    if (gate == NULL) {
        return ESP_ERR_NO_MEM;
    }
    return xSemaphoreTakeRecursive(gate, pdMS_TO_TICKS(2000U)) == pdTRUE ? ESP_OK : ESP_ERR_TIMEOUT;
}

void mcu_link_uart_release_exclusive(void) {
    mcu_link_uart_unlock();
}

static esp_err_t mcu_link_uart_init_locked(const mcu_link_uart_config_t *config) {
    uart_config_t uart_config = {0};
    esp_err_t ret;

    ESP_RETURN_ON_FALSE(config != NULL, ESP_ERR_INVALID_ARG, TAG, "missing config");

    if (s_uart.ready) {
        if (memcmp(&s_uart.config, config, sizeof(*config)) == 0) {
            return ESP_OK;
        }
        mcu_link_uart_deinit();
    }

    uart_config.baud_rate = config->baud_rate;
    uart_config.data_bits = UART_DATA_8_BITS;
    uart_config.parity = UART_PARITY_DISABLE;
    uart_config.stop_bits = UART_STOP_BITS_1;
    uart_config.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
    uart_config.source_clk = UART_SCLK_DEFAULT;

    ESP_RETURN_ON_ERROR(uart_param_config(config->port, &uart_config), TAG, "uart_param_config failed");
    ESP_RETURN_ON_ERROR(
        uart_set_pin(config->port, config->tx_io_num, config->rx_io_num, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE), TAG,
        "uart_set_pin failed");

    ret = uart_driver_install(config->port, config->rx_buffer_size, config->tx_buffer_size, 0, NULL,
                              ESP_INTR_FLAG_SHARED);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "uart_driver_install failed: %s", esp_err_to_name(ret));
        return ret;
    }

    s_uart.ready = true;
    s_uart.config = *config;
    ESP_LOGI(TAG, "Runtime UART ready: uart=%d tx=%d rx=%d baud=%d rx_buf=%d tx_buf=%d", (int)config->port,
             config->tx_io_num, config->rx_io_num, config->baud_rate, config->rx_buffer_size, config->tx_buffer_size);
    return ESP_OK;
}

static void mcu_link_uart_deinit_locked(void) {
    if (!s_uart.ready) {
        return;
    }

    (void)uart_driver_delete(s_uart.config.port);
    memset(&s_uart, 0, sizeof(s_uart));
}

static bool mcu_link_uart_is_ready_locked(void) {
    return s_uart.ready;
}

static uart_port_t mcu_link_uart_get_port_locked(void) {
    return s_uart.ready ? s_uart.config.port : UART_NUM_MAX;
}

static esp_err_t mcu_link_uart_write_locked(const uint8_t *data, size_t data_len, size_t *out_written) {
    int written;

    ESP_RETURN_ON_FALSE(data != NULL, ESP_ERR_INVALID_ARG, TAG, "missing data");
    ESP_RETURN_ON_FALSE(data_len > 0u, ESP_ERR_INVALID_ARG, TAG, "empty write");
    ESP_RETURN_ON_FALSE(s_uart.ready, ESP_ERR_INVALID_STATE, TAG, "uart not ready");

    written = uart_write_bytes(s_uart.config.port, data, data_len);
    if (written < 0) {
        ESP_LOGE(TAG, "uart_write_bytes failed");
        return ESP_FAIL;
    }

    if (out_written != NULL) {
        *out_written = (size_t)written;
    }

    return written == (int)data_len ? ESP_OK : ESP_ERR_INVALID_SIZE;
}

static esp_err_t mcu_link_uart_read_locked(uint8_t *buffer, size_t buffer_len, uint32_t timeout_ms, size_t *out_read) {
    int read_len;
    TickType_t ticks = pdMS_TO_TICKS(timeout_ms);

    ESP_RETURN_ON_FALSE(buffer != NULL, ESP_ERR_INVALID_ARG, TAG, "missing buffer");
    ESP_RETURN_ON_FALSE(buffer_len > 0u, ESP_ERR_INVALID_ARG, TAG, "empty read");
    ESP_RETURN_ON_FALSE(s_uart.ready, ESP_ERR_INVALID_STATE, TAG, "uart not ready");

    read_len = uart_read_bytes(s_uart.config.port, buffer, buffer_len, ticks);
    if (read_len < 0) {
        ESP_LOGE(TAG, "uart_read_bytes failed");
        return ESP_FAIL;
    }

    if (out_read != NULL) {
        *out_read = (size_t)read_len;
    }

    return ESP_OK;
}

static esp_err_t mcu_link_uart_get_buffered_bytes_locked(size_t *out_bytes) {
    size_t buffered = 0u;

    ESP_RETURN_ON_FALSE(out_bytes != NULL, ESP_ERR_INVALID_ARG, TAG, "missing out_bytes");
    ESP_RETURN_ON_FALSE(s_uart.ready, ESP_ERR_INVALID_STATE, TAG, "uart not ready");

    ESP_RETURN_ON_ERROR(uart_get_buffered_data_len(s_uart.config.port, &buffered), TAG,
                        "uart_get_buffered_data_len failed");
    *out_bytes = buffered;
    return ESP_OK;
}

static esp_err_t mcu_link_uart_wait_tx_done_locked(uint32_t timeout_ms) {
    ESP_RETURN_ON_FALSE(s_uart.ready, ESP_ERR_INVALID_STATE, TAG, "uart not ready");
    return uart_wait_tx_done(s_uart.config.port, pdMS_TO_TICKS(timeout_ms));
}

static esp_err_t mcu_link_uart_flush_input_locked(void) {
    ESP_RETURN_ON_FALSE(s_uart.ready, ESP_ERR_INVALID_STATE, TAG, "uart not ready");
    return uart_flush_input(s_uart.config.port);
}

esp_err_t mcu_link_uart_init(const mcu_link_uart_config_t *config) {
    if (!mcu_link_uart_try_lock()) {
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t result = mcu_link_uart_init_locked(config);
    mcu_link_uart_unlock();
    return result;
}

void mcu_link_uart_deinit(void) {
    if (!mcu_link_uart_try_lock()) {
        return;
    }
    mcu_link_uart_deinit_locked();
    mcu_link_uart_unlock();
}

bool mcu_link_uart_is_ready(void) {
    if (!mcu_link_uart_try_lock()) {
        return false;
    }
    bool result = mcu_link_uart_is_ready_locked();
    mcu_link_uart_unlock();
    return result;
}

uart_port_t mcu_link_uart_get_port(void) {
    if (!mcu_link_uart_try_lock()) {
        return UART_NUM_MAX;
    }
    uart_port_t result = mcu_link_uart_get_port_locked();
    mcu_link_uart_unlock();
    return result;
}

esp_err_t mcu_link_uart_write(const uint8_t *data, size_t data_len, size_t *out_written) {
    if (!mcu_link_uart_try_lock()) {
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t result = mcu_link_uart_write_locked(data, data_len, out_written);
    mcu_link_uart_unlock();
    return result;
}

esp_err_t mcu_link_uart_read(uint8_t *buffer, size_t buffer_len, uint32_t timeout_ms, size_t *out_read) {
    if (!mcu_link_uart_try_lock()) {
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t result = mcu_link_uart_read_locked(buffer, buffer_len, timeout_ms, out_read);
    mcu_link_uart_unlock();
    return result;
}

esp_err_t mcu_link_uart_get_buffered_bytes(size_t *out_bytes) {
    if (!mcu_link_uart_try_lock()) {
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t result = mcu_link_uart_get_buffered_bytes_locked(out_bytes);
    mcu_link_uart_unlock();
    return result;
}

esp_err_t mcu_link_uart_wait_tx_done(uint32_t timeout_ms) {
    if (!mcu_link_uart_try_lock()) {
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t result = mcu_link_uart_wait_tx_done_locked(timeout_ms);
    mcu_link_uart_unlock();
    return result;
}

esp_err_t mcu_link_uart_flush_input(void) {
    if (!mcu_link_uart_try_lock()) {
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t result = mcu_link_uart_flush_input_locked();
    mcu_link_uart_unlock();
    return result;
}

