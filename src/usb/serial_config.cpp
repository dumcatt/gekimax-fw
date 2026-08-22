#include "serial_config.h"
#include "config.h"
#include "led_driver.h"
#include "analog_lever.h"
#include "tusb.h"
#include "hardware/watchdog.h"
#include "pico/bootrom.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static char g_rx_buf[2048];
static size_t g_rx_pos = 0;

static void send_response(const char *str) {
    if (!tud_cdc_connected()) return;
    tud_cdc_write(str, strlen(str));
    tud_cdc_write_flush();
}

static void send_config_json() {
    char out[1536];
    const auto& cfg = config_get();

    int len = snprintf(out, sizeof(out),
        "CONFIG:{\"version\":%u,\"active_mode\":%u,\"invert_x\":%u,\"smoothing\":%u,"
        "\"kb_led_mode\":%u,\"gamepad_led_mode\":%u,"
        "\"calib_min\":%u,\"calib_max\":%u,\"calib_center\":%u,\"deadzone\":%u,",
        (unsigned)cfg.version,
        (unsigned)cfg.active_mode,
        (unsigned)cfg.gamepad_invert_x,
        (unsigned)cfg.gamepad_smoothing,
        (unsigned)cfg.kb_led_mode,
        (unsigned)cfg.gamepad_led_mode,
        (unsigned)cfg.calib_min,
        (unsigned)cfg.calib_max,
        (unsigned)cfg.calib_center,
        (unsigned)cfg.deadzone
    );

    // Keymap
    len += snprintf(out + len, sizeof(out) - len, "\"keymap\":[");
    for (int i = 0; i < NUM_BUTTONS; i++) {
        len += snprintf(out + len, sizeof(out) - len, "%u%s", cfg.keymap[i], (i == NUM_BUTTONS - 1) ? "" : ",");
    }
    len += snprintf(out + len, sizeof(out) - len, "],");

    // Button LED Colors
    len += snprintf(out + len, sizeof(out) - len, "\"led_colors\":[");
    for (int i = 0; i < NUM_BUTTONS; i++) {
        len += snprintf(out + len, sizeof(out) - len, "[%u,%u,%u]%s",
            cfg.led_colors[i][0], cfg.led_colors[i][1], cfg.led_colors[i][2],
            (i == NUM_BUTTONS - 1) ? "" : ",");
    }
    len += snprintf(out + len, sizeof(out) - len, "],");

    // Underglow Colors
    len += snprintf(out + len, sizeof(out) - len,
        "\"underglow_left\":[%u,%u,%u],\"underglow_right\":[%u,%u,%u]}\r\n",
        cfg.underglow_left[0], cfg.underglow_left[1], cfg.underglow_left[2],
        cfg.underglow_right[0], cfg.underglow_right[1], cfg.underglow_right[2]
    );

    send_response(out);
}

// Simple JSON field extractors
static bool parse_json_uint(const char *json, const char *key, unsigned *out_val) {
    char search[64];
    snprintf(search, sizeof(search), "\"%s\":", key);
    const char *p = strstr(json, search);
    if (!p) return false;
    p += strlen(search);
    while (*p == ' ' || *p == '\t') p++;
    *out_val = (unsigned)strtoul(p, NULL, 10);
    return true;
}

static bool parse_json_array(const char *json, const char *key, uint8_t *out_arr, size_t max_count) {
    char search[64];
    snprintf(search, sizeof(search), "\"%s\":", key);
    const char *p = strstr(json, search);
    if (!p) return false;
    p = strchr(p, '[');
    if (!p) return false;
    p++; // past '['

    for (size_t i = 0; i < max_count; i++) {
        while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
        if (*p == ']' || *p == '\0') break;
        char *next = NULL;
        out_arr[i] = (uint8_t)strtoul(p, &next, 10);
        if (!next || next == p) break;
        p = next;
        while (*p == ' ' || *p == '\t' || *p == ',') p++;
    }
    return true;
}

static bool parse_json_colors(const char *json, const char *key, uint8_t out_colors[][3], size_t count) {
    char search[64];
    snprintf(search, sizeof(search), "\"%s\":", key);
    const char *p = strstr(json, search);
    if (!p) return false;
    p = strchr(p, '[');
    if (!p) return false;
    p++; // past '['

    for (size_t i = 0; i < count; i++) {
        p = strchr(p, '[');
        if (!p) break;
        p++; // past '['
        for (int c = 0; c < 3; c++) {
            while (*p == ' ' || *p == '\t') p++;
            char *next = NULL;
            out_colors[i][c] = (uint8_t)strtoul(p, &next, 10);
            if (!next || next == p) break;
            p = next;
            while (*p == ' ' || *p == ',' || *p == ']') p++;
        }
    }
    return true;
}

