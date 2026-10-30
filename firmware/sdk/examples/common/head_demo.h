#pragma once
#include "esp_err.h"
/* Starts one process-lifetime media owner task and a shared demo UI.
 * Audio/camera lifecycle stays on that task; no official application policies.
 * This minimal example has no dynamic shutdown or automatic retry loop. */
esp_err_t head_demo_start(void);
