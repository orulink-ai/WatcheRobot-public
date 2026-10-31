#include "demo_audio.h"
#include <math.h>
unsigned demo_volume_step(unsigned current, int delta) {
    int64_t next = (int64_t)(current > DEMO_OUTPUT_MAX ? DEMO_OUTPUT_MAX : current) + delta;
    return next < 0 ? 0 : next > DEMO_OUTPUT_MAX ? DEMO_OUTPUT_MAX : (unsigned)next;
}
unsigned demo_boost_next(unsigned current) {
    return current == 1 || current == 2 || current == 4 || current == 8 || current == 16 ? current * 2 : 1;
}
static bool supported_boost(unsigned value) {
    return value == 1 || value == 2 || value == 4 || value == 8 || value == 16 || value == 32;
}
unsigned demo_safe_boost(unsigned requested, uint32_t peak) {
    if (!peak || !supported_boost(requested))
        return 1;
    while (requested > 1 && (uint64_t)peak * requested > INT16_MAX)
        requested /= 2;
    return requested;
}
int demo_peak_dbfs(uint32_t peak) {
    if (!peak)
        return -96;
    if (peak >= 32768)
        return 0;
    int db = (int)lroundf(20.0f * log10f(peak / 32768.0f));
    return db < -96 ? -96 : db;
}
unsigned demo_mic_meter(uint32_t peak) {
    int db = demo_peak_dbfs(peak);
    return db <= -60 ? 0 : (unsigned)(db + 60) * 100 / 60;
}
int16_t demo_tone_sample(uint32_t position, uint32_t sample_rate) {
    if (sample_rate < 100 || position >= sample_rate)
        return 0;
    uint32_t edge = position < sample_rate - 1 - position ? position : sample_rate - 1 - position;
    float envelope = fminf(1.0f, edge / (sample_rate / 100.0f));
    return (int16_t)(DEMO_TONE_AMPLITUDE * envelope * sinf(2.0f * 3.14159265f * 660.0f * position / sample_rate));
}
size_t demo_pcm_copy_boost(int16_t *output, const int16_t *input, size_t count, unsigned boost) {
    if (!supported_boost(boost))
        boost = 1;
    size_t clipped = 0;
    for (size_t i = 0; i < count; ++i) {
        int32_t sample = (int32_t)input[i] * (int32_t)boost;
        if (sample > INT16_MAX) {
            sample = INT16_MAX;
            ++clipped;
        } else if (sample < INT16_MIN) {
            sample = INT16_MIN;
            ++clipped;
        }
        output[i] = (int16_t)sample;
    }
    return clipped;
}
