#include "hid_mode.h"
#include "config.h"
#include "led_driver.h"
#include "tusb.h"
#include <string.h>

namespace hid_mode {

    static const uint8_t BUTTON_PINS[NUM_BUTTONS] = {
        PIN_LEFT_MENU, PIN_SIDE_LEFT, PIN_LEFT_RED, PIN_LEFT_GREEN, PIN_LEFT_BLUE,
        PIN_LEFT_1, PIN_LEFT_2, PIN_LEFT_3, PIN_LEFT_4, PIN_LEFT_SPACE,
        PIN_RIGHT_MENU, PIN_SIDE_RIGHT, PIN_RIGHT_RED, PIN_RIGHT_GREEN, PIN_RIGHT_BLUE,
        PIN_RIGHT_1, PIN_RIGHT_2, PIN_RIGHT_3, PIN_RIGHT_4, PIN_RIGHT_SPACE,
        PIN_MID_SPACE
    };

    void init() {
        led_set_io4_active(false);
    }

    void update_task() {
        if (!tud_hid_ready()) return;

        // 232-key NKRO bitmap (29 bytes) supporting all 21 buttons held simultaneously
        uint8_t nkro_report[29];
        memset(nkro_report, 0, sizeof(nkro_report));

        const auto& cfg = config_get();

        for (int i = 0; i < NUM_BUTTONS; i++) {
            if (is_button_pressed(BUTTON_PINS[i])) {
                uint8_t kc = cfg.keymap[i];
                if (kc > 0 && kc < 232) {
                    nkro_report[kc / 8] |= (1 << (kc % 8));
                }
            }
        }

        tud_hid_report(0, nkro_report, sizeof(nkro_report));
    }
}
