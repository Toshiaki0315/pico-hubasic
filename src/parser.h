#pragma once
#include "lexer.h"

bool parse_and_execute(const TokenList& tokens);
void store_line(int line_number, const TokenList& tokens);
void list_program();
void clear_program();
void run_program(int max_steps = -1);
