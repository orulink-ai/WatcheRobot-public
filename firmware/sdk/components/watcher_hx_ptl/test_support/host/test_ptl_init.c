#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(c)                                                                                                       \
    do {                                                                                                               \
        if (!(c)) {                                                                                                    \
            fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #c);                                                       \
            exit(1);                                                                                                   \
        }                                                                                                              \
    } while (0)
#define ESP_LOGI(...) ((void)0)
#define ESP_RETURN_ON_FALSE(c, e, ...)                                                                                 \
    do {                                                                                                               \
        if (!(c))                                                                                                      \
            return (e);                                                                                                \
    } while (0)
#define ESP_RETURN_ON_ERROR(call, ...)                                                                                 \
    do {                                                                                                               \
        int result_ = (call);                                                                                          \
        if (result_ != ESP_OK)                                                                                         \
            return result_;                                                                                            \
    } while (0)
#define pdMS_TO_TICKS(ms) (ms)
enum {
    ESP_OK = 0,
    ESP_ERR_INVALID_STATE = -1,
    ESP_ERR_NO_MEM = -2,
    ESP_FAIL = -3,
    BSP_PWR_AI_CHIP = 2048,
    BSP_SSCMA_CLIENT_RST = 128,
    IO_EXPANDER_INPUT = 0,
    IO_EXPANDER_OUTPUT = 1,
    WATCHER_HX_PTL_SPI_HOST = 2,
    WATCHER_HX_PTL_SPI_CS = 21,
    WATCHER_HX_PTL_SPI_HZ = 12000000,
    WATCHER_HX_PTL_SYNC_MASK = 64,
    WATCHER_HX_PTL_BODY_CAPACITY = 131104,
    MALLOC_CAP_SPIRAM = 1,
    MALLOC_CAP_8BIT = 2
};
typedef int esp_err_t;
typedef struct {
    int clock_speed_hz, mode, spics_io_num, queue_size;
} spi_device_interface_config_t;
static struct {
    void *expander, *device, *body;
    int sequence, scanner;
    struct {
        int spi_hz;
    } stats;
    bool initialized, resynchronizing;
} s_ctx;
static int expander_token, device_token, fail_step;
static unsigned steps, directions, reset_releases, bus_adds, allocations, deinitializations;
static bool board_available = true, fail_allocation, reset_driven, powered;
static int step(void) {
    return ++steps == (unsigned)fail_step ? ESP_FAIL : ESP_OK;
}
static void *bsp_io_expander_init(void) {
    return board_available ? &expander_token : NULL;
}
static int bsp_exp_io_set_level(unsigned mask, int level) {
    CHECK(mask == BSP_PWR_AI_CHIP || mask == BSP_SSCMA_CLIENT_RST);
    if (mask == BSP_SSCMA_CLIENT_RST && !reset_driven)
        return ESP_ERR_INVALID_STATE;
    int result = step();
    if (result == ESP_OK && mask == BSP_PWR_AI_CHIP)
        powered = level != 0;
    if (result == ESP_OK && mask == BSP_SSCMA_CLIENT_RST && level == 1)
        ++reset_releases;
    return result;
}
static int esp_io_expander_set_dir(void *expander, unsigned mask, int direction) {
    CHECK(expander == &expander_token);
    CHECK((mask == WATCHER_HX_PTL_SYNC_MASK && direction == IO_EXPANDER_INPUT) ||
          (mask == BSP_SSCMA_CLIENT_RST && direction == IO_EXPANDER_OUTPUT));
    ++directions;
    int result = step();
    if (result == ESP_OK && mask == BSP_SSCMA_CLIENT_RST)
        reset_driven = true;
    return result;
}
static void vTaskDelay(unsigned ticks) {
    CHECK(ticks > 0U);
}
static int bsp_spi_bus_init(void) {
    return step();
}
static int spi_bus_add_device(int host, const spi_device_interface_config_t *config, void **device) {
    CHECK(host == 2 && config->clock_speed_hz == 12000000 && config->queue_size == 1);
    CHECK(powered && reset_driven && reset_releases == 1U); /* Cold camera must be out of reset. */
    ++bus_adds;
    int result = step();
    if (result == ESP_OK)
        *device = &device_token;
    return result;
}
static void *heap_caps_malloc(size_t bytes, unsigned caps) {
    CHECK(bytes == WATCHER_HX_PTL_BODY_CAPACITY && caps == (MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    ++allocations;
    return fail_allocation ? NULL : malloc(bytes);
}
static void hx_media_sequence_tracker_reset(int *sequence) {
    *sequence = 0;
}
static int watcher_hx_ptl_deinit(void) {
    ++deinitializations;
    free(s_ctx.body);
    s_ctx.body = s_ctx.device = NULL;
    s_ctx.initialized = false;
    return ESP_OK;
}
#include "ptl_init.inc"
static void reset(void) {
    (void)watcher_hx_ptl_deinit();
    memset(&s_ctx, 0, sizeof(s_ctx));
    steps = directions = reset_releases = bus_adds = allocations = deinitializations = 0;
    fail_step = 0;
    board_available = true;
    fail_allocation = reset_driven = powered = false;
}
int main(void) {
    reset();
    CHECK(watcher_hx_ptl_init() == ESP_OK);
    CHECK(s_ctx.initialized && directions == 2U && reset_releases == 1U && bus_adds == 1U);
    unsigned first_steps = steps;
    CHECK(watcher_hx_ptl_init() == ESP_OK && steps == first_steps); /* Never reset a live transport. */
    for (int failure = 1; failure <= 6; ++failure) {
        reset();
        fail_step = failure;
        CHECK(watcher_hx_ptl_init() == ESP_FAIL);
        CHECK(!s_ctx.initialized && allocations == 0U && s_ctx.device == NULL);
    }
    reset();
    board_available = false;
    CHECK(watcher_hx_ptl_init() == ESP_ERR_INVALID_STATE && steps == 0U);
    reset();
    fail_allocation = true;
    CHECK(watcher_hx_ptl_init() == ESP_ERR_NO_MEM && deinitializations == 1U && s_ctx.device == NULL);
    reset();
    CHECK(watcher_hx_ptl_init() == ESP_OK);
    reset();
    puts("PTL board initialization tests passed");
    return 0;
}
