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

TEST_F(FileOpsTest, FilesCommand) {
    // Just verify it runs and prints something
    parse_and_execute(lex("FILES"));
    std::string out = mock_hal::get_raw_print_buffer();
    EXPECT_TRUE(out.find("File(s) found") != std::string::npos);
}

TEST_F(FileOpsTest, KillCommand) {
    // 1. Create a dummy file
    {
        std::ofstream fs("kill_test.tmp");
        fs << "test";
    }
    
    // 2. Kill it
    parse_and_execute(lex("KILL \"kill_test.tmp\""));
    
    // 3. Verify it's gone
    std::ifstream fs("kill_test.tmp");
    EXPECT_FALSE(fs.good());
    EXPECT_TRUE(mock_hal::get_raw_print_buffer().find("Deleted") != std::string::npos);
}

TEST_F(FileOpsTest, NameAsCommand) {
    // 1. Create a dummy file
    {
        std::ofstream fs("name_test_old.tmp");
        fs << "test";
    }
    std::remove("name_test_new.tmp");
    
    // 2. Rename it
    parse_and_execute(lex("NAME \"name_test_old.tmp\" AS \"name_test_new.tmp\""));
    
    // 3. Verify
    std::ifstream fs_old("name_test_old.tmp");
    EXPECT_FALSE(fs_old.good());
    std::ifstream fs_new("name_test_new.tmp");
    EXPECT_TRUE(fs_new.good());
    
    std::remove("name_test_new.tmp");
}

