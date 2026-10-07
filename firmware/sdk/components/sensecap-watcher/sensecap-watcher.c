
#include <inttypes.h>
#include <string.h>

#include "esp_check.h"
#include "esp_err.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"

#include "driver/i2c_master.h"
#include "driver/sdspi_host.h"
#include "esp_lvgl_port.h"
#include "esp_private/sdmmc_common.h"
#include "esp_vfs_fat.h"
#include "io_expander_read_guard.h"
#include "iot_button.h"
#include "sd_protocol_defs.h"
#include "sdmmc_cmd.h"
#include "sensecap-watcher.h"
#include "soc/gpio_reg.h"
#include "soc/soc.h"
#include "vfs_fat_internal.h"
#include "watcher_display_budget.h"
#ifdef WATCHER_CLAW_ENABLE
#define WATCHER_CLAW_DISPLAY_ENABLED true
#else
#define WATCHER_CLAW_DISPLAY_ENABLED false
#endif

static const char *TAG = "BSP";

/* IDF6 架构对称化：I2C 总线所有权封装进 context，统一生命周期管理。
   保留一个文件级默认实例作现有调用方的兼容层（签名不变），
   内部所有引用点从直接读单例改为读 s_bus_ctx 字段。 */
static bsp_i2c_bus_ctx_t s_bus_ctx = {
    .bus_handle = NULL,
    .rtc_dev_handle = NULL,
    .initialized = false,
};

#define WATCHER_SD_SPI_MAX_FREQ_KHZ 20000
#define WATCHER_SD_SPI_FALLBACK_FREQ_KHZ 10000
#define WATCHER_SD_SPI_SAFE_FREQ_KHZ 5000
#define WATCHER_SD_POWER_OFF_DELAY_MS 20
#define WATCHER_SD_POWER_ON_DELAY_MS 250
#define WATCHER_SD_OCR_PROBE_MAX_ATTEMPTS 300
#define WATCHER_SD_OCR_PROBE_RETRY_DELAY_MS 10
#define WATCHER_SD_SPI_MAX_TRANSFER_BYTES (128 * 1024)

static led_strip_handle_t rgb_led_handle = NULL;
static esp_io_expander_handle_t io_exp_handle = NULL;
static SemaphoreHandle_t io_expander_read_mutex = NULL;
static io_expander_read_guard_t io_expander_read_guard;

static sscma_client_io_handle_t sscma_client_io_handle = NULL;
static sscma_client_io_handle_t sscma_flasher_io_handle = NULL;
static sscma_client_handle_t sscma_client_handle = NULL;
static sscma_client_flasher_handle_t sscma_flasher_handle = NULL;
static bool sscma_client_initialized = false;

static lv_disp_t *lvgl_disp = NULL;
static esp_lcd_panel_io_handle_t panel_io_handle = NULL;
static esp_lcd_panel_handle_t panel_handle = NULL;
static esp_lcd_panel_io_handle_t tp_io_handle = NULL;
static esp_lcd_touch_handle_t tp_handle = NULL;
static i2c_master_bus_handle_t touch_bus_handle;
static lv_indev_t *touch_indev;
static lv_indev_t *knob_indev;

static sdmmc_card_t *card;
/* BSP-lifetime reserve, shared by IDF sector reads/writes under FatFs' volume
 * mutex. Raw FAT classification runs only before the volume is published.
 * Reserve before media starts: PSRAM reads must not allocate DMA per sector
 * while microphone, LCD and camera have fragmented the internal heap. */
static void *s_sd_dma_buffer;
static bsp_sdcard_status_t s_sdcard_status = {
    .state = BSP_SD_STATE_NOT_DETECTED,
    .card_type = BSP_SD_CARD_TYPE_UNKNOWN,
    .filesystem = BSP_SD_FILESYSTEM_UNKNOWN,
    .last_error = ESP_ERR_NOT_FOUND,
};
static portMUX_TYPE s_sdcard_status_mux = portMUX_INITIALIZER_UNLOCKED;
static esp_codec_dev_handle_t play_dev_handle;
static esp_codec_dev_handle_t record_dev_handle;
static bool play_dev_open = false;
static bool record_dev_open = false;
static SemaphoreHandle_t codec_mutex = NULL;
static const audio_codec_if_t *play_codec_if = NULL;
static const audio_codec_ctrl_if_t *play_ctrl_if = NULL;
static const audio_codec_gpio_if_t *play_gpio_if = NULL;
static const audio_codec_if_t *record_codec_if = NULL;
static const audio_codec_ctrl_if_t *record_ctrl_if = NULL;

static i2s_chan_handle_t i2s_tx_chan = NULL;
static i2s_chan_handle_t i2s_rx_chan = NULL;
static const audio_codec_data_if_t *i2s_data_if = NULL;
static volatile uint32_t i2s_tx_done_count = 0U;
static portMUX_TYPE i2s_tx_reference_lock = portMUX_INITIALIZER_UNLOCKED;
static bsp_i2s_tx_reference_cb_t i2s_tx_reference_cb = NULL;
static void *i2s_tx_reference_ctx = NULL;

static bool IRAM_ATTR bsp_i2s_tx_sent_callback(i2s_chan_handle_t handle, i2s_event_data_t *event, void *user_ctx) {
    (void)handle;
    (void)user_ctx;
    __atomic_fetch_add(&i2s_tx_done_count, 1U, __ATOMIC_RELAXED);
    bool should_yield = false;
    portENTER_CRITICAL_ISR(&i2s_tx_reference_lock);
    if (i2s_tx_reference_cb != NULL && event != NULL && event->dma_buf != NULL && event->size > 0U) {
        should_yield = i2s_tx_reference_cb(event->dma_buf, event->size, i2s_tx_reference_ctx);
    }
    portEXIT_CRITICAL_ISR(&i2s_tx_reference_lock);
    return should_yield;
}

static esp_err_t bsp_codec_close_play_locked(void) {
    if (!play_dev_open || play_dev_handle == NULL) {
        play_dev_open = false;
        return ESP_OK;
    }

    esp_err_t ret = esp_codec_dev_close(play_dev_handle);
    if (ret == ESP_OK) {
        play_dev_open = false;
    }
    return ret;
}

static esp_err_t bsp_codec_close_record_locked(void) {
    if (!record_dev_open || record_dev_handle == NULL) {
        record_dev_open = false;
        return ESP_OK;
    }

    esp_err_t ret = esp_codec_dev_close(record_dev_handle);
    if (ret == ESP_OK) {
        record_dev_open = false;
    }
    return ret;
}

static size_t bsp_lcd_max_transfer_bytes(void) {
    size_t max_transfer = DRV_LCD_H_RES * DRV_LCD_V_RES * DRV_LCD_BITS_PER_PIXEL / 8 / CONFIG_BSP_LCD_SPI_DMA_SIZE_DIV;
    max_transfer = max_transfer > 0 ? max_transfer : (DRV_LCD_H_RES * DRV_LCD_BITS_PER_PIXEL / 8);
    return watcher_lcd_transfer_budget(max_transfer, WATCHER_CLAW_DISPLAY_ENABLED);
}

void bsp_lvgl_rounder_cb(struct _lv_disp_drv_t *disp_drv, lv_area_t *area) {
    uint16_t x1 = area->x1;
    uint16_t x2 = area->x2;

    // round the start of coordinate down to the nearest 4M number
    area->x1 = (x1 >> 2) << 2;
    // round the end of coordinate up to the nearest 4N+3 number
    area->x2 = ((x2 >> 2) << 2) + 3;
}

static void bsp_btn_cb(void *arg, void *arg2) {
    void (*cb)(void) = arg2;
    button_handle_t btn = (button_handle_t)arg;
    if (cb && btn) {
        cb();
    }
}

void bsp_set_btn_long_press_cb(void (*cb)(void)) {
    (void)bsp_set_btn_long_press_ms_cb(0, cb);
}

esp_err_t bsp_set_btn_long_press_ms_cb(uint16_t press_time_ms, void (*cb)(void)) {
    lv_indev_t *tp = NULL;
    while (1) {
        tp = lv_indev_get_next(tp);
        if (tp == NULL || tp->driver->type == LV_INDEV_TYPE_ENCODER) {
            break;
        }
    }

    if (tp == NULL) {
        ESP_LOGE(TAG, "No encoder found");
        return ESP_ERR_NOT_FOUND;
    }

    if (press_time_ms == 0) {
        return lvgl_port_encoder_btn_register_event_cb(tp, BUTTON_LONG_PRESS_START, bsp_btn_cb, cb);
    }

    button_event_config_t event_cfg = {
        .event = BUTTON_LONG_PRESS_START,
        .event_data.long_press.press_time = press_time_ms,
    };
    esp_err_t ret = lvgl_port_encoder_btn_register_event_data_cb(tp, event_cfg, bsp_btn_cb, cb);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register %u ms long-press callback: %s", (unsigned)press_time_ms,
                 esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "Registered %u ms long-press callback", (unsigned)press_time_ms);
    }
    return ret;
}

void bsp_set_btn_long_release_cb(void (*cb)(void)) {
    lv_indev_t *tp = NULL;
    while (1) {
        tp = lv_indev_get_next(tp);
        if (tp == NULL || tp->driver->type == LV_INDEV_TYPE_ENCODER) {
            break;
        }
    }

    if (tp == NULL) {
        ESP_LOGE(TAG, "No encoder found");
        return;
    }

    lvgl_port_encoder_btn_register_event_cb(tp, BUTTON_LONG_PRESS_UP, bsp_btn_cb, cb);
}

esp_err_t bsp_set_btn_single_click_cb(void (*cb)(void)) {
    lv_indev_t *tp = NULL;
    while (1) {
        tp = lv_indev_get_next(tp);
        if (tp == NULL || tp->driver->type == LV_INDEV_TYPE_ENCODER) {
            break;
        }
    }

    if (tp == NULL) {
        ESP_LOGE(TAG, "No encoder found");
        return ESP_ERR_NOT_FOUND;
    }

    esp_err_t ret = lvgl_port_encoder_btn_register_event_cb(tp, BUTTON_SINGLE_CLICK, bsp_btn_cb, cb);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register single-click callback: %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "Registered single-click callback");
    }
    return ret;
}

void bsp_set_btn_multi_click_cb(int click_count, void (*cb)(void)) {
    lv_indev_t *tp = NULL;
    while (1) {
        tp = lv_indev_get_next(tp);
        if (tp == NULL || tp->driver->type == LV_INDEV_TYPE_ENCODER) {
            break;
        }
    }

    if (tp == NULL) {
        ESP_LOGE(TAG, "No encoder found");
        return;
    }

    /* Configure multiple click event with specified click count */
    button_event_config_t event_cfg = {
        .event = BUTTON_MULTIPLE_CLICK,
        .event_data.multiple_clicks.clicks = click_count,
    };

    esp_err_t ret = lvgl_port_encoder_btn_register_event_data_cb(tp, event_cfg, bsp_btn_cb, cb);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register multi-click callback: %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "Registered %d-click callback", click_count);
    }
}

esp_err_t bsp_i2c_detect(void) {
    return bsp_i2c_detect_ctx(&s_bus_ctx);
}

esp_err_t bsp_i2c_detect_ctx(bsp_i2c_bus_ctx_t *ctx) {
    /* 可注入 ctx 版本：单测/多实例场景自建 bsp_i2c_bus_ctx_t 实例并注入。 */
    ESP_RETURN_ON_FALSE(ctx != NULL, ESP_ERR_INVALID_ARG, TAG, "ctx is NULL");
    BSP_ERROR_CHECK_RETURN_ERR(bsp_i2c_bus_init_ctx(ctx));
    uint8_t address;
    printf("     0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f\r\n");
    for (int i = 0; i < 128; i += 16) {
        printf("%02x: ", i);
        for (int j = 0; j < 16; j++) {
            fflush(stdout);
            address = i + j;
            esp_err_t ret = i2c_master_probe(ctx->bus_handle, address, 50);
            if (ret == ESP_OK) {
                printf("%02x ", address);
            } else if (ret == ESP_ERR_TIMEOUT) {
                printf("UU ");
            } else {
                printf("-- ");
            }
        }
        printf("\r\n");
    }

    return ESP_OK;
}

esp_err_t bsp_i2c_check(uint8_t address) {
    return bsp_i2c_check_ctx(&s_bus_ctx, address);
}

esp_err_t bsp_i2c_check_ctx(bsp_i2c_bus_ctx_t *ctx, uint8_t address) {
    // IDF6: 与 bsp_i2c_detect 语义对齐——先确保总线已初始化再探测，
    // 否则 i2c_master_probe 收到 NULL 句柄会触发 NULL deref。
    // 统一为懒初始化语义：调用方无需关心总线是否已 init，
    // detect 与 check 行为一致，消除"前者初始化后者只断言"的不对称陷阱。
    ESP_RETURN_ON_FALSE(ctx != NULL, ESP_ERR_INVALID_ARG, TAG, "ctx is NULL");
    BSP_ERROR_CHECK_RETURN_ERR(bsp_i2c_bus_init_ctx(ctx));
    return i2c_master_probe(ctx->bus_handle, address, 50);
}

static esp_io_expander_handle_t bsp_io_expander_init_with_initial_inputs(uint32_t *out_initial_inputs,
                                                                         bool *out_initial_inputs_valid) {
    uint32_t initial_pin_val = DRV_IO_EXP_INPUT_MASK;
    bool initial_inputs_valid = false;

    if (out_initial_inputs != NULL) {
        *out_initial_inputs = initial_pin_val;
    }
    if (out_initial_inputs_valid != NULL) {
        *out_initial_inputs_valid = false;
    }
    if (io_exp_handle != NULL) {
        if (out_initial_inputs != NULL &&
            esp_io_expander_get_level(io_exp_handle, DRV_IO_EXP_INPUT_MASK, &initial_pin_val) == ESP_OK) {
            *out_initial_inputs = initial_pin_val;
            initial_inputs_valid = true;
            if (out_initial_inputs_valid != NULL) {
                *out_initial_inputs_valid = initial_inputs_valid;
            }
        }
        return io_exp_handle;
    }
    esp_err_t ret = ESP_OK;

    ESP_LOGI(TAG, "Initialize IO I2C bus");
    BSP_ERROR_CHECK_RETURN_NULL(bsp_i2c_bus_init());

    if (io_expander_read_mutex == NULL) {
        io_expander_read_mutex = xSemaphoreCreateMutex();
        if (io_expander_read_mutex == NULL) {
            ESP_LOGE(TAG, "Failed to create IO expander read mutex");
            return NULL;
        }
        io_expander_read_guard_init(&io_expander_read_guard);
    }

    const pca95xx_16bit_ex_config_t io_exp_config = {

        .int_gpio = BSP_IO_EXPANDER_INT,
        .update_interval_us = 1000000, // 1s
        .isr_cb = NULL,
        .user_ctx = NULL,
    };

    ret = esp_io_expander_new_i2c_pca95xx_16bit_ex(s_bus_ctx.bus_handle, ESP_IO_EXPANDER_I2C_PCA9535_ADDRESS_001,
                                                   &io_exp_config, &io_exp_handle);
    if (ret != ESP_OK) {
        io_exp_handle = NULL;
        ESP_LOGE(TAG, "IO expander creation failed: %s", esp_err_to_name(ret));
        return NULL;
    }

    ret |= esp_io_expander_set_dir(io_exp_handle, DRV_IO_EXP_INPUT_MASK, IO_EXPANDER_INPUT);
    ret |= esp_io_expander_set_dir(io_exp_handle, DRV_IO_EXP_OUTPUT_MASK, IO_EXPANDER_OUTPUT);
    if (esp_io_expander_get_level(io_exp_handle, DRV_IO_EXP_INPUT_MASK, &initial_pin_val) == ESP_OK) {
        if (out_initial_inputs != NULL) {
            *out_initial_inputs = initial_pin_val;
        }
        initial_inputs_valid = true;
        if (out_initial_inputs_valid != NULL) {
            *out_initial_inputs_valid = initial_inputs_valid;
        }
    }
    ret |= esp_io_expander_set_level(io_exp_handle, DRV_IO_EXP_OUTPUT_MASK, 0);
    ret |= esp_io_expander_set_level(io_exp_handle, BSP_PWR_SYSTEM, 1);
    vTaskDelay(100 / portTICK_PERIOD_MS);
    ret |= esp_io_expander_set_level(io_exp_handle, BSP_PWR_START_UP, 1);
    vTaskDelay(50 / portTICK_PERIOD_MS);

    uint32_t pin_val = 0;
    const esp_err_t final_read_ret = esp_io_expander_get_level(io_exp_handle, DRV_IO_EXP_INPUT_MASK, &pin_val);
    ret |= final_read_ret;
    if (!initial_inputs_valid && final_read_ret == ESP_OK) {
        if (out_initial_inputs != NULL) {
            *out_initial_inputs = pin_val;
        }
        if (out_initial_inputs_valid != NULL) {
            *out_initial_inputs_valid = true;
        }
    }
    ESP_LOGI(TAG, "IO expander initialized: %x", DRV_IO_EXP_OUTPUT_MASK | (uint16_t)pin_val);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "IO expander setup failed: %s", esp_err_to_name(ret));
        (void)esp_io_expander_del(io_exp_handle);
        io_exp_handle = NULL;
        return NULL;
    }

    return io_exp_handle;
}

