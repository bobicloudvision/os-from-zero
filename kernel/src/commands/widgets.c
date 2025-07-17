#include "widgets.h"
#include "system.h"
#include "../terminal.h"
#include "../widgets/system_widgets.h"
#include "../system_monitor.h"
#include "../string.h"
#include "../audio.h"
#include <stddef.h>

// Helper function to parse integer from string
static int parse_int(const char *str) {
    int result = 0;
    int sign = 1;
    
    if (*str == '-') {
        sign = -1;
        str++;
    }
    
    while (*str >= '0' && *str <= '9') {
        result = result * 10 + (*str - '0');
        str++;
    }
    
    return result * sign;
}

// Helper function to skip whitespace
static const char *skip_whitespace(const char *str) {
    while (*str == ' ' || *str == '\t') {
        str++;
    }
    return str;
}

// Helper function to parse next word
static const char *parse_word(const char *str, char *word, size_t max_len) {
    str = skip_whitespace(str);
    size_t i = 0;
    
    while (*str && *str != ' ' && *str != '\t' && i < max_len - 1) {
        word[i++] = *str++;
    }
    
    word[i] = '\0';
    return str;
}

// Command: ram widget
void cmd_ram_widget(const char *args) {
    // Parse optional position arguments
    int x = 50, y = 50;
    
    if (args && strlen(args) > 0) {
        char word[32];
        const char *ptr = parse_word(args, word, sizeof(word));
        if (strlen(word) > 0) {
            x = parse_int(word);
            ptr = parse_word(ptr, word, sizeof(word));
            if (strlen(word) > 0) {
                y = parse_int(word);
            }
        }
    }
    
    // Create RAM widget
    ram_widget_t *widget = create_ram_widget(x, y);
    if (widget) {
        terminal_print("RAM monitoring widget created at position (");
        char pos_str[16];
        int_to_string(x, pos_str);
        terminal_print(pos_str);
        terminal_print(", ");
        int_to_string(y, pos_str);
        terminal_print(pos_str);
        terminal_print(")\n");
        terminal_print("The widget shows detailed memory information:\n");
        terminal_print("- Total, used, and free memory amounts\n");
        terminal_print("- Color-coded usage percentage (OK/MED/HIGH)\n");
        terminal_print("- Real-time progress bar and mini graph\n");
        terminal_print("- Live update indicator (green blinking dot)\n");
        
        // Play creation sound
        audio_play_event(AUDIO_SYSTEM_BEEP);
        
        // Refresh display
        wm_draw_all();
    } else {
        terminal_print("Failed to create RAM widget: Out of memory or too many widgets\n");
        audio_play_event(AUDIO_ERROR_BEEP);
    }
}

// Command: cpu widget
void cmd_cpu_widget(const char *args) {
    // Parse optional position arguments
    int x = 270, y = 50; // Default to right of RAM widget
    
    if (args && strlen(args) > 0) {
        char word[32];
        const char *ptr = parse_word(args, word, sizeof(word));
        if (strlen(word) > 0) {
            x = parse_int(word);
            ptr = parse_word(ptr, word, sizeof(word));
            if (strlen(word) > 0) {
                y = parse_int(word);
            }
        }
    }
    
    // Create CPU widget
    cpu_widget_t *widget = create_cpu_widget(x, y);
    if (widget) {
        terminal_print("CPU monitoring widget created at position (");
        char pos_str[16];
        int_to_string(x, pos_str);
        terminal_print(pos_str);
        terminal_print(", ");
        int_to_string(y, pos_str);
        terminal_print(pos_str);
        terminal_print(")\n");
        terminal_print("The widget shows detailed CPU information:\n");
        terminal_print("- Current and average CPU usage percentages\n");
        terminal_print("- CPU frequency and load status (IDLE/LIGHT/BUSY/HIGH)\n");
        terminal_print("- Color-coded progress bar and mini graph\n");
        terminal_print("- Live update indicator (blue blinking dot)\n");
        
        // Play creation sound
        audio_play_event(AUDIO_SYSTEM_BEEP);
        
        // Refresh display
        wm_draw_all();
    } else {
        terminal_print("Failed to create CPU widget: Out of memory or too many widgets\n");
        audio_play_event(AUDIO_ERROR_BEEP);
    }
}

