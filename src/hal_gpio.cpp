#include "hal_gpio.h"

/* Pico SDK ビルドでは pico/stdlib.h が存在する。ホストテストでは mock が GPIO を提供 */
#if __has_include("pico/stdlib.h")
#include "pico/stdlib.h"
#include "hardware/gpio.h"

void hal_gpio_init(int pin, int mode, int pull) {
    gpio_init(pin);
    if (mode == 1) { // Output
        gpio_set_dir(pin, GPIO_OUT);
    } else { // Input
        gpio_set_dir(pin, GPIO_IN);
        if (pull == 1) gpio_pull_up(pin);
        else if (pull == 2) gpio_pull_down(pin);
        else {
            gpio_disable_pulls(pin);
        }
    }
}

void hal_gpio_write(int pin, bool value) {
    gpio_put(pin, value);
}

bool hal_gpio_read(int pin) {
    return gpio_get(pin);
}
#else
// ホストでは tests/mock_hal_display.cpp が GPIO をスタブ実装
#endif
