#include <gtest/gtest.h>
#include "parser.h"
#include "lexer.h"
#include "mock_hal_display.h"
#include <iostream>

class SoundLogicTest : public ::testing::Test {
protected:
    void SetUp() override {
        clear_program();
        mock_hal::reset();
    }
};

TEST_F(SoundLogicTest, BeepCommand) {
    EXPECT_NO_THROW(parse_and_execute(lex("BEEP")));
}

TEST_F(SoundLogicTest, SoundCommand) {
    EXPECT_NO_THROW(parse_and_execute(lex("SOUND 440, 500")));
    EXPECT_NO_THROW(parse_and_execute(lex("SOUND 880, 250")));
}

TEST_F(SoundLogicTest, MusicCommandBasic) {
    EXPECT_NO_THROW(parse_and_execute(lex("MUSIC \"CDE\"")));
    EXPECT_NO_THROW(parse_and_execute(lex("MUSIC \"O5 C D+ E- R4\"")));
}

TEST_F(SoundLogicTest, MusicCommandAdvanced) {
    // Check various MML parameters
    EXPECT_NO_THROW(parse_and_execute(lex("MUSIC \"T160 L8 V10 C D E F G A B > C < C\"")));
    EXPECT_NO_THROW(parse_and_execute(lex("MUSIC \"C. D8 E16\"")));
}

TEST_F(SoundLogicTest, MusicErrorHandling) {
    parse_and_execute(lex("MUSIC 123"));
    EXPECT_TRUE(mock_hal::get_raw_print_buffer().find("Type Mismatch") != std::string::npos);
}
