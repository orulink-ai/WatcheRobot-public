#include "watcher_hx_control.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "hx_vision_control_codec.h"
#include "sensecap-watcher.h"
#include "watcher_hx_ptl.h"
#include <stdio.h>
#include <string.h>

static bool owned;
static unsigned request_id;
static uint32_t active_generation;
static StaticSemaphore_t control_lock_storage;
static SemaphoreHandle_t control_lock;
static portMUX_TYPE lock_init_guard = portMUX_INITIALIZER_UNLOCKED;
static void deinit_locked(void);
static void take_control_lock(void) {
    taskENTER_CRITICAL(&lock_init_guard);
    if (!control_lock)
        control_lock = xSemaphoreCreateMutexStatic(&control_lock_storage);
    taskEXIT_CRITICAL(&lock_init_guard);
    (void)xSemaphoreTake(control_lock, portMAX_DELAY);
}
typedef struct {
    unsigned result, generation, mode, model, debug, error;
} control_state_t;

static esp_err_t exchange_numbers(const char *request, unsigned id, const char *prefix, uint32_t *values,
                                  size_t capacity, size_t *count, unsigned timeout_ms) {
    if (!owned)
        return ESP_ERR_INVALID_STATE;
    const int size = (int)strlen(request);
    if (uart_write_bytes(BSP_SSCMA_FLASHER_UART_NUM, request, size) != size)
        return ESP_FAIL;
    const int64_t deadline = esp_timer_get_time() + (int64_t)timeout_ms * 1000;
    char line[512];
    unsigned used = 0;
    bool rejected = false;
    while (esp_timer_get_time() < deadline) {
        uint8_t byte;
        if (uart_read_bytes(BSP_SSCMA_FLASHER_UART_NUM, &byte, 1, pdMS_TO_TICKS(20)) != 1)
            continue;
        if (byte == '\r' || byte == '\n') {
            line[used] = 0;
            if (!rejected && hx_vision_control_numbers(line, prefix, values, capacity, count) && *count > 0 &&
                values[0] == id) {
                return ESP_OK;
            }
            used = 0;
            rejected = false;
        } else if (byte < 32 || byte > 126 || used == sizeof(line) - 1U) {
            rejected = true;
        } else if (!rejected)
            line[used++] = (char)byte;
    }
    return ESP_ERR_TIMEOUT;
}
static esp_err_t exchange(const char *request, unsigned id, control_state_t *out) {
    uint32_t values[7];
    size_t count = 0;
    esp_err_t result = exchange_numbers(request, id, "WV1 STATE ", values, 7, &count, 700);
    if (result != ESP_OK)
        return result;
    if (count != 7 || values[1] > 6 || values[3] > 4 || values[4] > 4 || values[5] > 1 || values[6] > 6)
        return ESP_ERR_INVALID_RESPONSE;
    *out = (control_state_t){values[1], values[2], values[3], values[4], values[5], values[6]};
    return ESP_OK;
}

static esp_err_t catalog_locked(uint32_t crc[4]) {
    if (!crc)
        return ESP_ERR_INVALID_ARG;
    char request[64];
    unsigned id = ++request_id;
    snprintf(request, sizeof(request), "WV1 CATALOG %u\r\n", id);
    uint32_t values[5];
    size_t count = 0;
    esp_err_t result = exchange_numbers(request, id, "WV1 CATALOG ", values, 5, &count, 5000);
    if (result != ESP_OK)
        return result;
    if (count != 5)
        return ESP_ERR_INVALID_RESPONSE;
    memcpy(crc, values + 1, sizeof(uint32_t) * 4);
    return ESP_OK;
}

static esp_err_t boxes_locked(watcher_hx_result_t *out) {
    if (!out)
        return ESP_ERR_INVALID_ARG;
    char request[64];
    unsigned id = ++request_id;
    snprintf(request, sizeof(request), "WV1 RESULT %u\r\n", id);
    uint32_t values[55];
    size_t count = 0;
    esp_err_t result = exchange_numbers(request, id, "WV1 BOXES ", values, 55, &count, 700);
    if (result != ESP_OK)
        return result;
    if (count < 7 || values[2] != 2 || !values[3] || values[4] != 320 || values[5] != 240 || values[6] > 8 ||
        count != 7 + values[6] * 6U)
        return ESP_ERR_INVALID_RESPONSE;
    watcher_hx_result_t parsed = {
        .generation = values[1], .sequence = values[3], .width = 320, .height = 240, .count = (uint8_t)values[6]};
    for (unsigned i = 0; i < parsed.count; ++i) {
        uint32_t *v = values + 7 + 6 * i;
        if (v[0] > 320 || v[1] > 240 || v[2] > 320 || v[3] > 240 || v[4] > 100 || v[5] > 79)
            return ESP_ERR_INVALID_RESPONSE;
        parsed.boxes[i] = (watcher_hx_box_t){v[0], v[1], v[2], v[3], v[4], v[5]};
    }
    *out = parsed;
    return ESP_OK;
}

static esp_err_t status(control_state_t *out) {
    char request[64];
    unsigned id = ++request_id;
    snprintf(request, sizeof(request), "WV1 STATUS %u\r\n", id);
    return exchange(request, id, out);
}

