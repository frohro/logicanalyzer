/**
 * Master Pico - Clock Source and Test Pattern Generator
 * 
 * HUMAN-FRIENDLY TEST SEQUENCE:
 * 1. Flash this to one Pico (it becomes the master)
 * 2. Flash slave.c to the other Pico
 * 3. Connect wires:
 *    - Master GPIO21 -> Slave GPIO20 (clock)
 *    - Master GPIO22 -> Slave GPIO22 (sync)  
 *    - Master GPIO10 -> Slave GPIO10 (data)
 *    - GND -> GND
 * 4. Open terminal to SLAVE first, press Enter when prompted
 * 5. Open terminal to MASTER, press Enter when prompted
 * 6. Read results on SLAVE terminal
 * 
 * NO TIME PRESSURE - everything waits for your input!
 */

#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "hardware/gpio.h"
#include "hardware/dma.h"
#include "test_output.pio.h"

#define CLOCK_OUT_PIN 21
#define SYNC_PIN 22
#define DATA_PIN 10
#define LED_PIN 25
#define NUM_SAMPLES 256

uint32_t test_pattern[NUM_SAMPLES];

int main() {
    // Setup LED first
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    
    // Blink 3 times to show we're the master
    for (int i = 0; i < 6; i++) {
        gpio_put(LED_PIN, i & 1);
        sleep_ms(150);
    }
    
    // Set system clock to 200 MHz (overclocked)
    set_sys_clock_khz(200000, true);
    
    stdio_init_all();
    
    // Output 200 MHz clock on GPIO21 IMMEDIATELY
    // This way slave can detect it whenever it boots
    gpio_set_drive_strength(CLOCK_OUT_PIN, GPIO_DRIVE_STRENGTH_12MA);
    gpio_set_slew_rate(CLOCK_OUT_PIN, GPIO_SLEW_RATE_FAST);
    clock_gpio_init(CLOCK_OUT_PIN, CLOCKS_CLK_GPOUT0_CTRL_AUXSRC_VALUE_CLKSRC_PLL_SYS, 1);
    
    // Setup sync pin - keep LOW until we're ready
    gpio_init(SYNC_PIN);
    gpio_set_dir(SYNC_PIN, GPIO_OUT);
    gpio_put(SYNC_PIN, 0);
    
    // Initialize test pattern: 0x55555555 = 01010101... (alternating bits)
    // Each 32-bit word contains 32 bits of alternating pattern
    for (int i = 0; i < NUM_SAMPLES; i++) {
        test_pattern[i] = 0x55555555;  // All words same pattern
    }
    
    // Setup PIO
    PIO pio = pio0;
    uint sm = 0;
    uint offset = pio_add_program(pio, &test_output_program);
    
    pio_gpio_init(pio, DATA_PIN);
    pio_sm_set_consecutive_pindirs(pio, sm, DATA_PIN, 1, true);
    
    pio_sm_config c = test_output_program_get_default_config(offset);
    sm_config_set_out_pins(&c, DATA_PIN, 1);
    sm_config_set_clkdiv(&c, 1.0f);
    sm_config_set_out_shift(&c, true, true, 32);
    sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_TX);
    
    pio_sm_init(pio, sm, offset, &c);
    
    // Setup DMA
    int dma_chan = dma_claim_unused_channel(true);
    dma_channel_config dma_c = dma_channel_get_default_config(dma_chan);
    channel_config_set_transfer_data_size(&dma_c, DMA_SIZE_32);
    channel_config_set_read_increment(&dma_c, true);
    channel_config_set_write_increment(&dma_c, false);
    channel_config_set_dreq(&dma_c, pio_get_dreq(pio, sm, true));
    
    // LED solid = waiting for user
    gpio_put(LED_PIN, 1);
    
    // Wait FOREVER for terminal connection
    printf("\n\n");
    printf("========================================\n");
    printf("           MASTER PICO READY           \n");
    printf("========================================\n");
    printf("\n");
    printf("Clock output: 144 MHz on GPIO21 (always on)\n");
    printf("Sync output:  GPIO22 (currently LOW)\n");
    printf("Data output:  GPIO10\n");
    printf("\n");
    printf("INSTRUCTIONS:\n");
    printf("1. Make sure SLAVE terminal shows 'READY'\n");
    printf("2. Then press ENTER here to send test pattern\n");
    printf("\n");
    printf(">>> Press ENTER when slave is ready... ");
    fflush(stdout);
    
    getchar();
    
    printf("\nSending test pattern NOW!\n");
    
    // Start DMA
    dma_channel_configure(
        dma_chan,
        &dma_c,
        &pio->txf[sm],
        test_pattern,
        NUM_SAMPLES,
        true
    );
    
    // Start PIO
    pio_sm_set_enabled(pio, sm, true);
    
    // Small delay then assert sync
    busy_wait_us(1);
    gpio_put(SYNC_PIN, 1);
    
    // Wait for DMA to complete
    dma_channel_wait_for_finish_blocking(dma_chan);
    
    // Keep sync high for a bit
    sleep_ms(10);
    gpio_put(SYNC_PIN, 0);
    
    printf("Done! %d samples sent.\n", NUM_SAMPLES);
    printf("\nCheck SLAVE terminal for results.\n");
    
    // Blink slowly to indicate complete
    while (true) {
        gpio_put(LED_PIN, 1);
        sleep_ms(1000);
        gpio_put(LED_PIN, 0);
        sleep_ms(1000);
    }
    
    return 0;
}
