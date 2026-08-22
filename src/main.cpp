#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/watchdog.h"
#include "config.h"
#include "led_driver.h"
#include "analog_lever.h"
#include "usb/usb_descriptors.h"
#include "usb/io4_mode.h"
#include "usb/hid_mode.h"
#include "usb/xinput_mode.h"
#include "usb/serial_config.h"
#include "tusb.h"

static uint8_t g_active_mode = MODE_IO4;

// Core 1 handles reactive LED animations and WS2812 rendering
void core1_entry() {
    while (true) {
        if (g_active_mode == MODE_CONFIG) {
            led_config_mode_animation();
            sleep_ms(16); // ~60 FPS animation in config mode
        } else {
            uint32_t button_mask = read_all_buttons();
            led_update_reactive(button_mask);
            sleep_ms(8); // ~120 FPS LED update rate
        }
    }
}

int main() {
    stdio_init_all();
    config_init();
    led_driver_init();
    analog_lever_init();

    // Check startup buttons:
    // GP1: Config Mode, GP5: IO4, GP6: Keyboard, GP7: XInput, GP8: BOOTSEL, GP20: Calibration
    uint8_t boot_mode = check_startup_mode();

    if (boot_mode == MODE_CALIBRATION) {
        analog_lever_run_guided_calibration();
        boot_mode = config_get().active_mode;
    }

    g_active_mode = boot_mode;

    // Show mode color flash on startup
    led_flash_mode(g_active_mode);

    // Initialize USB Descriptors & Stack
    usb_descriptors_init(g_active_mode);
    tusb_init();

    // Mode-specific initializations
    if (g_active_mode == MODE_IO4) {
        io4::init();
    } else if (g_active_mode == MODE_KEYBOARD) {
        hid_mode::init();
    } else if (g_active_mode == MODE_XINPUT) {
        xinput_mode::init();
    } else if (g_active_mode == MODE_CONFIG) {
        serial_config_init();
    }

    // Start Core 1 for decoupled LED rendering
    multicore_launch_core1(core1_entry);

    // Core 0: High-priority 1000Hz USB task and input polling
    while (true) {
        tud_task();
        analog_lever_update();

        if (g_active_mode == MODE_IO4) {
            io4::update_task();
        } else if (g_active_mode == MODE_KEYBOARD) {
            hid_mode::update_task();
        } else if (g_active_mode == MODE_XINPUT) {
            xinput_mode::update_task();
        } else if (g_active_mode == MODE_CONFIG) {
            serial_config_update_task();
        }
    }

    return 0;
}
