/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_lvgl_port.h"
#include "esp_check.h"
#include "esp_err.h"
#include "esp_heap_caps.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#include "watcher_input_router.h"

#include "lvgl.h"

#include <stdlib.h>
#include <string.h>

#ifdef ESP_LVGL_PORT_TOUCH_COMPONENT
#include "esp_lcd_touch.h"
#endif

#ifdef ESP_LVGL_PORT_USB_HOST_HID_COMPONENT
#include "usb/hid_host.h"
#include "usb/hid_usage_keyboard.h"
#include "usb/hid_usage_mouse.h"
/* LVGL image of cursor */
LV_IMG_DECLARE(img_cursor)
#endif

#if (ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(4, 4, 4)) || (ESP_IDF_VERSION == ESP_IDF_VERSION_VAL(5, 0, 0))
#define LVGL_PORT_HANDLE_FLUSH_READY 0
#else
#define LVGL_PORT_HANDLE_FLUSH_READY 1
#endif

static const char *TAG = "LVGL";

/*******************************************************************************
* Types definitions
*******************************************************************************/

#ifdef ESP_LVGL_PORT_USB_HOST_HID_COMPONENT
typedef struct {
    QueueHandle_t queue; /* USB HID queue */
    TaskHandle_t task;   /* USB HID task */
    bool running;        /* USB HID task running */
    struct {
        lv_indev_drv_t drv;  /* LVGL mouse input device driver */
        uint8_t sensitivity; /* Mouse sensitivity (cannot be zero) */
        int16_t x;           /* Mouse X coordinate */
        int16_t y;           /* Mouse Y coordinate */
        bool left_button;    /* Mouse left button state */
    } mouse;
    struct {
        lv_indev_drv_t drv; /* LVGL keyboard input device driver */
        uint32_t last_key;
        bool pressed;
    } kb;
} lvgl_port_usb_hid_ctx_t;

typedef struct {
    hid_host_device_handle_t hid_device_handle;
    hid_host_driver_event_t event;
    void *arg;
} lvgl_port_usb_hid_event_t;

#endif

typedef struct lvgl_port_ctx_s {
    SemaphoreHandle_t lvgl_mux;
    SemaphoreHandle_t panel_mux;
    SemaphoreHandle_t panel_trans_done;
    esp_timer_handle_t tick_timer;
    TaskHandle_t task_handle;
#if CONFIG_LVGL_PORT_TASK_STACK_ALLOC_EXTERNAL
    StaticTask_t *task_tcb;
    StackType_t *task_stack;
    size_t task_stack_words;
#endif
    bool running;
    volatile bool panel_direct_draw_pending;
    int task_max_sleep_ms;
#ifdef ESP_LVGL_PORT_USB_HOST_HID_COMPONENT
    lvgl_port_usb_hid_ctx_t hid_ctx;
#endif
} lvgl_port_ctx_t;

typedef struct {
    esp_lcd_panel_io_handle_t io_handle; /* LCD panel IO handle */
    esp_lcd_panel_handle_t panel_handle; /* LCD panel handle */
    lvgl_port_rotation_cfg_t rotation;   /* Default values of the screen rotation */
    lv_disp_drv_t disp_drv;              /* LVGL display driver */
    lv_color_t *trans_buffer;            /* Display-owned internal DMA staging; never per-frame allocated */
    size_t trans_buffer_size;            /* Capacity in pixels */
} lvgl_port_display_ctx_t;

#ifdef ESP_LVGL_PORT_TOUCH_COMPONENT
typedef struct {
    esp_lcd_touch_handle_t handle; /* LCD touch IO handle */
    lv_indev_drv_t indev_drv;      /* LVGL input device driver */
    int16_t sensitivity;           /* Touch sensitivity (0 - 255) */
} lvgl_port_touch_ctx_t;
#endif

#ifdef ESP_LVGL_PORT_KNOB_COMPONENT
typedef struct {
    knob_handle_t knob_handle;  /* Encoder knob handlers */
    button_handle_t btn_handle; /* Encoder button handlers */
    lv_indev_drv_t indev_drv;   /* LVGL input device driver */
    lv_indev_t *indev;          /* Registered LVGL input device */
    bool read_rotating;         /* Rotation in the currently processed LVGL sample */
} lvgl_port_encoder_ctx_t;
#endif

#ifdef ESP_LVGL_PORT_BUTTON_COMPONENT

typedef enum {
    LVGL_PORT_NAV_BTN_PREV,
    LVGL_PORT_NAV_BTN_NEXT,
    LVGL_PORT_NAV_BTN_ENTER,
    LVGL_PORT_NAV_BTN_CNT,
} lvgl_port_nav_btns_t;

typedef struct {
    button_handle_t btn[LVGL_PORT_NAV_BTN_CNT]; /* Button handlers */
    lv_indev_drv_t indev_drv;                   /* LVGL input device driver */
    bool btn_prev;                              /* Button prev state */
    bool btn_next;                              /* Button next state */
    bool btn_enter;                             /* Button enter state */
} lvgl_port_nav_btns_ctx_t;
#endif

/*******************************************************************************
* Local variables
*******************************************************************************/
static lvgl_port_ctx_t lvgl_port_ctx;
static int lvgl_port_timer_period_ms = 5;
#ifdef ESP_LVGL_PORT_TOUCH_COMPONENT
static bool s_touch_diag_active = false;
static bool s_touch_diag_empty_frame = false;
static uint16_t s_touch_diag_x = 0;
static uint16_t s_touch_diag_y = 0;
#endif

/*******************************************************************************
* Function definitions
*******************************************************************************/
static void lvgl_port_task(void *arg);
static esp_err_t lvgl_port_tick_init(void);
static esp_err_t lvgl_port_task_create(const lvgl_port_cfg_t *cfg);
static void lvgl_port_task_deinit(void);
static esp_err_t lvgl_port_panel_draw_bitmap_locked(esp_lcd_panel_handle_t panel_handle, int x_start, int y_start,
                                                    int x_end, int y_end, const void *color_data, uint32_t timeout_ms);

// LVGL callbacks
#if LVGL_PORT_HANDLE_FLUSH_READY
static bool lvgl_port_flush_ready_callback(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *edata,
                                           void *user_ctx);
#endif
static void lvgl_port_flush_callback(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map);
static void lvgl_port_update_callback(lv_disp_drv_t *drv);
#ifdef ESP_LVGL_PORT_TOUCH_COMPONENT
static void lvgl_port_touchpad_read(lv_indev_drv_t *indev_drv, lv_indev_data_t *data);
#endif
#ifdef ESP_LVGL_PORT_KNOB_COMPONENT
static void lvgl_port_encoder_read(lv_indev_drv_t *indev_drv, lv_indev_data_t *data);
static void lvgl_port_encoder_btn_down_handler(void *arg, void *arg2);
static void lvgl_port_encoder_btn_up_handler(void *arg, void *arg2);
#endif
#ifdef ESP_LVGL_PORT_BUTTON_COMPONENT
static void lvgl_port_navigation_buttons_read(lv_indev_drv_t *indev_drv, lv_indev_data_t *data);
static void lvgl_port_btn_down_handler(void *arg, void *arg2);
static void lvgl_port_btn_up_handler(void *arg, void *arg2);
#endif
#ifdef ESP_LVGL_PORT_USB_HOST_HID_COMPONENT
static lvgl_port_usb_hid_ctx_t *lvgl_port_hid_init(void);
static void lvgl_port_usb_hid_task(void *arg);
static void lvgl_port_usb_hid_read_mouse(lv_indev_drv_t *indev_drv, lv_indev_data_t *data);
static void lvgl_port_usb_hid_read_kb(lv_indev_drv_t *indev_drv, lv_indev_data_t *data);
static void lvgl_port_usb_hid_callback(hid_host_device_handle_t hid_device_handle, const hid_host_driver_event_t event,
                                       void *arg);
#endif
static void lvgl_port_pix_monochrome_callback(lv_disp_drv_t *drv, uint8_t *buf, lv_coord_t buf_w, lv_coord_t x,
                                              lv_coord_t y, lv_color_t color, lv_opa_t opa);
/*******************************************************************************
* Public API functions
*******************************************************************************/

