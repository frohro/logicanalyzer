/**
 * Slave Pico - External Clock and Test Capture
 * 
 * HUMAN-FRIENDLY TEST SEQUENCE:
 * 1. Flash master.c to one Pico first
 * 2. Flash this to the other Pico (it becomes the slave)
 * 3. Connect wires:
 *    - Master GPIO21 -> Slave GPIO20 (clock)
 *    - Master GPIO22 -> Slave GPIO22 (sync)  
 *    - Master GPIO10 -> Slave GPIO10 (data)
 *    - GND -> GND
 * 4. Open terminal to THIS (slave) first, press Enter when prompted
 * 5. Open terminal to MASTER, press Enter when prompted
 * 6. Read results HERE on slave terminal
 * 
 * NO TIME PRESSURE - everything waits for your input!
 */

#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "hardware/structs/clocks.h"
#include "hardware/structs/iobank0.h"
#include "hardware/regs/pads_bank0.h"
#include "hardware/dma.h"
#include "test_capture.pio.h"

#define CLOCK_IN_PIN 20
#define SYNC_PIN 22
#define DATA_PIN 10
#define LED_PIN 25
#define NUM_SAMPLES 256

uint32_t capture_buffer[NUM_SAMPLES];

// Store results before switching clocks back
volatile bool capture_done = false;

