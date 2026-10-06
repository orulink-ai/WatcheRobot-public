#include "watcher_hx_ptl.h"

#include <stdbool.h>
#include <string.h>

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_io_expander.h"
#include "esp_log.h"
#include "esp_memory_utils.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "hx_media_protocol.h"
#include "hx_ptl_protocol.h"
#include "hx_ptl_resync.h"
#include "sensecap-watcher.h"

#define WATCHER_HX_PTL_SPI_HOST SPI2_HOST
#define WATCHER_HX_PTL_SPI_SCLK GPIO_NUM_4
#define WATCHER_HX_PTL_SPI_MOSI GPIO_NUM_5
#define WATCHER_HX_PTL_SPI_MISO GPIO_NUM_6
#define WATCHER_HX_PTL_SPI_CS GPIO_NUM_21
#define WATCHER_HX_PTL_SYNC_MASK (UINT32_C(1) << 6)
#define WATCHER_HX_PTL_SPI_HZ (12 * 1000 * 1000)
#define WATCHER_HX_PTL_MAX_JPEG_SIZE (128U * 1024U)
#define WATCHER_HX_PTL_EMPTY_BACKOFF_MS 5U
#define WATCHER_HX_PTL_BODY_CAPACITY (HX_MEDIA_HEADER_SIZE + WATCHER_HX_PTL_MAX_JPEG_SIZE)

static const char *TAG = "WATCHER_HX_PTL";

typedef struct {
    spi_device_handle_t device;
    esp_io_expander_handle_t expander;
    uint8_t *body;
    hx_media_sequence_tracker_t sequence;
    watcher_hx_ptl_stats_t stats;
    bool bus_owned;
    bool initialized;
    bool resynchronizing;
    hx_ptl_resync_t scanner;
} watcher_hx_ptl_context_t;

static watcher_hx_ptl_context_t s_ctx;

static esp_err_t spi_read(void *data, size_t size, bool keep_cs_active) {
    spi_transaction_t transaction = {
        .length = size * 8U,
        .rxlength = size * 8U,
        .rx_buffer = data,
        .flags = (keep_cs_active ? SPI_TRANS_CS_KEEP_ACTIVE : 0) |
                 (esp_ptr_external_ram(data) ? SPI_TRANS_DMA_USE_PSRAM : 0),
    };
    return spi_device_transmit(s_ctx.device, &transaction);
}

static void spi_finish_rejected_packet(void) {
    uint8_t discarded = 0U;

    /* The fast-path header holds CS low so the HX slave advances into the
     * matching body. Clock one byte without KEEP_ACTIVE to close a rejected
     * packet before the next SYNC-gated read attempts resynchronization. */
    (void)spi_read(&discarded, sizeof(discarded), false);
}

/* SYNC is a FIFO readiness hint, not a packet delimiter. A rejected header
 * leaves a partial packet in the slave. Keep clocking that bounded stream
 * even when SYNC drops, until a complete plausible header is found. Full
 * media/JPEG/CRC validation is still required before publishing anything. */
static esp_err_t read_resynchronized_header(uint8_t *header, hx_ptl_base_header_t *parsed, size_t *prefetched) {
    const int64_t deadline = esp_timer_get_time() + 50000;
    uint8_t bytes[16]; /* <= minimum body size, so never crosses a found packet end */
    *prefetched = 0;
    for (unsigned chunk = 0; chunk < 256U && esp_timer_get_time() < deadline; ++chunk) {
        esp_err_t ret = spi_read(bytes, sizeof(bytes), false);
        if (ret != ESP_OK)
            return ret;
        for (size_t i = 0; i < sizeof(bytes); ++i) {
            if (hx_ptl_resync_push(&s_ctx.scanner, bytes[i], WATCHER_HX_PTL_MAX_JPEG_SIZE, parsed)) {
                memcpy(header, s_ctx.scanner.header, HX_PTL_BASE_HEADER_SIZE);
                *prefetched = sizeof(bytes) - i - 1U;
                memcpy(s_ctx.body, bytes + i + 1U, *prefetched);
                s_ctx.scanner.used = 0;
                return ESP_OK;
            }
        }
    }
    return ESP_ERR_INVALID_RESPONSE;
}

static void update_stage_timing(uint32_t elapsed_us, uint32_t *last_us, uint32_t *max_us, uint64_t *total_us) {
    *last_us = elapsed_us;
    if (elapsed_us > *max_us) {
        *max_us = elapsed_us;
    }
    *total_us += elapsed_us;
}

static void update_stage_count(uint32_t value, uint32_t *last, uint32_t *maximum, uint64_t *total) {
    *last = value;
    if (value > *maximum) {
        *maximum = value;
    }
    *total += value;
}

