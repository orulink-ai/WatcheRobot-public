#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define IO_EXPANDER_READ_RETRY_BASE_US UINT32_C(100000)
#define IO_EXPANDER_READ_RETRY_MAX_US UINT32_C(1000000)

typedef struct {
    uint32_t consecutive_failures;
    int last_error;
    int64_t retry_after_us;
} io_expander_read_guard_t;

void io_expander_read_guard_init(io_expander_read_guard_t *guard);
bool io_expander_read_guard_can_attempt(const io_expander_read_guard_t *guard, int64_t now_us);
uint32_t io_expander_read_guard_record_failure(io_expander_read_guard_t *guard, int64_t now_us, int error);
void io_expander_read_guard_record_success(io_expander_read_guard_t *guard);
int io_expander_read_guard_last_error(const io_expander_read_guard_t *guard);
uint32_t io_expander_read_guard_consecutive_failures(const io_expander_read_guard_t *guard);
bool io_expander_read_guard_should_report(const io_expander_read_guard_t *guard);

#ifdef __cplusplus
}
#endif