// Command: system widget
void cmd_system_widget(const char *args) {
    // Parse optional position arguments
    int x = 50, y = 190; // Default to below other widgets
    
    if (args && strlen(args) > 0) {
        char word[32];
        const char *ptr = parse_word(args, word, sizeof(word));
        if (strlen(word) > 0) {
            x = parse_int(word);
            ptr = parse_word(ptr, word, sizeof(word));
            if (strlen(word) > 0) {
                y = parse_int(word);
            }
        }
    }
    
    // Create system info widget
    system_info_widget_t *widget = create_system_info_widget(x, y);
    if (widget) {
        terminal_print("System information widget created at position (");
        char pos_str[16];
        int_to_string(x, pos_str);
        terminal_print(pos_str);
        terminal_print(", ");
        int_to_string(y, pos_str);
        terminal_print(pos_str);
        terminal_print(")\n");
        terminal_print("The widget shows comprehensive system info:\n");
        terminal_print("- OS version, architecture, and status\n");
        terminal_print("- Real-time memory summary with usage percentage\n");
        terminal_print("- CPU details with current load status\n");
        terminal_print("- Live uptime counter and system status\n");
        terminal_print("- Orange blinking dot indicates live updates\n");
        
        // Play creation sound
        audio_play_event(AUDIO_SYSTEM_BEEP);
        
        // Refresh display
        wm_draw_all();
    } else {
        terminal_print("Failed to create system info widget: Out of memory or too many widgets\n");
        audio_play_event(AUDIO_ERROR_BEEP);
    }
}

// Command: widget list
void cmd_widget_list(const char *args) {
    (void)args; // Unused parameter
    
    terminal_print("Active System Widgets:\n");
    terminal_print("======================\n");
    
    int count = 0;
    for (int i = 0; i < g_widget_count; i++) {
        if (g_widget_registry[i].active) {
            count++;
            terminal_print("Widget ");
            char num_str[16];
            int_to_string(count, num_str);
            terminal_print(num_str);
            terminal_print(": ");
            
            switch (g_widget_registry[i].type) {
                case WIDGET_TYPE_RAM_MONITOR:
                    terminal_print("RAM Monitor");
                    break;
                case WIDGET_TYPE_CPU_MONITOR:
                    terminal_print("CPU Monitor");
                    break;
                case WIDGET_TYPE_SYSTEM_INFO:
                    terminal_print("System Info");
                    break;
                default:
                    terminal_print("Unknown");
                    break;
            }
            terminal_print("\n");
        }
    }
    
    if (count == 0) {
        terminal_print("No widgets are currently active.\n");
        terminal_print("Try: 'ramwidget', 'cpuwidget', or 'syswidget' to create widgets\n");
    } else {
        terminal_print("\nTotal widgets: ");
        char count_str[16];
        int_to_string(count, count_str);
        terminal_print(count_str);
        terminal_print("/");
        int_to_string(MAX_WIDGETS, count_str);
        terminal_print(count_str);
        terminal_print("\n");
    }
}

// Command: widget close (close all widgets)
void cmd_widget_close(const char *args) {
    (void)args; // Unused parameter
    
    int closed_count = 0;
    
    // Close all active widgets
    for (int i = g_widget_count - 1; i >= 0; i--) {
        if (g_widget_registry[i].active) {
            switch (g_widget_registry[i].type) {
                case WIDGET_TYPE_RAM_MONITOR:
                    destroy_ram_widget((ram_widget_t*)g_widget_registry[i].widget_data);
                    break;
                case WIDGET_TYPE_CPU_MONITOR:
                    destroy_cpu_widget((cpu_widget_t*)g_widget_registry[i].widget_data);
                    break;
                case WIDGET_TYPE_SYSTEM_INFO:
                    destroy_system_info_widget((system_info_widget_t*)g_widget_registry[i].widget_data);
                    break;
            }
            closed_count++;
        }
    }
    
    if (closed_count > 0) {
        terminal_print("Closed ");
        char count_str[16];
        int_to_string(closed_count, count_str);
        terminal_print(count_str);
        terminal_print(" widget(s)\n");
        
        // Refresh display
        wm_draw_all();
    } else {
        terminal_print("No widgets to close\n");
    }
}