esp_io_expander_handle_t bsp_io_expander_init() {
    return bsp_io_expander_init_with_initial_inputs(NULL, NULL);
}

uint8_t bsp_exp_io_get_level(uint16_t pin_mask) {
    uint8_t level = 0;
    (void)bsp_exp_io_get_level_checked(pin_mask, &level);
    return level;
}

esp_err_t bsp_exp_io_get_level_checked(uint16_t pin_mask, uint8_t *out_level) {
    uint32_t pin_val = 0;
    esp_err_t ret;
    int64_t now_us;
    uint32_t previous_failures;

    if (out_level == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (io_exp_handle == NULL || io_expander_read_mutex == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    if (xSemaphoreTake(io_expander_read_mutex, pdMS_TO_TICKS(5)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    now_us = esp_timer_get_time();
    if (!io_expander_read_guard_can_attempt(&io_expander_read_guard, now_us)) {
        ret = (esp_err_t)io_expander_read_guard_last_error(&io_expander_read_guard);
        xSemaphoreGive(io_expander_read_mutex);
        return ret != ESP_OK ? ret : ESP_ERR_TIMEOUT;
    }

    ret = esp_io_expander_get_level(io_exp_handle, DRV_IO_EXP_INPUT_MASK, &pin_val);
    if (ret != ESP_OK) {
        const uint32_t retry_delay_us =
            io_expander_read_guard_record_failure(&io_expander_read_guard, now_us, (int)ret);
        if (io_expander_read_guard_should_report(&io_expander_read_guard)) {
            ESP_LOGW(TAG, "IO expander input read degraded: err=%s consecutive=%lu retry_in_ms=%lu",
                     esp_err_to_name(ret),
                     (unsigned long)io_expander_read_guard_consecutive_failures(&io_expander_read_guard),
                     (unsigned long)(retry_delay_us / 1000U));
        }
        xSemaphoreGive(io_expander_read_mutex);
        return ret;
    }

    previous_failures = io_expander_read_guard_consecutive_failures(&io_expander_read_guard);
    io_expander_read_guard_record_success(&io_expander_read_guard);
    if (previous_failures > 0U) {
        ESP_LOGI(TAG, "IO expander input read recovered after %lu failed attempts", (unsigned long)previous_failures);
    }
    pin_mask &= DRV_IO_EXP_INPUT_MASK;
    *out_level = (uint8_t)((pin_val & pin_mask) ? 1 : 0);
    xSemaphoreGive(io_expander_read_mutex);
    return ESP_OK;
}

esp_err_t bsp_exp_io_set_level(uint16_t pin_mask, uint8_t level) {
    return esp_io_expander_set_level(io_exp_handle, pin_mask, level);
}

esp_err_t bsp_spi_bus_init(void) {
    static bool initialized = false;
    if (initialized) {
        return ESP_OK;
    }
    const spi_bus_config_t spi_cfg = {
        .mosi_io_num = BSP_SPI2_HOST_MOSI,
        .miso_io_num = BSP_SPI2_HOST_MISO,
        .sclk_io_num = BSP_SPI2_HOST_SCLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .isr_cpu_id = CONFIG_SSCMA_PROCESS_TASK_AFFINITY + 1,
        .max_transfer_sz = WATCHER_SD_SPI_MAX_TRANSFER_BYTES,
    };
    BSP_ERROR_CHECK_RETURN_ERR(spi_bus_initialize(SPI2_HOST, &spi_cfg, SPI_DMA_CH_AUTO));

    const spi_bus_config_t qspi_cfg = {
        .sclk_io_num = BSP_SPI3_HOST_PCLK,
        .data0_io_num = BSP_SPI3_HOST_DATA0,
        .data1_io_num = BSP_SPI3_HOST_DATA1,
        .data2_io_num = BSP_SPI3_HOST_DATA2,
        .data3_io_num = BSP_SPI3_HOST_DATA3,
        .isr_cpu_id = CONFIG_LVGL_PORT_TASK_AFFINITY + 1,
        .max_transfer_sz = bsp_lcd_max_transfer_bytes(),
    };

    BSP_ERROR_CHECK_RETURN_ERR(spi_bus_initialize(SPI3_HOST, &qspi_cfg, SPI_DMA_CH_AUTO));

    initialized = true;
    return ESP_OK;
}

esp_err_t bsp_i2c_bus_init_ctx(bsp_i2c_bus_ctx_t *ctx) {
    /* 可注入 ctx 版本：单测/多实例场景自建 bsp_i2c_bus_ctx_t 实例并注入，
       不绑定文件级单例。公共无参 bsp_i2c_bus_init(void) 委托给文件级 s_bus_ctx。 */
    ESP_RETURN_ON_FALSE(ctx != NULL, ESP_ERR_INVALID_ARG, TAG, "ctx is NULL");
    if (ctx->initialized) {
        return ESP_OK;
    }
    i2c_master_bus_config_t i2c_bus_conf = {
        .i2c_port = BSP_GENERAL_I2C_NUM,
        .sda_io_num = BSP_GENERAL_I2C_SDA,
        .scl_io_num = BSP_GENERAL_I2C_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = false,
    };
    BSP_ERROR_CHECK_RETURN_ERR(i2c_new_master_bus(&i2c_bus_conf, &ctx->bus_handle));

    // pulldown for lcd i2c
    const gpio_config_t io_config = {
        .pin_bit_mask = (1ULL << BSP_TOUCH_I2C_SDA) | (1ULL << BSP_TOUCH_I2C_SCL) | (1ULL << BSP_SPI3_HOST_PCLK) |
                        (1ULL << BSP_SPI3_HOST_DATA0) | (1ULL << BSP_SPI3_HOST_DATA1) | (1ULL << BSP_SPI3_HOST_DATA2) |
                        (1ULL << BSP_SPI3_HOST_DATA3) | (1ULL << BSP_LCD_SPI_CS) | (1UL << BSP_LCD_GPIO_BL),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLUP_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_config);

    gpio_set_level(BSP_TOUCH_I2C_SDA, 0);
    gpio_set_level(BSP_TOUCH_I2C_SCL, 0);

    gpio_set_level(BSP_LCD_SPI_CS, 0);
    gpio_set_level(BSP_LCD_GPIO_BL, 0);
    gpio_set_level(BSP_SPI3_HOST_PCLK, 0);
    gpio_set_level(BSP_SPI3_HOST_DATA0, 0);
    gpio_set_level(BSP_SPI3_HOST_DATA1, 0);
    gpio_set_level(BSP_SPI3_HOST_DATA2, 0);
    gpio_set_level(BSP_SPI3_HOST_DATA3, 0);

    ctx->initialized = true;
    return ESP_OK;
}

esp_err_t bsp_i2c_bus_init(void) {
    /* 公共无参兼容层：委托给文件级默认实例 s_bus_ctx，签名不变。 */
    return bsp_i2c_bus_init_ctx(&s_bus_ctx);
}

bool bsp_i2c_bus_is_initialized(void) {
    return s_bus_ctx.initialized && s_bus_ctx.bus_handle != NULL;
}

esp_err_t bsp_i2c_bus_deinit_ctx(bsp_i2c_bus_ctx_t *ctx) {
    /* 可注入 ctx 版本：与 bsp_i2c_bus_init_ctx 对称的资源回收路径。
       所有权边界：只释放 master bus，不越界 rm 掉 ctx->rtc_dev_handle；
       若 ctx->rtc_dev_handle 仍非空，返回 ESP_ERR_INVALID_STATE 强制
       "先回收 RTC device 再释放 bus"的调用顺序契约。 */
    ESP_RETURN_ON_FALSE(ctx != NULL, ESP_ERR_INVALID_ARG, TAG, "ctx is NULL");
    if (!ctx->initialized) {
        return ESP_OK;
    }
    if (ctx->rtc_dev_handle != NULL) {
        ESP_LOGE(TAG, "bsp_i2c_bus_deinit: rtc_dev_handle still alive, call bsp_rtc_deinit first");
        return ESP_ERR_INVALID_STATE;
    }
    if (ctx->bus_handle != NULL) {
        i2c_del_master_bus(ctx->bus_handle);
        ctx->bus_handle = NULL;
    }
    ctx->initialized = false;
    return ESP_OK;
}

esp_err_t bsp_i2c_bus_deinit(void) {
    /* 公共无参兼容层：委托给文件级默认实例 s_bus_ctx，签名不变。 */
    return bsp_i2c_bus_deinit_ctx(&s_bus_ctx);
}

esp_err_t bsp_uart_bus_init(void) {
    static bool initialized = false;
    if (initialized) {
        return ESP_OK;
    }
    uart_config_t uart_config = {
        .baud_rate = 921600,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    BSP_ERROR_CHECK_RETURN_ERR(uart_param_config(BSP_SSCMA_FLASHER_UART_NUM, &uart_config));
    BSP_ERROR_CHECK_RETURN_ERR(
        uart_set_pin(BSP_SSCMA_FLASHER_UART_NUM, BSP_SSCMA_FLASHER_UART_TX, BSP_SSCMA_FLASHER_UART_RX, -1, -1));
    BSP_ERROR_CHECK_RETURN_ERR(
        uart_driver_install(BSP_SSCMA_FLASHER_UART_NUM, 64 * 1024, 0, 0, NULL, ESP_INTR_FLAG_SHARED));
    initialized = true;
    return ESP_OK;
}

esp_err_t bsp_rgb_init() {
    led_strip_config_t bsp_strip_config = {
        .strip_gpio_num = BSP_RGB_CTRL,
        .max_leds = 1,
        .led_pixel_format = LED_PIXEL_FORMAT_GRB,
        .led_model = LED_MODEL_WS2812,
        .flags.invert_out = false,
    };

    led_strip_rmt_config_t bsp_rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10 * 1000 * 1000,
        .flags.with_dma = false,
    };

    BSP_ERROR_CHECK_RETURN_ERR(led_strip_new_rmt_device(&bsp_strip_config, &bsp_rmt_config, &rgb_led_handle));
    vTaskDelay(pdMS_TO_TICKS(10));
    led_strip_set_pixel(rgb_led_handle, 0, 0x00, 0x00, 0x00);
    led_strip_refresh(rgb_led_handle);

    return ESP_OK;
}

esp_err_t bsp_rgb_set(uint8_t r, uint8_t g, uint8_t b) {
    esp_err_t ret = ESP_OK;

    ret |= led_strip_set_pixel(rgb_led_handle, 0, r, g, b);
    ret |= led_strip_refresh(rgb_led_handle);
    return ret;
}

void bsp_system_deep_sleep(uint32_t time_in_sec) {
    if (time_in_sec > 0)
        esp_sleep_enable_timer_wakeup(time_in_sec * 1000000);

    uint32_t pin_mask_sleep =
        BSP_PWR_SDCARD | BSP_PWR_CODEC_PA | BSP_PWR_GROVE | BSP_PWR_BAT_ADC | BSP_PWR_LCD | BSP_PWR_AI_CHIP;
    if (io_exp_handle != NULL)
        esp_io_expander_set_level(io_exp_handle, pin_mask_sleep, 0);

    esp_sleep_enable_ext0_wakeup(BSP_IO_EXPANDER_INT, 0);
    rtc_gpio_pullup_en(BSP_IO_EXPANDER_INT);
    rtc_gpio_pulldown_dis(BSP_IO_EXPANDER_INT);

    esp_deep_sleep_start();
}

void bsp_system_reboot(void) {
    esp_restart();
}

esp_err_t bsp_system_shutdown(void) {
    esp_err_t ret = bsp_exp_io_set_level(BSP_PWR_SYSTEM, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "System shutdown request failed: %s", esp_err_to_name(ret));
    } else {
        ESP_LOGW(TAG, "System shutdown requested: BSP_PWR_SYSTEM=0");
    }
    return ret;
}

bool bsp_system_is_charging(void) {
    return !(bsp_exp_io_get_level(BSP_PWR_CHRG_DET) == 0);
}

bool bsp_system_is_standby(void) {
    return bsp_exp_io_get_level(BSP_PWR_STDBY_DET) == 0;
}

bool bsp_battery_is_present(void) {
    return bsp_exp_io_get_level(BSP_PWR_BAT_DET) == 0;
}

#ifdef CONFIG_HEAP_ABORT_WHEN_ALLOCATION_FAILS
void heap_caps_alloc_failed_hook(size_t requested_size, uint32_t caps, const char *function_name) {
    printf("%s failed to allocate %d bytes with 0x%X capabilities. \n", function_name, requested_size, caps);
}
#endif

uint16_t bsp_battery_get_voltage(void) {
    static bool initialized = false;
    static adc_oneshot_unit_handle_t adc_handle;
    static adc_cali_handle_t cali_handle = NULL;
    if (!initialized) {
        adc_oneshot_unit_init_cfg_t init_config = {
            .unit_id = ADC_UNIT_1,
        };
        adc_oneshot_new_unit(&init_config, &adc_handle);

        adc_oneshot_chan_cfg_t ch_config = {
            .bitwidth = ADC_BITWIDTH_DEFAULT,
            .atten = BSP_BAT_ADC_ATTEN,
        };
        adc_oneshot_config_channel(adc_handle, BSP_BAT_ADC_CHAN, &ch_config);

        adc_cali_curve_fitting_config_t cali_config = {
            .unit_id = ADC_UNIT_1,
            .chan = BSP_BAT_ADC_CHAN,
            .atten = BSP_BAT_ADC_ATTEN,
            .bitwidth = ADC_BITWIDTH_DEFAULT,
        };
        if (adc_cali_create_scheme_curve_fitting(&cali_config, &cali_handle) == ESP_OK) {
            initialized = true;
        }
    }
    if (initialized) {
        int raw_value = 0;
        int voltage = 0; // mV
        adc_oneshot_read(adc_handle, BSP_BAT_ADC_CHAN, &raw_value);
        adc_cali_raw_to_voltage(cali_handle, raw_value, &voltage);
        voltage = voltage * 82 / 20;
        ESP_LOGD(TAG, "voltage: %dmV", voltage);
        return (uint16_t)voltage;
    }
    return 0;
}

uint8_t bsp_battery_get_percent(void) {
    int32_t voltage = 0;
    for (uint8_t i = 0; i < 10; i++) {
        voltage += bsp_battery_get_voltage();
    }
    voltage /= 10;
    int percent = (-1 * voltage * voltage + 9016 * voltage - 19189000) / 10000;
    percent = (percent > 100) ? 100 : (percent < 0) ? 0 : percent;
    ESP_LOGD(TAG, "percentage: %d%%", percent);
    return (uint8_t)percent;
}

inline static esp_err_t bsp_rtc_reg_write_ctx(bsp_i2c_bus_ctx_t *ctx, uint8_t reg, uint8_t *val, size_t len) {
    // IDF6: 前置状态断言。rtc_dev_handle 由 bsp_rtc_init() 创建，
    // 若未初始化即调用本函数会触发 NULL deref，必须在此显式拦截。
    ESP_RETURN_ON_FALSE(ctx != NULL, ESP_ERR_INVALID_ARG, TAG, "ctx is NULL");
    ESP_RETURN_ON_FALSE(ctx->rtc_dev_handle, ESP_ERR_INVALID_STATE, TAG, "rtc device not initialized");
    uint8_t data[8] = {0};
    data[0] = reg;
    memcpy(data + 1, val, len);
    return i2c_master_transmit(ctx->rtc_dev_handle, data, len + 1, DRV_PCF8563_TIMEOUT_MS / portTICK_PERIOD_MS);
}

inline static esp_err_t bsp_rtc_reg_read_ctx(bsp_i2c_bus_ctx_t *ctx, uint8_t reg, uint8_t *val, size_t len) {
    ESP_RETURN_ON_FALSE(ctx != NULL, ESP_ERR_INVALID_ARG, TAG, "ctx is NULL");
    ESP_RETURN_ON_FALSE(ctx->rtc_dev_handle, ESP_ERR_INVALID_STATE, TAG, "rtc device not initialized");
    return i2c_master_transmit_receive(ctx->rtc_dev_handle, &reg, 1, val, len,
                                       DRV_PCF8563_TIMEOUT_MS / portTICK_PERIOD_MS);
}

/* 公共无参兼容层：委托给文件级默认实例 s_bus_ctx，签名不变。 */
inline static esp_err_t bsp_rtc_reg_write(uint8_t reg, uint8_t *val, size_t len) {
    return bsp_rtc_reg_write_ctx(&s_bus_ctx, reg, val, len);
}

inline static esp_err_t bsp_rtc_reg_read(uint8_t reg, uint8_t *val, size_t len) {
    return bsp_rtc_reg_read_ctx(&s_bus_ctx, reg, val, len);
}

esp_err_t bsp_rtc_init(void) {
    esp_err_t ret = ESP_OK;
    ret = bsp_i2c_bus_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C bus initialization failed");
        return ret;
    }

    // Create RTC device handle
    i2c_device_config_t rtc_dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = DRV_PCF8563_I2C_ADDR,
        // IDF6: 逐设备时钟须与上游 I2C 总线频率（BSP_GENERAL_I2C_CLK=400k）保持一致，
        // 否则会出现总线/设备频率不匹配导致通信不稳定。
        .scl_speed_hz = BSP_GENERAL_I2C_CLK,
    };
    ret = i2c_master_bus_add_device(s_bus_ctx.bus_handle, &rtc_dev_cfg, &s_bus_ctx.rtc_dev_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "RTC device creation failed");
        return ret;
    }

    uint8_t data = 0x00;
    BSP_ERROR_CHECK_RETURN_ERR(bsp_rtc_reg_write(DRV_RTC_REG_STATUS1, &data, 1));
    BSP_ERROR_CHECK_RETURN_ERR(bsp_rtc_reg_write(DRV_RTC_REG_STATUS2, &data, 1));

    // TODO: create feed dog timer ?

    return ret;
}

