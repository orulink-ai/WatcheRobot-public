#pragma once
#include "watche_hw_core.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Signed little-endian 16-bit mono PCM, 16000 or 24000 Hz; full duplex.
 * A single owner task must call init/read/write/close. Calls are blocking,
 * never call from an ISR, MCU/camera callback or while holding LVGL lock.
 * Read fills caller memory; write memory must be writable (codec SW volume).
 * Shared bus/codec are leased exclusively; no wake-word or idle policy. */
esp_err_t watche_hw_audio_init(uint32_t sample_rate, uint8_t volume_percent);
esp_err_t watche_hw_audio_read(void *pcm, size_t bytes);
esp_err_t watche_hw_audio_write(void *pcm, size_t bytes);
esp_err_t watche_hw_audio_volume(uint8_t percent);
esp_err_t watche_hw_audio_close(void);
esp_err_t watche_hw_audio_status(watche_hw_status_t *status);
#ifdef __cplusplus
}
#endif
