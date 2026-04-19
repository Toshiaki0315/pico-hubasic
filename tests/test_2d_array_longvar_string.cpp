// test_2d_array_longvar_string.cpp
// Tests for: 2D arrays, 8-char variable names, string heap in-place update
#include <gtest/gtest.h>
#include "parser.h"
#include "mock_hal_display.h"

class AdvancedFeaturesNewTest : public ::testing::Test {
protected:
    void SetUp() override {
        clear_program();
        mock_hal::reset();
    }
};

// =============================================================
// Proposal 3: 2D Array support
// =============================================================

TEST_F(AdvancedFeaturesNewTest, TwoDimBasicWrite) {
    store_line(10, lex("DIM M(2,3)"));
    store_line(20, lex("M(1,2) = 42"));
    store_line(30, lex("PRINT M(1,2)"));
    parse_and_execute(lex("RUN"));
    EXPECT_EQ(mock_hal::get_raw_print_buffer(), "42\n");
}

TEST_F(AdvancedFeaturesNewTest, TwoDimMultipleElements) {
    store_line(10, lex("DIM M(2,2)"));
    store_line(20, lex("M(0,0) = 1"));
    store_line(30, lex("M(0,1) = 2"));
    store_line(40, lex("M(1,0) = 3"));
    store_line(50, lex("M(1,1) = 4"));
    store_line(60, lex("PRINT M(0,0)"));
    store_line(70, lex("PRINT M(0,1)"));
    store_line(80, lex("PRINT M(1,0)"));
    store_line(90, lex("PRINT M(1,1)"));
    parse_and_execute(lex("RUN"));
    EXPECT_EQ(mock_hal::get_raw_print_buffer(), "1\n2\n3\n4\n");
}

TEST_F(AdvancedFeaturesNewTest, TwoDimDefaultZero) {
    store_line(10, lex("DIM A(3,3)"));
    store_line(20, lex("PRINT A(2,2)"));
    parse_and_execute(lex("RUN"));
    EXPECT_EQ(mock_hal::get_raw_print_buffer(), "0\n");
}

TEST_F(AdvancedFeaturesNewTest, TwoDimInLoop) {
    // Fill diagonal of 3x3 matrix using a loop
    store_line(10, lex("DIM M(2,2)"));
    store_line(20, lex("FOR I = 0 TO 2"));
    store_line(30, lex("M(I,I) = I + 1"));
    store_line(40, lex("NEXT I"));
    store_line(50, lex("PRINT M(0,0)"));
    store_line(60, lex("PRINT M(1,1)"));
    store_line(70, lex("PRINT M(2,2)"));
    store_line(80, lex("PRINT M(0,1)"));  // off-diagonal = 0
    parse_and_execute(lex("RUN"));
    EXPECT_EQ(mock_hal::get_raw_print_buffer(), "1\n2\n3\n0\n");
}

TEST_F(AdvancedFeaturesNewTest, TwoDimLongName) {
    store_line(10, lex("DIM BOARD(4,4)"));
    store_line(20, lex("BOARD(2,3) = 99"));
    store_line(30, lex("PRINT BOARD(2,3)"));
    store_line(40, lex("PRINT BOARD(0,0)"));
    parse_and_execute(lex("RUN"));
    EXPECT_EQ(mock_hal::get_raw_print_buffer(), "99\n0\n");
}

TEST_F(AdvancedFeaturesNewTest, TwoDimOutOfBoundsRow) {
    parse_and_execute(lex("DIM A(2,2)"));
    mock_hal::reset();
    parse_and_execute(lex("PRINT A(3,0)"));
    auto buf = mock_hal::get_raw_print_buffer();
    EXPECT_TRUE(buf.find("out of bounds") != std::string::npos ||
                buf.find("Out of bounds") != std::string::npos);
}

TEST_F(AdvancedFeaturesNewTest, TwoDimOutOfBoundsCol) {
    parse_and_execute(lex("DIM A(2,2)"));
    mock_hal::reset();
    parse_and_execute(lex("PRINT A(0,5)"));
    auto buf = mock_hal::get_raw_print_buffer();
    EXPECT_TRUE(buf.find("out of bounds") != std::string::npos ||
                buf.find("Out of bounds") != std::string::npos);
}

TEST_F(AdvancedFeaturesNewTest, TwoDimWrongDimCount) {
    parse_and_execute(lex("DIM A(2,2)"));
    mock_hal::reset();
    // 2D array accessed with 1 index should throw
    parse_and_execute(lex("PRINT A(1)"));
    auto buf = mock_hal::get_raw_print_buffer();
    EXPECT_TRUE(buf.find("Syntax Error") != std::string::npos ||
                buf.find("2D array") != std::string::npos);
}

TEST_F(AdvancedFeaturesNewTest, OneDimNotAffectedByTwoDimChange) {
    // Existing 1D arrays still work correctly
    store_line(10, lex("DIM V(5)"));
    store_line(20, lex("V(3) = 77"));
    store_line(30, lex("PRINT V(3)"));
    parse_and_execute(lex("RUN"));
    EXPECT_EQ(mock_hal::get_raw_print_buffer(), "77\n");
}

// =============================================================
// Proposal 4: 8-char variable names
// =============================================================

TEST_F(AdvancedFeaturesNewTest, LongNumericVar) {
    store_line(10, lex("SCORE = 100"));
    store_line(20, lex("PRINT SCORE"));
    parse_and_execute(lex("RUN"));
    EXPECT_EQ(mock_hal::get_raw_print_buffer(), "100\n");
}

