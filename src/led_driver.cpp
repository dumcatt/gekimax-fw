#include "led_driver.h"
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "ws2812.pio.h"
#include <string.h>
#include <math.h>

static PIO g_pio = pio0;
static uint g_sm = 0;
static uint g_offset = 0;

static RGBColor g_pixels[NUM_LEDS];
static RGBColor g_game_leds[6]; // 6 RGB buttons for ONGEKI IO4
static bool g_io4_mode_active = false;
static bool g_config_connected = false;
static int8_t g_preview_mode_override = -1;
static uint32_t g_last_game_led_update = 0;

void led_driver_init() {
    g_offset = pio_add_program(g_pio, &ws2812_program);
    ws2812_program_init(g_pio, g_sm, g_offset, PIN_LED_DATA, 800000, false);

    memset(g_pixels, 0, sizeof(g_pixels));
    led_show();
}

static inline void put_pixel(uint32_t pixel_grb) {
    pio_sm_put_blocking(g_pio, g_sm, pixel_grb << 8u);
}

void led_set_pixel(uint8_t index, RGBColor color) {
    if (index < NUM_LEDS) {
        g_pixels[index] = color;
    }
}

void led_fill(RGBColor color) {
    for (int i = 0; i < NUM_LEDS; i++) {
        g_pixels[i] = color;
    }
}

void led_show() {
    uint16_t scale = 256;
    if (!g_io4_mode_active) {
        uint8_t br = config_get().led_brightness;
        scale = (uint16_t)br + (br == 255 ? 1 : 0);
    }

    for (int i = 0; i < NUM_LEDS; i++) {
        uint32_t r = ((uint32_t)g_pixels[i].r * scale) >> 8;
        uint32_t g = ((uint32_t)g_pixels[i].g * scale) >> 8;
        uint32_t b = ((uint32_t)g_pixels[i].b * scale) >> 8;
        // WS2812 GRB ordering
        uint32_t grb = (g << 16) | (r << 8) | b;
        put_pixel(grb);
    }
}

void led_set_io4_active(bool active) {
    g_io4_mode_active = active;
}

void led_set_config_connected(bool connected) {
    g_config_connected = connected;
}

bool led_is_config_connected() {
    return g_config_connected;
}

void led_set_preview_mode_override(int8_t mode_override) {
    g_preview_mode_override = mode_override;
}

// Bit position mappings for the 6 ONGEKI game RGB buttons
static const uint8_t BIT_POS_MAP[18] = {
    // Left Red (SW3/D3): Red, Green, Blue
    17, 16, 15,
    // Left Green (SW4/D4): Red, Green, Blue
    14, 13, 12,
    // Left Blue (SW5/D5): Red, Green, Blue
    11, 10, 9,
    // Right Red (SW13/D13): Red, Green, Blue
    8, 7, 6,
    // Right Green (SW14/D14): Red, Green, Blue
    5, 4, 3,
    // Right Blue (SW15/D15): Red, Green, Blue
    2, 1, 0
};

void led_set_io4_game_output(uint32_t led_data) {
    g_last_game_led_update = to_ms_since_boot(get_absolute_time());

    for (int btn = 0; btn < 6; btn++) {
        uint8_t r = ((led_data >> BIT_POS_MAP[btn * 3 + 0]) & 1) ? 255 : 0;
        uint8_t g = ((led_data >> BIT_POS_MAP[btn * 3 + 1]) & 1) ? 255 : 0;
        uint8_t b = ((led_data >> BIT_POS_MAP[btn * 3 + 2]) & 1) ? 255 : 0;
        g_game_leds[btn] = RGBColor(r, g, b);
    }
}

static uint8_t fade_val(uint8_t val, uint8_t decay) {
    return (uint16_t(val) * decay) >> 8;
}

// HSV to RGB conversion for rainbow wave effect
static RGBColor hsv_to_rgb(uint16_t h, uint8_t s, uint8_t v) {
    uint8_t r, g, b;
    uint8_t region = h / 43;
    uint8_t remainder = (h - (region * 43)) * 6;

    uint8_t p = (v * (255 - s)) >> 8;
    uint8_t q = (v * (255 - ((s * remainder) >> 8))) >> 8;
    uint8_t t = (v * (255 - ((s * (255 - remainder)) >> 8))) >> 8;

    switch (region) {
        case 0:  r = v; g = t; b = p; break;
        case 1:  r = q; g = v; b = p; break;
        case 2:  r = p; g = v; b = t; break;
        case 3:  r = p; g = q; b = v; break;
        case 4:  r = t; g = p; b = v; break;
        default: r = v; g = p; b = q; break;
    }
    return RGBColor(r, g, b);
}

