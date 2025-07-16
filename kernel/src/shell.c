#include "shell.h"
#include "terminal.h"
#include "keyboard.h"
#include "string.h"
#include <stddef.h>

// Input handling
static char input_buffer[256];
static size_t input_pos = 0;

// Initialize shell
void shell_init(void) {
    input_pos = 0;
}

// Shell commands
void cmd_help(void) {
    terminal_print("Available commands:\n");
    terminal_print("  help    - Show this help message\n");
    terminal_print("  clear   - Clear the screen\n");
    terminal_print("  about   - Show system information\n");
    terminal_print("  echo    - Echo text\n");
}

void cmd_clear(void) {
    clear_screen();
}

void cmd_about(void) {
    terminal_print("DEA OS - A simple operating system from zero\n");
    terminal_print("Version: 0.1\n");
    terminal_print("Built with love and assembly!\n");
}

void cmd_echo(const char *args) {
    if (args && *args) {
        terminal_print(args);
    }
    terminal_print("\n");
}

// Parse and execute commands
void execute_command(const char *cmd) {
    if (strcmp(cmd, "help") == 0) {
        cmd_help();
    } else if (strcmp(cmd, "clear") == 0) {
        cmd_clear();
    } else if (strcmp(cmd, "about") == 0) {
        cmd_about();
    } else if (strncmp(cmd, "echo ", 5) == 0) {
        cmd_echo(cmd + 5);
    } else if (strcmp(cmd, "echo") == 0) {
        cmd_echo(NULL);
    } else if (strlen(cmd) > 0) {
        terminal_print("Unknown command: ");
        terminal_print(cmd);
        terminal_print("\n");
        terminal_print("Type 'help' for available commands.\n");
    }
}

// Shell main loop
void shell_loop(void) {
    terminal_print("Welcome to DEA OS Shell!\n");
    terminal_print("Type 'help' for available commands.\n\n");
    
    while (1) {
        terminal_print("DEA> ");
        
        // Read command
        input_pos = 0;
        while (1) {
            char c = read_key();
            
            if (c == '\n') {
                input_buffer[input_pos] = '\0';
                terminal_putchar('\n');
                break;
            } else if (c == '\b') {
                if (input_pos > 0) {
                    input_pos--;
                    terminal_putchar('\b');
                }
            } else if (c >= 32 && c <= 126 && input_pos < sizeof(input_buffer) - 1) {
                input_buffer[input_pos++] = c;
                terminal_putchar(c);
            }
        }
        
        // Execute command
        execute_command(input_buffer);
    }
} 