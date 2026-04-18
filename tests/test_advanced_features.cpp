#include <gtest/gtest.h>
#include "parser.h"
#include "mock_hal_display.h"

class AdvancedFeaturesTest : public ::testing::Test {
protected:
    void SetUp() override {
        clear_program();
        mock_hal::reset();
    }
};

TEST_F(AdvancedFeaturesTest, MultiStatementColon) {
    parse_and_execute(lex("A = 1 : B = 2 : PRINT A + B"));
    EXPECT_EQ(mock_hal::get_raw_print_buffer(), "3\n");
}

TEST_F(AdvancedFeaturesTest, IfMultiStatement) {
    // Condition true: both should execute
    parse_and_execute(lex("X = 1 : IF X = 1 THEN A = 10 : B = 20"));
    parse_and_execute(lex("PRINT A; B"));
    EXPECT_EQ(mock_hal::get_raw_print_buffer(), "1020\n");
    
    mock_hal::reset();
    // Condition false: rest of line should be skipped
    parse_and_execute(lex("NEW"));
    parse_and_execute(lex("X = 0 : IF X = 1 THEN A = 100 : B = 200"));
    parse_and_execute(lex("PRINT A; B"));
    EXPECT_EQ(mock_hal::get_raw_print_buffer(), "00\n");
}

TEST_F(AdvancedFeaturesTest, OnGoto) {
    store_line(10, lex("X = 2 : ON X GOTO 100, 200, 300"));
    store_line(100, lex("PRINT \"ONE\" : END"));
    store_line(200, lex("PRINT \"TWO\" : END"));
    store_line(300, lex("PRINT \"THREE\" : END"));
    
    parse_and_execute(lex("RUN"));
    EXPECT_EQ(mock_hal::get_raw_print_buffer(), "TWO\n");
}

TEST_F(AdvancedFeaturesTest, MidStatement) {
    parse_and_execute(lex("A$ = \"ABCDE\""));
    parse_and_execute(lex("MID$(A$, 2, 2) = \"XY\""));
    parse_and_execute(lex("PRINT A$"));
    EXPECT_EQ(mock_hal::get_raw_print_buffer(), "AXYDE\n");
}

TEST_F(AdvancedFeaturesTest, PrintTab) {
    // Note: mock_hal doesn't track exact cursor positions in get_raw_print_buffer,
    // but we can check if it calls hal_display_locate by checking internal state if we had it.
    // For now, just ensure it doesn't crash and prints parts correctly.
    parse_and_execute(lex("PRINT \"START\"; TAB(10); \"END\""));
    EXPECT_EQ(mock_hal::get_raw_print_buffer(), "STARTEND\n");
}
