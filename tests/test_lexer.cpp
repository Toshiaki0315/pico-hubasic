#include <gtest/gtest.h>
#include "lexer.h"
#include <cstring>

class LexerTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(LexerTest, ParseNumber) {
    TokenList tokens = lex("123.45");
    ASSERT_GE(tokens.size, 2); // NUMBER, EOF
    EXPECT_EQ(tokens.tokens[0].type, TokenType::NUMBER);
    EXPECT_STREQ(tokens.tokens[0].text, "123.45");
    EXPECT_EQ(tokens.tokens[1].type, TokenType::END_OF_FILE);
}

TEST_F(LexerTest, ParsePrintStatement) {
    TokenList tokens = lex("PRINT A");
    ASSERT_GE(tokens.size, 3); // PRINT, A, EOF
    EXPECT_EQ(tokens.tokens[0].type, TokenType::PRINT);
    EXPECT_STREQ(tokens.tokens[0].text, "PRINT");
    
    EXPECT_EQ(tokens.tokens[1].type, TokenType::IDENTIFIER);
    EXPECT_STREQ(tokens.tokens[1].text, "A");
    
    EXPECT_EQ(tokens.tokens[2].type, TokenType::END_OF_FILE);
}

TEST_F(LexerTest, ParseLetAssignment) {
    TokenList tokens1 = lex("LET A = 10");
    ASSERT_GE(tokens1.size, 5);
    EXPECT_EQ(tokens1.tokens[0].type, TokenType::LET);
    EXPECT_EQ(tokens1.tokens[1].type, TokenType::IDENTIFIER);
    EXPECT_EQ(tokens1.tokens[2].type, TokenType::ASSIGN);
    EXPECT_EQ(tokens1.tokens[3].type, TokenType::NUMBER);

    TokenList tokens2 = lex("A = 10");
    ASSERT_GE(tokens2.size, 4);
    EXPECT_EQ(tokens2.tokens[0].type, TokenType::IDENTIFIER);
    EXPECT_EQ(tokens2.tokens[1].type, TokenType::ASSIGN);
    EXPECT_EQ(tokens2.tokens[2].type, TokenType::NUMBER);
}

TEST_F(LexerTest, ParseMathOperators) {
    TokenList tokens = lex("1 + 2 * ( 3 - 4 ) / 5");
    // Expected: NUM, PLUS, NUM, MUL, LPAREN, NUM, MINUS, NUM, RPAREN, DIV, NUM, EOF (12)
    ASSERT_GE(tokens.size, 12);
    EXPECT_EQ(tokens.tokens[0].type, TokenType::NUMBER);
    EXPECT_EQ(tokens.tokens[1].type, TokenType::PLUS);
    EXPECT_EQ(tokens.tokens[2].type, TokenType::NUMBER);
    EXPECT_EQ(tokens.tokens[3].type, TokenType::MUL);
    EXPECT_EQ(tokens.tokens[4].type, TokenType::LPAREN);
    EXPECT_EQ(tokens.tokens[5].type, TokenType::NUMBER);
    EXPECT_EQ(tokens.tokens[6].type, TokenType::MINUS);
    EXPECT_EQ(tokens.tokens[7].type, TokenType::NUMBER);
    EXPECT_EQ(tokens.tokens[8].type, TokenType::RPAREN);
    EXPECT_EQ(tokens.tokens[9].type, TokenType::DIV);
    EXPECT_EQ(tokens.tokens[10].type, TokenType::NUMBER);
    EXPECT_EQ(tokens.tokens[11].type, TokenType::END_OF_FILE);
}
