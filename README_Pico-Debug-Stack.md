# LogicAnalyzer — Pico-Debug-Stack Edition

This document describes how to build, flash, and operate the LogicAnalyzer firmware for the **Pico-Debug-Stack** board.  The Pico-Debug-Stack is a "man-in-the-middle" hardware shim designed by Rob Frohne at Walla Walla University that gives you a full suite of debugging and signal-analysis tools with a single USB-C cable.

---

## Hardware Overview

The Pico-Debug-Stack has two onboard **RP2354B** chips (RP2350 architecture with 2 MB internal flash):

| Chip | Role | Firmware |
|------|------|----------|
| **U1** | Logic Analyzer | This firmware (`LogicAnalyzer`) |
| **U2** | Debug Probe / Stimulus Generator | `yapicoprobe` or `debugprobe` |

A **GL850G** USB hub (U3) exposes all three USB devices over a single USB-C connector.

### Physical Stack
```
┌─────────────────────────────┐
│  DUT (Target Pico H)        │  ← plug your project Pico here
├─────────────────────────────┤
│  Pico-Debug-Stack PCB       │  ← Analyzer + Probe + Hub
├─────────────────────────────┤
│  Male headers               │  ← plugs into your project board
└─────────────────────────────┘
```

### Logic Analyzer Pin Map (U1 ↔ DUT)

| DUT GPIO | Analyzer GPIO (U1) | Notes |
|----------|--------------------|-------|
| GPIO 0–22 | GPIO 0–22 | Direct, 1-to-1 |
| GPIO 26 | GPIO 23 | |
| GPIO 27 | GPIO 24 | |
| GPIO 28 | GPIO 25 | |
| — | GPIO 26 | Yellow status LED on the PCB |
| — | GPIO 29 & 30 | **Shorted together** — complex/fast trigger I/O |

> **Note on DUT USB:** The target Pico's USB data lines are **not** routed through the stack headers.  If your target code uses USB (e.g. TinyUSB), connect its USB port with a short cable to one of the two auxiliary USB-A ports on the stack.

---

## Building the Firmware

### Prerequisites

You need these tools installed **before** building.  Your instructor should have walked you through this setup; if not, follow the platform steps below.

#### Linux (Ubuntu/Debian)
```bash
sudo apt-get update
sudo apt-get install -y git cmake gcc-arm-none-eabi \
    libnewlib-arm-none-eabi libstdc++-arm-none-eabi-newlib make
```

