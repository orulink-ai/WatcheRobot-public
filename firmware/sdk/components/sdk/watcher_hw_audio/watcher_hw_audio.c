#include "watcher_hw_audio.h"
#include "hal_audio.h"
#include <limits.h>
static esp_codec_dev_handle_t mic, speaker;
static watcher_hw_status_t state;
static uint32_t active_rate;
static bool owns_lease;
static esp_err_t converted(int result) {
    return result == 0 ? ESP_OK : ESP_FAIL;
}
esp_err_t watcher_hw_audio_init(uint32_t rate, uint8_t volume) {
    if ((rate != 16000 && rate != 24000) || volume > 100)
        return ESP_ERR_INVALID_ARG;
    if (state.state == WATCHER_HW_READY)
        return active_rate == rate ? watcher_hw_audio_volume(volume) : ESP_ERR_INVALID_STATE;
    if (hal_audio_external_lease_is_active() || hal_audio_is_running())
        return ESP_ERR_INVALID_STATE;
    esp_err_t ret = converted(hal_audio_external_lease_acquire());
    if (ret != ESP_OK) {
        state = (watcher_hw_status_t){WATCHER_HW_FAULT, ret};
        return ret;
    }
    owns_lease = true;
    ret = converted(hal_audio_external_lease_get_handles(&mic, &speaker));
    /* BSP tracks open paths and selects the physical microphone slot.
     * Direct codec opens would bypass those flags and mismatch shared I2S. */
    if (ret == ESP_OK)
        ret = converted(hal_audio_external_lease_configure(rate));
    if (ret == ESP_OK)
        ret = converted(hal_audio_external_lease_prepare_output(volume));
    if (ret != ESP_OK) {
        (void)watcher_hw_audio_close();
    } else
        active_rate = rate;
    state = (watcher_hw_status_t){ret == ESP_OK ? WATCHER_HW_READY : WATCHER_HW_FAULT, ret};
    return ret;
}
static esp_err_t transfer(void *pcm, size_t bytes, bool playback) {
    if (pcm == NULL || bytes == 0 || bytes > INT_MAX || bytes % 2 != 0)
        return ESP_ERR_INVALID_ARG;
    if (state.state != WATCHER_HW_READY)
        return ESP_ERR_INVALID_STATE;
    state.last_error =
        converted(playback ? esp_codec_dev_write(speaker, pcm, (int)bytes) : esp_codec_dev_read(mic, pcm, (int)bytes));
    return state.last_error;
}
esp_err_t watcher_hw_audio_read(void *pcm, size_t bytes) {
    return transfer(pcm, bytes, false);
}
esp_err_t watcher_hw_audio_write(void *pcm, size_t bytes) {
    return transfer(pcm, bytes, true);
}
esp_err_t watcher_hw_audio_drain(uint32_t timeout_ms) {
    if (timeout_ms == 0)
        return ESP_ERR_INVALID_ARG;
    if (state.state != WATCHER_HW_READY)
        return ESP_ERR_INVALID_STATE;
    state.last_error = converted(hal_audio_external_lease_drain(timeout_ms));
    return state.last_error;
}
esp_err_t watcher_hw_audio_volume(uint8_t volume) {
    if (volume > 100)
        return ESP_ERR_INVALID_ARG;
    if (state.state != WATCHER_HW_READY)
        return ESP_ERR_INVALID_STATE;
    state.last_error = converted(hal_audio_external_lease_prepare_output(volume));
    return state.last_error;
}
esp_err_t watcher_hw_audio_close(void) {
    if (!owns_lease) {
        state = (watcher_hw_status_t){WATCHER_HW_CLOSED, ESP_OK};
        return ESP_OK;
    }
    esp_err_t ret = converted(hal_audio_external_lease_release());
    /* Failed cleanup may retain the lease, even if handles were never obtained. */
    if (!hal_audio_external_lease_is_active()) {
        owns_lease = false;
        mic = NULL;
        speaker = NULL;
        active_rate = 0;
    }
    state = (watcher_hw_status_t){ret == ESP_OK ? WATCHER_HW_CLOSED : WATCHER_HW_FAULT, ret};
    return ret;
}
esp_err_t watcher_hw_audio_status(watcher_hw_status_t *out) {
    if (out == NULL)
        return ESP_ERR_INVALID_ARG;
    *out = state;
    return ESP_OK;
}
