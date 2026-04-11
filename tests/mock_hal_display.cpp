#include "hal_display.h"
#include "mock_hal_display.h"

// ---------------------------------------------------------
// Implementation of Mock API
// ---------------------------------------------------------
static std::vector<std::string> printed_lines;
static std::string raw_buffer;
static bool cls_called = false;

namespace mock_hal {

std::vector<std::string> get_printed_lines() {
    return printed_lines;
}

std::string get_raw_print_buffer() {
    return raw_buffer;
}

void reset() {
    printed_lines.clear();
    raw_buffer.clear();
    cls_called = false;
}

bool was_cls_called() {
    return cls_called;
}

} // namespace mock_hal

// ---------------------------------------------------------
// Implementation of real HAL API used by the Parser/Engine
// ---------------------------------------------------------

void hal_display_init() {
    // No-op for host tests
}

void hal_display_print(const std::string& text) {
    raw_buffer += text;
    printed_lines.push_back(text);
}

void hal_display_cls() {
    cls_called = true;
}

void hal_display_locate(int x, int y) {
    // No-op
}
