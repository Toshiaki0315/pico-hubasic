#include <gtest/gtest.h>
#include "parser.h"
#include "mock_hal_display.h"
#include "lexer.h"
#include <string>

class GpioBrightnessTest : public ::testing::Test {
protected:
    void SetUp() override {
        mock_hal::reset();
        clear_program();
    }

    TokenList lex_tokens(const char* input) {
        return lex(input);
    }
};

TEST_F(GpioBrightnessTest, GpioOutput) {
    parse_and_execute(lex_tokens("GPIO 2, 1, 1")); // Pin 2, Mode 1 (Out), Value 1 (High)
    
    auto cmds = mock_hal::get_gpio_commands();
    ASSERT_EQ(cmds.size(), 1);
    EXPECT_EQ(cmds[0].pin, 2);
    EXPECT_EQ(cmds[0].mode, 1);
    EXPECT_EQ(cmds[0].value, true);
}

TEST_F(GpioBrightnessTest, GpioInputWithPullUp) {
    parse_and_execute(lex_tokens("GPIO 15, 0, 0, 1")); // Pin 15, Mode 0 (In), Pull 1 (Up)
    
    auto cmds = mock_hal::get_gpio_commands();
    ASSERT_EQ(cmds.size(), 1);
    EXPECT_EQ(cmds[0].pin, 15);
    EXPECT_EQ(cmds[0].mode, 0);
    EXPECT_EQ(cmds[0].pull, 1);
}

TEST_F(GpioBrightnessTest, BrightnessCommand) {
    parse_and_execute(lex_tokens("BRIGHTNESS 75"));
    
    auto levels = mock_hal::get_brightness_levels();
    ASSERT_EQ(levels.size(), 1);
    EXPECT_EQ(levels[0], 75);
}

TEST_F(GpioBrightnessTest, GpioImplicitArguments) {
    parse_and_execute(lex_tokens("GPIO 10, 0")); // Only pin and mode
    
    auto cmds = mock_hal::get_gpio_commands();
    ASSERT_EQ(cmds.size(), 1);
    EXPECT_EQ(cmds[0].pin, 10);
    EXPECT_EQ(cmds[0].mode, 0);
    EXPECT_EQ(cmds[0].pull, 0);
}