// Command: sysmon - show system monitoring info in terminal
void cmd_sysmon(const char *args) {
    (void)args; // Unused parameter
    
    terminal_print("System Monitor - Real-time Status\n");
    terminal_print("==================================\n");
    
    // Initialize system monitor if needed
    sysmon_init();
    sysmon_update();
    
    // Show memory information
    memory_info_t mem_info;
    if (sysmon_get_memory_info(&mem_info)) {
        terminal_print("Memory Information:\n");
        terminal_print("  Total Memory: ");
        char mem_str[32];
        sysmon_format_bytes(mem_info.total_memory, mem_str, sizeof(mem_str));
        terminal_print(mem_str);
        terminal_print("\n  Used Memory:  ");
        sysmon_format_bytes(mem_info.used_memory, mem_str, sizeof(mem_str));
        terminal_print(mem_str);
        terminal_print("\n  Free Memory:  ");
        sysmon_format_bytes(mem_info.free_memory, mem_str, sizeof(mem_str));
        terminal_print(mem_str);
        terminal_print("\n  Usage:        ");
        char percent_str[16];
        sysmon_format_percentage(mem_info.usage_percentage, percent_str, sizeof(percent_str));
        terminal_print(percent_str);
        terminal_print("\n\n");
    }
    
    // Show CPU information
    cpu_info_t cpu_info;
    if (sysmon_get_cpu_info(&cpu_info)) {
        terminal_print("CPU Information:\n");
        terminal_print("  Current Usage: ");
        char percent_str[16];
        sysmon_format_percentage(cpu_info.current_usage, percent_str, sizeof(percent_str));
        terminal_print(percent_str);
        terminal_print("\n  Average Usage: ");
        sysmon_format_percentage(cpu_info.average_usage, percent_str, sizeof(percent_str));
        terminal_print(percent_str);
        terminal_print("\n  Frequency:     2.4 GHz\n");
        terminal_print("  Architecture:  x86_64\n\n");
    }
    
    terminal_print("Use 'ramwidget' and 'cpuwidget' to create graphical monitors\n");
}

// Command: meminfo - detailed memory information
void cmd_meminfo(const char *args) {
    (void)args; // Unused parameter
    
    terminal_print("Detailed Memory Information\n");
    terminal_print("===========================\n");
    
    sysmon_init();
    
    memory_info_t mem_info;
    if (sysmon_get_memory_info(&mem_info)) {
        char mem_str[32];
        
        terminal_print("Total System Memory: ");
        sysmon_format_bytes(mem_info.total_memory, mem_str, sizeof(mem_str));
        terminal_print(mem_str);
        terminal_print("\n");
        
        terminal_print("Used Memory:         ");
        sysmon_format_bytes(mem_info.used_memory, mem_str, sizeof(mem_str));
        terminal_print(mem_str);
        terminal_print("\n");
        
        terminal_print("Free Memory:         ");
        sysmon_format_bytes(mem_info.free_memory, mem_str, sizeof(mem_str));
        terminal_print(mem_str);
        terminal_print("\n");
        
        terminal_print("Usage Percentage:    ");
        char percent_str[16];
        sysmon_format_percentage(mem_info.usage_percentage, percent_str, sizeof(percent_str));
        terminal_print(percent_str);
        terminal_print("\n\n");
        
        // Show memory breakdown in KB/MB for easier reading
        uint64_t total_kb = mem_info.total_memory / 1024;
        uint64_t used_kb = mem_info.used_memory / 1024;
        uint64_t free_kb = mem_info.free_memory / 1024;
        
        terminal_print("Memory Breakdown (KB):\n");
        terminal_print("  Total: ");
        char kb_str[16];
        int_to_string(total_kb, kb_str);
        terminal_print(kb_str);
        terminal_print(" KB\n");
        
        terminal_print("  Used:  ");
        int_to_string(used_kb, kb_str);
        terminal_print(kb_str);
        terminal_print(" KB\n");
        
        terminal_print("  Free:  ");
        int_to_string(free_kb, kb_str);
        terminal_print(kb_str);
        terminal_print(" KB\n");
    } else {
        terminal_print("Failed to get memory information\n");
    }
}

