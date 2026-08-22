#include "config.h"
#include "pico/stdlib.h"
#include "pico/bootrom.h"
#include "hardware/flash.h"
#include "hardware/sync.h"
#include "tusb.h"
#include <string.h>

#define FLASH_TARGET_OFFSET (PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE)
static const uint8_t *flash_target_contents = (const uint8_t *)(XIP_BASE + FLASH_TARGET_OFFSET);

static ConfigData g_config;

static uint32_t calculate_checksum(const ConfigData& cfg) {
    const uint32_t *data = (const uint32_t *)&cfg;
    uint32_t sum = 0;
    size_t words = (sizeof(ConfigData) - sizeof(uint32_t)) / sizeof(uint32_t);
    for (size_t i = 0; i < words; i++) {
        sum ^= data[i];
    }
    return sum;
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
    g_config.magic = CONFIG_MAGIC;
    g_config.version = CONFIG_VERSION;
    g_config.active_mode = MODE_IO4; // Default to ONGEKI IO4
    g_config.gamepad_invert_x = 1;   // Inverted X-axis default for Gamepad
    g_config.gamepad_smoothing = 35;  // 35% smoothing factor
    g_config.kb_led_mode = LED_MODE_REACTIVE;      // Keyboard LED Effect
    g_config.gamepad_led_mode = LED_MODE_REACTIVE; // Gamepad LED Effect
    memset(g_config.reserved1, 0, sizeof(g_config.reserved1));

    g_config.calib_min = 50;     // Safe minimum for 12-bit ADC (0..4095)
    g_config.calib_max = 4045;   // Safe maximum for 12-bit ADC
    g_config.calib_center = 2048;// Center
    g_config.deadzone = 8;

    memcpy(g_config.keymap, DEFAULT_KEYMAP, sizeof(DEFAULT_KEYMAP));
    memset(g_config.reserved2, 0, sizeof(g_config.reserved2));
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

void config_save() {
    g_config.checksum = calculate_checksum(g_config);
    
    uint8_t buffer[FLASH_PAGE_SIZE];
    memset(buffer, 0xFF, FLASH_PAGE_SIZE);
    memcpy(buffer, &g_config, sizeof(ConfigData));

    uint32_t ints = save_and_disable_interrupts();
    flash_range_erase(FLASH_TARGET_OFFSET, FLASH_SECTOR_SIZE);
    flash_range_program(FLASH_TARGET_OFFSET, buffer, FLASH_PAGE_SIZE);
    restore_interrupts(ints);
}

void gpio_buttons_init() {
    const uint8_t pins[] = {
        PIN_LEFT_MENU, PIN_SIDE_LEFT, PIN_LEFT_RED, PIN_LEFT_GREEN, PIN_LEFT_BLUE,
        PIN_LEFT_1, PIN_LEFT_2, PIN_LEFT_3, PIN_LEFT_4, PIN_LEFT_SPACE,
        PIN_RIGHT_MENU, PIN_SIDE_RIGHT, PIN_RIGHT_RED, PIN_RIGHT_GREEN, PIN_RIGHT_BLUE,
        PIN_RIGHT_1, PIN_RIGHT_2, PIN_RIGHT_3, PIN_RIGHT_4, PIN_RIGHT_SPACE,
        PIN_MID_SPACE
    };

    for (uint8_t pin : pins) {
        gpio_init(pin);
        gpio_set_dir(pin, GPIO_IN);
        gpio_pull_up(pin);
    }
}

bool is_button_pressed(uint8_t pin) {
    return !gpio_get(pin);
}

uint32_t read_all_buttons() {
    uint32_t state = 0;
    const uint8_t pins[] = {
        PIN_LEFT_MENU, PIN_SIDE_LEFT, PIN_LEFT_RED, PIN_LEFT_GREEN, PIN_LEFT_BLUE,
        PIN_LEFT_1, PIN_LEFT_2, PIN_LEFT_3, PIN_LEFT_4, PIN_LEFT_SPACE,
        PIN_RIGHT_MENU, PIN_SIDE_RIGHT, PIN_RIGHT_RED, PIN_RIGHT_GREEN, PIN_RIGHT_BLUE,
        PIN_RIGHT_1, PIN_RIGHT_2, PIN_RIGHT_3, PIN_RIGHT_4, PIN_RIGHT_SPACE,
        PIN_MID_SPACE
    };

    for (size_t i = 0; i < sizeof(pins); i++) {
        if (!gpio_get(pins[i])) {
            state |= (1u << i);
        }
    }
    return state;
}

uint8_t check_startup_mode() {
    gpio_buttons_init();
    sleep_ms(20); // Settle pull-ups

    // GP8 held -> Enter BOOTSEL immediately
    if (is_button_pressed(PIN_LEFT_4)) {
        reset_usb_boot(0, 0);
    }

    // GP1 held -> Enter WebSerial Configurator Mode
    if (is_button_pressed(PIN_SIDE_LEFT)) {
        return MODE_CONFIG;
    }

    // GP20 held -> Enter Guided Potentiometer Calibration Mode
    if (is_button_pressed(PIN_MID_SPACE)) {
        return MODE_CALIBRATION;
    }

    // GP5 held -> Switch to IO4 Mode
    if (is_button_pressed(PIN_LEFT_1)) {
        g_config.active_mode = MODE_IO4;
        config_save();
        return MODE_IO4;
    }

    // GP6 held -> Switch to Keyboard Mode
    if (is_button_pressed(PIN_LEFT_2)) {
        g_config.active_mode = MODE_KEYBOARD;
        config_save();
        return MODE_KEYBOARD;
    }

    // GP7 held -> Switch to XInput Mode
    if (is_button_pressed(PIN_LEFT_3)) {
        g_config.active_mode = MODE_XINPUT;
        config_save();
        return MODE_XINPUT;
    }

    // Default to stored mode from Flash
    return g_config.active_mode;
}
