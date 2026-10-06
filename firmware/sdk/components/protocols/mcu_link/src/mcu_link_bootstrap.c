#include "mcu_link_bootstrap.h"

#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "mcu_link_uart.h"
#include "sdkconfig.h"

#include <string.h>

static const char *TAG = "MCU_LINK_BOOT";
static const char *OBS_TAG = "MCU_OBS";
static const int64_t HELLO_RETRY_INTERVAL_US = 1000LL * 1000LL;
static const uint32_t OTA_RUNTIME_READY_TIMEOUT_MS = 10000U;

static mcu_link_t s_link;
static bool s_link_initialized;
static int64_t s_first_hello_req_us;
static int64_t s_last_hello_req_us;
static TaskHandle_t s_ota_owner;

static esp_err_t mcu_link_bootstrap_send_hello_req(void) {
    uint32_t seq = 0u;
    size_t wire_len = 0u;
    mcu_link_state_t previous_state = mcu_link_get_state(&s_link);
    esp_err_t ret;

    ret = mcu_link_send_hello_req(&s_link, &seq, &wire_len);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "MCU link hello request failed: %s", esp_err_to_name(ret));
        return ret;
    }

    s_last_hello_req_us = esp_timer_get_time();
    if (s_first_hello_req_us == 0 ||
        (previous_state != MCU_LINK_STATE_HANDSHAKING && previous_state != MCU_LINK_STATE_DEGRADED &&
         previous_state != MCU_LINK_STATE_RECOVERING)) {
        s_first_hello_req_us = s_last_hello_req_us;
    }
    ESP_LOGI(TAG, "MCU link hello request queued (seq=%lu wire_len=%u state=%d)", (unsigned long)seq,
             (unsigned)wire_len, (int)mcu_link_get_state(&s_link));
    ESP_LOGI(OBS_TAG, "evt=hello_req seq=%lu msg_class=%u msg_id=%u link_state=%d", (unsigned long)seq,
             (unsigned)MCU_FRAME_CLASS_SYS, (unsigned)MCU_SYS_MSG_HELLO_REQ, (int)mcu_link_get_state(&s_link));
    return ESP_OK;
}

static bool mcu_link_bootstrap_hello_retry_due(void) {
    const mcu_link_state_t state = mcu_link_get_state(&s_link);

    if (state != MCU_LINK_STATE_HANDSHAKING && state != MCU_LINK_STATE_DEGRADED && state != MCU_LINK_STATE_RECOVERING) {
        return false;
    }

    if (s_last_hello_req_us == 0) {
        return true;
    }

    return (esp_timer_get_time() - s_last_hello_req_us) >= HELLO_RETRY_INTERVAL_US;
}

static esp_err_t mcu_link_bootstrap_init_uart(void) {
#ifdef CONFIG_WATCHER_MCU_LINK_UART_ENABLE
    const mcu_link_uart_config_t config = {
        .port = (uart_port_t)CONFIG_WATCHER_MCU_LINK_UART_PORT_NUM,
        .tx_io_num = CONFIG_WATCHER_MCU_LINK_UART_TX_GPIO,
        .rx_io_num = CONFIG_WATCHER_MCU_LINK_UART_RX_GPIO,
        .baud_rate = CONFIG_WATCHER_MCU_LINK_UART_BAUD_RATE,
        .rx_buffer_size = CONFIG_WATCHER_MCU_LINK_UART_RX_BUFFER,
        .tx_buffer_size = CONFIG_WATCHER_MCU_LINK_UART_TX_BUFFER,
    };

    return mcu_link_uart_init(&config);
#else
    return ESP_OK;
#endif
}

static esp_err_t mcu_link_bootstrap_init_locked(void) {
    esp_err_t ret;

    if (s_link_initialized) {
        return ESP_OK;
    }

    ret = mcu_link_init(&s_link);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "MCU link bootstrap init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = mcu_link_bootstrap_init_uart();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "MCU link UART bootstrap init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    s_link_initialized = true;
    s_first_hello_req_us = 0;
    s_last_hello_req_us = 0;
    ESP_LOGI(TAG, "MCU link bootstrap initialized (uart_ready=%d link_ready=%d ready=%d)",
             mcu_link_uart_is_ready() ? 1 : 0, mcu_link_is_link_ready(&s_link) ? 1 : 0,
             mcu_link_is_ready(&s_link) ? 1 : 0);
    return ESP_OK;
}

static mcu_link_t *mcu_link_bootstrap_get_link_locked(void) {
    return s_link_initialized && !s_ota_owner ? &s_link : NULL;
}

static bool mcu_link_bootstrap_is_link_ready_locked(void) {
    return s_link_initialized && !s_ota_owner && mcu_link_is_link_ready(&s_link);
}

static bool mcu_link_bootstrap_is_ready_locked(void) {
    return s_link_initialized && !s_ota_owner && mcu_link_is_ready(&s_link);
}

