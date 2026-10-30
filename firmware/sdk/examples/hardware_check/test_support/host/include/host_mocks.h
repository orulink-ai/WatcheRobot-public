#pragma once
#include "check_view.h"
struct host_queue {
    check_input_t items[32];
    unsigned count;
};
extern unsigned host_image_releases;
