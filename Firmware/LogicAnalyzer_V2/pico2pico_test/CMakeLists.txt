cmake_minimum_required(VERSION 3.13)

# Set the Pico SDK path
set(PICO_SDK_PATH "$ENV{HOME}/Projects/pico-sdk")

# Include pico_sdk_import.cmake from the SDK
include(${PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(pico2pico_test C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Initialize the Pico SDK
pico_sdk_init()

# Master program
add_executable(master
    master.c
)

pico_generate_pio_header(master ${CMAKE_CURRENT_LIST_DIR}/test_output.pio)

target_link_libraries(master
    pico_stdlib
    hardware_clocks
    hardware_gpio
    hardware_pio
    hardware_dma
    pico_stdio_usb
)

# Enable USB output, disable UART output
pico_enable_stdio_usb(master 1)
pico_enable_stdio_uart(master 0)

pico_add_extra_outputs(master)

# Slave program
add_executable(slave
    slave.c
)

pico_generate_pio_header(slave ${CMAKE_CURRENT_LIST_DIR}/test_capture.pio)

target_link_libraries(slave
    pico_stdlib
    hardware_clocks
    hardware_gpio
    hardware_pio
    hardware_dma
)

# Enable USB output, disable UART output
pico_enable_stdio_usb(slave 1)
pico_enable_stdio_uart(slave 0)

pico_add_extra_outputs(slave)

# Custom targets for flashing
add_custom_target(flash-master
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/flash.sh ${CMAKE_CURRENT_BINARY_DIR}/master.uf2
    DEPENDS master
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Flashing master.uf2 to Pico"
)

add_custom_target(flash-slave
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/flash.sh ${CMAKE_CURRENT_BINARY_DIR}/slave.uf2
    DEPENDS slave
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Flashing slave.uf2 to Pico"
)
