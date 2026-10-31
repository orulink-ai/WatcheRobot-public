#include "demo_audio.h"
#include <limits.h>
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
int main(void) {
    CHECK(demo_volume_step(DEMO_OUTPUT_DEFAULT, 0) == 90);
    CHECK(demo_volume_step(UINT_MAX, 0) == 95);
    CHECK(demo_volume_step(80, 5) == 85);
    CHECK(demo_volume_step(95, 5) == 95);
    CHECK(demo_volume_step(0, -5) == 0);
    CHECK(demo_volume_step(80, INT_MAX) == 95);
    CHECK(demo_volume_step(80, INT_MIN) == 0);
    CHECK(demo_boost_next(1) == 2 && demo_boost_next(2) == 4);
    CHECK(demo_boost_next(4) == 8 && demo_boost_next(8) == 16);
    CHECK(demo_boost_next(16) == 32 && demo_boost_next(32) == 1);
    CHECK(demo_boost_next(99) == 1);
    CHECK(demo_safe_boost(8, 2000) == 8);
    CHECK(demo_safe_boost(32, 500) == 32);
    CHECK(demo_safe_boost(32, 2000) == 16);
    CHECK(demo_safe_boost(32, UINT32_MAX) == 1);
    CHECK(demo_safe_boost(8, 5000) == 4);
    CHECK(demo_safe_boost(8, 20000) == 1);
    CHECK(demo_safe_boost(8, 32768) == 1);
    CHECK(demo_safe_boost(8, 0) == 1); /* Do not boost digital silence. */
    CHECK(demo_safe_boost(99, 100) == 1);
    CHECK(demo_peak_dbfs(0) == -96);
    CHECK(demo_peak_dbfs(32768) == 0);
    CHECK(demo_peak_dbfs(16384) == -6);
    CHECK(demo_peak_dbfs(3277) == -20);
    CHECK(demo_mic_meter(0) == 0 && demo_mic_meter(32768) == 100);
    CHECK(demo_mic_meter(1000) >= 45); /* Weak input must remain visible. */
    int16_t original[] = {0, 1000, -1000, 32767, -32768};
    int16_t source[5], copy[5];
    memcpy(source, original, sizeof(source));
    CHECK(demo_pcm_copy_boost(copy, source, 5, 4) == 2);
    CHECK(copy[1] == 4000 && copy[2] == -4000 && copy[3] == 32767 && copy[4] == -32768);
    CHECK(memcmp(source, original, sizeof(source)) == 0);
    CHECK(demo_pcm_copy_boost(copy, source, 5, 1) == 0);
    CHECK(memcmp(copy, original, sizeof(copy)) == 0);
    CHECK(demo_pcm_copy_boost(copy, source, 5, 99) == 0);
    CHECK(memcmp(copy, original, sizeof(copy)) == 0);
    CHECK(demo_pcm_copy_boost(copy, source, 5, 32) == 2);
    CHECK(copy[1] == 32000 && copy[2] == -32000);
    CHECK(memcmp(source, original, sizeof(source)) == 0);
    CHECK(demo_tone_sample(0, 16000) == 0);
    CHECK(demo_tone_sample(15999, 16000) == 0);
    CHECK(demo_tone_sample(16000, 16000) == 0);
    CHECK(demo_tone_sample(UINT32_MAX, 16000) == 0);
    CHECK(demo_tone_sample(1, 0) == 0 && demo_tone_sample(1, 99) == 0);
    int32_t peak = 0;
    uint64_t squared = 0;
    for (uint32_t i = 0; i < 16000; ++i) {
        int32_t value = demo_tone_sample(i, 16000);
        int32_t magnitude = value < 0 ? -value : value;
        CHECK(magnitude <= DEMO_TONE_AMPLITUDE);
        if (magnitude > peak)
            peak = magnitude;
        squared += (uint64_t)((int64_t)value * value);
        if (i < 160 || i >= 15840) {
            uint32_t edge = i < 160 ? i : 15999 - i;
            CHECK((uint32_t)magnitude <= DEMO_TONE_AMPLITUDE * edge / 160);
        }
    }
    CHECK(peak >= 23900 && peak <= 24000);
    CHECK(squared / 16000 > 16000U * 16000U);
    return 0;
}
