#include "hid_mode.h"
#include "config.h"
#include "led_driver.h"
#include "tusb.h"
#include <string.h>

namespace hid_mode {

    void init() {
        led_set_io4_active(false);
    }

    void update_task() {
        if (!tud_hid_ready()) return;

        // 232-key NKRO bitmap (29 bytes) supporting all 21 buttons held simultaneously
        uint8_t nkro_report[29];
        memset(nkro_report, 0, sizeof(nkro_report));

        uint32_t mask = read_all_buttons();
        uint8_t resolved_codes[NUM_BUTTONS];
        config_process_buttons_with_fn(mask, false, resolved_codes);

        for (int i = 0; i < NUM_BUTTONS; i++) {
            uint8_t kc = resolved_codes[i];
            if (kc > 0 && kc < 232) {
                nkro_report[kc / 8] |= (1 << (kc % 8));
            }
        }

        tud_hid_report(0, nkro_report, sizeof(nkro_report));
    }
}