static esp_err_t wait_ready(uint32_t timeout_ms) {
    const int64_t deadline_us = esp_timer_get_time() + (int64_t)timeout_ms * 1000;

    do {
        uint32_t level = 0U;
        ESP_RETURN_ON_ERROR(esp_io_expander_get_level(s_ctx.expander, WATCHER_HX_PTL_SYNC_MASK, &level), TAG,
                            "Read HX sync line");
        if ((level & WATCHER_HX_PTL_SYNC_MASK) != 0U) {
            /* Match Seeed's SPI client guard time: SYNC can rise just before
             * the complete protocol header has reached the slave FIFO. */
            vTaskDelay(pdMS_TO_TICKS(2));
            return ESP_OK;
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    } while (esp_timer_get_time() < deadline_us);
    return ESP_ERR_TIMEOUT;
}

esp_err_t watcher_hx_ptl_init(void) {
    esp_err_t ret;

    if (s_ctx.initialized) {
        return ESP_OK;
    }
    /* Reuse the production board's singleton PCA9535 and power sequencing.
     * The standalone demo owns a private watcher_board implementation, but
     * linking that into the product would create a second I2C/bus owner. */
    s_ctx.expander = bsp_io_expander_init();
    ESP_RETURN_ON_FALSE(s_ctx.expander != NULL, ESP_ERR_INVALID_STATE, TAG, "PCA9535 unavailable");
    ESP_RETURN_ON_ERROR(bsp_exp_io_set_level(BSP_PWR_AI_CHIP, 1), TAG, "Power HX6538");
    vTaskDelay(pdMS_TO_TICKS(20));
    ESP_RETURN_ON_ERROR(bsp_spi_bus_init(), TAG, "Initialize shared SPI2 bus");
    ESP_RETURN_ON_ERROR(esp_io_expander_set_dir(s_ctx.expander, WATCHER_HX_PTL_SYNC_MASK, IO_EXPANDER_INPUT), TAG,
                        "Configure HX sync line");

    spi_device_interface_config_t device_config = {
        .clock_speed_hz = WATCHER_HX_PTL_SPI_HZ,
        .mode = 0,
        .spics_io_num = WATCHER_HX_PTL_SPI_CS,
        .queue_size = 1,
    };
    ret = spi_bus_add_device(WATCHER_HX_PTL_SPI_HOST, &device_config, &s_ctx.device);
    if (ret != ESP_OK) {
        goto fail;
    }
    s_ctx.body = heap_caps_malloc(WATCHER_HX_PTL_BODY_CAPACITY, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (s_ctx.body == NULL) {
        ret = ESP_ERR_NO_MEM;
        goto fail;
    }
    hx_media_sequence_tracker_reset(&s_ctx.sequence);
    memset(&s_ctx.stats, 0, sizeof(s_ctx.stats));
    s_ctx.stats.spi_hz = WATCHER_HX_PTL_SPI_HZ;
    s_ctx.initialized = true;
    s_ctx.resynchronizing = false;
    memset(&s_ctx.scanner, 0, sizeof(s_ctx.scanner));
    ESP_LOGI(TAG, "HX binary PTL receiver ready at 12 MHz (max JPEG %u)", WATCHER_HX_PTL_MAX_JPEG_SIZE);
    return ESP_OK;

fail:
    (void)watcher_hx_ptl_deinit();
    return ret;
}
esp_err_t watcher_hx_ptl_read(watcher_hx_ptl_frame_t *frame, uint32_t timeout_ms) {
    uint8_t base_header[HX_PTL_BASE_HEADER_SIZE];
    hx_ptl_base_header_t parsed_base;
    hx_ptl_frame_t parsed_frame;
    hx_ptl_result_t protocol_result;
    int64_t stage_start_us;
    int64_t read_start_us;
    esp_err_t ret;
    size_t prefetched = 0;

    ESP_RETURN_ON_FALSE(s_ctx.initialized && frame != NULL, ESP_ERR_INVALID_STATE, TAG, "Receiver not initialized");
    stage_start_us = esp_timer_get_time();
    ret = s_ctx.resynchronizing ? ESP_OK : wait_ready(timeout_ms);
    const uint32_t ready_wait_us = (uint32_t)(esp_timer_get_time() - stage_start_us);
    update_stage_timing(ready_wait_us, &s_ctx.stats.ready_wait_last_us, &s_ctx.stats.ready_wait_max_us,
                        &s_ctx.stats.ready_wait_total_us);
    if (ret != ESP_OK) {
        if (ret == ESP_ERR_TIMEOUT) {
            ++s_ctx.stats.timeouts;
        } else {
            ++s_ctx.stats.io_errors;
        }
        return ret;
    }
    read_start_us = esp_timer_get_time();
    ret = spi_device_acquire_bus(s_ctx.device, portMAX_DELAY);
    if (ret != ESP_OK) {
        ++s_ctx.stats.io_errors;
        return ret;
    }
    stage_start_us = esp_timer_get_time();
    if (s_ctx.resynchronizing) {
        ret = read_resynchronized_header(base_header, &parsed_base, &prefetched);
    } else {
        ret = spi_read(base_header, sizeof(base_header), true);
    }
    const uint32_t header_read_us = (uint32_t)(esp_timer_get_time() - stage_start_us);
    update_stage_timing(header_read_us, &s_ctx.stats.header_read_last_us, &s_ctx.stats.header_read_max_us,
                        &s_ctx.stats.header_read_total_us);
    update_stage_count(1U, &s_ctx.stats.header_transactions_last, &s_ctx.stats.header_transactions_max,
                       &s_ctx.stats.header_transactions_total);
    update_stage_count((uint32_t)sizeof(base_header), &s_ctx.stats.header_bytes_last, &s_ctx.stats.header_bytes_max,
                       &s_ctx.stats.header_bytes_total);
    if (ret != ESP_OK) {
        if (ret != ESP_ERR_INVALID_RESPONSE)
            ++s_ctx.stats.io_errors;
        spi_device_release_bus(s_ctx.device);
        vTaskDelay(1);
        return ret;
    }
    protocol_result =
        hx_ptl_parse_base_header(base_header, sizeof(base_header), WATCHER_HX_PTL_MAX_JPEG_SIZE, &parsed_base);
    if (protocol_result != HX_PTL_OK) {
        const bool empty = protocol_result == HX_PTL_ERR_EMPTY;
        if (!empty) {
            ++s_ctx.stats.header_errors;
            if (s_ctx.stats.header_errors <= 4U || (s_ctx.stats.header_errors % 1000U) == 0U) {
                ESP_LOGW(TAG, "Reject PTL header #%u: %s raw=%02x %02x %02x %02x %02x %02x %02x",
                         (unsigned)s_ctx.stats.header_errors, hx_ptl_result_name(protocol_result), base_header[0],
                         base_header[1], base_header[2], base_header[3], base_header[4], base_header[5],
                         base_header[6]);
            }
        }
        spi_finish_rejected_packet();
        /* An empty FIFO between frames is not loss of byte alignment. Do not
         * turn ordinary idle polling into continuous recovery SPI traffic. */
        if (!empty) {
            s_ctx.resynchronizing = true;
            s_ctx.scanner.used = 0;
        }
        spi_device_release_bus(s_ctx.device);
        vTaskDelay(pdMS_TO_TICKS(WATCHER_HX_PTL_EMPTY_BACKOFF_MS));
        /* hal_camera_ptl treats INVALID_RESPONSE as a retryable no-frame
         * result. ESP_ERR_NOT_FOUND is logged on every poll and can starve
         * the media pipeline, so keep the established retry contract while
         * excluding empty polls from protocol-error statistics. */
        return ESP_ERR_INVALID_RESPONSE;
    }
    update_stage_timing(0U, &s_ctx.stats.body_wait_last_us, &s_ctx.stats.body_wait_max_us,
                        &s_ctx.stats.body_wait_total_us);
    stage_start_us = esp_timer_get_time();
    ret = spi_read(s_ctx.body + prefetched, parsed_base.body_size - prefetched, false);
    const uint32_t body_read_us = (uint32_t)(esp_timer_get_time() - stage_start_us);
    update_stage_timing(body_read_us, &s_ctx.stats.body_read_last_us, &s_ctx.stats.body_read_max_us,
                        &s_ctx.stats.body_read_total_us);
    spi_device_release_bus(s_ctx.device);
    if (ret != ESP_OK) {
        ++s_ctx.stats.io_errors;
        s_ctx.resynchronizing = true;
        s_ctx.scanner.used = 0;
        return ret;
    }
    stage_start_us = esp_timer_get_time();
    protocol_result = hx_ptl_validate_jpeg_packet(base_header, sizeof(base_header), s_ctx.body, parsed_base.body_size,
                                                  WATCHER_HX_PTL_MAX_JPEG_SIZE, &parsed_frame);
    const uint32_t validate_us = (uint32_t)(esp_timer_get_time() - stage_start_us);
    update_stage_timing(validate_us, &s_ctx.stats.validate_last_us, &s_ctx.stats.validate_max_us,
                        &s_ctx.stats.validate_total_us);
    if (protocol_result != HX_PTL_OK) {
        if (protocol_result == HX_PTL_ERR_MEDIA_HEADER) {
            ++s_ctx.stats.crc_errors;
        } else {
            ++s_ctx.stats.header_errors;
        }
        const uint32_t rejected = s_ctx.stats.crc_errors + s_ctx.stats.header_errors;
        if (rejected <= 4U || (rejected % 1000U) == 0U) {
            ESP_LOGW(TAG, "Reject PTL body #%u: %s", (unsigned)rejected, hx_ptl_result_name(protocol_result));
            if (protocol_result == HX_PTL_ERR_MEDIA_HEADER || protocol_result == HX_PTL_ERR_JPEG) {
                const size_t diagnostic_size = parsed_base.body_size < 32U ? parsed_base.body_size : 32U;
                ESP_LOGW(TAG, "PTL body size=%u; first %u byte(s) follow", (unsigned)parsed_base.body_size,
                         (unsigned)diagnostic_size);
                ESP_LOG_BUFFER_HEX_LEVEL(TAG, s_ctx.body, diagnostic_size, ESP_LOG_WARN);
                if (parsed_base.body_size > diagnostic_size) {
                    ESP_LOGW(TAG, "Last %u PTL body byte(s) follow", (unsigned)diagnostic_size);
                    ESP_LOG_BUFFER_HEX_LEVEL(TAG, &s_ctx.body[parsed_base.body_size - diagnostic_size], diagnostic_size,
                                             ESP_LOG_WARN);
                }
            }
        }
        return ESP_ERR_INVALID_CRC;
    }
    if (parsed_frame.payload_size < 4U || parsed_frame.payload[0] != 0xffU || parsed_frame.payload[1] != 0xd8U ||
        parsed_frame.payload[parsed_frame.payload_size - 2U] != 0xffU ||
        parsed_frame.payload[parsed_frame.payload_size - 1U] != 0xd9U) {
        ++s_ctx.stats.header_errors;
        return ESP_ERR_INVALID_RESPONSE;
    }

    hx_media_sequence_tracker_accept(&s_ctx.sequence, parsed_frame.media.sequence);
    if (s_ctx.resynchronizing) {
        ESP_LOGW(TAG, "PTL stream resynchronized at frame %u", (unsigned)parsed_frame.media.sequence);
        s_ctx.resynchronizing = false;
    }
    s_ctx.stats.sequence_gaps = s_ctx.sequence.gaps;
    s_ctx.stats.duplicates = s_ctx.sequence.duplicates;
    s_ctx.stats.reordered = s_ctx.sequence.reordered;
    const uint32_t read_us = (uint32_t)(esp_timer_get_time() - read_start_us);
    s_ctx.stats.jpeg_last_bytes = (uint32_t)parsed_frame.payload_size;
    if (s_ctx.stats.jpeg_last_bytes > s_ctx.stats.jpeg_max_bytes) {
        s_ctx.stats.jpeg_max_bytes = s_ctx.stats.jpeg_last_bytes;
    }
    s_ctx.stats.read_last_us = read_us;
    if (read_us > s_ctx.stats.read_max_us) {
        s_ctx.stats.read_max_us = read_us;
    }
    ++s_ctx.stats.frames_ok;
    frame->jpeg = parsed_frame.payload;
    frame->jpeg_size = parsed_frame.payload_size;
    frame->sequence = parsed_frame.media.sequence;
    frame->timestamp_ms = parsed_frame.media.timestamp_ms;
    frame->flags = parsed_frame.media.flags;
    return ESP_OK;
}

esp_err_t watcher_hx_ptl_get_stats(watcher_hx_ptl_stats_t *stats) {
    ESP_RETURN_ON_FALSE(stats != NULL, ESP_ERR_INVALID_ARG, TAG, "Stats output is null");
    *stats = s_ctx.stats;
    return ESP_OK;
}

esp_err_t watcher_hx_ptl_deinit(void) {
    esp_err_t ret = ESP_OK;

    ESP_LOGI(TAG, "PTL stats: frames=%u header=%u crc=%u io=%u timeout=%u gaps=%u dup=%u reorder=%u",
             (unsigned)s_ctx.stats.frames_ok, (unsigned)s_ctx.stats.header_errors, (unsigned)s_ctx.stats.crc_errors,
             (unsigned)s_ctx.stats.io_errors, (unsigned)s_ctx.stats.timeouts, (unsigned)s_ctx.stats.sequence_gaps,
             (unsigned)s_ctx.stats.duplicates, (unsigned)s_ctx.stats.reordered);
    s_ctx.initialized = false;
    if (s_ctx.body != NULL) {
        heap_caps_free(s_ctx.body);
        s_ctx.body = NULL;
    }
    if (s_ctx.device != NULL) {
        ret = spi_bus_remove_device(s_ctx.device);
        s_ctx.device = NULL;
    }
    if (s_ctx.bus_owned) {
        esp_err_t bus_ret = spi_bus_free(WATCHER_HX_PTL_SPI_HOST);
        if (ret == ESP_OK) {
            ret = bus_ret;
        }
        s_ctx.bus_owned = false;
    }
    s_ctx.expander = NULL;
    return ret;
}

