#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"FAIL %s line %d\n",#x,__LINE__); exit(1); } } while (0)
#define ESP_IDF_VERSION 60000
#define ESP_IDF_VERSION_VAL(a,b,c) ((a)*10000+(b)*100+(c))
#ifndef SOC_I2S_HW_VERSION_1
#define SOC_I2S_HW_VERSION_1 1
#endif
#define ESP_CODEC_DEV_OK 0
#define ESP_CODEC_DEV_DRV_ERR -1
#define ESP_CODEC_DEV_INVALID_ARG -2
#define ESP_CODEC_DEV_WRONG_STATE -3
#define ESP_CODEC_DEV_NOT_SUPPORT -4
#define ESP_CODEC_DEV_TYPE_IN 1
#define ESP_CODEC_DEV_TYPE_OUT 2
#define ESP_CODEC_DEV_TYPE_IN_OUT 3
#define ESP_CODEC_DEV_MAKE_CHANNEL_MASK(x) (1U<<(x))
#define ESP_LOGI(...) ((void)0)
typedef int audio_codec_data_if_t, esp_codec_dev_type_t;
typedef struct { unsigned sample_rate, bits_per_sample, channel, channel_mask; } esp_codec_dev_sample_info_t;
typedef struct {
    audio_codec_data_if_t base;
    bool is_open; uint8_t port; void *out_handle,*in_handle;
    bool out_enable,in_enable,in_disable_pending,out_disable_pending,in_reconfig,out_reconfig,keep_slot_config;
    esp_codec_dev_sample_info_t in_fs,out_fs,fs;
} i2s_data_t;
static bool rx_running=true, fail_reset;
static unsigned stale=700, stops, starts;
static int _i2s_drv_enable(i2s_data_t *s,bool tx,bool enable) {
    (void)s;
    CHECK(!tx); /* Input privacy reset must never stop the active speaker. */
    if (fail_reset) return -1;
    if (enable) { CHECK(!rx_running); rx_running=true; stale=0; ++starts; }
    else { CHECK(rx_running); rx_running=false; ++stops; }
    return 0;
}
static bool _i2s_valid_fmt(esp_codec_dev_sample_info_t *f) { return f!=NULL; }
static int check_fs_compatible(i2s_data_t *s,bool tx,esp_codec_dev_sample_info_t *f) {
    (void)s; (void)f; CHECK(!tx && !rx_running); return 0;
}
static int set_fs(i2s_data_t *s,bool tx,bool skip) { (void)s;(void)tx;(void)skip; return 0; }
#include "i2s_privacy.inc"
int main(void) {
    i2s_data_t s={.is_open=true,.in_handle=(void*)1,.out_handle=(void*)2,.in_enable=true,.out_enable=true};
    esp_codec_dev_sample_info_t fs={16000,16,2,2};
    CHECK(_i2s_data_enable(&s.base,ESP_CODEC_DEV_TYPE_IN,false)==0);
#if SOC_I2S_HW_VERSION_1
    CHECK(s.in_disable_pending && !s.in_enable && rx_running && stale);
#else
    CHECK(!s.in_disable_pending && !s.in_enable && !rx_running && stale && s.out_enable);
#endif
    CHECK(_i2s_data_set_fmt(&s.base,ESP_CODEC_DEV_TYPE_IN,&fs)==0);
    CHECK(!rx_running && !s.in_disable_pending);
    CHECK(_i2s_data_enable(&s.base,ESP_CODEC_DEV_TYPE_IN,true)==0);
    CHECK(!stale && starts==1 && stops==1 && s.out_enable);
    CHECK(_i2s_data_enable(&s.base,ESP_CODEC_DEV_TYPE_IN,false)==0);
#if SOC_I2S_HW_VERSION_1
    fail_reset=true;
    CHECK(_i2s_data_set_fmt(&s.base,ESP_CODEC_DEV_TYPE_IN,&fs)!=0);
    CHECK(s.in_disable_pending && s.out_enable);
    fail_reset=false;
    CHECK(_i2s_data_set_fmt(&s.base,ESP_CODEC_DEV_TYPE_IN,&fs)==0);
    CHECK(_i2s_data_enable(&s.base,ESP_CODEC_DEV_TYPE_IN,true)==0);
#else
    CHECK(!s.in_disable_pending && !rx_running && s.out_enable);
    CHECK(_i2s_data_set_fmt(&s.base,ESP_CODEC_DEV_TYPE_IN,&fs)==0);
    CHECK(_i2s_data_enable(&s.base,ESP_CODEC_DEV_TYPE_IN,true)==0);
    CHECK(stale==0 && stops==2 && starts==2 && s.out_enable);
#endif
    /* Without TX deferral a failed physical RX stop must retain retry state. */
    s.out_enable=false;
    fail_reset=true;
    CHECK(_i2s_data_enable(&s.base,ESP_CODEC_DEV_TYPE_IN,false)!=0);
    CHECK(s.in_enable && rx_running);
    fail_reset=false;
    CHECK(_i2s_data_enable(&s.base,ESP_CODEC_DEV_TYPE_IN,false)==0);
    CHECK(!s.in_enable && !rx_running);
    puts("RX deferred reset and fail-closed passed");
}
