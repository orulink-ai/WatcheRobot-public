#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define DEMO_OUTPUT_DEFAULT 90
#define DEMO_OUTPUT_MAX 95
#define DEMO_TONE_AMPLITUDE 24000
/* Application-only helpers. The SDK remains raw PCM, without auto gain. */
unsigned demo_volume_step(unsigned current, int delta);
unsigned demo_boost_next(unsigned current);
unsigned demo_safe_boost(unsigned requested, uint32_t peak);
int demo_peak_dbfs(uint32_t peak);
unsigned demo_mic_meter(uint32_t peak);
/* One-second 660 Hz manual test tone; 10 ms fades. Out-of-range positions or
 * sample rates below 100 Hz return silence. No output hardware is accessed. */
int16_t demo_tone_sample(uint32_t position, uint32_t sample_rate);
/* Copy into writable codec scratch; source remains immutable. Clamp as a final
 * safety guard, including negative full scale. Returns clipped sample count. */
size_t demo_pcm_copy_boost(int16_t *output, const int16_t *input, size_t count, unsigned boost);
