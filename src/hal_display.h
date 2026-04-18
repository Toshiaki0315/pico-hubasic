#pragma once
#include <stdint.h>

// PICO_HUBASIC_HAL_DISPLAY_H
// Hardware Abstraction Layer for Display targeting Waveshare RP2350-Touch-LCD-2.8

// Phase 2: SPI Display Driver Implementation
void hal_display_init();

// Text Output related functions
void hal_display_print(const char* text);
void hal_display_cls();
void hal_display_locate(int x, int y);

// Text Input related functions
void hal_display_input(char* buffer, int max_len);

void hal_display_set_mock_input(const char* input);
