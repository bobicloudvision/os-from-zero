#include "system_widgets.h"
#include "../system_monitor.h"
#include "../string.h"
#include "../window_manager_rust.h"
#include <stddef.h>

// Widget registry
widget_entry_t g_widget_registry[MAX_WIDGETS] = {0};
int g_widget_count = 0;

// Static allocation pools for widgets (simple memory management)
static ram_widget_t ram_widget_pool[5];
static cpu_widget_t cpu_widget_pool[5];
static system_info_widget_t system_info_widget_pool[5];
static bool ram_widget_used[5] = {false};
static bool cpu_widget_used[5] = {false};
static bool system_info_widget_used[5] = {false};

// Widget management functions
void widgets_init(void) {
    // Initialize widget pools
    for (int i = 0; i < 5; i++) {
        ram_widget_pool[i].window = NULL;
        ram_widget_pool[i].update_counter = 0;
        ram_widget_pool[i].history_index = 0;
        for (int j = 0; j < 50; j++) {
            ram_widget_pool[i].memory_history[j] = 0.0f;
        }
        
        cpu_widget_pool[i].window = NULL;
        cpu_widget_pool[i].update_counter = 0;
        cpu_widget_pool[i].history_index = 0;
        for (int j = 0; j < 50; j++) {
            cpu_widget_pool[i].cpu_history[j] = 0.0f;
        }
        
        system_info_widget_pool[i].window = NULL;
        system_info_widget_pool[i].update_counter = 0;
    }
    
    // Clear widget registry
    for (int i = 0; i < MAX_WIDGETS; i++) {
        g_widget_registry[i].active = false;
        g_widget_registry[i].widget_data = NULL;
    }
    g_widget_count = 0;
}

void widgets_update(void) {
    // Update system monitor data
    sysmon_update();
    
    // Update all active widgets
    update_all_widgets();
}

void widgets_shutdown(void) {
    // Destroy all active widgets
    for (int i = 0; i < MAX_WIDGETS; i++) {
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
        }
    }
    g_widget_count = 0;
}

// RAM widget functions
ram_widget_t* create_ram_widget(int x, int y) {
    // Find free slot in pool
    int slot = -1;
    for (int i = 0; i < 5; i++) {
        if (!ram_widget_used[i]) {
            slot = i;
            break;
        }
    }
    
    if (slot == -1) {
        return NULL; // No free slots
    }
    
    ram_widget_t *widget = &ram_widget_pool[slot];
    ram_widget_used[slot] = true;
    
    // Create window
    widget->window = wm_create_window("RAM Monitor", x, y, WIDGET_RAM_WIDTH, WIDGET_RAM_HEIGHT,
                                     WINDOW_MOVABLE | WINDOW_CLOSABLE);
    
    if (!widget->window) {
        ram_widget_used[slot] = false;
        return NULL;
    }
    
    // Set custom draw callback
    widget->window->draw_callback = draw_ram_widget;
    widget->window->user_data = widget;
    
    // Initialize widget data
    widget->update_counter = 0;
    widget->history_index = 0;
    for (int i = 0; i < 50; i++) {
        widget->memory_history[i] = 0.0f;
    }
    
    // Register widget
    register_widget(WIDGET_TYPE_RAM_MONITOR, widget);
    
    return widget;
}

void destroy_ram_widget(ram_widget_t *widget) {
    if (!widget) return;
    
    // Unregister widget
    unregister_widget(widget);
    
    // Destroy window
    if (widget->window) {
        wm_destroy_window(widget->window);
        widget->window = NULL;
    }
    
    // Free slot in pool
    for (int i = 0; i < 5; i++) {
        if (&ram_widget_pool[i] == widget) {
            ram_widget_used[i] = false;
            break;
        }
    }
}

void update_ram_widget(ram_widget_t *widget) {
    if (!widget || !widget->window) return;
    
    widget->update_counter++;
    
    // Update every WIDGET_REFRESH_RATE cycles
    if (widget->update_counter % WIDGET_REFRESH_RATE == 0) {
        // Get current memory info
        sysmon_get_memory_info(&widget->last_memory_info);
        
        // Update history
        widget->memory_history[widget->history_index] = widget->last_memory_info.usage_percentage;
        widget->history_index = (widget->history_index + 1) % 50;
        
        // Mark window for redraw
        wm_invalidate_window(widget->window);
    }
}

