#include "demo_playback.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x)                                                                                                       \
    do {                                                                                                               \
        if (!(x)) {                                                                                                    \
            fprintf(stderr, "Failed: %s line %d\n", #x, __LINE__);                                                     \
            exit(1);                                                                                                   \
        }                                                                                                              \
    } while (0)

typedef struct {
    int writes, drains, fail_write, drain_error;
    size_t bytes, progress;
} fake_t;

static int write_pcm(void *context, int16_t *pcm, size_t bytes) {
    fake_t *fake = context;
    fake->writes++;
    CHECK(bytes > 0 && bytes <= 6 && bytes % 2 == 0);
    /* A mutating codec must not attenuate the immutable source. */
    pcm[0] = 0;
    if (fake->writes == fake->fail_write)
        return 17;
    fake->bytes += bytes;
    return 0;
}
static int drain(void *context) {
    fake_t *fake = context;
    fake->drains++;
    CHECK(fake->progress < 100);
    return fake->drain_error;
}
static void progress(void *context, size_t done, size_t total) {
    fake_t *fake = context;
    size_t percent = done * 100 / total;
    CHECK(percent >= fake->progress && percent <= 100);
    if (percent == 100)
        CHECK(fake->drains == 1);
    fake->progress = percent;
}
int main(void) {
    const int16_t pcm[] = {100, -100, 1000, -1000, 10000, -10000, 123};
    int16_t scratch[3];
    demo_playback_source_t source = {pcm, 7, 1, 0};
    fake_t fake = {0};
    demo_playback_sink_t sink = {&fake, write_pcm, drain, progress};
    demo_playback_result_t result;
    CHECK(demo_playback_run(&source, &sink, scratch, 3, &result) == 0);
    CHECK(result.submitted == 7 && result.clipped == 0);
    CHECK(fake.writes == 3 && fake.drains == 1 && fake.bytes == sizeof(pcm) && fake.progress == 100);
    CHECK(pcm[0] == 100 && pcm[6] == 123);
    fake = (fake_t){0};
    source.boost = 4;
    CHECK(demo_playback_run(&source, &sink, scratch, 3, &result) == 0);
    CHECK(result.clipped == 2 && pcm[4] == 10000);
    for (int fail = 1; fail <= 3; fail++) {
        fake = (fake_t){.fail_write = fail, .drain_error = 23};
        CHECK(demo_playback_run(&source, &sink, scratch, 3, &result) == 17);
        CHECK(fake.writes == fail && fake.drains == 1 && result.submitted == (size_t)(fail - 1) * 3);
        CHECK(fake.progress < 100);
    }
    fake = (fake_t){.drain_error = 23};
    CHECK(demo_playback_run(&source, &sink, scratch, 3, &result) == 23);
    CHECK(result.submitted == 7 && fake.progress < 100);
    fake = (fake_t){0};
    source = (demo_playback_source_t){NULL, 100, 1, 100};
    CHECK(demo_playback_run(&source, &sink, scratch, 3, &result) == 0);
    CHECK(result.submitted == 100 && fake.bytes == 200 && fake.drains == 1);
    fake = (fake_t){0};
    CHECK(demo_playback_run(NULL, &sink, scratch, 3, &result) == -1);
    CHECK(demo_playback_run(&source, NULL, scratch, 3, &result) == -1);
    CHECK(demo_playback_run(&source, &sink, NULL, 3, &result) == -1);
    CHECK(demo_playback_run(&source, &sink, scratch, 0, &result) == -1);
    CHECK(demo_playback_run(&source, &sink, scratch, SIZE_MAX, &result) == -1);
    CHECK(demo_playback_run(&source, &sink, scratch, 3, NULL) == -1);
    source.samples = 0;
    CHECK(demo_playback_run(&source, &sink, scratch, 3, &result) == -1);
    source.samples = 99; /* Tone must last exactly one second, rate >= 100. */
    CHECK(demo_playback_run(&source, &sink, scratch, 3, &result) == -1);
    source = (demo_playback_source_t){NULL, 7, 1, 0};
    CHECK(demo_playback_run(&source, &sink, scratch, 3, &result) == -1);
    source.pcm = pcm;
    sink.write = NULL;
    CHECK(demo_playback_run(&source, &sink, scratch, 3, &result) == -1);
    CHECK(fake.writes == 0 && fake.drains == 0);
    return 0;
}
