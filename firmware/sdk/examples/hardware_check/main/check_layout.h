#pragma once
#include <stdbool.h>

/* Example-only geometry, in logical LCD pixels. Shared by drawing and hit testing. */
enum {
    CHECK_SCREEN_SIZE = 412,
    CHECK_ZONE_TOP = 288,
    CHECK_ZONE_WIDTH = 178,
    CHECK_ZONE_HEIGHT = 124,
    CHECK_ZONE_RX = 112,
    CHECK_ZONE_RY = 100,
    CHECK_LABEL_LEFT = 108,
    CHECK_LABEL_Y = 340,
    CHECK_RESULT_Y = 188
};

/* Pixel-centre containment. Circle exterior, neutral gap and inner arc are excluded. */
bool check_layout_zone_hit(bool right, int x, int y);
/* Left-zone scanline, inclusive absolute x coordinates. Outputs are optional. */
bool check_layout_zone_span(int y, int *first, int *last);
