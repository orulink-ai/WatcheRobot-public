#pragma once

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void sscma_client_command_name(const char *command, char *out_name, size_t out_size);
bool sscma_client_command_name_equal(const char *left, const char *right);

#ifdef __cplusplus
}
#endif

