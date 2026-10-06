#pragma once

#include <stddef.h>
#include <stdint.h>

#include "hx_ptl_protocol.h"

#ifdef __cplusplus
extern "C" {
#endif
#define HX_PTL_TRANSPORT_MAGIC_SCAN_LIMIT 10000U
#define HX_PTL_TRANSPORT_BODY_CHUNK_SIZE 65000U
#define HX_PTL_TRANSPORT_BODY_PRE_DELAY_MS 10U
#define HX_PTL_TRANSPORT_BODY_INTER_CHUNK_DELAY_MS 30U

typedef int (*hx_ptl_transport_read_fn)(void *context, uint8_t *data, size_t size);
typedef void (*hx_ptl_transport_delay_fn)(void *context, uint32_t delay_ms);

typedef struct {
    hx_ptl_transport_read_fn read;
    hx_ptl_transport_delay_fn delay;
    void *context;
} hx_ptl_transport_io_t;

typedef enum {
    HX_PTL_TRANSPORT_OK = 0,
    HX_PTL_TRANSPORT_ERR_ARGUMENT,
    HX_PTL_TRANSPORT_ERR_IO,
    HX_PTL_TRANSPORT_ERR_MAGIC_TIMEOUT,
} hx_ptl_transport_result_t;

hx_ptl_transport_result_t hx_ptl_transport_read_base_header(const hx_ptl_transport_io_t *io,
                                                            uint8_t header[HX_PTL_BASE_HEADER_SIZE]);

hx_ptl_transport_result_t hx_ptl_transport_read_body(const hx_ptl_transport_io_t *io, uint8_t *body, size_t body_size);

#ifdef __cplusplus
}
#endif

