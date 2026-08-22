#pragma once

#include <stdint.h>
#include <stdbool.h>

void analog_lever_init();
void analog_lever_update();

// Raw filtered ADC value (0..4095)
uint16_t analog_lever_get_raw();

// Normalized 0.0f to 1.0f based on calibrated software range
float analog_lever_get_normalized();

// IO4 mode output: 0 to 65535 (as int16_t bitcast for SEGA IO4 protocol)
int16_t analog_lever_get_io4();

// XInput mode output: -32768 to 32767
int16_t analog_lever_get_xinput();

// Mouse X relative delta
int8_t analog_lever_get_mouse_dx();

// Guided interactive calibration mode
void analog_lever_run_guided_calibration();
