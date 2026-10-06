#include "hx_ptl_transport.h"

static hx_ptl_transport_result_t read_exact(const hx_ptl_transport_io_t *io, uint8_t *data, size_t size) {
    return io->read(io->context, data, size) == 0 ? HX_PTL_TRANSPORT_OK : HX_PTL_TRANSPORT_ERR_IO;
}
static hx_ptl_transport_result_t scan_byte(const hx_ptl_transport_io_t *io, uint8_t expected, uint8_t *value) {
    for (size_t attempt = 0; attempt < HX_PTL_TRANSPORT_MAGIC_SCAN_LIMIT; ++attempt) {
        hx_ptl_transport_result_t result = read_exact(io, value, 1U);
        if (result != HX_PTL_TRANSPORT_OK) {
            return result;
        }
        if (*value == expected) {
            return HX_PTL_TRANSPORT_OK;
        }
    }
    return HX_PTL_TRANSPORT_ERR_MAGIC_TIMEOUT;
}
hx_ptl_transport_result_t hx_ptl_transport_read_base_header(const hx_ptl_transport_io_t *io,
                                                            uint8_t header[HX_PTL_BASE_HEADER_SIZE]) {
    hx_ptl_transport_result_t result;

    if (io == NULL || io->read == NULL || header == NULL) {
        return HX_PTL_TRANSPORT_ERR_ARGUMENT;
    }
    result = scan_byte(io, HX_PTL_MAGIC_0, &header[0]);
    if (result != HX_PTL_TRANSPORT_OK) {
        return result;
    }
    result = scan_byte(io, HX_PTL_MAGIC_1, &header[1]);
    if (result != HX_PTL_TRANSPORT_OK) {
        return result;
    }
    return read_exact(io, &header[2], HX_PTL_BASE_HEADER_SIZE - 2U);
}

hx_ptl_transport_result_t hx_ptl_transport_read_body(const hx_ptl_transport_io_t *io, uint8_t *body, size_t body_size) {
    size_t offset = 0U;

    if (io == NULL || io->read == NULL || io->delay == NULL || body == NULL || body_size == 0U) {
        return HX_PTL_TRANSPORT_ERR_ARGUMENT;
    }

    io->delay(io->context, HX_PTL_TRANSPORT_BODY_PRE_DELAY_MS);
    while (offset < body_size) {
        size_t chunk = body_size - offset;
        if (chunk > HX_PTL_TRANSPORT_BODY_CHUNK_SIZE) {
            chunk = HX_PTL_TRANSPORT_BODY_CHUNK_SIZE;
        }
        if (read_exact(io, &body[offset], chunk) != HX_PTL_TRANSPORT_OK) {
            return HX_PTL_TRANSPORT_ERR_IO;
        }
        offset += chunk;
        if (offset < body_size) {
            io->delay(io->context, HX_PTL_TRANSPORT_BODY_INTER_CHUNK_DELAY_MS);
        }
    }
    return HX_PTL_TRANSPORT_OK;
}

