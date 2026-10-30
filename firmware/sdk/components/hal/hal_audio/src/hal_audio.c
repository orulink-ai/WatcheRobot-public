#include "hal_audio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#include "sensecap-watcher.h"
#include <math.h>

#define TAG "HAL_AUDIO"

/* Default sample rates */
#define SAMPLE_RATE_RECORD 16000 /* ASR expects 16kHz */
#define SAMPLE_RATE_PLAY 24000   /* 火山引擎 TTS uses 24kHz */

static bool codec_initialized = false; /* codec init is global, only once */
/* Destructive cleanup failed: old objects are gone, physical resource state
 * is not proven. Terminal until reboot; never reopen or publish capture-off. */
static bool codec_cleanup_fault = false;
static bool is_running = false;        /* current running state */
static bool is_playback_mode = false;  /* recording input or playback output */
#ifdef CONFIG_ENABLE_WAKE_WORD
static bool wake_word_stream_desired = false; /* live WakeNet runtime owns idle mic path */
#endif
static uint32_t current_sample_rate = SAMPLE_RATE_RECORD; /* current sample rate */
static uint8_t current_volume = CONFIG_WATCHER_AUDIO_VOLUME;
static esp_codec_dev_handle_t mic_handle = NULL;
static esp_codec_dev_handle_t speaker_handle = NULL;
static uint32_t consecutive_read_failures = 0;
static StaticSemaphore_t audio_lock_buffer;
static SemaphoreHandle_t audio_lock = NULL;
static hal_audio_capture_activity_cb_t capture_activity_cb = NULL;
static void *capture_activity_ctx = NULL;
static bool capture_activity_active = false;
static bool external_lease_active = false;
static bool external_lease_owns_codec_lifetime = false;
/* Retain a failed cold-gain initialization for a later lease retry. */
static bool external_microphone_default_gain_pending = false;

static bool hal_audio_lock(void) {
    if (audio_lock == NULL) {
        audio_lock = xSemaphoreCreateRecursiveMutexStatic(&audio_lock_buffer);
        if (audio_lock == NULL) {
            ESP_LOGE(TAG, "Failed to create audio lock");
            return false;
        }
    }

    if (xSemaphoreTakeRecursive(audio_lock, portMAX_DELAY) != pdTRUE)
        return false;
    if (__atomic_load_n(&codec_cleanup_fault, __ATOMIC_ACQUIRE)) {
        xSemaphoreGiveRecursive(audio_lock);
        return false;
    }
    return true;
}

static void hal_audio_unlock(void) {
    if (audio_lock != NULL) {
        xSemaphoreGiveRecursive(audio_lock);
    }
}

static void hal_audio_publish_capture_activity(bool active) {
    hal_audio_capture_activity_cb_t cb = NULL;
    void *ctx = NULL;

    if (!hal_audio_lock()) {
        return;
    }

    if (capture_activity_active != active) {
        capture_activity_active = active;
        cb = capture_activity_cb;
        ctx = capture_activity_ctx;
    }
    hal_audio_unlock();

    if (cb != NULL) {
        cb(active, ctx);
    }
}

int hal_audio_set_capture_activity_callback(hal_audio_capture_activity_cb_t cb, void *ctx) {
    bool active;

    if (!hal_audio_lock()) {
        return -1;
    }

    capture_activity_cb = cb;
    capture_activity_ctx = ctx;
    active = capture_activity_active;
    hal_audio_unlock();

    if (cb != NULL) {
        cb(active, ctx);
    }
    return 0;
}

static esp_err_t hal_audio_prepare_output(uint8_t volume) {
    esp_err_t ret;

    if (bsp_io_expander_init() == NULL) {
        return ESP_FAIL;
    }

    ret = bsp_exp_io_set_level(BSP_PWR_CODEC_PA, 1);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to enable codec PA power: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = bsp_codec_mute_set(volume == 0);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to set codec output mute: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = bsp_codec_volume_set(volume, NULL);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to set codec output volume: %s", esp_err_to_name(ret));
    }
    return ret;
}

static esp_err_t hal_audio_deinit_codec_locked(void);

