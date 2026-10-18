#include "config.h"
#include "pico/stdlib.h"
#include "pico/bootrom.h"
#include "hardware/flash.h"
#include "hardware/sync.h"
#include "hardware/watchdog.h"
#include "tusb.h"
#include <string.h>
#include <stddef.h>

#define FLASH_TARGET_OFFSET (PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE)
static const uint8_t *flash_target_contents = (const uint8_t *)(XIP_BASE + FLASH_TARGET_OFFSET);

static ConfigData g_config;

// Multicore SRAM lockout flags (ensures Core 1 never fetches from XIP Flash during erase/program)
static volatile bool g_core1_started = false;
static volatile bool g_pause_core1 = false;
static volatile bool g_core1_paused = false;

// Eager 0ms-latency switch debounce state
static const uint8_t BUTTON_PINS[NUM_BUTTONS] = {
    PIN_LEFT_MENU, PIN_SIDE_LEFT, PIN_LEFT_RED, PIN_LEFT_GREEN, PIN_LEFT_BLUE,
    PIN_LEFT_1, PIN_LEFT_2, PIN_LEFT_3, PIN_LEFT_4, PIN_LEFT_SPACE,
    PIN_RIGHT_MENU, PIN_SIDE_RIGHT, PIN_RIGHT_RED, PIN_RIGHT_GREEN, PIN_RIGHT_BLUE,
    PIN_RIGHT_1, PIN_RIGHT_2, PIN_RIGHT_3, PIN_RIGHT_4, PIN_RIGHT_SPACE,
    PIN_MID_SPACE
};

static bool g_debounced_state[NUM_BUTTONS] = {false};
static uint32_t g_last_change_us[NUM_BUTTONS] = {0};

// FN Layer & GP20 Tap-Hold State Machine
static volatile bool g_fn_layer_active = false;
static bool g_gp20_was_pressed = false;
static bool g_gp20_acted_as_fn = false;
static uint32_t g_gp20_press_start_us = 0;
static uint32_t g_gp20_tap_pulse_until_us = 0;
static uint8_t g_prev_hw_code[NUM_BUTTONS] = {0};
static uint8_t g_saved_brightness_before_mute = 220;

static uint32_t calculate_checksum(const ConfigData& cfg) {
    const uint8_t *bytes = (const uint8_t *)&cfg;
    size_t len = offsetof(ConfigData, checksum);
    uint32_t hash = 2166136261u; // FNV-1a 32-bit
    for (size_t i = 0; i < len; i++) {
        hash ^= bytes[i];
        hash *= 16777619u;
    }
    return hash;
}

static const uint8_t DEFAULT_KEYMAP[NUM_BUTTONS] = {
    HID_KEY_ESCAPE,      // GP0:  Esc (Left Menu)
    HID_KEY_SHIFT_LEFT,  // GP1:  Left Shift (Side Left)
    HID_KEY_A,           // GP2:  A (Left Red)
    HID_KEY_S,           // GP3:  S (Left Green)
    HID_KEY_D,           // GP4:  D (Left Blue)
    HID_KEY_Z,           // GP5:  Z (Left Bottom 1)
    HID_KEY_X,           // GP6:  X (Left Bottom 2)
    HID_KEY_V,           // GP7:  V (Left Bottom 3)
    HID_KEY_B,           // GP8:  B (Left Bottom 4)
    HID_KEY_C,           // GP9:  C (Left Space)
    HID_KEY_ENTER,       // GP10: Enter (Right Menu)
    HID_KEY_SHIFT_RIGHT, // GP11: Right Shift (Side Right)
    HID_KEY_L,           // GP12: L (Right Red)
    HID_KEY_SEMICOLON,   // GP13: ; (Right Green)
    HID_KEY_APOSTROPHE,  // GP14: ' (Right Blue)
    HID_KEY_N,           // GP15: N (Right Bottom 1)
    HID_KEY_COMMA,       // GP16: , (Right Bottom 2)
    HID_KEY_PERIOD,      // GP17: . (Right Bottom 3)
    HID_KEY_SLASH,       // GP18: / (Right Bottom 4)
    HID_KEY_M,           // GP19: M (Right Space)
    HID_KEY_SPACE        // GP20: Space (Mid Space / Fn)
};

