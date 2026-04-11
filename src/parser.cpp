#include "parser.h"
#include "hal_display.h"
#include <stdio.h>
#include <stdexcept>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <cctype>

// ---------------------------------------------------------
// Data Representation
// ---------------------------------------------------------
struct Value {
    enum class Type { NUM, STR };
    Type type;
    float num_val;
    char str_val[128];

    Value() : type(Type::NUM), num_val(0.0f) { str_val[0] = '\0'; }
    Value(float n) : type(Type::NUM), num_val(n) { str_val[0] = '\0'; }
    Value(const char* s) : type(Type::STR), num_val(0.0f) {
        strncpy(str_val, s, sizeof(str_val) - 1);
        str_val[sizeof(str_val) - 1] = '\0';
    }

    const char* c_str() const {
        if (type == Type::STR) return str_val;
        static char buf[32];
        snprintf(buf, sizeof(buf), "%g", num_val);
        return buf;
    }
};

// ---------------------------------------------------------
// Static Allocators for Memory
// ---------------------------------------------------------

#define MAX_VARIABLES 128
#define MAX_ARRAY_HEAP 1024
#define MAX_FOR_STACK 16
#define MAX_CALL_STACK 64
#define MAX_DATA_BUFFER 256
#define MAX_PROGRAM_LINES 256

struct Variable {
    char name[64];
    Value val;
    bool active;
};

struct ArrayRef {
    char name[64];
    int start_idx;
    int size;
    bool active;
};

struct ProgramLine {
    int line_number;
    TokenList tokens;
    bool active;
};

struct ForLoopContext {
    char var_name[64];
    float target;
    float step;
    int loop_start_line;
    int loop_start_pos;
};

// State
static Variable variables[MAX_VARIABLES];
static Value array_heap[MAX_ARRAY_HEAP];
static int array_heap_ptr = 0;
static ArrayRef arrays[MAX_VARIABLES];

static ProgramLine program_lines[MAX_PROGRAM_LINES];
static int current_line = 0;
static bool branch_taken = false;

static ForLoopContext for_stack[MAX_FOR_STACK];
static int for_stack_ptr = 0;

static int call_stack[MAX_CALL_STACK];
static int call_stack_ptr = 0;

static Value data_buffer[MAX_DATA_BUFFER];
static int data_buffer_size = 0;
static int data_ptr = 0;

// Variables lookup
static Value* get_variable(const char* name) {
    for (int i = 0; i < MAX_VARIABLES; ++i) {
        if (variables[i].active && strcmp(variables[i].name, name) == 0) {
            return &variables[i].val;
        }
    }
    return nullptr;
}

static void set_variable(const char* name, const Value& val) {
    Value* v = get_variable(name);
    if (v) { *v = val; return; }
    for (int i = 0; i < MAX_VARIABLES; ++i) {
        if (!variables[i].active) {
            variables[i].active = true;
            strncpy(variables[i].name, name, sizeof(variables[i].name)-1);
            variables[i].name[sizeof(variables[i].name)-1] = '\0';
            variables[i].val = val;
            return;
        }
    }
    throw std::runtime_error("Out of Memory: Too many variables");
}

static ArrayRef* get_array(const char* name) {
    for (int i = 0; i < MAX_VARIABLES; ++i) {
        if (arrays[i].active && strcmp(arrays[i].name, name) == 0) {
            return &arrays[i];
        }
    }
    return nullptr;
}

// Memory Utilities
static int find_program_line(int line_number) {
    for (int i = 0; i < MAX_PROGRAM_LINES; ++i) {
        if (program_lines[i].active && program_lines[i].line_number == line_number) return i;
    }
    return -1;
}

static int get_next_program_line(int line_number) {
    int next_line = 999999;
    int next_idx = -1;
    for (int i = 0; i < MAX_PROGRAM_LINES; ++i) {
        if (program_lines[i].active && program_lines[i].line_number > line_number) {
            if (program_lines[i].line_number < next_line) {
                next_line = program_lines[i].line_number;
                next_idx = i;
            }
        }
    }
    return next_idx;
}