#### macOS
Install [Homebrew](https://brew.sh), then:
```bash
brew install cmake git
brew install --cask gcc-arm-embedded
```

#### Windows
1. Install [Git for Windows](https://git-scm.com/download/win)
2. Install [CMake](https://cmake.org/download/) (add to PATH during install)
3. Install [Arm GNU Toolchain](https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads) — pick the `arm-none-eabi` Windows installer, add to PATH
4. Install [Make for Windows](https://gnuwin32.sourceforge.net/packages/make.htm) or use `ninja` (included with CMake)

#### Pico SDK (all platforms)
The easiest method is to install the [Raspberry Pi Pico VS Code Extension](https://marketplace.visualstudio.com/items?itemName=raspberry-pi.raspberry-pi-pico), which installs the SDK automatically into `~/.pico-sdk`.  Alternatively, clone it manually:
```bash
git clone https://github.com/raspberrypi/pico-sdk.git ~/pico-sdk
cd ~/pico-sdk && git submodule update --init
export PICO_SDK_PATH=~/pico-sdk   # add this to your shell profile
```

---

### Build Steps

#### 1. Set the board target

Open `Firmware/LogicAnalyzer_V2/LogicAnalyzer_Build_Settings.cmake` and set:
```cmake
set(BOARD_TYPE "BOARD_PICO_DEBUG_STACK")
```

#### 2. Configure and build

**Linux / macOS:**
```bash
cd Firmware/LogicAnalyzer_V2
mkdir -p build_pico_debug_stack
cd build_pico_debug_stack
PICO_SDK_PATH=~/.pico-sdk cmake ..   # adjust path if you cloned SDK elsewhere
PICO_SDK_PATH=~/.pico-sdk make -j$(nproc)
```

**Windows (PowerShell):**
```powershell
cd Firmware\LogicAnalyzer_V2
mkdir build_pico_debug_stack
cd build_pico_debug_stack
$env:PICO_SDK_PATH = "$env:USERPROFILE\.pico-sdk"  # adjust if needed
cmake .. -G "MinGW Makefiles"
mingw32-make -j4
```

The build produces `LogicAnalyzer.uf2` in the build directory.

---

### Flashing the Firmware

The Logic Analyzer chip is **U1** on the Pico-Debug-Stack.

1. **Enter bootloader mode:** Hold the **BOOTSEL** button on U1 while connecting the stack's USB-C cable, **or** hold BOOTSEL and press the RESET button if present.
2. A drive named `RPI-RP2` (or `RP2350`) appears on your computer.
3. Copy `LogicAnalyzer.uf2` to that drive:
   - **Linux:** `cp build_pico_debug_stack/LogicAnalyzer.uf2 /media/$USER/RPI-RP2/`
   - **macOS:** `cp build_pico_debug_stack/LogicAnalyzer.uf2 /Volumes/RPI-RP2/`
   - **Windows:** Drag and drop in File Explorer.
4. The drive disappears and U1 reboots automatically — flashing is complete.

> **Which USB port?** Each RP2354B chip has its own USB connection through the onboard hub.  Make sure you are putting the correct chip (U1, the Logic Analyzer) into BOOTSEL mode, not the Debug Probe (U2).

A pre-built `LogicAnalyzer.uf2` is available in `Firmware/LogicAnalyzer_V2/build_pico_debug_stack/` if you just want to flash without building.

---

## Operating the Logic Analyzer

### 1. Install and Launch the PC Software

The Logic Analyzer is controlled from **Dr. Gusman's LogicAnalyzer** desktop application.

**Prerequisites:** [.NET 8 SDK](https://dotnet.microsoft.com/download)

```bash
cd Software/LogicAnalyzer
dotnet run --project LogicAnalyzer/LogicAnalyzer.csproj -c Debug
```

> You must run this command each time you launch — the app is not installed globally.

### 2. Connect to the Device

1. Plug the Pico-Debug-Stack into your PC via USB-C.
2. Launch the PC software using the command above.
3. In the software, click **Connect** and select the device identified as **`PICO_DEBUG_STACK`**.
4. The status indicator should turn green once connected.

### 3. Connecting Your Signals

The DUT's GPIOs are wired directly to the Logic Analyzer through 330 Ω series protection resistors.  Simply plug your target Pico H into the socket on top of the stack — no jumper wires needed.

| Logic Analyzer Channel | DUT GPIO |
|------------------------|----------|
| Channel 0 | GPIO 0 |
| Channel 1 | GPIO 1 |
| … | … |
| Channel 22 | GPIO 22 |
| Channel 23 | GPIO 26 |

### 4. Capturing Signals

1. In the LogicAnalyzer software, select the **channels** you want to capture.
2. Set the **sample rate** (up to 100 MHz in normal mode, 200 MHz in blast mode).
3. Configure a **trigger** if needed:
   - **Simple trigger:** rising or falling edge on any captured channel.
   - **Complex / Fast trigger:** uses the GPIO 29/30 loopback on the PCB.
4. Click **Start** to arm the capture.
5. When the trigger fires (or immediately in free-run mode), the waveform appears in the viewer.

### 5. Protocol Decoding

The software includes built-in decoders for common protocols.  After capturing:

1. Right-click a channel group and select **Add Decoder**.
2. Choose the protocol (I2C, SPI, UART, I2S, …).
3. Assign the appropriate channels (clock, data, chip-select, etc.).
4. Decoded packets appear as annotations on the waveform.

Alternatively, export captures to **Sigrok PulseView** format and use the decoders in `Software/decoders/`.

### 6. Status LED

The yellow LED on GPIO 26 of U1 indicates Analyzer status:

| LED Behaviour | Meaning |
|---------------|---------|
| Slow blink | Idle / waiting for connection |
| Solid on | Capture in progress |
| Off | USB not connected or error |

---

## Triggering

GPIO 29 and GPIO 30 are **shorted together** on the PCB, so `COMPLEX_TRIGGER_OUT_PIN` (30) drives `COMPLEX_TRIGGER_IN_PIN` (29) directly.  This enables complex and fast triggering without any external wiring.

---

## Troubleshooting

### Device not detected by the software
- Verify U1 is running the LogicAnalyzer firmware (not in BOOTSEL mode).
- Try a different USB-C cable — some cables are power-only.
- On Linux, add your user to the `dialout` group: `sudo usermod -aG dialout $USER` then log out and back in.
- On Windows, the device should enumerate as a CDC/ACM serial port.  If not, install the [Zadig](https://zadig.akeo.ie/) driver.

### Build fails: `Compiler 'arm-none-eabi-gcc' not found`
- Follow the toolchain installation steps for your platform above.
- Set `PICO_TOOLCHAIN_PATH` to the `bin/` folder of your toolchain if it is in a non-standard location.

### Build fails: `SDK location was not specified`
- Make sure `PICO_SDK_PATH` is set as an environment variable **both** for the `cmake` step and the `make` / `make` step.

### Low sample rate / missed edges
- Use a short, direct USB connection to reduce latency.
- For high-speed signals (> 50 MHz), keep probe wires under 5 cm.
- Enable **TURBO_MODE** in `LogicAnalyzer_Build_Settings.cmake` for higher clock rates (experimental).

---

## Technical Specifications

| Parameter | Value |
|-----------|-------|
| Microcontroller | RP2354B (RP2350 architecture, dual Cortex-M33) |
| Flash | 2 MB internal |
| Max sample rate (normal) | 100 MHz |
| Max sample rate (blast mode) | 200 MHz |
| Capture buffer | 384 KB |
| Channels | 24 (DUT GPIO 0–22, 26–28) |
| Trigger types | Simple, complex, fast (via GPIO 29/30 loopback) |
| USB interface | USB CDC (via GL850G hub) |
| Reported device name | `PICO_DEBUG_STACK` |

---

## Support and Documentation

- **Project repository:** [github.com/gusmanb/logicanalyzer](https://github.com/gusmanb/logicanalyzer)
- **RP2350 datasheet:** [Raspberry Pi Foundation](https://www.raspberrypi.org/documentation/)
- **Pico-Debug-Stack hardware:** Designed by Rob Frohne, Walla Walla University

---

*This firmware is specifically built for the Pico-Debug-Stack (U1, Logic Analyzer chip).  For other boards, change `BOARD_TYPE` in `LogicAnalyzer_Build_Settings.cmake` and rebuild.*
