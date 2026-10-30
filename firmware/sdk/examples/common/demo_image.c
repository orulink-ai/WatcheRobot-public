#include "demo_image.h"
#include "demo_state.h"
#include "esp_heap_caps.h"
#include "jpeg_decoder.h"
#include "watcher_hw_camera.h"
#include <string.h>

bool demo_jpeg_valid(const uint8_t *jpeg, size_t n) {
    return jpeg && n >= 4 && n <= DEMO_JPEG_LIMIT && jpeg[0] == 0xff && jpeg[1] == 0xd8 && jpeg[n - 2] == 0xff && jpeg[n - 1] == 0xd9;
}
void demo_image_release(demo_image_t *image) {
    if (image) { heap_caps_free(image->pixels); memset(image, 0, sizeof(*image)); }
}
esp_err_t demo_image_decode(const uint8_t *jpeg, size_t bytes, bool swap, demo_image_t *out) {
    if (!out) return ESP_ERR_INVALID_ARG;
    memset(out, 0, sizeof(*out));
    if (!demo_jpeg_valid(jpeg, bytes)) return ESP_ERR_INVALID_RESPONSE;
    esp_jpeg_image_cfg_t cfg = {.indata = (uint8_t *)jpeg, .indata_size = bytes, .out_format = JPEG_IMAGE_FORMAT_RGB565};
    esp_jpeg_image_output_t info = {0}, decoded = {0};
    esp_err_t ret = esp_jpeg_get_image_info(&cfg, &info);
    unsigned w, h, divisor;
    if (ret != ESP_OK) return ret;
    if (!demo_preview_size(info.width, info.height, &w, &h, &divisor)) return ESP_ERR_NOT_SUPPORTED;
    cfg.out_scale = divisor == 1 ? JPEG_IMAGE_SCALE_0 : divisor == 2 ? JPEG_IMAGE_SCALE_1_2 : divisor == 4 ? JPEG_IMAGE_SCALE_1_4 : JPEG_IMAGE_SCALE_1_8;
    cfg.outbuf_size = w * h * sizeof(uint16_t);
    cfg.outbuf = heap_caps_malloc(cfg.outbuf_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!cfg.outbuf) return ESP_ERR_NO_MEM;
    cfg.flags.swap_color_bytes = swap;
    ret = esp_jpeg_decode(&cfg, &decoded);
    if (ret == ESP_OK && (decoded.width != w || decoded.height != h || decoded.output_len != cfg.outbuf_size)) ret = ESP_ERR_INVALID_RESPONSE;
    if (ret != ESP_OK) { heap_caps_free(cfg.outbuf); return ret; }
    *out = (demo_image_t){cfg.outbuf, w, h, decoded.output_len};
    return ESP_OK;
}
typedef struct { uint8_t *jpeg; size_t bytes; esp_err_t error; } one_shot_t;
static void copy_frame(const uint8_t *jpeg, size_t bytes, uint32_t timestamp, void *context) {
    (void)timestamp;
    one_shot_t *shot = context;
    if (shot->jpeg) return;
    if (!demo_jpeg_valid(jpeg, bytes)) { shot->error = ESP_ERR_INVALID_RESPONSE; return; }
    shot->jpeg = heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!shot->jpeg) { shot->error = ESP_ERR_NO_MEM; return; }
    memcpy(shot->jpeg, jpeg, bytes);
    shot->bytes = bytes; shot->error = ESP_OK;
}
esp_err_t demo_image_capture(bool swap, demo_image_t *out) {
    if (!out) return ESP_ERR_INVALID_ARG;
    memset(out, 0, sizeof(*out));
    one_shot_t shot = {.error = ESP_ERR_NOT_FOUND};
    esp_err_t ret = watcher_hw_camera_capture(copy_frame, &shot);
    if (ret == ESP_OK) ret = shot.error;
    if (ret == ESP_OK) ret = demo_image_decode(shot.jpeg, shot.bytes, swap, out);
    heap_caps_free(shot.jpeg);
    return ret;
}
