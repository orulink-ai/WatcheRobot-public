#include "demo_speech.h"
/* Read-only flash, copied to writable scratch before SDK playback. */
const int16_t demo_speech_pcm[DEMO_SPEECH_SAMPLES] = {
#include "assets/speaker_pcm.inc"
};
