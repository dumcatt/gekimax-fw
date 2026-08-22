#include "io4_mode.h"
#include "config.h"
#include "analog_lever.h"
#include "led_driver.h"
#include <string.h>

namespace io4 {
    static OutputReport g_output_data;

    void init() {
        memset(&g_output_data, 0, sizeof(g_output_data));
        g_output_data.coin[0].condition = CoinCondition::Normal;
        g_output_data.coin[1].condition = CoinCondition::Normal;
        led_set_io4_active(true);
    }

    void update_task() {
        if (!tud_hid_ready()) return;

        g_output_data.switches[0] = 0;
        g_output_data.switches[1] = 0;

        bool fn_pressed = is_button_pressed(PIN_MID_SPACE); // GP20 (Fn)

        // Menu & Test/Service logic
        if (is_button_pressed(PIN_LEFT_MENU)) { // GP0
            if (fn_pressed) {
                g_output_data.switches[0] |= (1 << 9); // Test button
            } else {
                g_output_data.switches[1] |= (1 << 14); // Left Menu
            }
        }

        if (is_button_pressed(PIN_RIGHT_MENU)) { // GP10
            if (fn_pressed) {
                g_output_data.switches[0] |= (1 << 6); // Service button
            } else {
                g_output_data.switches[0] |= (1 << 13); // Right Menu
            }
        }

        // Side buttons (Inverted logic for ONGEKI compatibility)
        if (!is_button_pressed(PIN_SIDE_LEFT)) {  // GP1 (Inverted)
            g_output_data.switches[1] |= (1 << 15);
        }
        if (!is_button_pressed(PIN_SIDE_RIGHT)) { // GP11 (Inverted)
            g_output_data.switches[0] |= (1 << 14);
        }

        // Left 3 RGB buttons
        if (is_button_pressed(PIN_LEFT_RED)) {   // GP2 (Left A / Red)
            g_output_data.switches[0] |= (1 << 0);
        }
        if (is_button_pressed(PIN_LEFT_GREEN)) { // GP3 (Left B / Green)
            g_output_data.switches[0] |= (1 << 5);
        }
        if (is_button_pressed(PIN_LEFT_BLUE)) {  // GP4 (Left C / Blue)
            g_output_data.switches[0] |= (1 << 4);
        }

        // Right 3 RGB buttons
        if (is_button_pressed(PIN_RIGHT_RED)) {   // GP12 (Right A / Red)
            g_output_data.switches[0] |= (1 << 1);
        }
        if (is_button_pressed(PIN_RIGHT_GREEN)) { // GP13 (Right B / Green)
            g_output_data.switches[1] |= (1 << 0);
        }
        if (is_button_pressed(PIN_RIGHT_BLUE)) {  // GP14 (Right C / Blue)
            g_output_data.switches[0] |= (1 << 15);
        }

        // Lever Analog reading
        int16_t lever_val = analog_lever_get_io4();
        g_output_data.analog[0] = lever_val;
        g_output_data.rotary[0] = lever_val;

        // Send 64-byte IO4 report
        tud_hid_report(0x01, &g_output_data, sizeof(g_output_data));
    }

    void handle_out_report(const uint8_t *buffer, uint16_t buf_size) {
        if (buf_size < 2) return;
        const auto *data = reinterpret_cast<const InputReport *>(buffer);

        if (data->report_id == 0x10) {
            switch (data->cmd) {
                case SET_COMM_TIMEOUT:
                    g_output_data.system_status = 0x30;
                    break;
                case SET_SAMPLING_COUNT:
                    g_output_data.system_status = 0x30;
                    break;
                case CLEAR_BOARD_STATUS:
                    g_output_data.coin[0].count = 0;
                    g_output_data.coin[0].condition = CoinCondition::Normal;
                    g_output_data.coin[1].count = 0;
                    g_output_data.coin[1].condition = CoinCondition::Normal;
                    g_output_data.system_status = 0x00;
                    break;
                case SET_GENERAL_OUTPUT: {
                    uint32_t ledData = (uint32_t(data->payload[0]) << 16) |
                                       (uint32_t(data->payload[1]) << 8) |
                                       (uint32_t(data->payload[2]));
                    led_set_io4_game_output(ledData);
                    break;
                }
                default:
                    break;
            }
        }
    }
}

// Global TinyUSB HID callbacks
extern "C" uint16_t tud_hid_get_report_cb(uint8_t itf, uint8_t report_id, hid_report_type_t report_type, uint8_t *buffer, uint16_t reqlen) {
    (void)itf; (void)report_id; (void)report_type; (void)buffer; (void)reqlen;
    return 0;
}

extern "C" void tud_hid_set_report_cb(uint8_t itf, uint8_t report_id, hid_report_type_t report_type, uint8_t const *buffer, uint16_t buf_size) {
    (void)itf; (void)report_id; (void)report_type;
    io4::handle_out_report(buffer, buf_size);
}
