#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
bool hx_vision_control_numbers(const char *line, const char *prefix, uint32_t *values, size_t capacity, size_t *count);

