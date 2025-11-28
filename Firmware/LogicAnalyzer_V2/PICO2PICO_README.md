# PICO2PICO Logic Analyzer Board

## Overview

The PICO2PICO is a "state mode" logic analyzer configuration where two Raspberry Pi Picos are stacked together:

- **Master (DUT)**: The device under test - your application code runs here
- **Slave (LogicAnalyzer)**: Captures data using the master's clock

This eliminates Nyquist sampling requirements because the logic analyzer samples at the exact same clock as the DUT, meaning no aliasing regardless of signal frequency.

## Hardware Setup

Stack two Picos with the **slave on top** (so you can access the slave's USB for LogicAnalyzer communication). Add one jumper wire:

```
Master GPIO21 (GPOUT0) ──────> Slave GPIO20 (GPIN0)
                         ^
                    Clock signal
```

All other GPIOs are directly connected when stacked (GPIO0-19, 22-28 match between boards).

## Pin to Channel Mapping

**LogicAnalyzer channels start at 1, not 0!**

The PIN_MAP in LogicAnalyzer_Board_Settings.h is:
```c
{2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,22,23,24,25,26,27,28,COMPLEX_TRIGGER_IN_PIN}
```

| GPIO | Channel | Notes |
|------|---------|-------|
| GPIO2 | Channel 1 | |
| GPIO3 | Channel 2 | |
| GPIO4 | Channel 3 | |
| GPIO5 | Channel 4 | |
| GPIO6 | Channel 5 | |
| GPIO7 | Channel 6 | |
| GPIO8 | Channel 7 | |
| GPIO9 | Channel 8 | |
| **GPIO10** | **Channel 9** | **Test pattern output** |
| GPIO11 | Channel 10 | |
| GPIO12 | Channel 11 | |
| GPIO13 | Channel 12 | |
| GPIO14 | Channel 13 | |
| GPIO15 | Channel 14 | |
| GPIO16 | Channel 15 | |
| GPIO17 | Channel 16 | |
| GPIO18 | Channel 17 | |
| GPIO19 | Channel 18 | |
| GPIO20 | -- | **Clock input from master (not capturable)** |
| GPIO21 | -- | **Jumper to master GPIO21 (not capturable)** |
| GPIO22 | Channel 19 | |
| GPIO23 | Channel 20 | |
| GPIO24 | Channel 21 | |
| GPIO25 | Channel 22 | Slave's LED (will show LED activity) |
| GPIO26 | Channel 23 | |
| GPIO27 | Channel 24 | |
| GPIO28 | Channel 25 | |

## Clock Configuration

- **Master outputs clock on GPIO21** using `clock_gpio_init()` with `CLKSRC_PLL_SYS`
- **Slave receives clock on GPIO20** (GPIN0) and switches clk_sys to it during capture
- After capture completes, slave switches back to internal PLL for USB communication

### Important: USB Limitation

USB **does not work** while the slave is running on the external clock. The slave firmware:
1. Runs normally on internal 125 MHz PLL
2. Switches to external clock ONLY during capture
3. Switches back to internal clock after capture completes
4. Then USB communication resumes

## Testing with pico2pico_master.c

The `pico2pico_master.c` test program:
- Runs at 125 MHz system clock
- Outputs clock on GPIO21
- Outputs a ~10 MHz square wave test pattern on GPIO10 (Channel 9)
- LED blinks to show it's running

**Expected capture result on Channel 9:** `0x55555555` or `0xAAAAAAAA` (depending on phase alignment when capture starts)

## Adding Clock Output to Your DUT Code

To use the PICO2PICO with your own application, add ONE line to your DUT code:

```c
// Add this near the start of main(), after setting system clock
clock_gpio_init(21, CLOCKS_CLK_GPOUT0_CTRL_AUXSRC_VALUE_CLKSRC_PLL_SYS, 1);
```

That's it! Your DUT now outputs its system clock for the logic analyzer to use.

## Building

Set the board type in `LogicAnalyzer_Build_Settings.cmake`:
```cmake
set(BOARD_TYPE "BOARD_PICO2PICO")
```

Then build:
```bash
cd Firmware/LogicAnalyzer_V2
mkdir build && cd build
cmake ..
make -j4
```

This produces:
- `LogicAnalyzer.uf2` - Flash to the **slave** Pico
- `pico2pico_master.uf2` - Flash to the **master** Pico (for testing)

## Flashing

```bash
# From the build directory:
make flash-slave   # Flash LogicAnalyzer to slave on /dev/ttyACM1
make flash-master  # Flash test program to master on /dev/ttyACM0
```

Or manually: hold BOOTSEL, plug in USB, copy the .uf2 file to the RPI-RP2 drive.

## Troubleshooting

### No serial output from master
- Wait 3 seconds after boot (LED blinks while USB enumerates)
- Check you're connected to the master's USB, not the slave's

### Capture shows all zeros or all ones
- The slave may be sampling at the wrong clock edge
- Try triggering on a different edge
- Verify the GPIO21→GPIO20 jumper wire is connected

### Capture shows random data
- External clock may not be detected - check jumper wire
- Master may not be running - check master's LED is blinking

### LogicAnalyzer won't connect
- The slave must be on internal clock for USB to work
- If capture is stuck, power cycle the slave Pico
