#include <stdio.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "hardware/clocks.h"
#include "hardware/pll.h"
#include "hardware/vreg.h"

// This demo routes a clean 120MHz system clock to a GPIO using hardware clock output.
// Using 120MHz with integer PLL dividers reduces jitter compared to 125MHz.
// 
// PLL configuration: 12MHz crystal * 120 / 12 = 120MHz (all integer dividers)

#define CLK_OUT_PIN 21
#define TARGET_CLK_KHZ 120000  // 120 MHz

int main() {
    // Set voltage regulator to 1.1V for stable 120MHz operation
    vreg_set_voltage(VREG_VOLTAGE_1_10);
    sleep_ms(10);
    
    // Configure system clock to 120MHz using PLL with integer dividers
    // This gives much cleaner output than fractional dividers
    set_sys_clock_khz(TARGET_CLK_KHZ, true);
    
    // Now initialize stdio (after clock is configured)
    stdio_init_all();
    
    uint32_t actual_freq = clock_get_hz(clk_sys);
    printf("System clock configured to %u Hz (%u MHz)\n", actual_freq, actual_freq / 1000000);
    printf("Routing clock to GPIO %d via hardware GPOUT\n", CLK_OUT_PIN);

    // Route the system clock directly to GPIO with no divider (integer divide by 1)
    // This provides the cleanest possible output
    clock_gpio_init(CLK_OUT_PIN, CLOCKS_CLK_GPOUT0_CTRL_AUXSRC_VALUE_CLK_SYS, 1);

    printf("Clock output active on GPIO %d\n", CLK_OUT_PIN);
    
    // Busy-loop; hardware does the clock output
    while (true) {
        tight_loop_contents();
    }

    return 0;
}
