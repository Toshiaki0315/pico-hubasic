#include "parser.h"
#include "hal_display.h"
#include <stdio.h>
#include <map>
#include <stdexcept>
#include <iostream>

// ---------------------------------------------------------
// Data Representation
// ---------------------------------------------------------
struct Value {
    enum class Type { NUM, STR };
    Type type;
    float num_val;
    std::string str_val;

    Value() : type(Type::NUM), num_val(0.0f) {}
    Value(float n) : type(Type::NUM), num_val(n) {}
    Value(const std::string& s) : type(Type::STR), num_val(0.0f), str_val(s) {}

    std::string to_string() const {
        if (type == Type::STR) return str_val;
        char buf[32];
        snprintf(buf, sizeof(buf), "%g", num_val);
        return std::string(buf);
    }
};

// ---------------------------------------------------------
// Engine State
// ---------------------------------------------------------
static std::map<std::string, Value> variables;
static std::map<std::string, std::vector<Value>> arrays;
static std::map<int, std::vector<Token>> program_lines;
static int current_line = 0;
static bool branch_taken = false;

struct ForLoopContext {
    std::string var_name;
    float target;
    float step;
    int loop_start_line;
    size_t loop_start_pos;
};
static std::vector<ForLoopContext> for_stack;
static std::vector<int> call_stack;

// Data structures for READ/DATA
static std::vector<Value> data_buffer;
static int data_ptr = 0;

// ---------------------------------------------------------
// Recursive Descent Parser for Expressions
// ---------------------------------------------------------
static Value parse_expression(const std::vector<Token>& tokens, size_t& pos);
static Value parse_term(const std::vector<Token>& tokens, size_t& pos);
static Value parse_factor(const std::vector<Token>& tokens, size_t& pos);
static Value parse_relation(const std::vector<Token>& tokens, size_t& pos);

static Value parse_factor(const std::vector<Token>& tokens, size_t& pos) {
    if (pos >= tokens.size()) return Value(0.0f);
    Token t = tokens[pos];
    
    if (t.type == TokenType::PLUS) {
        pos++; 
        Value v = parse_factor(tokens, pos);
        if (v.type != Value::Type::NUM) throw std::runtime_error("Type Mismatch: Unary + on string");
        return v;
    }
    if (t.type == TokenType::MINUS) {
        pos++; 
        Value v = parse_factor(tokens, pos);
        if (v.type != Value::Type::NUM) throw std::runtime_error("Type Mismatch: Unary - on string");
        return Value(-v.num_val);
    }
    if (t.type == TokenType::NUMBER) {
        pos++; return Value(std::stof(t.text));
    }
    if (t.type == TokenType::STRING) {
        pos++; return Value(t.text);
    }
    if (t.type == TokenType::IDENTIFIER) {
        std::string var_name = t.text;
        pos++;
        
        // Array Access: A(idx)
        if (pos < tokens.size() && tokens[pos].type == TokenType::LPAREN) {
            pos++; // skip '('
            Value idx_val = parse_relation(tokens, pos);
            if (idx_val.type != Value::Type::NUM) throw std::runtime_error("Type Mismatch: Array index must be numeric");
            
            if (pos < tokens.size() && tokens[pos].type == TokenType::RPAREN) {
                pos++; // skip ')'
            } else {
                throw std::runtime_error("Syntax Error: Expected ')' for array access");
            }
            
            int idx = static_cast<int>(idx_val.num_val);
            if (arrays.find(var_name) == arrays.end()) {
                throw std::runtime_error("Array not dimensioned: " + var_name);
            }
            if (idx < 0 || idx >= arrays[var_name].size()) {
                throw std::runtime_error("Out of bounds: Array index " + std::to_string(idx) + " is invalid");
            }
            return arrays[var_name][idx];
        }
        
        // Scalar variable
        if (variables.find(var_name) == variables.end()) {
            if (!var_name.empty() && var_name.back() == '$') {
                return Value(std::string(""));
            } else {
                return Value(0.0f);
            }
        }
        return variables[var_name];
    }
    if (t.type == TokenType::LPAREN) {
        pos++; 
        Value val = parse_relation(tokens, pos);
        if (pos < tokens.size() && tokens[pos].type == TokenType::RPAREN) {
            pos++;
        } else {
            throw std::runtime_error("Missing closing parenthesis");
        }
        return val;
    }
    throw std::runtime_error("Syntax Error in expression");
}