void draw_ram_widget(window_t *window) {
    if (!window || !window->user_data) return;
    
    ram_widget_t *widget = (ram_widget_t*)window->user_data;
    
    // Clear background
    wm_clear_window(window, WIDGET_BG_COLOR);
    
    // Draw title with live indicator
    wm_draw_text_to_window(window, "RAM Usage", 10, 10, WIDGET_TEXT_COLOR);
    
    // Draw live update indicator (blinking dot)
    if ((widget->update_counter / 5) % 2 == 0) {
        wm_draw_filled_rect_to_window(window, 185, 12, 4, 4, 0x00FF00);
    }
    
    // Get current memory info
    memory_info_t mem_info;
    if (sysmon_get_memory_info(&mem_info)) {
        // Draw total memory
        char total_text[48] = "Total: ";
        char temp_text[32];
        sysmon_format_bytes(mem_info.total_memory, temp_text, sizeof(temp_text));
        int pos = 7; // Skip "Total: "
        const char *src = temp_text;
        while (*src && pos < 47) {
            total_text[pos++] = *src++;
        }
        total_text[pos] = '\0';
        wm_draw_text_to_window(window, total_text, 10, 28, WIDGET_TEXT_COLOR);
        
        // Draw used memory
        char used_text[48] = "Used:  ";
        sysmon_format_bytes(mem_info.used_memory, temp_text, sizeof(temp_text));
        pos = 7; // Skip "Used:  "
        src = temp_text;
        while (*src && pos < 47) {
            used_text[pos++] = *src++;
        }
        used_text[pos] = '\0';
        wm_draw_text_to_window(window, used_text, 10, 42, WIDGET_TEXT_COLOR);
        
        // Draw free memory
        char free_text[48] = "Free:  ";
        sysmon_format_bytes(mem_info.free_memory, temp_text, sizeof(temp_text));
        pos = 7; // Skip "Free:  "
        src = temp_text;
        while (*src && pos < 47) {
            free_text[pos++] = *src++;
        }
        free_text[pos] = '\0';
        wm_draw_text_to_window(window, free_text, 10, 56, WIDGET_TEXT_COLOR);
        
        // Draw percentage with status
        char percent_text[32] = "Usage: ";
        char temp_percent[16];
        sysmon_format_percentage(mem_info.usage_percentage, temp_percent, sizeof(temp_percent));
        pos = 7; // Skip "Usage: "
        src = temp_percent;
        while (*src && pos < 31) {
            percent_text[pos++] = *src++;
        }
        percent_text[pos] = '\0';
        
        // Add status indicator
        uint32_t text_color = WIDGET_TEXT_COLOR;
        if (mem_info.usage_percentage > 80.0f) {
            text_color = 0xFF4444;
            const char *status = " HIGH";
            src = status;
            while (*src && pos < 31) {
                percent_text[pos++] = *src++;
            }
            percent_text[pos] = '\0';
        } else if (mem_info.usage_percentage > 60.0f) {
            text_color = 0xFFAA44;
            const char *status = " MED";
            src = status;
            while (*src && pos < 31) {
                percent_text[pos++] = *src++;
            }
            percent_text[pos] = '\0';
        } else {
            text_color = 0x44FF44;
            const char *status = " OK";
            src = status;
            while (*src && pos < 31) {
                percent_text[pos++] = *src++;
            }
            percent_text[pos] = '\0';
        }
        
        wm_draw_text_to_window(window, percent_text, 10, 70, text_color);
        
        // Draw progress bar
        uint32_t bar_color = WIDGET_RAM_BAR_COLOR;
        if (mem_info.usage_percentage > 80.0f) {
            bar_color = WIDGET_RAM_BAR_FULL_COLOR;
        } else if (mem_info.usage_percentage > 60.0f) {
            bar_color = WIDGET_RAM_BAR_HIGH_COLOR;
        }
        
        draw_progress_bar(window, 10, 84, 180, 12, mem_info.usage_percentage, 
                         bar_color, WIDGET_BAR_BG_COLOR);
        
        // Draw mini graph
        draw_mini_graph(window, 10, 98, 180, 16, widget->memory_history, 50, bar_color);
    }
}

