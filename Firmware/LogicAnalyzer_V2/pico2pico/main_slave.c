#include <stdio.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "hardware/clocks.h"
#include "hardware/pll.h"
#include "hardware/vreg.h"
#include "hardware/gpio.h"

// This demo configures the Pico to use an external clock on GPIO20 (GPIN0)
// Connect the master Pico's GPIO21 output to this Pico's GPIO20 input
//
// The external 120MHz clock is used directly as the system clock without PLL
// This ensures both Picos run synchronously from the same clock source

#define EXTERNAL_CLOCK_PIN 20  // GPIO20 = GPIN0
#define LED_PIN 25             // Onboard LED
#define TEST_OUTPUT_PIN 22     // GPIO22 - output a test signal synchronized to the clock

// External clock frequency (should match master's output)
#define EXTERNAL_CLOCK_HZ 120000000  // 120 MHz

int main() {
    // Set voltage regulator to 1.1V for stable 120MHz operation
    vreg_set_voltage(VREG_VOLTAGE_1_10);
    sleep_ms(10);
    
    // CRITICAL: Configure to use external clock on GPIN0 (GPIO20) as system clock
    // This makes the system clock directly driven by the external input
    // Both Picos will now run synchronously from the same clock source
    
    // First, configure the GPIO20 as clock input (GPIN0 function)
    gpio_set_function(EXTERNAL_CLOCK_PIN, GPIO_FUNC_GPCK);
    
    // Configure clk_sys to use GPIN0 as source (bypassing PLL)
    // This gives us direct synchronous operation with the master Pico
    clock_configure(clk_sys,
                    CLOCKS_CLK_SYS_CTRL_SRC_VALUE_CLKSRC_CLK_SYS_AUX,
                    CLOCKS_CLK_SYS_CTRL_AUXSRC_VALUE_CLKSRC_GPIN0,
                    EXTERNAL_CLOCK_HZ,
                    EXTERNAL_CLOCK_HZ);
    
    // Reconfigure peripheral clocks to work with the new system clock
    clock_configure(clk_peri,
                    0,
                    CLOCKS_CLK_PERI_CTRL_AUXSRC_VALUE_CLK_SYS,
                    EXTERNAL_CLOCK_HZ,
                    EXTERNAL_CLOCK_HZ);
    
    // USB clock needs 48MHz - derive from system clock
    clock_configure(clk_usb,
                    0,
                    CLOCKS_CLK_USB_CTRL_AUXSRC_VALUE_CLKSRC_PLL_USB,
                    48000000,
                    48000000);
    
    // Now initialize stdio (after clocks are configured)
    stdio_init_all();
    sleep_ms(100); // Give USB time to enumerate
    
    uint32_t actual_sys_freq = clock_get_hz(clk_sys);
    printf("\n=== Slave Pico - External Clock Input Demo ===\n");
    printf("System clock source: External via GPIN0 (GPIO%d)\n", EXTERNAL_CLOCK_PIN);
    printf("System clock frequency: %u Hz (%u MHz)\n", actual_sys_freq, actual_sys_freq / 1000000);
    printf("Expected frequency: %u Hz (%u MHz)\n", EXTERNAL_CLOCK_HZ, EXTERNAL_CLOCK_HZ / 1000000);
    
    if (actual_sys_freq > 0) {
        printf("SUCCESS: External clock is active!\n");
    } else {
        printf("WARNING: No clock detected on GPIO%d\n", EXTERNAL_CLOCK_PIN);
        printf("Make sure master Pico's GPIO21 is connected to this Pico's GPIO20\n");
    }
    
    // Configure LED and test output pin
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    
    gpio_init(TEST_OUTPUT_PIN);
    gpio_set_dir(TEST_OUTPUT_PIN, GPIO_OUT);
    
    printf("\nTest output on GPIO%d - will toggle synchronized with system clock\n", TEST_OUTPUT_PIN);
    printf("Connect logic analyzer to GPIO%d to verify synchronous operation\n", TEST_OUTPUT_PIN);
    printf("\nRunning synchronized test loop...\n");
    
    uint32_t counter = 0;
    bool led_state = false;
    
    // Main loop - demonstrates synchronous operation
    while (true) {
        // Toggle test output at a rate synchronized to the system clock
        gpio_put(TEST_OUTPUT_PIN, counter & 0x100);
        
        // Blink LED slowly to show we're alive
        if (counter % 120000000 == 0) {  // Once per second at 120MHz
            led_state = !led_state;
            gpio_put(LED_PIN, led_state);
            printf("Tick: %u seconds, sys_clk=%u Hz\n", 
                   counter / 120000000, 
                   clock_get_hz(clk_sys));
        }
        
        counter++;
        
        // Tight loop for fast toggling
        tight_loop_contents();
    }

    return 0;
}
