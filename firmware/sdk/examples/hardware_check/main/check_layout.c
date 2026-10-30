#include "check_layout.h"
#include <stdint.h>

bool check_layout_zone_hit(bool right, int x, int y) {
    if (x < 0 || x >= CHECK_SCREEN_SIZE || y < CHECK_ZONE_TOP || y >= CHECK_SCREEN_SIZE) return false;
    if (right) x = CHECK_SCREEN_SIZE - 1 - x;
    if (x >= CHECK_ZONE_WIDTH) return false;
    /* Double coordinates avoid floating point and preserve exact pixel symmetry. */
    int32_t cx = 2 * x + 1 - CHECK_SCREEN_SIZE, cy = 2 * y + 1 - CHECK_SCREEN_SIZE;
    if (cx * cx + cy * cy >= CHECK_SCREEN_SIZE * CHECK_SCREEN_SIZE) return false;
    int32_t dx = 2 * x + 1 - 2 * (CHECK_ZONE_WIDTH - CHECK_ZONE_RX);
    int32_t dy = 2 * (y - CHECK_ZONE_TOP) + 1 - 2 * CHECK_ZONE_RY;
    if (dx > 0 && dy < 0) {
        /* int64_t keeps the ellipse predicate safe if geometry later grows. */
        int64_t rx2 = CHECK_ZONE_RX * CHECK_ZONE_RX, ry2 = CHECK_ZONE_RY * CHECK_ZONE_RY;
        if ((int64_t)dx * dx * ry2 + (int64_t)dy * dy * rx2 > 4 * rx2 * ry2) return false;
    }
    return true;
}

bool check_layout_zone_span(int y, int *first, int *last) {
    if (y < CHECK_ZONE_TOP || y >= CHECK_SCREEN_SIZE) return false;
    int start = 0, end = CHECK_ZONE_WIDTH - 1;
    while (start <= end && !check_layout_zone_hit(false, start, y)) start++;
    if (start > end) return false;
    while (end > start && !check_layout_zone_hit(false, end, y)) end--;
    if (first) *first = start;
    if (last) *last = end;
    return true;
}
