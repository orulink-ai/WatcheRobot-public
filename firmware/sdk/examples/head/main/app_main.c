#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "watche_hw_audio.h"
#include "watche_hw_camera.h"
#include "watche_hw_display.h"
static QueueHandle_t actions;
static int16_t pcm[8000]; /* half a second at 16 kHz; no SD/model resources */
static void frame(const uint8_t *jpeg, size_t size, uint32_t timestamp, void *ctx) {
    (void)ctx;
    bool valid = size >= 4 && jpeg[0] == 0xff && jpeg[1] == 0xd8 && jpeg[size - 2] == 0xff && jpeg[size - 1] == 0xd9;
    ESP_LOGI("jpeg", "bytes=%u timestamp=%lu markers=%s", (unsigned)size, (unsigned long)timestamp,
             valid ? "OK" : "INVALID");
    /* Borrowed bytes; copy here if your own task needs the image. */
}
static void clicked(lv_event_t *event) {
    int action = (int)(intptr_t)lv_event_get_user_data(event);
    (void)xQueueSend(actions, &action, 0); /* Never block the LVGL task. */
}
static void head_init(void) {
    actions = xQueueCreate(4, sizeof(int));
    if (!actions)
        return;
    ESP_LOGI("example", "display: %s", esp_err_to_name(watche_hw_display_init()));
    if (watche_hw_display_lock(1000)) {
        const char *labels[] = {"Photo", "Record / play"};
        for (int i = 0; i < 2; ++i) {
            lv_obj_t *button = lv_btn_create(lv_scr_act());
            lv_obj_set_size(button, 180, 60);
            lv_obj_align(button, LV_ALIGN_CENTER, 0, i ? 45 : -45);
            lv_obj_t *label = lv_label_create(button);
            lv_label_set_text(label, labels[i]);
            lv_obj_center(label);
            lv_obj_add_event_cb(button, clicked, LV_EVENT_CLICKED, (void *)(intptr_t)(i + 1));
        }
        watche_hw_display_unlock();
    }
    ESP_LOGI("example", "camera: %s", esp_err_to_name(watche_hw_camera_init()));
    ESP_LOGI("example", "audio: %s", esp_err_to_name(watche_hw_audio_init(16000, 30)));
}
static void head_actions(void) {
    int action;
    if (!actions || xQueueReceive(actions, &action, 0) != pdTRUE)
        return;
    if (action == 1)
        ESP_LOGI("example", "photo: %s", esp_err_to_name(watche_hw_camera_capture(frame, NULL)));
    else {
        esp_err_t ret = watche_hw_audio_read(pcm, sizeof(pcm));
        if (ret == ESP_OK)
            ret = watche_hw_audio_write(pcm, sizeof(pcm));
        ESP_LOGI("example", "PCM record/play: %s", esp_err_to_name(ret));
    }
}
void app_main(void) {
    head_init();
    for (;;) {
        head_actions();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
