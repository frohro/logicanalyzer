cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

include(pico_sdk_import.cmake)

project(pico2pico_clock C CXX ASM)

pico_sdk_init()

add_executable(pico2pico_clock
    main.c
)

# Link libraries
target_link_libraries(pico2pico_clock
    pico_stdlib
    hardware_clocks
)

pico_enable_stdio_uart(pico2pico_clock 0)
pico_enable_stdio_usb(pico2pico_clock 1)

pico_add_extra_outputs(pico2pico_clock)

# Optional compile time message
message(STATUS "Configuring pico2pico_clock for Pico boards")
