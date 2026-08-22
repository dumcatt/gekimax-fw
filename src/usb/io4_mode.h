#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "tusb.h"

namespace io4 {
    enum class CoinCondition : uint8_t {
        Normal = 0x0,
        Jam = 0x1,
        Disconnect = 0x2,
        Busy = 0x3
    };

    struct CoinData {
        CoinCondition condition;
        uint8_t count;
    } __attribute__((packed));

    struct OutputReport {
        int16_t analog[8];
        int16_t rotary[4];
        CoinData coin[2];
        uint16_t switches[2];
        uint8_t system_status;
        uint8_t usb_status;
        uint8_t _unused[29];
    } __attribute__((packed));

    enum Cmd : uint8_t {
        SET_COMM_TIMEOUT   = 0x01,
        SET_SAMPLING_COUNT = 0x02,
        CLEAR_BOARD_STATUS = 0x03,
        SET_GENERAL_OUTPUT = 0x04,
        SET_PWM_OUTPUT     = 0x05,
        UPDATE_FIRMWARE    = 0x85
    };

    struct InputReport {
        uint8_t report_id;
        Cmd cmd;
        uint8_t payload[62];
    } __attribute__((packed));

    void init();
    void update_task();
    void handle_out_report(const uint8_t *buffer, uint16_t buf_size);
}
