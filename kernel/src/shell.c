#include "shell.h"
#include "commands/system.h"
#include "commands/filesystem.h"
#include "terminal.h"
#include "keyboard.h"
#include "string.h"
#include <stddef.h>
#include <stdbool.h>

// Input handling
static char input_buffer[256];
static size_t input_pos = 0;

// Command registry
command_t commands[MAX_COMMANDS];
size_t command_count = 0;

// Register a new command
bool register_command(const char *name, command_func_t func, const char *description, const char *usage, const char *category) {
    if (command_count >= MAX_COMMANDS) {
        return false;
    }
    
    commands[command_count].name = name;
    commands[command_count].func = func;
    commands[command_count].description = description;
    commands[command_count].usage = usage;
    commands[command_count].category = category;
    command_count++;
    
    return true;
}

// Find a command by name
static command_t* find_command(const char *name) {
    for (size_t i = 0; i < command_count; i++) {
        if (strcmp(commands[i].name, name) == 0) {
            return &commands[i];
        }
    }
    return NULL;
}

// Parse and execute commands
void execute_command(const char *cmd) {
    if (strlen(cmd) == 0) {
        return;
    }
    
    // Find the first space to separate command from arguments
    const char *space = cmd;
    while (*space && *space != ' ') {
        space++;
    }
    
    // Extract command name
    char command_name[64];
    size_t cmd_len = space - cmd;
    if (cmd_len >= sizeof(command_name)) {
        terminal_print("Command name too long.\n");
        return;
    }
    
    for (size_t i = 0; i < cmd_len; i++) {
        command_name[i] = cmd[i];
    }
    command_name[cmd_len] = '\0';
    
    // Skip spaces to get arguments
    while (*space == ' ') {
        space++;
    }
    
    // Find and execute the command
    command_t *command = find_command(command_name);
    if (command) {
        command->func((*space != '\0') ? space : NULL);
    } else {
        terminal_print("Unknown command: ");
        terminal_print(command_name);
        terminal_print("\n");
        terminal_print("Type 'help' for available commands.\n");
    }
}

// Initialize shell
void shell_init(void) {
    input_pos = 0;
    command_count = 0;
    
    // Register system commands
    register_system_commands();
    
    // Register filesystem commands
    register_filesystem_commands();
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