#include "mcu_runtime.h"
#include "mcu_link_bootstrap.h"
#include "mcu_motion_service.h"
#include "mcu_led_service.h"
#include "mcu_power_service.h"
esp_err_t mcu_runtime_complete_baseline(const mcu_link_event_t *event) {
    if (event == NULL) return ESP_ERR_INVALID_ARG;
    if (event->type != MCU_LINK_RX_EVENT_HELLO_RSP) return ESP_ERR_NOT_FOUND;
    mcu_link_t *link = mcu_link_bootstrap_get_link();
    if (link == NULL || !mcu_link_is_link_ready(link)) return ESP_ERR_INVALID_STATE;
    if (mcu_link_is_ready(link)) return ESP_OK;
    /* Same explicit-default baseline as the official runtime. No replay of
     * previous commands, and no claim that a snapshot has been restored. */
    return mcu_link_mark_baseline_synced(link);
}
esp_err_t mcu_runtime_dispatch(const mcu_link_event_t *event, bool *overwrote) {
    if (event == NULL) return ESP_ERR_INVALID_ARG;
    (void)mcu_motion_service_handle_link_event(event);
    (void)mcu_led_service_handle_link_event(event);
    (void)mcu_power_service_handle_link_event(event);
    return mcu_sensor_service_handle_link_event(event, overwrote);
}