/* Initialize codec once at system startup */
int hal_audio_init(void) {
    uint32_t initial_sample_rate = current_sample_rate;

    if (!hal_audio_lock()) {
        return -1;
    }

    if (codec_initialized && mic_handle != NULL) {
        /* An interrupted idle lease may leave a new mic awaiting its default.
         * Complete that initialization before a normal HAL owner can tune it. */
        if (external_microphone_default_gain_pending) {
            if (esp_codec_dev_set_in_gain(mic_handle, DRV_AUDIO_MIC_GAIN) != ESP_OK) {
                hal_audio_unlock();
                return -1;
            }
            external_microphone_default_gain_pending = false;
        }
        hal_audio_unlock();
        return 0;
    }

    ESP_LOGI(TAG, "Initializing audio codec via SDK...");

    esp_err_t ret = bsp_codec_init();
    /* BSP may retain partial construction on error. Publish ownership before
     * cleanup, including an input added to an output-only baseline. */
    mic_handle = bsp_codec_microphone_get();
    speaker_handle = bsp_codec_speaker_get();
    codec_initialized = mic_handle != NULL || speaker_handle != NULL;
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init codec: %s", esp_err_to_name(ret));
        /* Even with no codec handle, BSP init may own a mutex/data interface.
         * Force checked cleanup of that partial lifetime, under this lock. */
        codec_initialized = true;
        (void)hal_audio_deinit_codec_locked();
        hal_audio_unlock();
        return -1;
    }

    if (!mic_handle) {
        ESP_LOGE(TAG, "Failed to get microphone handle");
        hal_audio_unlock();
        return -1;
    }

    if (!speaker_handle) {
        ESP_LOGE(TAG, "Failed to get speaker handle");
        hal_audio_unlock();
        return -1;
    }

    codec_initialized = true;
    ret = bsp_codec_set_fs(initial_sample_rate, 16, 1);
    if (ret == ESP_OK)
        external_microphone_default_gain_pending = false;
    if (ret == ESP_OK) {
        ret = hal_audio_prepare_output(current_volume);
    }
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to prepare audio path: %s", esp_err_to_name(ret));
        /* Keep valid objects on close failure, or latch terminal destruction
         * failure. Neither case acknowledges physical capture-off. */
        (void)hal_audio_deinit_codec_locked();
        is_running = false;
        hal_audio_unlock();
        return -1;
    }
    is_running = true;
    current_sample_rate = initial_sample_rate;

    ESP_LOGI(TAG, "Audio codec initialized (%luHz, volume=%u)", current_sample_rate, (unsigned)current_volume);

    hal_audio_unlock();
    return 0;
}

static esp_err_t hal_audio_deinit_codec_locked(void) {
    if (__atomic_load_n(&codec_cleanup_fault, __ATOMIC_ACQUIRE))
        return ESP_ERR_INVALID_STATE;
    if (!codec_initialized) {
        mic_handle = NULL;
        speaker_handle = NULL;
        return ESP_OK;
    }

    const esp_err_t ret = bsp_codec_deinit();
    /* A close failure retains BSP objects for retry. After destructive cleanup
     * an error instead means unknown hardware state, not reusable handles. */
    mic_handle = bsp_codec_microphone_get();
    speaker_handle = bsp_codec_speaker_get();
    codec_initialized = mic_handle != NULL || speaker_handle != NULL;
    if (mic_handle == NULL)
        external_microphone_default_gain_pending = false;
    if (ret != ESP_OK && !codec_initialized) {
        __atomic_store_n(&codec_cleanup_fault, true, __ATOMIC_RELEASE);
    }
    if (ret != ESP_OK) {
        return ret;
    }

    mic_handle = NULL;
    speaker_handle = NULL;
    codec_initialized = false;
    return ESP_OK;
}

bool hal_audio_has_terminal_fault(void) {
    /* Must remain observable after the normal audio lock rejects operations. */
    return __atomic_load_n(&codec_cleanup_fault, __ATOMIC_ACQUIRE);
}

bool hal_audio_is_initialized(void) {
    if (!hal_audio_lock()) {
        return false;
    }
    const bool initialized = codec_initialized;
    hal_audio_unlock();
    return initialized;
}

