#pragma once

#define MAX_TOKEN_LEN 128
#define MAX_TOKENS_PER_LINE 128

enum class TokenType {
    NUMBER,
    IDENTIFIER,
    STRING,
    PRINT,
    LET,
    ASSIGN,
    PLUS,
    MINUS,
    MUL,
    DIV,
    POWER,
    LPAREN,
    RPAREN,
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
    DIM,
    INPUT,
    END,
    STOP,
    INIT, CLEAR, NEWON, WIDTH, CONSOLE, CLS, REPEAT, UNTIL,
    GET, FILES, GPIO,
    WINDOW, PSET, LINE, CIRCLE, POLY, PAINT, GET_AT, PUT_AT, COLOR,
    BEEP, MUSIC, SOUND,
    GT,
    LT,
    GTE,
    LTE,
    NEQ,
    COMMA,
    END_OF_FILE
};

struct Token {
    TokenType type;
    char text[MAX_TOKEN_LEN];
};

struct TokenList {
    Token tokens[MAX_TOKENS_PER_LINE];
    int size;

    TokenList() : size(0) {}
};

TokenList lex(const char* source);
