#include "shell.h"
#include "terminal.h"
#include "keyboard.h"
#include "string.h"
#include "fs/filesystem.h"
#include "fs/commands.h"
#include <stddef.h>

// Input handling
static char input_buffer[256];
static size_t input_pos = 0;

// Initialize shell
void shell_init(void) {
    input_pos = 0;
    // Don't initialize filesystem here - it's done in main.c
}

// Shell commands
void cmd_help(void) {
    terminal_print("Available commands:\n");
    terminal_print("System commands:\n");
    terminal_print("  help    - Show this help message\n");
    terminal_print("  clear   - Clear the screen\n");
    terminal_print("  about   - Show system information\n");
    terminal_print("  echo    - Echo text\n");
    terminal_print("\nFile system commands:\n");
    terminal_print("  ls      - List files\n");
    terminal_print("  cat     - Display file contents\n");
    terminal_print("  rm      - Remove file\n");
    terminal_print("  touch   - Create empty file\n");
    terminal_print("  write   - Write text to file\n");
    terminal_print("  df      - Show disk usage\n");
}

void cmd_clear(void) {
    clear_screen();
}

void cmd_about(void) {
    terminal_print("DEA OS - A simple operating system from zero\n");
    terminal_print("Version: 0.2\n");
    terminal_print("Now with filesystem support!\n");
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
    // System commands
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
    }
    // File system commands
    else if (strcmp(cmd, "ls") == 0) {
        cmd_ls(NULL);
    } else if (strncmp(cmd, "cat ", 4) == 0) {
        cmd_cat(cmd + 4);
    } else if (strcmp(cmd, "cat") == 0) {
        cmd_cat(NULL);
    } else if (strncmp(cmd, "rm ", 3) == 0) {
        cmd_rm(cmd + 3);
    } else if (strcmp(cmd, "rm") == 0) {
        cmd_rm(NULL);
    } else if (strncmp(cmd, "touch ", 6) == 0) {
        cmd_touch(cmd + 6);
    } else if (strcmp(cmd, "touch") == 0) {
        cmd_touch(NULL);
    } else if (strncmp(cmd, "write ", 6) == 0) {
        cmd_write(cmd + 6);
    } else if (strcmp(cmd, "write") == 0) {
        cmd_write(NULL);
    } else if (strcmp(cmd, "df") == 0) {
        cmd_df(NULL);
    }
    // Unknown command
    else if (strlen(cmd) > 0) {
        terminal_print("Unknown command: ");
        terminal_print(cmd);
        terminal_print("\n");
        terminal_print("Type 'help' for available commands.\n");
    }
}

// Shell main loop
void shell_loop(void) {
    terminal_print("Welcome to DEA OS Shell!\n");
    terminal_print("Type 'help' for available commands.\n");
    terminal_print("Try 'ls' to see some sample files!\n\n");
    
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