int hal_audio_deinit(void) {
    esp_err_t ret;

    if (!hal_audio_lock()) {
        return -1;
    }
    if (external_lease_active) {
        ESP_LOGW(TAG, "Audio deinit rejected while external lease is active");
        hal_audio_unlock();
        return -1;
    }

#ifdef CONFIG_ENABLE_WAKE_WORD
    wake_word_stream_desired = false;
#endif
    is_playback_mode = false;
    is_running = false;
    current_sample_rate = SAMPLE_RATE_RECORD;
    consecutive_read_failures = 0;

    ret = hal_audio_deinit_codec_locked();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Audio codec deinit returned warning: %s", esp_err_to_name(ret));
        hal_audio_unlock();
        return -1;
    }

    ESP_LOGI(TAG, "Audio codec deinitialized");
    hal_audio_unlock();
    hal_audio_publish_capture_activity(false);
    return 0;
}

/* Set sample rate for playback (call before TTS playback) */
void hal_audio_set_sample_rate(uint32_t sample_rate) {
    if (!hal_audio_lock()) {
        return;
    }
    if (external_lease_active) {
        ESP_LOGW(TAG, "Sample-rate change ignored while external lease is active");
        hal_audio_unlock();
        return;
    }

    if (!codec_initialized) {
        current_sample_rate = sample_rate;
        hal_audio_unlock();
        return;
    }

    if (current_sample_rate == sample_rate) {
        hal_audio_unlock();
        return; /* No change needed */
    }

    ESP_LOGI(TAG, "Switching sample rate: %lu -> %lu (playback_mode=%d)", current_sample_rate, sample_rate,
             is_playback_mode);

    /* For playback mode, we don't need to stop input channel (which may not be enabled).
     * Just set the new sample rate directly. */
    if (is_playback_mode) {
        /* Set new sample rate (16-bit, mono channel) */
        bsp_codec_set_fs(sample_rate, 16, 1);
        current_sample_rate = sample_rate;
        ESP_LOGI(TAG, "Sample rate switch complete (playback mode, no stop needed)");
        hal_audio_unlock();
        return;
    }

    /* For recording mode (input only), we need to reconfigure I2S.
     * Use bsp_codec_set_fs which handles I2S reconfiguration properly. */
    bsp_codec_set_fs(sample_rate, 16, 1);
    current_sample_rate = sample_rate;

    ESP_LOGI(TAG, "Sample rate switch complete");
    hal_audio_unlock();
}

void hal_audio_set_volume(uint8_t volume_percent) {
    esp_err_t ret;

    if (volume_percent > 100) {
        volume_percent = 100;
    }

    if (!hal_audio_lock()) {
        return;
    }

    current_volume = volume_percent;
    if (codec_initialized) {
        ret = bsp_codec_volume_set(current_volume, NULL);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Failed to apply codec volume %u: %s", (unsigned)current_volume, esp_err_to_name(ret));
        }
    }
    ESP_LOGI(TAG, "Audio volume set to %u", (unsigned)current_volume);
    hal_audio_unlock();
}

uint8_t hal_audio_get_volume(void) {
    return current_volume;
}

/* Mark audio as being used for playback (not just recording) */
void hal_audio_set_playback_mode(bool enable) {
    bool changed;

    if (!hal_audio_lock()) {
        return;
    }
    if (external_lease_active) {
        hal_audio_unlock();
        return;
    }

    changed = is_playback_mode != enable;
    is_playback_mode = enable;
    if (changed) {
        ESP_LOGI(TAG, "Audio mode: %s", enable ? "playback" : "recording");
    }
    hal_audio_unlock();

    if (enable && changed) {
        hal_audio_publish_capture_activity(false);
    }
}

bool hal_audio_is_running(void) {
    return is_running;
}

bool hal_audio_is_playback_mode(void) {
    return is_playback_mode;
}

