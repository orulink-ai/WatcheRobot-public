#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
typedef enum {
    DEMO_IDLE,
    DEMO_CAPTURING,
    DEMO_DECODING,
    DEMO_PREPARING,
    DEMO_RECORDING,
    DEMO_PLAYING,
    DEMO_COMPLETE,
    DEMO_ERROR
} demo_phase_t;
typedef enum {
    DEMO_PHOTO = 1,
    DEMO_RECORD = 2,
    DEMO_PLAY = 3,
    DEMO_SPEAKER_TEST = 4,
    DEMO_VOLUME = 5,
    DEMO_TONE_TEST = 6
} demo_action_t;
typedef struct {
    demo_phase_t phase;
    bool camera_ready;
    bool audio_ready;
    bool recording_ready;
} demo_state_t;
/* Application/UI owner serializes state; no hardware or LVGL dependency. */
bool demo_busy(const demo_state_t *state);
bool demo_begin(demo_state_t *state, int action);
void demo_finish(demo_state_t *state, bool success);
void demo_recorded(demo_state_t *state, bool success);
bool demo_preview_size(unsigned width, unsigned height, unsigned *out_width, unsigned *out_height, unsigned *divisor);
unsigned demo_image_zoom(unsigned width, unsigned height, unsigned area_width, unsigned area_height);
typedef struct {
    uint32_t peak;
    uint64_t absolute_sum;
    size_t samples;
} demo_pcm_stats_t;
void demo_pcm_measure(demo_pcm_stats_t *stats, const int16_t *pcm, size_t count);
unsigned demo_pcm_mean(const demo_pcm_stats_t *stats);
