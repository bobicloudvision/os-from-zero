#include "shell.h"
#include "terminal.h"
#include "keyboard.h"
#include "string.h"
#include "fs/filesystem.h"
#include "fs/commands.h"
#include <stddef.h>
#include <stdbool.h>

// Input handling
static char input_buffer[256];
static size_t input_pos = 0;

// Command registry
static command_t commands[MAX_COMMANDS];
static size_t command_count = 0;

// Register a new command
bool register_command(const char *name, command_func_t func, const char *description, const char *usage) {
    if (command_count >= MAX_COMMANDS) {
        return false;
    }
    
    commands[command_count].name = name;
    commands[command_count].func = func;
    commands[command_count].description = description;
    commands[command_count].usage = usage;
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

// System commands
void cmd_help(const char *args) {
    (void)args; // Unused parameter
    
    terminal_print("Available commands:\n\n");
    
    // Group commands by category
    terminal_print("System Commands:\n");
    for (size_t i = 0; i < command_count; i++) {
        // Check if it's a system command (starts with basic commands)
        if (strcmp(commands[i].name, "help") == 0 || 
            strcmp(commands[i].name, "clear") == 0 || 
            strcmp(commands[i].name, "about") == 0 || 
            strcmp(commands[i].name, "echo") == 0 ||
            strcmp(commands[i].name, "uptime") == 0) {
            terminal_print("  ");
            terminal_print(commands[i].name);
            terminal_print(" - ");
            terminal_print(commands[i].description);
            terminal_print("\n");
        }
    }
    
    terminal_print("\nFile System Commands:\n");
    for (size_t i = 0; i < command_count; i++) {
        // Check if it's a filesystem command
        if (strcmp(commands[i].name, "ls") == 0 || 
            strcmp(commands[i].name, "cat") == 0 || 
            strcmp(commands[i].name, "rm") == 0 || 
            strcmp(commands[i].name, "touch") == 0 || 
            strcmp(commands[i].name, "write") == 0 || 
            strcmp(commands[i].name, "df") == 0) {
            terminal_print("  ");
            terminal_print(commands[i].name);
            terminal_print(" - ");
            terminal_print(commands[i].description);
            terminal_print("\n");
        }
    }
    
    terminal_print("\nType 'help <command>' for detailed usage information.\n");
}

void cmd_clear(const char *args) {
    (void)args; // Unused parameter
    clear_screen();
}

void cmd_about(const char *args) {
    (void)args; // Unused parameter
    terminal_print("DEA OS - A simple operating system from zero\n");
    terminal_print("Version: 0.3\n");
    terminal_print("Now with dynamic command registry!\n");
    terminal_print("Built with love and assembly!\n");
}

void cmd_echo(const char *args) {
    if (args && *args) {
        terminal_print(args);
    }
    terminal_print("\n");
}

// Example of how easy it is to add new commands!
void cmd_uptime(const char *args) {
    (void)args; // Unused parameter
    terminal_print("DEA OS has been running since boot.\n");
    terminal_print("System is stable and responsive!\n");
}

// Register all commands
void register_all_commands(void) {
    // System commands
    register_command("help", cmd_help, "Show available commands", "help [command]");
    register_command("clear", cmd_clear, "Clear the screen", "clear");
    register_command("about", cmd_about, "Show system information", "about");
    register_command("echo", cmd_echo, "Echo text to output", "echo [text]");
    register_command("uptime", cmd_uptime, "Show system uptime", "uptime");
    
    // Register filesystem commands
    register_fs_commands();
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
    
    // Register all commands
    register_all_commands();
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