void led_update_reactive(uint32_t button_mask) {
    if (g_io4_mode_active) {
        // Pure game-controlled lighting in IO4 mode (no interference on game RGBs)
        memset(g_pixels, 0, sizeof(g_pixels));

        // 6 Game-controlled RGB buttons directly driven by ONGEKI
        g_pixels[2]  = g_game_leds[0]; // Left Red (D3 / SW3)
        g_pixels[3]  = g_game_leds[1]; // Left Green (D4 / SW4)
        g_pixels[4]  = g_game_leds[2]; // Left Blue (D5 / SW5)
        g_pixels[12] = g_game_leds[3]; // Right Red (D13 / SW13)
        g_pixels[13] = g_game_leds[4]; // Right Green (D14 / SW14)
        g_pixels[14] = g_game_leds[5]; // Right Blue (D15 / SW15)

        led_show();
        return;
    }

    const auto& cfg = config_get();
    uint8_t mode = (cfg.active_mode == MODE_XINPUT) ? cfg.gamepad_led_mode : cfg.kb_led_mode;
    if (g_preview_mode_override >= 0 && g_preview_mode_override <= LED_MODE_OFF) {
        mode = (uint8_t)g_preview_mode_override;
    }

    // 1. OFF MODE
    if (mode == LED_MODE_OFF) {
        memset(g_pixels, 0, sizeof(g_pixels));
        led_show();
        return;
    }

    // 2. SOLID MODE
    if (mode == LED_MODE_SOLID) {
        for (int i = 0; i < NUM_BUTTONS; i++) {
            g_pixels[i] = RGBColor(cfg.led_colors[i][0], cfg.led_colors[i][1], cfg.led_colors[i][2]);
        }
        RGBColor ug_l(cfg.underglow_left[0], cfg.underglow_left[1], cfg.underglow_left[2]);
        RGBColor ug_r(cfg.underglow_right[0], cfg.underglow_right[1], cfg.underglow_right[2]);
        for (int i = 21; i <= 23; i++) g_pixels[i] = ug_l;
        for (int i = 24; i <= 26; i++) g_pixels[i] = ug_r;
        led_show();
        return;
    }

    // 3. BREATHING MODE
    if (mode == LED_MODE_BREATHING) {
        uint32_t t = to_ms_since_boot(get_absolute_time());
        float phase = (float)(t % 2500) / 2500.0f * 2.0f * 3.14159f;
        float factor = sinf(phase) * 0.45f + 0.55f; // 0.10 to 1.00

        for (int i = 0; i < NUM_BUTTONS; i++) {
            g_pixels[i] = RGBColor(
                (uint8_t)(cfg.led_colors[i][0] * factor),
                (uint8_t)(cfg.led_colors[i][1] * factor),
                (uint8_t)(cfg.led_colors[i][2] * factor)
            );
        }
        RGBColor ug_l(
            (uint8_t)(cfg.underglow_left[0] * factor),
            (uint8_t)(cfg.underglow_left[1] * factor),
            (uint8_t)(cfg.underglow_left[2] * factor)
        );
        RGBColor ug_r(
            (uint8_t)(cfg.underglow_right[0] * factor),
            (uint8_t)(cfg.underglow_right[1] * factor),
            (uint8_t)(cfg.underglow_right[2] * factor)
        );
        for (int i = 21; i <= 23; i++) g_pixels[i] = ug_l;
        for (int i = 24; i <= 26; i++) g_pixels[i] = ug_r;
        led_show();
        return;
    }

    // 4. RAINBOW WAVE MODE
    if (mode == LED_MODE_RAINBOW) {
        uint32_t t = to_ms_since_boot(get_absolute_time());
        uint16_t base_hue = (t / 8) % 256;

        for (int i = 0; i < NUM_BUTTONS; i++) {
            uint16_t hue = (base_hue + (i * 12)) % 256;
            g_pixels[i] = hsv_to_rgb(hue, 255, 220);
        }
        for (int i = 21; i <= 23; i++) {
            uint16_t hue = (base_hue + 30) % 256;
            g_pixels[i] = hsv_to_rgb(hue, 255, 235);
        }
        for (int i = 24; i <= 26; i++) {
            uint16_t hue = (base_hue + 150) % 256;
            g_pixels[i] = hsv_to_rgb(hue, 255, 235);
        }
        led_show();
        return;
    }

    // 5. INVERTED REACTIVE MODE (Solid ON by default, turns off on press)
    if (mode == LED_MODE_INVERTED_REACTIVE) {
        const uint8_t recover_rate = 18;

        for (int i = 0; i < NUM_BUTTONS; i++) {
            RGBColor target(cfg.led_colors[i][0], cfg.led_colors[i][1], cfg.led_colors[i][2]);
            if ((button_mask >> i) & 1) {
                g_pixels[i] = RGBColor(0, 0, 0);
            } else {
                if (g_pixels[i].r < target.r) g_pixels[i].r = (uint8_t)fminf(target.r, g_pixels[i].r + recover_rate);
                else if (g_pixels[i].r > target.r) g_pixels[i].r = target.r;
                if (g_pixels[i].g < target.g) g_pixels[i].g = (uint8_t)fminf(target.g, g_pixels[i].g + recover_rate);
                else if (g_pixels[i].g > target.g) g_pixels[i].g = target.g;
                if (g_pixels[i].b < target.b) g_pixels[i].b = (uint8_t)fminf(target.b, g_pixels[i].b + recover_rate);
                else if (g_pixels[i].b > target.b) g_pixels[i].b = target.b;
            }
        }

        RGBColor ug_l(cfg.underglow_left[0], cfg.underglow_left[1], cfg.underglow_left[2]);
        RGBColor ug_r(cfg.underglow_right[0], cfg.underglow_right[1], cfg.underglow_right[2]);
        for (int i = 21; i <= 23; i++) g_pixels[i] = ug_l;
        for (int i = 24; i <= 26; i++) g_pixels[i] = ug_r;
        led_show();
        return;
    }

    // 6. DEFAULT REACTIVE MODE (Lights up on press, logarithmic fadeout)
    const uint8_t decay = 225;
    bool fn_active = config_is_fn_layer_active();
    const uint8_t* fn_map = (cfg.active_mode == MODE_XINPUT) ? cfg.fn_gamepad_map : cfg.fn_keymap;

    for (int i = 0; i < NUM_BUTTONS; i++) {
        if ((button_mask >> i) & 1) {
            g_pixels[i] = RGBColor(cfg.led_colors[i][0], cfg.led_colors[i][1], cfg.led_colors[i][2]);
        } else {
            uint8_t r = fade_val(g_pixels[i].r, decay);
            uint8_t g = fade_val(g_pixels[i].g, decay);
            uint8_t b = fade_val(g_pixels[i].b, decay);

            // When FN layer is held, softly highlight switches that have an FN binding
            if (fn_active && fn_map[i] != 0 && fn_map[i] != ACT_CODE_TRANSPARENT) {
                uint8_t hint_r = cfg.led_colors[i][0] >> 2;
                uint8_t hint_g = cfg.led_colors[i][1] >> 2;
                uint8_t hint_b = cfg.led_colors[i][2] >> 2;
                if (r < hint_r) r = hint_r;
                if (g < hint_g) g = hint_g;
                if (b < hint_b) b = hint_b;
            }

            g_pixels[i] = RGBColor(r, g, b);
        }
    }

    // Underglow LEDs: softly glow at ~18% of configured underglow color when idle, boost to 100% on wing press
    bool left_active = (button_mask & 0x0003FFu) != 0;
    bool right_active = (button_mask & 0x0FFC00u) != 0;

    RGBColor ug_l_full(cfg.underglow_left[0], cfg.underglow_left[1], cfg.underglow_left[2]);
    RGBColor ug_r_full(cfg.underglow_right[0], cfg.underglow_right[1], cfg.underglow_right[2]);
    RGBColor ug_l_idle(cfg.underglow_left[0] >> 3, cfg.underglow_left[1] >> 3, cfg.underglow_left[2] >> 3);
    RGBColor ug_r_idle(cfg.underglow_right[0] >> 3, cfg.underglow_right[1] >> 3, cfg.underglow_right[2] >> 3);

    for (int i = 21; i <= 23; i++) {
        if (left_active) {
            g_pixels[i] = ug_l_full;
        } else {
            uint8_t r = fade_val(g_pixels[i].r, decay);
            uint8_t g = fade_val(g_pixels[i].g, decay);
            uint8_t b = fade_val(g_pixels[i].b, decay);
            g_pixels[i].r = (r > ug_l_idle.r) ? r : ug_l_idle.r;
            g_pixels[i].g = (g > ug_l_idle.g) ? g : ug_l_idle.g;
            g_pixels[i].b = (b > ug_l_idle.b) ? b : ug_l_idle.b;
        }
    }
    for (int i = 24; i <= 26; i++) {
        if (right_active) {
            g_pixels[i] = ug_r_full;
        } else {
            uint8_t r = fade_val(g_pixels[i].r, decay);
            uint8_t g = fade_val(g_pixels[i].g, decay);
            uint8_t b = fade_val(g_pixels[i].b, decay);
            g_pixels[i].r = (r > ug_r_idle.r) ? r : ug_r_idle.r;
            g_pixels[i].g = (g > ug_r_idle.g) ? g : ug_r_idle.g;
            g_pixels[i].b = (b > ug_r_idle.b) ? b : ug_r_idle.b;
        }
    }

    led_show();
}

