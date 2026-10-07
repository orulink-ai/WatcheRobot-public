#include "motion_arbiter.h"

#if !defined(MOTION_ARBITER_HOST_TEST)
#include "freertos/FreeRTOS.h"
#endif

static volatile motion_arbiter_owner_t s_owner = MOTION_ARBITER_OWNER_NONE;

#if !defined(MOTION_ARBITER_HOST_TEST)
static portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;
#define MOTION_ARBITER_LOCK() taskENTER_CRITICAL(&s_lock)
#define MOTION_ARBITER_UNLOCK() taskEXIT_CRITICAL(&s_lock)
#else
#define MOTION_ARBITER_LOCK() ((void)0)
#define MOTION_ARBITER_UNLOCK() ((void)0)
#endif

static bool motion_arbiter_owner_valid(motion_arbiter_owner_t owner) {
    return owner > MOTION_ARBITER_OWNER_NONE && owner <= MOTION_ARBITER_OWNER_RECOVERY;
}

esp_err_t motion_arbiter_acquire(motion_arbiter_owner_t owner) {
    if (!motion_arbiter_owner_valid(owner)) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t result = ESP_ERR_INVALID_STATE;
    MOTION_ARBITER_LOCK();
    if (s_owner <= owner) {
        s_owner = owner;
        result = ESP_OK;
    }
    MOTION_ARBITER_UNLOCK();
    return result;
}

esp_err_t motion_arbiter_release(motion_arbiter_owner_t owner) {
    if (!motion_arbiter_owner_valid(owner)) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t result;
    MOTION_ARBITER_LOCK();
    if (s_owner == owner) {
        s_owner = MOTION_ARBITER_OWNER_NONE;
        result = ESP_OK;
    } else {
        result = s_owner == MOTION_ARBITER_OWNER_NONE ? ESP_OK : ESP_ERR_INVALID_STATE;
    }
    MOTION_ARBITER_UNLOCK();
    return result;
}

bool motion_arbiter_can_submit(motion_arbiter_owner_t owner) {
    if (!motion_arbiter_owner_valid(owner)) {
        return false;
    }
    MOTION_ARBITER_LOCK();
    const motion_arbiter_owner_t current = s_owner;
    MOTION_ARBITER_UNLOCK();
    return current == MOTION_ARBITER_OWNER_NONE || current <= owner;
}

motion_arbiter_owner_t motion_arbiter_current_owner(void) {
    MOTION_ARBITER_LOCK();
    const motion_arbiter_owner_t owner = s_owner;
    MOTION_ARBITER_UNLOCK();
    return owner;
}

void motion_arbiter_reset(void) {
    MOTION_ARBITER_LOCK();
    s_owner = MOTION_ARBITER_OWNER_NONE;
    MOTION_ARBITER_UNLOCK();
}

