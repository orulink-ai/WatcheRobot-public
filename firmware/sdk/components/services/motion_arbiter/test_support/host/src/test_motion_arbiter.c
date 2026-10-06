#include "motion_arbiter.h"

#include <assert.h>
#include <stdio.h>

int main(void) {
    motion_arbiter_reset();
    assert(motion_arbiter_acquire(MOTION_ARBITER_OWNER_FACE_TRACKING) == ESP_OK);
    assert(!motion_arbiter_can_submit(MOTION_ARBITER_OWNER_BEHAVIOR));
    assert(!motion_arbiter_can_submit(MOTION_ARBITER_OWNER_MANUAL));
    assert(motion_arbiter_can_submit(MOTION_ARBITER_OWNER_FACE_TRACKING));
    assert(motion_arbiter_can_submit(MOTION_ARBITER_OWNER_RECOVERY));
    assert(motion_arbiter_acquire(MOTION_ARBITER_OWNER_MANUAL) == ESP_ERR_INVALID_STATE);
    assert(motion_arbiter_acquire(MOTION_ARBITER_OWNER_RECOVERY) == ESP_OK);
    assert(motion_arbiter_current_owner() == MOTION_ARBITER_OWNER_RECOVERY);
    assert(motion_arbiter_release(MOTION_ARBITER_OWNER_FACE_TRACKING) == ESP_ERR_INVALID_STATE);
    assert(motion_arbiter_release(MOTION_ARBITER_OWNER_RECOVERY) == ESP_OK);
    assert(motion_arbiter_current_owner() == MOTION_ARBITER_OWNER_NONE);
    puts("motion_arbiter_host_tests: PASS");
    return 0;
}

