#ifndef SHELL_H
#define SHELL_H

// Shell functions
void shell_init(void);
void shell_loop(void);

// Command functions
void cmd_help(void);
void cmd_clear(void);
void cmd_about(void);
void cmd_echo(const char *args);
void execute_command(const char *cmd);

#endif 