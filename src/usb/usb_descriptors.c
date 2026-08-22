#include "usb_descriptors.h"
#include "config.h"
#include <string.h>

static uint8_t g_usb_mode = MODE_IO4;

void usb_descriptors_init(uint8_t mode) {
    g_usb_mode = mode;
}

//--------------------------------------------------------------------+
// 1. IO4 Descriptors (Sega ONGEKI - Exact lkick-io4 replica)
//--------------------------------------------------------------------+

static tusb_desc_device_t const desc_device_io4 = {
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = 0x0200,
    .bDeviceClass       = 0x00,
    .bDeviceSubClass    = 0x00,
    .bDeviceProtocol    = 0x00,
    .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor           = 0x0CA3, // SEGA
    .idProduct          = 0x0021, // IO4
    .bcdDevice          = 0x0100,
    .iManufacturer      = 0x01,   // "SEGA"
    .iProduct           = 0x02,   // "I/O CONTROL BD;15257;01;90;1831;6679A;00;GOUT=14_ADIN=8,E_ROTIN=4_COININ=2_SWIN=2,E_UQ1=41,6;"
    .iSerialNumber      = 0x03,   // ""
    .bNumConfigurations = 0x01
};

static uint8_t const desc_hid_report_io4[] = {
    0x05, 0x01,                     // Usage Page (Generic Desktop Ctrls)
    0x09, 0x04,                     // Usage (Joystick)
    0xA1, 0x01,                     // Collection (Application)
    0x85, 0x01,                     //   Report ID (1)
    0x09, 0x01,                     //   Usage (Pointer)
    0xA1, 0x00,                     //   Collection (Physical)
    0x09, 0x30,                     //     Usage (X)
    0x09, 0x31,                     //     Usage (Y)
    0x09, 0x30,                     //     Usage (X)
    0x09, 0x31,                     //     Usage (Y)
    0x09, 0x30,                     //     Usage (X)
    0x09, 0x31,                     //     Usage (Y)
    0x09, 0x30,                     //     Usage (X)
    0x09, 0x31,                     //     Usage (Y)
    0x09, 0x33,                     //     Usage (Rx)
    0x09, 0x34,                     //     Usage (Ry)
    0x09, 0x33,                     //     Usage (Rx)
    0x09, 0x34,                     //     Usage (Ry)
    0x09, 0x36,                     //     Usage (Slider)
    0x09, 0x36,                     //     Usage (Slider)
    0x15, 0x00,                     //     Logical Minimum (0)
    0x27, 0xFF, 0xFF, 0x00, 0x00,   //     Logical Maximum (65534)
    0x35, 0x00,                     //     Physical Minimum (0)
    0x47, 0xFF, 0xFF, 0x00, 0x00,   //     Physical Maximum (65534)
    0x95, 0x0E,                     //     Report Count (14)
    0x75, 0x10,                     //     Report Size (16)
    0x81, 0x02,                     //     Input (Data,Var,Abs)
    0xC0,                           //   End Collection
    0x05, 0x02,                     //   Usage Page (Sim Ctrls)
    0x05, 0x09,                     //   Usage Page (Button)
    0x19, 0x01,                     //   Usage Minimum (0x01)
    0x29, 0x30,                     //   Usage Maximum (0x30)
    0x15, 0x00,                     //   Logical Minimum (0)
    0x25, 0x01,                     //   Logical Maximum (1)
    0x45, 0x01,                     //   Physical Maximum (1)
    0x75, 0x01,                     //   Report Size (1)
    0x95, 0x30,                     //   Report Count (48)
    0x81, 0x02,                     //   Input (Data,Var,Abs)
    0x09, 0x00,                     //   Usage (0x00)
    0x75, 0x08,                     //   Report Size (8)
    0x95, 0x1D,                     //   Report Count (29)
    0x81, 0x01,                     //   Input (Const,Array,Abs)
    0x06, 0xA0, 0xFF,               //   Usage Page (Vendor Defined 0xFFA0)
    0x09, 0x00,                     //   Usage (0x00)
    0x85, 0x10,                     //   Report ID (16)
    0xA1, 0x01,                     //   Collection (Application)
    0x09, 0x00,                     //     Usage (0x00)
    0x15, 0x00,                     //     Logical Minimum (0)
    0x26, 0xFF, 0x00,               //     Logical Maximum (255)
    0x75, 0x08,                     //     Report Size (8)
    0x95, 0x3F,                     //     Report Count (63)
    0x91, 0x02,                     //     Output (Data,Var,Abs)
    0xC0,                           //   End Collection
    0xC0                            // End Collection
};

#define CONFIG_IO4_TOTAL_LEN (TUD_CONFIG_DESC_LEN + TUD_HID_INOUT_DESC_LEN)