// ---------------------------------------------------------
// Forward Declarations & Helpers
// ---------------------------------------------------------
static void execute_statement(const TokenList& tokens, int& pos);
static Value parse_expression(const TokenList& tokens, int& pos);
static Value parse_term(const TokenList& tokens, int& pos);
static Value parse_factor(const TokenList& tokens, int& pos);
static Value parse_relation(const TokenList& tokens, int& pos);

static void require_token(const TokenList& tokens, int& pos, TokenType expected, const char* err_msg) {
    if (pos >= tokens.size || tokens.tokens[pos].type != expected) {
        throw std::runtime_error(err_msg);
    }
}

static bool is_builtin_function(const char* name) {
    return strcmp(name, "ABS") == 0 || strcmp(name, "INT") == 0 || strcmp(name, "RND") == 0 || 
           strcmp(name, "LEN") == 0 || strcmp(name, "MID$") == 0 || strcmp(name, "LEFT$") == 0;
}

static Value evaluate_builtin_function(const char* var_name, Value* args, int arg_count) {
    if (strcmp(var_name, "ABS") == 0) {
        if (arg_count != 1 || args[0].type != Value::Type::NUM) throw std::runtime_error("Type Mismatch/Arg Count in ABS");
        return Value(std::abs(args[0].num_val));
    } else if (strcmp(var_name, "INT") == 0) {
        if (arg_count != 1 || args[0].type != Value::Type::NUM) throw std::runtime_error("Type Mismatch/Arg Count in INT");
        return Value(std::floor(args[0].num_val));
    } else if (strcmp(var_name, "RND") == 0) {
        if (arg_count != 1 || args[0].type != Value::Type::NUM) throw std::runtime_error("Type Mismatch/Arg Count in RND");
        float scale = args[0].num_val;
        float r = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
        return Value(r * scale);
    } else if (strcmp(var_name, "LEN") == 0) {
        if (arg_count != 1 || args[0].type != Value::Type::STR) throw std::runtime_error("Type Mismatch/Arg Count in LEN");
        return Value((float)strlen(args[0].str_val));
    } else if (strcmp(var_name, "MID$") == 0) {
        if (arg_count != 3 || args[0].type != Value::Type::STR || args[1].type != Value::Type::NUM || args[2].type != Value::Type::NUM)
            throw std::runtime_error("Type Mismatch/Arg Count in MID$");
        int start = static_cast<int>(args[1].num_val) - 1; 
        int len = static_cast<int>(args[2].num_val);
        char* s = args[0].str_val;
        if (start < 0) start = 0;
        if (len < 0) len = 0;
        if (start >= strlen(s)) return Value("");
        char buf[128];
        snprintf(buf, sizeof(buf), "%.*s", len, s + start);
        return Value(buf);
    } else if (strcmp(var_name, "LEFT$") == 0) {
        if (arg_count != 2 || args[0].type != Value::Type::STR || args[1].type != Value::Type::NUM)
            throw std::runtime_error("Type Mismatch/Arg Count in LEFT$");
        int len = static_cast<int>(args[1].num_val);
        char* s = args[0].str_val;
        if (len < 0) len = 0;
        if (len >= strlen(s)) return Value(s);
        char buf[128];
        snprintf(buf, sizeof(buf), "%.*s", len, s);
        return Value(buf);
    }
    throw std::runtime_error("Unknown Function");
}

// ---------------------------------------------------------
// Recursive Descent Parser for Expressions
// ---------------------------------------------------------

