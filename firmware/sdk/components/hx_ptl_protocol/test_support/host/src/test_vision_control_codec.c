#include "hx_vision_control_codec.h"
#include <stdio.h>
#define CHECK(x)                                                                                                       \
    do {                                                                                                               \
        if (!(x)) {                                                                                                    \
            fprintf(stderr, "failed line %d\n", __LINE__);                                                             \
            return 1;                                                                                                  \
        }                                                                                                              \
    } while (0)
int main(void) {
    uint32_t values[7];
    size_t count = 99;
    CHECK(hx_vision_control_numbers("WV1 STATE 1 0 4294967295 2 4 0 0", "WV1 STATE ", values, 7, &count));
    CHECK(count == 7 && values[2] == UINT32_MAX && values[4] == 4);
    CHECK(!hx_vision_control_numbers("WV1 STATE -1", "WV1 STATE ", values, 7, &count));
    CHECK(!hx_vision_control_numbers("WV1 STATE 4294967296", "WV1 STATE ", values, 7, &count));
    CHECK(!hx_vision_control_numbers("WV1 STATE 1x", "WV1 STATE ", values, 7, &count));
    CHECK(!hx_vision_control_numbers("WV1 STATE 1 ", "WV1 STATE ", values, 7, &count));
    CHECK(!hx_vision_control_numbers("WV2 STATE 1", "WV1 STATE ", values, 7, &count));
    CHECK(!hx_vision_control_numbers("WV1 STATE 1 2 3 4 5 6 7 8", "WV1 STATE ", values, 7, &count));
    puts("PASS: control integers, version, overflow, exact framing and bounds");
    return 0;
}

