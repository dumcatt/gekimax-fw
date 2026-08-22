#include "xinput_mode.h"
#include "config.h"
#include "analog_lever.h"
#include "led_driver.h"
#include "tusb.h"
#include "device/usbd_pvt.h"
#include <string.h>

static uint8_t g_xinput_ep_in = 0;
static uint8_t g_xinput_ep_out = 0;

struct XInputReport {
    uint8_t report_id;    // 0x00
    uint8_t report_size;  // 0x14 (20 bytes)
    uint8_t buttons1;     // D-Pad, Start, Back, Thumbs
    uint8_t buttons2;     // LB, RB, Guide, A, B, X, Y
    uint8_t left_trigger; // 0..255
    uint8_t right_trigger;// 0..255
    int16_t thumb_lx;     // Lever X (-32768..32767)
    int16_t thumb_ly;     // 0
    int16_t thumb_rx;     // 0
    int16_t thumb_ry;     // 0
    uint8_t reserved[6];  // 0
} __attribute__((packed));

//--------------------------------------------------------------------+
// Custom XInput Class Driver for TinyUSB
//--------------------------------------------------------------------+

static void xinput_driver_init(void) {}

static void xinput_driver_reset(uint8_t rhport) {
    (void)rhport;
    g_xinput_ep_in = 0;
    g_xinput_ep_out = 0;
}

static uint16_t xinput_driver_open(uint8_t rhport, tusb_desc_interface_t const *itf_desc, uint16_t max_len) {
    if (itf_desc->bInterfaceClass != 0xFF || itf_desc->bInterfaceSubClass != 0x5D) {
        return 0;
    }

    uint16_t const drv_len = sizeof(tusb_desc_interface_t) + itf_desc->bNumEndpoints * sizeof(tusb_desc_endpoint_t) + 16;
    TU_VERIFY(max_len >= drv_len, 0);

    uint8_t const * p_desc = tu_desc_next(itf_desc);
    uint8_t found_endpoints = 0;
    while ((found_endpoints < itf_desc->bNumEndpoints) && (p_desc < ((uint8_t const *)itf_desc + max_len))) {
        tusb_desc_endpoint_t const * desc_ep = (tusb_desc_endpoint_t const *) p_desc;
        if (TUSB_DESC_ENDPOINT == tu_desc_type(desc_ep)) {
            TU_ASSERT(usbd_edpt_open(rhport, desc_ep));
            if (tu_edpt_dir(desc_ep->bEndpointAddress) == TUSB_DIR_IN) {
                g_xinput_ep_in = desc_ep->bEndpointAddress;
            } else {
                g_xinput_ep_out = desc_ep->bEndpointAddress;
            }
            found_endpoints++;
        }
        p_desc = tu_desc_next(p_desc);
    }
    return drv_len;
}

static bool xinput_driver_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const * request) {
    if (stage != CONTROL_STAGE_SETUP) return true;

    if (request->bmRequestType_bit.type == TUSB_REQ_TYPE_VENDOR) {
        if (request->bRequest == 0x01 && (request->bmRequestType & 0x80)) {
            static uint8_t const xinput_caps[] = {
                0x00, 0x14, 0xFF, 0x00, 0xFF, 0xFF, 0xFF, 0xFF,
                0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00
            };
            return tud_control_xfer(rhport, request, (void*)(uintptr_t)xinput_caps, sizeof(xinput_caps));
        }
    }
    return false;
}

static bool xinput_driver_xfer_cb(uint8_t rhport, uint8_t ep_addr, xfer_result_t result, uint32_t xferred_bytes) {
    (void)rhport;
    (void)ep_addr;
    (void)result;
    (void)xferred_bytes;
    return true;
}

static usbd_class_driver_t const xinput_driver = {
#if CFG_TUSB_DEBUG >= 2
    .name             = "XINPUT",
#endif
    .init             = xinput_driver_init,
    .reset            = xinput_driver_reset,
    .open             = xinput_driver_open,
    .control_xfer_cb  = xinput_driver_control_xfer_cb,
    .xfer_cb          = xinput_driver_xfer_cb,
    .sof              = NULL
};

