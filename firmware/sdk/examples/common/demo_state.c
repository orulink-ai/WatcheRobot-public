#include "demo_state.h"
#include <stddef.h>
bool demo_busy(const demo_state_t *state) {
    return state->phase >= DEMO_CAPTURING && state->phase <= DEMO_PLAYING;
}
bool demo_begin(demo_state_t *state, int action) {
    if (demo_busy(state))
        return false;
    if (action == DEMO_PHOTO && state->camera_ready)
        state->phase = DEMO_PREPARING;
    else if (action == DEMO_VOLUME && state->audio_ready)
        state->phase = DEMO_PREPARING;
    else if (action == DEMO_RECORD && state->audio_ready) {
        state->recording_ready = false;
        state->phase = DEMO_PREPARING;
    } else if ((action == DEMO_PLAY && state->audio_ready && state->recording_ready) ||
               ((action == DEMO_SPEAKER_TEST || action == DEMO_TONE_TEST) && state->audio_ready))
        state->phase = DEMO_PLAYING;
    else
        return false;
    return true;
}
void demo_finish(demo_state_t *state, bool success) {
    state->phase = success ? DEMO_COMPLETE : DEMO_ERROR;
}
void demo_recorded(demo_state_t *state, bool success) {
    state->recording_ready = success;
}
bool demo_preview_size(unsigned width, unsigned height, unsigned *out_width, unsigned *out_height, unsigned *divisor) {
    if (!width || !height || !out_width || !out_height || !divisor)
        return false;
    for (unsigned scale = 1; scale <= 8; scale *= 2) {
        unsigned w = width / scale, h = height / scale;
        if (w && h && w <= 640 && h <= 480) {
            *out_width = w;
            *out_height = h;
            *divisor = scale;
            return true;
        }
    }
    return false;
}
unsigned demo_image_zoom(unsigned width, unsigned height, unsigned area_width, unsigned area_height) {
    if (!width || !height || !area_width || !area_height || area_width > 65535 || area_height > 65535)
        return 0;
    unsigned x = area_width * 256 / width;
    unsigned y = area_height * 256 / height;
    unsigned zoom = x < y ? x : y;
    return zoom > 512 ? 512 : zoom;
}
void demo_pcm_measure(demo_pcm_stats_t *stats, const int16_t *pcm, size_t count) {
    for (size_t i = 0; i < count; ++i) {
        int32_t sample = pcm[i];
        uint32_t magnitude = (uint32_t)(sample < 0 ? -sample : sample);
        if (magnitude > stats->peak)
            stats->peak = magnitude;
        stats->absolute_sum += magnitude;
    }
    stats->samples += count;
}
unsigned demo_pcm_mean(const demo_pcm_stats_t *stats) {
    return stats->samples ? (unsigned)(stats->absolute_sum / stats->samples) : 0;
}
