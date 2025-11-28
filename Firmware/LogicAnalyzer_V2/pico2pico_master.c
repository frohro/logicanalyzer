/**
 * PICO2PICO Master - Clock Output and Test Pattern Generator
 * 
 * This is the test/DUT program for the master Pico in a PICO2PICO setup.
 * It outputs:
 *   - System clock on GPIO21 (GPOUT0) -> slave's GPIO20 (GPIN0)
 *   - Test pattern on GPIO10 using PIO (slow 10 MHz square wave for debugging)
 * 
 * The slave LogicAnalyzer captures using the master's clock (state mode).
 * 
 * Wiring (stacked Picos):
 *   Master GPIO21 -> Slave GPIO20 (clock, via jumper wire)
 *   Master GPIO10 -> Slave GPIO10 (test data, directly connected when stacked)
 * 
 * CHANNEL MAPPING (LogicAnalyzer channels start at 1, not 0):
 *   GPIO2  = Channel 1      GPIO12 = Channel 11
 *   GPIO3  = Channel 2      GPIO13 = Channel 12
 *   GPIO4  = Channel 3      GPIO14 = Channel 13
 *   GPIO5  = Channel 4      GPIO15 = Channel 14
 *   GPIO6  = Channel 5      GPIO16 = Channel 15
 *   GPIO7  = Channel 6      GPIO17 = Channel 16
 *   GPIO8  = Channel 7      GPIO18 = Channel 17
 *   GPIO9  = Channel 8      GPIO19 = Channel 18
 *   GPIO10 = Channel 9  <-- TEST DATA
 *   GPIO11 = Channel 10
 *   (GPIO20/21 not mapped - used for clock)
 *   GPIO22 = Channel 19
 *   GPIO23 = Channel 20
 *   GPIO24 = Channel 21
 *   GPIO25 = Channel 22 (LED)
 *   GPIO26 = Channel 23
 *   GPIO27 = Channel 24
 *   GPIO28 = Channel 25
 */

#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/clocks.h"
#include "hardware/gpio.h"
#include "hardware/pio.h"
#include "hardware/dma.h"
#include "hardware/vreg.h"

#define CLOCK_OUT_PIN 21    // GPOUT0 - clock to slave's GPIO20 (GPIN0)
#define TEST_DATA_PIN 10    // Test pattern output (PIO driven) = Channel 9
#define LED_PIN 25

// PIO program to output 1 bit with a half-cycle delay
// The OUT happens on the second cycle, so data changes AFTER the rising edge
// This gives the slave time to sample the stable value on the rising edge
//
// Cycle 0: NOP (data still at old value - slave samples here)
// Cycle 1: OUT pins, 1 (data changes to new value)
// 
// With clkdiv=1.25, each PIO cycle = 10ns, so:
// - Data stable for 10ns, then changes
// - At 125 MHz sampling (8ns), slave samples during the stable period
//
#define test_output_wrap_target 0
#define test_output_wrap 1

static const uint16_t test_output_program_instructions[] = {
    0xA042, //  0: nop              ; delay - data is stable, slave samples here
    0x6001, //  1: out    pins, 1   ; change data after slave has sampled
};

static const struct pio_program test_output_program = {
    .instructions = test_output_program_instructions,
    .length = 2,
    .origin = -1,
};

// System clock frequency - 125 MHz
#define SYS_CLOCK_KHZ 125000

// PIO clock divider
// The PIO program is 2 instructions per bit (NOP + OUT)
// At 125 MHz sys clock with divider of 1.25:
//   PIO clock = 125/1.25 = 100 MHz
//   Bit rate = 100/2 = 50 MHz (each bit takes 2 PIO cycles)
//   Square wave freq = 50/2 = 25 MHz
// This gives 20ns per bit, matching 2 samples at 100 MHz or 2.5 at 125 MHz
#define PIO_CLK_DIV 1.25f

