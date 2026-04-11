#include "hal_display.h"
#include "pico/stdlib.h"
// #include "hardware/spi.h"

void hal_display_init() {
    // TODO: Phase 2 - Initialize SPI and Waveshare 2.8" LCD panel
    // (e.g., configure pins, reset display, send init commands)
}

void hal_display_print(const std::string& text) {
    // TODO: Phase 2 - Render text to LCD frame buffer / directly to SPI
    // Must implement text wrapping and scrolling behavior
}

void hal_display_cls() {
    // TODO: Phase 2 - Clear screen with the current background color
}

void hal_display_locate(int x, int y) {
    // TODO: Phase 2 - Update text cursor position mapping to LCD coordinates
}
