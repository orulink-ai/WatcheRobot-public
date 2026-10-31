/* Compile the real codec device implementation; replace hardware only. */
#include "audio_codec_sw_vol.h"
#include "esp_codec_dev.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#undef assert
#define assert(x) do { if (!(x)) { fprintf(stderr, "Assertion failed: %s at %d\n", #x, __LINE__); exit(1); } } while (0)
static int failure, disables;
static bool fail_input_codec_stop, fail_input_data_stop;
static float software_db;
static int software_volume(const audio_codec_vol_if_t *h, float db) {
    (void)h;
    software_db = db;
    return 0;
}
enum { FAIL_VOLUME = 1, FAIL_MUTE, FAIL_FORMAT, FAIL_ENABLE, FAIL_FS, FAIL_CODEC_ENABLE };
static int volume(const audio_codec_if_t *h, float db) {
    (void)h;
    (void)db;
    return failure == FAIL_VOLUME ? ESP_CODEC_DEV_DRV_ERR : 0;
}
static int mute(const audio_codec_if_t *h, bool value) {
    (void)h;
    (void)value;
    return failure == FAIL_MUTE ? ESP_CODEC_DEV_DRV_ERR : 0;
}
static int fmt(const audio_codec_data_if_t *h, esp_codec_dev_type_t type, esp_codec_dev_sample_info_t *fs) {
    (void)h;
    (void)type;
    (void)fs;
    return failure == FAIL_FORMAT ? ESP_CODEC_DEV_DRV_ERR : 0;
}
static int enable(const audio_codec_data_if_t *h, esp_codec_dev_type_t type, bool value) {
    (void)h;
    (void)type;
    if (!value)
        disables++;
    return (value && failure == FAIL_ENABLE) || (!value && fail_input_data_stop) ? ESP_CODEC_DEV_DRV_ERR : 0;
}
static int codec_enable(const audio_codec_if_t *h, bool value) {
    (void)h;
    return (value && failure == FAIL_CODEC_ENABLE) || (!value && fail_input_codec_stop) ? ESP_CODEC_DEV_DRV_ERR : 0;
}
static int fs(const audio_codec_if_t *h, esp_codec_dev_sample_info_t *sample) {
    (void)h;
    (void)sample;
    return failure == FAIL_FS ? ESP_CODEC_DEV_DRV_ERR : 0;
}
static int write_pcm(const audio_codec_data_if_t *h, uint8_t *data, int size) {
    (void)h;
    (void)data;
    (void)size;
    return 0;
}
const audio_codec_vol_if_t *audio_codec_new_sw_vol(void) {
    return NULL;
}
int audio_codec_delete_vol_if(const audio_codec_vol_if_t *h) {
    (void)h;
    return 0;
}
int main(void) {
    audio_codec_if_t codec = {.set_vol = volume, .mute = mute, .set_fs = fs, .enable = codec_enable};
    audio_codec_data_if_t data = {.set_fmt = fmt, .enable = enable, .write = write_pcm};
    esp_codec_dev_cfg_t config = {.codec_if = &codec, .data_if = &data, .dev_type = ESP_CODEC_DEV_TYPE_OUT};
    esp_codec_dev_handle_t dev = esp_codec_dev_new(&config);
    assert(dev);
    assert(esp_codec_dev_set_out_vol(dev, 60) == 0);
    failure = FAIL_VOLUME;
    assert(esp_codec_dev_set_out_vol(dev, 80) == ESP_CODEC_DEV_DRV_ERR);
    int cached = 0;
    assert(esp_codec_dev_get_out_vol(dev, &cached) == 0 && cached == 60);
    failure = FAIL_MUTE;
    assert(esp_codec_dev_set_out_mute(dev, true) == ESP_CODEC_DEV_DRV_ERR);
    bool muted = true;
    assert(esp_codec_dev_get_out_mute(dev, &muted) == 0 && !muted);
    esp_codec_dev_sample_info_t sample = {.sample_rate = 16000, .bits_per_sample = 16, .channel = 1};
    uint8_t pcm[4] = {0};
    for (int fail = FAIL_VOLUME; fail <= FAIL_CODEC_ENABLE; fail++) {
        failure = fail;
        assert(esp_codec_dev_open(dev, &sample) != 0);
        assert(esp_codec_dev_write(dev, pcm, sizeof(pcm)) == ESP_CODEC_DEV_WRONG_STATE);
        failure = 0;
        assert(esp_codec_dev_open(dev, &sample) == 0);
        assert(esp_codec_dev_write(dev, pcm, sizeof(pcm)) == 0);
        assert(esp_codec_dev_close(dev) == 0);
        assert(esp_codec_dev_close(dev) == 0);
    }
    assert(disables >= 6);
    audio_codec_vol_if_t software = {.set_vol = software_volume};
    assert(esp_codec_dev_set_vol_handler(dev, &software) == 0);
    /* A hardware mute must not latch the optional software gain at silence. */
    assert(esp_codec_dev_set_out_mute(dev, true) == 0);
    assert(esp_codec_dev_set_out_vol(dev, 80) == 0 && software_db == -10.0f);
    assert(esp_codec_dev_set_out_mute(dev, false) == 0 && software_db == -10.0f);
    codec.mute = NULL;
    assert(esp_codec_dev_set_out_mute(dev, true) == 0 && software_db == -100.0f);
    assert(esp_codec_dev_set_out_vol(dev, 60) == 0 && software_db == -100.0f);
    assert(esp_codec_dev_set_out_mute(dev, false) == 0 && software_db == -20.0f);
    esp_codec_dev_delete(dev);
    /* Privacy stop must expose actual ADC/data disable failures, retain the
     * opened owner, and really retry instead of reporting already closed. */
    config.dev_type = ESP_CODEC_DEV_TYPE_IN;
    data.read = write_pcm;
    dev = esp_codec_dev_new(&config);
    assert(dev && esp_codec_dev_open(dev, &sample) == 0);
    fail_input_codec_stop = true;
    assert(esp_codec_dev_close(dev) == ESP_CODEC_DEV_DRV_ERR);
    fail_input_codec_stop = false;
    fail_input_data_stop = true;
    assert(esp_codec_dev_close(dev) == ESP_CODEC_DEV_DRV_ERR);
    fail_input_data_stop = false;
    unsigned before = disables;
    assert(esp_codec_dev_close(dev) == 0 && disables == (int)before + 1);
    assert(esp_codec_dev_read(dev, pcm, sizeof(pcm)) == ESP_CODEC_DEV_WRONG_STATE);
    assert(esp_codec_dev_open(dev, &sample) == 0);
    assert(esp_codec_dev_close(dev) == 0);
    esp_codec_dev_delete(dev);
    return 0;
}