int hal_audio_start(void) {
    bool capture_active;

    if (!hal_audio_lock()) {
        return -1;
    }
    if (external_lease_active) {
        ESP_LOGW(TAG, "Audio start rejected while external lease is active");
        hal_audio_unlock();
        return -1;
    }

    /* Output-only preparation intentionally has no microphone handle. A
     * permitted capture start must complete the legacy capture initialization. */
    if (!codec_initialized || (!is_playback_mode && mic_handle == NULL)) {
        if (hal_audio_init() != 0) {
            hal_audio_unlock();
            return -1;
        }
    }

    if (is_running) {
        consecutive_read_failures = 0;
        if (is_playback_mode) {
            if (hal_audio_prepare_output(current_volume) != ESP_OK) {
                hal_audio_unlock();
                return -1;
            }
        }
        ESP_LOGD(TAG, "Audio already running (sample rate: %lu Hz, playback=%d)", current_sample_rate,
                 is_playback_mode);
        capture_active = !is_playback_mode;
        hal_audio_unlock();
        hal_audio_publish_capture_activity(capture_active);
        return 0;
    }

    esp_err_t ret = bsp_codec_set_fs(current_sample_rate, 16, 1);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open audio path at %lu Hz: %s", current_sample_rate, esp_err_to_name(ret));
        hal_audio_unlock();
        return -1;
    }

    consecutive_read_failures = 0;
    if (is_playback_mode) {
        if (hal_audio_prepare_output(current_volume) != ESP_OK) {
            (void)bsp_codec_dev_stop();
            hal_audio_unlock();
            return -1;
        }
    }
    is_running = true;
    ESP_LOGI(TAG, "Audio started (sample rate: %lu Hz)", current_sample_rate);
    capture_active = !is_playback_mode;
    hal_audio_unlock();
    hal_audio_publish_capture_activity(capture_active);
    return 0;
}

int hal_audio_read(uint8_t *out_buf, int max_len) {
    int result = -1;

    if (!hal_audio_lock()) {
        return -1;
    }
    if (external_lease_active) {
        goto cleanup;
    }

    if (!mic_handle) {
        goto cleanup;
    }

    /* In wake word mode, audio should always be available */
    /* If is_running is false, return 0 (no data) instead of -1 (error) to avoid error spam */
    if (!is_running) {
#ifdef CONFIG_ENABLE_WAKE_WORD
        result = 0; /* In wake word mode, gracefully handle temporary unavailability */
#else
        result = -1;
#endif
        goto cleanup;
    }

    size_t bytes_read = 0;
    esp_err_t ret = bsp_i2s_read(out_buf, max_len, &bytes_read, 100);

    if (ret != ESP_OK) {
#ifdef CONFIG_ENABLE_WAKE_WORD
        consecutive_read_failures++;
        if (consecutive_read_failures == 1U || (consecutive_read_failures % 50U) == 0U) {
            ESP_LOGW(TAG, "Read temporarily unavailable: %s (count=%lu running=%d playback=%d rate=%lu)",
                     esp_err_to_name(ret), (unsigned long)consecutive_read_failures, is_running, is_playback_mode,
                     current_sample_rate);
        }
        result = 0;
        goto cleanup;
#else
        ESP_LOGE(TAG, "Read error: %s", esp_err_to_name(ret));
        result = -1;
        goto cleanup;
#endif
    }

    consecutive_read_failures = 0;
    result = (int)bytes_read;

cleanup:
    hal_audio_unlock();
    return result;
}

int hal_audio_write(const uint8_t *data, int len) {
    int result = -1;

    if (!hal_audio_lock()) {
        return -1;
    }
    if (external_lease_active) {
        goto cleanup;
    }

    if (!is_running || !speaker_handle) {
        ESP_LOGW(TAG, "Write blocked: is_running=%d, speaker_handle=%p", is_running, speaker_handle);
        goto cleanup;
    }

    size_t bytes_written = 0;
    /* Use ESP_LOGD for high-frequency audio writes to avoid UART bottleneck */
    ESP_LOGD(TAG, "Writing %d bytes to speaker...", len);
    esp_err_t ret = bsp_i2s_write((void *)data, len, &bytes_written, 100);
    ESP_LOGD(TAG, "Write result: ret=%d, written=%d", ret, (int)bytes_written);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Write error: %s", esp_err_to_name(ret));
        goto cleanup;
    }

    result = (int)bytes_written;

cleanup:
    hal_audio_unlock();
    return result;
}

