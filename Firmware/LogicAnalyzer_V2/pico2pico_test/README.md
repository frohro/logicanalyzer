# Pico-to-Pico Clock Synchronization Test

## Project Goal

Synchronize two Raspberry Pi Picos so they sample data on the **exact same clock**, achieving both **frequency and phase synchronization**. This eliminates Nyquist sampling concerns when the slave (logic analyzer) captures data from the master (device under test).

## Why This Matters

When two systems run on independent clocks, even at the same nominal frequency, there's always some drift and phase difference. By having the slave Pico use the master's clock directly, every sample is taken at a precise, predictable time relative to the master's operations.

## Tested Clock Speeds

| Frequency | Status | Notes |
|-----------|--------|-------|
| 144 MHz | ✅ Works | Conservative, well within RP2040 specs |
| 200 MHz | ✅ Works | Overclocked, tested successfully |

Higher speeds may work but require:
- Short wires (few mm)
- Good ground connection
- 12mA drive strength + fast slew rate on clock output

## Hardware Setup

### Pin Connections (Test Setup)

| Master GPIO | Slave GPIO | Function | Notes |
|-------------|------------|----------|-------|
| GPIO21 | GPIO20 | Clock (144 MHz) | Master outputs PLL_SYS via GPOUT0 → Slave GPIN0 |
| GPIO22 | GPIO22 | Sync/Trigger | Rising edge starts capture (test only) |
| GPIO10 | GPIO10 | Test Data | For verification (alternating pattern) |
| GND | GND | Ground | Required for signal reference |

### Pin Connections (LogicAnalyzer Use)

| DUT (Master) GPIO | LogicAnalyzer (Slave) GPIO | Function |
|-------------------|----------------------------|----------|
| GPIO21 | GPIO20 | Clock - **only required DUT modification** |
| GPIO0-15 | GPIO0-15 | Data channels (directly connected) |
| Any trigger pin | Same pin on slave | Trigger (normal LogicAnalyzer trigger) |
| GND | GND | Ground |

**Key insight:** The DUT only needs to output its clock. No sync signal, no handshake, no protocol changes. The LogicAnalyzer uses its normal trigger mechanism on the data pins.

### Physical Setup
- Two Pico boards stacked close together (few mm apart for short wires)
- Short wires minimize signal degradation at 144 MHz
- Both connected via USB for programming and debug output

## Technical Details

### Clock Configuration

**Master (144 MHz):**
```c
// Set system clock to 144 MHz (12 MHz × 12)
set_sys_clock_khz(144000, true);

// Output PLL_SYS directly on GPIO21 with maximum drive
gpio_set_drive_strength(CLOCK_OUT_PIN, GPIO_DRIVE_STRENGTH_12MA);
gpio_set_slew_rate(CLOCK_OUT_PIN, GPIO_SLEW_RATE_FAST);
clock_gpio_init(CLOCK_OUT_PIN, CLOCKS_CLK_GPOUT0_CTRL_AUXSRC_VALUE_CLKSRC_PLL_SYS, 1);
```

**Slave (receives 144 MHz on GPIO20):**
```c
// Configure GPIO20 as clock input (GPIN0)
hw_write_masked(&pads_bank0_hw->io[CLOCK_IN_PIN],
                PADS_BANK0_GPIO0_IE_BITS,
                PADS_BANK0_GPIO0_IE_BITS | PADS_BANK0_GPIO0_OD_BITS);
iobank0_hw->io[CLOCK_IN_PIN].ctrl = GPIO_FUNC_GPCK << IO_BANK0_GPIO0_CTRL_FUNCSEL_LSB;

// Switch clk_sys to use GPIN0 (external clock)
// 1. Switch to clk_ref first (safe intermediate)
hw_clear_bits(&clocks_hw->clk[clk_sys].ctrl, CLOCKS_CLK_SYS_CTRL_SRC_BITS);
while (clocks_hw->clk[clk_sys].selected != 1) tight_loop_contents();

// 2. Configure GPIN0 as auxiliary source
hw_write_masked(&clocks_hw->clk[clk_sys].ctrl,
                CLOCKS_CLK_SYS_CTRL_AUXSRC_VALUE_CLKSRC_GPIN0 << CLOCKS_CLK_SYS_CTRL_AUXSRC_LSB,
                CLOCKS_CLK_SYS_CTRL_AUXSRC_BITS);

// 3. Set divider to 1 (no division)
clocks_hw->clk[clk_sys].div = 1 << CLOCKS_CLK_SYS_DIV_INT_LSB;

// 4. Switch to auxiliary source
hw_set_bits(&clocks_hw->clk[clk_sys].ctrl, 
            CLOCKS_CLK_SYS_CTRL_SRC_VALUE_CLKSRC_CLK_SYS_AUX << CLOCKS_CLK_SYS_CTRL_SRC_LSB);
while (clocks_hw->clk[clk_sys].selected != 2) tight_loop_contents();

clock_set_reported_hz(clk_sys, 144 * MHZ);
```