void led_calibration_animation(CalibStage stage, bool achieved, float lever_ratio) {
    memset(g_pixels, 0, sizeof(g_pixels));

    // Live lever position cursor along the 8 bottom row buttons:
    // Left-to-right physical order: GP5(5), GP6(6), GP7(7), GP8(8), GP15(15), GP16(16), GP17(17), GP18(18)
    static const uint8_t BOTTOM_BAR[8] = { 5, 6, 7, 8, 15, 16, 17, 18 };
    int pos_idx = (int)lroundf(lever_ratio * 7.0f);
    if (pos_idx < 0) pos_idx = 0;
    if (pos_idx > 7) pos_idx = 7;
    g_pixels[BOTTOM_BAR[pos_idx]] = RGBColor(60, 180, 255);

    if (stage == STAGE_LEFT) {
        RGBColor col = achieved ? RGBColor::Green() : RGBColor::Amber();
        g_pixels[0] = col; // Left Menu (D1)
        g_pixels[1] = col; // Side Left (D2)
        g_pixels[2] = col; // Left Red (D3)
        g_pixels[3] = col; // Left Green (D4)
        g_pixels[4] = col; // Left Blue (D5)
        for (int i = 21; i <= 23; i++) g_pixels[i] = col; // Left Underglow
        if (achieved) g_pixels[20] = RGBColor::Cyan();    // Prompt GP20 (Fn) to confirm Left
    } else if (stage == STAGE_RIGHT) {
        RGBColor col = achieved ? RGBColor::Green() : RGBColor::Amber();
        g_pixels[10] = col; // Right Menu (D11)
        g_pixels[11] = col; // Side Right (D12)
        g_pixels[12] = col; // Right Red (D13)
        g_pixels[13] = col; // Right Green (D14)
        g_pixels[14] = col; // Right Blue (D15)
        for (int i = 24; i <= 26; i++) g_pixels[i] = col; // Right Underglow
        if (achieved) g_pixels[20] = RGBColor::Cyan();    // Prompt GP20 (Fn) to confirm Right
    } else if (stage == STAGE_CENTER) {
        RGBColor col = achieved ? RGBColor::Cyan() : RGBColor::Amber();
        g_pixels[20] = RGBColor::Green(); // Mid Space / Fn (D21)
        g_pixels[3]  = col;               // Left Green (D4)
        g_pixels[13] = col;               // Right Green (D14)
    }

    led_show();
}

