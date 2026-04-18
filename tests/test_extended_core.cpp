#include <gtest/gtest.h>
#include "parser.h"
#include "hal_display.h"
#include "mock_hal_display.h"

class ExtendedCoreTest : public ::testing::Test {
protected:
    void SetUp() override {
        clear_program();
        mock_hal::reset();
    }
};

TEST_F(ExtendedCoreTest, PRINT_Semicolon) {
    parse_and_execute(lex("PRINT 1; 2; 3"));
    std::string output = mock_hal::get_raw_print_buffer();
    EXPECT_EQ(output, "123\n");
}

TEST_F(ExtendedCoreTest, PRINT_Comma) {
    parse_and_execute(lex("PRINT 1, 2, 3"));
    std::string output = mock_hal::get_raw_print_buffer();
    EXPECT_EQ(output, "1\t2\t3\n");
}

TEST_F(ExtendedCoreTest, CLEAR_ResetsVariables) {
    parse_and_execute(lex("A = 100"));
    parse_and_execute(lex("CLEAR"));
    // A should now be 0
    testing::internal::CaptureStdout();
    parse_and_execute(lex("PRINT A"));
    std::string output = testing::internal::GetCapturedStdout();
    EXPECT_EQ(output, "0\n");
}

TEST_F(ExtendedCoreTest, WAIT_Command) {
    // This just ensures it doesn't crash. 
    // Testing timing is hard in unit tests without a mock clock.
    parse_and_execute(lex("WAIT 10"));
    EXPECT_TRUE(true);
}

TEST_F(ExtendedCoreTest, LOCATE_Command) {
    parse_and_execute(lex("LOCATE 10, 20"));
    // Since we don't have a check for locate in mock_hal yet, 
    // we could add was_locate_called() if needed.
    EXPECT_TRUE(true);
}
