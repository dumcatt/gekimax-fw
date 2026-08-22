cmake_minimum_required(VERSION 3.13)

# Pull in SDK (must be before project)
include(pico_sdk_import.cmake)

project(gekimax-io4 C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Initialize the Raspberry Pi Pico SDK
pico_sdk_init()

add_executable(gekimax
    src/main.cpp
    src/config.cpp
    src/analog_lever.cpp
    src/led_driver.cpp
    src/usb/usb_descriptors.c
    src/usb/io4_mode.cpp
    src/usb/hid_mode.cpp
    src/usb/xinput_mode.cpp
    src/usb/serial_config.cpp
)

# Generate header from PIO file
pico_generate_pio_header(gekimax ${CMAKE_CURRENT_LIST_DIR}/src/ws2812.pio)

target_include_directories(gekimax PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/src
    ${CMAKE_CURRENT_LIST_DIR}/src/usb
)

target_link_libraries(gekimax PRIVATE
    pico_stdlib
    pico_multicore
    pico_bootrom
    hardware_adc
    hardware_flash
    hardware_sync
    hardware_watchdog
    hardware_pio
    hardware_clocks
    tinyusb_device
    tinyusb_board
)

# Enable USB output, disable UART stdio
pico_enable_stdio_usb(gekimax 0)
pico_enable_stdio_uart(gekimax 0)

# Create map/bin/hex/uf2 files
pico_add_extra_outputs(gekimax)
