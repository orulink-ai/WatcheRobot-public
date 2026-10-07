/**
 * @file hal_camera.h
 * @brief Camera HAL: SSCMA client wrapper for Himax HX6538 vision AI chip
 *
 * Provides JPEG frame callback interface via SSCMA protocol.
 * Himax chip communicates with ESP32-S3 via UART/SPI SSCMA protocol.
 *
 * Current implementation supports:
 * - SSCMA client initialization and connect wait
 * - one-shot frame capture via hal_camera_capture_once()
 * - continuous frame capture via hal_camera_start()/hal_camera_stop()
 *
 * Streaming is currently implemented as a paced one-shot INVOKE loop over SSCMA.
 */

#ifndef HAL_CAMERA_H
#define HAL_CAMERA_H

#include "esp_err.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/**
 * @brief JPEG frame callback type
 *
 * Called from the camera task when a new JPEG frame is available.
 *
 * @param jpeg         Pointer to JPEG data (valid only during callback)
 * @param size         JPEG data size in bytes
 * @param timestamp_ms Capture timestamp in milliseconds
 * @param ctx          User context pointer
 */
typedef void (*hal_camera_frame_cb_t)(const uint8_t *jpeg, size_t size, uint32_t timestamp_ms, void *ctx);
typedef void (*hal_camera_capture_activity_cb_t)(bool active, void *ctx);

#define HAL_CAMERA_INFERENCE_MAX_BOXES 8u
#define HAL_CAMERA_MODEL_NAME_MAX_LENGTH 64u

typedef struct {
    uint16_t x;
    uint16_t y;
    uint16_t width;
    uint16_t height;
    uint8_t score;
    uint8_t target;
} hal_camera_inference_box_t;

typedef struct {
    uint32_t sequence;
    uint32_t timestamp_ms;
    uint16_t frame_width;
    uint16_t frame_height;
    uint16_t preprocess_ms;
    uint16_t inference_ms;
    uint16_t postprocess_ms;
    uint8_t box_count;
    hal_camera_inference_box_t boxes[HAL_CAMERA_INFERENCE_MAX_BOXES];
    /**
     * Optional base64 JPEG view into the SSCMA reply.
     *
     * Valid only for the duration of the inference callback. Consumers that
     * need the image later must copy it into an owned, preallocated buffer.
     */
    const char *preview_image_base64;
    uint32_t preview_image_size;
} hal_camera_inference_frame_t;

typedef struct {
    uint32_t frame_count;
    uint32_t valid_box_count;
    uint32_t parse_failures;
    uint32_t callback_count;
    uint32_t last_interval_ms;
    uint32_t max_interval_ms;
    uint64_t interval_total_ms;
} hal_camera_inference_stats_t;

typedef struct {
    int model_id;
    bool contains_face_class;
    bool verified;
    char name[HAL_CAMERA_MODEL_NAME_MAX_LENGTH];
} hal_camera_model_info_t;

typedef struct {
    bool include_preview_image;
} hal_camera_inference_options_t;

typedef struct {
    const char *backend;
    bool initialized;
    bool connected;
    bool streaming;
    bool inferencing;
    bool busy;
    bool supports_capture;
    bool supports_preview;
    bool supports_inference;
    bool supports_model_info;
    bool supports_model_management;
    esp_err_t status;
    esp_err_t model_status;
    bool model_available;
    hal_camera_model_info_t model;
} hal_camera_diagnostics_t;

typedef void (*hal_camera_inference_cb_t)(const hal_camera_inference_frame_t *frame, void *ctx);

/**
 * Register the process-wide camera capture activity observer.
 *
 * Passing NULL unregisters the observer. The current state is replayed after
 * registration. Callbacks run synchronously in the calling task, outside the
 * camera HAL lock, and must remain brief. This API is not ISR-safe.
 */
esp_err_t hal_camera_set_capture_activity_callback(hal_camera_capture_activity_cb_t cb, void *ctx);