static uint8_t const desc_config_io4[] = {
    TUD_CONFIG_DESCRIPTOR(1, 1, 0, CONFIG_IO4_TOTAL_LEN, 0x00, 100),
    TUD_HID_INOUT_DESCRIPTOR(0, 6, HID_ITF_PROTOCOL_NONE, sizeof(desc_hid_report_io4), 0x01, 0x81, 64, 6)
};

//--------------------------------------------------------------------+
// 2. Keyboard Descriptors (NKRO - Full N-Key Rollover Bitmap)
//--------------------------------------------------------------------+

static tusb_desc_device_t const desc_device_km = {
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = 0x0200,
    .bDeviceClass       = 0x00,
    .bDeviceSubClass    = 0x00,
    .bDeviceProtocol    = 0x00,
    .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor           = 0xCAFE,
    .idProduct          = 0x4002,
    .bcdDevice          = 0x0100,
    .iManufacturer      = 0x01,
    .iProduct           = 0x02,
    .iSerialNumber      = 0x03,
    .bNumConfigurations = 0x01
};

static uint8_t const desc_hid_report_keyboard[] = {
    0x05, 0x01,         // USAGE_PAGE (Generic Desktop)
    0x09, 0x06,         // USAGE (Keyboard)
    0xA1, 0x01,         // COLLECTION (Application)
    0x05, 0x07,         //   USAGE_PAGE (Keyboard)
    0x19, 0x00,         //   USAGE_MINIMUM (0)
    0x29, 0xE7,         //   USAGE_MAXIMUM (231 - Right GUI)
    0x15, 0x00,         //   LOGICAL_MINIMUM (0)
    0x25, 0x01,         //   LOGICAL_MAXIMUM (1)
    0x75, 0x01,         //   REPORT_SIZE (1 bit)
    0x95, 0xE8,         //   REPORT_COUNT (232 bits = 29 bytes)
    0x81, 0x02,         //   INPUT (Data,Var,Abs)
    0xC0                // END_COLLECTION
};

#define CONFIG_KM_TOTAL_LEN (TUD_CONFIG_DESC_LEN + TUD_HID_DESC_LEN)

static uint8_t const desc_config_km[] = {
    TUD_CONFIG_DESCRIPTOR(1, 1, 0, CONFIG_KM_TOTAL_LEN, 0x00, 500),
    TUD_HID_DESCRIPTOR(0, 0, HID_ITF_PROTOCOL_NONE, sizeof(desc_hid_report_keyboard), 0x81, 32, 1)
};

//--------------------------------------------------------------------+
// 3. XInput Descriptors (Standard Xbox 360 Controller)
//--------------------------------------------------------------------+

static tusb_desc_device_t const desc_device_xinput = {
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = 0x0200,
    .bDeviceClass       = 0xFF,
    .bDeviceSubClass    = 0xFF,
    .bDeviceProtocol    = 0xFF,
    .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor           = 0x045E, // Microsoft
    .idProduct          = 0x028E, // Xbox 360 Controller
    .bcdDevice          = 0x0572,
    .iManufacturer      = 0x01,
    .iProduct           = 0x02,
    .iSerialNumber      = 0x03,
    .bNumConfigurations = 0x01
};

static uint8_t const desc_config_xinput[] = {
    0x09, 0x02, 0x30, 0x00, 0x01, 0x01, 0x00, 0x80, 0xFA,
    0x09, 0x04, 0x00, 0x00, 0x02, 0xFF, 0x5D, 0x01, 0x00,
    0x10, 0x21, 0x10, 0x01, 0x01, 0x24, 0x81, 0x14, 0x03, 0x00, 0x03, 0x13, 0x02, 0x00, 0x03, 0x00,
    0x07, 0x05, 0x81, 0x03, 0x20, 0x00, 0x01,
    0x07, 0x05, 0x02, 0x03, 0x20, 0x00, 0x08
};

//--------------------------------------------------------------------+
// 4. Config Mode Descriptors (USB CDC Serial for WebSerial)
//--------------------------------------------------------------------+

static tusb_desc_device_t const desc_device_config = {
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = 0x0200,
    .bDeviceClass       = TUSB_CLASS_MISC,
    .bDeviceSubClass    = MISC_SUBCLASS_COMMON,
    .bDeviceProtocol    = MISC_PROTOCOL_IAD,
    .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor           = 0xCAFE,
    .idProduct          = 0x4004,
    .bcdDevice          = 0x0100,
    .iManufacturer      = 0x01,
    .iProduct           = 0x02,
    .iSerialNumber      = 0x03,
    .bNumConfigurations = 0x01
};