static const uint8_t DEFAULT_FN_KEYMAP[NUM_BUTTONS] = {
    ACT_CODE_LED_NEXT,    // GP0:  Next LED Effect
    ACT_CODE_TRANSPARENT, // GP1:  Pass-through
    ACT_CODE_BRIGHT_DOWN, // GP2:  Brightness -
    ACT_CODE_LED_TOGGLE,  // GP3:  Toggle LEDs On/Off
    ACT_CODE_BRIGHT_UP,   // GP4:  Brightness +
    HID_KEY_F1,           // GP5:  F1
    HID_KEY_F2,           // GP6:  F2
    HID_KEY_F3,           // GP7:  F3
    ACT_CODE_BOOTSEL,     // GP8:  Enter BOOTSEL (UF2)
    HID_KEY_BACKSPACE,    // GP9:  Backspace
    ACT_CODE_SAVE_CONFIG, // GP10: Save Settings to Flash
    ACT_CODE_TRANSPARENT, // GP11: Pass-through
    HID_KEY_ARROW_LEFT,   // GP12: Arrow Left
    HID_KEY_ARROW_UP,     // GP13: Arrow Up
    HID_KEY_ARROW_RIGHT,  // GP14: Arrow Right
    HID_KEY_F5,           // GP15: F5
    HID_KEY_F6,           // GP16: F6
    HID_KEY_F7,           // GP17: F7
    HID_KEY_F8,           // GP18: F8
    HID_KEY_ARROW_DOWN,   // GP19: Arrow Down
    ACT_CODE_FN           // GP20: FN Hold
};

static const uint8_t DEFAULT_GAMEPAD_MAP[NUM_BUTTONS] = {
    GP_ACT_BACK,         // GP0:  Back / Select
    GP_ACT_LB,           // GP1:  Left Bumper (LB)
    GP_ACT_DPAD_LEFT,    // GP2:  D-Pad Left
    GP_ACT_DPAD_UP,      // GP3:  D-Pad Up
    GP_ACT_DPAD_RIGHT,   // GP4:  D-Pad Right
    GP_ACT_DPAD_DOWN,    // GP5:  D-Pad Down
    GP_ACT_LS_CLICK,     // GP6:  Left Stick Click (LS)
    GP_ACT_RS_CLICK,     // GP7:  Right Stick Click (RS)
    GP_ACT_B,            // GP8:  B Button
    GP_ACT_LT,           // GP9:  Left Trigger (LT)
    GP_ACT_START,        // GP10: Start
    GP_ACT_RB,           // GP11: Right Bumper (RB)
    GP_ACT_X,            // GP12: X Button
    GP_ACT_Y,            // GP13: Y Button
    GP_ACT_A,            // GP14: A Button
    GP_ACT_B,            // GP15: B Button
    GP_ACT_RS_UP,        // GP16: Right Stick Up
    GP_ACT_RS_LEFT,      // GP17: Right Stick Left
    GP_ACT_RS_RIGHT,     // GP18: Right Stick Right
    GP_ACT_RT,           // GP19: Right Trigger (RT)
    GP_ACT_GUIDE         // GP20: Xbox Guide / Home
};