esp_err_t bsp_rtc_deinit(void) {
    /* IDF6 架构对称化：与 bsp_rtc_init 对称，回收 RTC device 句柄。
       bus 本身由 bsp_i2c_bus_deinit 管理，这里只 rm device。 */
    if (s_bus_ctx.rtc_dev_handle == NULL) {
        return ESP_OK;
    }
    i2c_master_bus_rm_device(s_bus_ctx.rtc_dev_handle);
    s_bus_ctx.rtc_dev_handle = NULL;
    return ESP_OK;
}

esp_err_t bsp_rtc_get_time(struct tm *timeinfo) {
    esp_err_t ret = ESP_OK;
    ret = bsp_i2c_bus_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C bus initialization failed");
        return ret;
    }

    uint8_t data[7] = {0};
    ret = bsp_rtc_reg_read(DRV_RTC_REG_TIME, data, sizeof(data));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read time from RTC");
        return ret;
    }

    struct tm tm_data = {
        .tm_sec = BCD2DEC(data[0] & 0x7F),
        .tm_min = BCD2DEC(data[1] & 0x7F),
        .tm_hour = BCD2DEC(data[2] & 0x3F),
        .tm_mday = BCD2DEC(data[3] & 0x3F),
        .tm_wday = BCD2DEC(data[4] & 0x07),
        .tm_mon = BCD2DEC(data[5] & 0x1F) - 1,
        .tm_year = BCD2DEC(data[6]) + 2000 - 1900,
    };
    *timeinfo = tm_data;

    ESP_LOGI(TAG, "Current time: %d-%d-%d %d:%d:%d", timeinfo->tm_year + 1900, timeinfo->tm_mon + 1, timeinfo->tm_mday,
             timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);

    return ESP_OK;
}

esp_err_t bsp_rtc_set_time(const struct tm *timeinfo) {
    esp_err_t ret = ESP_OK;

    // IDF6: 与 bsp_rtc_get_time 对称，确保 I2C 总线已初始化后再写 RTC，
    // 否则 bsp_rtc_reg_write 的前置断言会返回 ESP_ERR_INVALID_STATE。
    ret = bsp_i2c_bus_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C bus initialization failed");
        return ret;
    }

    uint8_t data[7] = {0};
    data[0] = DEC2BCD(timeinfo->tm_sec);
    data[1] = DEC2BCD(timeinfo->tm_min);
    data[2] = DEC2BCD(timeinfo->tm_hour);
    data[3] = DEC2BCD(timeinfo->tm_mday);
    data[4] = DEC2BCD(timeinfo->tm_wday);    // 0 - 6
    data[5] = DEC2BCD(timeinfo->tm_mon + 1); // 0 - 11
    data[6] = DEC2BCD(timeinfo->tm_year - 100);

    ret = bsp_rtc_reg_write(DRV_RTC_REG_TIME, data, sizeof(data));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set time to RTC");
        return ret;
    }

    return ESP_OK;
}

