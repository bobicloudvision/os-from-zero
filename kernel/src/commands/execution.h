#ifndef EXECUTION_COMMANDS_H
#define EXECUTION_COMMANDS_H

// Program execution command functions
void cmd_exec(const char *args);
void cmd_load(const char *args);
void cmd_ps(const char *args);
void cmd_kill(const char *args);
void cmd_compile(const char *args);
void cmd_hello(const char *args);

// Register execution commands
void register_execution_commands(void);

#endif 