int hal_audio_drain_playback(uint32_t timeout_ms) {
    int result = -1;

    if (!hal_audio_lock()) {
        return -1;
    }
    if (external_lease_active) {
        goto cleanup;
    }
    if (!is_running || !is_playback_mode || speaker_handle == NULL) {
        ESP_LOGW(TAG, "Playback drain rejected: running=%d playback=%d speaker=%p", is_running, is_playback_mode,
                 speaker_handle);
        goto cleanup;
    }

    esp_err_t ret = bsp_i2s_wait_tx_drain(timeout_ms);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Playback DMA drain failed: %s timeout_ms=%lu", esp_err_to_name(ret), (unsigned long)timeout_ms);
        goto cleanup;
    }
    result = 0;

cleanup:
    hal_audio_unlock();
    return result;
}

int hal_audio_stop(void) {
    if (!hal_audio_lock()) {
        return -1;
    }
    if (external_lease_active) {
        hal_audio_unlock();
        return -1;
    }

    if (!is_running) {
        hal_audio_unlock();
        hal_audio_publish_capture_activity(false);
        return 0;
    }

#ifdef CONFIG_ENABLE_WAKE_WORD
    if (!wake_word_stream_desired) {
        is_playback_mode = false;
        is_running = false;
        current_sample_rate = SAMPLE_RATE_RECORD;
        consecutive_read_failures = 0;
        ESP_LOGI(TAG, "Audio stopped (wake stream not requested, mode=idle)");
        hal_audio_unlock();
        hal_audio_publish_capture_activity(false);
        return 0;
    }

    /* Wake word detection needs a continuous 16 kHz microphone stream.
     * Playback users call stop after local SFX/TTS; in wake mode, restore
     * the shared codec path to recording instead of disabling it. */
    if (is_playback_mode) {
        is_playback_mode = false;
    }
    current_sample_rate = SAMPLE_RATE_RECORD;
    esp_err_t ret = bsp_codec_set_fs(current_sample_rate, 16, 1);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to restore wake audio path: %s", esp_err_to_name(ret));
        is_running = false;
        hal_audio_unlock();
        hal_audio_publish_capture_activity(false);
        return -1;
    }
    consecutive_read_failures = 0;
    ESP_LOGI(TAG, "Audio stop requested; wake word path restored at %lu Hz", current_sample_rate);
    hal_audio_unlock();
    hal_audio_publish_capture_activity(true);
    return 0;
#else
    /* No wake-word detector needs a continuous microphone stream. Treat stop
     * after SFX/TTS as an idle audio path, not as a recording restore. */
    is_playback_mode = false;
    is_running = false;
    current_sample_rate = SAMPLE_RATE_RECORD;
    consecutive_read_failures = 0;
    ESP_LOGI(TAG, "Audio stopped (codec stays initialized, mode=idle)");
    hal_audio_unlock();
    hal_audio_publish_capture_activity(false);
    return 0;
#endif
}

void hal_audio_set_wake_word_stream_desired(bool enable) {
#ifdef CONFIG_ENABLE_WAKE_WORD
    if (!hal_audio_lock()) {
        return;
    }

    if (external_lease_active) {
        hal_audio_unlock();
        return;
    }

    wake_word_stream_desired = enable;
    ESP_LOGD(TAG, "Wake word stream desired=%d", enable ? 1 : 0);
    hal_audio_unlock();
#else
    (void)enable;
#endif
}

int hal_audio_prepare_playback(uint8_t volume) {
    if (volume > 100 || !hal_audio_lock())
        return -1;
    if (external_lease_active) {
        hal_audio_unlock();
        return -1;
    }
    esp_err_t ret = bsp_codec_init_output();
    /* Retain even partial output construction for checked cleanup/retry. */
    speaker_handle = bsp_codec_speaker_get();
    mic_handle = bsp_codec_microphone_get();
    if (speaker_handle != NULL)
        codec_initialized = true;
    if (ret == ESP_OK)
        ret = bsp_codec_set_output_fs(SAMPLE_RATE_RECORD, 16, 1);
    if (ret == ESP_OK)
        ret = hal_audio_prepare_output(volume);
    if (ret == ESP_OK) {
#ifdef CONFIG_ENABLE_WAKE_WORD
        wake_word_stream_desired = false;
#endif
        current_volume = volume;
        current_sample_rate = SAMPLE_RATE_RECORD;
        is_playback_mode = is_running = true;
        consecutive_read_failures = 0;
    }
    hal_audio_unlock();
    if (ret == ESP_OK)
        hal_audio_publish_capture_activity(false);
    return ret == ESP_OK ? 0 : -1;
}

