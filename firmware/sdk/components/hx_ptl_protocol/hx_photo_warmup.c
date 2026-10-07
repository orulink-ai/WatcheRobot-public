#include "hx_photo_warmup.h"
bool hx_photo_warmup_observe(hx_photo_warmup_t *policy, uint64_t now_us, bool valid_frame) {
    if (!policy || now_us < policy->started_us || !valid_frame)
        return false;
    if (policy->valid_frames < 12U)
        ++policy->valid_frames;
    return policy->valid_frames >= 12U && now_us - policy->started_us >= UINT64_C(1200000);
}