**Switching back to internal clock (after capture):**
```c
// Switch to clk_ref first
hw_clear_bits(&clocks_hw->clk[clk_sys].ctrl, CLOCKS_CLK_SYS_CTRL_SRC_BITS);
while (clocks_hw->clk[clk_sys].selected != 1) tight_loop_contents();

// Configure PLL_SYS as auxiliary source
hw_write_masked(&clocks_hw->clk[clk_sys].ctrl,
                CLOCKS_CLK_SYS_CTRL_AUXSRC_VALUE_CLKSRC_PLL_SYS << CLOCKS_CLK_SYS_CTRL_AUXSRC_LSB,
                CLOCKS_CLK_SYS_CTRL_AUXSRC_BITS);

// Switch to PLL_SYS
hw_set_bits(&clocks_hw->clk[clk_sys].ctrl, 
            CLOCKS_CLK_SYS_CTRL_SRC_VALUE_CLKSRC_CLK_SYS_AUX << CLOCKS_CLK_SYS_CTRL_SRC_LSB);
while (clocks_hw->clk[clk_sys].selected != 2) tight_loop_contents();

clock_set_reported_hz(clk_sys, 125 * MHZ);
```

### USB and Clock Switching - CRITICAL

**Key Insight:** USB requires a stable 48 MHz clock. When clk_sys switches to an external source, USB stops working reliably.

**Solution:** Switch clocks only during capture:
1. Boot on internal PLL → USB works
2. Print status, wait for user input → USB works
3. Switch to external clock → USB stops
4. Perform synchronized capture (PIO + DMA) → no USB needed
5. Switch back to internal PLL → USB works again
6. Print results → USB works

```c
// Before capture: switch to external
switch_to_external_clock(GPIO20);

// Capture happens here - synchronized to master
wait_for_sync();
start_pio_dma_capture();
wait_for_dma_complete();

// After capture: switch back to internal
switch_to_internal_clock();

// Now safe to print/send results over USB
printf("Results: ...");
```

### Verifying External Clock Before Switching

```c
// Measure external clock frequency before committing to switch
uint32_t ext_freq = frequency_count_khz(CLOCKS_FC0_SRC_VALUE_CLKSRC_GPIN0);
printf("External clock: %lu kHz\n", ext_freq);

if (ext_freq < 100000) {  // Less than 100 MHz
    printf("ERROR: External clock missing or too slow!\n");
    // Don't switch - stay on internal clock
}
```

### Synchronizer Delay

The RP2040 has a 2-cycle synchronizer on GPIO inputs. This means:
- Input signals are delayed by 2 clock cycles
- This delay is **deterministic** and can be compensated
- Dr. Gusman's LogicAnalyzer already accounts for this

### PIO Programs

**test_output.pio (Master):**
```
.program test_output
.wrap_target
    out pins, 1    ; Output 1 bit per clock cycle
.wrap
```

**test_capture.pio (Slave):**
```
.program test_capture
.wrap_target
    in pins, 1     ; Capture 1 bit per clock cycle
.wrap
```

Both run at clkdiv=1.0, so they execute one instruction per system clock cycle.

### Test Pattern

Master outputs 0x55555555 (alternating 0,1,0,1...) pattern.
Slave should capture either:
- 0x55555555 (phase offset even - started on 0)
- 0xAAAAAAAA (phase offset odd - started on 1)

Either result indicates successful synchronization. The phase depends on sync timing.

## Building and Running

```bash
cd ~/Classes/Digital_Design/pico2pico_test
mkdir -p build
cd build
cmake ..
make master slave
```

This creates:
- `build/master.uf2`
- `build/slave.uf2`

### Flashing

1. Hold BOOTSEL on Pico, plug in USB, release BOOTSEL
2. Drag .uf2 file to the RPI-RP2 drive that appears
3. Pico auto-reboots and runs

### Running the Test

```bash
# Use minicom (NOT CuteCom - it crashes on clock switch)
# Terminal 1 - Slave:
minicom -D /dev/ttyACM0 -b 115200

# Terminal 2 - Master:
minicom -D /dev/ttyACM1 -b 115200

# To exit minicom: Ctrl+A then X
```

**Test sequence:**
1. Press Enter on slave to arm capture
2. Press Enter on master to send pattern
3. View results on slave terminal

## Test Results

**Success indicators:**
- Slave reports "SUCCESS"
- Captured pattern is 0x55555555 or 0xAAAAAAAA
- All captured words match (no bit errors)
- Slave LED blinks fast (triple blink pattern)

