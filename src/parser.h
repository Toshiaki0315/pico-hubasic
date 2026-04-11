#pragma once
#include "lexer.h"
#include <vector>
#include <string>

void parse_and_execute(const std::vector<Token>& tokens);
void store_line(int line_number, const std::vector<Token>& tokens);
void run_program(int max_steps = -1);
void clear_program();
void list_program();