static Value parse_term(const std::vector<Token>& tokens, size_t& pos) {
    Value val = parse_factor(tokens, pos);
    while (pos < tokens.size()) {
        TokenType op = tokens[pos].type;
        if (op != TokenType::MUL && op != TokenType::DIV) break;
        pos++;
        Value next_val = parse_factor(tokens, pos);
        
        if (val.type != Value::Type::NUM || next_val.type != Value::Type::NUM) {
            throw std::runtime_error("Type Mismatch: Cannot multiply/divide strings");
        }
        
        if (op == TokenType::MUL) {
            val.num_val *= next_val.num_val;
        } else {
            if (next_val.num_val == 0.0f) throw std::runtime_error("Division by zero");
            val.num_val /= next_val.num_val;
        }
    }
    return val;
}

static Value parse_expression(const std::vector<Token>& tokens, size_t& pos) {
    Value val = parse_term(tokens, pos);
    while (pos < tokens.size()) {
        TokenType op = tokens[pos].type;
        if (op != TokenType::PLUS && op != TokenType::MINUS) break;
        pos++;
        Value next_val = parse_term(tokens, pos);
        
        if (op == TokenType::PLUS) {
            if (val.type == Value::Type::STR && next_val.type == Value::Type::STR) {
                val.str_val += next_val.str_val;
            } else if (val.type == Value::Type::NUM && next_val.type == Value::Type::NUM) {
                val.num_val += next_val.num_val;
            } else {
                throw std::runtime_error("Type Mismatch: Cannot add string and number");
            }
        } else {
            if (val.type != Value::Type::NUM || next_val.type != Value::Type::NUM) {
                throw std::runtime_error("Type Mismatch: Cannot subtract strings");
            }
            val.num_val -= next_val.num_val;
        }
    }
    return val;
}

static Value parse_relation(const std::vector<Token>& tokens, size_t& pos) {
    Value val = parse_expression(tokens, pos);
    while (pos < tokens.size()) {
        TokenType op = tokens[pos].type;
        if (op == TokenType::ASSIGN || op == TokenType::GT || op == TokenType::LT || 
            op == TokenType::GTE || op == TokenType::LTE || op == TokenType::NEQ) {
            pos++;
            Value next_val = parse_expression(tokens, pos);
            
            if (val.type != next_val.type) {
                throw std::runtime_error("Type Mismatch: Cannot compare string and number");
            }
            
            bool result = false;
            if (val.type == Value::Type::NUM) {
                float a = val.num_val, b = next_val.num_val;
                if (op == TokenType::ASSIGN) result = (a == b);
                else if (op == TokenType::GT) result = (a > b);
                else if (op == TokenType::LT) result = (a < b);
                else if (op == TokenType::GTE) result = (a >= b);
                else if (op == TokenType::LTE) result = (a <= b);
                else if (op == TokenType::NEQ) result = (a != b);
            } else {
                std::string a = val.str_val, b = next_val.str_val;
                if (op == TokenType::ASSIGN) result = (a == b);
                else if (op == TokenType::GT) result = (a > b);
                else if (op == TokenType::LT) result = (a < b);
                else if (op == TokenType::GTE) result = (a >= b);
                else if (op == TokenType::LTE) result = (a <= b);
                else if (op == TokenType::NEQ) result = (a != b);
            }
            
            val = Value(result ? 1.0f : 0.0f);
        } else {
            break;
        }
    }
    return val;
}