static Value parse_factor(const TokenList& tokens, int& pos) {
    if (pos >= tokens.size) return Value(0.0f);
    Token t = tokens.tokens[pos];
    
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
        pos++; return Value((float)atof(t.text));
    }
    if (t.type == TokenType::STRING) {
        pos++; return Value((const char*)t.text);
    }
    if (t.type == TokenType::IDENTIFIER) {
        char var_name[64];
        strncpy(var_name, t.text, sizeof(var_name)-1);
        var_name[sizeof(var_name)-1] = '\0';
        pos++;
        
        // Array Access or Function Call: RND(1), A(idx)
        if (pos < tokens.size && tokens.tokens[pos].type == TokenType::LPAREN) {
            pos++; // skip '('
            Value args[16];
            int arg_count = 0;
            args[arg_count++] = parse_relation(tokens, pos);
            while (pos < tokens.size && tokens.tokens[pos].type == TokenType::COMMA) {
                pos++;
                if (arg_count < 16) args[arg_count++] = parse_relation(tokens, pos);
                else { parse_relation(tokens, pos); arg_count++; } // parse but discard to error
            }
            require_token(tokens, pos, TokenType::RPAREN, "Syntax Error: Expected ')' for function or array");
            pos++; // skip ')'
            
            // Built-in functions
            if (is_builtin_function(var_name)) {
                return evaluate_builtin_function(var_name, args, arg_count);
            }

            // Fallback: Array lookup
            if (arg_count != 1) throw std::runtime_error("Syntax Error: Multidimensional arrays not supported yet");
            if (args[0].type != Value::Type::NUM) throw std::runtime_error("Type Mismatch: Array index must be numeric");
            
            int idx = static_cast<int>(args[0].num_val);
            ArrayRef* arr = get_array(var_name);
            if (!arr) throw std::runtime_error("Array not dimensioned");
            if (idx < 0 || idx >= arr->size) throw std::runtime_error("Out of bounds");

            return array_heap[arr->start_idx + idx];
        }
        
        // Scalar variable
        Value* v = get_variable(var_name);
        if (!v) {
            int len = strlen(var_name);
            if (len > 0 && var_name[len - 1] == '$') return Value("");
            return Value(0.0f);
        }
        return *v;
    }
    if (t.type == TokenType::LPAREN) {
        pos++; 
        Value val = parse_relation(tokens, pos);
        require_token(tokens, pos, TokenType::RPAREN, "Missing closing parenthesis");
        pos++;
        return val;
    }
    throw std::runtime_error("Syntax Error in expression");
}