void led_flash_confirm_green() {
    for (int k = 0; k < 3; k++) {
        led_fill(RGBColor::Green());
        led_show();
        sleep_ms(150);
        led_fill(RGBColor::Black());
        led_show();
        sleep_ms(100);
    }
}

void led_config_mode_animation() {
    uint32_t t = to_ms_since_boot(get_absolute_time());
    float phase = (float)(t % 2000) / 2000.0f * 2.0f * 3.14159f;
    uint8_t brightness = (uint8_t)((sinf(phase) * 0.5f + 0.5f) * 180.0f + 20.0f);

    RGBColor cyan_pulse(0, brightness, brightness);
    RGBColor blue_pulse(0, (uint8_t)(brightness * 0.4f), brightness);

    for (int i = 0; i < NUM_BUTTONS; i++) {
        g_pixels[i] = cyan_pulse;
    }
    for (int i = 21; i < NUM_LEDS; i++) {
        g_pixels[i] = blue_pulse;
    }

    led_show();
}

void led_flash_mode(uint8_t mode) {
    RGBColor col = RGBColor::Cyan();
    if (mode == MODE_IO4) col = RGBColor::Magenta();
    else if (mode == MODE_KEYBOARD) col = RGBColor::Cyan();
    else if (mode == MODE_XINPUT) col = RGBColor::Green();
    else if (mode == MODE_CONFIG) col = RGBColor::Yellow();

    for (int i = 0; i < 2; i++) {
        led_fill(col);
        led_show();
        sleep_ms(100);
        led_fill(RGBColor::Black());
        led_show();
        sleep_ms(50);
    }
}
