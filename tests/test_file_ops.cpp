#include <gtest/gtest.h>
#include "parser.h"
#include "mock_hal_display.h"
#include <cstdio>
#include <fstream>

class FileOpsTest : public ::testing::Test {
protected:
    void SetUp() override {
        clear_program();
        mock_hal::reset();
        std::remove("test_save.bas");
    }
    void TearDown() override {
        std::remove("test_save.bas");
    }
};

TEST_F(FileOpsTest, SaveAndLoadProgram) {
    // 1. Create a program
    store_line(10, lex("A = 10"));
    store_line(20, lex("PRINT A * 2"));
    
    // 2. Save it
    parse_and_execute(lex("SAVE \"test_save.bas\""));
    mock_hal::reset();
    
    // 3. Clear program
    parse_and_execute(lex("NEW"));
    
    // 4. Load it
    parse_and_execute(lex("LOAD \"test_save.bas\""));
    mock_hal::reset();
    
    // 5. Run it and verify output
    parse_and_execute(lex("RUN"));
    EXPECT_EQ(mock_hal::get_raw_print_buffer(), "20\n");
}

TEST_F(FileOpsTest, LoadNonExistentFile) {
    parse_and_execute(lex("LOAD \"missing.bas\""));
    EXPECT_TRUE(mock_hal::get_raw_print_buffer().find("File Error") != std::string::npos);
}