static const uint8_t DEFAULT_FN_GAMEPAD_MAP[NUM_BUTTONS] = {
    ACT_CODE_LED_NEXT,    // GP0:  Next LED Effect
    ACT_CODE_TRANSPARENT, // GP1:  Pass-through
    ACT_CODE_BRIGHT_DOWN, // GP2:  Brightness -
    ACT_CODE_LED_TOGGLE,  // GP3:  Toggle LEDs On/Off
    ACT_CODE_BRIGHT_UP,   // GP4:  Brightness +
    GP_ACT_LS_UP,         // GP5:  Left Stick Up
    GP_ACT_LS_DOWN,       // GP6:  Left Stick Down
    GP_ACT_LS_LEFT,       // GP7:  Left Stick Left
    ACT_CODE_BOOTSEL,     // GP8:  Enter BOOTSEL (UF2)
    ACT_CODE_TRANSPARENT, // GP9:  Pass-through
    ACT_CODE_SAVE_CONFIG, // GP10: Save Settings to Flash
    ACT_CODE_TRANSPARENT, // GP11: Pass-through
    GP_ACT_RS_LEFT,       // GP12: Right Stick Left
    GP_ACT_RS_UP,         // GP13: Right Stick Up
    GP_ACT_RS_RIGHT,      // GP14: Right Stick Right
    GP_ACT_RS_DOWN,       // GP15: Right Stick Down
    GP_ACT_LS_CLICK,      // GP16: Left Stick Click
    GP_ACT_RS_CLICK,      // GP17: Right Stick Click
    GP_ACT_GUIDE,         // GP18: Xbox Guide
    ACT_CODE_TRANSPARENT, // GP19: Pass-through
    ACT_CODE_FN           // GP20: FN Hold
};

static const uint8_t DEFAULT_LED_COLORS[NUM_BUTTONS][3] = {
    { 255, 0, 0 },     // SW1:  Left Menu (Red) [GP0]
    { 255, 0, 180 },   // SW2:  Side Left (Magenta) [GP1]
    { 255, 0, 0 },     // SW3:  Left Red (Red) [GP2]
    { 0, 255, 0 },     // SW4:  Left Green (Green) [GP3]
    { 0, 100, 255 },   // SW5:  Left Blue (Blue) [GP4]
    { 160, 30, 255 },  // SW6:  Left Bottom 1 (Purple) [GP5]
    { 160, 30, 255 },  // SW7:  Left Bottom 2 (Purple) [GP6]
    { 160, 30, 255 },  // SW8:  Left Bottom 3 (Purple) [GP7]
    { 160, 30, 255 },  // SW9:  Left Bottom 4 (Purple) [GP8]
    { 255, 60, 0 },    // SW10: Left Space / C (Orange) [GP9]
    { 255, 230, 0 },   // SW11: Right Menu (Yellow) [GP10]
    { 255, 0, 180 },   // SW12: Side Right (Magenta) [GP11]
    { 255, 0, 0 },     // SW13: Right Red (Red) [GP12]
    { 0, 255, 0 },     // SW14: Right Green (Green) [GP13]
    { 0, 100, 255 },   // SW15: Right Blue (Blue) [GP14]
    { 160, 30, 255 },  // SW16: Right Bottom 1 (Purple) [GP15]
    { 160, 30, 255 },  // SW17: Right Bottom 2 (Purple) [GP16]
    { 160, 30, 255 },  // SW18: Right Bottom 3 (Purple) [GP17]
    { 160, 30, 255 },  // SW19: Right Bottom 4 (Purple) [GP18]
    { 255, 60, 0 },    // SW20: Right Space / M (Orange) [GP19]
    { 255, 220, 50 }   // SW21: Mid Space / Fn (Gold) [GP20]
};

