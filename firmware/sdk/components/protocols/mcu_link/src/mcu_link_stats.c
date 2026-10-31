#include "mcu_link_stats.h"

#include <limits.h>
#include <string.h>

static void increment_counter(uint32_t *counter) {
    if (counter == NULL) {
        return;
    }

    if (*counter < UINT32_MAX) {
        ++(*counter);
    }
}

void mcu_link_stats_init(mcu_link_stats_t *stats) {
    if (stats == NULL) {
        return;
    }

    memset(stats, 0, sizeof(*stats));
}

void mcu_link_stats_reset(mcu_link_stats_t *stats) {
    mcu_link_stats_init(stats);
}

void mcu_link_stats_record_ack_timeout(mcu_link_stats_t *stats) {
    if (stats == NULL) {
        return;
    }

    increment_counter(&stats->ack_timeout_count);
}

void mcu_link_stats_record_crc_error(mcu_link_stats_t *stats) {
    if (stats == NULL) {
        return;
    }

    increment_counter(&stats->crc_error_count);
}

void mcu_link_stats_record_reconnect(mcu_link_stats_t *stats) {
    if (stats == NULL) {
        return;
    }

    increment_counter(&stats->reconnect_count);
}

void mcu_link_stats_record_dropped_state(mcu_link_stats_t *stats) {
    if (stats == NULL) {
        return;
    }

    increment_counter(&stats->dropped_state_count);
}

void mcu_link_stats_record_motion_done_fault(mcu_link_stats_t *stats) {
    if (stats == NULL) {
        return;
    }

    increment_counter(&stats->motion_done_fault_count);
}

void mcu_link_stats_accumulate(mcu_link_stats_t *dst, const mcu_link_stats_t *src) {
    if (dst == NULL || src == NULL) {
        return;
    }

    if (dst->ack_timeout_count < UINT32_MAX) {
        const uint64_t value = (uint64_t)dst->ack_timeout_count + src->ack_timeout_count;
        dst->ack_timeout_count = (value > UINT32_MAX) ? UINT32_MAX : (uint32_t)value;
    }
    if (dst->crc_error_count < UINT32_MAX) {
        const uint64_t value = (uint64_t)dst->crc_error_count + src->crc_error_count;
        dst->crc_error_count = (value > UINT32_MAX) ? UINT32_MAX : (uint32_t)value;
    }
    if (dst->reconnect_count < UINT32_MAX) {
        const uint64_t value = (uint64_t)dst->reconnect_count + src->reconnect_count;
        dst->reconnect_count = (value > UINT32_MAX) ? UINT32_MAX : (uint32_t)value;
    }
    if (dst->dropped_state_count < UINT32_MAX) {
        const uint64_t value = (uint64_t)dst->dropped_state_count + src->dropped_state_count;
        dst->dropped_state_count = (value > UINT32_MAX) ? UINT32_MAX : (uint32_t)value;
    }
    if (dst->motion_done_fault_count < UINT32_MAX) {
        const uint64_t value = (uint64_t)dst->motion_done_fault_count + src->motion_done_fault_count;
        dst->motion_done_fault_count = (value > UINT32_MAX) ? UINT32_MAX : (uint32_t)value;
    }
}

void mcu_link_stats_copy(mcu_link_stats_t *dst, const mcu_link_stats_t *src) {
    if (dst == NULL || src == NULL) {
        return;
    }

    *dst = *src;
}
