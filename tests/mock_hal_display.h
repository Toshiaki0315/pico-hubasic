#pragma once
#include <string>
#include <vector>

// ---------------------------------------------------------
// Host-side Mock Interface for Testing
// ---------------------------------------------------------

namespace mock_hal {

// Fetch all text printed via hal_display_print
std::vector<std::string> get_printed_lines();

// Fetch the raw concatenated string printed
std::string get_raw_print_buffer();

// Reset the internal state for the next test
void reset();

// Check if clear screen was called
bool was_cls_called();

} // namespace mock_hal
