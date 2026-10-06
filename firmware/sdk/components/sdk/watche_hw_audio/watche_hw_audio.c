#include "watche_hw_audio.h"
#include "hal_audio.h"
#include <limits.h>
static esp_codec_dev_handle_t mic, speaker;
static watche_hw_status_t state;
static uint32_t active_rate;
static esp_err_t converted(int result) { return result == 0 ? ESP_OK : ESP_FAIL; }
esp_err_t watche_hw_audio_init(uint32_t rate, uint8_t volume) {
    if ((rate != 16000 && rate != 24000) || volume > 100) return ESP_ERR_INVALID_ARG;
    if (state.state == WATCHE_HW_READY) return active_rate == rate ? watche_hw_audio_volume(volume) : ESP_ERR_INVALID_STATE;
    if (hal_audio_external_lease_is_active() || hal_audio_is_running()) return ESP_ERR_INVALID_STATE;
    esp_err_t ret = converted(hal_audio_external_lease_acquire());
    if (ret != ESP_OK) { state = (watche_hw_status_t){WATCHE_HW_FAULT, ret}; return ret; }
    ret = converted(hal_audio_external_lease_get_handles(&mic, &speaker));
    esp_codec_dev_sample_info_t format = { .sample_rate = rate, .channel = 1, .bits_per_sample = 16 };
    if (ret == ESP_OK) ret = converted(esp_codec_dev_open(mic, &format));
    if (ret == ESP_OK) ret = converted(esp_codec_dev_open(speaker, &format));
    if (ret == ESP_OK) ret = converted(esp_codec_dev_set_out_vol(speaker, volume));
    if (ret != ESP_OK) {
        (void)hal_audio_external_lease_release(); mic = NULL; speaker = NULL;
    } else active_rate = rate;
    state = (watche_hw_status_t){ret == ESP_OK ? WATCHE_HW_READY : WATCHE_HW_FAULT, ret};
    return ret;
}
static esp_err_t transfer(void *pcm, size_t bytes, bool playback) {
    if (pcm == NULL || bytes == 0 || bytes > INT_MAX || bytes % 2 != 0) return ESP_ERR_INVALID_ARG;
    if (state.state != WATCHE_HW_READY) return ESP_ERR_INVALID_STATE;
    state.last_error = converted(playback ? esp_codec_dev_write(speaker, pcm, (int)bytes) : esp_codec_dev_read(mic, pcm, (int)bytes));
    return state.last_error;
}
esp_err_t watche_hw_audio_read(void *pcm, size_t bytes) { return transfer(pcm, bytes, false); }
esp_err_t watche_hw_audio_write(void *pcm, size_t bytes) { return transfer(pcm, bytes, true); }
esp_err_t watche_hw_audio_volume(uint8_t volume) {
    if (volume > 100) return ESP_ERR_INVALID_ARG;
    if (state.state != WATCHE_HW_READY) return ESP_ERR_INVALID_STATE;
    state.last_error = converted(esp_codec_dev_set_out_vol(speaker, volume)); return state.last_error;
}
esp_err_t watche_hw_audio_close(void) {
    if (mic == NULL && speaker == NULL) { state = (watche_hw_status_t){WATCHE_HW_CLOSED, ESP_OK}; return ESP_OK; }
    esp_err_t ret = converted(hal_audio_external_lease_release());
    /* HAL invalidates its lease even when codec stop reports an error. */
    if (!hal_audio_external_lease_is_active()) { mic = NULL; speaker = NULL; active_rate = 0; }
    state = (watche_hw_status_t){ret == ESP_OK ? WATCHE_HW_CLOSED : WATCHE_HW_FAULT, ret}; return ret;
}
esp_err_t watche_hw_audio_status(watche_hw_status_t *out) {
    if (out == NULL) return ESP_ERR_INVALID_ARG;
    *out = state; return ESP_OK;
}