TEST_F(AdvancedFeaturesNewTest, LongStringVar) {
    store_line(10, lex("NAME$ = \"ALICE\""));
    store_line(20, lex("PRINT NAME$"));
    parse_and_execute(lex("RUN"));
    EXPECT_EQ(mock_hal::get_raw_print_buffer(), "ALICE\n");
}

TEST_F(AdvancedFeaturesNewTest, LongIntVar) {
    store_line(10, lex("COUNT% = 5"));
    store_line(20, lex("PRINT COUNT%"));
    parse_and_execute(lex("RUN"));
    EXPECT_EQ(mock_hal::get_raw_print_buffer(), "5\n");
}

TEST_F(AdvancedFeaturesNewTest, LongVarNoCollision) {
    // SCORE and SCORES must be distinct variables
    store_line(10, lex("SCORE = 10"));
    store_line(20, lex("SCORES = 20"));
    store_line(30, lex("PRINT SCORE"));
    store_line(40, lex("PRINT SCORES"));
    parse_and_execute(lex("RUN"));
    EXPECT_EQ(mock_hal::get_raw_print_buffer(), "10\n20\n");
}

TEST_F(AdvancedFeaturesNewTest, LongVarArithmetic) {
    store_line(10, lex("LEVEL = 3"));
    store_line(20, lex("BONUS = LEVEL * 100"));
    store_line(30, lex("PRINT BONUS"));
    parse_and_execute(lex("RUN"));
    EXPECT_EQ(mock_hal::get_raw_print_buffer(), "300\n");
}

TEST_F(AdvancedFeaturesNewTest, LongVarInLoop) {
    store_line(10, lex("TOTAL = 0"));
    store_line(20, lex("FOR STEP = 1 TO 5"));
    store_line(30, lex("TOTAL = TOTAL + STEP"));
    store_line(40, lex("NEXT STEP"));
    store_line(50, lex("PRINT TOTAL"));
    parse_and_execute(lex("RUN"));
    EXPECT_EQ(mock_hal::get_raw_print_buffer(), "15\n");
}

TEST_F(AdvancedFeaturesNewTest, MaxLengthVarName) {
    // 8-char variable name (ABCDEFGH)
    store_line(10, lex("ABCDEFGH = 42"));
    store_line(20, lex("PRINT ABCDEFGH"));
    parse_and_execute(lex("RUN"));
    EXPECT_EQ(mock_hal::get_raw_print_buffer(), "42\n");
}

// =============================================================
// Proposal 5: String heap in-place update (no fragmentation)
// =============================================================

TEST_F(AdvancedFeaturesNewTest, StringReassignShorter) {
    // Shorter string should update in-place, leaving heap_ptr unchanged
    store_line(10, lex("A$ = \"HELLO\""));
    store_line(20, lex("A$ = \"HI\""));
    store_line(30, lex("PRINT A$"));
    parse_and_execute(lex("RUN"));
    EXPECT_EQ(mock_hal::get_raw_print_buffer(), "HI\n");
}

TEST_F(AdvancedFeaturesNewTest, StringReassignSameLength) {
    store_line(10, lex("X$ = \"ABCD\""));
    store_line(20, lex("X$ = \"WXYZ\""));
    store_line(30, lex("PRINT X$"));
    parse_and_execute(lex("RUN"));
    EXPECT_EQ(mock_hal::get_raw_print_buffer(), "WXYZ\n");
}

TEST_F(AdvancedFeaturesNewTest, StringReassignLonger) {
    // Longer string must still work (new allocation)
    store_line(10, lex("S$ = \"HI\""));
    store_line(20, lex("S$ = \"HELLO WORLD\""));
    store_line(30, lex("PRINT S$"));
    parse_and_execute(lex("RUN"));
    EXPECT_EQ(mock_hal::get_raw_print_buffer(), "HELLO WORLD\n");
}

TEST_F(AdvancedFeaturesNewTest, StringHeapLoopNoOverflow) {
    // 200 iterations reassigning the same string variable should not cause heap overflow
    store_line(10, lex("FOR I = 1 TO 200"));
    store_line(20, lex("MSG$ = \"LOOP\""));
    store_line(30, lex("NEXT I"));
    store_line(40, lex("PRINT MSG$"));
    parse_and_execute(lex("RUN"));
    EXPECT_EQ(mock_hal::get_raw_print_buffer(), "LOOP\n");
}

TEST_F(AdvancedFeaturesNewTest, MultipleStringVarsIndependent) {
    // Multiple distinct string variables each keep their own values
    store_line(10, lex("FIRST$ = \"AAA\""));
    store_line(20, lex("SECOND$ = \"BBB\""));
    store_line(30, lex("FIRST$ = \"CCC\""));
    store_line(40, lex("PRINT FIRST$"));
    store_line(50, lex("PRINT SECOND$"));
    parse_and_execute(lex("RUN"));
    EXPECT_EQ(mock_hal::get_raw_print_buffer(), "CCC\nBBB\n");
}

TEST_F(AdvancedFeaturesNewTest, StringArrayInPlace) {
    // String array elements also benefit from in-place update
    store_line(10, lex("DIM S$(3)"));
    store_line(20, lex("S$(0) = \"HELLO\""));
    store_line(30, lex("S$(0) = \"HI\""));
    store_line(40, lex("PRINT S$(0)"));
    parse_and_execute(lex("RUN"));
    EXPECT_EQ(mock_hal::get_raw_print_buffer(), "HI\n");
}
