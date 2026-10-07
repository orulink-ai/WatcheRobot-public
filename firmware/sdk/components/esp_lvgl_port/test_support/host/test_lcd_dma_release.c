#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_ERR_INVALID_STATE 1
typedef struct {
    void *buf1, *buf2;
    int flushing;
} draw_buffer_t;
typedef struct {
    draw_buffer_t *draw_buf;
    void *user_data;
} lv_disp_drv_t;
typedef struct {
    lv_disp_drv_t *driver;
} lv_disp_t;
typedef struct {
    void *trans_buffer;
    void *io_handle;
} lvgl_port_display_ctx_t;
typedef struct {
    int unused;
} esp_lcd_panel_io_callbacks_t;
static int io_error, drain_count, detach_count;
static esp_err_t esp_lcd_panel_io_tx_param(void *io, int command, const void *data, size_t size) {
    (void)io;
    (void)command;
    (void)data;
    (void)size;
    ++drain_count;
    return io_error == 1 ? ESP_ERR_INVALID_STATE : ESP_OK;
}
static esp_err_t esp_lcd_panel_io_register_event_callbacks(void *io, const esp_lcd_panel_io_callbacks_t *callbacks,
                                                           void *context) {
    (void)io;
    (void)callbacks;
    (void)context;
    ++detach_count;
    return io_error == 2 ? ESP_ERR_INVALID_STATE : ESP_OK;
}
static struct {
    bool panel_direct_draw_pending;
} lvgl_port_ctx;
static int frees, removes, locks;
static bool lock_available = true;
static bool lvgl_port_lock(int timeout) {
    (void)timeout;
    if (!lock_available)
        return false;
    ++locks;
    return true;
}
static void lvgl_port_unlock(void) {
    --locks;
}
static void lv_disp_remove(lv_disp_t *disp) {
    (void)disp;
    ++removes;
}
static void tracked_free(void *p) {
    if (p)
        ++frees;
    free(p);
}
#define free tracked_free
#include "lcd_remove_disp.inc"
#undef free
#define CHECK(c)                                                                                                       \
    do {                                                                                                               \
        if (!(c))                                                                                                      \
            return __LINE__;                                                                                           \
    } while (0)
int main(void) {
    lvgl_port_display_ctx_t *ctx = calloc(1, sizeof(*ctx));
    ctx->trans_buffer = malloc(32);
    draw_buffer_t *buf = calloc(1, sizeof(*buf));
    buf->buf1 = malloc(32);
    buf->buf2 = malloc(32);
    lv_disp_drv_t driver = {.draw_buf = buf, .user_data = ctx};
    lv_disp_t disp = {.driver = &driver};
    lock_available = false;
    CHECK(lvgl_port_remove_disp(&disp) == ESP_ERR_INVALID_STATE && !frees && !removes && !locks);
    lock_available = true;
    lvgl_port_ctx.panel_direct_draw_pending = true;
    CHECK(lvgl_port_remove_disp(&disp) == ESP_ERR_INVALID_STATE && !frees && !removes && !locks);
    lvgl_port_ctx.panel_direct_draw_pending = false;
    buf->flushing = 1;
    CHECK(lvgl_port_remove_disp(&disp) == ESP_ERR_INVALID_STATE && !frees && !removes && !locks);
    buf->flushing = 0;
    io_error = 1;
    CHECK(lvgl_port_remove_disp(&disp) == ESP_ERR_INVALID_STATE && !frees && !removes && !locks);
    io_error = 2;
    CHECK(lvgl_port_remove_disp(&disp) == ESP_ERR_INVALID_STATE && !frees && !removes && !locks);
    io_error = 0;
    CHECK(lvgl_port_remove_disp(&disp) == ESP_OK && frees == 5 && removes == 1 && !locks);
    CHECK(drain_count == 3 && detach_count == 2);
    puts("LCD removal retains all buffers while flushing/DMA pending; successful retry releases exactly once");
    return 0;
}
