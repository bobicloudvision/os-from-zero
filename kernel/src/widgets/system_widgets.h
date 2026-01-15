#ifndef SYSTEM_WIDGETS_H
#define SYSTEM_WIDGETS_H

#include <stdint.h>
#include <stdbool.h>
#include "../system_monitor.h"

// Forward declaration - window manager removed
typedef struct window window_t;

// Widget types
typedef enum {
    WIDGET_TYPE_RAM_MONITOR,
    WIDGET_TYPE_CPU_MONITOR,
    WIDGET_TYPE_SYSTEM_INFO
} widget_type_t;

// Widget colors
#define WIDGET_BG_COLOR           0x2D2D2D
#define WIDGET_TEXT_COLOR         0xFFFFFF
#define WIDGET_BORDER_COLOR       0x404040
#define WIDGET_BAR_BG_COLOR       0x404040
#define WIDGET_RAM_BAR_COLOR      0x00AA00
#define WIDGET_RAM_BAR_HIGH_COLOR 0xFFAA00
#define WIDGET_RAM_BAR_FULL_COLOR 0xFF0000
#define WIDGET_CPU_BAR_COLOR      0x0088FF
#define WIDGET_CPU_BAR_HIGH_COLOR 0xFF8800
#define WIDGET_CPU_BAR_BUSY_COLOR 0xFF0000

// Widget dimensions
#define WIDGET_RAM_WIDTH          200
#define WIDGET_RAM_HEIGHT         120
#define WIDGET_CPU_WIDTH          200
#define WIDGET_CPU_HEIGHT         120
#define WIDGET_SYSTEM_WIDTH       250
#define WIDGET_SYSTEM_HEIGHT      150

// Widget refresh rate (in update cycles) - lower = more frequent updates
#define WIDGET_REFRESH_RATE       3

// RAM monitor widget structure
typedef struct {
    window_t *window;
    uint32_t update_counter;
    memory_info_t last_memory_info;
    float memory_history[50];  // History for graphical display
    int history_index;
} ram_widget_t;

// CPU monitor widget structure  
typedef struct {
    window_t *window;
    uint32_t update_counter;
    cpu_info_t last_cpu_info;
    float cpu_history[50];     // History for graphical display
    int history_index;
} cpu_widget_t;

// System info widget structure
typedef struct {
    window_t *window;
    uint32_t update_counter;
} system_info_widget_t;

// Widget management functions
void widgets_init(void);
void widgets_update(void);
void widgets_shutdown(void);

// RAM widget functions
ram_widget_t* create_ram_widget(int x, int y);
void destroy_ram_widget(ram_widget_t *widget);
void update_ram_widget(ram_widget_t *widget);
void draw_ram_widget(window_t *window);

// CPU widget functions
cpu_widget_t* create_cpu_widget(int x, int y);
void destroy_cpu_widget(cpu_widget_t *widget);
void update_cpu_widget(cpu_widget_t *widget);
void draw_cpu_widget(window_t *window);

// System info widget functions
system_info_widget_t* create_system_info_widget(int x, int y);
void destroy_system_info_widget(system_info_widget_t *widget);
void update_system_info_widget(system_info_widget_t *widget);
void draw_system_info_widget(window_t *window);

// Utility functions for widget drawing
void draw_progress_bar(window_t *window, int x, int y, int width, int height, 
                      float percentage, uint32_t bar_color, uint32_t bg_color);
void draw_mini_graph(window_t *window, int x, int y, int width, int height,
                    float *data, int data_count, uint32_t color);
void draw_text_centered(window_t *window, const char *text, int x, int y, 
                       int width, uint32_t color);

// Widget registry (for tracking active widgets)
#define MAX_WIDGETS 10

typedef struct {
    widget_type_t type;
    void *widget_data;
    bool active;
} widget_entry_t;

extern widget_entry_t g_widget_registry[MAX_WIDGETS];
extern int g_widget_count;

// Widget management functions
bool register_widget(widget_type_t type, void *widget_data);
void unregister_widget(void *widget_data);
void update_all_widgets(void);

#endif // SYSTEM_WIDGETS_H 