// CPU widget functions
cpu_widget_t* create_cpu_widget(int x, int y) {
    // Find free slot in pool
    int slot = -1;
    for (int i = 0; i < 5; i++) {
        if (!cpu_widget_used[i]) {
            slot = i;
            break;
        }
    }
    
    if (slot == -1) {
        return NULL; // No free slots
    }
    
    cpu_widget_t *widget = &cpu_widget_pool[slot];
    cpu_widget_used[slot] = true;
    
    // Create window
    widget->window = wm_create_window("CPU Monitor", x, y, WIDGET_CPU_WIDTH, WIDGET_CPU_HEIGHT,
                                     WINDOW_MOVABLE | WINDOW_CLOSABLE);
    
    if (!widget->window) {
        cpu_widget_used[slot] = false;
        return NULL;
    }
    
    // Set custom draw callback
    widget->window->draw_callback = draw_cpu_widget;
    widget->window->user_data = widget;
    
    // Initialize widget data
    widget->update_counter = 0;
    widget->history_index = 0;
    for (int i = 0; i < 50; i++) {
        widget->cpu_history[i] = 0.0f;
    }
    
    // Register widget
    register_widget(WIDGET_TYPE_CPU_MONITOR, widget);
    
    return widget;
}

void destroy_cpu_widget(cpu_widget_t *widget) {
    if (!widget) return;
    
    // Unregister widget
    unregister_widget(widget);
    
    // Destroy window
    if (widget->window) {
        wm_destroy_window(widget->window);
        widget->window = NULL;
    }
    
    // Free slot in pool
    for (int i = 0; i < 5; i++) {
        if (&cpu_widget_pool[i] == widget) {
            cpu_widget_used[i] = false;
            break;
        }
    }
}

void update_cpu_widget(cpu_widget_t *widget) {
    if (!widget || !widget->window) return;
    
    widget->update_counter++;
    
    // Update every WIDGET_REFRESH_RATE cycles
    if (widget->update_counter % WIDGET_REFRESH_RATE == 0) {
        // Get current CPU info
        sysmon_get_cpu_info(&widget->last_cpu_info);
        
        // Update history
        widget->cpu_history[widget->history_index] = widget->last_cpu_info.current_usage;
        widget->history_index = (widget->history_index + 1) % 50;
        
        // Mark window for redraw
        wm_invalidate_window(widget->window);
    }
}

void draw_cpu_widget(window_t *window) {
    if (!window || !window->user_data) return;
    
    cpu_widget_t *widget = (cpu_widget_t*)window->user_data;
    
    // Clear background
    wm_clear_window(window, WIDGET_BG_COLOR);
    
    // Draw title with live indicator
    wm_draw_text_to_window(window, "CPU Usage", 10, 10, WIDGET_TEXT_COLOR);
    
    // Draw live update indicator (blinking dot)
    if ((widget->update_counter / 5) % 2 == 0) {
        wm_draw_filled_rect_to_window(window, 185, 12, 4, 4, 0x0088FF);
    }
    
    // Get current CPU info
    cpu_info_t cpu_info;
    if (sysmon_get_cpu_info(&cpu_info)) {
        // Draw current usage percentage
        char current_text[32] = "Current: ";
        char temp_percent[16];
        sysmon_format_percentage(cpu_info.current_usage, temp_percent, sizeof(temp_percent));
        int pos = 9; // Skip "Current: "
        const char *src = temp_percent;
        while (*src && pos < 31) {
            current_text[pos++] = *src++;
        }
        current_text[pos] = '\0';
        wm_draw_text_to_window(window, current_text, 10, 28, WIDGET_TEXT_COLOR);
        
        // Draw average usage
        char avg_text[32] = "Average: ";
        sysmon_format_percentage(cpu_info.average_usage, temp_percent, sizeof(temp_percent));
        pos = 9; // Skip "Average: "
        src = temp_percent;
        while (*src && pos < 31) {
            avg_text[pos++] = *src++;
        }
        avg_text[pos] = '\0';
        wm_draw_text_to_window(window, avg_text, 10, 42, WIDGET_TEXT_COLOR);
        
        // Draw CPU frequency
        wm_draw_text_to_window(window, "Freq: 2.4 GHz", 10, 56, WIDGET_TEXT_COLOR);
        
        // Draw CPU load status with color coding
        char status_text[32] = "Status: ";
        uint32_t status_color = WIDGET_TEXT_COLOR;
        const char *status_msg;
        
        if (cpu_info.current_usage < 20.0f) {
            status_msg = "IDLE";
            status_color = 0x44FF44;
        } else if (cpu_info.current_usage < 50.0f) {
            status_msg = "LIGHT";
            status_color = 0x88FF88;
        } else if (cpu_info.current_usage < 80.0f) {
            status_msg = "BUSY";
            status_color = 0xFFAA44;
        } else {
            status_msg = "HIGH";
            status_color = 0xFF4444;
        }
        
        pos = 8; // Skip "Status: "
        src = status_msg;
        while (*src && pos < 31) {
            status_text[pos++] = *src++;
        }
        status_text[pos] = '\0';
        wm_draw_text_to_window(window, status_text, 10, 70, status_color);
        
        // Draw progress bar
        uint32_t bar_color = WIDGET_CPU_BAR_COLOR;
        if (cpu_info.current_usage > 80.0f) {
            bar_color = WIDGET_CPU_BAR_BUSY_COLOR;
        } else if (cpu_info.current_usage > 60.0f) {
            bar_color = WIDGET_CPU_BAR_HIGH_COLOR;
        }
        
        draw_progress_bar(window, 10, 84, 180, 12, cpu_info.current_usage, 
                         bar_color, WIDGET_BAR_BG_COLOR);
        
        // Draw mini graph
        draw_mini_graph(window, 10, 98, 180, 16, widget->cpu_history, 50, bar_color);
    }
}

