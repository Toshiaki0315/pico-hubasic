#include "lexer.h"
#include <cctype>

std::vector<Token> lex(const std::string& source) {
    std::vector<Token> tokens;
    size_t i = 0;

    while (i < source.length()) {
        char c = source[i];

        if (std::isspace(c)) {
            i++;
            continue;
        }

        // Handle double-quote strings
        if (c == '"') {
            std::string str_val = "";
            i++; // skip opening quote
            while (i < source.length() && source[i] != '"') {
                str_val += source[i];
                i++;
            }
            if (i < source.length()) {
                i++; // skip closing quote
            }
            tokens.push_back({TokenType::STRING, str_val});
            continue;
        }

        if (std::isalpha(c)) {
            std::string ident = "";
            while (i < source.length() && std::isalnum(source[i])) {
                ident += std::toupper(source[i]);
                i++;
            }
            // Optional $ suffix for string variables
            if (i < source.length() && source[i] == '$') {
                ident += '$';
                i++;
            }
            
            if (ident == "PRINT") { tokens.push_back({TokenType::PRINT, ident}); }
            else if (ident == "DIM") { tokens.push_back({TokenType::DIM, ident}); }
            else if (ident == "LET") { tokens.push_back({TokenType::LET, ident}); }
            else if (ident == "GOTO") { tokens.push_back({TokenType::GOTO, ident}); }
            else if (ident == "GOSUB") { tokens.push_back({TokenType::GOSUB, ident}); }
            else if (ident == "RETURN") { tokens.push_back({TokenType::RETURN, ident}); }
            else if (ident == "IF") { tokens.push_back({TokenType::IF, ident}); }
            else if (ident == "THEN") { tokens.push_back({TokenType::THEN, ident}); }
            else if (ident == "ELSE") { tokens.push_back({TokenType::ELSE, ident}); }
            else if (ident == "FOR") { tokens.push_back({TokenType::FOR, ident}); }
            else if (ident == "TO") { tokens.push_back({TokenType::TO, ident}); }
            else if (ident == "STEP") { tokens.push_back({TokenType::STEP, ident}); }
            else if (ident == "NEXT") { tokens.push_back({TokenType::NEXT, ident}); }
            else if (ident == "NEW") { tokens.push_back({TokenType::NEW, ident}); }
            else if (ident == "LIST") { tokens.push_back({TokenType::LIST, ident}); }
            else if (ident == "RUN") { tokens.push_back({TokenType::RUN, ident}); }
            else if (ident == "READ") { tokens.push_back({TokenType::READ, ident}); }
            else if (ident == "DATA") { tokens.push_back({TokenType::DATA, ident}); }
            else if (ident == "RESTORE") { tokens.push_back({TokenType::RESTORE, ident}); }
            else { tokens.push_back({TokenType::IDENTIFIER, ident}); }
            continue;
        }

        if (std::isdigit(c) || c == '.') {
            std::string num = "";
            while (i < source.length() && (std::isdigit(source[i]) || source[i] == '.')) {
                num += source[i];
                i++;
            }
            tokens.push_back({TokenType::NUMBER, num});
            continue;
        }

        // Multi-char relational operators
        if (c == '<' && i + 1 < source.length()) {
            if (source[i+1] == '=') { tokens.push_back({TokenType::LTE, "<="}); i+=2; continue; }
            if (source[i+1] == '>') { tokens.push_back({TokenType::NEQ, "<>"}); i+=2; continue; }
        }
        if (c == '>' && i + 1 < source.length() && source[i+1] == '=') {
            tokens.push_back({TokenType::GTE, ">="}); i+=2; continue;
        }

        // Single char operators
        if (c == '>') { tokens.push_back({TokenType::GT, ">"}); i++; continue; }
        if (c == '<') { tokens.push_back({TokenType::LT, "<"}); i++; continue; }

        if (c == '+') { tokens.push_back({TokenType::PLUS, "+"}); i++; continue; }
        if (c == '-') { tokens.push_back({TokenType::MINUS, "-"}); i++; continue; }
        if (c == '*') { tokens.push_back({TokenType::MUL, "*"}); i++; continue; }
        if (c == '/') { tokens.push_back({TokenType::DIV, "/"}); i++; continue; }
        if (c == '=') { tokens.push_back({TokenType::ASSIGN, "="}); i++; continue; }
        if (c == '(') { tokens.push_back({TokenType::LPAREN, "("}); i++; continue; }
        if (c == ')') { tokens.push_back({TokenType::RPAREN, ")"}); i++; continue; }
        if (c == ',') { tokens.push_back({TokenType::COMMA, ","}); i++; continue; }
        
        // Skip unknown characters
        i++;
    }
    
    tokens.push_back({TokenType::END_OF_FILE, ""});
    return tokens;
}
