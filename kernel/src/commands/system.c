#include "system.h"
#include "../terminal.h"
#include "../string.h"
#include <stddef.h>

// External command registry (defined in shell.c)
extern command_t commands[];
extern size_t command_count;

// System commands
void cmd_help(const char *args) {
    (void)args; // Unused parameter
    
    terminal_print("Available commands:\n\n");
    
    // Collect unique categories
    const char *categories[MAX_COMMANDS];
    size_t category_count = 0;
    
    // Find all unique categories
    for (size_t i = 0; i < command_count; i++) {
        const char *category = commands[i].category;
        bool found = false;
        
        // Check if we already have this category
        for (size_t j = 0; j < category_count; j++) {
            if (strcmp(categories[j], category) == 0) {
                found = true;
                break;
            }
        }
        
        // Add new category if not found
        if (!found) {
            categories[category_count++] = category;
        }
    }
    
    // Display commands grouped by category
    for (size_t cat = 0; cat < category_count; cat++) {
        terminal_print(categories[cat]);
        terminal_print(" Commands:\n");
        
        // Show all commands in this category
        for (size_t i = 0; i < command_count; i++) {
            if (strcmp(commands[i].category, categories[cat]) == 0) {
                terminal_print("  ");
                terminal_print(commands[i].name);
                terminal_print(" - ");
                terminal_print(commands[i].description);
                terminal_print("\n");
            }
        }
        terminal_print("\n");
    }
    
    terminal_print("Type 'help <command>' for detailed usage information.\n");
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

void cmd_version(const char *args) {
    (void)args; // Unused parameter
    terminal_print("DEA OS Version 0.3.1\n");
    terminal_print("Built with dynamic command registry\n");
    terminal_print("Compiler: x86_64-elf-gcc\n");
    terminal_print("Architecture: x86_64\n");
}

void cmd_exit(const char *args) {
    (void)args; // Unused parameter
    terminal_print("Shutting down DEA OS...\n");
    terminal_print("Thank you for using DEA OS!\n");
    terminal_print("System halted. You can now power off safely.\n");
    
    // Disable interrupts and halt the system
    __asm__ volatile (
        "cli\n\t"          // Clear interrupt flag
        "hlt_loop:\n\t"    // Halt loop label
        "hlt\n\t"          // Halt instruction
        "jmp hlt_loop\n\t" // Jump back to halt (in case of spurious interrupt)
        :
        :
        : "memory"
    );
}

// Register all system commands
void register_system_commands(void) {
    register_command("help", cmd_help, "Show available commands", "help [command]", "System");
    register_command("clear", cmd_clear, "Clear the screen", "clear", "System");
    register_command("about", cmd_about, "Show system information", "about", "System");
    register_command("echo", cmd_echo, "Echo text to output", "echo [text]", "System");
    register_command("uptime", cmd_uptime, "Show system uptime", "uptime", "System");
    register_command("exit", cmd_exit, "Exit and halt the system", "exit", "System");
    register_command("version", cmd_version, "Show detailed version info", "version", "Info");
} 