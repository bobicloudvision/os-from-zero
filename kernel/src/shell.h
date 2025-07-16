#ifndef SHELL_H
#define SHELL_H

#include <stddef.h>
#include <stdbool.h>
#include "commands/system.h"

// Command registry functions
bool register_command(const char *name, command_func_t func, const char *description, const char *usage, const char *category);
void execute_command(const char *cmd);

// Shell functions
void shell_init(void);
void shell_loop(void);

#endif // SHELL_H 