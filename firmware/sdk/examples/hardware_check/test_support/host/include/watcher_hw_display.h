#pragma once
#include <stdbool.h>
#include <stdint.h>
bool watcher_hw_display_lock(uint32_t timeout_ms);
void watcher_hw_display_unlock(void);
