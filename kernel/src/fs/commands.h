#ifndef FS_COMMANDS_H
#define FS_COMMANDS_H

#include <stdbool.h>
#include "../system_commands.h"

// Function to register all filesystem commands
void register_fs_commands(void);

// Filesystem commands
void cmd_ls(const char *args);
void cmd_cat(const char *args);
void cmd_rm(const char *args);
void cmd_touch(const char *args);
void cmd_write(const char *args);
void cmd_df(const char *args);

#endif // FS_COMMANDS_H 