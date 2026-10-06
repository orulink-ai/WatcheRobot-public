#pragma once
#include <stdbool.h>
#include <stdint.h>
typedef struct {
    uint64_t started_us;
    unsigned valid_frames;
} hx_photo_warmup_t;
bool hx_photo_warmup_observe(hx_photo_warmup_t *policy, uint64_t now_us, bool valid_frame);

