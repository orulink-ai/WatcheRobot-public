#pragma once
#include "mcu_sensor_service.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Stateless common runtime operations. The caller owns polling and policy.
 * Touch is copied immediately after applying THIS frame, before another poll. */
esp_err_t mcu_runtime_complete_baseline(const mcu_link_event_t *event);
esp_err_t mcu_runtime_dispatch(const mcu_link_event_t *event, bool *overwrote);
#ifdef __cplusplus
}
#endif

