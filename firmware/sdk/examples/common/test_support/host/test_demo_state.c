#include "demo_state.h"
#include <stdlib.h>
#define CHECK(x)                                                                                                       \
    do {                                                                                                               \
        if (!(x))                                                                                                      \
            abort();                                                                                                   \
    } while (0)
int main(void) {
    demo_state_t s = {0};
    CHECK(!demo_begin(&s, DEMO_PHOTO));
    CHECK(!demo_begin(&s, DEMO_RECORD));
    CHECK(!demo_begin(&s, DEMO_PLAY));
    CHECK(!demo_begin(&s, DEMO_VOLUME));
    CHECK(!demo_begin(&s, DEMO_TONE_TEST));
    s.camera_ready = true;
    CHECK(demo_begin(&s, DEMO_PHOTO));
    CHECK(s.phase == DEMO_PREPARING);
    s.audio_ready = true;
    CHECK(!demo_begin(&s, DEMO_SPEAKER_TEST));
    demo_finish(&s, false);
    CHECK(!demo_begin(&s, DEMO_PLAY));
    CHECK(demo_begin(&s, DEMO_RECORD));
    CHECK(!s.recording_ready);
    demo_recorded(&s, true);
    demo_finish(&s, true);
    CHECK(s.recording_ready && demo_begin(&s, DEMO_PLAY));
    demo_finish(&s, true);
    CHECK(s.recording_ready && demo_begin(&s, DEMO_PLAY)); /* replay is allowed */
    demo_finish(&s, false);
    CHECK(s.recording_ready); /* playback failure does not erase a recording */
    CHECK(demo_begin(&s, DEMO_RECORD));
    CHECK(!s.recording_ready); /* partial overwrite must never become playable */
    demo_recorded(&s, false);
    demo_finish(&s, false);
    CHECK(!demo_begin(&s, DEMO_PLAY));
    CHECK(demo_begin(&s, DEMO_SPEAKER_TEST));
    demo_finish(&s, true);
    CHECK(demo_begin(&s, DEMO_TONE_TEST));
    for (int phase = DEMO_CAPTURING; phase <= DEMO_PLAYING; ++phase) {
        s.phase = (demo_phase_t)phase;
        CHECK(demo_busy(&s));
        CHECK(!demo_begin(&s, DEMO_PHOTO));
        CHECK(!demo_begin(&s, DEMO_RECORD));
        CHECK(!demo_begin(&s, DEMO_PLAY));
        CHECK(!demo_begin(&s, DEMO_SPEAKER_TEST));
        CHECK(!demo_begin(&s, DEMO_VOLUME));
        CHECK(!demo_begin(&s, DEMO_TONE_TEST));
    }
    demo_finish(&s, true);
    CHECK(!demo_begin(&s, 99));
    s.camera_ready = false;
    CHECK(!demo_begin(&s, DEMO_PHOTO));
    CHECK(demo_begin(&s, DEMO_RECORD));
    demo_finish(&s, true);
    CHECK(demo_begin(&s, DEMO_VOLUME));
    CHECK(demo_busy(&s));
    demo_finish(&s, true);
    s.audio_ready = false;
    CHECK(!demo_begin(&s, DEMO_VOLUME));
    unsigned w, h, divisor;
    CHECK(!demo_preview_size(0, 240, &w, &h, &divisor));
    CHECK(demo_preview_size(416, 416, &w, &h, &divisor));
    CHECK(w == 416 && h == 416 && divisor == 1);
    CHECK(demo_preview_size(640, 480, &w, &h, &divisor));
    CHECK(w == 640 && h == 480 && divisor == 1);
    CHECK(!demo_preview_size(65535, 65535, &w, &h, &divisor));
    CHECK(!demo_preview_size(1, 1, NULL, &h, &divisor));
    CHECK(demo_image_zoom(416, 416, 224, 140) == 86);
    CHECK(demo_image_zoom(416, 416, 320, 280) == 172);
    CHECK(demo_image_zoom(0, 416, 320, 280) == 0);
    demo_pcm_stats_t stats = {0};
    int16_t samples[] = {0, -32768, 32767, -1};
    demo_pcm_measure(&stats, samples, 4);
    CHECK(stats.peak == 32768 && stats.samples == 4 && stats.absolute_sum == 65536);
    CHECK(demo_pcm_mean(&stats) == 16384);
    demo_pcm_stats_t quiet = {0};
    CHECK(demo_pcm_mean(&quiet) == 0);
    return 0;
}
