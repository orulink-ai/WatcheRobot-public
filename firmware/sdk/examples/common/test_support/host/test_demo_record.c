#include "demo_record.h"
#include "demo_audio.h"
#include "demo_playback.h"
#include <assert.h>
#include <stdio.h>
static unsigned calls, progress, fail_at, replayed, drained;
static int read_chunk(void *ctx, int16_t *pcm, size_t bytes) {
    (void)ctx;
    if (++calls == fail_at) return 9;
    for (size_t i = 0; i < bytes / 2; i++) pcm[i] = -100;
    return 0;
}
static void changed(void *ctx, size_t done, size_t total, uint32_t peak) {
    (void)ctx; assert(done <= total && peak == 100); progress++;
}
static int write_replay(void *ctx, int16_t *pcm, size_t bytes) {
    (void)ctx;
    for (size_t i = 0; i < bytes / sizeof(*pcm); i++) assert(pcm[i] == -3200);
    replayed += (unsigned)(bytes / sizeof(*pcm));
    return 0;
}
static int drain_replay(void *ctx) {
    (void)ctx; drained++; return 0;
}
int main(void) {
    int16_t pcm[9]; demo_pcm_stats_t stats;
    demo_record_sink_t sink = {NULL, read_chunk, changed};
    assert(demo_record_run(NULL, 9, 4, &sink, &stats) == -1);
    assert(demo_record_run(pcm, SIZE_MAX, 4, &sink, &stats) == -1);
    assert(demo_record_run(pcm, 9, 4, &sink, &stats) == 0);
    assert(stats.samples == 9 && stats.peak == 100 && calls == 3 && progress == 3);
    calls = progress = 0; fail_at = 2;
    assert(demo_record_run(pcm, 9, 4, &sink, &stats) == 9);
    assert(stats.samples == 4 && calls == 2 && progress == 1);
    calls = progress = fail_at = 0;
    assert(demo_record_run(pcm, 9, 4, &sink, &stats) == 0);
    demo_playback_source_t source = {.pcm = pcm, .samples = stats.samples,
                                    .boost = demo_safe_boost(32, stats.peak)};
    demo_playback_sink_t output = {NULL, write_replay, drain_replay, NULL};
    int16_t scratch[4]; demo_playback_result_t result;
    assert(source.boost == 32);
    assert(demo_playback_run(&source, &output, scratch, 4, &result) == 0);
    assert(result.submitted == 9 && result.clipped == 0 && replayed == 9 && drained == 1);
    for (size_t i = 0; i < 9; i++) assert(pcm[i] == -100); /* Replay never changes the saved recording. */
    assert(demo_safe_boost(32, 2000) == 16); /* Loud recordings retain peak protection. */
    assert(demo_safe_boost(32, 32768) == 1);
    puts("record/replay: complete, partial/error, saved PCM, 32x and peak protection passed");
    return 0;
}