int main() {
    // Initialize stdio first at default clock for reliable USB enumeration
    stdio_init_all();
    
    // Setup LED immediately so we can see if the Pico is running
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    gpio_put(LED_PIN, 1);
    
    // Wait for USB to enumerate (blink LED while waiting)
    for (int i = 0; i < 30; i++) {  // Wait up to 3 seconds
        gpio_xor_mask(1 << LED_PIN);
        sleep_ms(100);
    }
    gpio_put(LED_PIN, 1);  // LED on solid
    
    // Now set the system clock (after USB is stable)
    set_sys_clock_khz(SYS_CLOCK_KHZ, true);
    
    // Output system clock on GPIO21 (GPOUT0)
    // This goes to the slave's GPIO20 (GPIN0) for state-mode capture
    // Use high drive strength and fast slew for signal integrity
    gpio_set_drive_strength(CLOCK_OUT_PIN, GPIO_DRIVE_STRENGTH_12MA);
    gpio_set_slew_rate(CLOCK_OUT_PIN, GPIO_SLEW_RATE_FAST);
    clock_gpio_init(CLOCK_OUT_PIN, CLOCKS_CLK_GPOUT0_CTRL_AUXSRC_VALUE_CLK_SYS, 1);
    
    // Setup PIO for test pattern output on GPIO10
    PIO pio = pio0;
    uint sm = pio_claim_unused_sm(pio, true);
    uint offset = pio_add_program(pio, &test_output_program);
    
    // Configure the state machine
    pio_sm_config c = pio_get_default_sm_config();
    sm_config_set_wrap(&c, offset + test_output_wrap_target, offset + test_output_wrap);
    sm_config_set_out_pins(&c, TEST_DATA_PIN, 1);
    sm_config_set_clkdiv(&c, PIO_CLK_DIV);  // Slow down for debugging
    sm_config_set_out_shift(&c, true, true, 32);  // Autopull, shift right, 32 bits
    
    // Configure GPIO for PIO
    pio_gpio_init(pio, TEST_DATA_PIN);
    pio_sm_set_consecutive_pindirs(pio, sm, TEST_DATA_PIN, 1, true);
    gpio_set_drive_strength(TEST_DATA_PIN, GPIO_DRIVE_STRENGTH_12MA);
    gpio_set_slew_rate(TEST_DATA_PIN, GPIO_SLEW_RATE_FAST);
    
    // Initialize and enable state machine
    pio_sm_init(pio, sm, offset, &c);
    
    // Configure DMA to feed test pattern to PIO
    int dma_chan = dma_claim_unused_channel(true);
    dma_channel_config dma_c = dma_channel_get_default_config(dma_chan);
    channel_config_set_transfer_data_size(&dma_c, DMA_SIZE_32);
    channel_config_set_read_increment(&dma_c, false);  // Always read from same address
    channel_config_set_write_increment(&dma_c, false);
    channel_config_set_dreq(&dma_c, pio_get_dreq(pio, sm, true));
    
    // Test pattern: 0x55555555 = alternating 01010101...
    // When shifted out LSB first, this creates a toggling signal
    static uint32_t test_pattern = 0x55555555;
    
    // Calculate and display frequencies
    // PIO program is 2 cycles per bit (NOP + OUT)
    uint32_t sys_clk_hz = clock_get_hz(clk_sys);
    float pio_freq_hz = (float)sys_clk_hz / PIO_CLK_DIV;
    float bit_rate_hz = pio_freq_hz / 2.0f;  // 2 PIO cycles per bit
    float square_wave_freq_hz = bit_rate_hz / 2.0f;  // 2 bits per square wave cycle
    float bit_period_ns = 1000000000.0f / bit_rate_hz;
    float half_period_ns = bit_period_ns;  // One bit = one half of square wave
    float full_period_ns = half_period_ns * 2.0f;
    
    printf("\n");
    printf("================================================\n");
    printf("     PICO2PICO Master - Test Pattern Gen        \n");
    printf("================================================\n");
    printf("\n");
    printf("CLOCK FREQUENCIES:\n");
    printf("  System clock:      %lu Hz (%lu MHz)\n", sys_clk_hz, sys_clk_hz/1000000);
    printf("  Clock on GPIO21:   %lu Hz (%lu MHz) -> Slave GPIO20\n", sys_clk_hz, sys_clk_hz/1000000);
    printf("  PIO clock:         %.3f MHz (sys_clk / %.2f)\n", pio_freq_hz/1000000.0f, PIO_CLK_DIV);
    printf("  Bit rate:          %.3f MHz (PIO/2, since 2 cycles per bit)\n", bit_rate_hz/1000000.0f);
    printf("  Square wave:       %.3f MHz on GPIO10\n", square_wave_freq_hz/1000000.0f);
    printf("\n");
    printf("TIMING (GPIO10 test pattern):\n");
    printf("  Half-period:       %.2f ns (one high or one low)\n", half_period_ns);
    printf("  Full period:       %.2f ns\n", full_period_ns);
    printf("  At 100 MHz sample: %.2f samples per half-period\n", half_period_ns / 10.0f);
    printf("\n");
    printf("CHANNEL MAPPING (LogicAnalyzer GUI):\n");
    printf("  GPIO10 (test data) = Channel 9\n");
    printf("  GPIO22             = Channel 19\n");
    printf("  GPIO25 (LED)       = Channel 22\n");
    printf("\n");
    printf("Trigger on Channel 9 edge to capture the pattern.\n");
    printf("Expected capture: 0x55555555 or 0xAAAAAAAA\n");
    printf("\n");
    
    // Start PIO state machine
    pio_sm_set_enabled(pio, sm, true);
    
    // Start DMA - runs CONTINUOUSLY forever
    dma_channel_configure(dma_chan, &dma_c,
        &pio->txf[sm],      // Write to PIO TX FIFO
        &test_pattern,      // Read from test_pattern (doesn't increment)
        0xFFFFFFFF,         // Maximum transfers
        true                // Start immediately
    );
    
    printf(">>> Test pattern RUNNING on Channel 9 <<<\n");
    printf("LED blinking to show master is alive...\n");
    printf("\n");
    
    // Keep LED blinking to show we're alive
    while (true) {
        gpio_xor_mask(1 << LED_PIN);
        sleep_ms(500);
    }
    
    return 0;
}
