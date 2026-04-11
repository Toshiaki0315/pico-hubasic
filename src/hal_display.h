#pragma once
#include <stdint.h>
#include <string>

// PICO_HUBASIC_HAL_DISPLAY_H
// Hardware Abstraction Layer for Display targeting Waveshare RP2350-Touch-LCD-2.8

// Phase 2: SPI Display Driver Implementation
void hal_display_init();

// Text Output related functions
void hal_display_print(const std::string& text);
void hal_display_cls();
void hal_display_locate(int x, int y);

// Future Phase 4: Graphics primitives will be added here
// void hal_display_pset(int x, int y, int color);
// ...