// ---------------------------------------------------------
// Statement Execution Core
// ---------------------------------------------------------
static void execute_statement(const std::vector<Token>& tokens, size_t& pos) {
    if (pos >= tokens.size() || tokens[pos].type == TokenType::END_OF_FILE) return;

    if (tokens[pos].type == TokenType::PRINT) {
        pos++;
        std::string output = "";
        while (pos < tokens.size() && tokens[pos].type != TokenType::END_OF_FILE) {
            Value result = parse_relation(tokens, pos);
            output += result.to_string() + "\t"; // BASIC often uses tab or space for commas
            if (pos < tokens.size() && tokens[pos].type == TokenType::COMMA) {
                pos++;
            } else {
                break;
            }
        }
        // Remove trailing tab if not empty to make test matching easier
        if (!output.empty() && output.back() == '\t') output.pop_back();
        output += "\n";
        printf("%s", output.c_str());
        hal_display_print(output);
    } 
    else if (tokens[pos].type == TokenType::DATA) {
        // DATA is pre-processed before RUN, so skip it at runtime
        pos = tokens.size();
    }
    else if (tokens[pos].type == TokenType::RESTORE) {
        pos++;
        data_ptr = 0;
    }
    else if (tokens[pos].type == TokenType::READ) {
        pos++;
        while (pos < tokens.size() && tokens[pos].type != TokenType::END_OF_FILE) {
            std::string var_name;
            int arr_idx = -1;
            
            if (tokens[pos].type == TokenType::IDENTIFIER) {
                var_name = tokens[pos].text;
                pos++;
            } else {
                throw std::runtime_error("Syntax Error: READ expects identifier");
            }
            
            // Array indexing
            if (pos < tokens.size() && tokens[pos].type == TokenType::LPAREN) {
                pos++;
                Value idx_val = parse_relation(tokens, pos);
                if (idx_val.type != Value::Type::NUM) throw std::runtime_error("Type Mismatch: Array index");
                if (pos < tokens.size() && tokens[pos].type == TokenType::RPAREN) {
                    pos++;
                    arr_idx = static_cast<int>(idx_val.num_val);
                } else throw std::runtime_error("Syntax Error: Expected ')'");
            }
            
            if (data_ptr >= data_buffer.size()) {
                throw std::runtime_error("Out of DATA");
            }
            Value val = data_buffer[data_ptr++];
            
            bool is_str_var = (!var_name.empty() && var_name.back() == '$');
            if (is_str_var && val.type != Value::Type::STR) throw std::runtime_error("Type Mismatch in READ (Expected String)");
            if (!is_str_var && val.type != Value::Type::NUM) throw std::runtime_error("Type Mismatch in READ (Expected Number)");
            
            if (arr_idx >= 0) {
                 if (arrays.find(var_name) == arrays.end()) throw std::runtime_error("Array not dimensioned");
                 if (arr_idx < 0 || arr_idx >= arrays[var_name].size()) throw std::runtime_error("Out of bounds");
                 arrays[var_name][arr_idx] = val;
            } else {
                 variables[var_name] = val;
            }
            
            if (pos < tokens.size() && tokens[pos].type == TokenType::COMMA) {
                pos++;
            } else {
                break;
            }
        }
    }
    else if (tokens[pos].type == TokenType::GOTO) {
        pos++;
        Value line_to_go = parse_relation(tokens, pos);
        if (line_to_go.type != Value::Type::NUM) throw std::runtime_error("Type Mismatch: GOTO requires number");
        current_line = static_cast<int>(line_to_go.num_val);
        if (program_lines.find(current_line) == program_lines.end()) {
            throw std::runtime_error("GOTO target line not found");
        }
        branch_taken = true;
    }
    else if (tokens[pos].type == TokenType::GOSUB) {
        pos++;
        Value line_to_go = parse_relation(tokens, pos);
        if (line_to_go.type != Value::Type::NUM) throw std::runtime_error("Type Mismatch: GOSUB requires number");
        int target = static_cast<int>(line_to_go.num_val);
        if (program_lines.find(target) == program_lines.end()) {
            throw std::runtime_error("GOSUB target line not found");
        }
        call_stack.push_back(current_line);
        current_line = target;
        branch_taken = true;
    }
    else if (tokens[pos].type == TokenType::RETURN) {
        pos++;
        if (call_stack.empty()) throw std::runtime_error("RETURN WITHOUT GOSUB");
        int returned_from = call_stack.back();
        call_stack.pop_back();
        auto it = program_lines.find(returned_from);
        if (it != program_lines.end()) {
            auto next_it = std::next(it);
            if (next_it != program_lines.end()) {
                current_line = next_it->first;
                branch_taken = true;
            } else {
                current_line = -1;
                branch_taken = true;
            }
        } else {
            throw std::runtime_error("Original line disappeared during GOSUB");
        }
    }
    else if (tokens[pos].type == TokenType::IF) {
        pos++;
        Value condition_result = parse_relation(tokens, pos);
        if (condition_result.type != Value::Type::NUM) throw std::runtime_error("Type Mismatch: IF condition must be numeric");
        
        if (pos < tokens.size() && tokens[pos].type == TokenType::THEN) {
            pos++;
            if (condition_result.num_val != 0.0f) {
                execute_statement(tokens, pos);
            } else {
                pos = tokens.size();
            }
        } else {
            throw std::runtime_error("Syntax Error: Missing THEN in IF statement");
        }
    }
    else if (tokens[pos].type == TokenType::FOR) {
        pos++;
        if (pos < tokens.size() && tokens[pos].type == TokenType::IDENTIFIER) {
            std::string var_name = tokens[pos].text;
            pos++;
            if (pos < tokens.size() && tokens[pos].type == TokenType::ASSIGN) {
                pos++;
                Value start_val = parse_relation(tokens, pos);
                if (start_val.type != Value::Type::NUM) throw std::runtime_error("Type Mismatch: FOR start value");
                
                if (pos < tokens.size() && tokens[pos].type == TokenType::TO) {
                    pos++;
                    Value end_val = parse_relation(tokens, pos);
                    if (end_val.type != Value::Type::NUM) throw std::runtime_error("Type Mismatch: FOR end value");
                    
                    float step_val = 1.0f;
                    if (pos < tokens.size() && tokens[pos].type == TokenType::STEP) {
                        pos++;
                        Value step_v = parse_relation(tokens, pos);
                        if (step_v.type != Value::Type::NUM) throw std::runtime_error("Type Mismatch: FOR step value");
                        step_val = step_v.num_val;
                    }
                    
                    variables[var_name] = start_val;
                    for_stack.push_back({var_name, end_val.num_val, step_val, current_line, pos});
                } else {
                    throw std::runtime_error("Syntax Error: Expected TO in FOR");
                }
            } else {
                throw std::runtime_error("Syntax Error: Expected '=' in FOR");
            }
        } else {
            throw std::runtime_error("Syntax Error: Expected Identifier in FOR");
        }
    }
    else if (tokens[pos].type == TokenType::NEXT) {
        pos++;
        if (!for_stack.empty()) {
            std::string var_name = "";
            if (pos < tokens.size() && tokens[pos].type == TokenType::IDENTIFIER) {
                var_name = tokens[pos].text;
                pos++;
            }
            
            auto& ctx = for_stack.back();
            if (!var_name.empty() && ctx.var_name != var_name) throw std::runtime_error("NEXT variable does not match FOR");
            
            variables[ctx.var_name].num_val += ctx.step;
            
            bool cont = (ctx.step > 0) ? (variables[ctx.var_name].num_val <= ctx.target) 
                                       : (variables[ctx.var_name].num_val >= ctx.target);
            if (cont) {
                branch_taken = true;
                auto it = program_lines.find(ctx.loop_start_line);
                if (it != program_lines.end()) {
                    auto next_it = std::next(it);
                    if (next_it != program_lines.end()) {
                        current_line = next_it->first;
                    } else throw std::runtime_error("No line after FOR");
                }
            } else {
                for_stack.pop_back();
            }
        } else {
            throw std::runtime_error("NEXT without FOR");
        }
    }
    else if (tokens[pos].type == TokenType::DIM) {
        pos++;
        if (pos < tokens.size() && tokens[pos].type == TokenType::IDENTIFIER) {
            std::string var_name = tokens[pos].text;
            pos++;
            if (pos < tokens.size() && tokens[pos].type == TokenType::LPAREN) {
                pos++;
                Value size_val = parse_relation(tokens, pos);
                if (size_val.type != Value::Type::NUM || size_val.num_val < 0) throw std::runtime_error("Syntax Error: Invalid Array Size");
                
                if (pos < tokens.size() && tokens[pos].type == TokenType::RPAREN) {
                    pos++;
                    int arr_size = static_cast<int>(size_val.num_val) + 1;
                    Value default_val = (!var_name.empty() && var_name.back() == '$') ? Value(std::string("")) : Value(0.0f);
                    arrays[var_name] = std::vector<Value>(arr_size, default_val);
                } else throw std::runtime_error("Syntax Error: Expected ')' in DIM");
            } else throw std::runtime_error("Syntax Error: Expected '(' after DIM variable");
        } else throw std::runtime_error("Syntax Error: Expected identifier after DIM");
    }
    else if (tokens[pos].type == TokenType::LET || tokens[pos].type == TokenType::IDENTIFIER) {
        std::string var_name;
        if (tokens[pos].type == TokenType::LET) {
            pos++;
            if (pos < tokens.size() && tokens[pos].type == TokenType::IDENTIFIER) {
                var_name = tokens[pos].text;
                pos++;
            } else throw std::runtime_error("Syntax Error: Expected identifier after LET");
        } else {
            var_name = tokens[pos].text;
            pos++;
        }
        
        int arr_idx = -1;
        if (pos < tokens.size() && tokens[pos].type == TokenType::LPAREN) {
            pos++;
            Value idx_val = parse_relation(tokens, pos);
            if (idx_val.type != Value::Type::NUM) throw std::runtime_error("Type Mismatch: Array index");
            if (pos < tokens.size() && tokens[pos].type == TokenType::RPAREN) {
                pos++;
                arr_idx = static_cast<int>(idx_val.num_val);
            } else throw std::runtime_error("Syntax Error: Expected ')'");
        }
        
        if (pos < tokens.size() && tokens[pos].type == TokenType::ASSIGN) {
            pos++;
            Value result = parse_relation(tokens, pos);
            
            bool is_str_var = (!var_name.empty() && var_name.back() == '$');
            if (is_str_var && result.type != Value::Type::STR) throw std::runtime_error("Type Mismatch: Assigning NUM to STR variable");
            if (!is_str_var && result.type != Value::Type::NUM) throw std::runtime_error("Type Mismatch: Assigning STR to NUM variable");

            if (arr_idx >= 0) {
                if (arrays.find(var_name) == arrays.end()) throw std::runtime_error("Array not dimensioned: " + var_name);
                if (arr_idx < 0 || arr_idx >= arrays[var_name].size()) throw std::runtime_error("Out of bounds");
                arrays[var_name][arr_idx] = result;
            } else {
                variables[var_name] = result;
            }
        } else {
            throw std::runtime_error("Syntax Error: Expected assignment");
        }
    } else {
        throw std::runtime_error("Syntax Error: Unrecognized statement");
    }
}

