#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef uint32_t TickType_t;
typedef int portMUX_TYPE;
#define portMUX_INITIALIZER_UNLOCKED 0
#define pdTRUE 1
#define pdMS_TO_TICKS(ms) (ms)
void test_enter_critical(void);
void test_exit_critical(void);
#define taskENTER_CRITICAL(lock) test_enter_critical()
#define taskEXIT_CRITICAL(lock) test_exit_critical()
#ifdef __cplusplus
}
#endif