static bool mcu_link_bootstrap_handshake_timed_out_locked(uint32_t timeout_ms) {
    int64_t elapsed_us;
    mcu_link_state_t state;

    if (!s_link_initialized || s_first_hello_req_us == 0 || timeout_ms == 0) {
        return false;
    }

    state = mcu_link_get_state(&s_link);
    if (state != MCU_LINK_STATE_HANDSHAKING && state != MCU_LINK_STATE_DEGRADED && state != MCU_LINK_STATE_RECOVERING) {
        return false;
    }

    elapsed_us = esp_timer_get_time() - s_first_hello_req_us;
    return elapsed_us >= ((int64_t)timeout_ms * 1000LL);
}

static esp_err_t mcu_link_bootstrap_poll_locked(mcu_link_event_t *out_event) {
    mcu_link_event_t local_event;
    if (!s_link_initialized || s_ota_owner) {
        return ESP_ERR_INVALID_STATE;
    }

    if (!mcu_link_uart_is_ready()) {
        return ESP_ERR_INVALID_STATE;
    }

    if (out_event == NULL) {
        out_event = &local_event;
    }

    memset(out_event, 0, sizeof(*out_event));

    for (unsigned events = 0; events < 16U && mcu_link_poll(&s_link, out_event) == ESP_OK; ++events) {
        if (out_event != &local_event) {
            return ESP_OK;
        }
        memset(out_event, 0, sizeof(*out_event));
    }

    if (mcu_link_bootstrap_hello_retry_due()) {
        return mcu_link_bootstrap_send_hello_req();
    }

    return ESP_ERR_NOT_FOUND;
}

