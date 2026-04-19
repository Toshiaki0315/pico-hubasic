// test_touch_play.cpp – Unit tests for TOUCH() function and PLAY command
#include <gtest/gtest.h>
#include "parser.h"
#include "mock_hal_display.h"

class TouchPlayTest : public ::testing::Test {
protected:
    void SetUp() override {
        clear_program();
        mock_hal::reset();
    }
};

// ---------------------------------------------------------
// TOUCH() function tests
// ---------------------------------------------------------

TEST_F(TouchPlayTest, TouchNotTouched) {
    // Default state: no touch
    mock_hal::set_touch_state(0, 0, 0);
    store_line(10, lex("PRINT TOUCH(2)"));
    parse_and_execute(lex("RUN"));
    EXPECT_EQ(mock_hal::get_raw_print_buffer(), "0\n");
}

TEST_F(TouchPlayTest, TouchStateActive) {
    // Simulate a touch event at (120, 80)
    mock_hal::set_touch_state(1, 120, 80);
    store_line(10, lex("PRINT TOUCH(2)"));
    parse_and_execute(lex("RUN"));
    EXPECT_EQ(mock_hal::get_raw_print_buffer(), "1\n");
}

TEST_F(TouchPlayTest, TouchCoordinatesX) {
    mock_hal::set_touch_state(1, 120, 80);
    store_line(10, lex("PRINT TOUCH(0)"));
    parse_and_execute(lex("RUN"));
    EXPECT_EQ(mock_hal::get_raw_print_buffer(), "120\n");
}

TEST_F(TouchPlayTest, TouchCoordinatesY) {
    mock_hal::set_touch_state(1, 120, 80);
    store_line(10, lex("PRINT TOUCH(1)"));
    parse_and_execute(lex("RUN"));
    EXPECT_EQ(mock_hal::get_raw_print_buffer(), "80\n");
}

TEST_F(TouchPlayTest, TouchInProgramConditional) {
    // IF TOUCH(2)=1 THEN PRINT "HIT"
    mock_hal::set_touch_state(1, 50, 60);
    store_line(10, lex("IF TOUCH(2) = 1 THEN PRINT \"HIT\""));
    parse_and_execute(lex("RUN"));
    EXPECT_EQ(mock_hal::get_raw_print_buffer(), "HIT\n");
}

TEST_F(TouchPlayTest, TouchInProgramConditionalFalse) {
    mock_hal::set_touch_state(0, 0, 0);
    store_line(10, lex("IF TOUCH(2) = 1 THEN PRINT \"HIT\""));
    store_line(20, lex("PRINT \"MISS\""));
    parse_and_execute(lex("RUN"));
    EXPECT_EQ(mock_hal::get_raw_print_buffer(), "MISS\n");
}

TEST_F(TouchPlayTest, TouchInvalidArgument) {
    // TOUCH(3) should throw
    parse_and_execute(lex("PRINT TOUCH(3)"));
    EXPECT_TRUE(mock_hal::get_raw_print_buffer().find("TOUCH argument must be") != std::string::npos);
}

TEST_F(TouchPlayTest, TouchStoreInVariable) {
    mock_hal::set_touch_state(1, 200, 150);
    store_line(10, lex("X = TOUCH(0)"));
    store_line(20, lex("Y = TOUCH(1)"));
    store_line(30, lex("PRINT X"));
    store_line(40, lex("PRINT Y"));
    parse_and_execute(lex("RUN"));
    EXPECT_EQ(mock_hal::get_raw_print_buffer(), "200\n150\n");
}

// ---------------------------------------------------------
// PLAY command tests (Hu-BASIC alias for MUSIC / MML)
// ---------------------------------------------------------

TEST_F(TouchPlayTest, PlaySingleNote) {
    // PLAY "C" should execute without error and produce sound output
    parse_and_execute(lex("PLAY \"C\""));
    // Sound output goes to stdout via hal_sound_play, not to display buffer.
    // We just verify no error was reported on the display.
    EXPECT_TRUE(mock_hal::get_raw_print_buffer().find("Error") == std::string::npos);
    EXPECT_TRUE(mock_hal::get_raw_print_buffer().find("Syntax") == std::string::npos);
}

TEST_F(TouchPlayTest, PlayScale) {
    // PLAY "CDEFGAB" – full scale
    parse_and_execute(lex("PLAY \"CDEFGAB\""));
    EXPECT_TRUE(mock_hal::get_raw_print_buffer().find("Error") == std::string::npos);
}

TEST_F(TouchPlayTest, PlayWithOctaveAndTempo) {
    // PLAY "O5 T120 L8 CDEFG"
    parse_and_execute(lex("PLAY \"O5T120L8CDEFG\""));
    EXPECT_TRUE(mock_hal::get_raw_print_buffer().find("Error") == std::string::npos);
}

TEST_F(TouchPlayTest, PlayRest) {
    // PLAY "R" – rest note
    parse_and_execute(lex("PLAY \"R\""));
    EXPECT_TRUE(mock_hal::get_raw_print_buffer().find("Error") == std::string::npos);
}

TEST_F(TouchPlayTest, PlayTypeError) {
    // PLAY with number should raise a type mismatch
    parse_and_execute(lex("PLAY 123"));
    EXPECT_TRUE(mock_hal::get_raw_print_buffer().find("Type Mismatch") != std::string::npos);
}

TEST_F(TouchPlayTest, PlayAndMusicEquivalent) {
    // Both PLAY and MUSIC should parse and execute with the same MML string
    parse_and_execute(lex("PLAY \"C\""));
    std::string play_out = mock_hal::get_raw_print_buffer();

    mock_hal::reset();
    parse_and_execute(lex("MUSIC \"C\""));
    std::string music_out = mock_hal::get_raw_print_buffer();

    // Both should produce no display buffer error output
    EXPECT_TRUE(play_out.find("Error") == std::string::npos);
    EXPECT_TRUE(music_out.find("Error") == std::string::npos);
}

TEST_F(TouchPlayTest, PlayInProgram) {
    // PLAY inside a program line
    store_line(10, lex("PLAY \"O4CDE\""));
    store_line(20, lex("PRINT \"DONE\""));
    parse_and_execute(lex("RUN"));
    EXPECT_EQ(mock_hal::get_raw_print_buffer(), "DONE\n");
}
