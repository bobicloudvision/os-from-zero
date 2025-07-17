#ifndef WINDOW_MANAGER_H
#define WINDOW_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <limine.h>

// Window system constants
#define MAX_WINDOWS 16
#define MAX_WINDOW_TITLE_LENGTH 64
#define WINDOW_TITLE_BAR_HEIGHT 24
#define WINDOW_BORDER_WIDTH 2
#define WINDOW_MIN_WIDTH 120
#define WINDOW_MIN_HEIGHT 80

// Window state flags
#define WINDOW_VISIBLE      0x01
#define WINDOW_FOCUSED      0x02
#define WINDOW_MINIMIZED    0x04
#define WINDOW_MAXIMIZED    0x08
#define WINDOW_RESIZABLE    0x10
#define WINDOW_MOVABLE      0x20
#define WINDOW_CLOSABLE     0x40

// Window colors
#define WINDOW_BG_COLOR         0x2D2D2D
#define WINDOW_BORDER_COLOR     0x404040
#define WINDOW_TITLE_BG_COLOR   0x3A3A3A
#define WINDOW_TITLE_TEXT_COLOR 0xFFFFFF
#define WINDOW_BUTTON_COLOR     0x565656
#define WINDOW_BUTTON_HOVER     0x707070
#define WINDOW_BUTTON_CLOSE     0xFF5555
#define WINDOW_FOCUS_COLOR      0x0078D4
#define WINDOW_UNFOCUS_COLOR    0x5A5A5A

// Mouse cursor types
typedef enum {
    CURSOR_DEFAULT,
    CURSOR_RESIZE_NW,
    CURSOR_RESIZE_NE,
    CURSOR_RESIZE_SW,
    CURSOR_RESIZE_SE,
    CURSOR_RESIZE_N,
    CURSOR_RESIZE_S,
    CURSOR_RESIZE_E,
    CURSOR_RESIZE_W,
    CURSOR_MOVE
} cursor_type_t;

// Window structure
typedef struct window {
    uint32_t id;
    char title[MAX_WINDOW_TITLE_LENGTH];
    int x, y;                    // Position
    int width, height;           // Size
    uint32_t flags;              // Window state flags
    uint32_t *content_buffer;    // Window content buffer
    struct window *next;         // Linked list pointer
    void (*draw_callback)(struct window *win);  // Custom draw function
    void *user_data;             // Custom user data
} window_t;

// Window manager state
typedef struct {
    window_t *windows;           // Linked list of windows
    window_t *focused_window;    // Currently focused window
    uint32_t next_window_id;     // Next available window ID
    struct limine_framebuffer *framebuffer;
    bool dragging;               // Mouse drag state
    bool resizing;               // Mouse resize state
    int drag_start_x, drag_start_y;
    int drag_offset_x, drag_offset_y;
    window_t *drag_window;
    cursor_type_t cursor_type;
} window_manager_t;

// Window manager functions
void wm_init(struct limine_framebuffer *framebuffer);
void wm_shutdown(void);
void wm_update(void);
void wm_draw_all(void);

// Window management
window_t *wm_create_window(const char *title, int x, int y, int width, int height, uint32_t flags);
void wm_destroy_window(window_t *window);
void wm_close_window(uint32_t window_id);
void wm_show_window(window_t *window);
void wm_hide_window(window_t *window);
void wm_focus_window(window_t *window);
void wm_move_window(window_t *window, int x, int y);
void wm_resize_window(window_t *window, int width, int height);
void wm_maximize_window(window_t *window);
void wm_minimize_window(window_t *window);
void wm_restore_window(window_t *window);

// Window search and iteration
window_t *wm_find_window_by_id(uint32_t id);
window_t *wm_find_window_at_position(int x, int y);
window_t *wm_get_focused_window(void);
window_t *wm_get_window_list(void);

// Window content management
void wm_clear_window(window_t *window, uint32_t color);
void wm_draw_pixel_to_window(window_t *window, int x, int y, uint32_t color);
void wm_draw_text_to_window(window_t *window, const char *text, int x, int y, uint32_t color);
void wm_draw_rect_to_window(window_t *window, int x, int y, int width, int height, uint32_t color);
void wm_draw_filled_rect_to_window(window_t *window, int x, int y, int width, int height, uint32_t color);

// Event handling
void wm_handle_mouse_event(int x, int y, bool left_button, bool right_button, bool middle_button);
void wm_handle_keyboard_event(char key);
bool wm_is_point_in_window(window_t *window, int x, int y);
bool wm_is_point_in_title_bar(window_t *window, int x, int y);
bool wm_is_point_in_close_button(window_t *window, int x, int y);

// Window rendering
void wm_draw_window_frame(window_t *window);
void wm_draw_window_title_bar(window_t *window);
void wm_draw_window_close_button(window_t *window);
void wm_draw_window_content(window_t *window);

// Desktop management
void wm_draw_desktop(void);
void wm_draw_taskbar(void);

// Utility functions
void wm_invalidate_window(window_t *window);
void wm_set_window_title(window_t *window, const char *title);
void wm_bring_window_to_front(window_t *window);
void wm_send_window_to_back(window_t *window);

// Drawing utilities
void wm_draw_line(int x1, int y1, int x2, int y2, uint32_t color);
void wm_draw_rectangle(int x, int y, int width, int height, uint32_t color);
void wm_draw_filled_rectangle(int x, int y, int width, int height, uint32_t color);
void wm_draw_text(const char *text, int x, int y, uint32_t color);

// Window statistics
int wm_get_window_count(void);
void wm_print_window_info(window_t *window);

#endif // WINDOW_MANAGER_H 