// System info widget functions
system_info_widget_t* create_system_info_widget(int x, int y) {
    // Find free slot in pool
    int slot = -1;
    for (int i = 0; i < 5; i++) {
        if (!system_info_widget_used[i]) {
            slot = i;
            break;
        }
    }
    
    if (slot == -1) {
        return NULL; // No free slots
    }
    
    system_info_widget_t *widget = &system_info_widget_pool[slot];
    system_info_widget_used[slot] = true;
    
    // Create window
    widget->window = wm_create_window("System Info", x, y, WIDGET_SYSTEM_WIDTH, WIDGET_SYSTEM_HEIGHT,
                                     WINDOW_MOVABLE | WINDOW_CLOSABLE);
    
    if (!widget->window) {
        system_info_widget_used[slot] = false;
        return NULL;
    }
    
    // Set custom draw callback
    widget->window->draw_callback = draw_system_info_widget;
    widget->window->user_data = widget;
    
    // Initialize widget data
    widget->update_counter = 0;
    
    // Register widget
    register_widget(WIDGET_TYPE_SYSTEM_INFO, widget);
    
    return widget;
}

void destroy_system_info_widget(system_info_widget_t *widget) {
    if (!widget) return;
    
    // Unregister widget
    unregister_widget(widget);
    
    // Destroy window
    if (widget->window) {
        wm_destroy_window(widget->window);
        widget->window = NULL;
    }
    
    // Free slot in pool
    for (int i = 0; i < 5; i++) {
        if (&system_info_widget_pool[i] == widget) {
            system_info_widget_used[i] = false;
            break;
        }
    }
}

void update_system_info_widget(system_info_widget_t *widget) {
    if (!widget || !widget->window) return;
    
    widget->update_counter++;
    
    // Update every WIDGET_REFRESH_RATE cycles
    if (widget->update_counter % WIDGET_REFRESH_RATE == 0) {
        // Mark window for redraw
        wm_invalidate_window(widget->window);
    }
}

