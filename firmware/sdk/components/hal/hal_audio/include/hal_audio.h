#ifndef HAL_AUDIO_H
#define HAL_AUDIO_H

#include <stdbool.h>
#include <stdint.h>

#include "esp_codec_dev.h"

typedef void (*hal_audio_capture_activity_cb_t)(bool active, void *ctx);

/**
 * Register the process-wide microphone capture activity observer.
 *
 * Passing NULL unregisters the observer. The current state is replayed after
 * registration. Callbacks run synchronously in the calling task, outside the
 * audio HAL lock, and must remain brief. This API is not ISR-safe.
 */
int hal_audio_set_capture_activity_callback(hal_audio_capture_activity_cb_t cb, void *ctx);

/**
 * Initialize audio codec (call once at startup)
 * This initializes ES7243 ADC and ES8311 DAC via I2C
 */
int hal_audio_init(void);

/**
 * Fully release the audio codec and I2S resources owned by the audio HAL.
 *
 * Use this when the foreground audio application exits. Playback/recording
 * helpers will lazily reinitialize the codec on the next start/write/read.
 */
int hal_audio_deinit(void);

/**
 * Start audio capture/playback
 */
int hal_audio_start(void);

/**
 * Read audio samples from microphone
 * @param out_buf Output buffer
 * @param max_len Maximum length
 * @return Number of bytes read, or -1 on error
 */
int hal_audio_read(uint8_t *out_buf, int max_len);

/**
 * Write audio samples to speaker
 * @param data Audio data
 * @param len Data length
 * @return Number of bytes written, or -1 on error
 */
int hal_audio_write(const uint8_t *data, int len);

/**
 * Wait for PCM already accepted by the I2S driver to leave the TX DMA ring.
 * The caller must keep playback active until this function returns.
 */
int hal_audio_drain_playback(uint32_t timeout_ms);

/**
 * Stop audio capture/playback
 */
int hal_audio_stop(void);

/**
 * Declare whether a live wake-word runtime currently needs the shared
 * microphone path to stay at 16 kHz after playback users stop.
 */
void hal_audio_set_wake_word_stream_desired(bool enable);

/**
 * Put the shared audio path into app-idle state.
 *
 * Use this for foreground application teardown. Unlike hal_audio_stop(), this
 * does not keep the wake-word microphone stream alive.
 */
int hal_audio_enter_app_idle(void);

/**
 * Release the audio path after one-shot local playback when no foreground
 * owner needs it anymore.
 *
 * This fully deinitializes the codec in normal builds so boot/local SFX do
 * not leave DMA/internal heap pinned. Wake-word builds keep the live mic path
 * when it has explicitly requested ownership.
 */
int hal_audio_release_idle(void);

/**
 * Grant an external full-duplex media pipeline exclusive access to the BSP
 * codec handles. Normal HAL playback/capture calls are rejected until release.
 */
int hal_audio_external_lease_acquire(void);

/** Return the codec handles only while the external lease is active. */
int hal_audio_external_lease_get_handles(esp_codec_dev_handle_t *microphone, esp_codec_dev_handle_t *speaker);

/** Configure PCM through BSP tracking; only while the external lease is held. */
int hal_audio_external_lease_configure(uint32_t sample_rate);

/** Close any externally opened paths and return ownership to the normal HAL. */
int hal_audio_external_lease_release(void);

bool hal_audio_external_lease_is_active(void);

/**
 * Set sample rate for playback (call before TTS playback)
 * @param sample_rate Sample rate in Hz (e.g., 16000, 24000)
 */
void hal_audio_set_sample_rate(uint32_t sample_rate);

/**
 * Set speaker volume for subsequent playback.
 * @param volume_percent 0-100
 */
void hal_audio_set_volume(uint8_t volume_percent);

/**
 * Get current speaker volume setting.
 */
uint8_t hal_audio_get_volume(void);

/**
 * Mark audio as being used for playback (not just recording)
 * @param enable true for TTS playback mode, false for recording mode
 */
void hal_audio_set_playback_mode(bool enable);

/**
 * Query whether the shared audio path is currently marked running.
 */
bool hal_audio_is_running(void);
bool hal_audio_is_initialized(void);

/**
 * Query whether the shared audio path is currently configured for playback.
 */
bool hal_audio_is_playback_mode(void);

#endif /* HAL_AUDIO_H */
