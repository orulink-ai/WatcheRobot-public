#pragma once

#include "esp_err.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    MOTION_ARBITER_OWNER_NONE = 0,
    MOTION_ARBITER_OWNER_BEHAVIOR = 1,
    MOTION_ARBITER_OWNER_MANUAL = 2,
    MOTION_ARBITER_OWNER_FACE_TRACKING = 3,
    MOTION_ARBITER_OWNER_RECOVERY = 4,
} motion_arbiter_owner_t;

esp_err_t motion_arbiter_acquire(motion_arbiter_owner_t owner);
esp_err_t motion_arbiter_release(motion_arbiter_owner_t owner);
bool motion_arbiter_can_submit(motion_arbiter_owner_t owner);
motion_arbiter_owner_t motion_arbiter_current_owner(void);
void motion_arbiter_reset(void);

#ifdef __cplusplus
}
#endif

