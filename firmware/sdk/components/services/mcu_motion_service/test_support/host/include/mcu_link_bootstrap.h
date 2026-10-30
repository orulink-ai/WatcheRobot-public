#ifndef TEST_MCU_LINK_BOOTSTRAP_H
#define TEST_MCU_LINK_BOOTSTRAP_H

#include "mcu_link.h"

#include <stdbool.h>

mcu_link_t *mcu_link_bootstrap_get_link(void);
bool mcu_link_bootstrap_is_ready(void);

#endif /* TEST_MCU_LINK_BOOTSTRAP_H */