// Command: cpuinfo - detailed CPU information
void cmd_cpuinfo(const char *args) {
    (void)args; // Unused parameter
    
    terminal_print("Detailed CPU Information\n");
    terminal_print("========================\n");
    
    sysmon_init();
    
    cpu_info_t cpu_info;
    if (sysmon_get_cpu_info(&cpu_info)) {
        terminal_print("Processor Information:\n");
        terminal_print("  Architecture:    x86_64\n");
        terminal_print("  CPU Family:      Generic x86_64\n");
        terminal_print("  Cores:           1 (simulated)\n");
        terminal_print("  Frequency:       2.4 GHz (estimated)\n\n");
        
        terminal_print("Current Performance:\n");
        terminal_print("  Current Usage:   ");
        char percent_str[16];
        sysmon_format_percentage(cpu_info.current_usage, percent_str, sizeof(percent_str));
        terminal_print(percent_str);
        terminal_print("\n");
        
        terminal_print("  Average Usage:   ");
        sysmon_format_percentage(cpu_info.average_usage, percent_str, sizeof(percent_str));
        terminal_print(percent_str);
        terminal_print("\n\n");
        
        terminal_print("Performance Notes:\n");
        terminal_print("- CPU usage is simulated for demonstration\n");
        terminal_print("- In a real OS, this would measure actual CPU time\n");
        terminal_print("- Usage varies based on system activity\n");
    } else {
        terminal_print("Failed to get CPU information\n");
    }
}

// Command: live demo - create all widgets for demonstration
void cmd_live_demo(const char *args) {
    (void)args; // Unused parameter
    
    terminal_print("Creating Live System Monitoring Demo...\n");
    terminal_print("========================================\n");
    
    // Close any existing widgets first
    cmd_widget_close("");
    
    // Create RAM widget
    ram_widget_t *ram_widget = create_ram_widget(50, 50);
    if (ram_widget) {
        terminal_print("✓ RAM monitoring widget created\n");
    }
    
    // Create CPU widget  
    cpu_widget_t *cpu_widget = create_cpu_widget(270, 50);
    if (cpu_widget) {
        terminal_print("✓ CPU monitoring widget created\n");
    }
    
    // Create system info widget
    system_info_widget_t *sys_widget = create_system_info_widget(50, 190);
    if (sys_widget) {
        terminal_print("✓ System info widget created\n");
    }
    
    terminal_print("\nEnhanced Live Data Features:\n");
    terminal_print("🔴 RAM Widget:\n");
    terminal_print("  - Detailed memory breakdown (Total/Used/Free)\n");
    terminal_print("  - Color-coded status (OK/MED/HIGH)\n");
    terminal_print("  - Green blinking live indicator\n");
    terminal_print("🔵 CPU Widget:\n");
    terminal_print("  - Current and average usage percentages\n");
    terminal_print("  - Load status (IDLE/LIGHT/BUSY/HIGH)\n");
    terminal_print("  - Blue blinking live indicator\n");
    terminal_print("🟠 System Widget:\n");
    terminal_print("  - Live uptime counter\n");
    terminal_print("  - Real-time memory and CPU summaries\n");
    terminal_print("  - Orange blinking live indicator\n");
    terminal_print("\nAll widgets show live, dynamic data!\n");
    terminal_print("Use 'closewidgets' to close all widgets.\n");
    
    // Play demo sound
    audio_play_event(AUDIO_STARTUP_SOUND);
    
    // Refresh display
    wm_draw_all();
}

// Register all widget commands
void register_widget_commands(void) {
    register_command("ramwidget", cmd_ram_widget, "Create RAM usage monitoring widget", 
                    "ramwidget [x y]", "Widgets");
    register_command("cpuwidget", cmd_cpu_widget, "Create CPU usage monitoring widget", 
                    "cpuwidget [x y]", "Widgets");
    register_command("syswidget", cmd_system_widget, "Create system information widget", 
                    "syswidget [x y]", "Widgets");
    register_command("widgets", cmd_widget_list, "List active widgets", 
                    "widgets", "Widgets");
    register_command("closewidgets", cmd_widget_close, "Close all widgets", 
                    "closewidgets", "Widgets");
    register_command("livedemo", cmd_live_demo, "Create live system monitoring demo", 
                    "livedemo", "Widgets");
    register_command("sysmon", cmd_sysmon, "Show system monitoring info", 
                    "sysmon", "System Monitor");
    register_command("meminfo", cmd_meminfo, "Show detailed memory information", 
                    "meminfo", "System Monitor");
    register_command("cpuinfo", cmd_cpuinfo, "Show detailed CPU information", 
                    "cpuinfo", "System Monitor");
} 