esp_err_t lvgl_port_init(const lvgl_port_cfg_t *cfg) {
    esp_err_t ret = ESP_OK;
#if CONFIG_LVGL_PORT_TASK_STACK_ALLOC_EXTERNAL
    StaticTask_t *task_tcb = lvgl_port_ctx.task_tcb;
    StackType_t *task_stack = lvgl_port_ctx.task_stack;
    size_t task_stack_words = lvgl_port_ctx.task_stack_words;
#endif

    ESP_GOTO_ON_FALSE(cfg, ESP_ERR_INVALID_ARG, err, TAG, "invalid argument");
    ESP_GOTO_ON_FALSE(cfg->task_affinity < (configNUM_CORES), ESP_ERR_INVALID_ARG, err, TAG,
                      "Bad core number for task! Maximum core number is %d", (configNUM_CORES - 1));

    memset(&lvgl_port_ctx, 0, sizeof(lvgl_port_ctx));
#if CONFIG_LVGL_PORT_TASK_STACK_ALLOC_EXTERNAL
    lvgl_port_ctx.task_tcb = task_tcb;
    lvgl_port_ctx.task_stack = task_stack;
    lvgl_port_ctx.task_stack_words = task_stack_words;
#endif

    /* LVGL init */
    lv_init();
    /* Tick init */
    lvgl_port_timer_period_ms = cfg->timer_period_ms;
    ESP_RETURN_ON_ERROR(lvgl_port_tick_init(), TAG, "");
    /* Create task */
    lvgl_port_ctx.task_max_sleep_ms = cfg->task_max_sleep_ms;
    if (lvgl_port_ctx.task_max_sleep_ms == 0) {
        lvgl_port_ctx.task_max_sleep_ms = 500;
    }
    lvgl_port_ctx.lvgl_mux = xSemaphoreCreateRecursiveMutex();
    ESP_GOTO_ON_FALSE(lvgl_port_ctx.lvgl_mux, ESP_ERR_NO_MEM, err, TAG, "Create LVGL mutex fail!");
    lvgl_port_ctx.panel_mux = xSemaphoreCreateMutex();
    ESP_GOTO_ON_FALSE(lvgl_port_ctx.panel_mux, ESP_ERR_NO_MEM, err, TAG, "Create LCD panel mutex fail!");
    lvgl_port_ctx.panel_trans_done = xSemaphoreCreateBinary();
    ESP_GOTO_ON_FALSE(lvgl_port_ctx.panel_trans_done, ESP_ERR_NO_MEM, err, TAG,
                      "Create LCD panel transfer semaphore fail!");

    ESP_GOTO_ON_ERROR(lvgl_port_task_create(cfg), err, TAG, "Create LVGL task fail!");

err:
    if (ret != ESP_OK) {
        lvgl_port_deinit();
    }

    return ret;
}

esp_err_t lvgl_port_resume(void) {
    esp_err_t ret = ESP_ERR_INVALID_STATE;

    if (lvgl_port_ctx.tick_timer != NULL) {
        lv_timer_enable(true);
        ret = esp_timer_start_periodic(lvgl_port_ctx.tick_timer, lvgl_port_timer_period_ms * 1000);
    }

    return ret;
}

esp_err_t lvgl_port_stop(void) {
    esp_err_t ret = ESP_ERR_INVALID_STATE;

    if (lvgl_port_ctx.tick_timer != NULL) {
        lv_timer_enable(false);
        ret = esp_timer_stop(lvgl_port_ctx.tick_timer);
    }

    return ret;
}

static volatile bool task_stopped;
esp_err_t lvgl_port_quiesce(void) {
    if (lvgl_port_ctx.task_handle == xTaskGetCurrentTaskHandle())
        return ESP_ERR_INVALID_STATE;
    /* Stop and delete timer */
    if (lvgl_port_ctx.tick_timer != NULL) {
        esp_timer_stop(lvgl_port_ctx.tick_timer);
        esp_timer_delete(lvgl_port_ctx.tick_timer);
        lvgl_port_ctx.tick_timer = NULL;
    }

    lvgl_port_ctx.running = false;
    if (lvgl_port_ctx.task_handle != NULL) {
        TickType_t start = xTaskGetTickCount();
        while (!task_stopped) {
            if (xTaskGetTickCount() - start > pdMS_TO_TICKS(2000))
                return ESP_ERR_TIMEOUT;
            vTaskDelay(1);
        }
        vTaskDelete(lvgl_port_ctx.task_handle);
        lvgl_port_ctx.task_handle = NULL;
    }
    return ESP_OK;
}
esp_err_t lvgl_port_deinit(void) {
    esp_err_t ret = lvgl_port_quiesce();
    if (ret == ESP_OK)
        lvgl_port_task_deinit();
    return ret;
}

