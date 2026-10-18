#include "analog_lever.h"
#include "config.h"
#include "led_driver.h"
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/watchdog.h"
#include <algorithm>
#include <math.h>

static uint16_t g_filtered_adc = 2048;
static float g_prev_norm = 0.5f;
static float g_xinput_smoothed = 0.0f;
static uint32_t g_last_adc_sample_us = 0;

static inline uint16_t read_adc_oversampled() {
    // 4x hardware oversampling for reduced 12-bit ADC quantization noise
    uint32_t sum = 0;
    sum += adc_read();
    sum += adc_read();
    sum += adc_read();
    sum += adc_read();
    return (uint16_t)(sum >> 2);
}

void analog_lever_init() {
    adc_init();
    adc_gpio_init(PIN_POT_ADC);
    adc_select_input(0); // GP26 = ADC channel 0

    // Seed filter with initial reads
    uint32_t sum = 0;
    for (int i = 0; i < 16; i++) {
        sum += read_adc_oversampled();
        sleep_us(50);
    }
    g_filtered_adc = (uint16_t)(sum / 16);
    g_last_adc_sample_us = time_us_32();
}

void analog_lever_update() {
    uint32_t now_us = time_us_32();
    // Sample and filter at a deterministic 1000Hz (1ms) rate
    if ((now_us - g_last_adc_sample_us) < 1000u) {
        return;
    }
    g_last_adc_sample_us = now_us;

    uint16_t raw = read_adc_oversampled();
    // Low-pass EMA filter (alpha = 0.25 at 1000Hz)
    g_filtered_adc = (uint16_t)((g_filtered_adc * 3u + raw) >> 2);
}

uint16_t analog_lever_get_raw() {
    return g_filtered_adc;
}

float analog_lever_get_normalized() {
    const auto& cfg = config_get();
    int32_t min_v = (int32_t)cfg.calib_min;
    int32_t max_v = (int32_t)cfg.calib_max;
    int32_t center_v = (int32_t)cfg.calib_center;
    int32_t dz = (int32_t)cfg.deadzone;

    if (max_v <= min_v + 50) {
        min_v = 50;
        max_v = 4045;
    }
    if (center_v <= min_v + 10 || center_v >= max_v - 10) {
        center_v = (min_v + max_v) / 2;
    }
    if (dz < 0) dz = 0;
    if (dz > 512) dz = 512;

    int32_t val = (int32_t)g_filtered_adc;
    if (val < min_v) val = min_v;
    if (val > max_v) val = max_v;

    int32_t diff = val - center_v;
    if (diff >= -dz && diff <= dz) {
        return 0.5f;
    }

    float norm = 0.5f;
    if (diff < 0) {
        int32_t span = (center_v - dz) - min_v;
        if (span < 1) span = 1;
        norm = 0.5f - ((float)(-diff - dz) / (float)span) * 0.5f;
    } else {
        int32_t span = max_v - (center_v + dz);
        if (span < 1) span = 1;
        norm = 0.5f + ((float)(diff - dz) / (float)span) * 0.5f;
    }

    if (norm < 0.0f) norm = 0.0f;
    if (norm > 1.0f) norm = 1.0f;
    return norm;
}

int16_t analog_lever_get_io4() {
    float norm = analog_lever_get_normalized();
    uint16_t val16 = (uint16_t)(norm * 65535.0f);
    return *(int16_t*)&val16;
}

int16_t analog_lever_get_xinput() {
    const auto& cfg = config_get();
    float norm = analog_lever_get_normalized();
    if (cfg.gamepad_invert_x) {
        norm = 1.0f - norm;
    }
    float target = (norm * 65534.0f) - 32767.0f;

    // Smoothing slider: 0 = instant response (alpha = 1.0), 100 = heavy damping (alpha = 0.05)
    float alpha = 1.0f;
    if (cfg.gamepad_smoothing > 0) {
        float s = (float)(cfg.gamepad_smoothing > 100 ? 100 : cfg.gamepad_smoothing);
        alpha = 1.0f - (s * 0.0092f); // Maps 1..100 -> 0.99..0.08
        if (alpha < 0.05f) alpha = 0.05f;
    }

    g_xinput_smoothed = g_xinput_smoothed * (1.0f - alpha) + target * alpha;

    int32_t val = (int32_t)lroundf(g_xinput_smoothed);
    if (val < -32768) val = -32768;
    if (val > 32767) val = 32767;
    return (int16_t)val;
}

static float g_accum_mouse = 0.0f;

int8_t analog_lever_get_mouse_dx() {
    float curr_norm = analog_lever_get_normalized();
    float diff = (curr_norm - g_prev_norm) * 2000.0f;
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
    bool button_was_pressed = true; // Require releasing boot-held GP20 first

    while (stage != STAGE_DONE) {
        uint16_t raw = read_adc_oversampled();
        g_filtered_adc = (uint16_t)((g_filtered_adc * 3u + raw) >> 2);
        raw = g_filtered_adc;

        bool btn_pressed = !gpio_get(PIN_MID_SPACE); // GP20 (Fn)

        // Note on Gekimax hardware polarity:
        // Moving physical lever LEFT produces HIGH ADC values (> 2200, up to ~4095 = calib_max).
        // Moving physical lever RIGHT produces LOW ADC values (< 1800, down to ~0 = calib_min).
        // Visual ratio 0.0 = physical Left, 1.0 = physical Right:
        float visual_left_to_right = 1.0f - ((float)raw / 4095.0f);
        if (visual_left_to_right < 0.0f) visual_left_to_right = 0.0f;
        if (visual_left_to_right > 1.0f) visual_left_to_right = 1.0f;

        // Stage 1: Move lever full LEFT (Left LEDs lit; captures recorded_max > 2200)
        if (stage == STAGE_LEFT) {
            if (raw > recorded_max) {
                recorded_max = raw;
            }
            if (recorded_max > 2200) {
                left_achieved = true;
            }

            led_calibration_animation(STAGE_LEFT, left_achieved, visual_left_to_right);

            if (btn_pressed && !button_was_pressed && left_achieved) {
                stage = STAGE_RIGHT;
                sleep_ms(250);
            }
        }
        // Stage 2: Move lever full RIGHT (Right LEDs lit; captures recorded_min < 1800)
        else if (stage == STAGE_RIGHT) {
            if (raw < recorded_min) {
                recorded_min = raw;
            }
            if (recorded_min < 1800) {
                right_achieved = true;
            }

            led_calibration_animation(STAGE_RIGHT, right_achieved, visual_left_to_right);

            if (btn_pressed && !button_was_pressed && right_achieved) {
                stage = STAGE_CENTER;
                sleep_ms(250);
            }
        }
        // Stage 3: Return to CENTER and press GP20 to confirm
        else if (stage == STAGE_CENTER) {
            recorded_center = raw;
            bool center_ok = (raw > 1500 && raw < 2600);
            led_calibration_animation(STAGE_CENTER, center_ok, visual_left_to_right);

            if (btn_pressed && !button_was_pressed) {
                stage = STAGE_DONE;
            }
        }

        button_was_pressed = btn_pressed;
        sleep_ms(8);
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
