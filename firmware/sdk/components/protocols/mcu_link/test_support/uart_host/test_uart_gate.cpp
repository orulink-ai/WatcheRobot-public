/* Assertions perform driver calls as well as checks; keep them in Release. */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include "freertos/semphr.h"
#include "mcu_link_uart.h"
#include <atomic>
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>
#ifdef _MSC_VER
#include <crtdbg.h>
#include <stdlib.h>
#endif

static std::mutex critical;
static thread_local bool in_critical;
static std::atomic<unsigned> driver_calls{0};
static std::mutex read_mutex;
static std::condition_variable read_cv;
static bool block_read, read_entered, release_read;

extern "C" {
void test_enter_critical(void) {
    critical.lock();
    in_critical = true;
}
void test_exit_critical(void) {
    in_critical = false;
    critical.unlock();
}
SemaphoreHandle_t xSemaphoreCreateRecursiveMutex(void) {
    assert(!in_critical);
    return new std::recursive_timed_mutex;
}
int xSemaphoreTakeRecursive(SemaphoreHandle_t handle, TickType_t ticks) {
    assert(!in_critical);
    return static_cast<std::recursive_timed_mutex *>(handle)->try_lock_for(std::chrono::milliseconds(ticks));
}
int xSemaphoreGiveRecursive(SemaphoreHandle_t handle) {
    static_cast<std::recursive_timed_mutex *>(handle)->unlock();
    return pdTRUE;
}
void vSemaphoreDelete(SemaphoreHandle_t handle) {
    assert(!in_critical);
    delete static_cast<std::recursive_timed_mutex *>(handle);
}
static void driver_access(void) {
    assert(!in_critical);
    ++driver_calls;
}
esp_err_t uart_param_config(uart_port_t, const uart_config_t *) {
    driver_access();
    return ESP_OK;
}
esp_err_t uart_set_pin(uart_port_t, int, int, int, int) {
    driver_access();
    return ESP_OK;
}
esp_err_t uart_driver_install(uart_port_t, int, int, int, void *, int) {
    driver_access();
    return ESP_OK;
}
esp_err_t uart_driver_delete(uart_port_t) {
    driver_access();
    return ESP_OK;
}
int uart_write_bytes(uart_port_t, const void *, size_t length) {
    driver_access();
    return (int)length;
}
int uart_read_bytes(uart_port_t, void *, size_t, TickType_t) {
    driver_access();
    std::unique_lock<std::mutex> lock(read_mutex);
    if (block_read) {
        read_entered = true;
        read_cv.notify_all();
        read_cv.wait(lock, [] { return release_read; });
    }
    return 0;
}
esp_err_t uart_get_buffered_data_len(uart_port_t, size_t *length) {
    driver_access();
    *length = 0;
    return ESP_OK;
}
esp_err_t uart_wait_tx_done(uart_port_t, TickType_t) {
    driver_access();
    return ESP_OK;
}
esp_err_t uart_flush_input(uart_port_t) {
    driver_access();
    return ESP_OK;
}
}

int main(void) {
#ifdef _MSC_VER
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
    const mcu_link_uart_config_t config = {1, 17, 18, 115200, 512, 512};
    assert(mcu_link_uart_init(&config) == ESP_OK);
    assert(mcu_link_uart_acquire_exclusive() == ESP_OK);
    const unsigned before = driver_calls;
    std::thread rejected([&] {
        uint8_t byte = 0;
        size_t count = 0;
        assert(mcu_link_uart_read(&byte, 1, 0, &count) == ESP_ERR_INVALID_STATE);
        assert(mcu_link_uart_write(&byte, 1, &count) == ESP_ERR_INVALID_STATE);
        assert(mcu_link_uart_init(&config) == ESP_ERR_INVALID_STATE);
        assert(mcu_link_uart_flush_input() == ESP_ERR_INVALID_STATE);
        assert(mcu_link_uart_wait_tx_done(0) == ESP_ERR_INVALID_STATE);
        assert(mcu_link_uart_get_buffered_bytes(&count) == ESP_ERR_INVALID_STATE);
        mcu_link_uart_deinit();
        assert(!mcu_link_uart_is_ready());
        assert(mcu_link_uart_get_port() == UART_NUM_MAX);
    });
    rejected.join();
    assert(driver_calls == before);
    /* The owner can recursively enter the same APIs without releasing OTA. */
    assert(mcu_link_uart_flush_input() == ESP_OK);
    mcu_link_uart_release_exclusive();

    block_read = true;
    std::thread reader([] {
        uint8_t byte;
        size_t count;
        assert(mcu_link_uart_read(&byte, 1, 0, &count) == ESP_OK);
    });
    {
        std::unique_lock<std::mutex> lock(read_mutex);
        read_cv.wait(lock, [] { return read_entered; });
    }
    /* A runtime read already in progress must drain before the OTA owner may
     * switch baud or delete the driver. Exercise the real gate timeout. */
    assert(mcu_link_uart_acquire_exclusive() == ESP_ERR_TIMEOUT);
    {
        std::lock_guard<std::mutex> lock(read_mutex);
        release_read = true;
    }
    read_cv.notify_all();
    reader.join();
    assert(mcu_link_uart_acquire_exclusive() == ESP_OK);
    mcu_link_uart_deinit();
    assert(mcu_link_uart_init(&config) == ESP_OK);
    mcu_link_uart_release_exclusive();
    std::thread recovered([] { assert(mcu_link_uart_flush_input() == ESP_OK); });
    recovered.join();
    return 0;
}

