#include "check_layout.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
    unsigned area = 0;
    for (int y = 0; y < CHECK_SCREEN_SIZE; y++) {
        int first = -1, last = -1;
        bool span = check_layout_zone_span(y, &first, &last);
        for (int x = 0; x < CHECK_SCREEN_SIZE; x++) {
            bool hit = check_layout_zone_hit(false, x, y);
            assert(hit == check_layout_zone_hit(true, CHECK_SCREEN_SIZE - 1 - x, y));
            assert(!hit || !check_layout_zone_hit(true, x, y));
            assert(hit == (span && x >= first && x <= last));
            if (hit) area++;
        }
    }
    assert(area == 11043);
    assert(area > 2 * 100 * 52);
    assert(check_layout_zone_hit(false, 108, 340));
    assert(check_layout_zone_hit(true, 304, 340));
    assert(!check_layout_zone_hit(false, 174, 290));
    assert(!check_layout_zone_hit(true, 237, 290));
    assert(!check_layout_zone_hit(false, 206, 340));
    assert(!check_layout_zone_hit(true, 206, 340));
    assert(!check_layout_zone_hit(false, 5, 400));
    assert(!check_layout_zone_hit(false, -1, 340));
    assert(!check_layout_zone_hit(true, 412, 340));
    assert(!check_layout_zone_hit(false, 108, 412));
    assert(!check_layout_zone_span(-1, NULL, NULL));
    assert(!check_layout_zone_span(287, NULL, NULL));
    assert(check_layout_zone_span(340, NULL, NULL));
    puts("check_layout: mirrored pixel-accurate draw/hit areas and neutral gap passed");
    return 0;
}
