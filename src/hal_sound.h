#pragma once
#include <stdint.h>

// ---------------------------------------------------------
// Sound Hardware Abstraction Layer
// ---------------------------------------------------------

#ifdef __cplusplus
extern "C" {
#endif

void hal_sound_init();
void hal_sound_beep();
void hal_sound_play(float frequency, int duration_ms);
void hal_sound_set_volume(int volume); // 0-15

#ifdef __cplusplus
}
#endif
