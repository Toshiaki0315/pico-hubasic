#pragma once
#include <stdint.h>
#include <stdbool.h>

// ---------------------------------------------------------
// Hardware Abstraction Layer for GPIO
// ---------------------------------------------------------

// mode: 0=Input, 1=Output
// pull: 0=None, 1=Pull-up, 2=Pull-down
void hal_gpio_init(int pin, int mode, int pull);

void hal_gpio_write(int pin, bool value);

bool hal_gpio_read(int pin);
