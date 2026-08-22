#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void serial_config_init(void);
void serial_config_update_task(void);

#ifdef __cplusplus
}
#endif
