#include "repl.h"
#include "hal_display.h"
#include "lexer.h"
#include "parser.h"
#include <stdio.h>
#include <cstring>

#define MAX_LINE_LEN 256

void repl_start() {
    static char input_buffer[MAX_LINE_LEN];
    int input_ptr = 0;
    
    // Output initial startup banner
    const char* banner = "Pico-HuBASIC v2.0\n";
    printf("%s", banner);
    hal_display_print(banner);

    while (true) {
        const char* ready = "Ready\n";
        printf("%s", ready);
        hal_display_print(ready);
        
        // Wait for a full line of input
        input_ptr = 0;
        memset(input_buffer, 0, MAX_LINE_LEN);
        
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
                if (input_ptr > 0) {
                    input_ptr--;
                    input_buffer[input_ptr] = '\0';
                    printf("\b \b");
                    // Assuming hal_display_print supports backspace, or we redraw the line
                }
            } else if (c >= 32 && c <= 126) {
                if (input_ptr < MAX_LINE_LEN - 1) {
                    input_buffer[input_ptr++] = static_cast<char>(c);
                    input_buffer[input_ptr] = '\0';
                    putchar(c); // Echo back
                    
                    // Echo to LCD
                    char s[2] = {static_cast<char>(c), '\0'};
                    hal_display_print(s);
                }
            }
        }

        if (input_ptr > 0) {
            // Lexical Analysis (Phase 1) - Input buffer is passed directly
            TokenList tokens = lex(input_buffer);
            
            // Parse & Execute Engine (Phase 1)
            parse_and_execute(tokens);
        }
    }
}
