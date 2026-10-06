#include "hal_audio.h"
#include <assert.h>
static bool leased, running;
static int step, fail_step, releases, release_result;
static int result(void) { return ++step == fail_step ? -1 : 0; }
bool hal_audio_external_lease_is_active(void) { return leased; }
bool hal_audio_is_running(void) { return running; }
int hal_audio_external_lease_acquire(void) { int r=result(); if(!r) leased=true; return r; }
int hal_audio_external_lease_release(void) { leased=false; releases++; return release_result; }
int hal_audio_external_lease_get_handles(void **mic, void **speaker) { *mic=(void*)1; *speaker=(void*)2; return result(); }
int esp_codec_dev_open(void *handle, esp_codec_dev_sample_info_t *format) {
    assert(handle && format->channel==1 && format->bits_per_sample==16); return result();
}
int esp_codec_dev_set_out_vol(void *handle, int volume) { assert(handle && volume<=100); return result(); }
int esp_codec_dev_read(void *handle, void *pcm, int size) { assert(handle && pcm && size==4); return result(); }
int esp_codec_dev_write(void *handle, void *pcm, int size) { assert(handle && pcm && size==4); return result(); }
int main(void) {
    int16_t pcm[2]={0}; watche_hw_status_t status;
    assert(watche_hw_audio_init(44100,30)==ESP_ERR_INVALID_ARG);
    assert(watche_hw_audio_init(16000,101)==ESP_ERR_INVALID_ARG);
    assert(watche_hw_audio_read(pcm,4)==ESP_ERR_INVALID_STATE);
    leased=true; assert(watche_hw_audio_init(16000,30)==ESP_ERR_INVALID_STATE); leased=false;
    running=true; assert(watche_hw_audio_init(16000,30)==ESP_ERR_INVALID_STATE); running=false;
    for(int i=1;i<=5;i++) {
        step=0; fail_step=i;
        assert(watche_hw_audio_init(16000,30)==ESP_FAIL);
        assert(!leased);
        assert(watche_hw_audio_status(&status)==ESP_OK && status.state==WATCHE_HW_FAULT);
        assert(watche_hw_audio_close()==ESP_OK);
    }
    fail_step=0;
    for(int i=0;i<3;i++) {
        assert(watche_hw_audio_init(16000,30)==ESP_OK && leased);
        assert(watche_hw_audio_init(24000,30)==ESP_ERR_INVALID_STATE);
        assert(watche_hw_audio_read(NULL,4)==ESP_ERR_INVALID_ARG);
        assert(watche_hw_audio_read(pcm,3)==ESP_ERR_INVALID_ARG);
        assert(watche_hw_audio_read(pcm,4)==ESP_OK);
        assert(watche_hw_audio_write(pcm,4)==ESP_OK);
        release_result=-1;
        assert(watche_hw_audio_close()==ESP_FAIL && !leased);
        int before=releases;
        assert(watche_hw_audio_close()==ESP_OK && releases==before);
        release_result=0;
    }
    return 0;
}

