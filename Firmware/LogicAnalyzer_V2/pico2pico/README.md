# pico2pico clock synchronization demo

This demo shows how to synchronize two Raspberry Pi Picos using a shared clock:

- **Master Pico** (`main.c`): Outputs 120MHz clock on GPIO21 via hardware GPOUT
- **Slave Pico** (`main_slave.c`): Uses external clock from GPIO20 (GPIN0) as system clock

## Hardware Setup

```
Master Pico          Slave Pico
┌──────────┐        ┌──────────┐
│ GPIO21 ──┼────────┤ GPIO20   │  (120MHz clock)
│  (out)   │        │  (GPIN0) │
│          │        │          │
│ GND ─────┼────────┤ GND      │  (common ground)
│          │        │          │
│          │        │ GPIO22   │  (test output - optional)
└──────────┘        └──────────┘
```

**Important**: Connect a common ground between the two Picos!

## Build Master (Clock Output)

```bash
# Set SDK path
export PICO_SDK_PATH="$HOME/Projects/pico-sdk"

# Build master from repository root
cd /path/to/logicanalyzer
cmake -S Firmware/LogicAnalyzer_V2/pico2pico -B build/pico2pico_master -D PICO_BOARD=pico
cmake --build build/pico2pico_master
```

Flash: `cp build/pico2pico_master/pico2pico_clock.uf2 /media/$USER/RPI-RP2/`

## Build Slave (Clock Input)

```bash
# Build slave using alternate CMakeLists
cmake -S Firmware/LogicAnalyzer_V2/pico2pico -B build/pico2pico_slave \
      -D PICO_BOARD=pico \
      -D CMAKE_PROJECT_NAME=pico2pico_slave \
      -C Firmware/LogicAnalyzer_V2/pico2pico/CMakeLists_slave.txt
cmake --build build/pico2pico_slave
```

Or manually:
```bash
cd Firmware/LogicAnalyzer_V2/pico2pico
cp CMakeLists_slave.txt CMakeLists.txt
mkdir -p build_slave && cd build_slave
cmake .. -D PICO_BOARD=pico
cmake --build .
```

Flash: `cp build_slave/pico2pico_slave.uf2 /media/$USER/RPI-RP2/`

## Testing Synchronization

1. **Flash master Pico** with `pico2pico_clock.uf2`
2. **Flash slave Pico** with `pico2pico_slave.uf2`
3. **Connect GPIO21 (master) → GPIO20 (slave)**
4. **Connect GND between both Picos**
5. Power both Picos

### Verification

- Slave's LED should blink (shows it's running on external clock)
- Connect logic analyzer to:
  - Master GPIO21 (clock source)
  - Slave GPIO22 (synchronized output)
- Both signals should be perfectly in phase (no drift over time)

### Via Serial Monitor

Connect to slave Pico's USB serial:
```bash
screen /dev/ttyACM0 115200
# or
minicom -D /dev/ttyACM0 -b 115200
```

You should see:
```
=== Slave Pico - External Clock Input Demo ===
System clock source: External via GPIN0 (GPIO20)
System clock frequency: 120000000 Hz (120 MHz)
SUCCESS: External clock is active!
```

## Notes

1. **120MHz reduces jitter**: Uses integer PLL dividers (12MHz × 10) instead of fractional dividers

2. **Direct clock routing**: Slave uses GPIN0 directly as `clk_sys` source (no PLL), ensuring perfect synchronization

3. **GPIO GPOUT support**: GPIO21 may not support hardware clock output on all boards. Try GPIO 22-25 if needed.

4. **USB timing**: Slave's USB may be unstable since it's clocked from external source. Serial output works but enumeration might be flaky.

5. **Phase alignment**: Both Picos run from the same clock edges, so PIO programs execute in lockstep (no phase uncertainty).

## Use Cases

- **Logic analyzer + DUT**: Master runs logic analyzer, slave runs device under test - both perfectly synchronized
- **Multi-board projects**: Synchronize multiple Picos for distributed processing
- **Eliminating Nyquist oversampling**: Sample at exactly the DUT's output rate
