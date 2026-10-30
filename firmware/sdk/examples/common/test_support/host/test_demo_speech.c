#include "demo_speech.h"
#include <stdio.h>
#include <stdlib.h>
#define CHECK(x)                                                                                                       \
    do {                                                                                                               \
        if (!(x)) {                                                                                                    \
            fprintf(stderr, "Failed: %s line %d\n", #x, __LINE__);                                                     \
            exit(1);                                                                                                   \
        }                                                                                                              \
    } while (0)
int main(void) {
    unsigned rate = DEMO_SPEECH_RATE, count = DEMO_SPEECH_SAMPLES;
    CHECK(rate == 16000 && count > 48000 && count < 80000);
    uint32_t hash = 2166136261U, peak = 0;
    uint64_t squares = 0;
    for (unsigned i = 0; i < DEMO_SPEECH_SAMPLES; i++) {
        int32_t value = demo_speech_pcm[i];
        uint32_t magnitude = (uint32_t)(value < 0 ? -value : value);
        if (magnitude > peak)
            peak = magnitude;
        squares += (uint64_t)magnitude * magnitude;
        /* FNV-1a over canonical little-endian PCM, independent of host endian. */
        uint16_t raw = (uint16_t)demo_speech_pcm[i];
        hash = (hash ^ (raw & 255U)) * 16777619U;
        hash = (hash ^ (raw >> 8)) * 16777619U;
    }
    CHECK(hash == 2171032818U && peak == 30000);
    CHECK(squares / DEMO_SPEECH_SAMPLES > 4100U * 4100U);
    CHECK(squares / DEMO_SPEECH_SAMPLES < 4300U * 4300U);
    CHECK(demo_speech_pcm[0] == 0 && demo_speech_pcm[DEMO_SPEECH_SAMPLES - 1] == 0);
    return 0;
}
