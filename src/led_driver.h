#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "config.h"

struct RGBColor {
    uint8_t r;
    uint8_t g;
    uint8_t b;

    constexpr RGBColor() : r(0), g(0), b(0) {}
    constexpr RGBColor(uint8_t r, uint8_t g, uint8_t b) : r(r), g(g), b(b) {}
    
    static constexpr RGBColor Black() { return RGBColor(0, 0, 0); }
    static constexpr RGBColor Red() { return RGBColor(255, 0, 0); }
    static constexpr RGBColor Green() { return RGBColor(0, 255, 0); }
    static constexpr RGBColor Blue() { return RGBColor(0, 80, 255); }
    static constexpr RGBColor Cyan() { return RGBColor(0, 230, 255); }
    static constexpr RGBColor Magenta() { return RGBColor(255, 0, 180); }
    static constexpr RGBColor Yellow() { return RGBColor(255, 200, 0); }
    static constexpr RGBColor White() { return RGBColor(255, 255, 255); }
    static constexpr RGBColor Purple() { return RGBColor(180, 30, 255); }
    static constexpr RGBColor Amber() { return RGBColor(255, 120, 0); }
};

enum CalibStage {
    STAGE_LEFT,
    STAGE_RIGHT,
    STAGE_CENTER,
    STAGE_DONE
};

void led_driver_init();
void led_set_pixel(uint8_t index, RGBColor color);
void led_fill(RGBColor color);
void led_show();

// Reactive lighting update (called periodically)
void led_update_reactive(uint32_t button_mask);

// Game-controlled output for IO4 Mode
void led_set_io4_game_output(uint32_t led_data);
void led_set_io4_active(bool active);

// Guided calibration visual animations
void led_calibration_animation(CalibStage stage, bool achieved, float lever_ratio);
void led_flash_confirm_green();

// Config Mode visual animation
void led_config_mode_animation();

// Mode boot flash
void led_flash_mode(uint8_t mode);
