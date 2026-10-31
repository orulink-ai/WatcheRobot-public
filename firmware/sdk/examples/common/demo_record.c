#include "demo_record.h"
#include <string.h>
int demo_record_run(int16_t *pcm, size_t samples, size_t chunk, const demo_record_sink_t *sink, demo_pcm_stats_t *total) {
    if (!total) return -1;
    memset(total, 0, sizeof(*total));
    if (!pcm || !samples || !chunk || samples > SIZE_MAX / sizeof(*pcm) || !sink || !sink->read) return -1;
    while (total->samples < samples) {
        size_t count = samples - total->samples;
        if (count > chunk) count = chunk;
        int16_t *block = pcm + total->samples;
        int ret = sink->read(sink->context, block, count * sizeof(*block));
        if (ret) return ret;
        demo_pcm_stats_t level = {0};
        demo_pcm_measure(&level, block, count);
        demo_pcm_measure(total, block, count);
        if (sink->progress) sink->progress(sink->context, total->samples, samples, level.peak);
    }
    return 0;
}
