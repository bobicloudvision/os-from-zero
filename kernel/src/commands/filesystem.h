#ifndef COMMANDS_FILESYSTEM_H
#define COMMANDS_FILESYSTEM_H

#include <stdbool.h>
#include "system.h"

// Function to register all filesystem commands
void register_filesystem_commands(void);

// Filesystem commands
void cmd_ls(const char *args);
void cmd_cat(const char *args);
void cmd_rm(const char *args);
void cmd_touch(const char *args);
void cmd_write(const char *args);
void cmd_df(const char *args);

#endif // COMMANDS_FILESYSTEM_H 