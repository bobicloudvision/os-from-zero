#ifndef COMMANDS_SYSTEM_H
#define COMMANDS_SYSTEM_H

#include <stdbool.h>
#include <stddef.h>

#define MAX_COMMANDS 32

// Command function pointer type
typedef void (*command_func_t)(const char *args);

// Command structure
typedef struct {
    const char *name;
    command_func_t func;
    const char *description;
    const char *usage;
    const char *category;
} command_t;

// Command registry function
bool register_command(const char *name, void (*func)(const char *args), const char *description, const char *usage, const char *category);

// Function to register all system commands
void register_system_commands(void);

// System commands
void cmd_help(const char *args);
void cmd_clear(const char *args);
void cmd_about(const char *args);
void cmd_echo(const char *args);
void cmd_uptime(const char *args);
void cmd_version(const char *args);

#endif // COMMANDS_SYSTEM_H 