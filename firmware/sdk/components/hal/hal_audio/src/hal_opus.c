#include "hal_opus.h"

#include <stdbool.h>
#include <stddef.h>

#include "esp_audio_enc.h"
#include "esp_audio_types.h"
#include "esp_log.h"
#include "esp_opus_enc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#define TAG "HAL_OPUS"
#define HAL_OPUS_SAMPLE_RATE 16000
#define HAL_OPUS_FRAME_DURATION_MS 60
#define HAL_OPUS_STACK_WARN_BYTES (8U * 1024U)

static void *s_encoder = NULL;
static int s_encoder_frame_bytes = 0;
static int s_encoder_output_bytes = 0;
static SemaphoreHandle_t s_encoder_lock = NULL;
static uint32_t s_encoded_frames = 0U;
static size_t s_encode_stack_min_free = SIZE_MAX;

static void hal_opus_observe_encode_stack(size_t packet_bytes) {
    const size_t min_free = (size_t)uxTaskGetStackHighWaterMark(NULL);

    if (min_free >= s_encode_stack_min_free) {
        return;
    }

    s_encode_stack_min_free = min_free;
    if (min_free < HAL_OPUS_STACK_WARN_BYTES) {
        ESP_LOGW(TAG, "Opus encode stack watermark low: task=%s free=%u frame=%lu packet=%u", pcTaskGetName(NULL),
                 (unsigned)min_free, (unsigned long)s_encoded_frames, (unsigned)packet_bytes);
    } else {
        ESP_LOGI(TAG, "Opus encode stack watermark: task=%s free=%u frame=%lu packet=%u", pcTaskGetName(NULL),
                 (unsigned)min_free, (unsigned long)s_encoded_frames, (unsigned)packet_bytes);
    }
}

static esp_opus_enc_config_t hal_opus_encoder_config(void) {
    const esp_opus_enc_config_t config = {
        .sample_rate = ESP_AUDIO_SAMPLE_RATE_16K,
        .channel = ESP_AUDIO_MONO,
        .bits_per_sample = ESP_AUDIO_BIT16,
        .bitrate = ESP_OPUS_BITRATE_AUTO,
        .frame_duration = ESP_OPUS_ENC_FRAME_DURATION_60_MS,
        .application_mode = ESP_OPUS_ENC_APPLICATION_VOIP,
        .complexity = 0,
        .enable_fec = false,
        .enable_dtx = true,
        .enable_vbr = true,
    };
    return config;
}

static SemaphoreHandle_t hal_opus_get_lock(void) {
    SemaphoreHandle_t lock = __atomic_load_n(&s_encoder_lock, __ATOMIC_ACQUIRE);

    if (lock == NULL) {
        SemaphoreHandle_t candidate = xSemaphoreCreateMutex();
        SemaphoreHandle_t expected = NULL;

        if (candidate == NULL) {
            return NULL;
        }
        if (!__atomic_compare_exchange_n(&s_encoder_lock, &expected, candidate, false, __ATOMIC_RELEASE,
                                         __ATOMIC_ACQUIRE)) {
            vSemaphoreDelete(candidate);
            lock = expected;
        } else {
            lock = candidate;
        }
    }
    return lock;
}

static void hal_opus_deinit_locked(void) {
    if (s_encoder != NULL) {
        esp_opus_enc_close(s_encoder);
        s_encoder = NULL;
    }
    s_encoder_frame_bytes = 0;
    s_encoder_output_bytes = 0;
    s_encoded_frames = 0U;
    s_encode_stack_min_free = SIZE_MAX;
}