**Failure indicators:**
- Captured pattern is 0xDEDEDEDE (no data captured)
- Random bit patterns (clock not synchronized)
- Slave LED blinks slow (1 Hz)

## Applying to LogicAnalyzer

### Minimal DUT (Device Under Test) Changes

**The DUT only needs ONE line of code added:**

```c
// Add this to DUT's main() initialization:
clock_gpio_init(21, CLOCKS_CLK_GPOUT0_CTRL_AUXSRC_VALUE_CLKSRC_PLL_SYS, 1);
```

And optionally for better signal integrity at high frequencies:
```c
gpio_set_drive_strength(21, GPIO_DRIVE_STRENGTH_12MA);
gpio_set_slew_rate(21, GPIO_SLEW_RATE_FAST);
```

**That's it!** No sync signals, no handshake protocol, no ARM/READY pins. The DUT runs completely normally - it doesn't even know it's being captured.

### How It Works with Normal Triggers

The LogicAnalyzer's existing trigger system works perfectly:

1. **GUI** → Slave: "Capture 10K samples, trigger on CH5 rising edge"
2. **Slave**: Switches to external clock (from DUT's GPIO21)
3. **Slave**: Arms PIO with trigger settings, starts DMA
4. **DUT**: Runs normally, completely unaware of capture
5. **DUT**: Something causes CH5 to go high (the event you're debugging)
6. **Slave**: Trigger fires, captures N samples synchronized to DUT clock
7. **Slave**: Switches back to internal clock
8. **Slave** → GUI: "Here's your synchronized data"

The existing trigger modes all work:
- **Edge trigger** (`jmp pin`): Waits for pin HIGH or LOW
- **Pattern trigger** (`COMPLEX_TRIGGER`): Waits for 4-bit pattern match
- **Immediate** (`FAST_CAPTURE`, `BLAST_CAPTURE`): No trigger, captures immediately

### LogicAnalyzer Firmware Changes

To modify Dr. Gusman's LogicAnalyzer firmware:

#### 1. Add includes:
```c
#include "hardware/structs/clocks.h"
#include "hardware/structs/iobank0.h"
#include "hardware/regs/pads_bank0.h"
```

#### 2. Add clock switch functions (copy from slave.c):

```c
void configure_gpin0(uint gpio) {
    // gpio must be 20 for GPIN0 or 22 for GPIN1
    hw_write_masked(&pads_bank0_hw->io[gpio],
                    PADS_BANK0_GPIO0_IE_BITS,
                    PADS_BANK0_GPIO0_IE_BITS | PADS_BANK0_GPIO0_OD_BITS);
    iobank0_hw->io[gpio].ctrl = GPIO_FUNC_GPCK << IO_BANK0_GPIO0_CTRL_FUNCSEL_LSB;
}

void switch_to_external_clock(void) {
    hw_clear_bits(&clocks_hw->clk[clk_sys].ctrl, CLOCKS_CLK_SYS_CTRL_SRC_BITS);
    while (clocks_hw->clk[clk_sys].selected != 1) tight_loop_contents();
    
    hw_write_masked(&clocks_hw->clk[clk_sys].ctrl,
                    CLOCKS_CLK_SYS_CTRL_AUXSRC_VALUE_CLKSRC_GPIN0 << CLOCKS_CLK_SYS_CTRL_AUXSRC_LSB,
                    CLOCKS_CLK_SYS_CTRL_AUXSRC_BITS);
    clocks_hw->clk[clk_sys].div = 1 << CLOCKS_CLK_SYS_DIV_INT_LSB;
    
    hw_set_bits(&clocks_hw->clk[clk_sys].ctrl, 
                CLOCKS_CLK_SYS_CTRL_SRC_VALUE_CLKSRC_CLK_SYS_AUX << CLOCKS_CLK_SYS_CTRL_SRC_LSB);
    while (clocks_hw->clk[clk_sys].selected != 2) tight_loop_contents();
    
    clock_set_reported_hz(clk_sys, 144 * MHZ);  // Match DUT frequency
}

void switch_to_internal_clock(void) {
    hw_clear_bits(&clocks_hw->clk[clk_sys].ctrl, CLOCKS_CLK_SYS_CTRL_SRC_BITS);
    while (clocks_hw->clk[clk_sys].selected != 1) tight_loop_contents();
    
    hw_write_masked(&clocks_hw->clk[clk_sys].ctrl,
                    CLOCKS_CLK_SYS_CTRL_AUXSRC_VALUE_CLKSRC_PLL_SYS << CLOCKS_CLK_SYS_CTRL_AUXSRC_LSB,
                    CLOCKS_CLK_SYS_CTRL_AUXSRC_BITS);
    
    hw_set_bits(&clocks_hw->clk[clk_sys].ctrl, 
                CLOCKS_CLK_SYS_CTRL_SRC_VALUE_CLKSRC_CLK_SYS_AUX << CLOCKS_CLK_SYS_CTRL_SRC_LSB);
    while (clocks_hw->clk[clk_sys].selected != 2) tight_loop_contents();
    
    clock_set_reported_hz(clk_sys, 125 * MHZ);
    busy_wait_ms(100);  // Let USB recover
}
```

#### 3. Modify capture start (add clock switch):

```c
void startCapture(CaptureSettings* settings) {
    // NEW: Setup GPIO20 as clock input (once, at init)
    configure_gpin0(20);
    
    // NEW: Verify external clock exists
    uint32_t freq = frequency_count_khz(CLOCKS_FC0_SRC_VALUE_CLKSRC_GPIN0);
    if (freq < 100000) {
        // Optional: fall back to internal clock or report error
    }
    
    // NEW: Switch to external clock
    switch_to_external_clock();
    
    // EXISTING: Configure PIO with trigger settings (unchanged)
    configure_pio_trigger(settings);
    
    // EXISTING: Start DMA (unchanged)
    start_dma();
    
    // EXISTING: Enable PIO - waits for trigger (unchanged)
    pio_sm_set_enabled(pio, sm, true);
}
```

#### 4. Modify capture complete (add clock switch back):

```c
void onCaptureComplete() {
    // NEW: Switch back to internal clock BEFORE any USB activity
    switch_to_internal_clock();
    
    // EXISTING: Send data over USB (unchanged)
    send_capture_data();
}
```

### Summary of Changes

| Component | Changes Required |
|-----------|-----------------|
| **DUT (Master)** | Add 1 line: `clock_gpio_init(21, ...)` |
| **LogicAnalyzer Firmware** | Add clock switch functions, call before/after capture |
| **LogicAnalyzer GUI** | None - works exactly as before |
| **PIO Programs** | None - trigger logic unchanged |
| **Wiring** | Add GPIO21→GPIO20 (clock), keep all data pins same |

## Files in This Project

| File | Purpose |
|------|---------|
| `master.c` | Clock source and test pattern generator |
| `slave.c` | External clock receiver and pattern capture |
| `test_output.pio` | PIO program for outputting test pattern |
| `test_capture.pio` | PIO program for capturing test pattern |
| `CMakeLists.txt` | Build configuration |
| `LogicAnalyzer.pio` | Reference: Dr. Gusman's capture programs |

## Key Lessons Learned

1. **Minimal DUT changes** - Only need `clock_gpio_init()` on DUT - one line of code!

2. **Use existing triggers** - LogicAnalyzer's trigger mechanism works unchanged; no sync pulse needed

3. **Use PLL_SYS not CLK_SYS for clock output** - CLK_SYS goes through extra dividers

4. **GPIO21 is GPOUT0, GPIO20 is GPIN0** - Only certain GPIOs can be clock I/O

5. **Use direct register access for clock switching** - `clock_configure()` doesn't work for GPIN0

6. **USB breaks during external clock** - Switch back to internal before any USB activity

7. **High drive strength + fast slew** - Essential for 144 MHz signal integrity:
   ```c
   gpio_set_drive_strength(pin, GPIO_DRIVE_STRENGTH_12MA);
   gpio_set_slew_rate(pin, GPIO_SLEW_RATE_FAST);
   ```

8. **CuteCom crashes on clock switch** - Use minicom instead

9. **No printf during external clock** - Store results in RAM, print after switching back

10. **Verify clock before switching** - Use `frequency_count_khz()` to check external clock

11. **Clock switch sequence matters:**
    - Always go through clk_ref as intermediate
    - Wait for `clocks_hw->clk[clk_sys].selected` to confirm switch

12. **Pre-trigger capture still works** - LogicAnalyzer's circular buffer captures data before trigger fires

## RP2040 Clock Pin Constraints

| Function | GPIO | Notes |
|----------|------|-------|
| GPIN0 | GPIO20 only | External clock input |
| GPIN1 | GPIO22 only | Alternative external clock input |
| GPOUT0 | GPIO21 | Clock output |
| GPOUT1 | GPIO23 | Not accessible on Pico (SMPS) |
| GPOUT2 | GPIO24 | Clock output |
| GPOUT3 | GPIO25 | Onboard LED on Pico |

## References

- [RP2040 Datasheet](https://datasheets.raspberrypi.com/rp2040/rp2040-datasheet.pdf) - Section 2.15 (Clocks)
- [Pico SDK Documentation](https://raspberrypi.github.io/pico-sdk-doxygen/)
- [Dr. Gusman's LogicAnalyzer](https://github.com/gusmanb/logicanalyzer) - Target for integration
