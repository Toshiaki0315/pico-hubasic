#include "pico/stdlib.h"
#include <stdio.h>
#include "hal_display.h"
#include "repl.h"

int main() {
    // Initialize standard I/O (USB CDC)
    stdio_init_all();

    // Phase 2: Initialize Waveshare 2.8" Touch LCD via SPI
    hal_display_init();

    // Phase 1 & 2: Start the BASIC Read-Eval-Print Loop
    // REPL handles both Serial and LCD outputs
    repl_start();

    return 0;
}
