# PICO_DEBUG_STACK Setup & Verification Guide

This guide details how to build and verify the Logic Analyzer firmware for the PICO_DEBUG_STACK board (RP2354B A4).

## 1. Environment Setup

Ensure you have the Pico SDK v2.0+ installed and configured.

## 2. Compilation

To build the firmware for PICO_DEBUG_STACK, use the following CMake command:

```bash
mkdir -p build
cd build
cmake -DBOARD_TYPE=BOARD_PICO_DEBUG_STACK ..
make -j4
```

This will produce `LogicAnalyzer.uf2` in the build directory.

**Note:** Double check that `LogicAnalyzer_Board_Settings.h` contains the `BUILD_PICO_DEBUG_STACK` definitions.

## 3. Hardware Verification

Once you have the PICO_DEBUG_STACK board:

1.  **Flash Firmware:** Hold BOOTSEL and connect USB. Copy `LogicAnalyzer.uf2` to the drive.
2.  **LED Check:** The LED on **GPIO26** should behave as the status indicator (blinking/solid depending on state).
3.  **Connection:** Open the Logic Analyzer software. It should detect a device named `LogicAnalyzer`.
4.  **Pin Mapping Check:**
    - Connect a known signal (e.g., 1kHz square wave) to **GPIO0**. Verify Channel 0 sees it.
    - Connect to **GPIO29** (Complex Trigger OUT) or **GPIO30** (Complex Trigger IN).
    - **Note:** The board maps analyzer channels 0-22 directly to GPIO 0-22. Analyzer channels 23-25 are mapped to board GPIOs 26-28.

## 4. Performance Check

- **Sample Rate:** Attempt to set the sample rate to `400MHz` (Turbo Mode) in the software.
- **Buffer Size:** Verify the capture depth allows for ~512KB of data (approx 2x standard Pico).

## 5. Troubleshooting

- If compilation fails on `pico2_b.h` missing, ensure the `custom_boards` directory is correctly referenced in `CMakeLists.txt`.
- If the LED doesn't light up, verify GPIO26 usage.