static esp_err_t mcu_link_bootstrap_start_locked(void) {
    if (!s_link_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (!mcu_link_uart_is_ready()) {
        ESP_LOGI(TAG, "MCU link transport disabled; handshake not started");
        return ESP_OK;
    }

    return mcu_link_bootstrap_send_hello_req();
}

static void mcu_link_bootstrap_stop_locked(void) {
    if (s_ota_owner) {
        ESP_LOGW(TAG, "MCU link stop ignored while STM32 OTA owns the UART");
        return;
    }
    if (!s_link_initialized) {
        mcu_link_uart_deinit();
        return;
    }

    mcu_link_uart_deinit();
    memset(&s_link, 0, sizeof(s_link));
    s_link_initialized = false;
    s_first_hello_req_us = 0;
    s_last_hello_req_us = 0;
    ESP_LOGI(TAG, "MCU link bootstrap stopped");
}

static mcu_link_state_t mcu_link_bootstrap_get_state_locked(void) {
    return s_link_initialized ? mcu_link_get_state(&s_link) : MCU_LINK_STATE_DOWN;
}

static void mcu_link_bootstrap_release_ota_owner(void) {
    s_ota_owner = NULL;
    mcu_link_uart_release_exclusive();
}

esp_err_t mcu_link_bootstrap_begin_ota(void) {
    uint32_t sequence = 0U;
    size_t wire_length = 0U;
    esp_err_t ret;

    if (mcu_link_bootstrap_get_link() == NULL) {
        ret = mcu_link_bootstrap_init();
        if (ret != ESP_OK) {
            return ret;
        }
        ret = mcu_link_bootstrap_start();
        if (ret != ESP_OK) {
            return ret;
        }
    }

    const int64_t ready_deadline_us = esp_timer_get_time() + ((int64_t)OTA_RUNTIME_READY_TIMEOUT_MS * 1000LL);
    while (!mcu_link_bootstrap_is_link_ready() && esp_timer_get_time() < ready_deadline_us) {
        vTaskDelay(pdMS_TO_TICKS(10U));
    }

    ret = mcu_link_uart_acquire_exclusive();
    if (ret != ESP_OK) {
        return ret;
    }

    if (s_ota_owner) {
        mcu_link_uart_release_exclusive();
        return ESP_ERR_INVALID_STATE;
    }
    const bool runtime_ready = s_link_initialized && mcu_link_is_link_ready(&s_link);
    s_ota_owner = xTaskGetCurrentTaskHandle();

    /* An interrupted single-slot update has no application to answer HELLO.
     * The service must still be able to probe the resident bootloader. */
    if (!runtime_ready) {
        ESP_LOGI(TAG, "Runtime unavailable; probing STM32 bootloader for recovery");
        mcu_link_uart_deinit();
        return ESP_OK;
    }

    ret = mcu_link_send_frame(&s_link, MCU_FRAME_CLASS_SYS, MCU_SYS_MSG_OTA_ENTER, MCU_FRAME_FLAG_ACK_REQ, NULL, 0U,
                              &sequence, &wire_length);
    if (ret == ESP_OK) {
        ret = mcu_link_uart_wait_tx_done(500U);
    }
    if (ret != ESP_OK) {
        mcu_link_bootstrap_release_ota_owner();
        return ret;
    }
    ESP_LOGI(TAG, "STM32 OTA enter requested (seq=%lu wire_len=%u)", (unsigned long)sequence, (unsigned)wire_length);
    vTaskDelay(pdMS_TO_TICKS(80U));
    mcu_link_uart_deinit();
    return ESP_OK;
}

esp_err_t mcu_link_bootstrap_finish_ota(const char *expected_commit, bool expected_dirty, uint32_t timeout_ms,
                                        mcu_link_peer_info_t *out_peer_info) {
    const int64_t deadline_us = esp_timer_get_time() + ((int64_t)timeout_ms * 1000LL);
    int64_t next_hello_us = 0;
    esp_err_t ret;

    if (!mcu_link_uart_try_lock()) {
        return ESP_ERR_INVALID_STATE;
    }
    const bool owns_ota = s_ota_owner == xTaskGetCurrentTaskHandle();
    mcu_link_uart_unlock();
    if (!owns_ota) {
        return ESP_ERR_INVALID_ARG;
    }
    if (expected_commit == NULL || timeout_ms == 0U) {
        mcu_link_bootstrap_abort_ota();
        return ESP_ERR_INVALID_ARG;
    }
    mcu_link_uart_deinit();
    ret = mcu_link_reset(&s_link);
    if (ret == ESP_OK) {
        ret = mcu_link_bootstrap_init_uart();
    }
    while (ret == ESP_OK && esp_timer_get_time() < deadline_us) {
        mcu_link_event_t event;
        const int64_t now_us = esp_timer_get_time();
        if (now_us >= next_hello_us) {
            ret = mcu_link_bootstrap_send_hello_req();
            next_hello_us = now_us + 500000LL;
            if (ret != ESP_OK) {
                break;
            }
        }
        ret = mcu_link_poll(&s_link, &event);
        if (ret == ESP_OK || ret == ESP_ERR_NOT_FOUND) {
            mcu_link_peer_info_t info = {0};
            ret = ESP_OK;
            if (mcu_link_copy_peer_info(&s_link, &info) == ESP_OK && info.git_valid &&
                info.git_dirty == expected_dirty && strcmp(info.git_commit, expected_commit) == 0) {
                if (out_peer_info != NULL) {
                    *out_peer_info = info;
                }
                mcu_link_bootstrap_release_ota_owner();
                return ESP_OK;
            }
            vTaskDelay(pdMS_TO_TICKS(10U));
        }
    }
    mcu_link_bootstrap_release_ota_owner();
    return ret == ESP_OK ? ESP_ERR_TIMEOUT : ret;
}

void mcu_link_bootstrap_abort_ota(void) {
    if (!mcu_link_uart_try_lock()) {
        return;
    }
    const bool owns_ota = s_ota_owner == xTaskGetCurrentTaskHandle();
    mcu_link_uart_unlock();
    if (!owns_ota) {
        return;
    }
    mcu_link_uart_deinit();
    (void)mcu_link_reset(&s_link);
    if (mcu_link_bootstrap_init_uart() == ESP_OK) {
        (void)mcu_link_bootstrap_send_hello_req();
    }
    mcu_link_bootstrap_release_ota_owner();
}

esp_err_t mcu_link_bootstrap_init(void) {
    if (!mcu_link_uart_try_lock()) {
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t result = mcu_link_bootstrap_init_locked();
    mcu_link_uart_unlock();
    return result;
}

mcu_link_t *mcu_link_bootstrap_get_link(void) {
    if (!mcu_link_uart_try_lock()) {
        return NULL;
    }
    mcu_link_t *result = mcu_link_bootstrap_get_link_locked();
    mcu_link_uart_unlock();
    return result;
}

bool mcu_link_bootstrap_is_link_ready(void) {
    if (!mcu_link_uart_try_lock()) {
        return false;
    }
    bool result = mcu_link_bootstrap_is_link_ready_locked();
    mcu_link_uart_unlock();
    return result;
}

bool mcu_link_bootstrap_is_ready(void) {
    if (!mcu_link_uart_try_lock()) {
        return false;
    }
    bool result = mcu_link_bootstrap_is_ready_locked();
    mcu_link_uart_unlock();
    return result;
}

bool mcu_link_bootstrap_handshake_timed_out(uint32_t timeout_ms) {
    if (!mcu_link_uart_try_lock()) {
        return false;
    }
    bool result = mcu_link_bootstrap_handshake_timed_out_locked(timeout_ms);
    mcu_link_uart_unlock();
    return result;
}

esp_err_t mcu_link_bootstrap_poll(mcu_link_event_t *out_event) {
    if (!mcu_link_uart_try_lock()) {
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t result = mcu_link_bootstrap_poll_locked(out_event);
    mcu_link_uart_unlock();
    return result;
}

esp_err_t mcu_link_bootstrap_start(void) {
    if (!mcu_link_uart_try_lock()) {
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t result = mcu_link_bootstrap_start_locked();
    mcu_link_uart_unlock();
    return result;
}

void mcu_link_bootstrap_stop(void) {
    if (!mcu_link_uart_try_lock()) {
        return;
    }
    mcu_link_bootstrap_stop_locked();
    mcu_link_uart_unlock();
}

mcu_link_state_t mcu_link_bootstrap_get_state(void) {
    if (!mcu_link_uart_try_lock()) {
        return MCU_LINK_STATE_DOWN;
    }
    mcu_link_state_t result = mcu_link_bootstrap_get_state_locked();
    mcu_link_uart_unlock();
    return result;
}

