#include "hx_ptl_resync.h"
#include <string.h>

bool hx_ptl_resync_push(hx_ptl_resync_t *scan, uint8_t byte, size_t maximum, hx_ptl_base_header_t *parsed) {
    if (scan == NULL || parsed == NULL)
        return false;
    if (scan->used == sizeof(scan->header)) {
        memmove(scan->header, scan->header + 1, sizeof(scan->header) - 1);
        --scan->used;
    }
    scan->header[scan->used++] = byte;
    return scan->used == sizeof(scan->header) &&
           hx_ptl_parse_base_header(scan->header, sizeof(scan->header), maximum, parsed) == HX_PTL_OK;
}

