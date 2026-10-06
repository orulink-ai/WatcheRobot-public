#include "hx_photo_warmup.h"
#include <stdio.h>
#define CHECK(x)                                                                                                       \
    do {                                                                                                               \
        if (!(x)) {                                                                                                    \
            fprintf(stderr, "failed line %d\n", __LINE__);                                                             \
            return 1;                                                                                                  \
        }                                                                                                              \
    } while (0)
int main(void) {
    hx_photo_warmup_t policy = {10000000, 0};
    for (unsigned i = 0; i < 30; ++i)
        CHECK(!hx_photo_warmup_observe(&policy, 10000000 + i * 30000, true));
    CHECK(hx_photo_warmup_observe(&policy, 11200000, true));
    policy = (hx_photo_warmup_t){10000000, 0};
    for (unsigned i = 0; i < 11; ++i)
        CHECK(!hx_photo_warmup_observe(&policy, 12000000 + i, true));
    CHECK(!hx_photo_warmup_observe(&policy, 12001000, false));
    CHECK(hx_photo_warmup_observe(&policy, 12002000, true));
    CHECK(!hx_photo_warmup_observe(&policy, 9999999, true));
    policy = (hx_photo_warmup_t){UINT64_C(5000000000), 0};
    CHECK(!hx_photo_warmup_observe(&policy, UINT64_C(5001200000), true));
    puts("PASS: cold-start frame rejection, time and frame gates, invalid frames, monotonic clock");
    return 0;
}

