#pragma once
#include "watcher_hw_core.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Signed little-endian 16-bit mono PCM, 16000 or 24000 Hz; full duplex.
 * A single owner task must call init/read/write/close. Calls are blocking,
 * never call from an ISR, MCU/camera callback or while holding LVGL lock.
 * Read fills caller memory; write memory must be writable (codec SW volume).
 * Shared bus/codec are leased exclusively; no wake-word or idle policy.
 * Volume accepts 0..100 on the codec's dB curve, not perceived loudness.
 * BSP caps output at 95 to limit distortion. PCM amplitude also affects level.
 * Init/volume check PA power and mute at 0, unmute at nonzero output. */
esp_err_t watcher_hw_audio_init(uint32_t sample_rate, uint8_t volume_percent);
esp_err_t watcher_hw_audio_read(void *pcm, size_t bytes);
esp_err_t watcher_hw_audio_write(void *pcm, size_t bytes);
/* Wait for already submitted PCM to leave the DMA ring, before indicating
 * completion, changing volume or closing. Same audio-owner task; timeout_ms > 0.
 * Completion proves delivery, not audible quality. Does not release the lease. */
esp_err_t watcher_hw_audio_drain(uint32_t timeout_ms);
esp_err_t watcher_hw_audio_volume(uint8_t percent);
/* After init/close fails, retry close before handing ownership to another user.
 * A failed hardware cleanup retains this caller's lease until HAL releases it. */
esp_err_t watcher_hw_audio_close(void);
esp_err_t watcher_hw_audio_status(watcher_hw_status_t *status);
#ifdef __cplusplus
}
#endif