static void handle_set_config(const char *json) {
    auto& cfg = config_get();
    unsigned val = 0;

    if (parse_json_uint(json, "active_mode", &val)) {
        if (val <= MODE_XINPUT) cfg.active_mode = (uint8_t)val;
    }
    if (parse_json_uint(json, "kb_led_mode", &val)) {
        if (val <= LED_MODE_OFF) cfg.kb_led_mode = (uint8_t)val;
    }
    if (parse_json_uint(json, "gamepad_led_mode", &val)) {
        if (val <= LED_MODE_OFF) cfg.gamepad_led_mode = (uint8_t)val;
    }
    if (parse_json_uint(json, "led_mode", &val)) {
        if (val <= LED_MODE_OFF) {
            cfg.kb_led_mode = (uint8_t)val;
            cfg.gamepad_led_mode = (uint8_t)val;
        }
    }
    if (parse_json_uint(json, "invert_x", &val)) {
        cfg.gamepad_invert_x = (uint8_t)(val ? 1 : 0);
    }
    if (parse_json_uint(json, "smoothing", &val)) {
        cfg.gamepad_smoothing = (uint8_t)(val > 100 ? 100 : val);
    }
    if (parse_json_uint(json, "calib_min", &val)) {
        cfg.calib_min = (uint16_t)val;
    }
    if (parse_json_uint(json, "calib_max", &val)) {
        cfg.calib_max = (uint16_t)val;
    }
    if (parse_json_uint(json, "calib_center", &val)) {
        cfg.calib_center = (uint16_t)val;
    }
    if (parse_json_uint(json, "deadzone", &val)) {
        cfg.deadzone = (uint16_t)val;
    }

    parse_json_array(json, "keymap", cfg.keymap, NUM_BUTTONS);
    parse_json_colors(json, "led_colors", cfg.led_colors, NUM_BUTTONS);

    uint8_t ug_left[3] = {0}, ug_right[3] = {0};
    if (parse_json_array(json, "underglow_left", ug_left, 3)) {
        memcpy(cfg.underglow_left, ug_left, 3);
    }
    if (parse_json_array(json, "underglow_right", ug_right, 3)) {
        memcpy(cfg.underglow_right, ug_right, 3);
    }

    config_save();
    send_response("OK:CONFIG_SAVED\r\n");
}

static void handle_line(char *line) {
    // Trim leading whitespace
    while (*line == ' ' || *line == '\t') line++;

    if (strncmp(line, "PING", 4) == 0) {
        send_response("PONG\r\n");
    } else if (strncmp(line, "GET_CONFIG", 10) == 0) {
        send_config_json();
    } else if (strncmp(line, "SET_CONFIG ", 11) == 0) {
        handle_set_config(line + 11);
    } else if (strncmp(line, "RESET_DEFAULTS", 14) == 0) {
        config_reset_defaults();
        config_save();
        send_response("OK:DEFAULTS_RESET\r\n");
    } else if (strncmp(line, "GET_INPUTS", 10) == 0) {
        uint32_t buttons = read_all_buttons();
        uint16_t lever_raw = analog_lever_get_raw();
        float lever_norm = analog_lever_get_normalized();
        char resp[128];
        snprintf(resp, sizeof(resp), "INPUTS:{\"buttons\":%lu,\"raw_adc\":%u,\"norm\":%.3f}\r\n",
            buttons, lever_raw, lever_norm);
        send_response(resp);
    } else if (strncmp(line, "PREVIEW_LEDS ", 13) == 0) {
        // e.g. PREVIEW_LEDS [[r,g,b],...]
        uint8_t preview_colors[NUM_BUTTONS][3];
        if (parse_json_colors(line + 13, "colors", preview_colors, NUM_BUTTONS)) {
            for (int i = 0; i < NUM_BUTTONS; i++) {
                led_set_pixel(i, RGBColor(preview_colors[i][0], preview_colors[i][1], preview_colors[i][2]));
            }
            led_show();
            send_response("OK:PREVIEW_SET\r\n");
        } else {
            send_response("ERROR:INVALID_COLORS\r\n");
        }
    } else if (strncmp(line, "REBOOT_BOOTSEL", 14) == 0) {
        send_response("OK:REBOOTING_BOOTSEL\r\n");
        sleep_ms(100);
        reset_usb_boot(0, 0);
    } else if (strncmp(line, "REBOOT", 6) == 0) {
        send_response("OK:REBOOTING\r\n");
        sleep_ms(100);
        watchdog_reboot(0, 0, 0);
    } else {
        send_response("ERROR:UNKNOWN_COMMAND\r\n");
    }
}

void serial_config_init() {
    g_rx_pos = 0;
    led_set_io4_active(false);
}

void serial_config_update_task() {
    if (!tud_cdc_available()) return;

    while (tud_cdc_available()) {
        char ch = (char)tud_cdc_read_char();
        if (ch == '\r' || ch == '\n') {
            if (g_rx_pos > 0) {
                g_rx_buf[g_rx_pos] = '\0';
                handle_line(g_rx_buf);
                g_rx_pos = 0;
            }
        } else if (g_rx_pos < sizeof(g_rx_buf) - 1) {
            g_rx_buf[g_rx_pos++] = ch;
        }
    }
}