#define CONFIG_CDC_TOTAL_LEN (TUD_CONFIG_DESC_LEN + TUD_CDC_DESC_LEN)

static uint8_t const desc_config_cdc[] = {
    // Config number, interface count, string index, total length, attribute, power in mA
    TUD_CONFIG_DESCRIPTOR(1, 2, 0, CONFIG_CDC_TOTAL_LEN, 0x00, 500),
    // CDC: Interface number, string index, EP notification address and size, EP data address (out, in) and size.
    TUD_CDC_DESCRIPTOR(0, 4, 0x81, 8, 0x02, 0x82, 64)
};

//--------------------------------------------------------------------+
// TinyUSB Callbacks
//--------------------------------------------------------------------+

uint8_t const * tud_descriptor_device_cb(void) {
    switch (g_usb_mode) {
        case MODE_IO4:
            return (uint8_t const *)&desc_device_io4;
        case MODE_KEYBOARD:
            return (uint8_t const *)&desc_device_km;
        case MODE_XINPUT:
            return (uint8_t const *)&desc_device_xinput;
        case MODE_CONFIG:
            return (uint8_t const *)&desc_device_config;
        default:
            return (uint8_t const *)&desc_device_io4;
    }
}

uint8_t const * tud_hid_descriptor_report_cb(uint8_t itf) {
    (void)itf;
    if (g_usb_mode == MODE_KEYBOARD) {
        return desc_hid_report_keyboard;
    }
    return desc_hid_report_io4;
}

uint8_t const * tud_descriptor_configuration_cb(uint8_t index) {
    (void)index;
    switch (g_usb_mode) {
        case MODE_IO4:
            return desc_config_io4;
        case MODE_KEYBOARD:
            return desc_config_km;
        case MODE_XINPUT:
            return desc_config_xinput;
        case MODE_CONFIG:
            return desc_config_cdc;
        default:
            return desc_config_io4;
    }
}

// String Descriptors
static char const *string_desc_io4[] = {
    (const char[]) { 0x09, 0x04 }, // 0: English
    "SEGA",                         // 1: Manufacturer
    "I/O CONTROL BD;15257;01;90;1831;6679A;00;GOUT=14_ADIN=8,E_ROTIN=4_COININ=2_SWIN=2,E_UQ1=41,6;", // 2: Product Name
    "",                             // 3: Serial
    "AIME READER @COM1",           // 4: CDC1
    "LED BOARD @COM3",             // 5: CDC2
    "I/O CONTROL BD;15257;01;90;1831;6679A;00;GOUT=14_ADIN=8,E_ROTIN=4_COININ=2_SWIN=2,E_UQ1=41,6;"  // 6: IO4 Board String
};

static char const *string_desc_km[] = {
    (const char[]) { 0x09, 0x04 },
    "Gekimax",
    "Gekimax Keyboard NKRO",
    "GEKIMAX001"
};

static char const *string_desc_xinput[] = {
    (const char[]) { 0x09, 0x04 },
    "Microsoft Corp.",
    "Xbox 360 Controller",
    "1.0"
};

static char const *string_desc_config[] = {
    (const char[]) { 0x09, 0x04 },
    "Gekimax",
    "Gekimax Configurator",
    "GEKICFG01",
    "Gekimax Serial Interface"
};

static uint16_t _desc_str[128];

uint16_t const * tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
    (void)langid;
    char const **str_arr = string_desc_io4;
    size_t total_strings = sizeof(string_desc_io4) / sizeof(string_desc_io4[0]);

    if (g_usb_mode == MODE_KEYBOARD) {
        str_arr = string_desc_km;
        total_strings = sizeof(string_desc_km) / sizeof(string_desc_km[0]);
    } else if (g_usb_mode == MODE_XINPUT) {
        str_arr = string_desc_xinput;
        total_strings = sizeof(string_desc_xinput) / sizeof(string_desc_xinput[0]);
    } else if (g_usb_mode == MODE_CONFIG) {
        str_arr = string_desc_config;
        total_strings = sizeof(string_desc_config) / sizeof(string_desc_config[0]);
    }

    uint8_t count = 0;
    if (index == 0) {
        memcpy(&_desc_str[1], str_arr[0], 2);
        count = 1;
    } else {
        if (index >= total_strings) return NULL;
        const char *str = str_arr[index];
        size_t len = strlen(str);
        if (len > 120) len = 120;
        for (size_t i = 0; i < len; i++) {
            _desc_str[1 + i] = (uint16_t)str[i];
        }
        count = (uint8_t)len;
    }

    _desc_str[0] = (uint16_t)((TUSB_DESC_STRING << 8) | (2 * count + 2));
    return _desc_str;
}