lv_disp_t *lvgl_port_add_disp(const lvgl_port_display_cfg_t *disp_cfg) {
    esp_err_t ret = ESP_OK;
    lv_disp_t *disp = NULL;
    lv_color_t *buf1 = NULL;
    lv_color_t *buf2 = NULL;
    assert(disp_cfg != NULL);
    assert(disp_cfg->io_handle != NULL);
    assert(disp_cfg->panel_handle != NULL);
    assert(disp_cfg->buffer_size > 0);
    assert(disp_cfg->hres > 0);
    assert(disp_cfg->vres > 0);

    /* Display context */
    lvgl_port_display_ctx_t *disp_ctx = calloc(1, sizeof(lvgl_port_display_ctx_t));
    ESP_GOTO_ON_FALSE(disp_ctx, ESP_ERR_NO_MEM, err, TAG, "Not enough memory for display context allocation!");
    disp_ctx->io_handle = disp_cfg->io_handle;
    disp_ctx->panel_handle = disp_cfg->panel_handle;
    disp_ctx->rotation.swap_xy = disp_cfg->rotation.swap_xy;
    disp_ctx->rotation.mirror_x = disp_cfg->rotation.mirror_x;
    disp_ctx->rotation.mirror_y = disp_cfg->rotation.mirror_y;

    if (disp_cfg->trans_buffer_size != 0U) {
        ESP_GOTO_ON_FALSE(!disp_cfg->monochrome && disp_cfg->trans_buffer_size >= disp_cfg->hres, ESP_ERR_INVALID_ARG,
                          err, TAG, "DMA staging must hold a color row");
        disp_ctx->trans_buffer =
            heap_caps_calloc(disp_cfg->trans_buffer_size, sizeof(lv_color_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
        ESP_GOTO_ON_FALSE(disp_ctx->trans_buffer, ESP_ERR_NO_MEM, err, TAG, "DMA staging allocation failed");
        disp_ctx->trans_buffer_size = disp_cfg->trans_buffer_size;
    }

    uint32_t buff_caps = MALLOC_CAP_DEFAULT;
    if (disp_cfg->flags.buff_dma && disp_cfg->flags.buff_spiram) {
        ESP_GOTO_ON_FALSE(false, ESP_ERR_NOT_SUPPORTED, err, TAG,
                          "Alloc DMA capable buffer in SPIRAM is not supported!");
    } else if (disp_cfg->flags.buff_dma) {
        buff_caps = MALLOC_CAP_DMA;
    } else if (disp_cfg->flags.buff_spiram) {
        buff_caps = MALLOC_CAP_SPIRAM;
    }

    /* alloc draw buffers used by LVGL */
    /* it's recommended to choose the size of the draw buffer(s) to be at least 1/10 screen sized */
    buf1 = heap_caps_malloc(disp_cfg->buffer_size * sizeof(lv_color_t), buff_caps);
    ESP_GOTO_ON_FALSE(buf1, ESP_ERR_NO_MEM, err, TAG, "Not enough memory for LVGL buffer (buf1) allocation!");
    if (disp_cfg->double_buffer) {
        buf2 = heap_caps_malloc(disp_cfg->buffer_size * sizeof(lv_color_t), buff_caps);
        ESP_GOTO_ON_FALSE(buf2, ESP_ERR_NO_MEM, err, TAG, "Not enough memory for LVGL buffer (buf2) allocation!");
    }
    lv_disp_draw_buf_t *disp_buf = malloc(sizeof(lv_disp_draw_buf_t));
    ESP_GOTO_ON_FALSE(disp_buf, ESP_ERR_NO_MEM, err, TAG, "Not enough memory for LVGL display buffer allocation!");

    /* initialize LVGL draw buffers */
    lv_disp_draw_buf_init(disp_buf, buf1, buf2, disp_cfg->buffer_size);

    ESP_LOGD(TAG, "Register display driver to LVGL");
    lv_disp_drv_init(&disp_ctx->disp_drv);
    disp_ctx->disp_drv.hor_res = disp_cfg->hres;
    disp_ctx->disp_drv.ver_res = disp_cfg->vres;
    disp_ctx->disp_drv.flush_cb = lvgl_port_flush_callback;
    disp_ctx->disp_drv.drv_update_cb = lvgl_port_update_callback;
    disp_ctx->disp_drv.draw_buf = disp_buf;
    disp_ctx->disp_drv.user_data = disp_ctx;

#if LVGL_PORT_HANDLE_FLUSH_READY
    /* Register done callback */
    const esp_lcd_panel_io_callbacks_t cbs = {
        .on_color_trans_done = lvgl_port_flush_ready_callback,
    };
    esp_lcd_panel_io_register_event_callbacks(disp_ctx->io_handle, &cbs, &disp_ctx->disp_drv);
#endif

    /* Monochrome display settings */
    if (disp_cfg->monochrome) {
        /* When using monochromatic display, there must be used full bufer! */
        ESP_GOTO_ON_FALSE((disp_cfg->hres * disp_cfg->vres == disp_cfg->buffer_size), ESP_ERR_INVALID_ARG, err, TAG,
                          "Monochromatic display must using full buffer!");

        disp_ctx->disp_drv.full_refresh = 1;
        disp_ctx->disp_drv.set_px_cb = lvgl_port_pix_monochrome_callback;
    }

    disp = lv_disp_drv_register(&disp_ctx->disp_drv);

err:
    if (ret != ESP_OK) {
        if (buf1) {
            free(buf1);
        }
        if (buf2) {
            free(buf2);
        }
        if (disp_ctx) {
            free(disp_ctx->trans_buffer);
            free(disp_ctx);
        }
    }

    return disp;
}

esp_err_t lvgl_port_remove_disp(lv_disp_t *disp) {
    assert(disp);
    if (!lvgl_port_lock(0)) {
        return ESP_ERR_INVALID_STATE;
    }
    lv_disp_drv_t *disp_drv = disp->driver;
    assert(disp_drv);
    lvgl_port_display_ctx_t *disp_ctx = (lvgl_port_display_ctx_t *)disp_drv->user_data;

    /* A timed-out transfer still owns its source until the completion ISR.
     * Keep the display and its storage intact so the caller can retry. */
    if (lvgl_port_ctx.panel_direct_draw_pending || (disp_drv->draw_buf && disp_drv->draw_buf->flushing)) {
        lvgl_port_unlock();
        return ESP_ERR_INVALID_STATE;
    }

    /* Drain SPI transactions and detach the ISR before freeing its context. */
    esp_err_t ret = esp_lcd_panel_io_tx_param(disp_ctx->io_handle, -1, NULL, 0);
    if (ret != ESP_OK) {
        lvgl_port_unlock();
        return ret;
    }
    const esp_lcd_panel_io_callbacks_t callbacks = {0};
    ret = esp_lcd_panel_io_register_event_callbacks(disp_ctx->io_handle, &callbacks, NULL);
    if (ret != ESP_OK) {
        lvgl_port_unlock();
        return ret;
    }

    if (disp_drv) {
        if (disp_drv->draw_buf && disp_drv->draw_buf->buf1) {
            free(disp_drv->draw_buf->buf1);
            disp_drv->draw_buf->buf1 = NULL;
        }
        if (disp_drv->draw_buf && disp_drv->draw_buf->buf2) {
            free(disp_drv->draw_buf->buf2);
            disp_drv->draw_buf->buf2 = NULL;
        }
        if (disp_drv->draw_buf) {
            free(disp_drv->draw_buf);
            disp_drv->draw_buf = NULL;
        }
    }
    lv_disp_remove(disp);

    free(disp_ctx->trans_buffer);
    free(disp_ctx);
    lvgl_port_unlock();

    return ESP_OK;
}

#ifdef ESP_LVGL_PORT_TOUCH_COMPONENT
lv_indev_t *lvgl_port_add_touch(const lvgl_port_touch_cfg_t *touch_cfg) {
    assert(touch_cfg != NULL);
    assert(touch_cfg->disp != NULL);
    assert(touch_cfg->handle != NULL);

    /* Touch context */
    lvgl_port_touch_ctx_t *touch_ctx = malloc(sizeof(lvgl_port_touch_ctx_t));
    if (touch_ctx == NULL) {
        ESP_LOGE(TAG, "Not enough memory for touch context allocation!");
        return NULL;
    }
    touch_ctx->handle = touch_cfg->handle;
    touch_ctx->sensitivity = touch_cfg->sensitivity;

    /* Register a touchpad input device */
    lv_indev_drv_init(&touch_ctx->indev_drv);
    touch_ctx->indev_drv.type = LV_INDEV_TYPE_POINTER;
    touch_ctx->indev_drv.disp = touch_cfg->disp;
    touch_ctx->indev_drv.read_cb = lvgl_port_touchpad_read;
    touch_ctx->indev_drv.user_data = touch_ctx;
    lv_indev_t *indev = lv_indev_drv_register(&touch_ctx->indev_drv);
    if (indev == NULL)
        free(touch_ctx);
    return indev;
}

esp_err_t lvgl_port_remove_touch(lv_indev_t *touch) {
    assert(touch);
    lv_indev_drv_t *indev_drv = touch->driver;
    assert(indev_drv);
    lvgl_port_touch_ctx_t *touch_ctx = (lvgl_port_touch_ctx_t *)indev_drv->user_data;

    /* Remove input device driver */
    lv_indev_delete(touch);

    if (touch_ctx) {
        free(touch_ctx);
    }

    return ESP_OK;
}
#endif

#ifdef ESP_LVGL_PORT_KNOB_COMPONENT
lv_indev_t *lvgl_port_add_encoder(const lvgl_port_encoder_cfg_t *encoder_cfg) {
    esp_err_t ret = ESP_OK;
    lv_indev_t *indev = NULL;
    assert(encoder_cfg != NULL);
    assert(encoder_cfg->disp != NULL);

    /* Encoder context */
    lvgl_port_encoder_ctx_t *encoder_ctx = malloc(sizeof(lvgl_port_encoder_ctx_t));
    if (encoder_ctx == NULL) {
        ESP_LOGE(TAG, "Not enough memory for encoder context allocation!");
        return NULL;
    }

    encoder_ctx->read_rotating = false;
    /* Encoder_a/b */
    if (encoder_cfg->encoder_a_b != NULL) {
        encoder_ctx->knob_handle = iot_knob_create(encoder_cfg->encoder_a_b);
        ESP_GOTO_ON_FALSE(encoder_ctx->knob_handle, ESP_ERR_NO_MEM, err, TAG, "Not enough memory for knob create!");
    }

    /* Encoder Enter */
    if (encoder_cfg->encoder_enter != NULL) {
        encoder_ctx->btn_handle = iot_button_create(encoder_cfg->encoder_enter);
        ESP_GOTO_ON_FALSE(encoder_ctx->btn_handle, ESP_ERR_NO_MEM, err, TAG, "Not enough memory for button create!");
    }

    ESP_ERROR_CHECK(iot_button_register_cb(encoder_ctx->btn_handle, BUTTON_PRESS_DOWN,
                                           lvgl_port_encoder_btn_down_handler, encoder_ctx));
    ESP_ERROR_CHECK(iot_button_register_cb(encoder_ctx->btn_handle, BUTTON_PRESS_UP, lvgl_port_encoder_btn_up_handler,
                                           encoder_ctx));

    /* Register a encoder input device */
    lv_indev_drv_init(&encoder_ctx->indev_drv);
    encoder_ctx->indev_drv.type = LV_INDEV_TYPE_ENCODER;
    encoder_ctx->indev_drv.disp = encoder_cfg->disp;
    encoder_ctx->indev_drv.read_cb = lvgl_port_encoder_read;
    encoder_ctx->indev_drv.user_data = encoder_ctx;
    indev = lv_indev_drv_register(&encoder_ctx->indev_drv);
    encoder_ctx->indev = indev;

err:
    if (ret != ESP_OK) {
        if (encoder_ctx->knob_handle != NULL) {
            iot_knob_delete(encoder_ctx->knob_handle);
        }

        if (encoder_ctx->btn_handle != NULL) {
            iot_button_delete(encoder_ctx->btn_handle);
        }

        if (encoder_ctx != NULL) {
            free(encoder_ctx);
        }
    }
    return indev;
}

esp_err_t lvgl_port_remove_encoder(lv_indev_t *encoder) {
    assert(encoder);
    lv_indev_drv_t *indev_drv = encoder->driver;
    assert(indev_drv);
    lvgl_port_encoder_ctx_t *encoder_ctx = (lvgl_port_encoder_ctx_t *)indev_drv->user_data;

    if (encoder_ctx->knob_handle != NULL) {
        iot_knob_delete(encoder_ctx->knob_handle);
    }

    if (encoder_ctx->btn_handle != NULL) {
        iot_button_delete(encoder_ctx->btn_handle);
    }

    if (encoder_ctx != NULL) {
        lv_indev_delete(encoder);
        free(encoder_ctx);
    }

    return ESP_OK;
}

esp_err_t lvgl_port_encoder_btn_register_event_cb(lv_indev_t *encoder, button_event_t event, button_cb_t cb,
                                                  void *user_data) {
    assert(encoder);
    assert(cb);
    lv_indev_drv_t *indev_drv = encoder->driver;
    assert(indev_drv);
    lvgl_port_encoder_ctx_t *encoder_ctx = (lvgl_port_encoder_ctx_t *)indev_drv->user_data;
    assert(encoder_ctx);
    if (event == BUTTON_PRESS_DOWN || event == BUTTON_PRESS_UP) {
        ESP_LOGW(TAG, "Button Press down or up event is reserved for lvgl");
        return ESP_ERR_NOT_SUPPORTED;
    }
    return iot_button_register_cb(encoder_ctx->btn_handle, event, cb, user_data);
}

esp_err_t lvgl_port_encoder_btn_register_event_data_cb(lv_indev_t *encoder, button_event_config_t event_cfg,
                                                       button_cb_t cb, void *user_data) {
    assert(encoder);
    assert(cb);
    lv_indev_drv_t *indev_drv = encoder->driver;
    assert(indev_drv);
    lvgl_port_encoder_ctx_t *encoder_ctx = (lvgl_port_encoder_ctx_t *)indev_drv->user_data;
    assert(encoder_ctx);
    if (event_cfg.event == BUTTON_PRESS_DOWN || event_cfg.event == BUTTON_PRESS_UP) {
        ESP_LOGW(TAG, "Button Press down or up event is reserved for lvgl");
        return ESP_ERR_NOT_SUPPORTED;
    }
    return iot_button_register_event_cb(encoder_ctx->btn_handle, event_cfg, cb, user_data);
}

#endif

#ifdef ESP_LVGL_PORT_BUTTON_COMPONENT
lv_indev_t *lvgl_port_add_navigation_buttons(const lvgl_port_nav_btns_cfg_t *buttons_cfg) {
    esp_err_t ret = ESP_OK;
    lv_indev_t *indev = NULL;
    assert(buttons_cfg != NULL);
    assert(buttons_cfg->disp != NULL);

    /* Touch context */
    lvgl_port_nav_btns_ctx_t *buttons_ctx = malloc(sizeof(lvgl_port_nav_btns_ctx_t));
    if (buttons_ctx == NULL) {
        ESP_LOGE(TAG, "Not enough memory for buttons context allocation!");
        return NULL;
    }

    /* Previous button */
    if (buttons_cfg->button_prev != NULL) {
        buttons_ctx->btn[LVGL_PORT_NAV_BTN_PREV] = iot_button_create(buttons_cfg->button_prev);
        ESP_GOTO_ON_FALSE(buttons_ctx->btn[LVGL_PORT_NAV_BTN_PREV], ESP_ERR_NO_MEM, err, TAG,
                          "Not enough memory for button create!");
    }

    /* Next button */
    if (buttons_cfg->button_next != NULL) {
        buttons_ctx->btn[LVGL_PORT_NAV_BTN_NEXT] = iot_button_create(buttons_cfg->button_next);
        ESP_GOTO_ON_FALSE(buttons_ctx->btn[LVGL_PORT_NAV_BTN_NEXT], ESP_ERR_NO_MEM, err, TAG,
                          "Not enough memory for button create!");
    }

    /* Enter button */
    if (buttons_cfg->button_enter != NULL) {
        buttons_ctx->btn[LVGL_PORT_NAV_BTN_ENTER] = iot_button_create(buttons_cfg->button_enter);
        ESP_GOTO_ON_FALSE(buttons_ctx->btn[LVGL_PORT_NAV_BTN_ENTER], ESP_ERR_NO_MEM, err, TAG,
                          "Not enough memory for button create!");
    }

    /* Button handlers */
    for (int i = 0; i < LVGL_PORT_NAV_BTN_CNT; i++) {
        ESP_ERROR_CHECK(
            iot_button_register_cb(buttons_ctx->btn[i], BUTTON_PRESS_DOWN, lvgl_port_btn_down_handler, buttons_ctx));
        ESP_ERROR_CHECK(
            iot_button_register_cb(buttons_ctx->btn[i], BUTTON_PRESS_UP, lvgl_port_btn_up_handler, buttons_ctx));
    }

    buttons_ctx->btn_prev = false;
    buttons_ctx->btn_next = false;
    buttons_ctx->btn_enter = false;

    /* Register a touchpad input device */
    lv_indev_drv_init(&buttons_ctx->indev_drv);
    buttons_ctx->indev_drv.type = LV_INDEV_TYPE_ENCODER;
    buttons_ctx->indev_drv.disp = buttons_cfg->disp;
    buttons_ctx->indev_drv.read_cb = lvgl_port_navigation_buttons_read;
    buttons_ctx->indev_drv.user_data = buttons_ctx;
    buttons_ctx->indev_drv.long_press_repeat_time = 300;
    indev = lv_indev_drv_register(&buttons_ctx->indev_drv);

err:
    if (ret != ESP_OK) {
        for (int i = 0; i < LVGL_PORT_NAV_BTN_CNT; i++) {
            if (buttons_ctx->btn[i] != NULL) {
                iot_button_delete(buttons_ctx->btn[i]);
            }
        }

        if (buttons_ctx != NULL) {
            free(buttons_ctx);
        }
    }

    return indev;
}

esp_err_t lvgl_port_remove_navigation_buttons(lv_indev_t *buttons) {
    assert(buttons);
    lv_indev_drv_t *indev_drv = buttons->driver;
    assert(indev_drv);
    lvgl_port_nav_btns_ctx_t *buttons_ctx = (lvgl_port_nav_btns_ctx_t *)indev_drv->user_data;

    /* Remove input device driver */
    lv_indev_delete(buttons);

    if (buttons_ctx) {
        free(buttons_ctx);
    }

    return ESP_OK;
}
#endif

#ifdef ESP_LVGL_PORT_USB_HOST_HID_COMPONENT
lv_indev_t *lvgl_port_add_usb_hid_mouse_input(const lvgl_port_hid_mouse_cfg_t *mouse_cfg) {
    lv_indev_t *indev;
    assert(mouse_cfg);
    assert(mouse_cfg->disp);
    assert(mouse_cfg->disp->driver);

    /* Initialize USB HID */
    lvgl_port_usb_hid_ctx_t *hid_ctx = lvgl_port_hid_init();
    if (hid_ctx == NULL) {
        return NULL;
    }

    /* Mouse sensitivity cannot be zero */
    hid_ctx->mouse.sensitivity = (mouse_cfg->sensitivity == 0 ? 1 : mouse_cfg->sensitivity);
    /* Default coordinates to screen center */
    hid_ctx->mouse.x = (mouse_cfg->disp->driver->hor_res * hid_ctx->mouse.sensitivity) / 2;
    hid_ctx->mouse.y = (mouse_cfg->disp->driver->ver_res * hid_ctx->mouse.sensitivity) / 2;

    /* Register a mouse input device */
    lv_indev_drv_init(&hid_ctx->mouse.drv);
    hid_ctx->mouse.drv.type = LV_INDEV_TYPE_POINTER;
    hid_ctx->mouse.drv.disp = mouse_cfg->disp;
    hid_ctx->mouse.drv.read_cb = lvgl_port_usb_hid_read_mouse;
    hid_ctx->mouse.drv.user_data = hid_ctx;
    indev = lv_indev_drv_register(&hid_ctx->mouse.drv);

    /* Set image of cursor */
    lv_obj_t *cursor = mouse_cfg->cursor_img;
    if (cursor == NULL) {
        cursor = lv_img_create(lv_scr_act());
        lv_img_set_src(cursor, &img_cursor);
    }
    lv_indev_set_cursor(indev, cursor);

    return indev;
}

lv_indev_t *lvgl_port_add_usb_hid_keyboard_input(const lvgl_port_hid_keyboard_cfg_t *keyboard_cfg) {
    lv_indev_t *indev;
    assert(keyboard_cfg);
    assert(keyboard_cfg->disp);

    /* Initialize USB HID */
    lvgl_port_usb_hid_ctx_t *hid_ctx = lvgl_port_hid_init();
    if (hid_ctx == NULL) {
        return NULL;
    }

    /* Register a keyboard input device */
    lv_indev_drv_init(&hid_ctx->kb.drv);
    hid_ctx->kb.drv.type = LV_INDEV_TYPE_KEYPAD;
    hid_ctx->kb.drv.disp = keyboard_cfg->disp;
    hid_ctx->kb.drv.read_cb = lvgl_port_usb_hid_read_kb;
    hid_ctx->kb.drv.user_data = hid_ctx;
    indev = lv_indev_drv_register(&hid_ctx->kb.drv);

    return indev;
}

esp_err_t lvgl_port_remove_usb_hid_input(lv_indev_t *hid) {
    assert(hid);
    lv_indev_drv_t *indev_drv = hid->driver;
    assert(indev_drv);
    lvgl_port_usb_hid_ctx_t *hid_ctx = (lvgl_port_usb_hid_ctx_t *)indev_drv->user_data;

    /* Remove input device driver */
    lv_indev_delete(hid);

    /* If all hid input devices are removed, stop task and clean all */
    if (lvgl_port_ctx.hid_ctx.mouse.drv.disp == NULL && lvgl_port_ctx.hid_ctx.kb.drv.disp) {
        hid_ctx->running = false;
    }

    return ESP_OK;
}
#endif

bool lvgl_port_lock(uint32_t timeout_ms) {
    assert(lvgl_port_ctx.lvgl_mux && "lvgl_port_init must be called first");

    const TickType_t timeout_ticks = (timeout_ms == 0) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    return xSemaphoreTakeRecursive(lvgl_port_ctx.lvgl_mux, timeout_ticks) == pdTRUE;
}

void lvgl_port_unlock(void) {
    assert(lvgl_port_ctx.lvgl_mux && "lvgl_port_init must be called first");
    xSemaphoreGiveRecursive(lvgl_port_ctx.lvgl_mux);
}

esp_err_t lvgl_port_panel_draw_bitmap(esp_lcd_panel_handle_t panel_handle, int x_start, int y_start, int x_end,
                                      int y_end, const void *color_data, uint32_t timeout_ms) {
    if (lvgl_port_ctx.panel_mux == NULL || lvgl_port_ctx.panel_trans_done == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    const TickType_t timeout_ticks = timeout_ms == 0 ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    if (xSemaphoreTake(lvgl_port_ctx.panel_mux, timeout_ticks) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    esp_err_t ret =
        lvgl_port_panel_draw_bitmap_locked(panel_handle, x_start, y_start, x_end, y_end, color_data, timeout_ms);
    xSemaphoreGive(lvgl_port_ctx.panel_mux);
    return ret;
}

void lvgl_port_flush_ready(lv_disp_t *disp) {
    assert(disp);
    assert(disp->driver);
    lv_disp_flush_ready(disp->driver);
}

/*******************************************************************************
* Private functions
*******************************************************************************/

static void lvgl_port_task(void *arg) {
    uint32_t task_delay_ms = lvgl_port_ctx.task_max_sleep_ms;

    ESP_LOGI(TAG, "Starting LVGL task");
    while (lvgl_port_ctx.running) {
        if (lvgl_port_lock(0)) {
            task_delay_ms = lv_timer_handler();
            lvgl_port_unlock();
        }
        if ((task_delay_ms > lvgl_port_ctx.task_max_sleep_ms) || (1 == task_delay_ms)) {
            task_delay_ms = lvgl_port_ctx.task_max_sleep_ms;
        } else if (task_delay_ms < 1) {
            task_delay_ms = 1;
        }
        vTaskDelay(pdMS_TO_TICKS(task_delay_ms));
    }

    task_stopped = true;
    for (;;)
        vTaskSuspend(NULL);
}

static esp_err_t lvgl_port_task_create(const lvgl_port_cfg_t *cfg) {
    task_stopped = false;
    lvgl_port_ctx.running = true;
    ESP_RETURN_ON_FALSE(cfg != NULL, ESP_ERR_INVALID_ARG, TAG, "invalid LVGL task config");

#if CONFIG_LVGL_PORT_TASK_STACK_ALLOC_EXTERNAL
    if (lvgl_port_ctx.task_tcb == NULL) {
        lvgl_port_ctx.task_tcb =
            (StaticTask_t *)heap_caps_calloc(1, sizeof(StaticTask_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    } else {
        memset(lvgl_port_ctx.task_tcb, 0, sizeof(StaticTask_t));
    }
    ESP_RETURN_ON_FALSE(lvgl_port_ctx.task_tcb != NULL, ESP_ERR_NO_MEM, TAG, "alloc LVGL task TCB failed");

    if (lvgl_port_ctx.task_stack != NULL && lvgl_port_ctx.task_stack_words < (size_t)cfg->task_stack) {
        free(lvgl_port_ctx.task_stack);
        lvgl_port_ctx.task_stack = NULL;
        lvgl_port_ctx.task_stack_words = 0;
    }
    if (lvgl_port_ctx.task_stack == NULL) {
        lvgl_port_ctx.task_stack = (StackType_t *)heap_caps_calloc(1, (size_t)cfg->task_stack * sizeof(StackType_t),
                                                                   MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (lvgl_port_ctx.task_stack != NULL) {
            lvgl_port_ctx.task_stack_words = (size_t)cfg->task_stack;
        }
    } else {
        memset(lvgl_port_ctx.task_stack, 0, (size_t)cfg->task_stack * sizeof(StackType_t));
    }
    if (lvgl_port_ctx.task_stack == NULL) {
        free(lvgl_port_ctx.task_tcb);
        lvgl_port_ctx.task_tcb = NULL;
        lvgl_port_ctx.task_stack_words = 0;
        ESP_LOGE(TAG, "alloc LVGL task stack in PSRAM failed, size=%d", cfg->task_stack);
        return ESP_ERR_NO_MEM;
    }

    if (cfg->task_affinity < 0) {
        lvgl_port_ctx.task_handle =
            xTaskCreateStatic(lvgl_port_task, "LVGL task", cfg->task_stack, NULL, cfg->task_priority,
                              lvgl_port_ctx.task_stack, lvgl_port_ctx.task_tcb);
    } else {
        lvgl_port_ctx.task_handle =
            xTaskCreateStaticPinnedToCore(lvgl_port_task, "LVGL task", cfg->task_stack, NULL, cfg->task_priority,
                                          lvgl_port_ctx.task_stack, lvgl_port_ctx.task_tcb, cfg->task_affinity);
    }

    if (lvgl_port_ctx.task_handle == NULL) {
        free(lvgl_port_ctx.task_stack);
        free(lvgl_port_ctx.task_tcb);
        lvgl_port_ctx.task_stack = NULL;
        lvgl_port_ctx.task_tcb = NULL;
        lvgl_port_ctx.task_stack_words = 0;
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "LVGL task stack allocated in PSRAM, size=%d", cfg->task_stack);
    return ESP_OK;
#else
    BaseType_t res;

    if (cfg->task_affinity < 0) {
        res = xTaskCreate(lvgl_port_task, "LVGL task", cfg->task_stack, NULL, cfg->task_priority,
                          &lvgl_port_ctx.task_handle);
    } else {
        res = xTaskCreatePinnedToCore(lvgl_port_task, "LVGL task", cfg->task_stack, NULL, cfg->task_priority,
                                      &lvgl_port_ctx.task_handle, cfg->task_affinity);
    }
    return res == pdPASS ? ESP_OK : ESP_FAIL;
#endif
}

static void lvgl_port_task_deinit(void) {
#if CONFIG_LVGL_PORT_TASK_STACK_ALLOC_EXTERNAL
    StaticTask_t *task_tcb = lvgl_port_ctx.task_tcb;
    StackType_t *task_stack = lvgl_port_ctx.task_stack;
    size_t task_stack_words = lvgl_port_ctx.task_stack_words;
#endif
    if (lvgl_port_ctx.lvgl_mux) {
        vSemaphoreDelete(lvgl_port_ctx.lvgl_mux);
    }
    if (lvgl_port_ctx.panel_mux) {
        vSemaphoreDelete(lvgl_port_ctx.panel_mux);
    }
    if (lvgl_port_ctx.panel_trans_done) {
        vSemaphoreDelete(lvgl_port_ctx.panel_trans_done);
    }
    memset(&lvgl_port_ctx, 0, sizeof(lvgl_port_ctx));
#if CONFIG_LVGL_PORT_TASK_STACK_ALLOC_EXTERNAL
    lvgl_port_ctx.task_tcb = task_tcb;
    lvgl_port_ctx.task_stack = task_stack;
    lvgl_port_ctx.task_stack_words = task_stack_words;
#endif
#if LV_ENABLE_GC || !LV_MEM_CUSTOM
    /* Deinitialize LVGL */
    lv_deinit();
#endif
}

#if LVGL_PORT_HANDLE_FLUSH_READY
static bool lvgl_port_flush_ready_callback(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *edata,
                                           void *user_ctx) {
    (void)panel_io;
    (void)edata;
    lv_disp_drv_t *disp_drv = (lv_disp_drv_t *)user_ctx;
    if (lvgl_port_ctx.panel_direct_draw_pending && lvgl_port_ctx.panel_trans_done != NULL) {
        BaseType_t high_task_woken = pdFALSE;
        lvgl_port_ctx.panel_direct_draw_pending = false;
        xSemaphoreGiveFromISR(lvgl_port_ctx.panel_trans_done, &high_task_woken);
        if (high_task_woken == pdTRUE) {
            portYIELD_FROM_ISR();
        }
    } else {
        assert(disp_drv != NULL);
        lv_disp_flush_ready(disp_drv);
    }
    return false;
}
#endif

static esp_err_t lvgl_port_panel_draw_bitmap_locked(esp_lcd_panel_handle_t panel_handle, int x_start, int y_start,
                                                    int x_end, int y_end, const void *color_data, uint32_t timeout_ms) {
    if (panel_handle == NULL || color_data == NULL || lvgl_port_ctx.panel_trans_done == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (lvgl_port_ctx.panel_direct_draw_pending) {
        return ESP_ERR_INVALID_STATE;
    }

    while (xSemaphoreTake(lvgl_port_ctx.panel_trans_done, 0) == pdTRUE) {
    }

    lvgl_port_ctx.panel_direct_draw_pending = true;
    esp_err_t ret = esp_lcd_panel_draw_bitmap(panel_handle, x_start, y_start, x_end, y_end, color_data);
    if (ret != ESP_OK) {
        lvgl_port_ctx.panel_direct_draw_pending = false;
        return ret;
    }

    const TickType_t timeout_ticks = timeout_ms == 0 ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    if (xSemaphoreTake(lvgl_port_ctx.panel_trans_done, timeout_ticks) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    lvgl_port_ctx.panel_direct_draw_pending = false;
    return ESP_OK;
}

static esp_err_t lvgl_port_flush_staged(lvgl_port_display_ctx_t *disp_ctx, const lv_area_t *area,
                                        const lv_color_t *color_map) {
    if (disp_ctx == NULL || area == NULL || color_map == NULL || disp_ctx->trans_buffer == NULL ||
        area->x2 < area->x1 || area->y2 < area->y1) {
        return ESP_ERR_INVALID_ARG;
    }
    size_t width = (size_t)(area->x2 - area->x1 + 1);
    size_t rows_per_transfer = disp_ctx->trans_buffer_size / width;
    if (rows_per_transfer == 0U) {
        return ESP_ERR_INVALID_ARG;
    }
    for (int y = area->y1; y <= area->y2;) {
        /* Do not overwrite storage still borrowed by a previous timed-out DMA. */
        if (lvgl_port_ctx.panel_direct_draw_pending) {
            return ESP_ERR_INVALID_STATE;
        }
        size_t rows = (size_t)(area->y2 - y + 1);
        if (rows > rows_per_transfer) {
            rows = rows_per_transfer;
        }
        size_t pixels = rows * width;
        memcpy(disp_ctx->trans_buffer, color_map, pixels * sizeof(lv_color_t));
        esp_err_t ret = lvgl_port_panel_draw_bitmap(disp_ctx->panel_handle, area->x1, y, area->x2 + 1, y + (int)rows,
                                                    disp_ctx->trans_buffer, 50);
        if (ret != ESP_OK) {
            return ret;
        }
        color_map += pixels;
        y += (int)rows;
    }
    return ESP_OK;
}

static void lvgl_port_flush_callback(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map) {
    assert(drv != NULL);
    lvgl_port_display_ctx_t *disp_ctx = (lvgl_port_display_ctx_t *)drv->user_data;
    assert(disp_ctx != NULL);

    if (disp_ctx->trans_buffer != NULL) {
        esp_err_t ret = lvgl_port_flush_staged(disp_ctx, area, color_map);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Staged panel flush failed: %s", esp_err_to_name(ret));
        }
        /* Staging waits for DMA itself, so its ISR releases panel_trans_done.
         * LVGL's original PSRAM draw buffer can now be reused, even on error. */
        lv_disp_flush_ready(drv);
        return;
    }

    const int offsetx1 = area->x1;
    const int offsetx2 = area->x2;
    const int offsety1 = area->y1;
    const int offsety2 = area->y2;
    // copy a buffer's content to a specific area of the display
    esp_err_t ret =
        esp_lcd_panel_draw_bitmap(disp_ctx->panel_handle, offsetx1, offsety1, offsetx2 + 1, offsety2 + 1, color_map);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Panel flush failed: %s", esp_err_to_name(ret));
        lv_disp_flush_ready(drv);
    }
}

static void lvgl_port_update_callback(lv_disp_drv_t *drv) {
    assert(drv);
    lvgl_port_display_ctx_t *disp_ctx = (lvgl_port_display_ctx_t *)drv->user_data;
    assert(disp_ctx != NULL);
    esp_lcd_panel_handle_t panel_handle = disp_ctx->panel_handle;

    /* Solve rotation screen and touch */
    switch (drv->rotated) {
    case LV_DISP_ROT_NONE:
        /* Rotate LCD display */
        esp_lcd_panel_swap_xy(panel_handle, disp_ctx->rotation.swap_xy);
        esp_lcd_panel_mirror(panel_handle, disp_ctx->rotation.mirror_x, disp_ctx->rotation.mirror_y);
        break;
    case LV_DISP_ROT_90:
        /* Rotate LCD display */
        esp_lcd_panel_swap_xy(panel_handle, !disp_ctx->rotation.swap_xy);
        if (disp_ctx->rotation.swap_xy) {
            esp_lcd_panel_mirror(panel_handle, !disp_ctx->rotation.mirror_x, disp_ctx->rotation.mirror_y);
        } else {
            esp_lcd_panel_mirror(panel_handle, disp_ctx->rotation.mirror_x, !disp_ctx->rotation.mirror_y);
        }
        break;
    case LV_DISP_ROT_180:
        /* Rotate LCD display */
        esp_lcd_panel_swap_xy(panel_handle, disp_ctx->rotation.swap_xy);
        esp_lcd_panel_mirror(panel_handle, !disp_ctx->rotation.mirror_x, !disp_ctx->rotation.mirror_y);
        break;
    case LV_DISP_ROT_270:
        /* Rotate LCD display */
        esp_lcd_panel_swap_xy(panel_handle, !disp_ctx->rotation.swap_xy);
        if (disp_ctx->rotation.swap_xy) {
            esp_lcd_panel_mirror(panel_handle, disp_ctx->rotation.mirror_x, !disp_ctx->rotation.mirror_y);
        } else {
            esp_lcd_panel_mirror(panel_handle, !disp_ctx->rotation.mirror_x, disp_ctx->rotation.mirror_y);
        }
        break;
    }
}

static void lvgl_port_pix_monochrome_callback(lv_disp_drv_t *drv, uint8_t *buf, lv_coord_t buf_w, lv_coord_t x,
                                              lv_coord_t y, lv_color_t color, lv_opa_t opa) {
    if (drv->rotated == LV_DISP_ROT_90 || drv->rotated == LV_DISP_ROT_270) {
        lv_coord_t tmp_x = x;
        x = y;
        y = tmp_x;
    }

    /* Write to the buffer as required for the display.
    * It writes only 1-bit for monochrome displays mapped vertically.*/
    buf += drv->hor_res * (y >> 3) + x;
    if (lv_color_to1(color)) {
        (*buf) &= ~(1 << (y % 8));
    } else {
        (*buf) |= (1 << (y % 8));
    }
}

#ifdef ESP_LVGL_PORT_TOUCH_COMPONENT
static void lvgl_port_touchpad_read(lv_indev_drv_t *indev_drv, lv_indev_data_t *data) {
#if CONFIG_WATCHER_DEBUG_CLI_ENABLE
    static bool debug_touch_reader_started = false;
    static bool debug_touch_pressed = false;
    static uint32_t debug_touch_read_errors = 0;
#endif
    assert(indev_drv);
    lvgl_port_touch_ctx_t *touch_ctx = (lvgl_port_touch_ctx_t *)indev_drv->user_data;
    assert(touch_ctx->handle);

    uint16_t touchpad_x[1] = {0};
    uint16_t touchpad_y[1] = {0};
    uint16_t touchpad_strength[1] = {0};
    uint8_t touchpad_cnt = 0;

    /* Read data from touch controller into memory */
    esp_err_t read_result = esp_lcd_touch_read_data(touch_ctx->handle);
#if CONFIG_WATCHER_DEBUG_CLI_ENABLE
    if (!debug_touch_reader_started) {
        ESP_LOGI(TAG, "debug screen touch reader started sensitivity=%d", (int)touch_ctx->sensitivity);
        debug_touch_reader_started = true;
    }
#endif
    if (read_result != ESP_OK) {
        data->state = LV_INDEV_STATE_RELEASED;
#if CONFIG_WATCHER_DEBUG_CLI_ENABLE
        debug_touch_read_errors++;
        if (debug_touch_read_errors == 1 || debug_touch_read_errors % 100 == 0) {
            ESP_LOGW(TAG, "debug screen touch read failed result=%s count=%lu", esp_err_to_name(read_result),
                     (unsigned long)debug_touch_read_errors);
        }
#endif
        return;
    }

    /* Read data from touch controller */
    bool touchpad_pressed =
        esp_lcd_touch_get_coordinates(touch_ctx->handle, touchpad_x, touchpad_y, touchpad_strength, &touchpad_cnt, 1);

    const bool touchpad_has_coordinates = touchpad_pressed && touchpad_cnt > 0;
    const bool touchpad_accepted = touchpad_has_coordinates && touchpad_strength[0] > touch_ctx->sensitivity;

    if (touchpad_pressed && touchpad_cnt == 0 && !s_touch_diag_empty_frame) {
        ESP_LOGW(TAG, "Touch raw frame has no coordinates");
    }
    s_touch_diag_empty_frame = touchpad_pressed && touchpad_cnt == 0;

    if (touchpad_has_coordinates) {
        const int delta_x = (int)touchpad_x[0] - (int)s_touch_diag_x;
        const int delta_y = (int)touchpad_y[0] - (int)s_touch_diag_y;
        if (!s_touch_diag_active) {
            ESP_LOGI(TAG, "Touch raw press x=%u y=%u strength=%u accepted=%d threshold=%d", (unsigned)touchpad_x[0],
                     (unsigned)touchpad_y[0], (unsigned)touchpad_strength[0], touchpad_accepted ? 1 : 0,
                     (int)touch_ctx->sensitivity);
        } else if (delta_x >= 12 || delta_x <= -12 || delta_y >= 12 || delta_y <= -12) {
            ESP_LOGI(TAG, "Touch raw move x=%u y=%u strength=%u", (unsigned)touchpad_x[0], (unsigned)touchpad_y[0],
                     (unsigned)touchpad_strength[0]);
        }
        s_touch_diag_active = true;
        s_touch_diag_x = touchpad_x[0];
        s_touch_diag_y = touchpad_y[0];
    } else if (s_touch_diag_active) {
        ESP_LOGI(TAG, "Touch raw release");
        s_touch_diag_active = false;
    }

    if (touchpad_accepted) {
        data->point.x = touchpad_x[0];
        data->point.y = touchpad_y[0];
        data->state = LV_INDEV_STATE_PRESSED;
#if CONFIG_WATCHER_DEBUG_CLI_ENABLE
        if (!debug_touch_pressed) {
            ESP_LOGI(TAG, "debug screen touch pressed x=%u y=%u strength=%u", (unsigned)touchpad_x[0],
                     (unsigned)touchpad_y[0], (unsigned)touchpad_strength[0]);
            debug_touch_pressed = true;
        }
#endif
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
#if CONFIG_WATCHER_DEBUG_CLI_ENABLE
        if (debug_touch_pressed) {
            ESP_LOGI(TAG, "debug screen touch released");
            debug_touch_pressed = false;
        }
#endif
    }
}
#endif

#ifdef ESP_LVGL_PORT_KNOB_COMPONENT
static void lvgl_port_encoder_read(lv_indev_drv_t *indev_drv, lv_indev_data_t *data) {
    static int32_t last_v = 0;
    bool cancelled = false;

    assert(indev_drv);
    lvgl_port_encoder_ctx_t *ctx = (lvgl_port_encoder_ctx_t *)indev_drv->user_data;
    assert(ctx);

    const int32_t invd = iot_knob_get_count_value(ctx->knob_handle);

    if (last_v != invd) {
        const knob_event_t event = iot_knob_get_event(ctx->knob_handle);
        const int32_t diff = watcher_input_encoder_count_delta(last_v, invd, event == KNOB_H_LIM, event == KNOB_L_LIM);
        last_v = invd;
        const watcher_input_result_t result = watcher_input_router_global_on_rotate(diff);
        data->enc_diff = result.owner == WATCHER_INPUT_OWNER_LVGL ? diff : 0;
    } else {
        data->enc_diff = 0;
    }
    ctx->read_rotating = data->enc_diff != 0;
    data->state =
        watcher_input_router_global_lvgl_button_pressed(&cancelled) ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
    if (cancelled && ctx->indev != NULL) {
        lv_indev_reset(ctx->indev, NULL);
    }
}

bool lvgl_port_encoder_is_rotating(lv_indev_t *indev) {
    if (indev == NULL || indev != lv_indev_get_act() || indev->driver->read_cb != lvgl_port_encoder_read)
        return false;
    const lvgl_port_encoder_ctx_t *ctx = indev->driver->user_data;
    return ctx != NULL && ctx->read_rotating;
}

static void lvgl_port_encoder_btn_down_handler(void *arg, void *arg2) {
    lvgl_port_encoder_ctx_t *ctx = (lvgl_port_encoder_ctx_t *)arg2;
    button_handle_t button = (button_handle_t)arg;
    if (ctx && button) {
        /* ENTER */
        if (button == ctx->btn_handle) {
            const watcher_input_result_t result =
                watcher_input_router_global_on_button_down((uint32_t)(esp_timer_get_time() / 1000LL));
            ESP_LOGI(TAG, "input context=%s event=%s owner=%s", watcher_input_context_name(result.scope.context),
                     watcher_input_event_name(result.event), watcher_input_owner_name(result.owner));
        }
    }
}

static void lvgl_port_encoder_btn_up_handler(void *arg, void *arg2) {
    lvgl_port_encoder_ctx_t *ctx = (lvgl_port_encoder_ctx_t *)arg2;
    button_handle_t button = (button_handle_t)arg;
    if (ctx && button) {
        /* ENTER */
        if (button == ctx->btn_handle) {
            const watcher_input_result_t result =
                watcher_input_router_global_on_button_up((uint32_t)(esp_timer_get_time() / 1000LL));
            if (result.event == WATCHER_INPUT_EVENT_REJECTED) {
                ESP_LOGI(TAG, "input context=%s event=%s owner=%s reject=%s duration_ms=%lu",
                         watcher_input_context_name(result.scope.context), watcher_input_event_name(result.event),
                         watcher_input_owner_name(result.owner), watcher_input_reject_name(result.reject),
                         (unsigned long)result.duration_ms);
            } else {
                ESP_LOGI(TAG, "input context=%s event=%s owner=%s duration_ms=%lu",
                         watcher_input_context_name(result.scope.context), watcher_input_event_name(result.event),
                         watcher_input_owner_name(result.owner), (unsigned long)result.duration_ms);
            }
        }
    }
}
#endif

#ifdef ESP_LVGL_PORT_BUTTON_COMPONENT
static uint32_t last_key = 0;
static void lvgl_port_navigation_buttons_read(lv_indev_drv_t *indev_drv, lv_indev_data_t *data) {
    assert(indev_drv);
    lvgl_port_nav_btns_ctx_t *ctx = (lvgl_port_nav_btns_ctx_t *)indev_drv->user_data;
    assert(ctx);

    /* Buttons */
    if (ctx->btn_prev) {
        data->key = LV_KEY_LEFT;
        last_key = LV_KEY_LEFT;
        data->state = LV_INDEV_STATE_PRESSED;
    } else if (ctx->btn_next) {
        data->key = LV_KEY_RIGHT;
        last_key = LV_KEY_RIGHT;
        data->state = LV_INDEV_STATE_PRESSED;
    } else if (ctx->btn_enter) {
        data->key = LV_KEY_ENTER;
        last_key = LV_KEY_ENTER;
        data->state = LV_INDEV_STATE_PRESSED;
    } else {
        data->key = last_key;
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

static void lvgl_port_btn_down_handler(void *arg, void *arg2) {
    lvgl_port_nav_btns_ctx_t *ctx = (lvgl_port_nav_btns_ctx_t *)arg2;
    button_handle_t button = (button_handle_t)arg;
    if (ctx && button) {
        /* PREV */
        if (button == ctx->btn[LVGL_PORT_NAV_BTN_PREV]) {
            ctx->btn_prev = true;
        }
        /* NEXT */
        if (button == ctx->btn[LVGL_PORT_NAV_BTN_NEXT]) {
            ctx->btn_next = true;
        }
        /* ENTER */
        if (button == ctx->btn[LVGL_PORT_NAV_BTN_ENTER]) {
            ctx->btn_enter = true;
        }
    }
}

static void lvgl_port_btn_up_handler(void *arg, void *arg2) {
    lvgl_port_nav_btns_ctx_t *ctx = (lvgl_port_nav_btns_ctx_t *)arg2;
    button_handle_t button = (button_handle_t)arg;
    if (ctx && button) {
        /* PREV */
        if (button == ctx->btn[LVGL_PORT_NAV_BTN_PREV]) {
            ctx->btn_prev = false;
        }
        /* NEXT */
        if (button == ctx->btn[LVGL_PORT_NAV_BTN_NEXT]) {
            ctx->btn_next = false;
        }
        /* ENTER */
        if (button == ctx->btn[LVGL_PORT_NAV_BTN_ENTER]) {
            ctx->btn_enter = false;
        }
    }
}
#endif

#ifdef ESP_LVGL_PORT_USB_HOST_HID_COMPONENT
static lvgl_port_usb_hid_ctx_t *lvgl_port_hid_init(void) {
    esp_err_t ret;

    /* USB HID is already initialized */
    if (lvgl_port_ctx.hid_ctx.task) {
        return &lvgl_port_ctx.hid_ctx;
    }

    /* USB HID host driver config */
    const hid_host_driver_config_t hid_host_driver_config = {
        .create_background_task = true,
        .task_priority = 5,
        .stack_size = 4096,
        .core_id = 0,
        .callback = lvgl_port_usb_hid_callback,
        .callback_arg = &lvgl_port_ctx.hid_ctx,
    };

    ret = hid_host_install(&hid_host_driver_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "USB HID install failed!");
        return NULL;
    }

    lvgl_port_ctx.hid_ctx.queue = xQueueCreate(10, sizeof(lvgl_port_usb_hid_event_t));
    xTaskCreate(&lvgl_port_usb_hid_task, "hid_task", 4 * 1024, &lvgl_port_ctx.hid_ctx, 2, &lvgl_port_ctx.hid_ctx.task);

    return &lvgl_port_ctx.hid_ctx;
}

static char usb_hid_get_keyboard_char(uint8_t key, uint8_t shift) {
    char ret_key = 0;

    const uint8_t keycode2ascii[57][2] = {
        {0, 0},       /* HID_KEY_NO_PRESS        */
        {0, 0},       /* HID_KEY_ROLLOVER        */
        {0, 0},       /* HID_KEY_POST_FAIL       */
        {0, 0},       /* HID_KEY_ERROR_UNDEFINED */
        {'a', 'A'},   /* HID_KEY_A               */
        {'b', 'B'},   /* HID_KEY_B               */
        {'c', 'C'},   /* HID_KEY_C               */
        {'d', 'D'},   /* HID_KEY_D               */
        {'e', 'E'},   /* HID_KEY_E               */
        {'f', 'F'},   /* HID_KEY_F               */
        {'g', 'G'},   /* HID_KEY_G               */
        {'h', 'H'},   /* HID_KEY_H               */
        {'i', 'I'},   /* HID_KEY_I               */
        {'j', 'J'},   /* HID_KEY_J               */
        {'k', 'K'},   /* HID_KEY_K               */
        {'l', 'L'},   /* HID_KEY_L               */
        {'m', 'M'},   /* HID_KEY_M               */
        {'n', 'N'},   /* HID_KEY_N               */
        {'o', 'O'},   /* HID_KEY_O               */
        {'p', 'P'},   /* HID_KEY_P               */
        {'q', 'Q'},   /* HID_KEY_Q               */
        {'r', 'R'},   /* HID_KEY_R               */
        {'s', 'S'},   /* HID_KEY_S               */
        {'t', 'T'},   /* HID_KEY_T               */
        {'u', 'U'},   /* HID_KEY_U               */
        {'v', 'V'},   /* HID_KEY_V               */
        {'w', 'W'},   /* HID_KEY_W               */
        {'x', 'X'},   /* HID_KEY_X               */
        {'y', 'Y'},   /* HID_KEY_Y               */
        {'z', 'Z'},   /* HID_KEY_Z               */
        {'1', '!'},   /* HID_KEY_1               */
        {'2', '@'},   /* HID_KEY_2               */
        {'3', '#'},   /* HID_KEY_3               */
        {'4', '$'},   /* HID_KEY_4               */
        {'5', '%'},   /* HID_KEY_5               */
        {'6', '^'},   /* HID_KEY_6               */
        {'7', '&'},   /* HID_KEY_7               */
        {'8', '*'},   /* HID_KEY_8               */
        {'9', '('},   /* HID_KEY_9               */
        {'0', ')'},   /* HID_KEY_0               */
        {'\r', '\r'}, /* HID_KEY_ENTER           */
        {0, 0},       /* HID_KEY_ESC             */
        {'\b', 0},    /* HID_KEY_DEL             */
        {0, 0},       /* HID_KEY_TAB             */
        {' ', ' '},   /* HID_KEY_SPACE           */
        {'-', '_'},   /* HID_KEY_MINUS           */
        {'=', '+'},   /* HID_KEY_EQUAL           */
        {'[', '{'},   /* HID_KEY_OPEN_BRACKET    */
        {']', '}'},   /* HID_KEY_CLOSE_BRACKET   */
        {'\\', '|'},  /* HID_KEY_BACK_SLASH      */
        {'\\', '|'},
        /* HID_KEY_SHARP           */ // HOTFIX: for NonUS Keyboards repeat HID_KEY_BACK_SLASH
        {';', ':'},                   /* HID_KEY_COLON           */
        {'\'', '"'},                  /* HID_KEY_QUOTE           */
        {'`', '~'},                   /* HID_KEY_TILDE           */
        {',', '<'},                   /* HID_KEY_LESS            */
        {'.', '>'},                   /* HID_KEY_GREATER         */
        {'/', '?'}                    /* HID_KEY_SLASH           */
    };

    if (shift > 1) {
        shift = 1;
    }

    if ((key >= HID_KEY_A) && (key <= HID_KEY_SLASH)) {
        ret_key = keycode2ascii[key][shift];
    }

    return ret_key;
}

static void lvgl_port_usb_hid_host_interface_callback(hid_host_device_handle_t hid_device_handle,
                                                      const hid_host_interface_event_t event, void *arg) {
    hid_host_dev_params_t dev;
    hid_host_device_get_params(hid_device_handle, &dev);
    lvgl_port_usb_hid_ctx_t *hid_ctx = (lvgl_port_usb_hid_ctx_t *)arg;
    uint8_t data[10];
    unsigned int data_length = 0;

    assert(hid_ctx != NULL);

    switch (event) {
    case HID_HOST_INTERFACE_EVENT_INPUT_REPORT:
        hid_host_device_get_raw_input_report_data(hid_device_handle, data, sizeof(data), &data_length);
        if (dev.proto == HID_PROTOCOL_KEYBOARD) {
            hid_keyboard_input_report_boot_t *keyboard = (hid_keyboard_input_report_boot_t *)data;
            if (data_length < sizeof(hid_keyboard_input_report_boot_t)) {
                return;
            }
            for (int i = 0; i < HID_KEYBOARD_KEY_MAX; i++) {
                if (keyboard->key[i] > HID_KEY_ERROR_UNDEFINED) {
                    char key = 0;

                    /* LVGL special keys */
                    if (keyboard->key[i] == HID_KEY_TAB) {
                        if ((keyboard->modifier.left_shift || keyboard->modifier.right_shift)) {
                            key = LV_KEY_PREV;
                        } else {
                            key = LV_KEY_NEXT;
                        }
                    } else if (keyboard->key[i] == HID_KEY_RIGHT) {
                        key = LV_KEY_RIGHT;
                    } else if (keyboard->key[i] == HID_KEY_LEFT) {
                        key = LV_KEY_LEFT;
                    } else if (keyboard->key[i] == HID_KEY_DOWN) {
                        key = LV_KEY_DOWN;
                    } else if (keyboard->key[i] == HID_KEY_UP) {
                        key = LV_KEY_UP;
                    } else if (keyboard->key[i] == HID_KEY_ENTER || keyboard->key[i] == HID_KEY_KEYPAD_ENTER) {
                        key = LV_KEY_ENTER;
                    } else if (keyboard->key[i] == HID_KEY_DELETE) {
                        key = LV_KEY_DEL;
                    } else if (keyboard->key[i] == HID_KEY_HOME) {
                        key = LV_KEY_HOME;
                    } else if (keyboard->key[i] == HID_KEY_END) {
                        key = LV_KEY_END;
                    } else {
                        /* Get ASCII char */
                        key = usb_hid_get_keyboard_char(
                            keyboard->key[i], (keyboard->modifier.left_shift || keyboard->modifier.right_shift));
                    }

                    if (key == 0) {
                        ESP_LOGI(TAG, "Not recognized key: %c (%d)", keyboard->key[i], keyboard->key[i]);
                    }
                    hid_ctx->kb.last_key = key;
                    hid_ctx->kb.pressed = true;
                }
            }

        } else if (dev.proto == HID_PROTOCOL_MOUSE) {
            hid_mouse_input_report_boot_t *mouse = (hid_mouse_input_report_boot_t *)data;
            if (data_length < sizeof(hid_mouse_input_report_boot_t)) {
                break;
            }
            hid_ctx->mouse.left_button = mouse->buttons.button1;
            hid_ctx->mouse.x += mouse->x_displacement;
            hid_ctx->mouse.y += mouse->y_displacement;
        }
        break;
    case HID_HOST_INTERFACE_EVENT_TRANSFER_ERROR:
        break;
    case HID_HOST_INTERFACE_EVENT_DISCONNECTED:
        hid_host_device_close(hid_device_handle);
        break;
    default:
        break;
    }
}

static void lvgl_port_usb_hid_task(void *arg) {
    hid_host_dev_params_t dev;
    lvgl_port_usb_hid_ctx_t *ctx = (lvgl_port_usb_hid_ctx_t *)arg;
    hid_host_device_handle_t hid_device_handle = NULL;
    lvgl_port_usb_hid_event_t msg;

    assert(ctx);

    ctx->running = true;

    while (ctx->running) {
        if (xQueueReceive(ctx->queue, &msg, pdMS_TO_TICKS(50))) {
            hid_device_handle = msg.hid_device_handle;
            hid_host_device_get_params(hid_device_handle, &dev);

            switch (msg.event) {
            case HID_HOST_DRIVER_EVENT_CONNECTED:
                /* Handle mouse or keyboard */
                if (dev.proto == HID_PROTOCOL_KEYBOARD || dev.proto == HID_PROTOCOL_MOUSE) {
                    const hid_host_device_config_t dev_config = {.callback = lvgl_port_usb_hid_host_interface_callback,
                                                                 .callback_arg = ctx};

                    ESP_ERROR_CHECK(hid_host_device_open(hid_device_handle, &dev_config));
                    ESP_ERROR_CHECK(hid_class_request_set_idle(hid_device_handle, 0, 0));
                    ESP_ERROR_CHECK(hid_class_request_set_protocol(hid_device_handle, HID_REPORT_PROTOCOL_BOOT));
                    ESP_ERROR_CHECK(hid_host_device_start(hid_device_handle));
                }
                break;
            default:
                break;
            }
        }
    }

    xQueueReset(ctx->queue);
    vQueueDelete(ctx->queue);

    hid_host_uninstall();

    memset(&lvgl_port_ctx.hid_ctx, 0, sizeof(lvgl_port_usb_hid_ctx_t));

    vTaskDelete(NULL);
}

static void lvgl_port_usb_hid_read_mouse(lv_indev_drv_t *indev_drv, lv_indev_data_t *data) {
    int16_t width = 0;
    int16_t height = 0;
    assert(indev_drv);
    lvgl_port_usb_hid_ctx_t *ctx = (lvgl_port_usb_hid_ctx_t *)indev_drv->user_data;
    assert(ctx);

    if (indev_drv->disp->driver->rotated == LV_DISP_ROT_NONE || indev_drv->disp->driver->rotated == LV_DISP_ROT_180) {
        width = indev_drv->disp->driver->hor_res;
        height = indev_drv->disp->driver->ver_res;
    } else {
        width = indev_drv->disp->driver->ver_res;
        height = indev_drv->disp->driver->hor_res;
    }

    /* Screen borders */
    if (ctx->mouse.x < 0) {
        ctx->mouse.x = 0;
    } else if (ctx->mouse.x > width * ctx->mouse.sensitivity) {
        ctx->mouse.x = width * ctx->mouse.sensitivity;
    }
    if (ctx->mouse.y < 0) {
        ctx->mouse.y = 0;
    } else if (ctx->mouse.y > height * ctx->mouse.sensitivity) {
        ctx->mouse.y = height * ctx->mouse.sensitivity;
    }

    /* Get coordinates by rotation with sensitivity */
    switch (indev_drv->disp->driver->rotated) {
    case LV_DISP_ROT_NONE:
        data->point.x = ctx->mouse.x / ctx->mouse.sensitivity;
        data->point.y = ctx->mouse.y / ctx->mouse.sensitivity;
        break;
    case LV_DISP_ROT_90:
        data->point.y = width - ctx->mouse.x / ctx->mouse.sensitivity;
        data->point.x = ctx->mouse.y / ctx->mouse.sensitivity;
        break;
    case LV_DISP_ROT_180:
        data->point.x = width - ctx->mouse.x / ctx->mouse.sensitivity;
        data->point.y = height - ctx->mouse.y / ctx->mouse.sensitivity;
        break;
    case LV_DISP_ROT_270:
        data->point.y = ctx->mouse.x / ctx->mouse.sensitivity;
        data->point.x = height - ctx->mouse.y / ctx->mouse.sensitivity;
        break;
    }

    if (ctx->mouse.left_button) {
        data->state = LV_INDEV_STATE_PRESSED;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

static void lvgl_port_usb_hid_read_kb(lv_indev_drv_t *indev_drv, lv_indev_data_t *data) {
    assert(indev_drv);
    lvgl_port_usb_hid_ctx_t *ctx = (lvgl_port_usb_hid_ctx_t *)indev_drv->user_data;
    assert(ctx);

    data->key = ctx->kb.last_key;
    if (ctx->kb.pressed) {
        data->state = LV_INDEV_STATE_PRESSED;
        ctx->kb.pressed = false;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
        ctx->kb.last_key = 0;
    }
}

static void lvgl_port_usb_hid_callback(hid_host_device_handle_t hid_device_handle, const hid_host_driver_event_t event,
                                       void *arg) {
    lvgl_port_usb_hid_ctx_t *hid_ctx = (lvgl_port_usb_hid_ctx_t *)arg;

    const lvgl_port_usb_hid_event_t msg = {.hid_device_handle = hid_device_handle, .event = event, .arg = arg};

    xQueueSend(hid_ctx->queue, &msg, 0);
}
#endif

static void lvgl_port_tick_increment(void *arg) {
    /* Tell LVGL how many milliseconds have elapsed */
    lv_tick_inc(lvgl_port_timer_period_ms);
}

static esp_err_t lvgl_port_tick_init(void) {
    // Tick interface for LVGL (using esp_timer to generate 2ms periodic event)
    const esp_timer_create_args_t lvgl_tick_timer_args = {
        .callback = &lvgl_port_tick_increment,
        .name = "LVGL tick",
    };
    ESP_RETURN_ON_ERROR(esp_timer_create(&lvgl_tick_timer_args, &lvgl_port_ctx.tick_timer), TAG,
                        "Creating LVGL timer filed!");
    return esp_timer_start_periodic(lvgl_port_ctx.tick_timer, lvgl_port_timer_period_ms * 1000);
}