esp_err_t bsp_rtc_set_timer(uint32_t time_in_sec) {
    esp_err_t ret = ESP_OK;

    if ((time_in_sec > 255 * 60) || (time_in_sec < 15)) {
        ESP_LOGE(TAG, "RTC set timer - out of range");
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t data = 0x00;
    uint8_t freq = (time_in_sec > 255) ? 0b11 : 0b10; // 1/60 Hz or 1 Hz
    uint8_t cnt = (time_in_sec > 255) ? time_in_sec / 60 : time_in_sec;

    data = 0x80 | (freq & 0x03);
    BSP_ERROR_CHECK_RETURN_ERR(bsp_rtc_reg_write(DRV_RTC_REG_TIMER_CTL, &data, 1));
    BSP_ERROR_CHECK_RETURN_ERR(bsp_rtc_reg_write(DRV_RTC_REG_TIMER, &cnt, 1));

    data = 0x11;
    BSP_ERROR_CHECK_RETURN_ERR(bsp_rtc_reg_write(DRV_RTC_REG_STATUS2, &data, 1));

    return ret;
}

esp_err_t bsp_knob_btn_init(void *param) {
    esp_io_expander_handle_t io_exp = bsp_io_expander_init();
    if (io_exp == NULL) {
        ESP_LOGE(TAG, "IO expander initialization failed");
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t bsp_knob_btn_init_with_initial_sample(bool *out_pressed) {
    uint32_t initial_inputs = DRV_IO_EXP_INPUT_MASK;
    bool initial_inputs_valid = false;

    if (out_pressed == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    *out_pressed = false;

    if (bsp_io_expander_init_with_initial_inputs(&initial_inputs, &initial_inputs_valid) == NULL) {
        ESP_LOGE(TAG, "IO expander initialization failed during boot button sample");
        return ESP_FAIL;
    }
    if (!initial_inputs_valid) {
        ESP_LOGW(TAG, "Initial boot button sample unavailable");
        return ESP_FAIL;
    }

    *out_pressed = (initial_inputs & BSP_KNOB_BTN) == 0U;
    return ESP_OK;
}

uint8_t bsp_knob_btn_get_key_value(void *param) {
    uint8_t value = 1;
    if (bsp_knob_btn_get_key_value_checked(param, &value) != ESP_OK) {
        /* The knob is active-low. A failed I2C sample must be treated as
         * released, otherwise a bus fault becomes a synthetic long press. */
        return 1;
    }
    return value;
}

esp_err_t bsp_knob_btn_get_key_value_checked(void *param, uint8_t *out_value) {
    (void)param;
    return bsp_exp_io_get_level_checked(BSP_KNOB_BTN, out_value);
}

esp_err_t bsp_knob_btn_deinit(void *param) {
    (void)param;
    /* Board-owned expander is shared with camera/audio. */
    return ESP_OK;
}

static esp_err_t bsp_lcd_backlight_init() {
    const ledc_channel_config_t backlight_channel = {.gpio_num = BSP_LCD_GPIO_BL,
                                                     .speed_mode = LEDC_LOW_SPEED_MODE,
                                                     .channel = DRV_LCD_LEDC_CH,
                                                     .intr_type = LEDC_INTR_DISABLE,
                                                     .timer_sel = LEDC_TIMER_1,
                                                     .duty = BIT(DRV_LCD_LEDC_DUTY_RES),
                                                     .hpoint = 0};
    const ledc_timer_config_t backlight_timer = {.speed_mode = LEDC_LOW_SPEED_MODE,
                                                 .duty_resolution = DRV_LCD_LEDC_DUTY_RES,
                                                 .timer_num = LEDC_TIMER_1,
                                                 .freq_hz = 5000,
                                                 .clk_cfg = LEDC_AUTO_CLK};
    BSP_ERROR_CHECK_RETURN_ERR(ledc_timer_config(&backlight_timer));
    BSP_ERROR_CHECK_RETURN_ERR(ledc_channel_config(&backlight_channel));

    BSP_ERROR_CHECK_RETURN_ERR(bsp_lcd_brightness_set(0));

    return ESP_OK;
}

esp_err_t bsp_lcd_brightness_set(int brightness_percent) {
    if (brightness_percent > 100) {
        brightness_percent = 100;
    }
    if (brightness_percent < 0) {
        brightness_percent = 0;
    }

    ESP_LOGD(TAG, "Setting LCD backlight: %d%%", brightness_percent);
    uint32_t duty_cycle = (BIT(DRV_LCD_LEDC_DUTY_RES) * (brightness_percent)) / 100;
    BSP_ERROR_CHECK_RETURN_ERR(ledc_set_duty(LEDC_LOW_SPEED_MODE, DRV_LCD_LEDC_CH, duty_cycle));
    BSP_ERROR_CHECK_RETURN_ERR(ledc_update_duty(LEDC_LOW_SPEED_MODE, DRV_LCD_LEDC_CH));

    return ESP_OK;
}

static esp_err_t bsp_lcd_pannel_init(esp_lcd_panel_handle_t *ret_panel, esp_lcd_panel_io_handle_t *ret_io) {
    esp_err_t ret = ESP_OK;

    ESP_RETURN_ON_ERROR(bsp_lcd_backlight_init(), TAG, "Brightness init failed");

    ESP_LOGD(TAG, "Install panel IO");
    const esp_lcd_panel_io_spi_config_t io_config = {
        .cs_gpio_num = BSP_LCD_SPI_CS,
        .dc_gpio_num = -1,
        .spi_mode = 3,
        .pclk_hz = DRV_LCD_PIXEL_CLK_HZ,
        .trans_queue_depth =
            watcher_lcd_queue_depth(CONFIG_BSP_LCD_PANEL_SPI_TRANS_Q_DEPTH, WATCHER_CLAW_DISPLAY_ENABLED),
        .lcd_cmd_bits = DRV_LCD_CMD_BITS,
        .lcd_param_bits = DRV_LCD_PARAM_BITS,
        .flags =
            {
                .quad_mode = true,
            },
    };
    spd2010_vendor_config_t vendor_config = {
        .flags =
            {
                .use_qspi_interface = 1,
            },
    };
    ESP_GOTO_ON_ERROR(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)BSP_LCD_SPI_NUM, &io_config, ret_io), err, TAG,
                      "New panel IO failed");

    ESP_LOGD(TAG, "Install LCD driver");
    const esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = BSP_LCD_GPIO_RST, // Shared with Touch reset
        .rgb_ele_order = DRV_LCD_RGB_ELEMENT_ORDER,
        .bits_per_pixel = DRV_LCD_BITS_PER_PIXEL,
        .vendor_config = &vendor_config,
    };
    ESP_GOTO_ON_ERROR(esp_lcd_new_panel_spd2010(*ret_io, &panel_config, ret_panel), err, TAG, "New panel failed");

    BSP_ERROR_CHECK_RETURN_ERR(esp_lcd_panel_reset(*ret_panel));
    BSP_ERROR_CHECK_RETURN_ERR(esp_lcd_panel_init(*ret_panel));
    BSP_ERROR_CHECK_RETURN_ERR(esp_lcd_panel_mirror(*ret_panel, DRV_LCD_MIRROR_X, DRV_LCD_MIRROR_Y));
    BSP_ERROR_CHECK_RETURN_ERR(esp_lcd_panel_disp_on_off(*ret_panel, true));

    bsp_lcd_brightness_set(CONFIG_BSP_LCD_DEFAULT_BRIGHTNESS);

    return ret;
err:
    if (*ret_panel)
        esp_lcd_panel_del(*ret_panel);
    if (*ret_io)
        esp_lcd_panel_io_del(*ret_io);
    spi_bus_free(BSP_LCD_SPI_NUM);
    return ret;
}

static lv_disp_t *bsp_display_lcd_init(const bsp_display_cfg_t *cfg) {
    assert(cfg != NULL);

    ESP_LOGD(TAG, "Initialize SPI bus");
    if (bsp_spi_bus_init() != ESP_OK)
        return NULL;

    ESP_LOGD(TAG, "Initialize LCD panel");

    if (bsp_lcd_pannel_init(&panel_handle, &panel_io_handle) != ESP_OK)
        return NULL;

    /* Add LCD screen */
    ESP_LOGD(TAG, "Add LCD screen");
    const lvgl_port_display_cfg_t disp_cfg = {.io_handle = panel_io_handle,
                                              .panel_handle = panel_handle,
                                              .buffer_size = cfg->buffer_size,
                                              .double_buffer = cfg->double_buffer,
                                              .hres = DRV_LCD_H_RES,
                                              .vres = DRV_LCD_V_RES,
                                              .monochrome = false,
                                              .rotation =
                                                  {
                                                      .swap_xy = DRV_LCD_SWAP_XY,
                                                      .mirror_x = DRV_LCD_MIRROR_X,
                                                      .mirror_y = DRV_LCD_MIRROR_Y,
                                                  },
                                              .flags = {
                                                  .buff_dma = cfg->flags.buff_dma,
                                                  .buff_spiram = cfg->flags.buff_spiram,
#if LVGL_VERSION_MAJOR == 9 && defined(CONFIG_LV_COLOR_16_SWAP)
                                                  .swap_bytes = true,
#endif
                                              }};

    return lvgl_port_add_disp(&disp_cfg);
}

static lv_indev_t *bsp_knob_indev_init(lv_disp_t *disp) {
    ESP_LOGI(TAG, "Initialize knob input device");
    const static knob_config_t knob_cfg = {
        .default_direction = 0,
        .gpio_encoder_a = BSP_KNOB_A,
        .gpio_encoder_b = BSP_KNOB_B,
    };
    const static button_config_t btn_config = {
        .type = BUTTON_TYPE_CUSTOM,
        .long_press_time = 2000,
        .short_press_time = 200,
        .custom_button_config =
            {
                .active_level = 0,
                .button_custom_init = bsp_knob_btn_init,
                .button_custom_deinit = bsp_knob_btn_deinit,
                .button_custom_get_key_value = bsp_knob_btn_get_key_value,
            },
    };
    const lvgl_port_encoder_cfg_t encoder = {.disp = disp, .encoder_a_b = &knob_cfg, .encoder_enter = &btn_config};
    return lvgl_port_add_encoder(&encoder);
}

static lv_indev_t *bsp_touch_indev_init(lv_disp_t *disp) {
    /* Initilize I2C (IDF6 new API) */
    ESP_LOGI(TAG, "Initialize I2C bus");
    i2c_master_bus_config_t i2c_bus_conf = {
        .i2c_port = BSP_TOUCH_I2C_NUM,
        .sda_io_num = BSP_TOUCH_I2C_SDA,
        .scl_io_num = BSP_TOUCH_I2C_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    i2c_master_bus_handle_t i2c_bus_handle = NULL;
    if (i2c_new_master_bus(&i2c_bus_conf, &i2c_bus_handle) != ESP_OK) {
        ESP_LOGE(TAG, "I2C initialization failed");
        return NULL;
    }
    touch_bus_handle = i2c_bus_handle;

    /* Initialize touch HW */
    ESP_LOGI(TAG, "Initialize touch panel");

    const esp_lcd_touch_config_t tp_cfg = {
        .x_max = DRV_LCD_H_RES,
        .y_max = DRV_LCD_V_RES,
        .rst_gpio_num = GPIO_NUM_NC, // Shared with LCD reset
        .int_gpio_num = GPIO_NUM_NC,
        .levels =
            {
                .reset = 0,
                .interrupt = 0,
            },
        .flags =
            {
                .swap_xy = DRV_LCD_SWAP_XY,
                .mirror_x = DRV_LCD_MIRROR_X,
                .mirror_y = DRV_LCD_MIRROR_Y,
            },
    };
    const esp_lcd_panel_io_i2c_config_t tp_io_config = ESP_LCD_TOUCH_IO_I2C_SPD2010_CONFIG();
    /* IDF6: esp_lcd_panel_io_i2c_config_t 把频率字段从 freq_hz 改名为 scl_speed_hz，
       且 ESP_LCD_TOUCH_IO_I2C_SPD2010_CONFIG() 宏不再设置它，这里必须在调用前补齐，
       否则 i2c_master_bus_add_device 会报 invalid scl frequency。 */
    esp_lcd_panel_io_i2c_config_t tp_io_cfg = tp_io_config;
    tp_io_cfg.scl_speed_hz = BSP_TOUCH_I2C_CLK;
    /* IDF6 触摸回归修复（真机验证 v4）：保持官方默认 disable_control_phase=1。
       根因：disable_control_phase=0（= control_phase_enabled=1）会让 IDF6 panel_io
       在每次 tx/rx 前多发 1 字节 0x00 control byte，SPD2010 把它当成寄存器地址，
       导致 write 命令写错位置、IC 状态机被写入垃圾、卡在 tic_busy=1 永不进 bios/cpu；
       read 读错寄存器返回全 0。
       v4 决策（基于 raw diag 真机验证：raw status 稳定 cpu_run=1，点屏时 pt_exist=1）：
       - write 路径：driver i2c_write 宏保持 cmd=0 不变（官方原值）。在
         disable_control_phase=1 + lcd_cmd_bits=0 下，cmd 段与 control 段都为空，
         multi_buffer 跳过空段，只发 payload（寄存器地址），等价于 raw diag 已验证
         的成功路径 tx_param(io, 0, ...)。
       - read 路径：driver i2c_read 宏 cmd 从 0 改为 -1（patch 脚本持久化），
         走 IDF6 i2c_master_receive 纯读，不发 write phase，避免破坏
         “先写寄存器地址再纯读” 的两步法时序。*/
    BSP_ERROR_CHECK_RETURN_NULL(esp_lcd_new_panel_io_i2c(i2c_bus_handle, &tp_io_cfg, &tp_io_handle));
    esp_err_t tp_ret = esp_lcd_touch_new_i2c_spd2010(tp_io_handle, &tp_cfg, &tp_handle);
    if (tp_ret != ESP_OK) {
        ESP_LOGW(TAG, "SPD2010 touch init failed (%s), touch disabled but LCD continues", esp_err_to_name(tp_ret));
        return NULL;
    }

    // Note: read once to initialize the touch panel
    vTaskDelay(50 / portTICK_PERIOD_MS);
    esp_lcd_touch_read_data(tp_handle);

    vTaskDelay(100 / portTICK_PERIOD_MS);

    const lvgl_port_touch_cfg_t touch = {
        .disp = disp,
        .handle = tp_handle,
        .sensitivity = CONFIG_LVGL_INPUT_DEVICE_SENSITIVITY,
    };
    return lvgl_port_add_touch(&touch);
}

esp_lcd_panel_handle_t bsp_lcd_get_panel_handle() {
    return panel_handle;
}

esp_lcd_touch_handle_t bsp_lcd_get_touch_handle() {
    return tp_handle;
}

lv_disp_t *bsp_lvgl_init(void) {
#ifdef CONFIG_HEAP_ABORT_WHEN_ALLOCATION_FAILS
    BSP_ERROR_CHECK_RETURN_NULL(heap_caps_register_failed_alloc_callback(heap_caps_alloc_failed_hook));
#endif
    bsp_display_cfg_t cfg = {
        .lvgl_port_cfg = ESP_LVGL_PORT_INIT_CONFIG(),
        .buffer_size = DRV_LCD_H_RES * watcher_lvgl_draw_lines(LVGL_DRAW_BUFF_HEIGHT, WATCHER_CLAW_DISPLAY_ENABLED),
        .double_buffer = LVGL_DRAW_BUFF_DOUBLE,
        .flags = {
            .buff_dma = false,
            .buff_spiram = true,
        }};
    cfg.lvgl_port_cfg.task_priority = CONFIG_LVGL_PORT_TASK_PRIORITY;
    cfg.lvgl_port_cfg.task_affinity = CONFIG_LVGL_PORT_TASK_AFFINITY;
    cfg.lvgl_port_cfg.task_stack = CONFIG_LVGL_PORT_TASK_STACK_SIZE;
    cfg.lvgl_port_cfg.task_max_sleep_ms = CONFIG_LVGL_PORT_TASK_MAX_SLEEP_MS;
    cfg.lvgl_port_cfg.timer_period_ms = CONFIG_LVGL_PORT_TIMER_PERIOD_MS;
    return bsp_lvgl_init_with_cfg(&cfg);
}

lv_disp_t *bsp_lvgl_init_with_cfg(const bsp_display_cfg_t *cfg) {
    if (lvgl_disp != NULL)
        return lvgl_disp;
    /* Standalone callers must power the panel before LCD/touch reset.
     * Prepare the shared expander first: its initial output reset must not
     * turn off a panel that has already been initialized. */
    if (bsp_io_expander_init() == NULL)
        return NULL;
    if (bsp_exp_io_set_level(BSP_PWR_LCD, 1) != ESP_OK)
        return NULL;
    vTaskDelay(pdMS_TO_TICKS(20));
    if (lvgl_port_init(&cfg->lvgl_port_cfg) != ESP_OK)
        return NULL;
    if (bsp_lcd_backlight_init() != ESP_OK)
        return NULL;
    lvgl_disp = bsp_display_lcd_init(cfg);
    if (lvgl_disp != NULL) {
        lvgl_disp->driver->rounder_cb = bsp_lvgl_rounder_cb;

#if CONFIG_LVGL_INPUT_DEVICE_USE_KNOB
        knob_indev = bsp_knob_indev_init(lvgl_disp);
#endif
#if CONFIG_LVGL_INPUT_DEVICE_USE_TP
        touch_indev = bsp_touch_indev_init(lvgl_disp);
        /* Official applications may deliberately run without touch. SDK
         * validates the registered input independently of LCD readiness. */
#endif
    }
    return lvgl_disp;
}

lv_disp_t *bsp_lvgl_get_disp(void) {
    return lvgl_disp;
}

lv_indev_t *bsp_lvgl_get_touch_indev(void) {
    return touch_indev;
}

esp_err_t bsp_lvgl_deinit(void) {
    /* Stop the LVGL worker before destroying borrowed handles. Shared board
     * buses are not freed: camera/audio may still be using them. */
    esp_err_t ret = lvgl_port_quiesce();
    if (ret != ESP_OK)
        return ret;
    if (touch_indev != NULL) {
        lvgl_port_remove_touch(touch_indev);
        touch_indev = NULL;
    }
    if (knob_indev != NULL) {
        lvgl_port_remove_encoder(knob_indev);
        knob_indev = NULL;
    }
    if (lvgl_disp != NULL) {
        ret = lvgl_port_remove_disp(lvgl_disp);
        if (ret != ESP_OK)
            return ret;
        lvgl_disp = NULL;
    }
    if (tp_handle != NULL) {
        ret = esp_lcd_touch_del(tp_handle);
        if (ret != ESP_OK)
            return ret;
        tp_handle = NULL;
    }
    if (tp_io_handle != NULL) {
        ret = esp_lcd_panel_io_del(tp_io_handle);
        if (ret != ESP_OK)
            return ret;
        tp_io_handle = NULL;
    }
    if (touch_bus_handle != NULL) {
        ret = i2c_del_master_bus(touch_bus_handle);
        if (ret != ESP_OK)
            return ret;
        touch_bus_handle = NULL;
    }
    if (panel_handle != NULL) {
        ret = esp_lcd_panel_del(panel_handle);
        if (ret != ESP_OK)
            return ret;
        panel_handle = NULL;
    }
    if (panel_io_handle != NULL) {
        ret = esp_lcd_panel_io_del(panel_io_handle);
        if (ret != ESP_OK)
            return ret;
        panel_io_handle = NULL;
    }
    ret = lvgl_port_deinit();
    return ret == ESP_OK ? bsp_lcd_brightness_set(0) : ret;
}

bool bsp_sdcard_is_inserted(void) {
    return bsp_exp_io_get_level(BSP_SD_GPIO_DET) == 0;
}

void bsp_sdcard_get_status(bsp_sdcard_status_t *status) {
    if (status != NULL) {
        taskENTER_CRITICAL(&s_sdcard_status_mux);
        *status = s_sdcard_status;
        taskEXIT_CRITICAL(&s_sdcard_status_mux);
    }
}

static void watcher_sdcard_set_status(bsp_sdcard_state_t state, bsp_sdcard_type_t card_type,
                                      bsp_sdcard_filesystem_t filesystem, esp_err_t last_error) {
    taskENTER_CRITICAL(&s_sdcard_status_mux);
    s_sdcard_status = (bsp_sdcard_status_t){
        .state = state,
        .card_type = card_type,
        .filesystem = filesystem,
        .last_error = last_error,
    };
    taskEXIT_CRITICAL(&s_sdcard_status_mux);
}

static esp_err_t watcher_sdcard_fail_mount(esp_err_t error) {
    watcher_sdcard_set_status(BSP_SD_STATE_MOUNT_ERROR, BSP_SD_CARD_TYPE_UNKNOWN, BSP_SD_FILESYSTEM_UNKNOWN, error);
    return error;
}

const char *bsp_sdcard_state_name(bsp_sdcard_state_t state) {
    switch (state) {
    case BSP_SD_STATE_NOT_DETECTED:
        return "not_detected";
    case BSP_SD_STATE_PROBING:
        return "probing";
    case BSP_SD_STATE_MOUNTED:
        return "mounted";
    case BSP_SD_STATE_UNSUPPORTED_FILESYSTEM:
        return "unsupported_filesystem";
    case BSP_SD_STATE_MOUNT_ERROR:
        return "mount_error";
    default:
        return "unknown";
    }
}

const char *bsp_sdcard_type_name(bsp_sdcard_type_t type) {
    switch (type) {
    case BSP_SD_CARD_TYPE_SDSC:
        return "sdsc";
    case BSP_SD_CARD_TYPE_SDHC_SDXC:
        return "sdhc_sdxc";
    default:
        return "unknown";
    }
}

const char *bsp_sdcard_filesystem_name(bsp_sdcard_filesystem_t filesystem) {
    switch (filesystem) {
    case BSP_SD_FILESYSTEM_FAT12:
        return "fat12";
    case BSP_SD_FILESYSTEM_FAT16:
        return "fat16";
    case BSP_SD_FILESYSTEM_FAT32:
        return "fat32";
    default:
        return "unknown";
    }
}

static uint16_t watcher_sdcard_read_le16(const uint8_t *value) {
    return (uint16_t)value[0] | ((uint16_t)value[1] << 8U);
}

static uint32_t watcher_sdcard_read_le32(const uint8_t *value) {
    return (uint32_t)value[0] | ((uint32_t)value[1] << 8U) | ((uint32_t)value[2] << 16U) | ((uint32_t)value[3] << 24U);
}

static bool watcher_sdcard_sector_is_fat_boot(const uint8_t sector[512]) {
    uint16_t bytes_per_sector = watcher_sdcard_read_le16(&sector[11]);
    uint8_t sectors_per_cluster = sector[13];
    return sector[510] == 0x55U && sector[511] == 0xaaU && bytes_per_sector == 512U && sectors_per_cluster != 0U &&
           (sectors_per_cluster & (sectors_per_cluster - 1U)) == 0U && sector[16] != 0U;
}

static bsp_sdcard_filesystem_t watcher_sdcard_classify_fat_boot(const uint8_t sector[512]) {
    uint32_t bytes_per_sector = watcher_sdcard_read_le16(&sector[11]);
    uint32_t sectors_per_cluster = sector[13];
    uint32_t reserved_sectors = watcher_sdcard_read_le16(&sector[14]);
    uint32_t fat_count = sector[16];
    uint32_t root_entries = watcher_sdcard_read_le16(&sector[17]);
    uint32_t total_sectors = watcher_sdcard_read_le16(&sector[19]);
    uint32_t fat_sectors = watcher_sdcard_read_le16(&sector[22]);
    if (total_sectors == 0U) {
        total_sectors = watcher_sdcard_read_le32(&sector[32]);
    }
    if (fat_sectors == 0U) {
        fat_sectors = watcher_sdcard_read_le32(&sector[36]);
    }
    if (bytes_per_sector == 0U || sectors_per_cluster == 0U || total_sectors == 0U || fat_sectors == 0U) {
        return BSP_SD_FILESYSTEM_UNKNOWN;
    }
    uint64_t root_sectors = (((uint64_t)root_entries * 32U) + (bytes_per_sector - 1U)) / bytes_per_sector;
    uint64_t metadata_sectors = reserved_sectors + ((uint64_t)fat_count * fat_sectors) + root_sectors;
    if (metadata_sectors >= total_sectors) {
        return BSP_SD_FILESYSTEM_UNKNOWN;
    }
    uint32_t clusters = (uint32_t)(((uint64_t)total_sectors - metadata_sectors) / sectors_per_cluster);
    if (clusters < 4085U) {
        return BSP_SD_FILESYSTEM_FAT12;
    }
    if (clusters < 65525U) {
        return BSP_SD_FILESYSTEM_FAT16;
    }
    return BSP_SD_FILESYSTEM_FAT32;
}

static bsp_sdcard_filesystem_t watcher_sdcard_classify_fat(sdmmc_card_t *mounted_card) {
    uint8_t sector[512];
    if (mounted_card == NULL || mounted_card->csd.sector_size != sizeof(sector) ||
        sdmmc_read_sectors(mounted_card, sector, 0U, 1U) != ESP_OK) {
        return BSP_SD_FILESYSTEM_UNKNOWN;
    }
    if (watcher_sdcard_sector_is_fat_boot(sector)) {
        return watcher_sdcard_classify_fat_boot(sector);
    }

    for (size_t partition = 0U; partition < 4U; ++partition) {
        const uint8_t *entry = &sector[446U + partition * 16U];
        uint32_t first_sector = watcher_sdcard_read_le32(&entry[8]);
        if (entry[4] == 0U || first_sector == 0U) {
            continue;
        }
        if (sdmmc_read_sectors(mounted_card, sector, first_sector, 1U) == ESP_OK &&
            watcher_sdcard_sector_is_fat_boot(sector)) {
            return watcher_sdcard_classify_fat_boot(sector);
        }
    }
    return BSP_SD_FILESYSTEM_UNKNOWN;
}

static esp_err_t watcher_sdcard_power_cycle(void) {
    esp_err_t ret = bsp_exp_io_set_level(BSP_PWR_SDCARD, 0);
    if (ret != ESP_OK) {
        return ret;
    }
    vTaskDelay(pdMS_TO_TICKS(WATCHER_SD_POWER_OFF_DELAY_MS));

    ret = bsp_exp_io_set_level(BSP_PWR_SDCARD, 1);
    if (ret != ESP_OK) {
        return ret;
    }
    vTaskDelay(pdMS_TO_TICKS(WATCHER_SD_POWER_ON_DELAY_MS));
    uint32_t output_latch = 0;
    ret = io_exp_handle != NULL && io_exp_handle->read_output_reg != NULL
              ? io_exp_handle->read_output_reg(io_exp_handle, &output_latch)
              : ESP_ERR_INVALID_STATE;
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read SD power latch: %s", esp_err_to_name(ret));
        return ret;
    }
    if ((output_latch & BSP_PWR_SDCARD) == 0U) {
        ESP_LOGE(TAG, "SD power latch mismatch: expected=on actual=0x%04" PRIx32, output_latch);
        return ESP_ERR_INVALID_STATE;
    }
    ESP_LOGI(TAG, "SD power latch verified: output=0x%04" PRIx32, output_latch);
    return ESP_OK;
}

static esp_err_t watcher_sdcard_verify_shared_chip_selects(void) {
    // gpio_get_level() reads the input path and therefore always returns zero
    // for pins configured as output-only. Inspect the GPIO output latch here;
    // SDSPI will switch the SD pin between high and low during transactions.
    const uint32_t output_low = REG_READ(GPIO_OUT_REG);
    const uint32_t output_high = REG_READ(GPIO_OUT1_REG);
    const int sscma_cs = BSP_SSCMA_CLIENT_SPI_CS < 32 ? (int)((output_low >> BSP_SSCMA_CLIENT_SPI_CS) & 1U)
                                                      : (int)((output_high >> (BSP_SSCMA_CLIENT_SPI_CS - 32)) & 1U);
    const int sd_cs = BSP_SD_SPI_CS < 32 ? (int)((output_low >> BSP_SD_SPI_CS) & 1U)
                                         : (int)((output_high >> (BSP_SD_SPI_CS - 32)) & 1U);
    if (sscma_cs != 1 || sd_cs != 1) {
        ESP_LOGE(TAG, "SPI2 chip-select isolation failed: sscma_cs=%d sd_cs=%d", sscma_cs, sd_cs);
        return ESP_ERR_INVALID_STATE;
    }
    ESP_LOGI(TAG, "SPI2 chip selects verified high: sscma_cs=%d sd_cs=%d", sscma_cs, sd_cs);
    return ESP_OK;
}

static bool watcher_sdcard_mount_error_is_retryable(esp_err_t error) {
    return error != ESP_ERR_INVALID_ARG && error != ESP_ERR_INVALID_STATE && error != ESP_ERR_NO_MEM;
}

#define WATCHER_SDMMC_INIT_STEP(target_card, condition, function)                                                      \
    do {                                                                                                               \
        if ((condition)) {                                                                                             \
            esp_err_t step_err = (function)(target_card);                                                              \
            if (step_err != ESP_OK) {                                                                                  \
                ESP_LOGD(TAG, "%s: %s returned 0x%x", __func__, #function, step_err);                                  \
                return step_err;                                                                                       \
            }                                                                                                          \
        }                                                                                                              \
    } while (0)

static esp_err_t watcher_sdmmc_card_init_compatible(const sdmmc_host_t *config, sdmmc_card_t *out_card) {
    esp_err_t ret = ESP_FAIL;
    memset(out_card, 0, sizeof(*out_card));
    memcpy(&out_card->host, config, sizeof(*config));

    const bool is_spi = host_is_spi(out_card);
    const bool always = true;
#if CONFIG_SD_ENABLE_SDIO_SUPPORT
    // This board exposes the resource card through SDSPI only. Probing SDIO
    // with CMD52/CMD5 before memory initialisation is unnecessary here and can
    // leave older SPI cards in idle state indefinitely under IDF 6.
    const bool io_supported = !is_spi;
    if (is_spi) {
        out_card->is_mem = 1;
    }
#else
    out_card->is_mem = 1;
#endif

    if (config->pwr_ctrl_handle != NULL) {
        const int voltage_mv = config->io_voltage * 1000;
        ret = sd_pwr_ctrl_set_io_voltage(config->pwr_ctrl_handle, voltage_mv);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to set SD I/O voltage: %s", esp_err_to_name(ret));
            return ret;
        }
    }

    WATCHER_SDMMC_INIT_STEP(out_card, !is_spi, sdmmc_allocate_aligned_buf);

    WATCHER_SDMMC_INIT_STEP(out_card, !is_spi, sdmmc_fix_host_flags);
    WATCHER_SDMMC_INIT_STEP(out_card, always, sdmmc_check_host_function_ptr_integrity);
#if CONFIG_SD_ENABLE_SDIO_SUPPORT
    WATCHER_SDMMC_INIT_STEP(out_card, io_supported, sdmmc_io_reset);
#endif
    WATCHER_SDMMC_INIT_STEP(out_card, always, sdmmc_send_cmd_go_idle_state);
    WATCHER_SDMMC_INIT_STEP(out_card, always, sdmmc_init_sd_if_cond);
#if CONFIG_SD_ENABLE_SDIO_SUPPORT
    WATCHER_SDMMC_INIT_STEP(out_card, io_supported, sdmmc_init_io);
#endif

    const bool is_mem = out_card->is_mem;
    const bool is_sdio = !is_mem;
    const bool ignore_data_crc = (config->flags & SDMMC_HOST_FLAG_SPI_IGNORE_DATA_CRC) != 0U;

    if (is_spi && !ignore_data_crc) {
        esp_err_t crc_err = sdmmc_init_spi_crc(out_card);
        if (crc_err == ESP_ERR_NOT_SUPPORTED) {
            ESP_LOGW(TAG, "SD card rejected CMD59 CRC_ON_OFF in SPI mode; continuing without data CRC");
        } else if (crc_err != ESP_OK) {
            ESP_LOGD(TAG, "%s: sdmmc_init_spi_crc returned 0x%x", __func__, crc_err);
            return crc_err;
        }
    }

    WATCHER_SDMMC_INIT_STEP(out_card, is_mem, sdmmc_init_ocr);

    const bool is_mmc = is_mem && out_card->is_mmc;
    const bool is_sdmem = is_mem && !is_mmc;
    ESP_LOGD(TAG, "%s: card type is %s", __func__, is_sdio ? "SDIO" : is_mmc ? "MMC" : "SD");

    const bool is_uhs1 = is_sdmem && (out_card->ocr & SD_OCR_S18_RA) != 0U && (out_card->ocr & SD_OCR_SDHC_CAP) != 0U;
    WATCHER_SDMMC_INIT_STEP(out_card, is_uhs1, sdmmc_init_sd_uhs1);

    WATCHER_SDMMC_INIT_STEP(out_card, is_mem, sdmmc_init_cid);
    WATCHER_SDMMC_INIT_STEP(out_card, !is_spi, sdmmc_init_rca);
    WATCHER_SDMMC_INIT_STEP(out_card, is_mem, sdmmc_init_csd);
    WATCHER_SDMMC_INIT_STEP(out_card, is_mmc && !is_spi, sdmmc_init_mmc_decode_cid);
    WATCHER_SDMMC_INIT_STEP(out_card, !is_spi, sdmmc_init_select_card);
    WATCHER_SDMMC_INIT_STEP(out_card, is_sdmem, sdmmc_init_sd_blocklen);
    WATCHER_SDMMC_INIT_STEP(out_card, is_sdmem, sdmmc_init_sd_scr);
    WATCHER_SDMMC_INIT_STEP(out_card, is_sdmem, sdmmc_init_sd_wait_data_ready);
    WATCHER_SDMMC_INIT_STEP(out_card, is_mmc, sdmmc_init_mmc_read_ext_csd);
    WATCHER_SDMMC_INIT_STEP(out_card, always, sdmmc_init_card_hs_mode);

    if (!is_spi) {
        WATCHER_SDMMC_INIT_STEP(out_card, is_sdmem, sdmmc_init_sd_bus_width);
#if CONFIG_SD_ENABLE_SDIO_SUPPORT
        WATCHER_SDMMC_INIT_STEP(out_card, is_sdio, sdmmc_init_io_bus_width);
#endif
        WATCHER_SDMMC_INIT_STEP(out_card, is_mmc, sdmmc_init_mmc_bus_width);
        WATCHER_SDMMC_INIT_STEP(out_card, always, sdmmc_init_host_bus_width);
    }

    WATCHER_SDMMC_INIT_STEP(out_card, is_sdmem, sdmmc_init_sd_ssr);
    WATCHER_SDMMC_INIT_STEP(out_card, always, sdmmc_init_host_frequency);
    WATCHER_SDMMC_INIT_STEP(out_card, is_uhs1, sdmmc_init_sd_driver_strength);
    WATCHER_SDMMC_INIT_STEP(out_card, is_uhs1, sdmmc_init_sd_current_limit);
    WATCHER_SDMMC_INIT_STEP(out_card, is_uhs1, sdmmc_init_sd_timing_tuning);
    WATCHER_SDMMC_INIT_STEP(out_card, is_sdmem, sdmmc_check_scr);
    WATCHER_SDMMC_INIT_STEP(out_card, is_mmc, sdmmc_init_mmc_check_ext_csd);
    return ESP_OK;
}

static esp_err_t watcher_sdspi_mount_compatible(const char *base_path, const sdmmc_host_t *host_config,
                                                const sdspi_device_config_t *slot_config,
                                                const esp_vfs_fat_mount_config_t *mount_config,
                                                sdmmc_card_t **out_card) {
    if (base_path == NULL || host_config == NULL || slot_config == NULL || mount_config == NULL || out_card == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    sdmmc_card_t *new_card = calloc(1, sizeof(*new_card));
    if (new_card == NULL) {
        return ESP_ERR_NO_MEM;
    }

    sdmmc_host_t host = *host_config;
    sdspi_dev_handle_t card_handle = -1;
    bool device_attached = false;
    esp_err_t ret = host.init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize SDSPI host: %s", esp_err_to_name(ret));
        goto fail;
    }

    ret = sdspi_host_init_device(slot_config, &card_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to attach SD card to SPI bus: %s", esp_err_to_name(ret));
        goto fail;
    }
    device_attached = true;
    host.slot = card_handle;

    ret = watcher_sdmmc_card_init_compatible(&host, new_card);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Compatible SDSPI card init failed: %s", esp_err_to_name(ret));
        goto fail;
    }

    ret = esp_vfs_fat_mount_initialized(new_card, base_path, mount_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount initialized SD card: %s", esp_err_to_name(ret));
        goto fail;
    }

    *out_card = new_card;
    return ESP_OK;

fail:
    /* esp_vfs_fat_mount_initialized() can fail after registering the disk and
     * VFS context. Its public failure cleanup removes those registrations, but
     * the card and SDSPI device still belong to this wrapper. Remove the
     * device exactly once before freeing the card. */
    if (device_attached) {
        (void)sdspi_host_remove_device(card_handle);
    }
    free(new_card);
    return ret;
}

