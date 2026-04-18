#include "lexer.h"
#include <cctype>
#include <cstring>
#include <cstdio>
#include <stdexcept>

TokenList lex(const char* source) {
    TokenList token_list;
    int pos = 0;
    int len = strlen(source);

    while (pos < len) {
        if (token_list.size >= MAX_TOKENS_PER_LINE) break;

        char c = source[pos];

        if (std::isspace(c)) {
            pos++;
            continue;
        }

        if (c == '<' && pos + 1 < len && source[pos+1] == '>') {
            token_list.tokens[token_list.size].type = TokenType::NEQ;
            strcpy(token_list.tokens[token_list.size].text, "<>");
            token_list.size++;
            pos += 2;
            continue;
        }
        if (c == '<' && pos + 1 < len && source[pos+1] == '=') {
            token_list.tokens[token_list.size].type = TokenType::LTE;
            strcpy(token_list.tokens[token_list.size].text, "<=");
            token_list.size++;
            pos += 2;
            continue;
        }
        if (c == '>' && pos + 1 < len && source[pos+1] == '=') {
            token_list.tokens[token_list.size].type = TokenType::GTE;
            strcpy(token_list.tokens[token_list.size].text, ">=");
            token_list.size++;
            pos += 2;
            continue;
        }

        if (c == '=') { token_list.tokens[token_list.size] = {TokenType::ASSIGN, "="}; token_list.size++; pos++; continue; }
        if (c == '+') { token_list.tokens[token_list.size] = {TokenType::PLUS, "+"}; token_list.size++; pos++; continue; }
        if (c == '-') { token_list.tokens[token_list.size] = {TokenType::MINUS, "-"}; token_list.size++; pos++; continue; }
        if (c == '*') { token_list.tokens[token_list.size] = {TokenType::MUL, "*"}; token_list.size++; pos++; continue; }
        if (c == '/') { token_list.tokens[token_list.size] = {TokenType::DIV, "/"}; token_list.size++; pos++; continue; }
        if (c == '^') { token_list.tokens[token_list.size] = {TokenType::POWER, "^"}; token_list.size++; pos++; continue; }
        if (c == '(') { token_list.tokens[token_list.size] = {TokenType::LPAREN, "("}; token_list.size++; pos++; continue; }
        if (c == ')') { token_list.tokens[token_list.size] = {TokenType::RPAREN, ")"}; token_list.size++; pos++; continue; }
        if (c == '>') { token_list.tokens[token_list.size] = {TokenType::GT, ">"}; token_list.size++; pos++; continue; }
        if (c == '<') { token_list.tokens[token_list.size] = {TokenType::LT, "<"}; token_list.size++; pos++; continue; }
        if (c == ',') { token_list.tokens[token_list.size] = {TokenType::COMMA, ","}; token_list.size++; pos++; continue; }

        if (c == '"') {
            pos++;
            int start = pos;
            while (pos < len && source[pos] != '"') pos++;
            
            Token t;
            t.type = TokenType::STRING;
            int text_len = pos - start;
            if (text_len >= MAX_TOKEN_LEN) text_len = MAX_TOKEN_LEN - 1;
            strncpy(t.text, source + start, text_len);
            t.text[text_len] = '\0';
            token_list.tokens[token_list.size++] = t;
            
            if (pos < len && source[pos] == '"') pos++;
            continue;
        }

        if (std::isdigit(c)) {
            int start = pos;
            bool has_dot = false;
            while (pos < len && (std::isdigit(source[pos]) || source[pos] == '.')) {
                if (source[pos] == '.') {
                    if (has_dot) break;
                    has_dot = true;
                }
                pos++;
            }
            Token t;
            t.type = TokenType::NUMBER;
            int text_len = pos - start;
            if (text_len >= MAX_TOKEN_LEN) text_len = MAX_TOKEN_LEN - 1;
            strncpy(t.text, source + start, text_len);
            t.text[text_len] = '\0';
            token_list.tokens[token_list.size++] = t;
            continue;
        }

        if (std::isalpha(c)) {
            int start = pos;
            while (pos < len && (std::isalnum(source[pos]) || source[pos] == '$' || source[pos] == '%')) {
                pos++;
            }
            char ident[MAX_TOKEN_LEN];
            int text_len = pos - start;
            if (text_len >= MAX_TOKEN_LEN) text_len = MAX_TOKEN_LEN - 1;
            strncpy(ident, source + start, text_len);
            ident[text_len] = '\0';

            // Convert to uppercase for keywords
            char upper_ident[MAX_TOKEN_LEN];
            for (int i = 0; i <= text_len; ++i) upper_ident[i] = std::toupper(ident[i]);

            Token t;
            t.type = TokenType::IDENTIFIER;
            strncpy(t.text, upper_ident, MAX_TOKEN_LEN);

            if (strcmp(upper_ident, "PRINT") == 0) t.type = TokenType::PRINT;
            else if (strcmp(upper_ident, "LET") == 0) t.type = TokenType::LET;
            else if (strcmp(upper_ident, "GOTO") == 0) t.type = TokenType::GOTO;
            else if (strcmp(upper_ident, "GOSUB") == 0) t.type = TokenType::GOSUB;
            else if (strcmp(upper_ident, "RETURN") == 0) t.type = TokenType::RETURN;
            else if (strcmp(upper_ident, "IF") == 0) t.type = TokenType::IF;
            else if (strcmp(upper_ident, "THEN") == 0) t.type = TokenType::THEN;
            else if (strcmp(upper_ident, "ELSE") == 0) t.type = TokenType::ELSE;
            else if (strcmp(upper_ident, "FOR") == 0) t.type = TokenType::FOR;
            else if (strcmp(upper_ident, "TO") == 0) t.type = TokenType::TO;
            else if (strcmp(upper_ident, "STEP") == 0) t.type = TokenType::STEP;
            else if (strcmp(upper_ident, "NEXT") == 0) t.type = TokenType::NEXT;
            else if (strcmp(upper_ident, "NEW") == 0) t.type = TokenType::NEW;
            else if (strcmp(upper_ident, "LIST") == 0) t.type = TokenType::LIST;
            else if (strcmp(upper_ident, "RUN") == 0) t.type = TokenType::RUN;
            else if (strcmp(upper_ident, "READ") == 0) t.type = TokenType::READ;
            else if (strcmp(upper_ident, "DATA") == 0) t.type = TokenType::DATA;
            else if (strcmp(upper_ident, "RESTORE") == 0) t.type = TokenType::RESTORE;
            else if (strcmp(upper_ident, "DIM") == 0) t.type = TokenType::DIM;
            else if (strcmp(upper_ident, "INPUT") == 0) t.type = TokenType::INPUT;
            else if (strcmp(upper_ident, "END") == 0) t.type = TokenType::END;
            else if (strcmp(upper_ident, "STOP") == 0) t.type = TokenType::STOP;
            else if (strcmp(upper_ident, "ABS") == 0 || strcmp(upper_ident, "INT") == 0 || strcmp(upper_ident, "RND") == 0 ||
                     strcmp(upper_ident, "LEN") == 0 || strcmp(upper_ident, "MID$") == 0 || strcmp(upper_ident, "LEFT$") == 0) {} // Built-in functions bypass variable checks
            else {
                // Not a keyword. Check strict variable name rules: [A-Z][0-9]?[$%]?
                bool valid = true;
                if (text_len > 3) valid = false;
                else if (text_len == 1) { 
                    if (!std::isalpha(upper_ident[0])) valid = false;
                }
                else if (text_len == 2) {
                    if (!std::isalpha(upper_ident[0])) valid = false;
                    else if (!std::isdigit(upper_ident[1]) && upper_ident[1] != '$' && upper_ident[1] != '%') valid = false;
                }
                else if (text_len == 3) {
                    if (!std::isalpha(upper_ident[0])) valid = false;
                    else if (!std::isdigit(upper_ident[1])) valid = false;
                    else if (upper_ident[2] != '$' && upper_ident[2] != '%') valid = false;
                }
                
                if (!valid) {
                    throw std::runtime_error("Syntax Error: Invalid variable name");
                }
            }

            token_list.tokens[token_list.size++] = t;
            continue;
        }

        // Unknown character, skip it
        pos++;
    }

    if (token_list.size < MAX_TOKENS_PER_LINE) {
        token_list.tokens[token_list.size++] = {TokenType::END_OF_FILE, ""};
    } else {
        token_list.tokens[MAX_TOKENS_PER_LINE - 1] = {TokenType::END_OF_FILE, ""};
    }

    return token_list;
}
