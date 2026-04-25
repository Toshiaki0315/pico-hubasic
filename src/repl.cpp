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
    const char* banner = "pico-basic v2.0\n";
    printf("%s", banner);
    hal_display_print(banner);

    bool show_ready = true;

    while (true) {
        if (show_ready) {
            const char* ready = "Ready\n";
            printf("%s", ready);
            hal_display_print(ready);
        }
        
        // Wait for a full line of input
        input_ptr = 0;
        memset(input_buffer, 0, MAX_LINE_LEN);
        
        while (true) {
            int c = getchar(); // USB CDC Blocking Input
            
            if (c == EOF) {
                continue; // Prevent infinite loop on EOF
            }
            
            // Simple Line Editor implementation
            if (c == '\r' || c == '\n') {
                printf("\n");
                hal_display_print("\n");
                break;
            } else if (c == '\b' || c == 127) { // Backspace
                if (input_ptr > 0) {
                    input_ptr--;
                    input_buffer[input_ptr] = '\0';
                    printf("\b \b");
                    hal_display_print("\b \b");
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
            // Lexical Analysis
            TokenList tokens = lex(input_buffer);
            
            // Parse & Execute.
            // Returns true if a line was stored (line-number mode) -> suppress Ready
            bool line_stored = parse_and_execute(tokens);
            show_ready = !line_stored;
        } else {
            show_ready = true;
        }
    }
}