static Value parse_term(const TokenList& tokens, int& pos) {
    Value val = parse_factor(tokens, pos);
    while (pos < tokens.size) {
        TokenType op = tokens.tokens[pos].type;
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

static Value parse_expression(const TokenList& tokens, int& pos) {
    Value val = parse_term(tokens, pos);
    while (pos < tokens.size) {
        TokenType op = tokens.tokens[pos].type;
        if (op != TokenType::PLUS && op != TokenType::MINUS) break;
        pos++;
        Value next_val = parse_term(tokens, pos);
        
        if (op == TokenType::PLUS) {
            if (val.type == Value::Type::STR && next_val.type == Value::Type::STR) {
                char buf[128];
                snprintf(buf, sizeof(buf), "%s%s", val.str_val, next_val.str_val);
                val = Value(buf);
            } else if (val.type == Value::Type::NUM && next_val.type == Value::Type::NUM) {
                val.num_val += next_val.num_val;
            } else {
                throw std::runtime_error("Type Mismatch: Cannot add string and number");
            }
        } else {
            if (val.type != Value::Type::NUM || next_val.type != Value::Type::NUM) throw std::runtime_error("Type Mismatch: Cannot subtract strings");
            val.num_val -= next_val.num_val;
        }
    }
    return val;
}

static Value parse_relation(const TokenList& tokens, int& pos) {
    Value val = parse_expression(tokens, pos);
    while (pos < tokens.size) {
        TokenType op = tokens.tokens[pos].type;
        if (op == TokenType::ASSIGN || op == TokenType::GT || op == TokenType::LT || 
            op == TokenType::GTE || op == TokenType::LTE || op == TokenType::NEQ) {
            pos++;
            Value next_val = parse_expression(tokens, pos);
            
            if (val.type != next_val.type) throw std::runtime_error("Type Mismatch: Cannot compare string and number");
            
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
                const char* a = val.str_val, *b = next_val.str_val;
                int cmp = strcmp(a, b);
                if (op == TokenType::ASSIGN) result = (cmp == 0);
                else if (op == TokenType::GT) result = (cmp > 0);
                else if (op == TokenType::LT) result = (cmp < 0);
                else if (op == TokenType::GTE) result = (cmp >= 0);
                else if (op == TokenType::LTE) result = (cmp <= 0);
                else if (op == TokenType::NEQ) result = (cmp != 0);
            }
            val = Value(result ? 1.0f : 0.0f);
        } else break;
    }
    return val;
}

// ---------------------------------------------------------
// Statement Execution Core (Delegates)
// ---------------------------------------------------------

static void execute_print(const TokenList& tokens, int& pos) {
    pos++; // skip PRINT
    char output[512] = "";
    while (pos < tokens.size && tokens.tokens[pos].type != TokenType::END_OF_FILE) {
        Value result = parse_relation(tokens, pos);
        strncat(output, result.c_str(), sizeof(output) - strlen(output) - 1);
        strncat(output, "\t", sizeof(output) - strlen(output) - 1);
        if (pos < tokens.size && tokens.tokens[pos].type == TokenType::COMMA) pos++;
        else break;
    }
    int out_len = strlen(output);
    if (out_len > 0 && output[out_len - 1] == '\t') output[out_len - 1] = '\0';
    strncat(output, "\n", sizeof(output) - strlen(output) - 1);
    printf("%s", output);
    hal_display_print(output);
}

static void execute_read(const TokenList& tokens, int& pos) {
    pos++; // skip READ
    while (pos < tokens.size && tokens.tokens[pos].type != TokenType::END_OF_FILE) {
        require_token(tokens, pos, TokenType::IDENTIFIER, "Syntax Error: READ expects identifier");
        char var_name[64];
        strncpy(var_name, tokens.tokens[pos].text, sizeof(var_name)-1);
        var_name[sizeof(var_name)-1] = '\0';
        pos++;
        
        int arr_idx = -1;
        if (pos < tokens.size && tokens.tokens[pos].type == TokenType::LPAREN) {
            pos++;
            Value idx_val = parse_relation(tokens, pos);
            if (idx_val.type != Value::Type::NUM) throw std::runtime_error("Type Mismatch: Array index");
            require_token(tokens, pos, TokenType::RPAREN, "Syntax Error: Expected ')'");
            pos++;
            arr_idx = static_cast<int>(idx_val.num_val);
        }
        
        if (data_ptr >= data_buffer_size) throw std::runtime_error("Out of DATA");
        Value val = data_buffer[data_ptr++];
        
        int nlen = strlen(var_name);
        bool is_str_var = (nlen > 0 && var_name[nlen-1] == '$');
        if (is_str_var && val.type != Value::Type::STR) throw std::runtime_error("Type Mismatch in READ (Expected String)");
        if (!is_str_var && val.type != Value::Type::NUM) throw std::runtime_error("Type Mismatch in READ (Expected Number)");
        
        if (arr_idx >= 0) {
            ArrayRef* arr = get_array(var_name);
            if (!arr) throw std::runtime_error("Array not dimensioned");
            if (arr_idx < 0 || arr_idx >= arr->size) throw std::runtime_error("Out of bounds");
            array_heap[arr->start_idx + arr_idx] = val;
        } else {
            set_variable(var_name, val);
        }
        
        if (pos < tokens.size && tokens.tokens[pos].type == TokenType::COMMA) { pos++; } else { break; }
    }
}

static void execute_goto(const TokenList& tokens, int& pos) {
    pos++; // skip GOTO
    Value line_to_go = parse_relation(tokens, pos);
    if (line_to_go.type != Value::Type::NUM) throw std::runtime_error("Type Mismatch: GOTO requires number");
    current_line = static_cast<int>(line_to_go.num_val);
    if (find_program_line(current_line) == -1) throw std::runtime_error("GOTO target line not found");
    branch_taken = true;
}

static void execute_gosub(const TokenList& tokens, int& pos) {
    pos++; // skip GOSUB
    Value line_to_go = parse_relation(tokens, pos);
    if (line_to_go.type != Value::Type::NUM) throw std::runtime_error("Type Mismatch: GOSUB requires number");
    int target = static_cast<int>(line_to_go.num_val);
    if (find_program_line(target) == -1) throw std::runtime_error("GOSUB target line not found");
    if (call_stack_ptr >= MAX_CALL_STACK) throw std::runtime_error("Out of Memory: Call Stack Limit Reached");
    call_stack[call_stack_ptr++] = current_line;
    current_line = target;
    branch_taken = true;
}

static void execute_return(const TokenList& tokens, int& pos) {
    pos++; // skip RETURN
    if (call_stack_ptr == 0) throw std::runtime_error("RETURN WITHOUT GOSUB");
    int returned_from = call_stack[--call_stack_ptr];
    int returned_idx = find_program_line(returned_from);
    if (returned_idx != -1) {
        int next_idx = get_next_program_line(returned_from);
        if (next_idx != -1) {
            current_line = program_lines[next_idx].line_number;
            branch_taken = true;
        } else {
            current_line = -1;
            branch_taken = true;
        }
    } else {
        throw std::runtime_error("Original line disappeared during GOSUB");
    }
}

static void execute_if(const TokenList& tokens, int& pos) {
    pos++; // skip IF
    Value condition_result = parse_relation(tokens, pos);
    if (condition_result.type != Value::Type::NUM) throw std::runtime_error("Type Mismatch: IF condition must be numeric");
    
    require_token(tokens, pos, TokenType::THEN, "Syntax Error: Missing THEN in IF statement");
    pos++; // skip THEN
    
    if (condition_result.num_val != 0.0f) {
        execute_statement(tokens, pos);
        if (pos < tokens.size && tokens.tokens[pos].type == TokenType::ELSE) pos = tokens.size;
    } else {
        while (pos < tokens.size && tokens.tokens[pos].type != TokenType::ELSE && tokens.tokens[pos].type != TokenType::END_OF_FILE) { pos++; }
        if (pos < tokens.size && tokens.tokens[pos].type == TokenType::ELSE) {
            pos++; // Consume ELSE
            execute_statement(tokens, pos);
        }
    }
}

static void execute_for(const TokenList& tokens, int& pos) {
    pos++; // skip FOR
    require_token(tokens, pos, TokenType::IDENTIFIER, "Syntax Error: Expected Identifier in FOR");
    char var_name[64];
    strncpy(var_name, tokens.tokens[pos].text, sizeof(var_name)-1);
    var_name[sizeof(var_name)-1] = '\0';
    pos++;

    require_token(tokens, pos, TokenType::ASSIGN, "Syntax Error: Expected '=' in FOR");
    pos++;
    Value start_val = parse_relation(tokens, pos);
    if (start_val.type != Value::Type::NUM) throw std::runtime_error("Type Mismatch: FOR start value");
    
    require_token(tokens, pos, TokenType::TO, "Syntax Error: Expected TO in FOR");
    pos++;
    Value end_val = parse_relation(tokens, pos);
    if (end_val.type != Value::Type::NUM) throw std::runtime_error("Type Mismatch: FOR end value");
    
    float step_val = 1.0f;
    if (pos < tokens.size && tokens.tokens[pos].type == TokenType::STEP) {
        pos++;
        Value step_v = parse_relation(tokens, pos);
        if (step_v.type != Value::Type::NUM) throw std::runtime_error("Type Mismatch: FOR step value");
        step_val = step_v.num_val;
    }
    
    set_variable(var_name, start_val);
    if (for_stack_ptr >= MAX_FOR_STACK) throw std::runtime_error("Out of Memory: FOR Stack Limit Reached");
    ForLoopContext ctx = {};
    strncpy(ctx.var_name, var_name, sizeof(ctx.var_name)-1);
    ctx.target = end_val.num_val;
    ctx.step = step_val;
    ctx.loop_start_line = current_line;
    ctx.loop_start_pos = pos;
    for_stack[for_stack_ptr++] = ctx;
}

static void execute_next(const TokenList& tokens, int& pos) {
    pos++; // skip NEXT
    if (for_stack_ptr == 0) throw std::runtime_error("NEXT without FOR");
    
    char var_name[64] = "";
    if (pos < tokens.size && tokens.tokens[pos].type == TokenType::IDENTIFIER) {
        strncpy(var_name, tokens.tokens[pos].text, sizeof(var_name)-1);
        var_name[sizeof(var_name)-1] = '\0';
        pos++;
    }
    
    ForLoopContext& ctx = for_stack[for_stack_ptr - 1];
    if (strlen(var_name) > 0 && strcmp(ctx.var_name, var_name) != 0) throw std::runtime_error("NEXT variable does not match FOR");
    
    Value* v = get_variable(ctx.var_name);
    if (!v) throw std::runtime_error("Intern Error: FOR variable missing");
    v->num_val += ctx.step;
    
    bool cont = (ctx.step > 0) ? (v->num_val <= ctx.target) : (v->num_val >= ctx.target);
    if (cont) {
        branch_taken = true;
        int idx = get_next_program_line(ctx.loop_start_line);
        if (idx != -1) current_line = program_lines[idx].line_number;
        else throw std::runtime_error("No line after FOR");
    } else {
        for_stack_ptr--;
    }
}

static void execute_dim(const TokenList& tokens, int& pos) {
    pos++; // skip DIM
    require_token(tokens, pos, TokenType::IDENTIFIER, "Syntax Error: Expected identifier after DIM");
    char var_name[64];
    strncpy(var_name, tokens.tokens[pos].text, sizeof(var_name)-1);
    var_name[sizeof(var_name)-1] = '\0';
    pos++;
    
    require_token(tokens, pos, TokenType::LPAREN, "Syntax Error: Expected '(' after DIM variable");
    pos++;
    Value size_val = parse_relation(tokens, pos);
    if (size_val.type != Value::Type::NUM || size_val.num_val < 0) throw std::runtime_error("Syntax Error: Invalid Array Size");
    
    require_token(tokens, pos, TokenType::RPAREN, "Syntax Error: Expected ')' in DIM");
    pos++;
    
    int arr_size = static_cast<int>(size_val.num_val) + 1;
    if (array_heap_ptr + arr_size > MAX_ARRAY_HEAP) throw std::runtime_error("Out of Memory: Array Heap Full");

    ArrayRef* arr = get_array(var_name);
    if (!arr) {
        for (int i=0; i<MAX_VARIABLES; i++) {
            if (!arrays[i].active) {
                arr = &arrays[i];
                break;
            }
        }
    }
    if (!arr) throw std::runtime_error("Out of Memory: Too many arrays");
    
    arr->active = true;
    strncpy(arr->name, var_name, sizeof(arr->name)-1);
    arr->start_idx = array_heap_ptr;
    arr->size = arr_size;
    array_heap_ptr += arr_size;

    int nlen = strlen(var_name);
    Value default_val = (nlen > 0 && var_name[nlen-1] == '$') ? Value("") : Value(0.0f);
    for (int i=0; i<arr_size; i++) array_heap[arr->start_idx + i] = default_val;
}

static void execute_assignment(const TokenList& tokens, int& pos, bool explicit_let) {
    char var_name[64];
    if (explicit_let) {
        pos++;
        require_token(tokens, pos, TokenType::IDENTIFIER, "Syntax Error: Expected identifier after LET");
    }
    strncpy(var_name, tokens.tokens[pos].text, sizeof(var_name)-1);
    var_name[sizeof(var_name)-1] = '\0';
    pos++;
    
    int arr_idx = -1;
    if (pos < tokens.size && tokens.tokens[pos].type == TokenType::LPAREN) {
        pos++;
        Value idx_val = parse_relation(tokens, pos);
        if (idx_val.type != Value::Type::NUM) throw std::runtime_error("Type Mismatch: Array index");
        require_token(tokens, pos, TokenType::RPAREN, "Syntax Error: Expected ')'");
        pos++;
        arr_idx = static_cast<int>(idx_val.num_val);
    }
    
    require_token(tokens, pos, TokenType::ASSIGN, "Syntax Error: Expected assignment");
    pos++;
    Value result = parse_relation(tokens, pos);
    
    int nlen = strlen(var_name);
    bool is_str_var = (nlen > 0 && var_name[nlen-1] == '$');
    if (is_str_var && result.type != Value::Type::STR) throw std::runtime_error("Type Mismatch: Assigning NUM to STR variable");
    if (!is_str_var && result.type != Value::Type::NUM) throw std::runtime_error("Type Mismatch: Assigning STR to NUM variable");

    if (arr_idx >= 0) {
        ArrayRef* arr = get_array(var_name);
        if (!arr) throw std::runtime_error("Array not dimensioned");
        if (arr_idx < 0 || arr_idx >= arr->size) throw std::runtime_error("Out of bounds");
        array_heap[arr->start_idx + arr_idx] = result;
    } else {
        set_variable(var_name, result);
    }
}

// Switchboard
static void execute_statement(const TokenList& tokens, int& pos) {
    if (pos >= tokens.size || tokens.tokens[pos].type == TokenType::END_OF_FILE) return;

    switch (tokens.tokens[pos].type) {
        case TokenType::PRINT:   execute_print(tokens, pos); break;
        case TokenType::DATA:    pos = tokens.size; break; // Skip at runtime
        case TokenType::RESTORE: pos++; data_ptr = 0; break;
        case TokenType::READ:    execute_read(tokens, pos); break;
        case TokenType::GOTO:    execute_goto(tokens, pos); break;
        case TokenType::GOSUB:   execute_gosub(tokens, pos); break;
        case TokenType::RETURN:  execute_return(tokens, pos); break;
        case TokenType::IF:      execute_if(tokens, pos); break;
        case TokenType::FOR:     execute_for(tokens, pos); break;
        case TokenType::NEXT:    execute_next(tokens, pos); break;
        case TokenType::DIM:     execute_dim(tokens, pos); break;
        case TokenType::LET:     execute_assignment(tokens, pos, true); break;
        case TokenType::IDENTIFIER: execute_assignment(tokens, pos, false); break;
        default: throw std::runtime_error("Syntax Error: Unrecognized statement");
    }
}

// ---------------------------------------------------------
// Public API
// ---------------------------------------------------------
void parse_and_execute(const TokenList& tokens) {
    if (tokens.size == 0 || tokens.tokens[0].type == TokenType::END_OF_FILE) return;
    
    try {
        if (tokens.tokens[0].type == TokenType::NUMBER) {
            int line_num = atoi(tokens.tokens[0].text);
            TokenList remainder;
            int j = 0;
            for (int i = 1; i < tokens.size; i++) remainder.tokens[j++] = tokens.tokens[i];
            remainder.size = j;
            store_line(line_num, remainder);
            return;
        } else if (tokens.tokens[0].type == TokenType::NEW) {
            clear_program();
            return;
        } else if (tokens.tokens[0].type == TokenType::LIST) {
            list_program();
            return;
        } else if (tokens.tokens[0].type == TokenType::RUN) {
            run_program();
            return;
        }
    
        int pos = 0;
        execute_statement(tokens, pos);
    } catch (const std::exception& e) {
        char buf[128];
        snprintf(buf, sizeof(buf), "%s\n", e.what());
        printf("%s", buf);
        hal_display_print(buf);
    }
}

void store_line(int line_number, const TokenList& tokens) {
    int idx = find_program_line(line_number);
    if (tokens.size == 0 || (tokens.size == 1 && tokens.tokens[0].type == TokenType::END_OF_FILE)) {
        if (idx != -1) program_lines[idx].active = false;
    } else {
        if (idx != -1) {
            program_lines[idx].tokens = tokens;
        } else {
            for (int i = 0; i < MAX_PROGRAM_LINES; i++) {
                if (!program_lines[i].active) {
                    program_lines[i].active = true;
                    program_lines[i].line_number = line_number;
                    program_lines[i].tokens = tokens;
                    return;
                }
            }
            throw std::runtime_error("Out of Memory: Too many program lines");
        }
    }
}

void clear_program() {
    for (int i=0; i<MAX_PROGRAM_LINES; i++) program_lines[i].active = false;
    for (int i=0; i<MAX_VARIABLES; i++) { variables[i].active = false; arrays[i].active = false; }
    array_heap_ptr = 0;
    for_stack_ptr = 0;
    call_stack_ptr = 0;
    data_buffer_size = 0;
    data_ptr = 0;
}

void run_program(int max_steps) {
    for_stack_ptr = 0;
    call_stack_ptr = 0;
    
    // Pre-scan DATA statements
    data_buffer_size = 0;
    data_ptr = 0;
    
    int sorted_lines[MAX_PROGRAM_LINES];
    int line_count = 0;
    for (int i=0; i<MAX_PROGRAM_LINES; i++) {
        if (program_lines[i].active) sorted_lines[line_count++] = i;
    }
    // simple bubble sort
    for (int i=0; i<line_count-1; i++) {
        for (int j=0; j<line_count-i-1; j++) {
            if (program_lines[sorted_lines[j]].line_number > program_lines[sorted_lines[j+1]].line_number) {
                int temp = sorted_lines[j];
                sorted_lines[j] = sorted_lines[j+1];
                sorted_lines[j+1] = temp;
            }
        }
    }

    for (int i=0; i<line_count; i++) {
        ProgramLine& pl = program_lines[sorted_lines[i]];
        if (pl.tokens.size > 0 && pl.tokens.tokens[0].type == TokenType::DATA) {
            int pos = 1;
            while (pos < pl.tokens.size && pl.tokens.tokens[pos].type != TokenType::END_OF_FILE) {
                if (data_buffer_size < MAX_DATA_BUFFER) {
                    data_buffer[data_buffer_size++] = parse_relation(pl.tokens, pos);
                }
                if (pos < pl.tokens.size && pl.tokens.tokens[pos].type == TokenType::COMMA) pos++;
                else if (pos < pl.tokens.size && pl.tokens.tokens[pos].type != TokenType::END_OF_FILE) break;
            }
        }
    }
    
    int current_idx = line_count > 0 ? sorted_lines[0] : -1;
    if (current_idx == -1) return;
    
    int steps = 0;
    while (current_idx != -1) {
        if (max_steps != -1 && steps >= max_steps) {
            printf("Break: Max steps reached.\n");
            break;
        }
        
        current_line = program_lines[current_idx].line_number;
        branch_taken = false;
        
        try {
            int pos = 0;
            execute_statement(program_lines[current_idx].tokens, pos);
        } catch (const std::exception& e) {
            char buf[128];
            snprintf(buf, sizeof(buf), "Error in line %d: %s\n", current_line, e.what());
            printf("%s", buf);
            hal_display_print(buf);
            break;
        }
        
        if (branch_taken) {
            if (current_line == -1) break; 
            current_idx = find_program_line(current_line);
            if (current_idx == -1) break;
        } else {
            current_idx = get_next_program_line(current_line);
        }
        steps++;
    }
}

void list_program() {
    int sorted_lines[MAX_PROGRAM_LINES];
    int line_count = 0;
    for (int i=0; i<MAX_PROGRAM_LINES; i++) {
        if (program_lines[i].active) sorted_lines[line_count++] = i;
    }
    for (int i=0; i<line_count-1; i++) {
        for (int j=0; j<line_count-i-1; j++) {
            if (program_lines[sorted_lines[j]].line_number > program_lines[sorted_lines[j+1]].line_number) {
                int temp = sorted_lines[j];
                sorted_lines[j] = sorted_lines[j+1];
                sorted_lines[j+1] = temp;
            }
        }
    }
    
    for (int i=0; i<line_count; i++) {
        ProgramLine& pl = program_lines[sorted_lines[i]];
        char line_str[512];
        snprintf(line_str, sizeof(line_str), "%d", pl.line_number);
        for (int j=0; j<pl.tokens.size; j++) {
            Token& t = pl.tokens.tokens[j];
            if (t.type == TokenType::END_OF_FILE) continue;
            if (t.type == TokenType::STRING) {
                strncat(line_str, " \"", sizeof(line_str) - strlen(line_str) - 1);
                strncat(line_str, t.text, sizeof(line_str) - strlen(line_str) - 1);
                strncat(line_str, "\"", sizeof(line_str) - strlen(line_str) - 1);
            } else if (t.type == TokenType::COMMA) {
                strncat(line_str, ",", sizeof(line_str) - strlen(line_str) - 1);
            } else {
                strncat(line_str, " ", sizeof(line_str) - strlen(line_str) - 1);
                strncat(line_str, t.text, sizeof(line_str) - strlen(line_str) - 1);
            }
        }
        strncat(line_str, "\n", sizeof(line_str) - strlen(line_str) - 1);
        printf("%s", line_str);
        hal_display_print(line_str);
    }
}
