#include "parser.h"
#include "hal_display.h"
#include "hal_gpio.h"
#include "hal_sound.h"
#include "hal_touch.h"
#include "hal_sdcard.h"
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
    enum class Type { NUM, INT, STR };
    Type type;
    float num_val;
    int int_val;
    char str_val[128];

    Value() : type(Type::NUM), num_val(0.0f), int_val(0) { str_val[0] = '\0'; }
    Value(float n) : type(Type::NUM), num_val(n), int_val(0) { str_val[0] = '\0'; }
    Value(int i) : type(Type::INT), num_val((float)i), int_val(i) { str_val[0] = '\0'; }
    Value(const char* s) : type(Type::STR), num_val(0.0f), int_val(0) {
        strncpy(str_val, s, sizeof(str_val) - 1);
        str_val[sizeof(str_val) - 1] = '\0';
    }

    float get_num() const {
        return num_val;
    }

    const char* c_str() const {
        if (type == Type::STR) return str_val;
        static char buf[32];
        if (type == Type::INT) snprintf(buf, sizeof(buf), "%d", int_val);
        else snprintf(buf, sizeof(buf), "%g", num_val);
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

struct ForLoopContext {
    char var_name[64];
    float target;
    float step;
    int loop_start_line;
    int loop_start_pos;
};

// State
uint8_t logical_memory[65536];

#define MEMORY_TEXT_BASE 0x0000
#define MEMORY_VAR_BASE 0x8000
#define VAR_TABLE_SIZE (MAX_VARIABLES * 16)
#define ARRAY_TABLE_BASE (MEMORY_VAR_BASE + VAR_TABLE_SIZE)
#define ARRAY_TABLE_SIZE (MAX_VARIABLES * 16)
#define DATA_HEAP_BASE (ARRAY_TABLE_BASE + ARRAY_TABLE_SIZE)
#define STRING_HEAP_BASE 0xC000 // Simple string heap

static uint16_t string_heap_ptr = STRING_HEAP_BASE;
static uint16_t array_heap_inner_ptr = DATA_HEAP_BASE;
static float last_rnd_val = 0.0f;

static int current_line = 0;
static bool branch_taken = false;

// Colors: RGB565 Palette (Standard EGA/CGA-like 16 colors)
static const uint16_t PALETTE[16] = {
    0x0000, // 0: Black
    0x001F, // 1: Blue
    0x07E0, // 2: Green
    0x07FF, // 3: Cyan
    0xF800, // 4: Red
    0xF81F, // 5: Magenta
    0xFBE0, // 6: Brown
    0xC618, // 7: Light Gray
    0x7BEF, // 8: Dark Gray
    0x55FF, // 9: Light Blue
    0x5FE0, // 10: Light Green
    0x5FFF, // 11: Light Cyan
    0xFF80, // 12: Light Red
    0xFFDF, // 13: Light Magenta
    0xFFE0, // 14: Yellow
    0xFFFF  // 15: White
};
uint16_t current_color_565 = PALETTE[15]; // Default white

// Token serialization helpers
static int tokenize(const TokenList& tokens, uint8_t* buffer) {
    int ptr = 0;
    for (int i=0; i<tokens.size; i++) {
        buffer[ptr++] = (uint8_t)tokens.tokens[i].type;
        if (tokens.tokens[i].type == TokenType::NUMBER || 
            tokens.tokens[i].type == TokenType::IDENTIFIER || 
            tokens.tokens[i].type == TokenType::STRING) {
            int len = strlen(tokens.tokens[i].text);
            buffer[ptr++] = (uint8_t)len;
            memcpy(&buffer[ptr], tokens.tokens[i].text, len);
            ptr += len;
        }
    }
    buffer[ptr++] = 0xFF; // EOL byte
    return ptr;
}

static const char* token_type_to_string(TokenType type) {
    switch (type) {
        case TokenType::PRINT: return "PRINT";
        case TokenType::LET: return "LET";
        case TokenType::ASSIGN: return "=";
        case TokenType::PLUS: return "+";
        case TokenType::MINUS: return "-";
        case TokenType::MUL: return "*";
        case TokenType::DIV: return "/";
        case TokenType::POWER: return "^";
        case TokenType::LPAREN: return "(";
        case TokenType::RPAREN: return ")";
        case TokenType::GOTO: return "GOTO";
        case TokenType::GOSUB: return "GOSUB";
        case TokenType::RETURN: return "RETURN";
        case TokenType::IF: return "IF";
        case TokenType::THEN: return "THEN";
        case TokenType::ELSE: return "ELSE";
        case TokenType::FOR: return "FOR";
        case TokenType::TO: return "TO";
        case TokenType::STEP: return "STEP";
        case TokenType::NEXT: return "NEXT";
        case TokenType::NEW: return "NEW";
        case TokenType::LIST: return "LIST";
        case TokenType::RUN: return "RUN";
        case TokenType::READ: return "READ";
        case TokenType::DATA: return "DATA";
        case TokenType::RESTORE: return "RESTORE";
        case TokenType::DIM: return "DIM";
        case TokenType::INPUT: return "INPUT";
        case TokenType::END: return "END";
        case TokenType::STOP: return "STOP";
        case TokenType::INIT: return "INIT";
        case TokenType::CLEAR: return "CLEAR";
        case TokenType::NEWON: return "NEWON";
        case TokenType::WIDTH: return "WIDTH";
        case TokenType::CONSOLE: return "CONSOLE";
        case TokenType::CLS: return "CLS";
        case TokenType::REPEAT: return "REPEAT";
        case TokenType::UNTIL: return "UNTIL";
        case TokenType::GET: return "GET";
        case TokenType::FILES: return "FILES";
        case TokenType::SAVE: return "SAVE";
        case TokenType::LOAD: return "LOAD";
        case TokenType::ON: return "ON";
        case TokenType::COLON: return "COLON";
        case TokenType::GPIO: return "GPIO";
        case TokenType::WINDOW: return "WINDOW";
        case TokenType::PSET: return "PSET";
        case TokenType::LINE: return "LINE";
        case TokenType::CIRCLE: return "CIRCLE";
        case TokenType::POLY: return "POLY";
        case TokenType::PAINT: return "PAINT";
        case TokenType::GET_AT: return "GET@";
        case TokenType::PUT_AT: return "PUT@";
        case TokenType::COLOR: return "COLOR";
        case TokenType::BEEP: return "BEEP";
        case TokenType::PLAY: return "PLAY";
        case TokenType::MUSIC: return "MUSIC";
        case TokenType::SOUND: return "SOUND";
        case TokenType::GT: return ">";
        case TokenType::LT: return "<";
        case TokenType::GTE: return ">=";
        case TokenType::LTE: return "<=";
        case TokenType::NEQ: return "<>";
        case TokenType::COMMA: return ",";
        case TokenType::SEMICOLON: return ";";
        case TokenType::WAIT: return "WAIT";
        default: return "";
    }
}

static TokenList detokenize(const uint8_t* buffer) {
    TokenList t;
    int ptr = 0;
    while (t.size < MAX_TOKENS_PER_LINE && buffer[ptr] != 0xFF) {
        // Safety: don't scan too far
        if (ptr > 512) break; 

        t.tokens[t.size].type = (TokenType)buffer[ptr++];
        if (t.tokens[t.size].type == TokenType::NUMBER || 
            t.tokens[t.size].type == TokenType::IDENTIFIER || 
            t.tokens[t.size].type == TokenType::STRING) {
            int len = buffer[ptr++];
            if (len > 64) len = 64; // Safety
            memcpy(t.tokens[t.size].text, &buffer[ptr], len);
            t.tokens[t.size].text[len] = '\0';
            ptr += len;
        } else {
            const char* kw = token_type_to_string(t.tokens[t.size].type);
            strncpy(t.tokens[t.size].text, kw, MAX_TOKEN_LEN - 1);
            t.tokens[t.size].text[MAX_TOKEN_LEN - 1] = '\0';
        }
        t.size++;
    }
    return t;
}

static void update_program_links() {
    uint16_t ptr = MEMORY_TEXT_BASE;
    // Empty program check
    uint16_t first_next = logical_memory[ptr] | (logical_memory[ptr+1] << 8);
    if (first_next == 0 && logical_memory[ptr+2] == 0 && logical_memory[ptr+3] == 0) return;

    while (ptr < MEMORY_VAR_BASE) {
        uint16_t code_ptr = ptr + 4;
        while (code_ptr < MEMORY_VAR_BASE && logical_memory[code_ptr] != 0xFF) {
            TokenType type = (TokenType)logical_memory[code_ptr++];
            if (type == TokenType::NUMBER || type == TokenType::IDENTIFIER || type == TokenType::STRING) {
                if (code_ptr < MEMORY_VAR_BASE) {
                    int len = logical_memory[code_ptr++];
                    code_ptr += len;
                }
            }
        }
        if (code_ptr >= MEMORY_VAR_BASE) break;
        code_ptr++; // skip 0xFF
        
        uint16_t actual_next = code_ptr;
        logical_memory[ptr] = actual_next & 0xFF;
        logical_memory[ptr+1] = (actual_next >> 8) & 0xFF;

        // If the next address contains the 0x0000 terminator, we are done
        if (actual_next + 4 >= MEMORY_VAR_BASE || 
            (logical_memory[actual_next] == 0 && logical_memory[actual_next+1] == 0 &&
             logical_memory[actual_next+2] == 0 && logical_memory[actual_next+3] == 0)) {
            // Ensure the terminator at actual_next is really 0x0000
            logical_memory[actual_next] = 0;
            logical_memory[actual_next+1] = 0;
            break;
        }

        ptr = actual_next;
    }
}


static ForLoopContext for_stack[MAX_FOR_STACK];
static int for_stack_ptr = 0;

static int call_stack[MAX_CALL_STACK];
static int call_stack_ptr = 0;

static Value data_buffer[MAX_DATA_BUFFER];
static int data_buffer_size = 0;
static int data_ptr = 0;

// ---------------------------------------------------------
// Memory Compact Storage Helpers
// ---------------------------------------------------------

// Variable record layout (16 bytes per slot):
//   [0..7]   name (8 bytes, null-padded, up to 8-char names)
//   [8]      type (0=NUM, 1=INT, 2=STR)
//   [9]      active flag
//   [10..11] str_ptr (uint16, only for STR type)
//   [12..15] value (float or int, 4 bytes)
//
// String heap layout: each slot has a 2-byte size-prefix so the interpreter
// can reuse the slot in-place when a shorter string is reassigned.
//   [str_ptr+0..1]  allocated size (uint16, bytes including null terminator)
//   [str_ptr+2..]   string data (null-terminated)

static void write_compact_value(uint16_t addr, const Value& val) {
    if (val.type == Value::Type::STR) {
        // Try in-place update when the variable already holds a string
        uint8_t old_type   = logical_memory[addr + 8];
        uint8_t old_active = logical_memory[addr + 9];
        if (old_active && (Value::Type)old_type == Value::Type::STR) {
            uint16_t old_str_ptr;
            memcpy(&old_str_ptr, &logical_memory[addr + 10], 2);
            // Guard: only reuse if within current valid heap range
            if (old_str_ptr >= STRING_HEAP_BASE && old_str_ptr < string_heap_ptr) {
                uint16_t old_alloc;
                memcpy(&old_alloc, &logical_memory[old_str_ptr], 2);
                int new_len = (int)strlen(val.str_val);
                if (old_alloc > 0 && (uint16_t)(new_len + 1) <= old_alloc) {
                    // Fits — update string bytes in-place, no heap growth
                    memcpy(&logical_memory[old_str_ptr + 2], val.str_val, (size_t)(new_len + 1));
                    logical_memory[addr + 8] = (uint8_t)val.type;
                    return;
                }
            }
        }
        // Allocate new heap slot with 2-byte size prefix
        uint16_t str_ptr = string_heap_ptr;
        int len = (int)strlen(val.str_val);
        uint16_t alloc_size = (uint16_t)(len + 1);
        if ((uint32_t)str_ptr + 2 + alloc_size >= 65536u)
            throw std::runtime_error("String Heap Overflow");
        logical_memory[str_ptr]     = (uint8_t)(alloc_size & 0xFF);
        logical_memory[str_ptr + 1] = (uint8_t)(alloc_size >> 8);
        memcpy(&logical_memory[str_ptr + 2], val.str_val, (size_t)alloc_size);
        memcpy(&logical_memory[addr + 10], &str_ptr, 2);
        string_heap_ptr = (uint16_t)(str_ptr + 2 + alloc_size);
        logical_memory[addr + 8] = (uint8_t)val.type;
        return;
    }
    logical_memory[addr + 8] = (uint8_t)val.type;
    if (val.type == Value::Type::NUM) {
        memcpy(&logical_memory[addr + 12], &val.num_val, 4);
    } else if (val.type == Value::Type::INT) {
        memcpy(&logical_memory[addr + 12], &val.int_val, 4);
    }
}

static Value read_compact_value(uint16_t addr) {
    Value v;
    v.type = (Value::Type)logical_memory[addr + 8];
    if (v.type == Value::Type::NUM) {
        memcpy(&v.num_val, &logical_memory[addr + 12], 4);
    } else if (v.type == Value::Type::INT) {
        memcpy(&v.int_val, &logical_memory[addr + 12], 4);
        v.num_val = (float)v.int_val;
    } else if (v.type == Value::Type::STR) {
        uint16_t str_ptr;
        memcpy(&str_ptr, &logical_memory[addr + 10], 2);
        // Skip 2-byte size prefix to reach string data
        strncpy(v.str_val, (const char*)&logical_memory[str_ptr + 2], 127);
        v.str_val[127] = '\0';
    }
    return v;
}

// Variables lookup  — active flag at offset 9, name comparison up to 8 chars
static bool get_variable(const char* name, Value& out_val) {
    for (int i = 0; i < MAX_VARIABLES; ++i) {
        uint16_t addr = MEMORY_VAR_BASE + (i * 16);
        bool active = logical_memory[addr + 9] != 0;
        if (active && strncmp((const char*)&logical_memory[addr], name, 8) == 0) {
            out_val = read_compact_value(addr);
            return true;
        }
    }
    return false;
}

static void set_variable(const char* name, const Value& val) {
    for (int i = 0; i < MAX_VARIABLES; ++i) {
        uint16_t addr = MEMORY_VAR_BASE + (i * 16);
        bool active = logical_memory[addr + 9] != 0;
        if (active && strncmp((const char*)&logical_memory[addr], name, 8) == 0) {
            write_compact_value(addr, val);
            return;
        }
    }
    for (int i = 0; i < MAX_VARIABLES; ++i) {
        uint16_t addr = MEMORY_VAR_BASE + (i * 16);
        if (logical_memory[addr + 9] == 0) {
            logical_memory[addr + 9] = 1;
            strncpy((char*)&logical_memory[addr], name, 8);
            write_compact_value(addr, val);
            return;
        }
    }
    throw std::runtime_error("Out of Memory: Too many variables");
}

// Array record layout (16 bytes per slot):
//   [0..7]   name (8 bytes)
//   [8]      active flag
//   [9]      ndim (1 = 1D, 2 = 2D)
//   [10..11] dim1 (uint16: rows+1 for 2D, total+1 for 1D)
//   [12..13] dim2 (uint16: cols+1 for 2D, 0 for 1D)
//   [14..15] start_addr (uint16)
struct ArrayRef {
    char name[9];        // up to 8-char name + null terminator
    uint16_t start_addr;
    uint16_t dim1;       // 1D: total elements; 2D: rows
    uint16_t dim2;       // 1D: 0;              2D: cols
    uint8_t  ndim;       // 1 or 2
    bool active;
    uint16_t total_size() const {
        return ndim == 2 ? (uint16_t)(dim1 * dim2) : dim1;
    }
};

// Compute flat index for 1D or 2D array access.
// Pass j=-1 for 1D access.
static int flatten_array_index(const ArrayRef* arr, int i, int j = -1) {
    if (j < 0) {
        if (arr->ndim == 2)
            throw std::runtime_error("Syntax Error: 2D array requires 2 indices A(row,col)");
        if (i < 0 || i >= (int)arr->dim1)
            throw std::runtime_error("Array index out of bounds");
        return i;
    } else {
        if (arr->ndim != 2)
            throw std::runtime_error("Syntax Error: 1D array requires 1 index A(idx)");
        if (i < 0 || i >= (int)arr->dim1 || j < 0 || j >= (int)arr->dim2)
            throw std::runtime_error("Array index out of bounds");
        return i * (int)arr->dim2 + j;
    }
}

static void write_heap_value(uint16_t addr, const Value& val) {
    if (val.type == Value::Type::STR) {
        // Try in-place update if this element already holds a string
        uint8_t old_type = logical_memory[addr];
        if ((Value::Type)old_type == Value::Type::STR) {
            uint16_t old_str_ptr;
            memcpy(&old_str_ptr, &logical_memory[addr + 4], 2);
            if (old_str_ptr >= STRING_HEAP_BASE && old_str_ptr < string_heap_ptr) {
                uint16_t old_alloc;
                memcpy(&old_alloc, &logical_memory[old_str_ptr], 2);
                int new_len = (int)strlen(val.str_val);
                if (old_alloc > 0 && (uint16_t)(new_len + 1) <= old_alloc) {
                    memcpy(&logical_memory[old_str_ptr + 2], val.str_val, (size_t)(new_len + 1));
                    logical_memory[addr] = (uint8_t)val.type;
                    return;
                }
            }
        }
        // New heap slot with 2-byte size prefix
        uint16_t str_ptr = string_heap_ptr;
        int len = (int)strlen(val.str_val);
        uint16_t alloc_size = (uint16_t)(len + 1);
        if ((uint32_t)str_ptr + 2 + alloc_size >= 65536u)
            throw std::runtime_error("String Heap Overflow");
        logical_memory[str_ptr]     = (uint8_t)(alloc_size & 0xFF);
        logical_memory[str_ptr + 1] = (uint8_t)(alloc_size >> 8);
        memcpy(&logical_memory[str_ptr + 2], val.str_val, (size_t)alloc_size);
        memcpy(&logical_memory[addr + 4], &str_ptr, 2);
        string_heap_ptr = (uint16_t)(str_ptr + 2 + alloc_size);
        logical_memory[addr] = (uint8_t)val.type;
        return;
    }
    logical_memory[addr] = (uint8_t)val.type;
    if (val.type == Value::Type::NUM) {
        memcpy(&logical_memory[addr + 4], &val.num_val, 4);
    } else if (val.type == Value::Type::INT) {
        memcpy(&logical_memory[addr + 4], &val.int_val, 4);
    }
}

static Value read_heap_value(uint16_t addr) {
    Value v;
    v.type = (Value::Type)logical_memory[addr];
    if (v.type == Value::Type::NUM) {
        memcpy(&v.num_val, &logical_memory[addr + 4], 4);
    } else if (v.type == Value::Type::INT) {
        memcpy(&v.int_val, &logical_memory[addr + 4], 4);
        v.num_val = (float)v.int_val;
    } else if (v.type == Value::Type::STR) {
        uint16_t str_ptr;
        memcpy(&str_ptr, &logical_memory[addr + 4], 2);
        // Skip 2-byte size prefix to reach string data
        strncpy(v.str_val, (const char*)&logical_memory[str_ptr + 2], 127);
        v.str_val[127] = '\0';
    }
    return v;
}

static ArrayRef* get_array(const char* name) {
    static ArrayRef temp_arr;
    for (int i = 0; i < MAX_VARIABLES; ++i) {
        uint16_t addr = ARRAY_TABLE_BASE + (i * 16);
        bool active = logical_memory[addr + 8] != 0;
        if (active && strncmp((const char*)&logical_memory[addr], name, 8) == 0) {
            strncpy(temp_arr.name, (const char*)&logical_memory[addr], 8);
            temp_arr.name[8] = '\0';
            temp_arr.active  = true;
            temp_arr.ndim    = logical_memory[addr + 9];
            memcpy(&temp_arr.dim1,       &logical_memory[addr + 10], 2);
            memcpy(&temp_arr.dim2,       &logical_memory[addr + 12], 2);
            memcpy(&temp_arr.start_addr, &logical_memory[addr + 14], 2);
            return &temp_arr;
        }
    }
    return nullptr;
}

// Memory Utilities
static uint16_t find_program_line(int line_number) {
    uint16_t ptr = MEMORY_TEXT_BASE;
    // Empty program check
    if (logical_memory[ptr] == 0 && logical_memory[ptr+1] == 0 && 
        logical_memory[ptr+2] == 0 && logical_memory[ptr+3] == 0) return 0xFFFF;

    while (true) {
        if (logical_memory[ptr+2] == 0 && logical_memory[ptr+3] == 0 && ptr != MEMORY_TEXT_BASE) return 0xFFFF;
        uint16_t current_line = logical_memory[ptr+2] | (logical_memory[ptr+3] << 8);
        if (current_line == line_number) return ptr;
        uint16_t next_ptr = logical_memory[ptr] | (logical_memory[ptr+1] << 8);
        if (next_ptr == 0) return 0xFFFF;
        ptr = next_ptr;
    }
}

static uint16_t get_next_program_line(int line_number) {
    uint16_t ptr = MEMORY_TEXT_BASE;
    while (true) {
        uint16_t next_ptr = logical_memory[ptr] | (logical_memory[ptr+1] << 8);
        if (next_ptr == 0) return 0xFFFF;
        uint16_t current_line = logical_memory[ptr+2] | (logical_memory[ptr+3] << 8);
        if (current_line > line_number) return ptr;
        ptr = next_ptr;
    }
}

static uint16_t get_end_of_text() {
    uint16_t ptr = MEMORY_TEXT_BASE;
    while (true) {
        uint16_t next_ptr = logical_memory[ptr] | (logical_memory[ptr+1] << 8);
        if (next_ptr == 0) return ptr;
        ptr = next_ptr;
    }
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
           strcmp(name, "SGN") == 0 || strcmp(name, "SQR") == 0 ||
           strcmp(name, "SIN") == 0 || strcmp(name, "COS") == 0 || strcmp(name, "TAN") == 0 ||
           strcmp(name, "LOG") == 0 || strcmp(name, "EXP") == 0 ||
           strcmp(name, "LEN") == 0 || strcmp(name, "MID$") == 0 || 
           strcmp(name, "LEFT$") == 0 || strcmp(name, "RIGHT$") == 0 ||
           strcmp(name, "CHR$") == 0 || strcmp(name, "ASC") == 0 ||
           strcmp(name, "VAL") == 0 || strcmp(name, "STR$") == 0 ||
           strcmp(name, "TOUCH") == 0;
}

static Value evaluate_builtin_function(const char* var_name, Value* args, int arg_count) {
    if (strcmp(var_name, "ABS") == 0) {
        if (arg_count != 1 || args[0].type != Value::Type::NUM) throw std::runtime_error("Type Mismatch/Arg Count in ABS");
        return Value(std::abs(args[0].num_val));
    } else if (strcmp(var_name, "INT") == 0) {
        if (arg_count != 1 || args[0].type != Value::Type::NUM) throw std::runtime_error("Type Mismatch/Arg Count in INT");
        return Value(std::floor(args[0].num_val));
    } else if (strcmp(var_name, "SGN") == 0) {
        if (arg_count != 1 || args[0].type != Value::Type::NUM) throw std::runtime_error("Type Mismatch/Arg Count in SGN");
        return Value((args[0].num_val > 0) ? 1.0f : (args[0].num_val < 0) ? -1.0f : 0.0f);
    } else if (strcmp(var_name, "SQR") == 0) {
        if (arg_count != 1 || args[0].type != Value::Type::NUM) throw std::runtime_error("Type Mismatch/Arg Count in SQR");
        if (args[0].num_val < 0) throw std::runtime_error("Math Error: SQR of negative number");
        return Value(std::sqrt(args[0].num_val));
    } else if (strcmp(var_name, "SIN") == 0) {
        if (arg_count != 1 || args[0].type != Value::Type::NUM) throw std::runtime_error("Type Mismatch/Arg Count in SIN");
        return Value(std::sin(args[0].num_val));
    } else if (strcmp(var_name, "COS") == 0) {
        if (arg_count != 1 || args[0].type != Value::Type::NUM) throw std::runtime_error("Type Mismatch/Arg Count in COS");
        return Value(std::cos(args[0].num_val));
    } else if (strcmp(var_name, "TAN") == 0) {
        if (arg_count != 1 || args[0].type != Value::Type::NUM) throw std::runtime_error("Type Mismatch/Arg Count in TAN");
        return Value(std::tan(args[0].num_val));
    } else if (strcmp(var_name, "LOG") == 0) {
        if (arg_count != 1 || args[0].type != Value::Type::NUM) throw std::runtime_error("Type Mismatch/Arg Count in LOG");
        if (args[0].num_val <= 0) throw std::runtime_error("Math Error: LOG of non-positive number");
        return Value(std::log(args[0].num_val));
    } else if (strcmp(var_name, "EXP") == 0) {
        if (arg_count != 1 || args[0].type != Value::Type::NUM) throw std::runtime_error("Type Mismatch/Arg Count in EXP");
        return Value(std::exp(args[0].num_val));
    } else if (strcmp(var_name, "RND") == 0) {
        if (arg_count != 1 || args[0].type != Value::Type::NUM) throw std::runtime_error("Type Mismatch/Arg Count in RND");
        float n = args[0].num_val;
        if (n < 0) {
            srand(static_cast<unsigned int>(std::abs(n)));
            last_rnd_val = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
            return Value(last_rnd_val);
        } else if (n == 0) {
            return Value(last_rnd_val);
        } else {
            last_rnd_val = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
            return Value(last_rnd_val * n);
        }
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
        if (start >= (int)strlen(s)) return Value("");
        char buf[128];
        snprintf(buf, sizeof(buf), "%.*s", len, s + start);
        return Value(buf);
    } else if (strcmp(var_name, "LEFT$") == 0) {
        if (arg_count != 2 || args[0].type != Value::Type::STR || args[1].type != Value::Type::NUM)
            throw std::runtime_error("Type Mismatch/Arg Count in LEFT$");
        int len = static_cast<int>(args[1].num_val);
        char* s = args[0].str_val;
        if (len < 0) len = 0;
        if (len >= (int)strlen(s)) return Value(s);
        char buf[128];
        snprintf(buf, sizeof(buf), "%.*s", len, s);
        return Value(buf);
    } else if (strcmp(var_name, "RIGHT$") == 0) {
        if (arg_count != 2 || args[0].type != Value::Type::STR || args[1].type != Value::Type::NUM)
            throw std::runtime_error("Type Mismatch/Arg Count in RIGHT$");
        int len = static_cast<int>(args[1].num_val);
        char* s = args[0].str_val;
        int s_len = strlen(s);
        if (len < 0) len = 0;
        if (len >= s_len) return Value(s);
        return Value(s + s_len - len);
    } else if (strcmp(var_name, "CHR$") == 0) {
        if (arg_count != 1 || args[0].type != Value::Type::NUM) throw std::runtime_error("Type Mismatch/Arg Count in CHR$");
        char buf[2] = {(char)args[0].num_val, '\0'};
        return Value(buf);
    } else if (strcmp(var_name, "ASC") == 0) {
        if (arg_count != 1 || args[0].type != Value::Type::STR) throw std::runtime_error("Type Mismatch/Arg Count in ASC");
        if (args[0].str_val[0] == '\0') return Value(0.0f);
        return Value((float)static_cast<unsigned char>(args[0].str_val[0]));
    } else if (strcmp(var_name, "VAL") == 0) {
        if (arg_count != 1 || args[0].type != Value::Type::STR) throw std::runtime_error("Type Mismatch/Arg Count in VAL");
        return Value((float)atof(args[0].str_val));
    } else if (strcmp(var_name, "STR$") == 0) {
        if (arg_count != 1 || args[0].type != Value::Type::NUM) throw std::runtime_error("Type Mismatch/Arg Count in STR$");
        char buf[32];
        snprintf(buf, sizeof(buf), "%g", args[0].num_val);
        return Value(buf);
    } else if (strcmp(var_name, "TOUCH") == 0) {
        if (arg_count != 1 || args[0].type != Value::Type::NUM) throw std::runtime_error("Type Mismatch/Arg Count in TOUCH");
        int n = static_cast<int>(args[0].num_val);
        switch (n) {
            case 0: return Value((float)hal_touch_get_x());
            case 1: return Value((float)hal_touch_get_y());
            case 2: return Value((float)hal_touch_is_touched());
            default: throw std::runtime_error("TOUCH argument must be 0, 1, or 2");
        }
    }
    throw std::runtime_error("Unknown Function");
}

// ---------------------------------------------------------
// Recursive Descent Parser for Expressions
// ---------------------------------------------------------

static Value parse_power_expr(const TokenList& tokens, int& pos);

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
        if (v.type == Value::Type::STR) throw std::runtime_error("Type Mismatch: Unary - on string");
        if (v.type == Value::Type::INT) return Value(-v.int_val);
        return Value(-v.num_val);
    }
    if (t.type == TokenType::NUMBER) {
        pos++;
        float fv = (float)atof(t.text);
        // 小数点なし、かつ整数範囲内なら INT 型として扱う
        bool has_dot = false;
        for (int i = 0; t.text[i]; i++) if (t.text[i] == '.') { has_dot = true; break; }
        if (!has_dot && fv >= -2147483648.0f && fv <= 2147483647.0f)
            return Value((int)(int)fv);
        return Value(fv);
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

            // Fallback: 1D or 2D array lookup
            ArrayRef* arr = get_array(var_name);
            if (!arr) throw std::runtime_error("Array not dimensioned");
            int flat_idx;
            if (arg_count == 1) {
                if (args[0].type != Value::Type::NUM)
                    throw std::runtime_error("Type Mismatch: Array index must be numeric");
                flat_idx = flatten_array_index(arr, static_cast<int>(args[0].num_val));
            } else if (arg_count == 2) {
                if (args[0].type != Value::Type::NUM || args[1].type != Value::Type::NUM)
                    throw std::runtime_error("Type Mismatch: Array index must be numeric");
                flat_idx = flatten_array_index(arr,
                    static_cast<int>(args[0].num_val),
                    static_cast<int>(args[1].num_val));
            } else {
                throw std::runtime_error("Syntax Error: Arrays support 1 or 2 dimensions");
            }
            return read_heap_value(arr->start_addr + (flat_idx * 8));
        }
        
        // Scalar variable
        Value v_val;
        if (!get_variable(var_name, v_val)) {
            int len = strlen(var_name);
            if (len > 0 && var_name[len - 1] == '$') return Value("");
            return Value(0.0f);
        }
        return v_val;
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