int hal_audio_capture_pause(void) {
    if (!hal_audio_lock())
        return -1;
    if (external_lease_active) {
        hal_audio_unlock();
        return -1;
    }
    /* Do not close the speaker or restore the wake stream. The ADC close is
     * checked; a failed close must retain the activity/owner state for retry. */
    if (codec_initialized && mic_handle != NULL && esp_codec_dev_close(mic_handle) != ESP_OK) {
        hal_audio_unlock();
        return -1;
    }
#ifdef CONFIG_ENABLE_WAKE_WORD
    /* Explicit app capture resume, not a later playback stop, owns reopening. */
    wake_word_stream_desired = false;
#endif
    if (!is_playback_mode)
        is_running = false;
    hal_audio_unlock();
    hal_audio_publish_capture_activity(false);
    return 0;
}

int hal_audio_enter_app_idle(void) {
    esp_err_t ret;

    if (!hal_audio_lock()) {
        return -1;
    }
    if (external_lease_active) {
        hal_audio_unlock();
        return -1;
    }

    ret = codec_initialized ? bsp_codec_dev_stop() : ESP_OK;
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to close audio path for app idle: %s", esp_err_to_name(ret));
        hal_audio_unlock();
        return -1;
    }

#ifdef CONFIG_ENABLE_WAKE_WORD
    wake_word_stream_desired = false;
#endif
    is_playback_mode = false;
    is_running = false;
    current_sample_rate = SAMPLE_RATE_RECORD;
    consecutive_read_failures = 0;
    ESP_LOGI(TAG, "Audio entered app-idle state");
    hal_audio_unlock();
    hal_audio_publish_capture_activity(false);
    return 0;
}

int hal_audio_release_idle(void) {
#ifdef CONFIG_ENABLE_WAKE_WORD
    bool wake_stream_requested = false;

    if (!hal_audio_lock()) {
        return -1;
    }
    wake_stream_requested = wake_word_stream_desired;
    hal_audio_unlock();

    if (wake_stream_requested) {
        return hal_audio_stop();
    }
#endif

    return hal_audio_deinit();
}

int hal_audio_external_lease_acquire(void) {
    esp_err_t ret;

    if (!hal_audio_lock()) {
        return -1;
    }
    if (external_lease_active) {
        hal_audio_unlock();
        return 0;
    }

    const bool codec_initialized_for_lease = !codec_initialized;
    const bool microphone_created_for_lease = bsp_codec_microphone_get() == NULL;
    ret = bsp_codec_init_idle();
    mic_handle = bsp_codec_microphone_get();
    speaker_handle = bsp_codec_speaker_get();
    if (mic_handle != NULL || speaker_handle != NULL)
        codec_initialized = true; /* Partial construction remains owned for cleanup. */
    if (microphone_created_for_lease && mic_handle != NULL)
        external_microphone_default_gain_pending = true;
    if (ret != ESP_OK || mic_handle == NULL || speaker_handle == NULL) {
        hal_audio_unlock();
        return -1;
    }
    ret = bsp_codec_dev_stop();
    if (ret == ESP_OK && external_microphone_default_gain_pending) {
        /* Codec settings can be applied while ADC/RX remains closed. Do not
         * overwrite a shared microphone's previously selected (possibly low) gain. */
        ret = esp_codec_dev_set_in_gain(mic_handle, DRV_AUDIO_MIC_GAIN);
        if (ret == ESP_OK)
            external_microphone_default_gain_pending = false;
    }
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to idle codec before external lease: %s", esp_err_to_name(ret));
        if (codec_initialized_for_lease) {
            (void)hal_audio_deinit_codec_locked();
        }
        hal_audio_unlock();
        return -1;
    }
#ifdef CONFIG_ENABLE_WAKE_WORD
    wake_word_stream_desired = false;
