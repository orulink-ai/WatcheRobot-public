#pragma once

#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const uint8_t *jpeg;
    size_t jpeg_size;
    uint32_t sequence;
    uint32_t timestamp_ms;
    uint8_t flags;
} watcher_hx_ptl_frame_t;

typedef struct {
    uint32_t frames_ok;
    uint32_t header_errors;
    uint32_t crc_errors;
    uint32_t io_errors;
    uint32_t timeouts;
    uint32_t sequence_gaps;
    uint32_t duplicates;
    uint32_t reordered;
    uint32_t jpeg_last_bytes;
    uint32_t jpeg_max_bytes;
    uint32_t read_last_us;
    uint32_t read_max_us;
    uint32_t ready_wait_last_us;
    uint32_t ready_wait_max_us;
    uint64_t ready_wait_total_us;
    uint32_t header_read_last_us;
    uint32_t header_read_max_us;
    uint64_t header_read_total_us;
    uint32_t header_transactions_last;
    uint32_t header_transactions_max;
    uint64_t header_transactions_total;
    uint32_t header_bytes_last;
    uint32_t header_bytes_max;
    uint64_t header_bytes_total;
    uint32_t body_wait_last_us;
    uint32_t body_wait_max_us;
    uint64_t body_wait_total_us;
    uint32_t body_read_last_us;
    uint32_t body_read_max_us;
    uint64_t body_read_total_us;
    uint32_t validate_last_us;
    uint32_t validate_max_us;
    uint64_t validate_total_us;
    uint32_t spi_hz;
} watcher_hx_ptl_stats_t;

/** Initialize the custom HX PTL transport. Mutually exclusive with SSCMA. */
esp_err_t watcher_hx_ptl_init(void);

/**
 * Wait for and validate one binary JPEG frame.
 * The returned JPEG view remains valid until the next read or deinit call.
 */
esp_err_t watcher_hx_ptl_read(watcher_hx_ptl_frame_t *frame, uint32_t timeout_ms);

esp_err_t watcher_hx_ptl_get_stats(watcher_hx_ptl_stats_t *stats);
esp_err_t watcher_hx_ptl_deinit(void);

#ifdef __cplusplus
}
#endif

