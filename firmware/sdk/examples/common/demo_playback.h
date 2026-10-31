#pragma once
#include <stddef.h>
#include <stdint.h>

/* Example policy, not a raw SDK API. Immutable mono PCM or a generated tone.
 * Callbacks execute synchronously on the caller (media-owner) task. */
typedef struct {
    const int16_t *pcm;
    size_t samples;
    unsigned boost;
    uint32_t tone_rate; /* Nonzero only for a one-second tone; pcm must be NULL. */
} demo_playback_source_t;
typedef struct {
    void *context;
    int (*write)(void *context, int16_t *scratch, size_t bytes);
    int (*drain)(void *context);
    void (*progress)(void *context, size_t submitted, size_t total);
} demo_playback_sink_t;
typedef struct {
    size_t submitted;
    size_t clipped;
} demo_playback_result_t;
/* 0 = submitted and drained; -1 = invalid arguments; otherwise callback error.
 * Stop at first write error, drain any submitted/possibly submitted prefix and
 * never retry it automatically. Write error takes precedence over drain error.
 * Progress reaches 100% only after drain succeeds. Scratch is caller-owned. */
int demo_playback_run(const demo_playback_source_t *source, const demo_playback_sink_t *sink, int16_t *scratch,
                      size_t capacity, demo_playback_result_t *result);
