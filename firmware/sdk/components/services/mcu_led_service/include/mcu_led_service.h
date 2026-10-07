#ifndef MCU_LED_SERVICE_H
#define MCU_LED_SERVICE_H

#include "esp_err.h"
#include "mcu_link.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    MCU_LED_MODE_OFF = 0,
    MCU_LED_MODE_STATIC = 1,
    MCU_LED_MODE_EFFECT = 2,
} mcu_led_mode_t;

typedef enum {
    MCU_LED_EFFECT_BLINK = 1,
    MCU_LED_EFFECT_BREATHING = 2,
    MCU_LED_EFFECT_RAINBOW = 3,
    MCU_LED_EFFECT_STATUS_PULSE = 4,
} mcu_led_effect_t;

typedef enum {
    MCU_LED_ZONE_ALL = 0,
    MCU_LED_ZONE_SIDE = 1,
    MCU_LED_ZONE_BOTTOM = 2,
} mcu_led_zone_t;

typedef struct {
    mcu_led_mode_t mode;
    mcu_led_zone_t zone;
    uint8_t primary_red;
    uint8_t primary_green;
    uint8_t primary_blue;
    uint8_t secondary_red;
    uint8_t secondary_green;
    uint8_t secondary_blue;
    uint8_t brightness;
    uint8_t effect_id;
    uint16_t period_ms; /* effect mode -> period_ms; static mode keeps current LED count */
    uint16_t repeat_count;
} mcu_led_request_t;

esp_err_t mcu_led_service_init(void);
esp_err_t mcu_led_service_enter_shutdown_mode(void);
esp_err_t mcu_led_submit_boot_green_baseline(void);
esp_err_t mcu_led_submit(const mcu_led_request_t *request);
esp_err_t mcu_led_service_get_last_request(mcu_led_request_t *out_request);
esp_err_t mcu_led_service_handle_link_event(const mcu_link_event_t *event);

#ifdef __cplusplus
}
#endif

#endif /* MCU_LED_SERVICE_H */