static esp_err_t watcher_sdcard_send_app_command(sdmmc_card_t *probe_card, sdmmc_command_t *command,
                                                 uint8_t *out_cmd55_r1) {
    sdmmc_command_t app_command = {
        .opcode = MMC_APP_CMD,
        .arg = 0,
        .flags = SCF_CMD_AC | SCF_RSP_R1,
    };
    esp_err_t ret = probe_card->host.do_transaction(probe_card->host.slot, &app_command);
    if (out_cmd55_r1 != NULL) {
        *out_cmd55_r1 = (uint8_t)(app_command.response[0] & 0xffU);
    }
    if (ret != ESP_OK) {
        return ret;
    }
    return probe_card->host.do_transaction(probe_card->host.slot, command);
}

static esp_err_t watcher_sdcard_reset_protocol_probe(sdmmc_card_t *probe_card) {
    sdmmc_command_t command = {
        .opcode = MMC_GO_IDLE_STATE,
        .arg = 0,
        .flags = SCF_CMD_BC,
    };
    (void)probe_card->host.do_transaction(probe_card->host.slot, &command);
    vTaskDelay(pdMS_TO_TICKS(10));

    command = (sdmmc_command_t){
        .opcode = MMC_GO_IDLE_STATE,
        .arg = 0,
        .flags = SCF_CMD_BC | SCF_RSP_R1,
    };
    esp_err_t ret = probe_card->host.do_transaction(probe_card->host.slot, &command);
    if (ret != ESP_OK) {
        return ret;
    }
    vTaskDelay(pdMS_TO_TICKS(10));

    command = (sdmmc_command_t){
        .opcode = SD_SEND_IF_COND,
        .arg = (1U << 8) | 0xaaU,
        .flags = SCF_CMD_BCR | SCF_RSP_R7,
    };
    ret = probe_card->host.do_transaction(probe_card->host.slot, &command);
    if (ret == ESP_ERR_TIMEOUT || ret == ESP_ERR_NOT_SUPPORTED) {
        return ESP_OK;
    }
    return ret;
}