// ---------------------------------------------------------
// Public API
// ---------------------------------------------------------
void parse_and_execute(const std::vector<Token>& tokens) {
    if (tokens.empty() || tokens[0].type == TokenType::END_OF_FILE) return;
    
    try {
        if (tokens[0].type == TokenType::NUMBER) {
            int line_num = std::stoi(tokens[0].text);
            std::vector<Token> remainder;
            for (size_t i = 1; i < tokens.size(); i++) remainder.push_back(tokens[i]);
            store_line(line_num, remainder);
            return;
        } else if (tokens[0].type == TokenType::NEW) {
            clear_program();
            return;
        } else if (tokens[0].type == TokenType::LIST) {
            list_program();
            return;
        } else if (tokens[0].type == TokenType::RUN) {
            run_program();
            return;
        }
    
        size_t pos = 0;
        execute_statement(tokens, pos);
    } catch (const std::exception& e) {
        std::string err = e.what();
        printf("%s\n", err.c_str());
        hal_display_print(err + "\n");
    }
}

void store_line(int line_number, const std::vector<Token>& tokens) {
    if (tokens.empty() || (tokens.size() == 1 && tokens[0].type == TokenType::END_OF_FILE)) {
        program_lines.erase(line_number);
    } else {
        program_lines[line_number] = tokens;
    }
}

