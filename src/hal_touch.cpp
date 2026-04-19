#include "hal_touch.h"
#include <stdio.h>

// ---------------------------------------------------------
// Host/Stub implementation of Touch HAL
// On actual RP2350 hardware, this would read from the
// XPT2046 (or CST816S) touch controller via SPI/I2C.
// ---------------------------------------------------------

static int touch_state  = 0;
static int touch_x      = 0;
static int touch_y      = 0;

void hal_touch_init() {
    touch_state = 0;
    touch_x     = 0;
    touch_y     = 0;
}

int hal_touch_is_touched() {
    return touch_state;
}

int hal_touch_get_x() {
    return touch_x;
}

int hal_touch_get_y() {
    return touch_y;
}
