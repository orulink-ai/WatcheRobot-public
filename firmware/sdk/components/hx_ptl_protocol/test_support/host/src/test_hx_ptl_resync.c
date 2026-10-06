#include "hx_ptl_resync.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    hx_ptl_resync_t scan = {0};
    hx_ptl_base_header_t parsed;
    /* Corrupt packet tail remains readable even after SYNC falls. */
    for (unsigned i = 0; i < 23000; ++i)
        assert(!hx_ptl_resync_push(&scan, 0xff, 131072, &parsed));
    const uint8_t invalid[] = {0xc0, 0, 0x5a, 1, 20, 0, 0, 0, 0xc0, 0x5a, 1, 0xff, 0xff, 0xff, 0xff};
    for (size_t i = 0; i < sizeof(invalid); ++i)
        assert(!hx_ptl_resync_push(&scan, invalid[i], 131072, &parsed));
    /* Overlapping magic and a header split between calls must work. */
    assert(!hx_ptl_resync_push(&scan, 0xc0, 131072, &parsed));
    const uint8_t valid[] = {0xc0, 0x5a, 1, 20, 0, 0, 0};
    for (size_t i = 0; i < sizeof(valid); ++i)
        assert(hx_ptl_resync_push(&scan, valid[i], 131072, &parsed) == (i == 6));
    assert(parsed.body_size == 20 && parsed.payload_size == 4);
    assert(memcmp(scan.header, valid, sizeof(valid)) == 0);
    assert(scan.used == 7);
    puts("hx_ptl_resync_tests: PASS");
    return 0;
}

