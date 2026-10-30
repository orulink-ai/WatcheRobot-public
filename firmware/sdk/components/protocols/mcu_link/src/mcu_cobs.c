#include "mcu_cobs.h"

size_t mcu_cobs_max_encoded_size(size_t input_len) {
    return input_len + (input_len / 254u) + 1u;
}

esp_err_t mcu_cobs_encode(const uint8_t *input, size_t input_len, uint8_t *output, size_t output_len,
                          size_t *encoded_len) {
    if (output == NULL || encoded_len == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    *encoded_len = 0;

    if (input_len > 0u && input == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (output_len < mcu_cobs_max_encoded_size(input_len)) {
        return ESP_ERR_NO_MEM;
    }

    size_t code_index = 0u;
    size_t write_index = 1u;
    uint8_t code = 1u;

    for (size_t read_index = 0u; read_index < input_len; ++read_index) {
        const uint8_t byte = input[read_index];

        if (byte == 0u) {
            output[code_index] = code;
            code_index = write_index++;
            code = 1u;
            continue;
        }

        output[write_index++] = byte;
        ++code;

        if (code == 0xFFu) {
            output[code_index] = code;
            code_index = write_index++;
            code = 1u;
        }
    }

    output[code_index] = code;
    *encoded_len = write_index;
    return ESP_OK;
}

esp_err_t mcu_cobs_decode(const uint8_t *input, size_t input_len, uint8_t *output, size_t output_len,
                          size_t *decoded_len) {
    if (input == NULL || output == NULL || decoded_len == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    *decoded_len = 0u;

    size_t read_index = 0u;
    size_t write_index = 0u;

    while (read_index < input_len) {
        const uint8_t code = input[read_index++];

        if (code == 0u) {
            return ESP_ERR_INVALID_ARG;
        }

        for (uint8_t i = 1u; i < code; ++i) {
            if (read_index >= input_len) {
                return ESP_ERR_INVALID_ARG;
            }
            if (write_index >= output_len) {
                return ESP_ERR_NO_MEM;
            }
            output[write_index++] = input[read_index++];
        }

        if (code != 0xFFu && read_index < input_len) {
            if (write_index >= output_len) {
                return ESP_ERR_NO_MEM;
            }
            output[write_index++] = 0u;
        }
    }

    *decoded_len = write_index;
    return ESP_OK;
}
