#pragma once
#include "demo_state.h"
typedef struct {
    void *context;
    int (*read)(void *context, int16_t *pcm, size_t bytes);
    void (*progress)(void *context, size_t done, size_t total, uint32_t chunk_peak);
} demo_record_sink_t;
/* Pure bounded chunk runner. Caller owns PCM; any failure leaves only a partial
 * buffer which callers MUST NOT expose for replay. Never owns a codec or UI. */
int demo_record_run(int16_t *pcm, size_t samples, size_t chunk_samples, const demo_record_sink_t *sink, demo_pcm_stats_t *stats);
