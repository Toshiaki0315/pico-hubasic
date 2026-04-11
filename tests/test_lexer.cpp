#include <gtest/gtest.h>
#include "lexer.h"

class LexerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup code here if needed
    }
};

TEST_F(LexerTest, ParseNumber) {
    auto tokens = lex("   123.45  ");
    
    // We expect NUMBER then END_OF_FILE
    ASSERT_EQ(tokens.size(), 2);
    EXPECT_EQ(tokens[0].type, TokenType::NUMBER);
    EXPECT_EQ(tokens[0].text, "123.45");
    EXPECT_EQ(tokens[1].type, TokenType::END_OF_FILE);
}

TEST_F(LexerTest, ParsePrintStatement) {
    auto tokens = lex("PRINT A");
    
    ASSERT_GE(tokens.size(), 3); // PRINT, A, EOF
    EXPECT_EQ(tokens[0].type, TokenType::PRINT);
    EXPECT_EQ(tokens[0].text, "PRINT");

    EXPECT_EQ(tokens[1].type, TokenType::IDENTIFIER);
    EXPECT_EQ(tokens[1].text, "A");
    
    EXPECT_EQ(tokens[2].type, TokenType::END_OF_FILE);
}

TEST_F(LexerTest, ParseLetAssignment) {
    // Both implicit "A = 10" and explicit "LET A = 10"
    auto tokens1 = lex("LET A = 10");
    ASSERT_GE(tokens1.size(), 5);
    EXPECT_EQ(tokens1[0].type, TokenType::LET);
    EXPECT_EQ(tokens1[1].type, TokenType::IDENTIFIER);
    EXPECT_EQ(tokens1[2].type, TokenType::ASSIGN);
    EXPECT_EQ(tokens1[3].type, TokenType::NUMBER);

    auto tokens2 = lex("B=20");
    ASSERT_GE(tokens2.size(), 4);
    EXPECT_EQ(tokens2[0].type, TokenType::IDENTIFIER);
    EXPECT_EQ(tokens2[1].type, TokenType::ASSIGN);
    EXPECT_EQ(tokens2[2].type, TokenType::NUMBER);
}

TEST_F(LexerTest, ParseMathOperators) {
    auto tokens = lex("1 + 2 * 3 / 4 - 5");
    
    std::vector<TokenType> expected_types = {
        TokenType::NUMBER, TokenType::PLUS, TokenType::NUMBER, 
        TokenType::MUL, TokenType::NUMBER, TokenType::DIV, 
        TokenType::NUMBER, TokenType::MINUS, TokenType::NUMBER,
        TokenType::END_OF_FILE
    };

    ASSERT_EQ(tokens.size(), expected_types.size());
    for (size_t i = 0; i < expected_types.size(); ++i) {
        EXPECT_EQ(tokens[i].type, expected_types[i]) << "Mismatch at index " << i;
    }
}