void draw_system_info_widget(window_t *window) {
    if (!window || !window->user_data) return;
    
    system_info_widget_t *widget = (system_info_widget_t*)window->user_data;
    
    // Clear background
    wm_clear_window(window, WIDGET_BG_COLOR);
    
    // Draw title with live indicator
    wm_draw_text_to_window(window, "System Information", 10, 10, WIDGET_TEXT_COLOR);
    
    // Draw live update indicator (blinking dot)
    if ((widget->update_counter / 7) % 2 == 0) {
        wm_draw_filled_rect_to_window(window, 235, 12, 4, 4, 0xFFAA00);
    }
    
    // Draw OS info
    wm_draw_text_to_window(window, "OS: DEA OS v0.3.1", 10, 28, WIDGET_TEXT_COLOR);
    wm_draw_text_to_window(window, "Arch: x86_64", 10, 42, WIDGET_TEXT_COLOR);
    
    // Draw memory summary
    memory_info_t mem_info;
    if (sysmon_get_memory_info(&mem_info)) {
        char mem_text[64] = "RAM: ";
        char total_text[32];
        sysmon_format_bytes(mem_info.total_memory, total_text, sizeof(total_text));
        int pos = 5; // Skip "RAM: "
        const char *src = total_text;
        while (*src && pos < 30) {
            mem_text[pos++] = *src++;
        }
        
        // Add usage percentage
        const char *usage_prefix = " (";
        src = usage_prefix;
        while (*src && pos < 62) {
            mem_text[pos++] = *src++;
        }
        
        char percent_text[16];
        sysmon_format_percentage(mem_info.usage_percentage, percent_text, sizeof(percent_text));
        src = percent_text;
        while (*src && pos < 62) {
            mem_text[pos++] = *src++;
        }
        
        mem_text[pos++] = ')';
        mem_text[pos] = '\0';
        
        uint32_t mem_color = WIDGET_TEXT_COLOR;
        if (mem_info.usage_percentage > 80.0f) {
            mem_color = 0xFF4444;
        } else if (mem_info.usage_percentage > 60.0f) {
            mem_color = 0xFFAA44;
        }
        
        wm_draw_text_to_window(window, mem_text, 10, 56, mem_color);
    }
    
    // Draw CPU info with current load
    cpu_info_t cpu_info;
    if (sysmon_get_cpu_info(&cpu_info)) {
        wm_draw_text_to_window(window, "CPU: Intel x86_64", 10, 70, WIDGET_TEXT_COLOR);
        wm_draw_text_to_window(window, "Cores: 1  Freq: 2.4GHz", 10, 84, WIDGET_TEXT_COLOR);
        
        // CPU load with color coding
        char cpu_load_text[48] = "Load: ";
        char temp_percent[16];
        sysmon_format_percentage(cpu_info.current_usage, temp_percent, sizeof(temp_percent));
        int pos = 6; // Skip "Load: "
        const char *src = temp_percent;
        while (*src && pos < 35) {
            cpu_load_text[pos++] = *src++;
        }
        
        const char *load_status;
        uint32_t cpu_color;
        if (cpu_info.current_usage < 25.0f) {
            load_status = " IDLE";
            cpu_color = 0x44FF44;
        } else if (cpu_info.current_usage < 75.0f) {
            load_status = " ACTIVE";
            cpu_color = 0xFFAA44;
        } else {
            load_status = " BUSY";
            cpu_color = 0xFF4444;
        }
        
        src = load_status;
        while (*src && pos < 47) {
            cpu_load_text[pos++] = *src++;
        }
        cpu_load_text[pos] = '\0';
        
        wm_draw_text_to_window(window, cpu_load_text, 10, 98, cpu_color);
    }
    
    // Draw uptime counter (simulated)
    char uptime_text[48] = "Uptime: ";
    uint32_t uptime_seconds = widget->update_counter / 10; // Rough estimate
    uint32_t minutes = uptime_seconds / 60;
    uint32_t seconds = uptime_seconds % 60;
    
    int pos = 8; // Skip "Uptime: "
    
    // Add minutes
    if (minutes > 0) {
        char min_str[16];
        int_to_string(minutes, min_str);
        const char *src = min_str;
        while (*src && pos < 40) {
            uptime_text[pos++] = *src++;
        }
        uptime_text[pos++] = 'm';
        uptime_text[pos++] = ' ';
    }
    
    // Add seconds
    char sec_str[16];
    int_to_string(seconds, sec_str);
    const char *src = sec_str;
    while (*src && pos < 46) {
        uptime_text[pos++] = *src++;
    }
    uptime_text[pos++] = 's';
    uptime_text[pos] = '\0';
    
    wm_draw_text_to_window(window, uptime_text, 10, 112, WIDGET_TEXT_COLOR);
    
    // Draw system status
    wm_draw_text_to_window(window, "Status: RUNNING", 10, 126, 0x44FF44);
}

