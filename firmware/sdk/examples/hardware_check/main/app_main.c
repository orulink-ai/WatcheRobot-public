#include "check_app.h"
#include "esp_log.h"

void app_main(void) {
    esp_err_t error = check_app_start();
    ESP_LOGI("hardware_check", "start=%s", esp_err_to_name(error));
}