void config_reset_defaults() {
    memset(&g_config, 0, sizeof(ConfigData));
    g_config.magic = CONFIG_MAGIC;
    g_config.version = CONFIG_VERSION;
    g_config.active_mode = MODE_IO4; // Default to ONGEKI IO4
    g_config.gamepad_invert_x = 1;   // Inverted X-axis default for Gamepad
    g_config.gamepad_smoothing = 35; // 35% smoothing factor
    g_config.kb_led_mode = LED_MODE_REACTIVE;      // Keyboard LED Effect
    g_config.gamepad_led_mode = LED_MODE_REACTIVE; // Gamepad LED Effect
    g_config.led_brightness = 220;   // ~86% master brightness default
    g_config.debounce_ms = 3;        // 3ms eager switch debounce default
    g_config.fn_tap_hold_gp20 = 1;   // Enable Tap-Hold FN on GP20 (Mid Space) by default

    g_config.calib_min = 50;         // Safe minimum for 12-bit ADC (Right limit)
    g_config.calib_max = 4045;       // Safe maximum for 12-bit ADC (Left limit)
    g_config.calib_center = 2048;    // Center
    g_config.deadzone = 8;

    memcpy(g_config.keymap, DEFAULT_KEYMAP, sizeof(DEFAULT_KEYMAP));
    memcpy(g_config.fn_keymap, DEFAULT_FN_KEYMAP, sizeof(DEFAULT_FN_KEYMAP));
    memcpy(g_config.gamepad_map, DEFAULT_GAMEPAD_MAP, sizeof(DEFAULT_GAMEPAD_MAP));
    memcpy(g_config.fn_gamepad_map, DEFAULT_FN_GAMEPAD_MAP, sizeof(DEFAULT_FN_GAMEPAD_MAP));
    memcpy(g_config.led_colors, DEFAULT_LED_COLORS, sizeof(DEFAULT_LED_COLORS));

    // Underglow defaults
    g_config.underglow_left[0] = 0;
    g_config.underglow_left[1] = 200;
    g_config.underglow_left[2] = 255; // Cyan

    g_config.underglow_right[0] = 255;
    g_config.underglow_right[1] = 0;
    g_config.underglow_right[2] = 200; // Magenta

    g_config.checksum = calculate_checksum(g_config);
}

void config_init() {
    memcpy(&g_config, flash_target_contents, sizeof(ConfigData));
    if (g_config.magic != CONFIG_MAGIC || g_config.version != CONFIG_VERSION ||
        g_config.checksum != calculate_checksum(g_config)) {
        config_reset_defaults();
        config_save();
    }
}

ConfigData* config_get_ptr() {
    return &g_config;
}

void config_notify_core1_started(void) {
    g_core1_started = true;
}

void __not_in_flash_func(config_core1_check_lockout)(void) {
    if (g_pause_core1) {
        uint32_t ints = save_and_disable_interrupts();
        g_core1_paused = true;
        __dmb();
        while (g_pause_core1) {
            tight_loop_contents();
        }
        g_core1_paused = false;
        __dmb();
        restore_interrupts(ints);
    }
}

void config_save() {
    g_config.checksum = calculate_checksum(g_config);

    uint8_t buffer[FLASH_PAGE_SIZE];
    memset(buffer, 0xFF, FLASH_PAGE_SIZE);
    memcpy(buffer, &g_config, sizeof(ConfigData));

    // Safely park Core 1 in SRAM if running so XIP Flash never faults
    if (g_core1_started) {
        g_pause_core1 = true;
        __dmb();
        uint32_t start_us = time_us_32();
        while (!g_core1_paused && (time_us_32() - start_us < 50000)) {
            tight_loop_contents();
        }
    }

    uint32_t ints = save_and_disable_interrupts();
    flash_range_erase(FLASH_TARGET_OFFSET, FLASH_SECTOR_SIZE);
    flash_range_program(FLASH_TARGET_OFFSET, buffer, FLASH_PAGE_SIZE);
    restore_interrupts(ints);

    if (g_core1_started) {
        g_pause_core1 = false;
        __dmb();
    }
}

void gpio_buttons_init() {
    uint32_t now_us = time_us_32();
    for (size_t i = 0; i < NUM_BUTTONS; i++) {
        uint8_t pin = BUTTON_PINS[i];
        gpio_init(pin);
        gpio_set_dir(pin, GPIO_IN);
        gpio_pull_up(pin);
        g_debounced_state[i] = false;
        g_last_change_us[i] = now_us;
    }
}

static inline bool update_debounced_switch(size_t idx, uint32_t now_us, uint32_t window_us) {
    bool raw = !gpio_get(BUTTON_PINS[idx]);
    if (window_us == 0) {
        g_debounced_state[idx] = raw;
        return raw;
    }
    if (raw != g_debounced_state[idx]) {
        if ((now_us - g_last_change_us[idx]) >= window_us) {
            g_debounced_state[idx] = raw;
            g_last_change_us[idx] = now_us;
        }
    }
    return g_debounced_state[idx];
}