static int hal_opus_init_locked(void) {
    esp_opus_enc_config_t config;
    int frame_size = 0;
    int output_size = 0;
    int result;

    if (s_encoder != NULL) {
        return 0;
    }

    config = hal_opus_encoder_config();
    result = esp_opus_enc_open(&config, sizeof(config), &s_encoder);
    if (result != ESP_AUDIO_ERR_OK || s_encoder == NULL) {
        ESP_LOGE(TAG, "failed to initialize Opus encoder: result=%d", result);
        hal_opus_deinit_locked();
        return -1;
    }

    result = esp_opus_enc_get_frame_size(s_encoder, &frame_size, &output_size);
    if (result != ESP_AUDIO_ERR_OK || frame_size <= 0 || output_size <= 0) {
        ESP_LOGE(TAG, "failed to query Opus frame size: result=%d frame=%d output=%d", result, frame_size, output_size);
        hal_opus_deinit_locked();
        return -1;
    }

    s_encoder_frame_bytes = frame_size;
    s_encoder_output_bytes = output_size;
    ESP_LOGI(TAG, "Opus uplink ready: rate=%d frame_ms=%d pcm_bytes=%d max_packet=%d stack_hwm=%u",
             HAL_OPUS_SAMPLE_RATE, HAL_OPUS_FRAME_DURATION_MS, s_encoder_frame_bytes, s_encoder_output_bytes,
             (unsigned int)uxTaskGetStackHighWaterMark(NULL));
    return 0;
}

int hal_opus_init(void) {
    SemaphoreHandle_t lock = hal_opus_get_lock();
    int result;

    if (lock == NULL || xSemaphoreTake(lock, portMAX_DELAY) != pdTRUE) {
        return -1;
    }
    result = hal_opus_init_locked();
    xSemaphoreGive(lock);
    return result;
}

void hal_opus_deinit(void) {
    SemaphoreHandle_t lock = hal_opus_get_lock();

    if (lock == NULL || xSemaphoreTake(lock, portMAX_DELAY) != pdTRUE) {
        return;
    }
    hal_opus_deinit_locked();
    xSemaphoreGive(lock);
}

bool hal_opus_is_available(void) {
    return hal_opus_init() == 0;
}

int hal_opus_reset(void) {
    SemaphoreHandle_t lock = hal_opus_get_lock();
    int result;

    if (lock == NULL || xSemaphoreTake(lock, portMAX_DELAY) != pdTRUE) {
        return -1;
    }
    hal_opus_deinit_locked();
    result = hal_opus_init_locked();
    xSemaphoreGive(lock);
    return result;
}

int hal_opus_encode(const uint8_t *pcm_in, int pcm_len, uint8_t *out_buf, int out_max_len) {
    esp_audio_enc_in_frame_t input;
    esp_audio_enc_out_frame_t output;
    SemaphoreHandle_t lock = hal_opus_get_lock();
    int result;

    if (pcm_in == NULL || out_buf == NULL || pcm_len <= 0 || out_max_len <= 0 || lock == NULL ||
        xSemaphoreTake(lock, portMAX_DELAY) != pdTRUE) {
        return -1;
    }
    if (hal_opus_init_locked() != 0) {
        xSemaphoreGive(lock);
        return -1;
    }
    if (pcm_len != s_encoder_frame_bytes || out_max_len < s_encoder_output_bytes) {
        ESP_LOGE(TAG, "invalid Opus frame buffer: pcm=%d/%d out=%d/%d", pcm_len, s_encoder_frame_bytes, out_max_len,
                 s_encoder_output_bytes);
        xSemaphoreGive(lock);
        return -1;
    }

    input = (esp_audio_enc_in_frame_t){
        .buffer = (uint8_t *)pcm_in,
        .len = (uint32_t)pcm_len,
    };
    output = (esp_audio_enc_out_frame_t){
        .buffer = out_buf,
        .len = (uint32_t)out_max_len,
        .encoded_bytes = 0,
    };
    result = esp_opus_enc_process(s_encoder, &input, &output);
    if (result != ESP_AUDIO_ERR_OK || output.encoded_bytes == 0U || output.encoded_bytes > (uint32_t)out_max_len) {
        ESP_LOGE(TAG, "Opus encode failed: result=%d encoded=%lu", result, (unsigned long)output.encoded_bytes);
        xSemaphoreGive(lock);
        return -1;
    }
    s_encoded_frames++;
    hal_opus_observe_encode_stack((size_t)output.encoded_bytes);
    xSemaphoreGive(lock);
    return (int)output.encoded_bytes;
}

int hal_opus_decode(const uint8_t *in_data, int in_len, uint8_t *pcm_out, int pcm_max_len) {
    (void)in_data;
    (void)in_len;
    (void)pcm_out;
    (void)pcm_max_len;
    ESP_LOGE(TAG, "Opus downlink decoding is not enabled; TTS remains PCM");
    return -1;
}

