#include <gtest/gtest.h>
#include "parser.h"
#include "hal_display.h"
#include "mock_hal_display.h"
#include "lexer.h"
#include <string>

class AdvancedGraphicsTest : public ::testing::Test {
protected:
    void SetUp() override {
        mock_hal::reset();
        clear_program();
    }

    TokenList lex_tokens(const char* input) {
        return lex(input);
    }
};

TEST_F(AdvancedGraphicsTest, PaintCommand) {
    // Draw a rectangle border
    parse_and_execute(lex_tokens("COLOR 1")); // Blue
    parse_and_execute(lex_tokens("LINE 0,0,10,0"));
    parse_and_execute(lex_tokens("LINE 10,0,10,10"));
    parse_and_execute(lex_tokens("LINE 10,10,0,10"));
    parse_and_execute(lex_tokens("LINE 0,10,0,0"));
    
    // Paint inside
    parse_and_execute(lex_tokens("PAINT (5,5), 2")); // Green
    
    // Verify interior pixel
    EXPECT_EQ(hal_graphics_get_pixel(5, 5), 0x07E0); // PALETTE[2] (Green)
    EXPECT_EQ(hal_graphics_get_pixel(0, 0), 0x001F); // PALETTE[1] (Blue) - Border
}

TEST_F(AdvancedGraphicsTest, GetAtPutAt) {
    // 1. Draw something
    parse_and_execute(lex_tokens("PSET 5,5,1"));
    parse_and_execute(lex_tokens("PSET 6,5,2"));
    
    // 2. Capture to array
    parse_and_execute(lex_tokens("DIM A(10)"));
    parse_and_execute(lex_tokens("GET_AT (5,5)-(6,5), A"));
    
    // 3. Clear and Put at new location
    parse_and_execute(lex_tokens("CLS"));
    EXPECT_EQ(hal_graphics_get_pixel(5, 5), 0);
    
    parse_and_execute(lex_tokens("PUT_AT (10,10), A"));
    
    // 4. Verify
    EXPECT_EQ(hal_graphics_get_pixel(10, 10), 0x001F); // Blue
    EXPECT_EQ(hal_graphics_get_pixel(11, 10), 0x07E0); // Green
}