/**
 * @brief Initialize camera HAL (SSCMA client setup).
 *
 * @return ESP_OK on success, ESP_FAIL if SSCMA initialization fails
 */
esp_err_t hal_camera_init(void);

/**
 * @brief Release camera HAL runtime resources owned by the active app/session.
 *
 * Stops streaming, drops pending capture state, and releases the SSCMA client
 * tasks and buffers. Shared board-level SPI/IO setup remains owned by the BSP.
 *
 * @return ESP_OK on success
 */
esp_err_t hal_camera_deinit(void);

/**
 * @brief Configure the active HX6538 camera sensor output profile.
 *
 * Supported profiles currently map to the sensor catalog reported by the coprocessor:
 * - 240x240
 * - 416x416
 * - 480x480
 * - 640x480
 *
 * Quality is accepted as a hint for upper layers, but the current SSCMA path
 * does not expose a writable JPEG quality control.
 *
 * @param width          Requested output width
 * @param height         Requested output height
 * @param quality        Requested quality hint (stored for diagnostics only)
 * @param applied_width  Optional: applied width on success
 * @param applied_height Optional: applied height on success
 * @return ESP_OK on success
 */
esp_err_t hal_camera_configure(int width, int height, int quality, int *applied_width, int *applied_height);

/**
 * @brief Start continuous frame capture.
 *
 * @param fps Target frame rate (1–30). Actual rate depends on Himax AI workload.
 * @param cb  Frame callback (called from camera task context)
 * @param ctx User context passed to callback
 * @return ESP_OK on success
 */
esp_err_t hal_camera_start(int fps, hal_camera_frame_cb_t cb, void *ctx);

/**
 * @brief Stop continuous frame capture.
 *
 * @return ESP_OK on success
 */
esp_err_t hal_camera_stop(void);

/* Prepare sensor exposure before starting the shutter animation. Idempotent; stop/deinit cancels it. */
esp_err_t hal_camera_prepare_capture(void);

/**
 * @brief Capture a single JPEG frame.
 *
 * @param cb  Frame callback (called once when frame is ready)
 * @param ctx User context passed to callback
 * @return ESP_OK on success
 */
esp_err_t hal_camera_capture_once(hal_camera_frame_cb_t cb, void *ctx);

/**
 * Start continuous on-device inference without transferring JPEG frames.
 *
 * Model compatibility is checked by the consuming service. Inference owns the
 * Himax client until stopped, so JPEG capture and streaming return busy.
 */
esp_err_t hal_camera_start_inference(hal_camera_inference_cb_t cb, void *ctx);
esp_err_t hal_camera_start_inference_ex(hal_camera_inference_cb_t cb, void *ctx,
                                        const hal_camera_inference_options_t *options);
esp_err_t hal_camera_stop_inference(void);
bool hal_camera_is_inferencing(void);
esp_err_t hal_camera_get_inference_stats(hal_camera_inference_stats_t *out_stats);
esp_err_t hal_camera_get_model_info(hal_camera_model_info_t *out_info);
/* Read installed descriptors without selecting or writing slots. */
esp_err_t hal_camera_list_models(hal_camera_model_info_t *models, size_t capacity, size_t *count);

/**
 * Select a model that has already been provisioned into the vision coprocessor.
 *
 * This low-level maintenance API does not upload model bytes. It is rejected
 * while capture, streaming, or inference owns the Himax transport.
 */
esp_err_t hal_camera_set_active_model(int model_id);

/**
 * Replace the metadata associated with the currently active model.
 *
 * The JSON metadata is forwarded to SSCMA after validating the local size and
 * camera ownership state. PTL backends return ESP_ERR_NOT_SUPPORTED.
 */
esp_err_t hal_camera_set_model_info(const char *model_info);

esp_err_t hal_camera_get_diagnostics(hal_camera_diagnostics_t *out_diagnostics);

/**
 * @brief Check if camera is currently streaming.
 *
 * @return true if streaming, false otherwise
 */
bool hal_camera_is_streaming(void);

#endif /* HAL_CAMERA_H */

