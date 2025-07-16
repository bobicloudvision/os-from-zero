#ifndef SHELL_H
#define SHELL_H

#include <stddef.h>
#include <stdbool.h>

// Maximum number of commands that can be registered
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

// Shell functions
void shell_init(void);
void shell_loop(void);

// Command registry functions
bool register_command(const char *name, command_func_t func, const char *description, const char *usage, const char *category);
void register_all_commands(void);
void execute_command(const char *cmd);

// Built-in system commands
void cmd_help(const char *args);
void cmd_clear(const char *args);
void cmd_about(const char *args);
void cmd_echo(const char *args);
void cmd_uptime(const char *args);
void cmd_version(const char *args);

#endif 