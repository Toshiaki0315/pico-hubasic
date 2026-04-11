#pragma once
#include <string>
#include <vector>

enum class TokenType {
    NUMBER,
    IDENTIFIER, // Variables
    STRING,
    
    // Keywords
    PRINT,
    DIM,
    LET,
    GOTO,
    GOSUB,
    RETURN,
    IF,
    THEN,
    ELSE,
    FOR,
    TO,
    STEP,
    NEXT,
    NEW,
    LIST,
    RUN,
    READ,
    DATA,
    RESTORE,
    
    // Operators
    PLUS,       // +
    MINUS,      // -
    MUL,        // *
    DIV,        // /
    ASSIGN,     // = (Also used for equals)
    LPAREN,     // (
    RPAREN,     // )
    
    // Relational
    GT,         // >
    LT,         // <
    GTE,        // >=
    LTE,        // <=
    NEQ,        // <>
    COMMA,      // ,
    
    END_OF_FILE
};

struct Token {
    TokenType type;
    std::string text;
};

std::vector<Token> lex(const std::string& source);