// Utility functions for widget drawing
void draw_progress_bar(window_t *window, int x, int y, int width, int height, 
                      float percentage, uint32_t bar_color, uint32_t bg_color) {
    if (!window) return;
    
    // Draw background
    wm_draw_filled_rect_to_window(window, x, y, width, height, bg_color);
    
    // Draw border
    wm_draw_rect_to_window(window, x, y, width, height, WIDGET_BORDER_COLOR);
    
    // Draw progress bar
    if (percentage > 0.0f) {
        int bar_width = (int)((percentage / 100.0f) * (width - 2));
        if (bar_width > width - 2) bar_width = width - 2;
        wm_draw_filled_rect_to_window(window, x + 1, y + 1, bar_width, height - 2, bar_color);
    }
}

void draw_mini_graph(window_t *window, int x, int y, int width, int height,
                    float *data, int data_count, uint32_t color) {
    if (!window || !data || data_count <= 0) return;
    
    // Draw background
    wm_draw_filled_rect_to_window(window, x, y, width, height, WIDGET_BAR_BG_COLOR);
    
    // Draw border
    wm_draw_rect_to_window(window, x, y, width, height, WIDGET_BORDER_COLOR);
    
    // Draw grid lines for better readability
    for (int i = 1; i < 4; i++) {
        int grid_y = y + (height * i) / 4;
        for (int grid_x = x + 2; grid_x < x + width - 2; grid_x += 4) {
            wm_draw_pixel_to_window(window, grid_x, grid_y, 0x555555);
        }
    }
    
    // Draw graph data with improved visualization
    int step = width / data_count;
    if (step < 1) step = 1;
    
    for (int i = 0; i < data_count && i * step < width; i++) {
        float value = data[i];
        if (value > 100.0f) value = 100.0f;
        if (value < 0.0f) value = 0.0f;
        
        int bar_height = (int)((value / 100.0f) * (height - 2));
        int bar_x = x + 1 + i * step;
        int bar_y = y + height - 1 - bar_height;
        
        // Dynamic color based on value
        uint32_t bar_color = color;
        if (value > 80.0f) {
            bar_color = 0xFF4444; // Red for high values
        } else if (value > 60.0f) {
            bar_color = 0xFFAA44; // Orange for medium-high values
        }
        
        if (bar_height > 0) {
            wm_draw_filled_rect_to_window(window, bar_x, bar_y, step - 1, bar_height, bar_color);
            
            // Add highlight on the most recent data point
            if (i == data_count - 1 && bar_height > 2) {
                wm_draw_filled_rect_to_window(window, bar_x, bar_y, step - 1, 2, 0xFFFFFF);
            }
        }
    }
}

void draw_text_centered(window_t *window, const char *text, int x, int y, 
                       int width, uint32_t color) {
    if (!window || !text) return;
    
    // Calculate text center position (simplified)
    int text_len = strlen(text);
    int text_x = x + (width - text_len * 8) / 2; // Assume 8px char width
    
    wm_draw_text_to_window(window, text, text_x, y, color);
}

// Widget registry functions
bool register_widget(widget_type_t type, void *widget_data) {
    if (g_widget_count >= MAX_WIDGETS) {
        return false;
    }
    
    g_widget_registry[g_widget_count].type = type;
    g_widget_registry[g_widget_count].widget_data = widget_data;
    g_widget_registry[g_widget_count].active = true;
    g_widget_count++;
    
    return true;
}

void unregister_widget(void *widget_data) {
    for (int i = 0; i < MAX_WIDGETS; i++) {
        if (g_widget_registry[i].active && g_widget_registry[i].widget_data == widget_data) {
            g_widget_registry[i].active = false;
            g_widget_registry[i].widget_data = NULL;
            
            // Compact the registry
            for (int j = i; j < MAX_WIDGETS - 1; j++) {
                g_widget_registry[j] = g_widget_registry[j + 1];
            }
            g_widget_count--;
            break;
        }
    }
}

void update_all_widgets(void) {
    for (int i = 0; i < g_widget_count; i++) {
        if (g_widget_registry[i].active) {
            switch (g_widget_registry[i].type) {
                case WIDGET_TYPE_RAM_MONITOR:
                    update_ram_widget((ram_widget_t*)g_widget_registry[i].widget_data);
                    break;
                case WIDGET_TYPE_CPU_MONITOR:
                    update_cpu_widget((cpu_widget_t*)g_widget_registry[i].widget_data);
                    break;
                case WIDGET_TYPE_SYSTEM_INFO:
                    update_system_info_widget((system_info_widget_t*)g_widget_registry[i].widget_data);
                    break;
            }
        }
    }
} 