int main() {
    // Setup LED first
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    
    // Blink 5 times to show we're the slave (different from master's 3)
    for (int i = 0; i < 10; i++) {
        gpio_put(LED_PIN, i & 1);
        sleep_ms(100);
    }
    
    stdio_init_all();
    
    // LED solid = running
    gpio_put(LED_PIN, 1);
    
    // Configure GPIO20 as clock input BEFORE printing
    // (so we can measure external clock frequency)
    hw_write_masked(&pads_bank0_hw->io[CLOCK_IN_PIN],
                    PADS_BANK0_GPIO0_IE_BITS,
                    PADS_BANK0_GPIO0_IE_BITS | PADS_BANK0_GPIO0_OD_BITS);
    iobank0_hw->io[CLOCK_IN_PIN].ctrl = GPIO_FUNC_GPCK << IO_BANK0_GPIO0_CTRL_FUNCSEL_LSB;
    
    // Wait FOREVER for terminal connection
    printf("\n\n");
    printf("========================================\n");
    printf("           SLAVE PICO READY            \n");
    printf("========================================\n");
    printf("\n");
    printf("Clock input: GPIO20 (from master GPIO21)\n");
    printf("Sync input:  GPIO22 (from master GPIO22)\n");
    printf("Data input:  GPIO10 (from master GPIO10)\n");
    printf("\n");
    
    // Check for external clock
    uint32_t ext_freq = frequency_count_khz(CLOCKS_FC0_SRC_VALUE_CLKSRC_GPIN0);
    printf("External clock detected: %lu kHz", ext_freq);
    
    if (ext_freq > 100000) {
        printf(" (OK!)\n");
    } else if (ext_freq > 0) {
        printf(" (LOW - check master)\n");
    } else {
        printf(" (NONE - is master running? check wiring)\n");
    }
    
    printf("\n");
    printf("INSTRUCTIONS:\n");
    printf("1. If clock shows 0 or LOW, check:\n");
    printf("   - Master is flashed and running (LED on)\n");
    printf("   - Wire from Master GPIO21 to Slave GPIO20\n");
    printf("2. Press ENTER here to arm capture\n");
    printf("3. Then press ENTER on MASTER to send data\n");
    printf("\n");
    
    if (ext_freq < 100000) {
        printf("WARNING: External clock not detected!\n");
        printf("Continue anyway? (capture will probably fail)\n");
    }
    
    printf(">>> Press ENTER to arm capture... ");
    fflush(stdout);
    
    getchar();
    
    printf("\nArming capture...\n");
    
    // Re-check clock
    ext_freq = frequency_count_khz(CLOCKS_FC0_SRC_VALUE_CLKSRC_GPIN0);
    printf("External clock now: %lu kHz\n", ext_freq);
    
    if (ext_freq < 100000) {
        printf("\nERROR: Still no external clock!\n");
        printf("Cannot continue. Please check wiring and restart.\n");
        while(1) {
            gpio_put(LED_PIN, 1); sleep_ms(200);
            gpio_put(LED_PIN, 0); sleep_ms(200);
        }
    }
    
    printf("\nSwitching to external clock... (USB will stop working)\n");
    printf("LED OFF = waiting for sync from master\n");
    printf("LED ON  = capture complete, processing...\n");
    printf("\n*** GO PRESS ENTER ON MASTER NOW ***\n\n");
    fflush(stdout);
    sleep_ms(100);  // Let the print finish
    
    // ===== SWITCH TO EXTERNAL CLOCK =====
    // From here until we switch back, NO PRINTF!
    
    // Switch clk_sys to clk_ref first (safe)
    hw_clear_bits(&clocks_hw->clk[clk_sys].ctrl, CLOCKS_CLK_SYS_CTRL_SRC_BITS);
    while (clocks_hw->clk[clk_sys].selected != 1)
        tight_loop_contents();
    
    // Configure GPIN0 as auxiliary source
    hw_write_masked(&clocks_hw->clk[clk_sys].ctrl,
                    CLOCKS_CLK_SYS_CTRL_AUXSRC_VALUE_CLKSRC_GPIN0 << CLOCKS_CLK_SYS_CTRL_AUXSRC_LSB,
                    CLOCKS_CLK_SYS_CTRL_AUXSRC_BITS);
    
    // Set divider to 1
    clocks_hw->clk[clk_sys].div = 1 << CLOCKS_CLK_SYS_DIV_INT_LSB;
    
    // Switch to GPIN0
    hw_set_bits(&clocks_hw->clk[clk_sys].ctrl, 
                CLOCKS_CLK_SYS_CTRL_SRC_VALUE_CLKSRC_CLK_SYS_AUX << CLOCKS_CLK_SYS_CTRL_SRC_LSB);
    while (clocks_hw->clk[clk_sys].selected != 2)
        tight_loop_contents();
    
    clock_set_reported_hz(clk_sys, 200 * MHZ);
    
    // Setup sync pin as input
    gpio_init(SYNC_PIN);
    gpio_set_dir(SYNC_PIN, GPIO_IN);
    
    // Setup data pin as input
    gpio_init(DATA_PIN);
    gpio_set_dir(DATA_PIN, GPIO_IN);
    
    // Setup PIO
    PIO pio = pio0;
    uint sm = 0;
    uint offset = pio_add_program(pio, &test_capture_program);
    
    pio_sm_config c = test_capture_program_get_default_config(offset);
    sm_config_set_in_pins(&c, DATA_PIN);
    sm_config_set_clkdiv(&c, 1.0f);
    sm_config_set_in_shift(&c, true, true, 32);
    sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_RX);
    
    pio_sm_init(pio, sm, offset, &c);
    
    // Setup DMA
    int dma_chan = dma_claim_unused_channel(true);
    dma_channel_config dma_c = dma_channel_get_default_config(dma_chan);
    channel_config_set_transfer_data_size(&dma_c, DMA_SIZE_32);
    channel_config_set_read_increment(&dma_c, false);
    channel_config_set_write_increment(&dma_c, true);
    channel_config_set_dreq(&dma_c, pio_get_dreq(pio, sm, false));
    
    memset(capture_buffer, 0xDE, sizeof(capture_buffer));
    
    // LED OFF = waiting for sync
    gpio_put(LED_PIN, 0);
    
    // Wait for sync signal
    while (gpio_get(SYNC_PIN) == 0) {
        tight_loop_contents();
    }
    
    // Sync received! Start capture
    gpio_put(LED_PIN, 1);
    
    // Start DMA
    dma_channel_configure(
        dma_chan,
        &dma_c,
        capture_buffer,
        &pio->rxf[sm],
        NUM_SAMPLES / 32,
        true
    );
    
    // Start PIO
    pio_sm_set_enabled(pio, sm, true);
    
    // Wait for DMA
    dma_channel_wait_for_finish_blocking(dma_chan);
    
    pio_sm_set_enabled(pio, sm, false);
    
    // ===== SWITCH BACK TO INTERNAL CLOCK =====
    
    hw_clear_bits(&clocks_hw->clk[clk_sys].ctrl, CLOCKS_CLK_SYS_CTRL_SRC_BITS);
    while (clocks_hw->clk[clk_sys].selected != 1)
        tight_loop_contents();
    
    hw_write_masked(&clocks_hw->clk[clk_sys].ctrl,
                    CLOCKS_CLK_SYS_CTRL_AUXSRC_VALUE_CLKSRC_PLL_SYS << CLOCKS_CLK_SYS_CTRL_AUXSRC_LSB,
                    CLOCKS_CLK_SYS_CTRL_AUXSRC_BITS);
    
    hw_set_bits(&clocks_hw->clk[clk_sys].ctrl, 
                CLOCKS_CLK_SYS_CTRL_SRC_VALUE_CLKSRC_CLK_SYS_AUX << CLOCKS_CLK_SYS_CTRL_SRC_LSB);
    while (clocks_hw->clk[clk_sys].selected != 2)
        tight_loop_contents();
    
    clock_set_reported_hz(clk_sys, 125 * MHZ);
    
    // Let USB recover
    busy_wait_ms(100);
    
    // ===== NOW WE CAN PRINT RESULTS =====
    
    printf("\n");
    printf("========================================\n");
    printf("           CAPTURE RESULTS             \n");
    printf("========================================\n");
    printf("\n");
    printf("Captured %d bits (%d words)\n", NUM_SAMPLES, NUM_SAMPLES/32);
    printf("\nFirst 8 words (hex):\n");
    for (int w = 0; w < 8 && w < NUM_SAMPLES/32; w++) {
        printf("  [%d] = 0x%08X\n", w, capture_buffer[w]);
    }
    
    // Analyze
    uint32_t first = capture_buffer[0];
    printf("\nAnalysis:\n");
    printf("Expected pattern: 0x55555555 or 0xAAAAAAAA\n");
    printf("First word:       0x%08X\n", first);
    
    bool pattern_ok = (first == 0x55555555 || first == 0xAAAAAAAA);
    int mismatches = 0;
    
    for (int w = 0; w < NUM_SAMPLES/32; w++) {
        if (capture_buffer[w] != first) {
            mismatches++;
        }
    }
    
    printf("\n");
    if (pattern_ok && mismatches == 0) {
        printf("*** SUCCESS! ***\n");
        printf("All %d words match the expected alternating pattern.\n", NUM_SAMPLES/32);
        printf("Clock synchronization is working!\n");
        if (first == 0x55555555) {
            printf("Phase: captured starting with bit 0\n");
        } else {
            printf("Phase: captured starting with bit 1\n");
        }
    } else if (first == 0xDEDEDEDE) {
        printf("*** FAILURE: No data captured ***\n");
        printf("Buffer still contains initial pattern.\n");
        printf("Check: GPIO10 connection, sync timing\n");
    } else if (!pattern_ok) {
        printf("*** FAILURE: Unexpected pattern ***\n");
        printf("First word is not alternating bits.\n");
        printf("Check: GPIO10 connection, clock sync\n");
    } else {
        printf("*** PARTIAL: Pattern changes ***\n");
        printf("%d of %d words differ from first.\n", mismatches, NUM_SAMPLES/32);
    }
    
    printf("\n========================================\n");
    
    // Final blink pattern
    while (true) {
        if (pattern_ok && mismatches == 0) {
            // Fast triple blink = success
            for (int i = 0; i < 3; i++) {
                gpio_put(LED_PIN, 1); sleep_ms(100);
                gpio_put(LED_PIN, 0); sleep_ms(100);
            }
            sleep_ms(500);
        } else {
            // Slow blink = failure
            gpio_put(LED_PIN, 1); sleep_ms(500);
            gpio_put(LED_PIN, 0); sleep_ms(500);
        }
    }
    
    return 0;
}
