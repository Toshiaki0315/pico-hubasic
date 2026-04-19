#pragma once
#include <stdint.h>

// ---------------------------------------------------------
// Touch Hardware Abstraction Layer
// ---------------------------------------------------------
// TOUCH(n) function semantics:
//   TOUCH(0) -> X coordinate (0-319)
//   TOUCH(1) -> Y coordinate (0-239)
//   TOUCH(2) -> Touch state: 1 = touched, 0 = not touched

#ifdef __cplusplus
extern "C" {
#endif

// Initialize touch controller
void hal_touch_init();

// Read touch state. Returns 1 if currently touched, 0 otherwise.
int hal_touch_is_touched();

// Read touch X coordinate (0-319). Valid only when hal_touch_is_touched() == 1.
int hal_touch_get_x();

// Read touch Y coordinate (0-239). Valid only when hal_touch_is_touched() == 1.
int hal_touch_get_y();

#ifdef __cplusplus
}
#endif