void clear_program() {
    program_lines.clear();
    variables.clear();
    arrays.clear();
    for_stack.clear();
    call_stack.clear();
    data_buffer.clear();
    data_ptr = 0;
}

void run_program(int max_steps) {
    if (program_lines.empty()) return;
    
    for_stack.clear();
    call_stack.clear();
    
    // Pre-scan DATA statements
    data_buffer.clear();
    data_ptr = 0;
    for (const auto& pair : program_lines) {
        const auto& toks = pair.second;
        if (!toks.empty() && toks[0].type == TokenType::DATA) {
            size_t pos = 1;
            while (pos < toks.size() && toks[pos].type != TokenType::END_OF_FILE) {
                // Constants only via expression parsing
                data_buffer.push_back(parse_relation(toks, pos));
                if (pos < toks.size() && toks[pos].type == TokenType::COMMA) {
                    pos++;
                } else if (pos < toks.size() && toks[pos].type != TokenType::END_OF_FILE) {
                    // Ignore remaining garbage on DATA line if any or throw, letting it skip is safer
                    break;
                }
            }
        }
    }
    
    auto it = program_lines.begin();
    int steps = 0;
    
    while (it != program_lines.end()) {
        if (max_steps != -1 && steps >= max_steps) {
            printf("Break: Max steps reached.\n");
            break;
        }
        
        current_line = it->first;
        branch_taken = false;
        
        try {
            size_t pos = 0;
            execute_statement(it->second, pos);
        } catch (const std::exception& e) {
            char buf[128];
            snprintf(buf, sizeof(buf), "Error in line %d: %s\n", current_line, e.what());
            printf("%s", buf);
            hal_display_print(buf);
            break;
        }
        
        if (branch_taken) {
            if (current_line == -1) break; 
            it = program_lines.find(current_line);
            if (it == program_lines.end()) break;
        } else {
            it++;
        }
        steps++;
    }
}

void list_program() {
    for (const auto& pair : program_lines) {
        std::string line_str = std::to_string(pair.first);
        for (const auto& token : pair.second) {
            if (token.type == TokenType::END_OF_FILE) continue;
            if (token.type == TokenType::STRING) {
                line_str += " \"" + token.text + "\"";
            } else if (token.type == TokenType::COMMA) {
                line_str += ","; 
            } else {
                line_str += " " + token.text;
            }
        }
        line_str += "\n";
        printf("%s", line_str.c_str());
        hal_display_print(line_str);
    }
}
