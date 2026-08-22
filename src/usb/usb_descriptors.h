#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "tusb.h"

#ifdef __cplusplus
extern "C" {
#endif

void usb_descriptors_init(uint8_t mode);

#ifdef __cplusplus
}
#endif