bool is_button_pressed(uint8_t pin) {
    if (pin >= NUM_BUTTONS) {
        return !gpio_get(pin);
    }
    uint32_t now_us = time_us_32();
    uint32_t window_us = (uint32_t)g_config.debounce_ms * 1000u;
    return update_debounced_switch(pin, now_us, window_us);
}

uint32_t read_all_buttons() {
    uint32_t state = 0;
    uint32_t now_us = time_us_32();
    uint32_t window_us = (uint32_t)g_config.debounce_ms * 1000u;

    for (size_t i = 0; i < NUM_BUTTONS; i++) {
        if (update_debounced_switch(i, now_us, window_us)) {
            state |= (1u << i);
        }
    }
    return state;
}

static void execute_hardware_action(uint8_t code, bool is_gamepad_mode) {
    uint8_t* mode_ptr = is_gamepad_mode ? &g_config.gamepad_led_mode : &g_config.kb_led_mode;

    switch (code) {
        case ACT_CODE_LED_NEXT:
            *mode_ptr = (*mode_ptr + 1) % 6;
            break;
        case ACT_CODE_LED_PREV:
            *mode_ptr = (*mode_ptr + 5) % 6;
            break;
        case ACT_CODE_BRIGHT_UP: {
            if (g_config.led_brightness == 0 && g_saved_brightness_before_mute > 0) {
                g_config.led_brightness = g_saved_brightness_before_mute;
            } else {
                int next_b = (int)g_config.led_brightness + 25;
                g_config.led_brightness = (next_b > 255) ? 255 : (uint8_t)next_b;
            }
            break;
        }
        case ACT_CODE_BRIGHT_DOWN: {
            int next_b = (int)g_config.led_brightness - 25;
            g_config.led_brightness = (next_b < 0) ? 0 : (uint8_t)next_b;
            break;
        }
        case ACT_CODE_LED_TOGGLE:
            if (g_config.led_brightness > 0) {
                g_saved_brightness_before_mute = g_config.led_brightness;
                g_config.led_brightness = 0;
            } else {
                g_config.led_brightness = (g_saved_brightness_before_mute > 0) ? g_saved_brightness_before_mute : 220;
            }
            break;
        case ACT_CODE_SAVE_CONFIG:
            config_save();
            break;
        case ACT_CODE_BOOTSEL:
            reset_usb_boot(0, 0);
            break;
        case ACT_CODE_REBOOT:
            watchdog_reboot(0, 0, 10);
            break;
        default:
            break;
    }
}

bool config_is_fn_layer_active(void) {
    return g_fn_layer_active;
}

