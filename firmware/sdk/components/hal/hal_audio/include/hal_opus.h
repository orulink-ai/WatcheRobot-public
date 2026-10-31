#ifndef HAL_OPUS_H
#define HAL_OPUS_H

#include <stdbool.h>
#include <stdint.h>

/* esp_audio_codec documents about 40 KiB of task stack for encoder execution.
 * Keep the owner task tied to this contract so SILK's speech-only deep path
 * cannot silently regress to an unsafe stack size. */
#define HAL_OPUS_MIN_TASK_STACK_BYTES (40U * 1024U)

/** Initialize the 16 kHz mono 60 ms Opus uplink encoder. */
int hal_opus_init(void);

/** Return true only when the real Opus encoder can be initialized. */
bool hal_opus_is_available(void);

/** Reset predictive encoder state at a turn/connection boundary. */
int hal_opus_reset(void);

/** Release encoder resources. */
void hal_opus_deinit(void);

/** Encode exactly one 60 ms PCM16LE frame into one Opus packet. */
int hal_opus_encode(const uint8_t *pcm_in, int pcm_len, uint8_t *out_buf, int out_max_len);

/** Downlink TTS remains PCM; this function intentionally returns an error. */
int hal_opus_decode(const uint8_t *in_data, int in_len, uint8_t *pcm_out, int pcm_max_len);

#endif /* HAL_OPUS_H */
