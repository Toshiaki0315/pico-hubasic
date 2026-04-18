#include <gtest/gtest.h>
#include "parser.h"
#include "hal_display.h"

class InputEndStopTest : public ::testing::Test {
protected:
    void SetUp() override {
        clear_program();
    }
    void TearDown() override {
        clear_program();
    }
};

TEST_F(InputEndStopTest, ExecuteEnd) {
    parse_and_execute(lex("10 A = 1"));
    parse_and_execute(lex("20 END"));
    parse_and_execute(lex("30 A = 2"));
    run_program();
    
    testing::internal::CaptureStdout();
    parse_and_execute(lex("PRINT A"));
    std::string output = testing::internal::GetCapturedStdout();
    EXPECT_EQ(output, "1\n");
}

TEST_F(InputEndStopTest, ExecuteStop) {
    parse_and_execute(lex("10 A = 1"));
    parse_and_execute(lex("20 STOP"));
    parse_and_execute(lex("30 A = 2"));
    
    testing::internal::CaptureStdout();
    run_program();
    std::string output = testing::internal::GetCapturedStdout();
    EXPECT_TRUE(output.find("Break in 20") != std::string::npos);
    
    testing::internal::CaptureStdout();
    parse_and_execute(lex("PRINT A"));
    std::string output2 = testing::internal::GetCapturedStdout();
    EXPECT_EQ(output2, "1\n");
}

TEST_F(InputEndStopTest, ExecuteInputString) {
    hal_display_set_mock_input("HELLO");
    testing::internal::CaptureStdout();
    parse_and_execute(lex("INPUT \"Name \"; A$"));
    parse_and_execute(lex("PRINT A$"));
    std::string output = testing::internal::GetCapturedStdout();
    EXPECT_EQ(output, "Name HELLO\n");
}

TEST_F(InputEndStopTest, ExecuteInputNumber) {
    hal_display_set_mock_input("999");
    testing::internal::CaptureStdout();
    parse_and_execute(lex("INPUT B"));
    parse_and_execute(lex("PRINT B"));
    std::string output = testing::internal::GetCapturedStdout();
    EXPECT_EQ(output, "? 999\n");
}
