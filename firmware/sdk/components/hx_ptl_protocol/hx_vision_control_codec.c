#include "hx_vision_control_codec.h"
#include <string.h>
bool hx_vision_control_numbers(const char *line, const char *prefix, uint32_t *values, size_t capacity, size_t *count) {
    if (!line || !prefix || !values || !count)
        return false;
    const size_t prefix_size = strlen(prefix);
    if (strncmp(line, prefix, prefix_size) != 0)
        return false;
    const char *at = line + prefix_size;
    size_t used = 0;
    for (;;) {
        if (used == capacity || *at < '0' || *at > '9')
            return false;
        uint32_t value = 0;
        do {
            const uint32_t digit = (uint32_t)(*at++ - '0');
            if (value > (UINT32_MAX - digit) / 10U)
                return false;
            value = value * 10U + digit;
        } while (*at >= '0' && *at <= '9');
        values[used++] = value;
        if (*at == 0) {
            *count = used;
            return true;
        }
        if (*at++ != ' ')
            return false;
    }
}

