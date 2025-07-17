#ifndef COMMANDS_WINDOW_MANAGER_H
#define COMMANDS_WINDOW_MANAGER_H

#include <stdbool.h>
#include "system.h"

// Function to register all window manager commands
void register_window_manager_commands(void);

// Window manager commands
void cmd_window_create(const char *args);
void cmd_window_list(const char *args);
void cmd_window_close(const char *args);
void cmd_window_move(const char *args);
void cmd_window_resize(const char *args);
void cmd_window_focus(const char *args);
void cmd_window_maximize(const char *args);
void cmd_window_minimize(const char *args);
void cmd_window_restore(const char *args);
void cmd_window_info(const char *args);
void cmd_window_demo(const char *args);
void cmd_desktop(const char *args);
void cmd_terminal_mode(const char *args);
void cmd_mouse_test(const char *args);
void cmd_window_debug(const char *args);

#endif // COMMANDS_WINDOW_MANAGER_H 