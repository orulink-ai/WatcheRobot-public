#include "io_expander_read_guard.h"

#include <stddef.h>

void io_expander_read_guard_init(io_expander_read_guard_t *guard) {
    if (guard == NULL) {
        return;
    }
    guard->consecutive_failures = 0U;
    guard->last_error = 0;
    guard->retry_after_us = 0;
}

bool io_expander_read_guard_can_attempt(const io_expander_read_guard_t *guard, int64_t now_us) {
    return guard == NULL || guard->consecutive_failures == 0U || now_us >= guard->retry_after_us;
}

uint32_t io_expander_read_guard_record_failure(io_expander_read_guard_t *guard, int64_t now_us, int error) {
    uint32_t delay_us = IO_EXPANDER_READ_RETRY_BASE_US;

    if (guard == NULL) {
        return delay_us;
    }

    if (guard->consecutive_failures < UINT32_MAX) {
        guard->consecutive_failures++;
    }
    for (uint32_t step = 1U; step < guard->consecutive_failures && delay_us < IO_EXPANDER_READ_RETRY_MAX_US; ++step) {
        if (delay_us > IO_EXPANDER_READ_RETRY_MAX_US / 2U) {
            delay_us = IO_EXPANDER_READ_RETRY_MAX_US;
        } else {
            delay_us *= 2U;
        }
    }
    if (delay_us > IO_EXPANDER_READ_RETRY_MAX_US) {
        delay_us = IO_EXPANDER_READ_RETRY_MAX_US;
    }

    guard->last_error = error;
    guard->retry_after_us = now_us + (int64_t)delay_us;
    return delay_us;
}

void io_expander_read_guard_record_success(io_expander_read_guard_t *guard) {
    io_expander_read_guard_init(guard);
}

int io_expander_read_guard_last_error(const io_expander_read_guard_t *guard) {
    return guard != NULL ? guard->last_error : 0;
}

uint32_t io_expander_read_guard_consecutive_failures(const io_expander_read_guard_t *guard) {
    return guard != NULL ? guard->consecutive_failures : 0U;
}

bool io_expander_read_guard_should_report(const io_expander_read_guard_t *guard) {
    if (guard == NULL || guard->consecutive_failures == 0U) {
        return false;
    }
    return guard->consecutive_failures == 1U || guard->consecutive_failures % 10U == 0U;
}