static Value parse_power_expr(const TokenList& tokens, int& pos) {
    Value val = parse_factor(tokens, pos);
    while (pos < tokens.size) {
        if (tokens.tokens[pos].type != TokenType::POWER) break;
        pos++;
        Value next_val = parse_factor(tokens, pos);
        if (val.type == Value::Type::STR || next_val.type == Value::Type::STR) throw std::runtime_error("Type Mismatch: Power on string");
        val.num_val = std::pow(val.num_val, next_val.num_val);
        val.type = Value::Type::NUM;
    }
    return val;
}

static Value parse_term(const TokenList& tokens, int& pos) {
    Value val = parse_power_expr(tokens, pos);
    while (pos < tokens.size) {
        TokenType op = tokens.tokens[pos].type;
        if (op != TokenType::MUL && op != TokenType::DIV) break;
        pos++;
        Value next_val = parse_power_expr(tokens, pos);
        
        if (val.type == Value::Type::STR || next_val.type == Value::Type::STR) {
            throw std::runtime_error("Type Mismatch: Cannot multiply/divide strings");
        }
        
        if (op == TokenType::MUL) {
            val.num_val *= next_val.num_val;
        } else {
            if (next_val.num_val == 0.0f) throw std::runtime_error("Division by zero");
            val.num_val /= next_val.num_val;
        }
        val.type = Value::Type::NUM;
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
            } else if (val.type != Value::Type::STR && next_val.type != Value::Type::STR) {
                val.num_val += next_val.num_val;
                val.type = Value::Type::NUM;
            } else {
                throw std::runtime_error("Type Mismatch: Cannot add string and number");
            }
        } else {
            if (val.type == Value::Type::STR || next_val.type == Value::Type::STR) throw std::runtime_error("Type Mismatch: Cannot subtract strings");
            val.num_val -= next_val.num_val;
            val.type = Value::Type::NUM;
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
            
            if ((val.type == Value::Type::STR) != (next_val.type == Value::Type::STR)) throw std::runtime_error("Type Mismatch: Cannot compare string and number");
            
            bool result = false;
            if (val.type != Value::Type::STR) {
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
    bool newline = true;
    while (pos < tokens.size && tokens.tokens[pos].type != TokenType::END_OF_FILE && 
           tokens.tokens[pos].type != TokenType::ELSE && tokens.tokens[pos].type != TokenType::COLON) {
        if (tokens.tokens[pos].type == TokenType::COMMA) {
            strncat(output, "\t", sizeof(output) - strlen(output) - 1);
            pos++;
            newline = false;
        } else if (tokens.tokens[pos].type == TokenType::SEMICOLON) {
            pos++;
            newline = false;
        } else if (tokens.tokens[pos].type == TokenType::IDENTIFIER && strcmp(tokens.tokens[pos].text, "TAB") == 0) {
            pos++;
            require_token(tokens, pos, TokenType::LPAREN, "Expected '(' after TAB"); pos++;
            Value v = parse_relation(tokens, pos);
            require_token(tokens, pos, TokenType::RPAREN, "Expected ')' after TAB"); pos++;
            
            // For TAB, we need to flush current output and then move cursor
            printf("%s", output);
            hal_display_print(output);
            output[0] = '\0';
            hal_display_locate(static_cast<int>(v.num_val), -1);
            newline = false;
        } else {
            Value result = parse_relation(tokens, pos);
            strncat(output, result.c_str(), sizeof(output) - strlen(output) - 1);
            newline = true;
        }
    }
    if (newline) strncat(output, "\n", sizeof(output) - strlen(output) - 1);
    printf("%s", output);
    hal_display_print(output);
}

static void execute_clear() {
    memset(&logical_memory[MEMORY_VAR_BASE], 0, VAR_TABLE_SIZE);
    memset(&logical_memory[ARRAY_TABLE_BASE], 0, ARRAY_TABLE_SIZE);
    string_heap_ptr = STRING_HEAP_BASE;
    array_heap_inner_ptr = DATA_HEAP_BASE;
}

static void execute_wait(const TokenList& tokens, int& pos) {
    pos++; // skip WAIT
    Value val = parse_relation(tokens, pos);
    hal_system_wait(static_cast<int>(val.num_val));
}

static void execute_locate(const TokenList& tokens, int& pos) {
    pos++; // skip LOCATE
    Value vx = parse_relation(tokens, pos);
    require_token(tokens, pos, TokenType::COMMA, "Expected ','");
    pos++;
    Value vy = parse_relation(tokens, pos);
    hal_display_locate(static_cast<int>(vx.num_val), static_cast<int>(vy.num_val));
}

static void execute_read(const TokenList& tokens, int& pos) {
    pos++; // skip READ
    while (pos < tokens.size && tokens.tokens[pos].type != TokenType::END_OF_FILE) {
        require_token(tokens, pos, TokenType::IDENTIFIER, "Syntax Error: READ expects identifier");
        char var_name[64];
        strncpy(var_name, tokens.tokens[pos].text, sizeof(var_name)-1);
        var_name[sizeof(var_name)-1] = '\0';
        pos++;
        
        int arr_idx = -1, arr_idx2 = -1;
        if (pos < tokens.size && tokens.tokens[pos].type == TokenType::LPAREN) {
            pos++;
            Value idx_val = parse_relation(tokens, pos);
            if (idx_val.type != Value::Type::NUM) throw std::runtime_error("Type Mismatch: Array index");
            arr_idx = static_cast<int>(idx_val.num_val);
            if (pos < tokens.size && tokens.tokens[pos].type == TokenType::COMMA) {
                pos++;
                Value idx2_val = parse_relation(tokens, pos);
                if (idx2_val.type != Value::Type::NUM) throw std::runtime_error("Type Mismatch: Array index");
                arr_idx2 = static_cast<int>(idx2_val.num_val);
            }
            require_token(tokens, pos, TokenType::RPAREN, "Syntax Error: Expected ')'");
            pos++;
        }
        
        if (data_ptr >= data_buffer_size) throw std::runtime_error("Out of DATA");
        Value val = data_buffer[data_ptr++];
        
        int nlen = strlen(var_name);
        bool is_str_var = (nlen > 0 && var_name[nlen-1] == '$');
        bool is_int_var = (nlen > 0 && var_name[nlen-1] == '%');
        if (is_str_var && val.type != Value::Type::STR) throw std::runtime_error("Type Mismatch in READ (Expected String)");
        if (!is_str_var && val.type == Value::Type::STR) throw std::runtime_error("Type Mismatch in READ (Expected Number)");
        if (is_int_var) val = Value((int)val.num_val);
        else if (!is_str_var && val.type == Value::Type::INT) val = Value(val.num_val);
        
        if (arr_idx >= 0) {
            ArrayRef* arr = get_array(var_name);
            if (!arr) throw std::runtime_error("Array not dimensioned");
            int flat_idx = flatten_array_index(arr, arr_idx, arr_idx2);
            write_heap_value(arr->start_addr + (flat_idx * 8), val);
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
    if (find_program_line(current_line) == 0xFFFF) throw std::runtime_error("GOTO target line not found");
    branch_taken = true;
}

static void execute_gosub(const TokenList& tokens, int& pos) {
    pos++; // skip GOSUB
    Value line_to_go = parse_relation(tokens, pos);
    if (line_to_go.type != Value::Type::NUM) throw std::runtime_error("Type Mismatch: GOSUB requires number");
    int target = static_cast<int>(line_to_go.num_val);
    if (find_program_line(target) == 0xFFFF) throw std::runtime_error("GOSUB target line not found");
    if (call_stack_ptr >= MAX_CALL_STACK) throw std::runtime_error("Out of Memory: Call Stack Limit Reached");
    call_stack[call_stack_ptr++] = current_line;
    current_line = target;
    branch_taken = true;
}

static void execute_return(const TokenList& tokens, int& pos) {
    pos++; // skip RETURN
    if (call_stack_ptr == 0) throw std::runtime_error("RETURN WITHOUT GOSUB");
    int returned_from = call_stack[--call_stack_ptr];
    uint16_t returned_idx = find_program_line(returned_from);
    if (returned_idx != 0xFFFF) {
        uint16_t next_idx = get_next_program_line(returned_from);
        if (next_idx != 0xFFFF) {
            current_line = logical_memory[next_idx+2] | (logical_memory[next_idx+3] << 8);
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
        if (pos < tokens.size && tokens.tokens[pos].type == TokenType::ELSE) {
            // Skip ELSE branch and everything after it on this line
            pos = tokens.size;
        }
    } else {
        // Skip until ELSE or End of line
        while (pos < tokens.size && tokens.tokens[pos].type != TokenType::ELSE && tokens.tokens[pos].type != TokenType::END_OF_FILE) {
            pos++;
        }
        if (pos < tokens.size && tokens.tokens[pos].type == TokenType::ELSE) {
            pos++; // Consume ELSE
            execute_statement(tokens, pos);
        } else {
            // No ELSE: condition is false, skip rest of this line and fall through to next
            pos = tokens.size;
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
    
    Value v_val;
    if (get_variable(ctx.var_name, v_val)) {
        v_val.num_val += ctx.step;
        set_variable(ctx.var_name, v_val);
    } else {
        throw std::runtime_error("Intern Error: FOR variable missing");
    }
    
    bool cont = (ctx.step > 0) ? (v_val.num_val <= ctx.target) : (v_val.num_val >= ctx.target);
    if (cont) {
        branch_taken = true;
        uint16_t idx = get_next_program_line(ctx.loop_start_line);
        if (idx != 0xFFFF) current_line = logical_memory[idx+2] | (logical_memory[idx+3] << 8);
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
    Value size1_val = parse_relation(tokens, pos);
    if (size1_val.type != Value::Type::NUM || size1_val.num_val < 0)
        throw std::runtime_error("Syntax Error: Invalid Array Size");
    int dim1_count = static_cast<int>(size1_val.num_val) + 1;

    // Check for second dimension (2D array)
    int dim2_count = 0;
    if (pos < tokens.size && tokens.tokens[pos].type == TokenType::COMMA) {
        pos++;
        Value size2_val = parse_relation(tokens, pos);
        if (size2_val.type != Value::Type::NUM || size2_val.num_val < 0)
            throw std::runtime_error("Syntax Error: Invalid Array Size");
        dim2_count = static_cast<int>(size2_val.num_val) + 1;
    }

    require_token(tokens, pos, TokenType::RPAREN, "Syntax Error: Expected ')' in DIM");
    pos++;

    int ndim           = (dim2_count > 0) ? 2 : 1;
    int arr_size_count = (ndim == 2) ? dim1_count * dim2_count : dim1_count;
    uint16_t total_bytes = (uint16_t)(arr_size_count * 8);
    if (array_heap_inner_ptr + total_bytes > STRING_HEAP_BASE)
        throw std::runtime_error("Out of Memory: Array Heap Full");

    // Find existing or allocate new array table slot
    uint16_t table_addr = 0xFFFF;
    for (int i = 0; i < MAX_VARIABLES; ++i) {
        uint16_t addr = ARRAY_TABLE_BASE + (i * 16);
        if (logical_memory[addr + 8] != 0 &&
            strncmp((const char*)&logical_memory[addr], var_name, 8) == 0) {
            table_addr = addr;
            break;
        }
    }
    if (table_addr == 0xFFFF) {
        for (int i = 0; i < MAX_VARIABLES; ++i) {
            uint16_t addr = ARRAY_TABLE_BASE + (i * 16);
            if (logical_memory[addr + 8] == 0) {
                table_addr = addr;
                break;
            }
        }
    }
    if (table_addr == 0xFFFF) throw std::runtime_error("Out of Memory: Too many arrays");

    uint16_t start_addr  = array_heap_inner_ptr;
    uint16_t dim1_u16    = (uint16_t)dim1_count;
    uint16_t dim2_u16    = (uint16_t)dim2_count;

    // Write array record (new layout)
    logical_memory[table_addr + 8] = 1;              // active
    logical_memory[table_addr + 9] = (uint8_t)ndim;  // ndim
    strncpy((char*)&logical_memory[table_addr], var_name, 8);
    memcpy(&logical_memory[table_addr + 10], &dim1_u16,    2);
    memcpy(&logical_memory[table_addr + 12], &dim2_u16,    2);
    memcpy(&logical_memory[table_addr + 14], &start_addr,  2);

    array_heap_inner_ptr += total_bytes;

    // Zero the freshly allocated data area so old garbage doesn't misfire
    // the in-place string update check in write_heap_value.
    memset(&logical_memory[start_addr], 0, total_bytes);

    int nlen = strlen(var_name);
    Value default_val = (nlen > 0 && var_name[nlen-1] == '$') ? Value("") : Value(0.0f);
    for (int i = 0; i < arr_size_count; i++)
        write_heap_value(start_addr + (i * 8), default_val);
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
    
    int arr_idx = -1, arr_idx2 = -1;
    if (pos < tokens.size && tokens.tokens[pos].type == TokenType::LPAREN) {
        pos++;
        Value idx_val = parse_relation(tokens, pos);
        if (idx_val.type != Value::Type::NUM) throw std::runtime_error("Type Mismatch: Array index");
        arr_idx = static_cast<int>(idx_val.num_val);
        if (pos < tokens.size && tokens.tokens[pos].type == TokenType::COMMA) {
            pos++;
            Value idx2_val = parse_relation(tokens, pos);
            if (idx2_val.type != Value::Type::NUM) throw std::runtime_error("Type Mismatch: Array index");
            arr_idx2 = static_cast<int>(idx2_val.num_val);
        }
        require_token(tokens, pos, TokenType::RPAREN, "Syntax Error: Expected ')'");
        pos++;
    }
    
    require_token(tokens, pos, TokenType::ASSIGN, "Syntax Error: Expected assignment");
    pos++;
    Value result = parse_relation(tokens, pos);
    
    int nlen = strlen(var_name);
    bool is_str_var = (nlen > 0 && var_name[nlen-1] == '$');
    bool is_int_var = (nlen > 0 && var_name[nlen-1] == '%');
    
    if (is_str_var && result.type != Value::Type::STR) throw std::runtime_error("Type Mismatch: Assigning NUM to STR variable");
    if (!is_str_var && result.type == Value::Type::STR) throw std::runtime_error("Type Mismatch: Assigning STR to NUM variable");

    if (is_int_var) {
        result = Value((int)result.num_val);
    } else if (!is_str_var && result.type == Value::Type::INT) {
        result = Value(result.num_val);
    }

    if (arr_idx >= 0) {
        ArrayRef* arr = get_array(var_name);
        if (!arr) throw std::runtime_error("Array not dimensioned");
        int flat_idx = flatten_array_index(arr, arr_idx, arr_idx2);
        write_heap_value(arr->start_addr + (flat_idx * 8), result);
    } else {
        set_variable(var_name, result);
    }
}

static void execute_input(const TokenList& tokens, int& pos) {
    pos++; // skip INPUT
    if (pos < tokens.size && tokens.tokens[pos].type == TokenType::STRING) {
        hal_display_print(tokens.tokens[pos].text);
        printf("%s", tokens.tokens[pos].text);
        pos++;
        // BASIC often uses ';' or ',' here
        if (pos < tokens.size && (tokens.tokens[pos].type == TokenType::COMMA || tokens.tokens[pos].type == TokenType::SEMICOLON)) pos++;
    } else {
        hal_display_print("? ");
        printf("? ");
    }
    
    while (pos < tokens.size && tokens.tokens[pos].type != TokenType::END_OF_FILE) {
        require_token(tokens, pos, TokenType::IDENTIFIER, "Syntax Error: INPUT expects identifier");
        char var_name[64];
        strncpy(var_name, tokens.tokens[pos].text, sizeof(var_name)-1);
        var_name[sizeof(var_name)-1] = '\0';
        pos++;
        
        int arr_idx = -1, arr_idx2 = -1;
        if (pos < tokens.size && tokens.tokens[pos].type == TokenType::LPAREN) {
            pos++;
            Value idx_val = parse_relation(tokens, pos);
            if (idx_val.type != Value::Type::NUM) throw std::runtime_error("Type Mismatch: Array index");
            arr_idx = static_cast<int>(idx_val.num_val);
            if (pos < tokens.size && tokens.tokens[pos].type == TokenType::COMMA) {
                pos++;
                Value idx2_val = parse_relation(tokens, pos);
                if (idx2_val.type != Value::Type::NUM) throw std::runtime_error("Type Mismatch: Array index");
                arr_idx2 = static_cast<int>(idx2_val.num_val);
            }
            require_token(tokens, pos, TokenType::RPAREN, "Syntax Error: Expected ')'");
            pos++;
        }
        
        char in_buf[128] = "";
        hal_display_input(in_buf, sizeof(in_buf));
        
        int nlen = strlen(var_name);
        bool is_str_var = (nlen > 0 && var_name[nlen-1] == '$');
        bool is_int_var = (nlen > 0 && var_name[nlen-1] == '%');
        Value val;
        if (is_str_var) {
            val = Value(in_buf);
        } else if (is_int_var) {
            val = Value((int)atof(in_buf));
        } else {
            val = Value((float)atof(in_buf));
        }
        
        if (arr_idx >= 0) {
            ArrayRef* arr = get_array(var_name);
            if (!arr) throw std::runtime_error("Array not dimensioned");
            int flat_idx = flatten_array_index(arr, arr_idx, arr_idx2);
            write_heap_value(arr->start_addr + (flat_idx * 8), val);
        } else {
            set_variable(var_name, val);
        }
        
        if (pos < tokens.size && tokens.tokens[pos].type == TokenType::COMMA) pos++;
        else break;
    }
}

static void execute_end(const TokenList& tokens, int& pos) {
    pos = tokens.size;
    current_line = -1;
    branch_taken = true;
}

static void execute_stop(const TokenList& tokens, int& pos) {
    char buf[64];
    snprintf(buf, sizeof(buf), "Break in %d\n", current_line);
    hal_display_print(buf);
    printf("%s", buf);
    pos = tokens.size;
    current_line = -1;
    branch_taken = true;
}

static void execute_not_implemented(const TokenList& tokens, int& pos) {
    char buf[128];
    snprintf(buf, sizeof(buf), "Notice: Command '%s' is registered but not yet implemented.\n", tokens.tokens[pos].text);
    hal_display_print(buf);
    printf("%s", buf);
    pos = tokens.size; // Skip to the end of statement/line
}

// Switchboard
static void execute_color(const TokenList& tokens, int& pos) {
    pos++; // skip COLOR
    Value val = parse_relation(tokens, pos);
    if (val.type != Value::Type::NUM && val.type != Value::Type::INT) 
        throw std::runtime_error("Type Mismatch: COLOR expects number");
    int idx = static_cast<int>(val.num_val);
    if (idx < 0 || idx > 15) throw std::runtime_error("Invalid color index (0-15)");
    current_color_565 = PALETTE[idx];
}

static void execute_pset(const TokenList& tokens, int& pos) {
    pos++; // skip PSET
    Value vx = parse_relation(tokens, pos);
    if (pos < tokens.size && tokens.tokens[pos].type == TokenType::COMMA) pos++;
    Value vy = parse_relation(tokens, pos);
    
    uint16_t color = current_color_565;
    if (pos < tokens.size && tokens.tokens[pos].type == TokenType::COMMA) {
        pos++;
        Value vc = parse_relation(tokens, pos);
        int idx = static_cast<int>(vc.num_val);
        if (idx >= 0 && idx <= 15) color = PALETTE[idx];
    }
    hal_graphics_pset(static_cast<int>(vx.num_val), static_cast<int>(vy.num_val), color);
}

static void execute_line(const TokenList& tokens, int& pos) {
    pos++; // skip LINE
    Value vx1 = parse_relation(tokens, pos);
    if (pos < tokens.size && tokens.tokens[pos].type == TokenType::COMMA) pos++;
    Value vy1 = parse_relation(tokens, pos);
    if (pos < tokens.size && tokens.tokens[pos].type == TokenType::COMMA) pos++;
    Value vx2 = parse_relation(tokens, pos);
    if (pos < tokens.size && tokens.tokens[pos].type == TokenType::COMMA) pos++;
    Value vy2 = parse_relation(tokens, pos);
    
    uint16_t color = current_color_565;
    if (pos < tokens.size && tokens.tokens[pos].type == TokenType::COMMA) {
        pos++;
        Value vc = parse_relation(tokens, pos);
        int idx = static_cast<int>(vc.num_val);
        if (idx >= 0 && idx <= 15) color = PALETTE[idx];
    }
    hal_graphics_line(static_cast<int>(vx1.num_val), static_cast<int>(vy1.num_val), 
                      static_cast<int>(vx2.num_val), static_cast<int>(vy2.num_val), color);
}

static void execute_circle(const TokenList& tokens, int& pos) {
    pos++; // skip CIRCLE
    Value vx = parse_relation(tokens, pos);
    if (pos < tokens.size && tokens.tokens[pos].type == TokenType::COMMA) pos++;
    Value vy = parse_relation(tokens, pos);
    if (pos < tokens.size && tokens.tokens[pos].type == TokenType::COMMA) pos++;
    Value vr = parse_relation(tokens, pos);
    
    uint16_t color = current_color_565;
    if (pos < tokens.size && tokens.tokens[pos].type == TokenType::COMMA) {
        pos++;
        Value vc = parse_relation(tokens, pos);
        int idx = static_cast<int>(vc.num_val);
        if (idx >= 0 && idx <= 15) color = PALETTE[idx];
    }
    hal_graphics_circle(static_cast<int>(vx.num_val), static_cast<int>(vy.num_val), 
                        static_cast<int>(vr.num_val), color);
}

static void execute_gpio(const TokenList& tokens, int& pos) {
    pos++; // skip GPIO
    Value vpin = parse_relation(tokens, pos);
    require_token(tokens, pos, TokenType::COMMA, "Expected ','");
    pos++;
    Value vmode = parse_relation(tokens, pos);
    
    int pin = static_cast<int>(vpin.num_val);
    int mode = static_cast<int>(vmode.num_val); // 0=In, 1=Out
    int value = 0;
    int pull = 0;

    if (pos < tokens.size && tokens.tokens[pos].type == TokenType::COMMA) {
        pos++;
        Value vval = parse_relation(tokens, pos);
        value = static_cast<int>(vval.num_val);
    }
    if (pos < tokens.size && tokens.tokens[pos].type == TokenType::COMMA) {
        pos++;
        Value vpull = parse_relation(tokens, pos);
        pull = static_cast<int>(vpull.num_val);
    }

    hal_gpio_init(pin, mode, pull);
    if (mode == 1) { // Output
        hal_gpio_write(pin, value != 0);
    }
}

static void execute_brightness(const TokenList& tokens, int& pos) {
    pos++; // skip BRIGHTNESS
    Value level = parse_relation(tokens, pos);
    hal_display_set_brightness(static_cast<int>(level.num_val));
}

static void execute_paint(const TokenList& tokens, int& pos) {
    pos++; // skip PAINT
    require_token(tokens, pos, TokenType::LPAREN, "Expected '('"); pos++;
    Value vx = parse_relation(tokens, pos);
    require_token(tokens, pos, TokenType::COMMA, "Expected ','"); pos++;
    Value vy = parse_relation(tokens, pos);
    require_token(tokens, pos, TokenType::RPAREN, "Expected ')'"); pos++;
    require_token(tokens, pos, TokenType::COMMA, "Expected ','"); pos++;
    Value vc = parse_relation(tokens, pos);
    
    int x = static_cast<int>(vx.num_val);
    int y = static_cast<int>(vy.num_val);
    int color_idx = static_cast<int>(vc.num_val);
    if (color_idx < 0 || color_idx > 15) throw std::runtime_error("Invalid color index");
    uint16_t fill_color = PALETTE[color_idx];
    
    uint16_t border_color = 0xFFFF;
    bool stop_at_border = false;
    if (pos < tokens.size && tokens.tokens[pos].type == TokenType::COMMA) {
        pos++;
        Value vb = parse_relation(tokens, pos);
        int b_idx = static_cast<int>(vb.num_val);
        if (b_idx >= 0 && b_idx <= 15) {
            border_color = PALETTE[b_idx];
            stop_at_border = true;
        }
    }

    uint16_t target_color = hal_graphics_get_pixel(x, y);
    if (target_color == fill_color) return;
    if (stop_at_border && target_color == border_color) return;

    // Scanline Flood Fill (DFS-based Spans)
    struct Point { int x, y; };
    static Point stack[4096];
    int sp = 0;
    
    auto push = [&](int px, int py) {
        if (sp < 4096 && px >= 0 && px < 320 && py >= 0 && py < 240) {
            if (hal_graphics_get_pixel(px, py) == target_color) {
                stack[sp++] = {px, py};
            }
        }
    };
    
    push(x, y);
    
    while (sp > 0) {
        Point p = stack[--sp];
        int px = p.x;
        int py = p.y;
        
        if (hal_graphics_get_pixel(px, py) != target_color) continue;
        
        // Find left and right boundaries
        int lx = px;
        while (lx > 0 && hal_graphics_get_pixel(lx - 1, py) == target_color) lx--;
        int rx = px;
        while (rx < 319 && hal_graphics_get_pixel(rx + 1, py) == target_color) rx++;
        
        // Fill the scanline span
        for (int i = lx; i <= rx; i++) {
            hal_graphics_pset(i, py, fill_color);
        }
        
        // Scan above and below
        for (int i = lx; i <= rx; i++) {
            if (py > 0 && hal_graphics_get_pixel(i, py - 1) == target_color) {
                // Only push if it's the start of a new span
                if (i == lx || hal_graphics_get_pixel(i - 1, py - 1) != target_color) {
                    push(i, py - 1);
                }
            }
            if (py < 239 && hal_graphics_get_pixel(i, py + 1) == target_color) {
                if (i == lx || hal_graphics_get_pixel(i - 1, py + 1) != target_color) {
                    push(i, py + 1);
                }
            }
        }
    }
    
    hal_display_sync();
}

static void execute_get_at(const TokenList& tokens, int& pos) {
    pos++; // skip GET@
    require_token(tokens, pos, TokenType::LPAREN, "Expected '('"); pos++;
    Value vx1 = parse_relation(tokens, pos);
    require_token(tokens, pos, TokenType::COMMA, "Expected ','"); pos++;
    Value vy1 = parse_relation(tokens, pos);
    require_token(tokens, pos, TokenType::RPAREN, "Expected ')'"); pos++;
    require_token(tokens, pos, TokenType::MINUS, "Expected '-'"); pos++;
    require_token(tokens, pos, TokenType::LPAREN, "Expected '('"); pos++;
    Value vx2 = parse_relation(tokens, pos);
    require_token(tokens, pos, TokenType::COMMA, "Expected ','"); pos++;
    Value vy2 = parse_relation(tokens, pos);
    require_token(tokens, pos, TokenType::RPAREN, "Expected ')'"); pos++;
    require_token(tokens, pos, TokenType::COMMA, "Expected ','"); pos++;
    
    if (pos >= tokens.size || tokens.tokens[pos].type != TokenType::IDENTIFIER)
        throw std::runtime_error("Expected array name");
    
    char array_name[64];
    strncpy(array_name, tokens.tokens[pos].text, sizeof(array_name)-1);
    pos++;
    
    ArrayRef* arr = get_array(array_name);
    if (!arr) throw std::runtime_error("Array not dimensioned");
    
    int x1 = static_cast<int>(vx1.num_val), y1 = static_cast<int>(vy1.num_val);
    int x2 = static_cast<int>(vx2.num_val), y2 = static_cast<int>(vy2.num_val);
    int w = abs(x2 - x1) + 1, h = abs(y2 - y1) + 1;
    
    if (2 + w * h > arr->total_size()) throw std::runtime_error("Array too small for image");
    
    write_heap_value(arr->start_addr, Value((float)w));
    write_heap_value(arr->start_addr + 8, Value((float)h));
    
    int idx = 2;
    for (int j = 0; j < h; j++) {
        for (int i = 0; i < w; i++) {
            uint16_t color = hal_graphics_get_pixel(x1 + i, y1 + j);
            write_heap_value(arr->start_addr + (idx++ * 8), Value((float)color));
        }
    }
}

static void execute_put_at(const TokenList& tokens, int& pos) {
    pos++; // skip PUT@
    require_token(tokens, pos, TokenType::LPAREN, "Expected '('"); pos++;
    Value vx = parse_relation(tokens, pos);
    require_token(tokens, pos, TokenType::COMMA, "Expected ','"); pos++;
    Value vy = parse_relation(tokens, pos);
    require_token(tokens, pos, TokenType::RPAREN, "Expected ')'"); pos++;
    require_token(tokens, pos, TokenType::COMMA, "Expected ','"); pos++;
    
    if (pos >= tokens.size || tokens.tokens[pos].type != TokenType::IDENTIFIER)
        throw std::runtime_error("Expected array name");
    
    char array_name[64];
    strncpy(array_name, tokens.tokens[pos].text, sizeof(array_name)-1);
    pos++;
    
    ArrayRef* arr = get_array(array_name);
    if (!arr) throw std::runtime_error("Array not dimensioned");
    
    int w = static_cast<int>(read_heap_value(arr->start_addr).num_val);
    int h = static_cast<int>(read_heap_value(arr->start_addr + 8).num_val);
    int px = static_cast<int>(vx.num_val), py = static_cast<int>(vy.num_val);
    
    int idx = 2;
    for (int j = 0; j < h; j++) {
        for (int i = 0; i < w; i++) {
            uint16_t color = static_cast<uint16_t>(read_heap_value(arr->start_addr + (idx++ * 8)).num_val);
            hal_graphics_pset(px + i, py + j, color);
        }
    }
    hal_display_sync();
}

static void execute_save(const TokenList& tokens, int& pos) {
    pos++; // skip SAVE
    require_token(tokens, pos, TokenType::STRING, "Syntax Error: SAVE expects filename string");
    const char* filename = tokens.tokens[pos].text;
    pos++;
    
    void* fp = hal_file_open(filename, "w");
    if (!fp) throw std::runtime_error("File Error: Cannot open file for writing");
    
    uint16_t ptr = MEMORY_TEXT_BASE;
    while (true) {
        // Terminator check
        if (logical_memory[ptr+2] == 0 && logical_memory[ptr+3] == 0 && ptr != MEMORY_TEXT_BASE) break;

        uint16_t line_num = logical_memory[ptr+2] | (logical_memory[ptr+3] << 8);
        hal_file_printf(fp, "%d ", line_num);
        
        TokenList t = detokenize(&logical_memory[ptr+4]);
        for (int i=0; i<t.size; i++) {
            if (t.tokens[i].type == TokenType::END_OF_FILE) break;
            if (t.tokens[i].type == TokenType::STRING) hal_file_printf(fp, "\"%s\" ", t.tokens[i].text);
            else hal_file_printf(fp, "%s ", t.tokens[i].text);
        }
        hal_file_printf(fp, "\n");
        
        uint16_t next_ptr = logical_memory[ptr] | (logical_memory[ptr+1] << 8);
        if (next_ptr == 0) break;
        ptr = next_ptr;
    }
    hal_file_close(fp);
    hal_display_print("Saved\n");
}

static void execute_load(const TokenList& tokens, int& pos) {
    pos++; // skip LOAD
    require_token(tokens, pos, TokenType::STRING, "Syntax Error: LOAD expects filename string");
    const char* filename = tokens.tokens[pos].text;
    pos++;
    
    void* fp = hal_file_open(filename, "r");
    if (!fp) throw std::runtime_error("File Error: Cannot open file for reading");
    
    clear_program();
    char line_buf[256];
    while (hal_file_gets(line_buf, sizeof(line_buf), fp)) {
        TokenList t = lex(line_buf);
        if (t.size > 0 && t.tokens[0].type == TokenType::NUMBER) {
            int line_num = atoi(t.tokens[0].text);
            TokenList remainder;
            int j = 0;
            for (int i = 1; i < t.size; i++) {
                if (t.tokens[i].type == TokenType::END_OF_FILE) break;
                remainder.tokens[j++] = t.tokens[i];
            }
            remainder.size = j;
            store_line(line_num, remainder);
        }
    }
    hal_file_close(fp);
    hal_display_print("Loaded\n");
}

static void execute_kill(const TokenList& tokens, int& pos) {
    pos++; // skip KILL
    require_token(tokens, pos, TokenType::STRING, "Syntax Error: KILL expects filename string");
    const char* filename = tokens.tokens[pos].text;
    pos++;
    
    if (hal_file_remove(filename) != 0) {
        throw std::runtime_error("File Error: Cannot delete file");
    }
    hal_display_print("Deleted\n");
}

static void execute_name(const TokenList& tokens, int& pos) {
    pos++; // skip NAME
    require_token(tokens, pos, TokenType::STRING, "Syntax Error: NAME expects old filename string");
    char oldname[128];
    strncpy(oldname, tokens.tokens[pos].text, sizeof(oldname)-1);
    pos++;
    
    require_token(tokens, pos, TokenType::AS, "Syntax Error: Expected AS in NAME command");
    pos++;
    
    require_token(tokens, pos, TokenType::STRING, "Syntax Error: NAME expects new filename string");
    const char* newname = tokens.tokens[pos].text;
    pos++;
    
    if (hal_file_rename(oldname, newname) != 0) {
        throw std::runtime_error("File Error: Cannot rename file");
    }
    hal_display_print("Renamed\n");
}

static void execute_files(const TokenList& tokens, int& pos) {
    pos++; // skip FILES
    
    void* dir = hal_dir_open(".");
    if (dir == NULL) {
        hal_display_print("Error: Cannot open directory\n");
        return;
    }
    
    const char* d_name;
    int count = 0;
    char buf[128];
    while ((d_name = hal_dir_read(dir)) != NULL) {
        // Skip hidden files
        if (d_name[0] == '.') continue;
        
        snprintf(buf, sizeof(buf), "%-16s", d_name);
        hal_display_print(buf);
        count++;
        if (count % 4 == 0) hal_display_print("\n");
    }
    if (count % 4 != 0) hal_display_print("\n");
    
    hal_dir_close(dir);
    snprintf(buf, sizeof(buf), "%d File(s) found\n", count);
    hal_display_print(buf);
}

static void execute_on(const TokenList& tokens, int& pos) {
    pos++; // skip ON
    Value idx_val = parse_relation(tokens, pos);
    int idx = static_cast<int>(idx_val.num_val);
    
    if (pos >= tokens.size) throw std::runtime_error("Syntax Error: Expected GOTO/GOSUB after ON");
    TokenType type = tokens.tokens[pos].type;
    if (type != TokenType::GOTO && type != TokenType::GOSUB)
        throw std::runtime_error("Syntax Error: Expected GOTO or GOSUB");
    pos++;
    
    int current_idx = 1;
    bool found = false;
    while (pos < tokens.size) {
        require_token(tokens, pos, TokenType::NUMBER, "Expected line number");
        int target = atoi(tokens.tokens[pos].text);
        pos++;
        
        if (current_idx == idx) {
            if (type == TokenType::GOTO) {
                current_line = target;
                branch_taken = true;
                found = true;
                break;
            } else {
                if (call_stack_ptr >= MAX_CALL_STACK) throw std::runtime_error("GOSUB Stack Overflow");
                call_stack[call_stack_ptr++] = current_line;
                current_line = target;
                branch_taken = true;
                found = true;
                break;
            }
        }
        
        if (pos < tokens.size && tokens.tokens[pos].type == TokenType::COMMA) {
            pos++;
            current_idx++;
        } else break;
    }
    
    // If not found, just consume remaining line numbers if any
    if (!found) {
        while (pos < tokens.size && tokens.tokens[pos].type == TokenType::COMMA) {
            pos++;
            if (pos < tokens.size && tokens.tokens[pos].type == TokenType::NUMBER) pos++;
            else break;
        }
    }
}

static void execute_mid_statement(const TokenList& tokens, int& pos) {
    pos++; // skip MID$
    require_token(tokens, pos, TokenType::LPAREN, "Expected '('"); pos++;
    
    char var_name[64];
    require_token(tokens, pos, TokenType::IDENTIFIER, "Expected string variable name");
    strncpy(var_name, tokens.tokens[pos].text, sizeof(var_name)-1);
    pos++;
    
    require_token(tokens, pos, TokenType::COMMA, "Expected ','"); pos++;
    Value v_start = parse_relation(tokens, pos);
    int start = static_cast<int>(v_start.num_val) - 1;
    
    int len = -1;
    if (tokens.tokens[pos].type == TokenType::COMMA) {
        pos++;
        Value v_len = parse_relation(tokens, pos);
        len = static_cast<int>(v_len.num_val);
    }
    
    require_token(tokens, pos, TokenType::RPAREN, "Expected ')'"); pos++;
    require_token(tokens, pos, TokenType::ASSIGN, "Expected '='"); pos++;
    
    Value replacement = parse_relation(tokens, pos);
    if (replacement.type != Value::Type::STR) throw std::runtime_error("Type Mismatch: Expected string replacement");
    
    Value original;
    if (!get_variable(var_name, original) || original.type != Value::Type::STR)
        throw std::runtime_error("Variable Error: String variable not found");
    
    int orig_len = strlen(original.str_val);
    if (start < 0) start = 0;
    if (start >= orig_len) return; // Nothing to replace
    
    int replace_len = strlen(replacement.str_val);
    if (len != -1 && len < replace_len) replace_len = len;
    if (start + replace_len > orig_len) replace_len = orig_len - start;
    
    memcpy(original.str_val + start, replacement.str_val, replace_len);
    set_variable(var_name, original); // Write back modified string
}

static void execute_beep(const TokenList& tokens, int& pos) {
    pos++; // skip BEEP
    hal_sound_beep();
}

static void execute_sound(const TokenList& tokens, int& pos) {
    pos++; // skip SOUND
    Value freq_val = parse_relation(tokens, pos);
    require_token(tokens, pos, TokenType::COMMA, "Expected ','"); pos++;
    Value dur_val = parse_relation(tokens, pos);
    
    hal_sound_play(freq_val.num_val, static_cast<int>(dur_val.num_val));
}

static void execute_music(const TokenList& tokens, int& pos) {
    pos++; // skip MUSIC
    Value mml_val = parse_relation(tokens, pos);
    if (mml_val.type != Value::Type::STR) throw std::runtime_error("Type Mismatch: MUSIC expects string");
    
    const char* mml = mml_val.str_val;
    int octave = 4;
    int default_len = 4;
    int tempo = 120;
    int volume = 15;
    
    int i = 0;
    while (mml[i] != '\0') {
        char c = std::toupper(mml[i++]);
        if (std::isspace(c)) continue;
        
        switch (c) {
            case 'O': { // Octave
                int val = 0;
                while (std::isdigit(mml[i])) val = val * 10 + (mml[i++] - '0');
                if (val >= 1 && val <= 8) octave = val;
                break;
            }
            case 'L': { // Length
                int val = 0;
                while (std::isdigit(mml[i])) val = val * 10 + (mml[i++] - '0');
                if (val >= 1 && val <= 64) default_len = val;
                break;
            }
            case 'T': { // Tempo
                int val = 0;
                while (std::isdigit(mml[i])) val = val * 10 + (mml[i++] - '0');
                if (val >= 32 && val <= 255) tempo = val;
                break;
            }
            case 'V': { // Volume
                int val = 0;
                while (std::isdigit(mml[i])) val = val * 10 + (mml[i++] - '0');
                hal_sound_set_volume(val);
                break;
            }
            case '>': octave++; if (octave > 8) octave = 8; break;
            case '<': octave--; if (octave < 1) octave = 1; break;
            
            case 'C': case 'D': case 'E': case 'F': case 'G': case 'A': case 'B':
            case 'R': {
                int note = 0;
                bool is_rest = (c == 'R');
                if (!is_rest) {
                    if (c == 'C') note = 0;
                    else if (c == 'D') note = 2;
                    else if (c == 'E') note = 4;
                    else if (c == 'F') note = 5;
                    else if (c == 'G') note = 7;
                    else if (c == 'A') note = 9;
                    else if (c == 'B') note = 11;
                    
                    // accidental
                    if (mml[i] == '#' || mml[i] == '+') { note++; i++; }
                    else if (mml[i] == '-') { note--; i++; }
                }
                
                int len = default_len;
                if (std::isdigit(mml[i])) {
                    len = 0;
                    while (std::isdigit(mml[i])) len = len * 10 + (mml[i++] - '0');
                }
                
                float duration_ms = (60.0f / tempo) * (4.0f / len) * 1000.0f;
                if (mml[i] == '.') {
                    duration_ms *= 1.5f;
                    i++;
                }
                
                if (is_rest) {
                    hal_sound_play(0, (int)duration_ms);
                } else {
                    float freq = 440.0f * powf(2.0f, (note - 9 + (octave - 4) * 12) / 12.0f);
                    hal_sound_play(freq, (int)duration_ms);
                }
                break;
            }
        }
    }
}

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
        case TokenType::FILES:   execute_files(tokens, pos); break;
        case TokenType::KILL:    execute_kill(tokens, pos); break;
        case TokenType::NAME:    execute_name(tokens, pos); break;
        case TokenType::ON:      execute_on(tokens, pos); break;
        case TokenType::FOR:     execute_for(tokens, pos); break;
        case TokenType::NEXT:    execute_next(tokens, pos); break;
        case TokenType::DIM:     execute_dim(tokens, pos); break;
        case TokenType::INPUT:   execute_input(tokens, pos); break;
        case TokenType::END:     execute_end(tokens, pos); break;
        case TokenType::STOP:    execute_stop(tokens, pos); break;
        case TokenType::LET:     execute_assignment(tokens, pos, true); break;
        case TokenType::IDENTIFIER: 
            if (strcmp(tokens.tokens[pos].text, "MID$") == 0) execute_mid_statement(tokens, pos);
            else execute_assignment(tokens, pos, false); 
            break;
        
        // Phase 2 Extended Commands
        case TokenType::COLOR:   execute_color(tokens, pos); break;
        case TokenType::PSET:    execute_pset(tokens, pos); break;
        case TokenType::LINE:    execute_line(tokens, pos); break;
        case TokenType::CIRCLE:  execute_circle(tokens, pos); break;
        case TokenType::CLS:     pos++; hal_display_cls(); break;
        case TokenType::LOCATE:  execute_locate(tokens, pos); break;
        case TokenType::WAIT:    execute_wait(tokens, pos); break;
        case TokenType::CLEAR:   pos++; execute_clear(); break;
        
        case TokenType::GPIO:    execute_gpio(tokens, pos); break;
        case TokenType::BRIGHTNESS: execute_brightness(tokens, pos); break;
        
        case TokenType::PAINT:   execute_paint(tokens, pos); break;
        case TokenType::GET_AT:  execute_get_at(tokens, pos); break;
        case TokenType::PUT_AT:  execute_put_at(tokens, pos); break;
        
        case TokenType::INIT: case TokenType::NEWON:
        case TokenType::WIDTH: case TokenType::CONSOLE:
        case TokenType::REPEAT: case TokenType::UNTIL: case TokenType::GET:
        case TokenType::WINDOW:
        case TokenType::POLY:    execute_not_implemented(tokens, pos); break;
        case TokenType::BEEP:    execute_beep(tokens, pos); break;
        case TokenType::PLAY:    execute_music(tokens, pos); break; // PLAY is Hu-BASIC alias for MUSIC (MML)
        case TokenType::MUSIC:   execute_music(tokens, pos); break;
        case TokenType::SOUND:   execute_sound(tokens, pos); break;

        default: throw std::runtime_error("Syntax Error: Unrecognized statement");
    }
}

// ---------------------------------------------------------
// Public API
// ---------------------------------------------------------
// Returns true if a program line was stored (suppresses Ready prompt).
bool parse_and_execute(const TokenList& tokens) {
    if (tokens.size == 0 || tokens.tokens[0].type == TokenType::END_OF_FILE) return false;
    
    try {
        if (tokens.tokens[0].type == TokenType::NUMBER) {
            int line_num = atoi(tokens.tokens[0].text);
            TokenList remainder;
            int j = 0;
            for (int i = 1; i < tokens.size; i++) remainder.tokens[j++] = tokens.tokens[i];
            remainder.size = j;
            store_line(line_num, remainder);
            return true; // 行番号入力時はプロンプトを出さない
        } else if (tokens.tokens[0].type == TokenType::NEW) {
            clear_program();
            return false;
        } else if (tokens.tokens[0].type == TokenType::LIST) {
            list_program();
            return false;
        } else if (tokens.tokens[0].type == TokenType::RUN) {
            run_program();
            return false;
        } else if (tokens.tokens[0].type == TokenType::SAVE) {
            int p = 0; execute_save(tokens, p);
            return false;
        } else if (tokens.tokens[0].type == TokenType::LOAD) {
            int p = 0; execute_load(tokens, p);
            return false;
        } else if (tokens.tokens[0].type == TokenType::FILES) {
            int p = 0; execute_files(tokens, p);
            return false;
        }
        int pos = 0;
        branch_taken = false;
        while (pos < tokens.size && tokens.tokens[pos].type != TokenType::END_OF_FILE) {
            execute_statement(tokens, pos);
            if (branch_taken) break;
            if (pos < tokens.size && tokens.tokens[pos].type == TokenType::COLON) {
                pos++;
            } else break;
        }
    } catch (const std::exception& e) {
        char buf[128];
        snprintf(buf, sizeof(buf), "%s\n", e.what());
        printf("%s", buf);
        hal_display_print(buf);
    }
    return false;
}

void store_line(int line_number, const TokenList& tokens) {
    uint16_t ptr = find_program_line(line_number);
    
    uint16_t end_ptr = get_end_of_text() + 2; // +2 for the 0x0000 terminator
    
    // Deletion: if line exists, we delete it first
    if (ptr != 0xFFFF) {
        uint16_t next_ptr = logical_memory[ptr] | (logical_memory[ptr+1] << 8);
        memmove(&logical_memory[ptr], &logical_memory[next_ptr], end_ptr - next_ptr);
        end_ptr -= (next_ptr - ptr);
        // Zero out the new end to maintain terminator
        if (end_ptr + 2 < MEMORY_VAR_BASE) {
            logical_memory[end_ptr] = 0;
            logical_memory[end_ptr+1] = 0;
        }
        update_program_links();
    }

    if (tokens.size == 0 || (tokens.size == 1 && tokens.tokens[0].type == TokenType::END_OF_FILE)) return;

    // Insertion
    uint16_t insert_ptr = MEMORY_TEXT_BASE;
    while (true) {
        uint16_t next_ptr = logical_memory[insert_ptr] | (logical_memory[insert_ptr+1] << 8);
        if (next_ptr == 0) break; // End of list
        uint16_t current_line = logical_memory[insert_ptr+2] | (logical_memory[insert_ptr+3] << 8);
        if (current_line > line_number) break; // Insert before this
        insert_ptr = next_ptr;
    }
    
    uint8_t buffer[256];
    int code_len = tokenize(tokens, buffer);
    int total_len = 4 + code_len;
    
    if (end_ptr + total_len >= MEMORY_VAR_BASE) {
        throw std::runtime_error("Out of Memory: Program too large");
    }
    
    memmove(&logical_memory[insert_ptr + total_len], &logical_memory[insert_ptr], end_ptr - insert_ptr);
    logical_memory[insert_ptr+2] = line_number & 0xFF;
    logical_memory[insert_ptr+3] = (line_number >> 8) & 0xFF;
    memcpy(&logical_memory[insert_ptr+4], buffer, code_len);
    
    // Ensure final terminator is preserved/reset
    uint16_t new_end = end_ptr + total_len;
    if (new_end + 2 < MEMORY_VAR_BASE) {
        logical_memory[new_end] = 0;
        logical_memory[new_end+1] = 0;
    }

    update_program_links();
}

void clear_program() {
    // Clear initial next_ptr and line_num (terminator/empty guard)
    memset(logical_memory, 0, 8);
    
    // Clear variable and array tables
    memset(&logical_memory[MEMORY_VAR_BASE], 0, VAR_TABLE_SIZE + ARRAY_TABLE_SIZE);
    
    string_heap_ptr = STRING_HEAP_BASE;
    array_heap_inner_ptr = DATA_HEAP_BASE;
    
    for_stack_ptr = 0;
    call_stack_ptr = 0;
    data_buffer_size = 0;
    data_ptr = 0;
}

void list_program() {
    uint16_t ptr = MEMORY_TEXT_BASE;
    // Empty program check
    if (logical_memory[ptr] == 0 && logical_memory[ptr+1] == 0 && 
        logical_memory[ptr+2] == 0 && logical_memory[ptr+3] == 0) return;

    char buffer[1024];
    while (ptr < MEMORY_VAR_BASE) {
        // Terminator check
        if (logical_memory[ptr+2] == 0 && logical_memory[ptr+3] == 0 && ptr != MEMORY_TEXT_BASE) break;
        
        uint16_t line_num = logical_memory[ptr+2] | (logical_memory[ptr+3] << 8);
        TokenList tokens = detokenize(&logical_memory[ptr+4]);
        
        int bpos = snprintf(buffer, sizeof(buffer), "%d", line_num);
        for (int i = 0; i < tokens.size; i++) {
            if (tokens.tokens[i].type == TokenType::END_OF_FILE) break;
            bpos += snprintf(buffer + bpos, sizeof(buffer) - bpos, " ");
            if (tokens.tokens[i].type == TokenType::STRING) {
                bpos += snprintf(buffer + bpos, sizeof(buffer) - bpos, "\"%s\"", tokens.tokens[i].text);
            } else {
                bpos += snprintf(buffer + bpos, sizeof(buffer) - bpos, "%s", tokens.tokens[i].text);
            }
        }
        snprintf(buffer + bpos, sizeof(buffer) - bpos, "\n");
        hal_display_print(buffer);
        printf("%s", buffer);
        
        uint16_t next_ptr = logical_memory[ptr] | (logical_memory[ptr+1] << 8);
        if (next_ptr == 0) break;
        ptr = next_ptr;
    }
}

void run_program(int max_steps) {
    for_stack_ptr = 0;
    call_stack_ptr = 0;
    
    // Pre-scan DATA statements
    data_buffer_size = 0;
    data_ptr = 0;
    
    uint16_t ptr = MEMORY_TEXT_BASE;
    while (true) {
        if (logical_memory[ptr+2] == 0 && logical_memory[ptr+3] == 0 && ptr != MEMORY_TEXT_BASE) break;
        TokenList tokens = detokenize(&logical_memory[ptr+4]);
        
        if (tokens.size > 0 && tokens.tokens[0].type == TokenType::DATA) {
            int pos = 1;
            while (pos < tokens.size && tokens.tokens[pos].type != TokenType::END_OF_FILE) {
                if (data_buffer_size < MAX_DATA_BUFFER) {
                    data_buffer[data_buffer_size++] = parse_relation(tokens, pos);
                }
                if (pos < tokens.size && tokens.tokens[pos].type == TokenType::COMMA) pos++;
                else if (pos < tokens.size && tokens.tokens[pos].type != TokenType::END_OF_FILE) break;
            }
        }
        uint16_t next_ptr = logical_memory[ptr] | (logical_memory[ptr+1] << 8);
        if (next_ptr == 0) break;
        ptr = next_ptr;
    }
    
    // Interpreter loop
    ptr = MEMORY_TEXT_BASE;
    if (logical_memory[ptr+2] == 0 && logical_memory[ptr+3] == 0 && ptr != MEMORY_TEXT_BASE) return;
    current_line = logical_memory[ptr+2] | (logical_memory[ptr+3] << 8);
    int steps = 0;

    while (current_line != -1 && (max_steps == -1 || steps < max_steps)) {
        uint16_t line_ptr = find_program_line(current_line);
        if (line_ptr == 0xFFFF) break;
        
        TokenList tokens = detokenize(&logical_memory[line_ptr+4]);
        int pos = 0;
        branch_taken = false;
        
        try {
            while (pos < tokens.size && tokens.tokens[pos].type != TokenType::END_OF_FILE) {
                execute_statement(tokens, pos);
                if (branch_taken) break;
                if (pos < tokens.size && tokens.tokens[pos].type == TokenType::COLON) {
                    pos++;
                } else break;
            }
        } catch (const std::exception& e) {
            char buf[128];
            snprintf(buf, sizeof(buf), "Error in line %d: %s\n", current_line, e.what());
            printf("%s", buf);
            hal_display_print(buf);
            break;
        }
        
        if (!branch_taken) {
            uint16_t next_ptr = logical_memory[line_ptr] | (logical_memory[line_ptr+1] << 8);
            if (next_ptr == 0 || (logical_memory[next_ptr+2] == 0 && logical_memory[next_ptr+3] == 0)) {
                current_line = -1;
            } else {
                current_line = logical_memory[next_ptr+2] | (logical_memory[next_ptr+3] << 8);
            }
        }
        steps++;
    }
}
