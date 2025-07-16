#ifndef FS_COMMANDS_H
#define FS_COMMANDS_H

#include <stddef.h>

// File system command functions
void cmd_ls(const char *args);
void cmd_cat(const char *args);
void cmd_rm(const char *args);
void cmd_touch(const char *args);
void cmd_df(const char *args);
void cmd_write(const char *args);

// Helper functions
void print_file_size(size_t size);
void print_file_type(int type);

#endif 