#pragma once
#include "watcher_hw_audio.h"
typedef void *esp_codec_dev_handle_t;
typedef struct {
    int sample_rate, channel, bits_per_sample;
} esp_codec_dev_sample_info_t;
bool hal_audio_external_lease_is_active(void);
bool hal_audio_is_running(void);
int hal_audio_external_lease_acquire(void);
int hal_audio_external_lease_release(void);
int hal_audio_external_lease_get_handles(esp_codec_dev_handle_t *, esp_codec_dev_handle_t *);
int hal_audio_external_lease_configure(uint32_t rate);
int hal_audio_external_lease_prepare_output(uint8_t volume);
int hal_audio_external_lease_drain(uint32_t timeout_ms);
int esp_codec_dev_open(esp_codec_dev_handle_t, esp_codec_dev_sample_info_t *);
int esp_codec_dev_set_out_vol(esp_codec_dev_handle_t, int);
int esp_codec_dev_read(esp_codec_dev_handle_t, void *, int);
int esp_codec_dev_write(esp_codec_dev_handle_t, void *, int);