extern "C" usbd_class_driver_t const *usbd_app_driver_get_cb(uint8_t *driver_count) {
    *driver_count = 1;
    return &xinput_driver;
}

//--------------------------------------------------------------------+
// XInput Mode Operations
//--------------------------------------------------------------------+

namespace xinput_mode {

    void init() {
        led_set_io4_active(false);
    }

    void update_task() {
        if (g_xinput_ep_in == 0) return;
        if (!tud_ready()) return;
        if (usbd_edpt_busy(0, g_xinput_ep_in)) return;

        XInputReport report;
        memset(&report, 0, sizeof(report));
        report.report_id = 0x00;
        report.report_size = sizeof(XInputReport);

        // Buttons 1 (D-Pad, Menu, Sticks)
        if (is_button_pressed(PIN_LEFT_GREEN)) report.buttons1 |= 0x01; // GP3:  DPAD_UP
        if (is_button_pressed(PIN_LEFT_1))     report.buttons1 |= 0x02; // GP5:  DPAD_DOWN
        if (is_button_pressed(PIN_LEFT_RED))   report.buttons1 |= 0x04; // GP2:  DPAD_LEFT
        if (is_button_pressed(PIN_LEFT_BLUE))  report.buttons1 |= 0x08; // GP4:  DPAD_RIGHT
        if (is_button_pressed(PIN_RIGHT_MENU)) report.buttons1 |= 0x10; // GP10: START
        if (is_button_pressed(PIN_LEFT_MENU))  report.buttons1 |= 0x20; // GP0:  BACK
        if (is_button_pressed(PIN_LEFT_2))     report.buttons1 |= 0x40; // GP6:  LEFT_THUMB (LS)
        if (is_button_pressed(PIN_LEFT_3))     report.buttons1 |= 0x80; // GP7:  RIGHT_THUMB (RS)

        // Buttons 2 (Bumpers, Guide, Face buttons)
        if (is_button_pressed(PIN_SIDE_LEFT))  report.buttons2 |= 0x01; // GP1:  LB
        if (is_button_pressed(PIN_SIDE_RIGHT)) report.buttons2 |= 0x02; // GP11: RB
        if (is_button_pressed(PIN_MID_SPACE))  report.buttons2 |= 0x04; // GP20: GUIDE / Home
        if (is_button_pressed(PIN_RIGHT_BLUE)) report.buttons2 |= 0x10; // GP14: A
        if (is_button_pressed(PIN_RIGHT_1) ||
            is_button_pressed(PIN_LEFT_4))     report.buttons2 |= 0x20; // GP15/GP8: B
        if (is_button_pressed(PIN_RIGHT_RED))  report.buttons2 |= 0x40; // GP12: X
        if (is_button_pressed(PIN_RIGHT_GREEN))report.buttons2 |= 0x80; // GP13: Y

        // Triggers
        if (is_button_pressed(PIN_LEFT_SPACE))  report.left_trigger = 255;  // GP9:  LT
        if (is_button_pressed(PIN_RIGHT_SPACE)) report.right_trigger = 255; // GP19: RT

        // Right Stick D-Pad/Axes mapping for rhythm game extra buttons
        if (is_button_pressed(PIN_RIGHT_2)) report.thumb_ry = 32767;  // GP16: Right Stick Up
        if (is_button_pressed(PIN_RIGHT_3)) report.thumb_rx = -32768; // GP17: Right Stick Left
        if (is_button_pressed(PIN_RIGHT_4)) report.thumb_rx = 32767;  // GP18: Right Stick Right

        // Lever Analog mapping
        report.thumb_lx = analog_lever_get_xinput();

        usbd_edpt_claim(0, g_xinput_ep_in);
        usbd_edpt_xfer(0, g_xinput_ep_in, (uint8_t*)&report, sizeof(report));
        usbd_edpt_release(0, g_xinput_ep_in);
    }
}