static esp_err_t watcher_sdcard_probe_acmd41_variant(sdmmc_card_t *probe_card, const char *name, uint32_t arg) {
    esp_err_t ret = watcher_sdcard_reset_protocol_probe(probe_card);
    uint8_t r1 = 0xffU;
    uint8_t cmd55_r1 = 0xffU;
    unsigned attempts = 0;

    if (ret == ESP_OK) {
        for (attempts = 1; attempts <= WATCHER_SD_OCR_PROBE_MAX_ATTEMPTS; ++attempts) {
            sdmmc_command_t command = {
                .opcode = SD_APP_OP_COND,
                .arg = arg,
                .flags = SCF_CMD_BCR | SCF_RSP_R3,
            };
            ret = watcher_sdcard_send_app_command(probe_card, &command, &cmd55_r1);
            r1 = (uint8_t)(command.response[0] & 0xffU);
            if (ret != ESP_OK || (r1 & SD_SPI_R1_IDLE_STATE) == 0U) {
                break;
            }
            vTaskDelay(pdMS_TO_TICKS(WATCHER_SD_OCR_PROBE_RETRY_DELAY_MS));
        }
        if (ret == ESP_OK && (r1 & SD_SPI_R1_IDLE_STATE) != 0U) {
            ret = ESP_ERR_TIMEOUT;
        }
    }

    ESP_LOGI(TAG, "SD protocol probe: ACMD41[%s]=%s cmd55_r1=0x%02x r1=0x%02x attempts=%u", name, esp_err_to_name(ret),
             cmd55_r1, r1, attempts > WATCHER_SD_OCR_PROBE_MAX_ATTEMPTS ? WATCHER_SD_OCR_PROBE_MAX_ATTEMPTS : attempts);
    return ret;
}

static esp_err_t watcher_sdcard_probe_after_ocr_timeout(const sdspi_device_config_t *slot_config,
                                                        const sdmmc_host_t *mount_host) {
    sdmmc_host_t probe_host = SDSPI_HOST_DEFAULT();
    sdmmc_card_t probe_card = {0};
    sdspi_dev_handle_t probe_handle = -1;
    esp_err_t ret = probe_host.init();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "SD protocol probe host init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = sdspi_host_init_device(slot_config, &probe_handle);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "SD protocol probe device attach failed: %s", esp_err_to_name(ret));
        return ret;
    }
    probe_host.slot = probe_handle;
    probe_host.max_freq_khz = mount_host->max_freq_khz;
    probe_host.dma_aligned_buffer = mount_host->dma_aligned_buffer;
    memcpy(&probe_card.host, &probe_host, sizeof(probe_host));
    probe_card.is_mem = 1;

    sdmmc_command_t command = {
        .opcode = MMC_GO_IDLE_STATE,
        .arg = 0,
        .flags = SCF_CMD_BC | SCF_RSP_R1,
    };
    ret = probe_card.host.do_transaction(probe_card.host.slot, &command);
    ESP_LOGI(TAG, "SD protocol probe: CMD0=%s", esp_err_to_name(ret));
    if (ret != ESP_OK) {
        goto done;
    }

    command = (sdmmc_command_t){
        .opcode = SD_SEND_IF_COND,
        .arg = (1U << 8) | 0xaaU,
        .flags = SCF_CMD_BCR | SCF_RSP_R7,
    };
    ret = probe_card.host.do_transaction(probe_card.host.slot, &command);
    ESP_LOGI(TAG, "SD protocol probe: CMD8=%s", esp_err_to_name(ret));
    if (ret != ESP_OK && ret != ESP_ERR_TIMEOUT && ret != ESP_ERR_NOT_SUPPORTED) {
        goto done;
    }

    static const struct {
        const char *name;
        uint32_t arg;
    } acmd41_variants[] = {
        {.name = "hcs", .arg = SD_OCR_SDHC_CAP},
        {.name = "legacy-zero", .arg = 0},
        {.name = "hcs-voltage", .arg = SD_OCR_SDHC_CAP | SD_OCR_VOL_MASK},
    };
    esp_err_t acmd41_ret = ESP_ERR_TIMEOUT;
    for (size_t variant_index = 0; variant_index < sizeof(acmd41_variants) / sizeof(acmd41_variants[0]);
         ++variant_index) {
        esp_err_t variant_ret = watcher_sdcard_probe_acmd41_variant(&probe_card, acmd41_variants[variant_index].name,
                                                                    acmd41_variants[variant_index].arg);
        if (variant_ret == ESP_OK) {
            acmd41_ret = ESP_OK;
        }
    }

    (void)watcher_sdcard_reset_protocol_probe(&probe_card);

    command = (sdmmc_command_t){
        .opcode = MMC_SEND_OP_COND,
        .arg = 0,
        .flags = SCF_CMD_BCR | SCF_RSP_R3,
    };
    esp_err_t cmd1_ret = probe_card.host.do_transaction(probe_card.host.slot, &command);
    uint8_t cmd1_r1 = (uint8_t)(command.response[0] & 0xffU);
    ESP_LOGI(TAG, "SD protocol probe: CMD1=%s r1=0x%02x", esp_err_to_name(cmd1_ret), cmd1_r1);
    ret = acmd41_ret;

done:
    (void)sdspi_host_remove_device(probe_handle);
    return ret;
}

esp_err_t bsp_sdcard_init(char *mount_point, size_t max_files) {
    typedef struct {
        uint32_t frequency_khz;
        bool ignore_data_crc;
        bool allow_sdsc;
    } watcher_sd_mount_attempt_t;
    static const watcher_sd_mount_attempt_t attempts[] = {
        {.frequency_khz = WATCHER_SD_SPI_MAX_FREQ_KHZ, .ignore_data_crc = false, .allow_sdsc = false},
        {.frequency_khz = WATCHER_SD_SPI_FALLBACK_FREQ_KHZ, .ignore_data_crc = false, .allow_sdsc = true},
        {.frequency_khz = WATCHER_SD_SPI_SAFE_FREQ_KHZ, .ignore_data_crc = false, .allow_sdsc = true},
        {.frequency_khz = WATCHER_SD_SPI_SAFE_FREQ_KHZ, .ignore_data_crc = true, .allow_sdsc = true},
    };
    esp_err_t ret;
    size_t attempts_performed = 0;

    if (card != NULL) {
        return ESP_OK;
    }
    if (mount_point == NULL || max_files == 0U) {
        return ESP_ERR_INVALID_ARG;
    }

    bool detect_asserted = bsp_sdcard_is_inserted();
    watcher_sdcard_set_status(BSP_SD_STATE_PROBING, BSP_SD_CARD_TYPE_UNKNOWN, BSP_SD_FILESYSTEM_UNKNOWN,
                              ESP_ERR_NOT_FINISHED);
    if (!detect_asserted) {
        ESP_LOGW(TAG, "SD detect switch is open; running the bounded bus probe in case the switch is absent or faulty");
    }

    ret = bsp_spi_bus_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize SPI bus for SD card: %s", esp_err_to_name(ret));
        return watcher_sdcard_fail_mount(ret);
    }
    bsp_io_expander_init();

    // SD and SSCMA share SPI2. Drive both chip selects high before probing the card
    // so the uninitialized peer device cannot see the bus handshake.
    const gpio_config_t shared_bus_cs_config = {
        .pin_bit_mask = (1ULL << BSP_SD_SPI_CS) | (1ULL << BSP_SSCMA_CLIENT_SPI_CS),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ret = gpio_config(&shared_bus_cs_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure SPI2 chip selects for SD card: %s", esp_err_to_name(ret));
        return watcher_sdcard_fail_mount(ret);
    }
    ret = bsp_exp_io_set_level(BSP_PWR_AI_CHIP, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to power down SSCMA client during SD init: %s", esp_err_to_name(ret));
        return watcher_sdcard_fail_mount(ret);
    }
    ret = gpio_set_level(BSP_SSCMA_CLIENT_SPI_CS, 1);
    ret |= gpio_set_level(BSP_SD_SPI_CS, 1);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to isolate SPI2 chip selects before SD init: %s", esp_err_to_name(ret));
        return watcher_sdcard_fail_mount(ret);
    }
    vTaskDelay(pdMS_TO_TICKS(20));

    // Keep the board power-on sequence used by the known-good firmware for the
    // first probe. Toggling the expander-controlled SD rail here can leave some
    // cards partially powered through the shared SPI pins, so reserve a real
    // power cycle for bounded recovery attempts after a failed probe.
    ret = watcher_sdcard_verify_shared_chip_selects();
    if (ret != ESP_OK) {
        return watcher_sdcard_fail_mount(ret);
    }

    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = BSP_SD_SPI_CS;
    slot_config.host_id = BSP_SD_SPI_NUM;
    // IDF 6 waits for MISO to become high before every command. On this board
    // SPI2 is shared with the powered-down SSCMA device, so that pre-command
    // poll can consume the old card's complete power-up window. IDF 5 did not
    // perform the poll; preserve that proven bus behaviour for this slot.
    slot_config.wait_for_miso = -1;
    esp_vfs_fat_mount_config_t mount_config = {
        .format_if_mount_failed = false, .max_files = max_files, .allocation_unit_size = 16 * 1024};

    if (s_sd_dma_buffer == NULL) {
        s_sd_dma_buffer = heap_caps_aligned_alloc(4, 4096, MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
        if (s_sd_dma_buffer == NULL) {
            return watcher_sdcard_fail_mount(ESP_ERR_NO_MEM);
        }
    }
    ret = ESP_FAIL;
    for (size_t attempt_index = 0; attempt_index < sizeof(attempts) / sizeof(attempts[0]); ++attempt_index) {
        const watcher_sd_mount_attempt_t *attempt = &attempts[attempt_index];
        attempts_performed = attempt_index + 1U;
        sdmmc_host_t host = SDSPI_HOST_DEFAULT();
        host.dma_aligned_buffer = s_sd_dma_buffer;
        host.slot = BSP_SD_SPI_NUM;
        host.max_freq_khz = attempt->frequency_khz;
        if (attempt->ignore_data_crc) {
            host.flags |= SDMMC_HOST_FLAG_SPI_IGNORE_DATA_CRC;
        }

        ESP_LOGI(TAG, "Mounting SD card: attempt=%u frequency=%" PRIu32 "kHz data_crc=%s",
                 (unsigned)(attempt_index + 1U), attempt->frequency_khz,
                 attempt->ignore_data_crc ? "compatibility-ignore" : "required");
        ret = watcher_sdspi_mount_compatible(mount_point, &host, &slot_config, &mount_config, &card);
        if (ret == ESP_OK) {
            bsp_sdcard_type_t card_type =
                (card->ocr & SD_OCR_SDHC_CAP) != 0U ? BSP_SD_CARD_TYPE_SDHC_SDXC : BSP_SD_CARD_TYPE_SDSC;
            bsp_sdcard_filesystem_t filesystem = watcher_sdcard_classify_fat(card);
            if (filesystem != BSP_SD_FILESYSTEM_FAT32) {
                ESP_LOGE(TAG, "Unsupported SD filesystem: card=%s fs=%s; product resources require FAT32",
                         bsp_sdcard_type_name(card_type), bsp_sdcard_filesystem_name(filesystem));
                (void)esp_vfs_fat_sdcard_unmount(mount_point, card);
                card = NULL;
                esp_err_t classification_error =
                    filesystem == BSP_SD_FILESYSTEM_UNKNOWN ? ESP_FAIL : ESP_ERR_NOT_SUPPORTED;
                watcher_sdcard_set_status(filesystem == BSP_SD_FILESYSTEM_UNKNOWN ? BSP_SD_STATE_MOUNT_ERROR
                                                                                  : BSP_SD_STATE_UNSUPPORTED_FILESYSTEM,
                                          card_type, filesystem, classification_error);
                return classification_error;
            }
            if (card_type == BSP_SD_CARD_TYPE_SDSC && !attempt->allow_sdsc) {
                ESP_LOGW(TAG, "Remounting SDSC at conservative runtime frequency; the 20 MHz probe is not a durability "
                              "qualification");
                (void)esp_vfs_fat_sdcard_unmount(mount_point, card);
                card = NULL;
                esp_err_t power_ret = watcher_sdcard_power_cycle();
                if (power_ret != ESP_OK) {
                    ESP_LOGE(TAG, "Failed to power-cycle SDSC before conservative remount: %s",
                             esp_err_to_name(power_ret));
                    return watcher_sdcard_fail_mount(power_ret);
                }
                gpio_set_level(BSP_SSCMA_CLIENT_SPI_CS, 1);
                gpio_set_level(BSP_SD_SPI_CS, 1);
                continue;
            }
            watcher_sdcard_set_status(BSP_SD_STATE_MOUNTED, card_type, filesystem, ESP_OK);
            ESP_LOGI(TAG, "SD card mounted: attempt=%u frequency=%" PRIu32 "kHz data_crc=%s",
                     (unsigned)(attempt_index + 1U), attempt->frequency_khz,
                     attempt->ignore_data_crc ? "compatibility-ignore" : "required");
            ESP_LOGI(TAG, "SD product contract accepted: card=%s fs=%s sector=512", bsp_sdcard_type_name(card_type),
                     bsp_sdcard_filesystem_name(filesystem));
            sdmmc_card_print_info(stdout, card);
            return ESP_OK;
        }

        card = NULL;
        ESP_LOGW(TAG, "SD mount failed: attempt=%u frequency=%" PRIu32 "kHz data_crc=%s err=%s (0x%x)",
                 (unsigned)(attempt_index + 1U), attempt->frequency_khz,
                 attempt->ignore_data_crc ? "compatibility-ignore" : "required", esp_err_to_name(ret), ret);
        if (ret == ESP_ERR_TIMEOUT && attempt_index == 0U) {
            (void)watcher_sdcard_probe_after_ocr_timeout(&slot_config, &host);
        }
        if (!watcher_sdcard_mount_error_is_retryable(ret) ||
            attempt_index + 1U >= sizeof(attempts) / sizeof(attempts[0])) {
            break;
        }

        esp_err_t power_ret = watcher_sdcard_power_cycle();
        if (power_ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to power-cycle SD card before retry: %s", esp_err_to_name(power_ret));
            return watcher_sdcard_fail_mount(power_ret);
        }
        gpio_set_level(BSP_SSCMA_CLIENT_SPI_CS, 1);
        gpio_set_level(BSP_SD_SPI_CS, 1);
    }

    ESP_LOGE(TAG, "Failed to mount SD card at %s after %u bounded attempt(s): %s", mount_point,
             (unsigned)attempts_performed, esp_err_to_name(ret));
    watcher_sdcard_set_status(detect_asserted ? BSP_SD_STATE_MOUNT_ERROR : BSP_SD_STATE_NOT_DETECTED,
                              BSP_SD_CARD_TYPE_UNKNOWN, BSP_SD_FILESYSTEM_UNKNOWN, ret);
    return ret;
}

