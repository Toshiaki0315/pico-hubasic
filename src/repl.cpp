#include "repl.h"
#include "hal_display.h"
#include "lexer.h"
#include "parser.h"
#include <stdio.h>
#include <string>

void repl_start() {
    std::string input_line;
    
    // Output initial startup banner
    printf("Pico-HuBASIC v2.0\n");
    hal_display_print("Pico-HuBASIC v2.0\n");

    while (true) {
        printf("Ready\n");
        hal_display_print("Ready\n");
        
        // Wait for a full line of input
        input_line.clear();
        
        while (true) {
            int c = getchar(); // USB CDC Blocking Input
            
            if (c == EOF) {
                continue; // Prevent infinite loop on EOF
            }
            
            // Simple Line Editor implementation (Phase 1)
            if (c == '\r' || c == '\n') {
                printf("\n");
                hal_display_print("\n");
                break;
            } else if (c == '\b' || c == 127) { // Backspace
                if (!input_line.empty()) {
                    input_line.pop_back();
                    printf("\b \b");
                    // Assuming hal_display_print supports backspace, or we redraw the line
                }
            } else if (c >= 32 && c <= 126) {
                input_line += static_cast<char>(c);
                putchar(c); // Echo back
                
                // Echo to LCD
                std::string s(1, static_cast<char>(c));
                hal_display_print(s);
            }
        }

        if (!input_line.empty()) {
            // Lexical Analysis (Phase 1)
            auto tokens = lex(input_line);
            
            // Parse & Execute Engine (Phase 1)
            parse_and_execute(tokens);
        }
    }
}
