#ifndef MCU_POWER_SERVICE_H
#define MCU_POWER_SERVICE_H

#include "esp_err.h"
#include "mcu_link.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    MCU_POWER_SOURCE_UNKNOWN = 0,
    MCU_POWER_SOURCE_BEHAVIOR = 1,
    MCU_POWER_SOURCE_BLE = 2,
    MCU_POWER_SOURCE_WS = 3,
    MCU_POWER_SOURCE_RECOVERY = 4,
} mcu_power_source_t;

esp_err_t mcu_power_service_init(void);
esp_err_t mcu_power_set_5v_enabled(bool enabled, mcu_power_source_t source, uint32_t *out_seq);
esp_err_t mcu_power_service_handle_link_event(const mcu_link_event_t *event);

#ifdef __cplusplus
}
#endif

#endif /* MCU_POWER_SERVICE_H */