esp_err_t bsp_sdcard_init_default(void) {
    return bsp_sdcard_init(DRV_BASE_PATH_SD, DRV_FS_MAX_FILES);
}

esp_err_t bsp_sdcard_deinit(char *mount_point) {
    esp_err_t ret;

    if (NULL == mount_point) {
        return ESP_ERR_INVALID_STATE;
    }
    if (card == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    ret = esp_vfs_fat_sdcard_unmount(mount_point, card);
    if (ret == ESP_OK) {
        card = NULL;
        watcher_sdcard_set_status(bsp_sdcard_is_inserted() ? BSP_SD_STATE_PROBING : BSP_SD_STATE_NOT_DETECTED,
                                  BSP_SD_CARD_TYPE_UNKNOWN, BSP_SD_FILESYSTEM_UNKNOWN, ESP_ERR_INVALID_STATE);
    }
    return ret;
}

esp_err_t bsp_sdcard_deinit_default(void) {
    return bsp_sdcard_deinit(DRV_BASE_PATH_SD);
}

esp_err_t bsp_spiffs_init(char *mount_point, size_t max_files) {
    static bool inited = false;
    esp_err_t register_ret;
    if (inited) {
        return ESP_OK;
    }
    esp_vfs_spiffs_conf_t conf = {
        .base_path = mount_point,
        .partition_label = "storage",
        .max_files = max_files,
        .format_if_mount_failed = false,
    };
    register_ret = esp_vfs_spiffs_register(&conf);
    if (register_ret != ESP_OK) {
        ESP_LOGE(TAG, "SPIFFS mount failed: label=%s base_path=%s err=%s", conf.partition_label, conf.base_path,
                 esp_err_to_name(register_ret));
        return register_ret;
    }

    size_t total = 0, used = 0;
    esp_err_t ret_val = esp_spiffs_info(conf.partition_label, &total, &used);
    if (ret_val != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get SPIFFS partition information (%s)", esp_err_to_name(ret_val));
        esp_vfs_spiffs_unregister(conf.partition_label);
        return ret_val;
    } else {
        ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
    }
    inited = true;
    return ESP_OK;
}

esp_err_t bsp_spiffs_init_default(void) {
    return bsp_spiffs_init(DRV_BASE_PATH_FLASH, DRV_FS_MAX_FILES);
}

esp_err_t bsp_audio_init(const i2s_std_config_t *i2s_config) {
    esp_err_t ret = ESP_FAIL;
    if (i2s_tx_chan && i2s_rx_chan) {
        /* Audio was initialized before */
        return ESP_OK;
    }

    /* Setup I2S peripheral */
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(BSP_AUDIO_I2S_NUM, I2S_ROLE_MASTER);
    chan_cfg.auto_clear = true; // Auto clear the legacy data in the DMA buffer
    chan_cfg.intr_priority = 4;
    /* Keep enough queued audio to absorb scheduler jitter without exhausting
     * internal DMA-capable RAM. dma_frame_num is a frame count, not bytes, and
     * duplex mode allocates a separate descriptor/buffer set for TX and RX.
     */
    chan_cfg.dma_desc_num = CONFIG_BSP_AUDIO_DMA_BUFFER_NUM;
    chan_cfg.dma_frame_num = CONFIG_BSP_AUDIO_DMA_BUFFER_SIZE;
    BSP_ERROR_CHECK_RETURN_ERR(i2s_new_channel(&chan_cfg, &i2s_tx_chan, &i2s_rx_chan));
    {
        const i2s_event_callbacks_t callbacks = {
            .on_sent = bsp_i2s_tx_sent_callback,
        };
        __atomic_store_n(&i2s_tx_done_count, 0U, __ATOMIC_RELAXED);
        BSP_ERROR_CHECK_RETURN_ERR(i2s_channel_register_event_callback(i2s_tx_chan, &callbacks, NULL));
    }

    /* Setup I2S channels */
    i2s_std_config_t std_cfg_default = BSP_I2S_DUPLEX_MONO_CFG(DRV_AUDIO_SAMPLE_RATE);
    i2s_std_config_t *p_i2s_cfg = &std_cfg_default;
    if (i2s_config != NULL) {
        memcpy(p_i2s_cfg, i2s_config, sizeof(i2s_std_config_t));
    }

    if (i2s_tx_chan != NULL) {
        ESP_GOTO_ON_ERROR(i2s_channel_init_std_mode(i2s_tx_chan, p_i2s_cfg), err, TAG,
                          "I2S channel initialization failed");
    }
    if (i2s_rx_chan != NULL) {
        p_i2s_cfg->slot_cfg.slot_mask = I2S_STD_SLOT_RIGHT;
        ESP_GOTO_ON_ERROR(i2s_channel_init_std_mode(i2s_rx_chan, p_i2s_cfg), err, TAG,
                          "I2S channel initialization failed");
    }

    audio_codec_i2s_cfg_t i2s_cfg = {
        .port = BSP_AUDIO_I2S_NUM,
        .rx_handle = i2s_rx_chan,
        .tx_handle = i2s_tx_chan,
        .keep_slot_config = true,
    };
    i2s_data_if = audio_codec_new_i2s_data(&i2s_cfg);
    BSP_NULL_CHECK_GOTO(i2s_data_if, err);

    return ESP_OK;

err:
    if (i2s_tx_chan) {
        i2s_del_channel(i2s_tx_chan);
        i2s_tx_chan = NULL;
    }
    if (i2s_rx_chan) {
        i2s_del_channel(i2s_rx_chan);
        i2s_rx_chan = NULL;
    }
    i2s_data_if = NULL;

    return ret;
}

const audio_codec_data_if_t *bsp_audio_get_codec_itf(void) {
    return i2s_data_if;
}

esp_codec_dev_handle_t bsp_audio_codec_speaker_init(void) {
    const audio_codec_data_if_t *i2s_data_if = bsp_audio_get_codec_itf();
    esp_codec_dev_handle_t dev_handle = NULL;

    if (play_dev_handle != NULL) {
        return play_dev_handle;
    }

    if (i2s_data_if == NULL) {
        esp_err_t ret;
        /* Initilize I2C */
        ret = bsp_i2c_bus_init();
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "speaker init: i2c init failed: %s", esp_err_to_name(ret));
            return NULL;
        }
        /* Configure I2S peripheral and Power Amplifier */
        ret = bsp_audio_init(NULL);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "speaker init: audio init failed: %s", esp_err_to_name(ret));
            return NULL;
        }
        i2s_data_if = bsp_audio_get_codec_itf();
    }
    assert(i2s_data_if);

    const audio_codec_gpio_if_t *gpio_if = audio_codec_new_gpio();
    BSP_NULL_CHECK(gpio_if, NULL);

    audio_codec_i2c_cfg_t i2c_cfg = {
        .port = BSP_GENERAL_I2C_NUM,
        .addr = DRV_ES8311_I2C_ADDR << 1,
        .bus_handle = s_bus_ctx.bus_handle,
        .scl_speed_hz = BSP_GENERAL_I2C_CLK,
    };
    const audio_codec_ctrl_if_t *i2c_ctrl_if = audio_codec_new_i2c_ctrl(&i2c_cfg);
    if (i2c_ctrl_if == NULL) {
        audio_codec_delete_gpio_if(gpio_if);
        return NULL;
    }

    esp_codec_dev_hw_gain_t gain = {
        .pa_voltage = 5.0,
        .codec_dac_voltage = 3.3,
    };

    es8311_codec_cfg_t es8311_cfg = {
        .ctrl_if = i2c_ctrl_if,
        .gpio_if = gpio_if,
        .codec_mode = ESP_CODEC_DEV_WORK_MODE_DAC,
        .pa_pin = GPIO_NUM_NC,
        .pa_reverted = false,
        .master_mode = false,
        .use_mclk = true,
        .digital_mic = false,
        .invert_mclk = false,
        .invert_sclk = false,
        .hw_gain = gain,
    };
    const audio_codec_if_t *es8311_dev = es8311_codec_new(&es8311_cfg);
    if (es8311_dev == NULL) {
        audio_codec_delete_ctrl_if(i2c_ctrl_if);
        audio_codec_delete_gpio_if(gpio_if);
        return NULL;
    }

    esp_codec_dev_cfg_t codec_dev_cfg = {
        .dev_type = ESP_CODEC_DEV_TYPE_OUT,
        .codec_if = es8311_dev,
        .data_if = i2s_data_if,
    };
    dev_handle = esp_codec_dev_new(&codec_dev_cfg);
    if (dev_handle == NULL) {
        audio_codec_delete_codec_if(es8311_dev);
        audio_codec_delete_ctrl_if(i2c_ctrl_if);
        audio_codec_delete_gpio_if(gpio_if);
        return NULL;
    }

    play_dev_handle = dev_handle;
    play_codec_if = es8311_dev;
    play_ctrl_if = i2c_ctrl_if;
    play_gpio_if = gpio_if;
    return dev_handle;
}

esp_codec_dev_handle_t bsp_audio_codec_microphone_init(void) {
    const audio_codec_data_if_t *i2s_data_if = bsp_audio_get_codec_itf();
    const audio_codec_if_t *es7243_dev = NULL;
    const audio_codec_ctrl_if_t *i2c_ctrl_if = NULL;
    esp_codec_dev_handle_t dev_handle = NULL;

    if (record_dev_handle != NULL) {
        return record_dev_handle;
    }

    if (i2s_data_if == NULL) {
        esp_err_t ret;
        /* Initilize I2C */
        ret = bsp_i2c_bus_init();
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "microphone init: i2c init failed: %s", esp_err_to_name(ret));
            return NULL;
        }
        /* Configure I2S peripheral and Power Amplifier */
        ret = bsp_audio_init(NULL);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "microphone init: audio init failed: %s", esp_err_to_name(ret));
            return NULL;
        }
        i2s_data_if = bsp_audio_get_codec_itf();
    }
    assert(i2s_data_if);

    if (bsp_i2c_check(DRV_ES7243_I2C_ADDR) == ESP_OK) {
        audio_codec_i2c_cfg_t i2c_cfg = {
            .port = BSP_GENERAL_I2C_NUM,
            .addr = DRV_ES7243_I2C_ADDR << 1,
            .bus_handle = s_bus_ctx.bus_handle,
            .scl_speed_hz = BSP_GENERAL_I2C_CLK,
        };

        i2c_ctrl_if = audio_codec_new_i2c_ctrl(&i2c_cfg);
        BSP_NULL_CHECK(i2c_ctrl_if, NULL);

        es7243_codec_cfg_t es7243_cfg = {
            .ctrl_if = i2c_ctrl_if,
        };
        es7243_dev = es7243_codec_new(&es7243_cfg);
    } else {
        audio_codec_i2c_cfg_t i2c_cfg = {
            .port = BSP_GENERAL_I2C_NUM,
            .addr = DRV_ES7243E_I2C_ADDR << 1,
            .bus_handle = s_bus_ctx.bus_handle,
            .scl_speed_hz = BSP_GENERAL_I2C_CLK,
        };
        i2c_ctrl_if = audio_codec_new_i2c_ctrl(&i2c_cfg);
        BSP_NULL_CHECK(i2c_ctrl_if, NULL);
        es7243e_codec_cfg_t es7243e_cfg = {
            .ctrl_if = i2c_ctrl_if,
        };
        es7243_dev = es7243e_codec_new(&es7243e_cfg);
    }

    if (es7243_dev == NULL) {
        audio_codec_delete_ctrl_if(i2c_ctrl_if);
        return NULL;
    }

    esp_codec_dev_cfg_t codec_es7243_dev_cfg = {
        .dev_type = ESP_CODEC_DEV_TYPE_IN,
        .codec_if = es7243_dev,
        .data_if = i2s_data_if,
    };

    dev_handle = esp_codec_dev_new(&codec_es7243_dev_cfg);
    if (dev_handle == NULL) {
        audio_codec_delete_codec_if(es7243_dev);
        audio_codec_delete_ctrl_if(i2c_ctrl_if);
        return NULL;
    }

    record_dev_handle = dev_handle;
    record_codec_if = es7243_dev;
    record_ctrl_if = i2c_ctrl_if;
    return dev_handle;
}

esp_err_t bsp_i2s_read(void *audio_buffer, size_t len, size_t *bytes_read, uint32_t timeout_ms) {
    esp_err_t ret = ESP_OK;
    xSemaphoreTake(codec_mutex, portMAX_DELAY);
    ret = esp_codec_dev_read(record_dev_handle, audio_buffer, len);
    xSemaphoreGive(codec_mutex);
    *bytes_read = len;
#if CONFIG_BSP_AUDIO_MIC_VALUE_GAIN > 0
    uint16_t *buffer = (uint16_t *)audio_buffer;
    for (size_t i = 0; i < len / 2; i++) {
        buffer[i] = buffer[i] << CONFIG_BSP_AUDIO_MIC_VALUE_GAIN;
    }
#endif
    return ret;
}

esp_err_t bsp_i2s_write(void *audio_buffer, size_t len, size_t *bytes_written, uint32_t timeout_ms) {
    esp_err_t ret = ESP_OK;
    xSemaphoreTake(codec_mutex, portMAX_DELAY);
    ret = esp_codec_dev_write(play_dev_handle, audio_buffer, len);
    xSemaphoreGive(codec_mutex);
    *bytes_written = len;
    return ret;
}

