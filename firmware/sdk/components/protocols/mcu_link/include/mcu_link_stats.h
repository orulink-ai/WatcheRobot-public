/**
 * @file mcu_link_stats.h
 * @brief Lightweight counters for the coprocessor link.
 */

#ifndef MCU_LINK_STATS_H
#define MCU_LINK_STATS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t ack_timeout_count;
    uint32_t crc_error_count;
    uint32_t reconnect_count;
    uint32_t dropped_state_count;
    uint32_t motion_done_fault_count;
} mcu_link_stats_t;

void mcu_link_stats_init(mcu_link_stats_t *stats);
void mcu_link_stats_reset(mcu_link_stats_t *stats);
void mcu_link_stats_record_ack_timeout(mcu_link_stats_t *stats);
void mcu_link_stats_record_crc_error(mcu_link_stats_t *stats);
void mcu_link_stats_record_reconnect(mcu_link_stats_t *stats);
void mcu_link_stats_record_dropped_state(mcu_link_stats_t *stats);
void mcu_link_stats_record_motion_done_fault(mcu_link_stats_t *stats);
void mcu_link_stats_accumulate(mcu_link_stats_t *dst, const mcu_link_stats_t *src);
void mcu_link_stats_copy(mcu_link_stats_t *dst, const mcu_link_stats_t *src);

#ifdef __cplusplus
}
#endif

#endif /* MCU_LINK_STATS_H */
