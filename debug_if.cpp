#include "parser.h"
#include "lexer.h"
#include <iostream>

int main() {
    auto tokens = lex("IF A = 1 THEN PRINT 111 ELSE PRINT 222");
    for (auto t : tokens) std::cout << (int)t.type << ":" << t.text << " ";
    std::cout << "\n";
    return 0;
}
