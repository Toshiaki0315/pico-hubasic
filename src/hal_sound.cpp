#include "hal_sound.h"
#include <stdio.h>

static int current_volume = 15;

void hal_sound_init() {
    // Initialization for mock sound
}

void hal_sound_beep() {
    printf("[Sound] BEEP!\n");
}

void hal_sound_play(float frequency, int duration_ms) {
    if (frequency <= 0) {
        printf("[Sound] REST %d ms\n", duration_ms);
    } else {
        printf("[Sound] PLAY %0.2f Hz for %d ms (Vol: %d)\n", frequency, duration_ms, current_volume);
    }
}

void hal_sound_set_volume(int volume) {
    if (volume < 0) volume = 0;
    if (volume > 15) volume = 15;
    current_volume = volume;
    printf("[Sound] Volume set to %d\n", current_volume);
}
