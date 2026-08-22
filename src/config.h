#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Pin definitions matching Gekimax PCB
#define PIN_LEFT_MENU    0   // SW1, D1
#define PIN_SIDE_LEFT    1   // SW2, D2
#define PIN_LEFT_RED     2   // SW3, D3 (Left A)
#define PIN_LEFT_GREEN   3   // SW4, D4 (Left B)
#define PIN_LEFT_BLUE    4   // SW5, D5 (Left C)
#define PIN_LEFT_1       5   // SW6, D6 (Left Bottom 1)
#define PIN_LEFT_2       6   // SW7, D7 (Left Bottom 2)
#define PIN_LEFT_3       7   // SW8, D8 (Left Bottom 3)
#define PIN_LEFT_4       8   // SW9, D9 (Left Bottom 4)
#define PIN_LEFT_SPACE   9   // SW10, D10 (Left Space / C)
#define PIN_RIGHT_MENU   10  // SW11, D11
#define PIN_SIDE_RIGHT   11  // SW12, D12
#define PIN_RIGHT_RED    12  // SW13, D13 (Right A)
#define PIN_RIGHT_GREEN  13  // SW14, D14 (Right B)
#define PIN_RIGHT_BLUE   14  // SW15, D15 (Right C)
#define PIN_RIGHT_1      15  // SW16, D16 (Right Bottom 1)
#define PIN_RIGHT_2      16  // SW17, D17 (Right Bottom 2)
#define PIN_RIGHT_3      17  // SW18, D18 (Right Bottom 3)
#define PIN_RIGHT_4      18  // SW19, D19 (Right Bottom 4)
#define PIN_RIGHT_SPACE  19  // SW20, D20 (Right Space / M)
#define PIN_MID_SPACE    20  // SW21, D21 (Fn)

#define PIN_LED_DATA     21  // 27 WS2812/SK6812 LEDs
#define PIN_POT_ADC      26  // ADC0 (Potentiometer Lever)

#define NUM_BUTTONS      21
#define NUM_LEDS         27

#define MODE_IO4         0   // Sega ONGEKI IO4 Emulation
#define MODE_KEYBOARD    1   // Keyboard Mode (NKRO)
#define MODE_XINPUT      2   // Xbox 360 Gamepad Mode
#define MODE_CALIBRATION 3   // Lever Guided Calibration Mode (GP20 held on boot)
#define MODE_CONFIG      4   // WebSerial Configurator Mode (GP1 held on boot)

// LED Effects for Keyboard & Gamepad Modes
#define LED_MODE_REACTIVE          0   // Lights up on press, fades out
#define LED_MODE_BREATHING         1   // Smooth pulsating colors
#define LED_MODE_INVERTED_REACTIVE 2   // Solid on, fades out on press
#define LED_MODE_RAINBOW           3   // Animated RGB rainbow cycle
#define LED_MODE_SOLID             4   // Always solid assigned colors
#define LED_MODE_OFF               5   // LEDs disabled/off

#define CONFIG_MAGIC 0x47454B35 // 'GEK5'
#define CONFIG_VERSION 5

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint8_t  active_mode;
    uint8_t  gamepad_invert_x;
    uint8_t  gamepad_smoothing; // 0..100 (e.g. 35 -> alpha 0.35)
    uint8_t  kb_led_mode;       // LED effect in Keyboard Mode (0..5)
    uint8_t  gamepad_led_mode;  // LED effect in Gamepad Mode (0..5)
    uint8_t  reserved1[2];
    
    // Potentiometer Calibration Bounds
    uint16_t calib_min;
    uint16_t calib_max;
    uint16_t calib_center;
    uint16_t deadzone;

    // Keymap for 21 buttons (GP0..GP20, 0 = None/Disabled)
    uint8_t  keymap[NUM_BUTTONS];
    uint8_t  reserved2[3];

    // Reactive / Assigned LED Colors for 21 buttons: [r, g, b]
    uint8_t  led_colors[NUM_BUTTONS][3];

    // Underglow Colors: [r, g, b]
    uint8_t  underglow_left[3];
    uint8_t  underglow_right[3];

    uint32_t checksum;
} ConfigData;

void config_init(void);
ConfigData* config_get_ptr(void);
void config_save(void);
void config_reset_defaults(void);
uint8_t check_startup_mode(void);
void gpio_buttons_init(void);
bool is_button_pressed(uint8_t pin);
uint32_t read_all_buttons(void);

#ifdef __cplusplus
}

inline ConfigData& config_get() {
    return *config_get_ptr();
}
#endif