static esp_err_t init_locked(void) {
    if (owned)
        return ESP_OK;
    const uart_config_t config = {.baud_rate = 921600,
                                  .data_bits = UART_DATA_8_BITS,
                                  .parity = UART_PARITY_DISABLE,
                                  .stop_bits = UART_STOP_BITS_1,
                                  .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
                                  .source_clk = UART_SCLK_DEFAULT};
    esp_err_t result = uart_driver_install(BSP_SSCMA_FLASHER_UART_NUM, 1024, 0, 0, NULL, 0);
    if (result != ESP_OK)
        return result;
    owned = true;
    result = uart_param_config(BSP_SSCMA_FLASHER_UART_NUM, &config);
    if (result == ESP_OK)
        result = uart_set_pin(BSP_SSCMA_FLASHER_UART_NUM, BSP_SSCMA_FLASHER_UART_TX, BSP_SSCMA_FLASHER_UART_RX, -1, -1);
    control_state_t state;
    for (unsigned attempt = 0; result == ESP_OK && attempt < 3; ++attempt) {
        if (status(&state) == ESP_OK) {
            ESP_LOGI("HX_CONTROL", "Normal UART1 verified generation=%u mode=%u", state.generation, state.mode);
            return ESP_OK;
        }
        if (attempt == 2)
            result = ESP_ERR_TIMEOUT;
    }
    deinit_locked();
    return result;
}

static esp_err_t mode_locked(unsigned mode, unsigned model, bool preview) {
    if (mode > 2 || model > 4 || (mode == 2 ? model == 0 : model != 0) || (preview && mode != 2))
        return ESP_ERR_INVALID_ARG;
    control_state_t state;
    esp_err_t result = status(&state);
    if (result != ESP_OK) {
        ESP_LOGW("HX_CONTROL", "Mode %u status query failed: %s", mode, esp_err_to_name(result));
        return result;
    }
    const bool drain = state.mode == 1 || state.mode == 3 || state.mode == 4 || state.debug;
    char request[96];
    unsigned id = ++request_id;
    snprintf(request, sizeof(request), "WV1 MODE %u %u %u %u %u\r\n", id, state.generation, mode, model, preview ? 1U : 0U);
    result = exchange(request, id, &state);
    if (result != ESP_OK) {
        ESP_LOGW("HX_CONTROL", "Mode %u request ACK missing: %s", mode, esp_err_to_name(result));
        return result;
    }
    if (state.result > 1)
        return ESP_ERR_INVALID_STATE;
    unsigned generation = state.generation;
    const int64_t deadline = esp_timer_get_time() + 3500000;
    while (state.mode == 3 && esp_timer_get_time() < deadline) {
        /* No caller may concurrently read PTL here. Drain the last immutable
         * frame so Himax can prove its SPI DMA/FIFO have stopped. */
        if (drain) {
            watcher_hx_ptl_frame_t discarded;
            (void)watcher_hx_ptl_read(&discarded, 20);
        }
        vTaskDelay(pdMS_TO_TICKS(5));
        result = status(&state);
        if (result != ESP_OK) {
            ESP_LOGW("HX_CONTROL", "Mode %u completion query failed generation=%u: %s", mode, generation,
                     esp_err_to_name(result));
            return result;
        }
        if (state.generation != generation)
            return ESP_ERR_INVALID_STATE;
    }
    if (state.mode == 3)
        return ESP_ERR_TIMEOUT;
    if (state.mode != mode || state.model != model || state.debug != (unsigned)preview || state.error != 0)
        return ESP_FAIL;
    active_generation = state.generation;
    return ESP_OK;
}
uint32_t watcher_hx_control_generation(void) {
    return active_generation;
}

static void deinit_locked(void) {
    if (owned && uart_driver_delete(BSP_SSCMA_FLASHER_UART_NUM) == ESP_OK)
        owned = false;
}

/* RTC capture workers and HAL lifecycle callers share one physical UART.
 * Serialize whole transactions, including replies and mode completion polls. */
esp_err_t watcher_hx_control_init(void) {
    take_control_lock();
    esp_err_t result = init_locked();
    xSemaphoreGive(control_lock);
    return result;
}
void watcher_hx_control_deinit(void) {
    take_control_lock();
    deinit_locked();
    xSemaphoreGive(control_lock);
}
esp_err_t watcher_hx_control_mode(unsigned mode, unsigned model) {
    return watcher_hx_control_mode_preview(mode, model, false);
}
esp_err_t watcher_hx_control_mode_preview(unsigned mode, unsigned model, bool preview) {
    take_control_lock();
    esp_err_t result = mode_locked(mode, model, preview);
    xSemaphoreGive(control_lock);
    return result;
}
esp_err_t watcher_hx_control_catalog(uint32_t crc[4]) {
    take_control_lock();
    esp_err_t result = catalog_locked(crc);
    xSemaphoreGive(control_lock);
    return result;
}
esp_err_t watcher_hx_control_boxes(watcher_hx_result_t *out) {
    take_control_lock();
    esp_err_t result = boxes_locked(out);
    xSemaphoreGive(control_lock);
    return result;
}
esp_err_t watcher_hx_control_status(watcher_hx_state_t *out) {
    if (!out)
        return ESP_ERR_INVALID_ARG;
    take_control_lock();
    control_state_t state;
    esp_err_t result = status(&state);
    if (result == ESP_OK)
        *out = (watcher_hx_state_t){state.generation, state.mode, state.model, state.error};
    xSemaphoreGive(control_lock);
    return result;
}