#endif
    is_running = false;
    is_playback_mode = false;
    current_sample_rate = SAMPLE_RATE_RECORD;
    consecutive_read_failures = 0;
    external_lease_owns_codec_lifetime = codec_initialized_for_lease;
    __atomic_store_n(&external_lease_active, true, __ATOMIC_RELEASE);
    ESP_LOGI(TAG, "External full-duplex audio lease acquired (codec=%s)",
             external_lease_owns_codec_lifetime ? "transient" : "shared");
    hal_audio_unlock();
    hal_audio_publish_capture_activity(false);
    return 0;
}

int hal_audio_external_lease_get_handles(esp_codec_dev_handle_t *microphone, esp_codec_dev_handle_t *speaker) {
    int ret = -1;

    if (microphone == NULL || speaker == NULL || !hal_audio_lock()) {
        return -1;
    }
    if (external_lease_active && codec_initialized && mic_handle != NULL && speaker_handle != NULL) {
        *microphone = mic_handle;
        *speaker = speaker_handle;
        ret = 0;
    }
    hal_audio_unlock();
    return ret;
}

int hal_audio_external_lease_configure(uint32_t sample_rate) {
    if ((sample_rate != 16000 && sample_rate != 24000) || !hal_audio_lock())
        return -1;
    if (!external_lease_active) {
        hal_audio_unlock();
        return -1;
    }
    /* Explicit duplex opt-in for SDK capture. JoyInside prepare only acquires
     * idle handles; its capture source opens RX after authorization. */
    esp_err_t ret = bsp_codec_set_fs(sample_rate, 16, 1);
    if (ret == ESP_OK)
        current_sample_rate = sample_rate;
    hal_audio_unlock();
    return ret == ESP_OK ? 0 : -1;
}

int hal_audio_external_lease_prepare_output(uint8_t volume) {
    if (volume > 100 || !hal_audio_lock()) {
        return -1;
    }
    esp_err_t ret = ESP_ERR_INVALID_STATE;
    if (external_lease_active && codec_initialized && speaker_handle != NULL) {
        ret = hal_audio_prepare_output(volume);
    }
    hal_audio_unlock();
    return ret == ESP_OK ? 0 : -1;
}

int hal_audio_external_lease_drain(uint32_t timeout_ms) {
    if (!timeout_ms || !hal_audio_lock())
        return -1;
    esp_err_t ret = ESP_ERR_INVALID_STATE;
    if (external_lease_active && codec_initialized && speaker_handle != NULL)
        ret = bsp_i2s_wait_tx_drain(timeout_ms);
    hal_audio_unlock();
    return ret == ESP_OK ? 0 : -1;
}

int hal_audio_external_lease_release(void) {
    esp_err_t ret = ESP_OK;
    bool released_transient_codec = false;

    if (!hal_audio_lock()) {
        return -1;
    }
    if (!external_lease_active) {
        hal_audio_unlock();
        return 0;
    }
    if (codec_initialized) {
        ret = bsp_codec_dev_stop();
    }
    if (ret != ESP_OK) {
        hal_audio_unlock();
        return -1;
    }
    is_running = false;
    is_playback_mode = false;
    current_sample_rate = SAMPLE_RATE_RECORD;
    consecutive_read_failures = 0;
    if (external_lease_owns_codec_lifetime) {
        released_transient_codec = true;
        const esp_err_t deinit_ret = hal_audio_deinit_codec_locked();
        if (deinit_ret != ESP_OK) {
            hal_audio_unlock();
            return -1;
        }
        if (ret == ESP_OK) {
            ret = deinit_ret;
        }
    }
    external_lease_owns_codec_lifetime = false;
    __atomic_store_n(&external_lease_active, false, __ATOMIC_RELEASE);
    ESP_LOGI(TAG, "External full-duplex audio lease released (codec=%s): %s",
             released_transient_codec ? "transient-freed" : "shared-retained", esp_err_to_name(ret));
    hal_audio_unlock();
    hal_audio_publish_capture_activity(false);
    return ret == ESP_OK ? 0 : -1;
}

bool hal_audio_external_lease_is_active(void) {
    /* Lock refusal, including terminal cleanup, never proves owner release.
     * Only successful checked release publishes false. This is metadata,
     * not permission to use codec handles or reopen an unproven device. */
    return __atomic_load_n(&external_lease_active, __ATOMIC_ACQUIRE);
}
