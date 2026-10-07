#pragma once
#include "hx_ptl_protocol.h"
#include <stdbool.h>

/* Sliding header search retains at most seven bytes across bounded reads. */
typedef struct {
    uint8_t header[HX_PTL_BASE_HEADER_SIZE];
    size_t used;
} hx_ptl_resync_t;
bool hx_ptl_resync_push(hx_ptl_resync_t *scan, uint8_t byte, size_t maximum, hx_ptl_base_header_t *parsed);