void config_process_buttons_with_fn(uint32_t button_mask, bool is_gamepad_mode, uint8_t out_codes[NUM_BUTTONS]) {
    uint32_t now_us = time_us_32();
    const uint8_t* base_map = is_gamepad_mode ? g_config.gamepad_map : g_config.keymap;
    const uint8_t* fn_map   = is_gamepad_mode ? g_config.fn_gamepad_map : g_config.fn_keymap;

    // 1. Check if any button explicitly bound to ACT_CODE_FN (240) is held
    bool explicit_fn_held = false;
    for (int i = 0; i < NUM_BUTTONS; i++) {
        if (((button_mask >> i) & 1u) && base_map[i] == ACT_CODE_FN) {
            explicit_fn_held = true;
            break;
        }
    }

    // 2. Evaluate GP20 (Mid Space) Tap-Hold state machine when enabled
    bool use_gp20_tap_hold = (g_config.fn_tap_hold_gp20 != 0) && (base_map[PIN_MID_SPACE] != ACT_CODE_FN);
    bool gp20_pressed = ((button_mask >> PIN_MID_SPACE) & 1u) != 0;

    if (use_gp20_tap_hold) {
        if (gp20_pressed && !g_gp20_was_pressed) {
            g_gp20_press_start_us = now_us;
            g_gp20_acted_as_fn = false;
        }
        if (gp20_pressed) {
            bool other_pressed = (button_mask & ~(1u << PIN_MID_SPACE)) != 0;
            if (other_pressed || (now_us - g_gp20_press_start_us) >= 180000u) {
                g_gp20_acted_as_fn = true;
            }
        }
        if (!gp20_pressed && g_gp20_was_pressed) {
            // Released before 180ms with no chord -> emit crisp 30ms tap pulse of GP20's base key
            if (!g_gp20_acted_as_fn) {
                g_gp20_tap_pulse_until_us = now_us + 30000u;
            }
        }
        g_gp20_was_pressed = gp20_pressed;
    } else {
        g_gp20_was_pressed = gp20_pressed;
        g_gp20_acted_as_fn = false;
    }

    bool fn_active = explicit_fn_held || (use_gp20_tap_hold && gp20_pressed && g_gp20_acted_as_fn);
    g_fn_layer_active = fn_active;

    // 3. Resolve effective action code for all 21 buttons
    for (int i = 0; i < NUM_BUTTONS; i++) {
        out_codes[i] = 0;

        if (i == PIN_MID_SPACE && use_gp20_tap_hold) {
            // Emit base key during the 30ms tap release pulse
            if ((int32_t)(g_gp20_tap_pulse_until_us - now_us) > 0) {
                out_codes[i] = base_map[i];
            }
            continue;
        }

        if (!((button_mask >> i) & 1u)) {
            g_prev_hw_code[i] = 0;
            continue;
        }

        if (base_map[i] == ACT_CODE_FN) {
            g_prev_hw_code[i] = 0;
            continue;
        }

        uint8_t resolved = 0;
        if (fn_active) {
            uint8_t fn_code = fn_map[i];
            resolved = (fn_code == ACT_CODE_TRANSPARENT) ? base_map[i] : fn_code;
        } else {
            resolved = base_map[i];
        }

        // Handle rising-edge Hardware Control Actions (241..248)
        if (resolved >= ACT_CODE_LED_NEXT && resolved <= ACT_CODE_REBOOT) {
            if (g_prev_hw_code[i] != resolved) {
                execute_hardware_action(resolved, is_gamepad_mode);
            }
            g_prev_hw_code[i] = resolved;
            out_codes[i] = 0;
        } else {
            g_prev_hw_code[i] = 0;
            out_codes[i] = (resolved == ACT_CODE_FN || resolved == ACT_CODE_TRANSPARENT) ? 0 : resolved;
        }
    }
}

uint8_t check_startup_mode() {
    gpio_buttons_init();
    sleep_ms(20); // Settle pull-ups

    // GP8 held -> Enter BOOTSEL immediately
    if (!gpio_get(PIN_LEFT_4)) {
        reset_usb_boot(0, 0);
    }

    // GP1 held -> Enter WebSerial Configurator Mode
    if (!gpio_get(PIN_SIDE_LEFT)) {
        return MODE_CONFIG;
    }

    // GP20 held -> Enter Guided Potentiometer Calibration Mode
    if (!gpio_get(PIN_MID_SPACE)) {
        return MODE_CALIBRATION;
    }

    // GP5 held -> Switch to IO4 Mode
    if (!gpio_get(PIN_LEFT_1)) {
        g_config.active_mode = MODE_IO4;
        config_save();
        return MODE_IO4;
    }

    // GP6 held -> Switch to Keyboard Mode
    if (!gpio_get(PIN_LEFT_2)) {
        g_config.active_mode = MODE_KEYBOARD;
        config_save();
        return MODE_KEYBOARD;
    }

    // GP7 held -> Switch to XInput Mode
    if (!gpio_get(PIN_LEFT_3)) {
        g_config.active_mode = MODE_XINPUT;
        config_save();
        return MODE_XINPUT;
    }

    // Default to stored mode from Flash
    return g_config.active_mode;
}
