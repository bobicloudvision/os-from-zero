#ifndef COMMANDS_WIDGETS_H
#define COMMANDS_WIDGETS_H

#include <stdbool.h>
#include "system.h"

// Function to register all widget commands
void register_widget_commands(void);

// Widget commands
void cmd_ram_widget(const char *args);
void cmd_cpu_widget(const char *args);
void cmd_system_widget(const char *args);
void cmd_widget_list(const char *args);
void cmd_widget_close(const char *args);
void cmd_live_demo(const char *args);
void cmd_sysmon(const char *args);
void cmd_meminfo(const char *args);
void cmd_cpuinfo(const char *args);

#endif // COMMANDS_WIDGETS_H 