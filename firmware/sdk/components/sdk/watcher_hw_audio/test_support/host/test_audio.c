#include "hal_audio.h"
#include <assert.h>
static bool leased, running, retain_on_release_failure;
static int step, fail_step, releases, release_result, drains;
static int result(void) {
    return ++step == fail_step ? -1 : 0;
}
int hal_audio_external_lease_drain(uint32_t timeout_ms) {
    assert(leased && timeout_ms > 0);
    drains++;
    return result();
}
bool hal_audio_external_lease_is_active(void) {
    return leased;
}
bool hal_audio_is_running(void) {
    return running;
}
int hal_audio_external_lease_acquire(void) {
    int r = result();
    if (!r)
        leased = true;
    return r;
}
int hal_audio_external_lease_release(void) {
    if (!release_result || !retain_on_release_failure)
        leased = false;
    releases++;
    return release_result;
}
int hal_audio_external_lease_get_handles(void **mic, void **speaker) {
    *mic = (void *)1;
    *speaker = (void *)2;
    return result();
}
int hal_audio_external_lease_configure(uint32_t rate) {
    assert(leased && (rate == 16000 || rate == 24000));
    return result();
}
int esp_codec_dev_open(void *handle, esp_codec_dev_sample_info_t *format) {
    (void)handle;
    (void)format;
    assert(!"SDK must configure through HAL/BSP ownership");
    return -1;
}
int esp_codec_dev_set_out_vol(void *handle, int volume) {
    (void)handle;
    (void)volume;
    assert(!"SDK must prepare PA, mute and volume through its HAL lease");
    return -1;
}
int hal_audio_external_lease_prepare_output(uint8_t volume) {
    assert(leased && volume <= 100);
    return result();
}
int esp_codec_dev_read(void *handle, void *pcm, int size) {
    assert(handle && pcm && size == 4);
    return result();
}
int esp_codec_dev_write(void *handle, void *pcm, int size) {
    assert(handle && pcm && size == 4);
    return result();
}
int main(void) {
    int16_t pcm[2] = {0};
    watcher_hw_status_t status;
    assert(watcher_hw_audio_init(44100, 30) == ESP_ERR_INVALID_ARG);
    assert(watcher_hw_audio_init(16000, 101) == ESP_ERR_INVALID_ARG);
    assert(watcher_hw_audio_read(pcm, 4) == ESP_ERR_INVALID_STATE);
    assert(watcher_hw_audio_drain(1000) == ESP_ERR_INVALID_STATE && drains == 0);
    assert(watcher_hw_audio_drain(0) == ESP_ERR_INVALID_ARG && drains == 0);
    leased = true;
    assert(watcher_hw_audio_init(16000, 30) == ESP_ERR_INVALID_STATE);
    leased = false;
    running = true;
    assert(watcher_hw_audio_init(16000, 30) == ESP_ERR_INVALID_STATE);
    running = false;
    for (int i = 1; i <= 4; i++) {
        step = 0;
        fail_step = i;
        assert(watcher_hw_audio_init(16000, 30) == ESP_FAIL);
        assert(!leased);
        assert(watcher_hw_audio_status(&status) == ESP_OK && status.state == WATCHER_HW_FAULT);
        assert(watcher_hw_audio_close() == ESP_OK);
    }
    /* A failed rollback must retain our ownership, including get_handles failure. */
    for (int i = 2; i <= 4; ++i) {
        step = 0;
        fail_step = i;
        release_result = -1;
        retain_on_release_failure = true;
        assert(watcher_hw_audio_init(16000, 30) == ESP_FAIL && leased);
        int before = releases;
        assert(watcher_hw_audio_close() == ESP_FAIL && leased && releases == before + 1);
        release_result = 0;
        assert(watcher_hw_audio_close() == ESP_OK && !leased);
        fail_step = 0;
        assert(watcher_hw_audio_init(16000, 30) == ESP_OK);
        assert(watcher_hw_audio_close() == ESP_OK);
    }
    retain_on_release_failure = false;
    /* Closing an idle caller must not release somebody else's lease. */
    leased = true;
    int before_foreign_close = releases;
    assert(watcher_hw_audio_close() == ESP_OK && leased && releases == before_foreign_close);
    leased = false;
    fail_step = 0;
    for (int i = 0; i < 3; i++) {
        assert(watcher_hw_audio_init(16000, 30) == ESP_OK && leased);
        assert(watcher_hw_audio_init(24000, 30) == ESP_ERR_INVALID_STATE);
        assert(watcher_hw_audio_read(NULL, 4) == ESP_ERR_INVALID_ARG);
        assert(watcher_hw_audio_read(pcm, 3) == ESP_ERR_INVALID_ARG);
        assert(watcher_hw_audio_read(pcm, 4) == ESP_OK);
        assert(watcher_hw_audio_write(pcm, 4) == ESP_OK);
        int before_drain = drains;
        assert(watcher_hw_audio_drain(0) == ESP_ERR_INVALID_ARG && drains == before_drain);
        assert(watcher_hw_audio_drain(1000) == ESP_OK && drains == before_drain + 1);
        fail_step = step + 1;
        assert(watcher_hw_audio_drain(1000) == ESP_FAIL);
        assert(watcher_hw_audio_status(&status) == ESP_OK && status.last_error == ESP_FAIL);
        fail_step = 0;
        assert(watcher_hw_audio_drain(1000) == ESP_OK); /* Retryable; keeps the lease. */
        release_result = -1;
        assert(watcher_hw_audio_close() == ESP_FAIL && !leased);
        assert(watcher_hw_audio_drain(1000) == ESP_ERR_INVALID_STATE);
        int before = releases;
        assert(watcher_hw_audio_close() == ESP_OK && releases == before);
        release_result = 0;
    }
    return 0;
}