esp_err_t bsp_i2s_set_tx_reference_callback(bsp_i2s_tx_reference_cb_t callback, void *ctx) {
    if (callback != NULL && ctx == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    /* Serialize callback execution with replacement/removal. Once disabling
     * returns, no ISR still owns the old context, so teardown may free it. */
    portENTER_CRITICAL(&i2s_tx_reference_lock);
    i2s_tx_reference_ctx = callback != NULL ? ctx : NULL;
    i2s_tx_reference_cb = callback;
    portEXIT_CRITICAL(&i2s_tx_reference_lock);
    return ESP_OK;
}

esp_err_t bsp_i2s_wait_tx_drain(uint32_t timeout_ms) {
    const uint32_t required_completions = (uint32_t)CONFIG_BSP_AUDIO_DMA_BUFFER_NUM;
    const uint32_t started_count = __atomic_load_n(&i2s_tx_done_count, __ATOMIC_RELAXED);
    const int64_t deadline_us = esp_timer_get_time() + ((int64_t)timeout_ms * 1000LL);

    if (i2s_tx_chan == NULL || required_completions == 0U) {
        return ESP_ERR_INVALID_STATE;
    }

    while ((uint32_t)(__atomic_load_n(&i2s_tx_done_count, __ATOMIC_RELAXED) - started_count) < required_completions) {
        if (esp_timer_get_time() >= deadline_us) {
            return ESP_ERR_TIMEOUT;
        }
        vTaskDelay(pdMS_TO_TICKS(2));
    }
    return ESP_OK;
}

esp_err_t bsp_codec_set_fs(uint32_t rate, uint32_t bits_cfg, i2s_slot_mode_t ch) {
    esp_err_t ret = ESP_OK;

    esp_codec_dev_sample_info_t fs = {
        .sample_rate = rate,
        .channel = ch,
        .bits_per_sample = bits_cfg,
    };

    xSemaphoreTake(codec_mutex, portMAX_DELAY);
    if (play_dev_handle) {
        ret = bsp_codec_close_play_locked();
    }
    if (record_dev_handle) {
        esp_err_t close_ret = bsp_codec_close_record_locked();
        if (ret == ESP_OK) {
            ret = close_ret;
        }
        esp_err_t gain_ret = esp_codec_dev_set_in_gain(record_dev_handle, DRV_AUDIO_MIC_GAIN);
        if (ret == ESP_OK) {
            ret = gain_ret;
        }
    }

    if (ret == ESP_OK && play_dev_handle) {
        ret = esp_codec_dev_open(play_dev_handle, &fs);
        if (ret == ESP_OK) {
            play_dev_open = true;
        }
    }
    if (ret == ESP_OK && record_dev_handle) {
        fs.channel = 2;
        fs.channel_mask = ESP_CODEC_DEV_MAKE_CHANNEL_MASK(1);
        ret = esp_codec_dev_open(record_dev_handle, &fs);
        if (ret == ESP_OK) {
            record_dev_open = true;
        }
    }
    xSemaphoreGive(codec_mutex);
    return ret;
}

esp_err_t bsp_codec_volume_set(int volume, int *volume_set) {
    esp_err_t ret = ESP_OK;
    float v = volume;
    if (volume < 0) {
        v = 0;
    }
    if (volume > 95) // Note: restrict max volume to 95 to avoid audio distortion
    {
        v = 95;
    }
    xSemaphoreTake(codec_mutex, portMAX_DELAY);
    ret = esp_codec_dev_set_out_vol(play_dev_handle, (int)v);
    xSemaphoreGive(codec_mutex);
    return ret;
}

esp_err_t bsp_codec_mute_set(bool enable) {
    esp_err_t ret = ESP_OK;
    xSemaphoreTake(codec_mutex, portMAX_DELAY);
    ret = esp_codec_dev_set_out_mute(play_dev_handle, enable);
    xSemaphoreGive(codec_mutex);
    return ret;
}

esp_err_t bsp_codec_dev_stop(void) {
    esp_err_t ret = ESP_OK;

    xSemaphoreTake(codec_mutex, portMAX_DELAY);

    if (play_dev_handle) {
        ret = bsp_codec_close_play_locked();
    }

    if (record_dev_handle) {
        esp_err_t close_ret = bsp_codec_close_record_locked();
        if (ret == ESP_OK) {
            ret = close_ret;
        }
    }
    xSemaphoreGive(codec_mutex);
    return ret;
}

esp_err_t bsp_codec_dev_resume(void) {
    return bsp_codec_set_fs(DRV_AUDIO_SAMPLE_RATE, DRV_AUDIO_SAMPLE_BITS, DRV_AUDIO_CHANNELS);
}

esp_err_t bsp_codec_init(void) {
    if (play_dev_handle != NULL && record_dev_handle != NULL) {
        return ESP_OK;
    }

    if (codec_mutex == NULL) {
        codec_mutex = xSemaphoreCreateMutex();
        if (codec_mutex == NULL) {
            return ESP_ERR_NO_MEM;
        }
    }

    play_dev_handle = bsp_audio_codec_speaker_init();
    if (play_dev_handle == NULL) {
        (void)bsp_codec_deinit();
        return ESP_FAIL;
    }

    record_dev_handle = bsp_audio_codec_microphone_init();
    if (record_dev_handle == NULL) {
        (void)bsp_codec_deinit();
        return ESP_FAIL;
    }

    esp_err_t ret = bsp_codec_set_fs(DRV_AUDIO_SAMPLE_RATE, DRV_AUDIO_SAMPLE_BITS, DRV_AUDIO_CHANNELS);
    if (ret != ESP_OK) {
        (void)bsp_codec_deinit();
        return ret;
    }
    return ESP_OK;
}

esp_err_t bsp_codec_deinit(void) {
    esp_err_t status = ESP_OK;

    if (codec_mutex != NULL) {
        xSemaphoreTake(codec_mutex, portMAX_DELAY);
    }

    if (play_dev_handle != NULL) {
        if (bsp_codec_close_play_locked() != ESP_OK) {
            status = ESP_FAIL;
        }
        esp_codec_dev_delete(play_dev_handle);
        play_dev_handle = NULL;
    }
    if (record_dev_handle != NULL) {
        if (bsp_codec_close_record_locked() != ESP_OK) {
            status = ESP_FAIL;
        }
        esp_codec_dev_delete(record_dev_handle);
        record_dev_handle = NULL;
    }

    if (play_codec_if != NULL && audio_codec_delete_codec_if(play_codec_if) != ESP_CODEC_DEV_OK) {
        status = ESP_FAIL;
    }
    play_codec_if = NULL;
    if (record_codec_if != NULL && audio_codec_delete_codec_if(record_codec_if) != ESP_CODEC_DEV_OK) {
        status = ESP_FAIL;
    }
    record_codec_if = NULL;
    if (play_ctrl_if != NULL && audio_codec_delete_ctrl_if(play_ctrl_if) != ESP_CODEC_DEV_OK) {
        status = ESP_FAIL;
    }
    play_ctrl_if = NULL;
    if (record_ctrl_if != NULL && audio_codec_delete_ctrl_if(record_ctrl_if) != ESP_CODEC_DEV_OK) {
        status = ESP_FAIL;
    }
    record_ctrl_if = NULL;
    if (play_gpio_if != NULL && audio_codec_delete_gpio_if(play_gpio_if) != ESP_CODEC_DEV_OK) {
        status = ESP_FAIL;
    }
    play_gpio_if = NULL;
    if (i2s_data_if != NULL && audio_codec_delete_data_if(i2s_data_if) != ESP_CODEC_DEV_OK) {
        status = ESP_FAIL;
    }
    i2s_data_if = NULL;

    if (play_dev_open && i2s_tx_chan != NULL) {
        esp_err_t ret = i2s_channel_disable(i2s_tx_chan);
        if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
            status = ESP_FAIL;
        }
        play_dev_open = false;
    }
    if (i2s_tx_chan != NULL) {
        esp_err_t ret;
        ret = i2s_del_channel(i2s_tx_chan);
        if (ret != ESP_OK) {
            status = ESP_FAIL;
        }
        i2s_tx_chan = NULL;
    }
    if (record_dev_open && i2s_rx_chan != NULL) {
        esp_err_t ret = i2s_channel_disable(i2s_rx_chan);
        if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
            status = ESP_FAIL;
        }
        record_dev_open = false;
    }
    if (i2s_rx_chan != NULL) {
        esp_err_t ret;
        ret = i2s_del_channel(i2s_rx_chan);
        if (ret != ESP_OK) {
            status = ESP_FAIL;
        }
        i2s_rx_chan = NULL;
    }

    if (codec_mutex != NULL) {
        xSemaphoreGive(codec_mutex);
        vSemaphoreDelete(codec_mutex);
        codec_mutex = NULL;
    }

    return status;
}

esp_codec_dev_handle_t bsp_codec_speaker_get(void) {
    return play_dev_handle;
}
esp_codec_dev_handle_t bsp_codec_microphone_get(void) {
    return record_dev_handle;
}

esp_err_t bsp_get_feed_data(bool is_get_raw_channel, int16_t *buffer, int buffer_len) {
    esp_err_t ret = ESP_OK;

    xSemaphoreTake(codec_mutex, portMAX_DELAY);
    ret = esp_codec_dev_read(record_dev_handle, (void *)buffer, buffer_len);
    xSemaphoreGive(codec_mutex);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read data from codec device");
    }
    // int audio_chunksize = buffer_len / (sizeof(int16_t) * DRV_AUDIO_I2S_CHANNEL);
    // if (!is_get_raw_channel)
    // {
    //     for (int i = 0; i < audio_chunksize; i++)
    //     {
    //         buffer[i] = buffer[i] << 2;
    //     }
    // }
    return ret;
}

int bsp_get_feed_channel(void) {
    return DRV_AUDIO_I2S_CHANNEL;
}

static esp_err_t bsp_sscma_prepare_sd_cs(void) {
    /* Mounted SDSPI owns this pin. Reconfiguring it can interrupt an animation
     * read even when subsequent camera allocation fails. */
    if (card != NULL) {
        return ESP_OK;
    }
    const gpio_config_t io_config = {
        .pin_bit_mask = (1ULL << BSP_SD_SPI_CS),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    esp_err_t ret = gpio_config(&io_config);
    if (ret != ESP_OK) {
        return ret;
    }
    return gpio_set_level(BSP_SD_SPI_CS, 1);
}

sscma_client_handle_t bsp_sscma_client_init() {
    if (sscma_client_initialized && sscma_client_handle != NULL)
        return sscma_client_handle;

    if (sscma_client_io_handle == NULL) {
        if (bsp_io_expander_init() == NULL)
            return NULL;

        if (bsp_exp_io_set_level(BSP_PWR_AI_CHIP, 1) != ESP_OK)
            return NULL;
        vTaskDelay(pdMS_TO_TICKS(20));

        if (bsp_spi_bus_init() != ESP_OK)
            return NULL;

        esp_err_t ret = bsp_sscma_prepare_sd_cs();
        if (ret != ESP_OK)
            return NULL;

        const sscma_client_io_spi_config_t spi_io_config = {
            .sync_gpio_num = BSP_SSCMA_CLIENT_SPI_SYNC,
            .cs_gpio_num = BSP_SSCMA_CLIENT_SPI_CS,
            .pclk_hz = BSP_SSCMA_CLIENT_SPI_CLK,
            .spi_mode = 0,
            .wait_delay = 2,
            .user_ctx = NULL,
            .io_expander = io_exp_handle,
            .flags.sync_use_expander = BSP_SSCMA_CLIENT_RST_USE_EXPANDER,
        };

        ret = sscma_client_new_io_spi_bus((sscma_client_spi_bus_handle_t)BSP_SSCMA_CLIENT_SPI_NUM, &spi_io_config,
                                          &sscma_client_io_handle);
        if (ret != ESP_OK)
            return NULL;
    }

    sscma_client_config_t sscma_client_config = SSCMA_CLIENT_CONFIG_DEFAULT();

    sscma_client_config.event_queue_size = CONFIG_SSCMA_EVENT_QUEUE_SIZE;
    sscma_client_config.tx_buffer_size = CONFIG_SSCMA_TX_BUFFER_SIZE;
    sscma_client_config.rx_buffer_size = CONFIG_SSCMA_RX_BUFFER_SIZE;
    sscma_client_config.process_task_stack = CONFIG_SSCMA_PROCESS_TASK_STACK_SIZE;
    sscma_client_config.process_task_affinity = CONFIG_SSCMA_PROCESS_TASK_AFFINITY;
    sscma_client_config.process_task_priority = CONFIG_SSCMA_PROCESS_TASK_PRIORITY;
    sscma_client_config.monitor_task_stack = CONFIG_SSCMA_MONITOR_TASK_STACK_SIZE;
    sscma_client_config.monitor_task_affinity = CONFIG_SSCMA_MONITOR_TASK_AFFINITY;
    sscma_client_config.monitor_task_priority = CONFIG_SSCMA_MONITOR_TASK_PRIORITY;
    sscma_client_config.reset_gpio_num = BSP_SSCMA_CLIENT_RST;
    sscma_client_config.io_expander = io_exp_handle;
    sscma_client_config.flags.reset_use_expander = BSP_SSCMA_CLIENT_RST_USE_EXPANDER;

    if (sscma_client_new(sscma_client_io_handle, &sscma_client_config, &sscma_client_handle) != ESP_OK)
        return NULL;

    sscma_client_initialized = true;

    return sscma_client_handle;
}

esp_err_t bsp_sscma_client_deinit(void) {
    esp_err_t ret;

    if (sscma_client_handle == NULL) {
        sscma_client_initialized = false;
        return ESP_OK;
    }

    ret = sscma_client_del(sscma_client_handle);
    if (ret == ESP_OK) {
        sscma_client_handle = NULL;
        sscma_client_initialized = false;
    }

    return ret;
}

sscma_client_flasher_handle_t bsp_sscma_flasher_init() {
    static bool initialized = false;
    if (initialized)
        return sscma_flasher_handle;

    if (bsp_io_expander_init() == NULL)
        return NULL;

    //     uart_config_t uart_config = {
    //         .baud_rate = BSP_SSCMA_FLASHER_UART_BAUD_RATE,
    //         .data_bits = UART_DATA_8_BITS,
    //         .parity = UART_PARITY_DISABLE,
    //         .stop_bits = UART_STOP_BITS_1,
    //         .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    //         .source_clk = UART_SCLK_DEFAULT,
    //     };
    //     int intr_alloc_flags = 0;

    // #if CONFIG_UART_ISR_IN_IRAM
    //     intr_alloc_flags = ESP_INTR_FLAG_IRAM;
    // #endif

    //     ESP_ERROR_CHECK(uart_driver_install(BSP_SSCMA_FLASHER_UART_NUM, 64 * 1024, 0, 0, NULL, intr_alloc_flags));
    //     ESP_ERROR_CHECK(uart_param_config(BSP_SSCMA_FLASHER_UART_NUM, &uart_config));
    //     ESP_ERROR_CHECK(uart_set_pin(BSP_SSCMA_FLASHER_UART_NUM, BSP_SSCMA_FLASHER_UART_TX, BSP_SSCMA_FLASHER_UART_RX, -1, -1));

    //     sscma_client_io_uart_config_t io_uart_config = {
    //         .user_ctx = NULL,
    //     };

    //     sscma_client_new_io_uart_bus((sscma_client_uart_bus_handle_t)BSP_SSCMA_FLASHER_UART_NUM, &io_uart_config, &sscma_flasher_io_handle);
    (void)sscma_flasher_io_handle;

    const sscma_client_flasher_we2_config_t flasher_config = {
        .reset_gpio_num = BSP_SSCMA_CLIENT_RST,
        .io_expander = io_exp_handle,
        .flags.reset_use_expander = BSP_SSCMA_CLIENT_RST_USE_EXPANDER,
        .flags.reset_high_active = false,
        .user_ctx = NULL,
    };

    const esp_err_t result =
        sscma_client_new_flasher_we2_spi(sscma_client_io_handle, &flasher_config, &sscma_flasher_handle);
    if (result != ESP_OK) {
        sscma_flasher_handle = NULL;
        return NULL;
    }

    initialized = sscma_flasher_handle != NULL;

    return sscma_flasher_handle;
}
