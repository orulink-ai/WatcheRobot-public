#include "sscma_client_command.h"

#include <string.h>

#define SSCMA_COMMAND_PREFIX "AT+"
#define SSCMA_COMMAND_NAME_MAX_LENGTH 32u

void sscma_client_command_name(const char *command, char *out_name, size_t out_size) {
    if (out_name == NULL || out_size == 0u) {
        return;
    }
    out_name[0] = '\0';
    if (command == NULL) {
        return;
    }

    const char *cursor = command;
    if (strncmp(cursor, SSCMA_COMMAND_PREFIX, strlen(SSCMA_COMMAND_PREFIX)) == 0) {
        cursor += strlen(SSCMA_COMMAND_PREFIX);
    }

    size_t length = 0u;
    while (cursor[length] != '\0' && cursor[length] != '?' && cursor[length] != '=' && cursor[length] != '\r' &&
           cursor[length] != '\n' && length + 1u < out_size) {
        out_name[length] = cursor[length];
        ++length;
    }
    out_name[length] = '\0';
}

bool sscma_client_command_name_equal(const char *left, const char *right) {
    char normalized_left[SSCMA_COMMAND_NAME_MAX_LENGTH];
    char normalized_right[SSCMA_COMMAND_NAME_MAX_LENGTH];

    sscma_client_command_name(left, normalized_left, sizeof(normalized_left));
    sscma_client_command_name(right, normalized_right, sizeof(normalized_right));
    return normalized_left[0] != '\0' && strcmp(normalized_left, normalized_right) == 0;
}

