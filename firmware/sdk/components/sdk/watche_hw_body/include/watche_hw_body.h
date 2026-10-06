#pragma once
#include "watche_hw_core.h"
#include "mcu_led_service.h"
#include "mcu_motion_service.h"
#include "mcu_sensor_service.h"
#ifdef __cplusplus
extern "C" {
#endif
/* One process-wide body runtime. Init/close/status/control are serialized.
 * No other code may initialize/poll MCU link while this runtime is open.
 * Event consumption happens in application tasks; never a hardware callback.
 * X: 0..180 degrees; Y: 100..140 mechanical limit; duration: 1..65535 ms.
 * Submit returns a sequence, NOT completion. Motion events retain protocol
 * result/reason and final angles in 0.1 degree units as named in their fields. */
esp_err_t watche_hw_body_init(void);
esp_err_t watche_hw_body_close(void);
esp_err_t watche_hw_body_status(watche_hw_status_t *status);
esp_err_t watche_hw_body_move(int x_degrees, int y_degrees, uint32_t duration_ms, uint32_t *sequence);
esp_err_t watche_hw_body_stop(void);
/* Read-only query; POSITION payload is mcu_motion_servo_feedback_t (x10 units). */
esp_err_t watche_hw_body_request_position(void);
/* LED request uses existing zone/effect enums, RGB/brightness 0..255. */
esp_err_t watche_hw_body_light(const mcu_led_request_t *request);
esp_err_t watche_hw_body_next_event(watche_hw_event_t *event);
typedef void (*watche_hw_body_event_cb_t)(const watche_hw_event_t *event, void *context);
/* Up to four subscribers. One application task owns subscription changes and
 * dispatch. dispatch consumes the same FIFO as next_event: choose one mode.
 * Callbacks run synchronously in the dispatch caller, outside the MCU mutex;
 * event is borrowed until callback returns. Keep context alive until dispatch
 * returns, even after unsubscribe. Callbacks must not recursively dispatch,
 * block on audio, or close the runtime. Init/close clear subscriptions. */
esp_err_t watche_hw_body_subscribe(watche_hw_body_event_cb_t callback, void *context);
esp_err_t watche_hw_body_unsubscribe(watche_hw_body_event_cb_t callback, void *context);
esp_err_t watche_hw_body_dispatch_events(uint32_t max_events, uint32_t *dispatched);
esp_err_t watche_hw_body_dropped_events(uint32_t *dropped);
#ifdef __cplusplus
}
#endif

