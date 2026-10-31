#include "demo_playback.h"
#include "demo_audio.h"
#include <string.h>
int demo_playback_run(const demo_playback_source_t *source, const demo_playback_sink_t *sink, int16_t *scratch,
                      size_t capacity, demo_playback_result_t *result) {
    if (!result)
        return -1;
    memset(result, 0, sizeof(*result));
    if (!source || !sink || !sink->write || !sink->drain || !scratch || !capacity ||
        capacity > SIZE_MAX / sizeof(*scratch) || !source->samples || source->samples > SIZE_MAX / sizeof(*scratch))
        return -1;
    if (source->tone_rate) {
        if (source->pcm || source->tone_rate < 100 || source->samples != source->tone_rate || source->boost != 1)
            return -1;
    } else if (!source->pcm || !(source->boost == 1 || source->boost == 2 || source->boost == 4 || source->boost == 8 ||
                                 source->boost == 16 || source->boost == 32))
        return -1;

    int error = 0;
    if (sink->progress)
        sink->progress(sink->context, 0, source->samples);
    while (result->submitted < source->samples) {
        size_t count = source->samples - result->submitted;
        if (count > capacity)
            count = capacity;
        if (source->tone_rate) {
            for (size_t i = 0; i < count; i++)
                scratch[i] = demo_tone_sample((uint32_t)(result->submitted + i), source->tone_rate);
        } else
            result->clipped += demo_pcm_copy_boost(scratch, source->pcm + result->submitted, count, source->boost);
        error = sink->write(sink->context, scratch, count * sizeof(*scratch));
        if (error)
            break; /* The failed call may have submitted a prefix. Never retry it. */
        result->submitted += count;
        if (sink->progress && result->submitted < source->samples)
            sink->progress(sink->context, result->submitted, source->samples);
    }
    int drain_error = sink->drain(sink->context);
    if (!error)
        error = drain_error;
    if (!error && sink->progress)
        sink->progress(sink->context, source->samples, source->samples);
    return error;
}
