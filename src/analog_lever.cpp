#include "analog_lever.h"
#include "config.h"
#include "led_driver.h"
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/watchdog.h"
#include <algorithm>

static uint16_t g_filtered_adc = 2048;
static float g_prev_norm = 0.5f;

void analog_lever_init() {
    adc_init();
    adc_gpio_init(PIN_POT_ADC);
    adc_select_input(0); // GP26 = ADC channel 0

    // Seed filter with initial reads
    uint32_t sum = 0;
    for (int i = 0; i < 16; i++) {
        sum += adc_read();
        sleep_us(50);
    }
    g_filtered_adc = sum / 16;
}

void analog_lever_update() {
    uint16_t raw = adc_read();
    // Low-pass EMA filter (alpha = 0.25)
    g_filtered_adc = (g_filtered_adc * 3 + raw) / 4;
}

uint16_t analog_lever_get_raw() {
    return g_filtered_adc;
}

float analog_lever_get_normalized() {
    const auto& cfg = config_get();
    uint16_t min_v = cfg.calib_min;
    uint16_t max_v = cfg.calib_max;

    if (max_v <= min_v + 50) {
        min_v = 50;
        max_v = 4045;
    }

    uint16_t val = g_filtered_adc;
    if (val < min_v) val = min_v;
    if (val > max_v) val = max_v;

    return (float)(val - min_v) / (float)(max_v - min_v);
}

int16_t analog_lever_get_io4() {
    float norm = analog_lever_get_normalized();
    uint16_t val16 = (uint16_t)(norm * 65535.0f);
    return *(int16_t*)&val16;
}

static float g_xinput_smoothed = 0.0f;

int16_t analog_lever_get_xinput() {
    const auto& cfg = config_get();
    float norm = analog_lever_get_normalized();
    if (cfg.gamepad_invert_x) {
        norm = 1.0f - norm;
    }
    float target = (norm * 65534.0f) - 32767.0f;

    // Dynamic EMA filter (smoothing: 0..100)
    float alpha = (float)cfg.gamepad_smoothing / 100.0f;
    if (alpha <= 0.01f) alpha = 1.0f; // Direct response if 0
    if (alpha > 1.0f) alpha = 1.0f;

    g_xinput_smoothed = g_xinput_smoothed * (1.0f - alpha) + target * alpha;

    int32_t val = (int32_t)g_xinput_smoothed;
    if (val < -32768) val = -32768;
    if (val > 32767) val = 32767;
    return (int16_t)val;
}

static float g_accum_mouse = 0.0f;

int8_t analog_lever_get_mouse_dx() {
    float curr_norm = analog_lever_get_normalized();
    float diff = (curr_norm - g_prev_norm) * 2000.0f; // Scale factor for responsive lever motion
    g_prev_norm = curr_norm;
    g_accum_mouse += diff;

    if (g_accum_mouse >= 1.0f || g_accum_mouse <= -1.0f) {
        int8_t step = (int8_t)g_accum_mouse;
        if (step > 127) step = 127;
        if (step < -127) step = -127;
        g_accum_mouse -= (float)step;
        return step;
    }
    return 0;
}

void analog_lever_run_guided_calibration() {
    uint16_t recorded_min = 2048;
    uint16_t recorded_max = 2048;
    uint16_t recorded_center = 2048;

    CalibStage stage = STAGE_LEFT;
    bool left_achieved = false;
    bool right_achieved = false;

    uint32_t button_press_start = 0;
    bool button_was_pressed = false;

    while (stage != STAGE_DONE) {
        analog_lever_update();
        uint16_t raw = analog_lever_get_raw();

        bool btn_pressed = is_button_pressed(PIN_MID_SPACE); // GP20 (Fn)

        // Stage 1: Move lever full LEFT
        if (stage == STAGE_LEFT) {
            if (raw < recorded_min) {
                recorded_min = raw;
            }
            if (recorded_min < 1800) {
                left_achieved = true;
            }

            led_calibration_animation(STAGE_LEFT, left_achieved, 0.0f);

            // User presses GP20 or holds left to advance
            if (btn_pressed && !button_was_pressed && left_achieved) {
                stage = STAGE_RIGHT;
                sleep_ms(300);
            }
        }
        // Stage 2: Move lever full RIGHT
        else if (stage == STAGE_RIGHT) {
            if (raw > recorded_max) {
                recorded_max = raw;
            }
            if (recorded_max > 2200) {
                right_achieved = true;
            }

            led_calibration_animation(STAGE_RIGHT, right_achieved, 1.0f);

            if (btn_pressed && !button_was_pressed && right_achieved) {
                stage = STAGE_CENTER;
                sleep_ms(300);
            }
        }
        // Stage 3: Return to CENTER and press GP20 to confirm
        else if (stage == STAGE_CENTER) {
            recorded_center = raw;
            led_calibration_animation(STAGE_CENTER, true, 0.5f);

            if (btn_pressed && !button_was_pressed) {
                stage = STAGE_DONE;
            }
        }

        button_was_pressed = btn_pressed;
        sleep_ms(10);
    }

    // Safety checks on calibration values
    if (recorded_max <= recorded_min + 200) {
        recorded_min = 50;
        recorded_max = 4045;
        recorded_center = 2048;
    }

    // Save calibration to flash
    auto& cfg = config_get();
    cfg.calib_min = recorded_min;
    cfg.calib_max = recorded_max;
    cfg.calib_center = recorded_center;
    config_save();

    // Confirm visual indication
    led_flash_confirm_green();
}
