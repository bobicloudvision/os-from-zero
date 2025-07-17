
#include "window_manager.h"
#include "terminal.h"
#include "string.h"
#include "mouse.h"
#include <stddef.h>

// Global window manager instance
static window_manager_t g_wm;

// Helper function to allocate memory for window content buffer
static uint32_t *allocate_window_buffer(int width, int height) {
    // Simple allocation - in a real OS this would use proper memory management
    static uint32_t buffer_pool[MAX_WINDOWS * 800 * 600]; // Max 800x600 per window
    static size_t buffer_offset = 0;
    
    size_t needed = width * height;
    if (buffer_offset + needed > sizeof(buffer_pool) / sizeof(buffer_pool[0])) {
        return NULL; // Out of memory
    }
    
    uint32_t *buffer = &buffer_pool[buffer_offset];
    buffer_offset += needed;
    return buffer;
}

// Initialize the window manager
void wm_init(struct limine_framebuffer *framebuffer) {
    g_wm.windows = NULL;
    g_wm.focused_window = NULL;
    g_wm.next_window_id = 1;
    g_wm.framebuffer = framebuffer;
    g_wm.dragging = false;
    g_wm.resizing = false;
    g_wm.drag_window = NULL;
    g_wm.cursor_type = CURSOR_DEFAULT;
    
    // Clear the desktop and draw mouse cursor
    wm_draw_desktop();
}

// Shutdown the window manager
void wm_shutdown(void) {
    // Close all windows
    window_t *window = g_wm.windows;
    while (window) {
        window_t *next = window->next;
        wm_destroy_window(window);
        window = next;
    }
    
    g_wm.windows = NULL;
    g_wm.focused_window = NULL;
}

// Update the window manager (called regularly)
void wm_update(void) {
    // Handle mouse events
    mouse_state_t *mouse = mouse_get_state();
    static bool last_left_button = false;
    static bool last_right_button = false;
    static bool last_middle_button = false;
    
    bool left_button = mouse->left_button;
    bool right_button = mouse->right_button;
    bool middle_button = mouse->middle_button;
    
    // Debug: Print mouse state changes (minimal)
    if (left_button != last_left_button && left_button) {
        extern void terminal_print(const char *str);
        terminal_print("Click!\n");
    }
    
    // Handle mouse button press (when button goes from not pressed to pressed)
    if (left_button && !last_left_button) {
        wm_handle_mouse_event(mouse->x, mouse->y, left_button, right_button, middle_button);
    }
    
    // Handle dragging (while button is held down)
    if (g_wm.dragging && g_wm.drag_window && left_button) {
        int new_x = mouse->x - g_wm.drag_offset_x;
        int new_y = mouse->y - g_wm.drag_offset_y;
        wm_move_window(g_wm.drag_window, new_x, new_y);
        wm_draw_all(); // Redraw after moving
    } else if (g_wm.dragging && !left_button) {
        // Stop dragging when button is released
        g_wm.dragging = false;
        g_wm.drag_window = NULL;
    }
    
    // Update button state tracking
    last_left_button = left_button;
    last_right_button = right_button;
    last_middle_button = middle_button;
    
    // Ensure mouse cursor is always visible
    extern void update_mouse_cursor(void);
    update_mouse_cursor();
}

// Draw all windows
void wm_draw_all(void) {
    // Draw desktop background
    wm_draw_desktop();
    
    // Draw windows from back to front
    window_t *window = g_wm.windows;
    while (window) {
        if (window->flags & WINDOW_VISIBLE) {
            wm_draw_window_frame(window);
            wm_draw_window_content(window);
        }
        window = window->next;
    }
    
    // Always draw mouse cursor on top of everything
    extern void update_mouse_cursor(void);
    update_mouse_cursor();
}

// Create a new window
window_t *wm_create_window(const char *title, int x, int y, int width, int height, uint32_t flags) {
    // Allocate memory for the window
    window_t *window = (window_t *)allocate_window_buffer(1, sizeof(window_t) / sizeof(uint32_t));
    if (!window) {
        return NULL;
    }
    
    // Initialize window
    window->id = g_wm.next_window_id++;
    strncpy(window->title, title, MAX_WINDOW_TITLE_LENGTH - 1);
    window->title[MAX_WINDOW_TITLE_LENGTH - 1] = '\0';
    window->x = x;
    window->y = y;
    window->width = width < WINDOW_MIN_WIDTH ? WINDOW_MIN_WIDTH : width;
    window->height = height < WINDOW_MIN_HEIGHT ? WINDOW_MIN_HEIGHT : height;
    window->flags = flags | WINDOW_VISIBLE;
    window->draw_callback = NULL;
    window->user_data = NULL;
    
    // Allocate content buffer
    window->content_buffer = allocate_window_buffer(window->width, window->height);
    if (!window->content_buffer) {
        return NULL;
    }
    
    // Clear the content buffer
    wm_clear_window(window, WINDOW_BG_COLOR);
    
    // Add to window list (at the front for z-order)
    window->next = g_wm.windows;
    g_wm.windows = window;
    
    // Focus the new window
    wm_focus_window(window);
    
    return window;
}

// Destroy a window
void wm_destroy_window(window_t *window) {
    if (!window) return;
    
    // Remove from window list
    if (g_wm.windows == window) {
        g_wm.windows = window->next;
    } else {
        window_t *prev = g_wm.windows;
        while (prev && prev->next != window) {
            prev = prev->next;
        }
        if (prev) {
            prev->next = window->next;
        }
    }
    
    // Update focused window
    if (g_wm.focused_window == window) {
        g_wm.focused_window = g_wm.windows; // Focus the next window
    }
    
    // Clear dragging state if this window was being dragged
    if (g_wm.drag_window == window) {
        g_wm.dragging = false;
        g_wm.drag_window = NULL;
    }
    
    // Note: In a real OS, we would free the memory here
    // For now, we just mark it as destroyed
    window->flags = 0;
}

// Close a window by ID
void wm_close_window(uint32_t window_id) {
    window_t *window = wm_find_window_by_id(window_id);
    if (window) {
        wm_destroy_window(window);
    }
}

// Show a window
void wm_show_window(window_t *window) {
    if (window) {
        window->flags |= WINDOW_VISIBLE;
    }
}

// Hide a window
void wm_hide_window(window_t *window) {
    if (window) {
        window->flags &= ~WINDOW_VISIBLE;
    }
}

// Focus a window
void wm_focus_window(window_t *window) {
    if (!window) return;
    
    // Remove focus from current window
    if (g_wm.focused_window) {
        g_wm.focused_window->flags &= ~WINDOW_FOCUSED;
    }
    
    // Set new focused window
    g_wm.focused_window = window;
    window->flags |= WINDOW_FOCUSED;
    
    // Bring to front
    wm_bring_window_to_front(window);
}

// Move a window
void wm_move_window(window_t *window, int x, int y) {
    if (!window) return;
    
    // Ensure window stays on screen
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x + window->width > (int)g_wm.framebuffer->width) {
        x = g_wm.framebuffer->width - window->width;
    }
    if (y + window->height > (int)g_wm.framebuffer->height) {
        y = g_wm.framebuffer->height - window->height;
    }
    
    window->x = x;
    window->y = y;
}

// Resize a window
void wm_resize_window(window_t *window, int width, int height) {
    if (!window) return;
    
    // Enforce minimum size
    if (width < WINDOW_MIN_WIDTH) width = WINDOW_MIN_WIDTH;
    if (height < WINDOW_MIN_HEIGHT) height = WINDOW_MIN_HEIGHT;
    
    // Ensure window fits on screen
    if (window->x + width > (int)g_wm.framebuffer->width) {
        width = g_wm.framebuffer->width - window->x;
    }
    if (window->y + height > (int)g_wm.framebuffer->height) {
        height = g_wm.framebuffer->height - window->y;
    }
    
    window->width = width;
    window->height = height;
    
    // Note: In a real OS, we would reallocate the content buffer here
    // For now, we just update the dimensions
}

// Maximize a window
void wm_maximize_window(window_t *window) {
    if (!window) return;
    
    window->flags |= WINDOW_MAXIMIZED;
    window->x = 0;
    window->y = 0;
    window->width = g_wm.framebuffer->width;
    window->height = g_wm.framebuffer->height;
}

// Minimize a window
void wm_minimize_window(window_t *window) {
    if (!window) return;
    
    window->flags |= WINDOW_MINIMIZED;
    window->flags &= ~WINDOW_VISIBLE;
}

// Restore a window
void wm_restore_window(window_t *window) {
    if (!window) return;
    
    window->flags &= ~(WINDOW_MINIMIZED | WINDOW_MAXIMIZED);
    window->flags |= WINDOW_VISIBLE;
}

// Find window by ID
window_t *wm_find_window_by_id(uint32_t id) {
    window_t *window = g_wm.windows;
    while (window) {
        if (window->id == id) {
            return window;
        }
        window = window->next;
    }
    return NULL;
}

// Find window at position (topmost)
window_t *wm_find_window_at_position(int x, int y) {
    window_t *window = g_wm.windows; // Start from front
    while (window) {
        if ((window->flags & WINDOW_VISIBLE) && wm_is_point_in_window(window, x, y)) {
            return window;
        }
        window = window->next;
    }
    return NULL;
}

// Get focused window
window_t *wm_get_focused_window(void) {
    return g_wm.focused_window;
}

// Get window list
window_t *wm_get_window_list(void) {
    return g_wm.windows;
}

// Clear window content
void wm_clear_window(window_t *window, uint32_t color) {
    if (!window || !window->content_buffer) return;
    
    for (int i = 0; i < window->width * window->height; i++) {
        window->content_buffer[i] = color;
    }
}

// Draw pixel to window
void wm_draw_pixel_to_window(window_t *window, int x, int y, uint32_t color) {
    if (!window || !window->content_buffer) return;
    if (x < 0 || y < 0 || x >= window->width || y >= window->height) return;
    
    window->content_buffer[y * window->width + x] = color;
}

// Draw text to window
void wm_draw_text_to_window(window_t *window, const char *text, int x, int y, uint32_t color) {
    if (!window || !text) return;
    
    // Simple text rendering - we'll use the existing terminal font
    int current_x = x;
    while (*text) {
        if (*text == '\n') {
            current_x = x;
            y += 16; // Font height
        } else {
            // Draw character using existing font system
            // This is a simplified version - in reality we'd render to the window buffer
            current_x += 16; // Font width
        }
        text++;
    }
}

// Draw rectangle to window
void wm_draw_rect_to_window(window_t *window, int x, int y, int width, int height, uint32_t color) {
    if (!window) return;
    
    // Draw top and bottom lines
    for (int i = 0; i < width; i++) {
        wm_draw_pixel_to_window(window, x + i, y, color);
        wm_draw_pixel_to_window(window, x + i, y + height - 1, color);
    }
    
    // Draw left and right lines
    for (int i = 0; i < height; i++) {
        wm_draw_pixel_to_window(window, x, y + i, color);
        wm_draw_pixel_to_window(window, x + width - 1, y + i, color);
    }
}

// Draw filled rectangle to window
void wm_draw_filled_rect_to_window(window_t *window, int x, int y, int width, int height, uint32_t color) {
    if (!window) return;
    
    for (int j = 0; j < height; j++) {
        for (int i = 0; i < width; i++) {
            wm_draw_pixel_to_window(window, x + i, y + j, color);
        }
    }
}

// Handle mouse events
void wm_handle_mouse_event(int x, int y, bool left_button, bool right_button, bool middle_button) {
    (void)right_button; // Mark as used
    (void)middle_button; // Mark as used
    
    // Handle left button press (only on button press, not hold)
    if (left_button) {
        window_t *window = wm_find_window_at_position(x, y);
        if (window) {
            // Debug: Print click information
            extern void terminal_print(const char *str);
            terminal_print("Click detected at (");
            char pos_str[16];
            extern void int_to_string(int value, char *buffer);
            int_to_string(x, pos_str);
            terminal_print(pos_str);
            terminal_print(", ");
            int_to_string(y, pos_str);
            terminal_print(pos_str);
            terminal_print(") on window ");
            int_to_string(window->id, pos_str);
            terminal_print(pos_str);
            
            // Check if clicking close button
            if (wm_is_point_in_close_button(window, x, y)) {
                terminal_print(" - CLOSE BUTTON!\n");
                wm_destroy_window(window);
                wm_draw_all(); // Redraw after closing
                return;
            }
            
            // Check if clicking title bar (but not close button)
            if (wm_is_point_in_title_bar(window, x, y)) {
                terminal_print(" - TITLE BAR (dragging)\n");
                // Start dragging
                g_wm.dragging = true;
                g_wm.drag_window = window;
                g_wm.drag_offset_x = x - window->x;
                g_wm.drag_offset_y = y - window->y;
                wm_focus_window(window);
                wm_draw_all(); // Redraw to show focus change
                return;
            }
            
            // Focus the window if clicking on content area
            terminal_print(" - CONTENT AREA (focusing)\n");
            wm_focus_window(window);
            wm_draw_all(); // Redraw to show focus change
        } else {
            // Debug: Click on desktop
            terminal_print("Click on desktop\n");
        }
    }
}

// Handle keyboard events
void wm_handle_keyboard_event(char key) {
    // Handle keyboard shortcuts
    if (key == 'q' && g_wm.focused_window) {
        wm_destroy_window(g_wm.focused_window);
    }
}

// Check if point is in window
bool wm_is_point_in_window(window_t *window, int x, int y) {
    if (!window) return false;
    
    return (x >= window->x && x < window->x + window->width &&
            y >= window->y && y < window->y + window->height);
}

// Check if point is in title bar
bool wm_is_point_in_title_bar(window_t *window, int x, int y) {
    if (!window) return false;
    
    return (x >= window->x && x < window->x + window->width &&
            y >= window->y && y < window->y + WINDOW_TITLE_BAR_HEIGHT);
}

// Check if point is in close button
bool wm_is_point_in_close_button(window_t *window, int x, int y) {
    if (!window) return false;
    
    int button_x = window->x + window->width - 22;  // Moved left slightly for easier clicking
    int button_y = window->y + 2;
    
    return (x >= button_x && x < button_x + 20 &&  // Made button larger (20x20)
            y >= button_y && y < button_y + 20);
}

// Draw window frame
void wm_draw_window_frame(window_t *window) {
    if (!window || !(window->flags & WINDOW_VISIBLE)) return;
    
    uint32_t border_color = (window->flags & WINDOW_FOCUSED) ? WINDOW_FOCUS_COLOR : WINDOW_BORDER_COLOR;
    
    // Draw border
    wm_draw_filled_rectangle(window->x - WINDOW_BORDER_WIDTH, 
                           window->y - WINDOW_BORDER_WIDTH,
                           window->width + 2 * WINDOW_BORDER_WIDTH,
                           window->height + 2 * WINDOW_BORDER_WIDTH,
                           border_color);
    
    // Draw title bar
    wm_draw_window_title_bar(window);
    
    // Draw close button
    wm_draw_window_close_button(window);
}

// Draw window title bar
void wm_draw_window_title_bar(window_t *window) {
    if (!window) return;
    
    uint32_t title_color = (window->flags & WINDOW_FOCUSED) ? WINDOW_TITLE_BG_COLOR : WINDOW_UNFOCUS_COLOR;
    
    // Draw title bar background
    wm_draw_filled_rectangle(window->x, window->y, 
                           window->width, WINDOW_TITLE_BAR_HEIGHT,
                           title_color);
    
    // Draw title text
    wm_draw_text(window->title, window->x + 5, window->y + 4, WINDOW_TITLE_TEXT_COLOR);
}

// Draw window close button
void wm_draw_window_close_button(window_t *window) {
    if (!window) return;
    
    int button_x = window->x + window->width - 22;  // Match the hit detection
    int button_y = window->y + 2;
    
    // Draw button background
    wm_draw_filled_rectangle(button_x, button_y, 20, 20, WINDOW_BUTTON_CLOSE);
    
    // Draw X (centered in the larger button)
    wm_draw_line(button_x + 5, button_y + 5, button_x + 15, button_y + 15, 0xFFFFFF);
    wm_draw_line(button_x + 15, button_y + 5, button_x + 5, button_y + 15, 0xFFFFFF);
}

// Draw window content
void wm_draw_window_content(window_t *window) {
    if (!window || !window->content_buffer) return;
    
    // Copy content buffer to framebuffer
    volatile uint32_t *fb_ptr = g_wm.framebuffer->address;
    int content_y = window->y + WINDOW_TITLE_BAR_HEIGHT;
    int content_height = window->height - WINDOW_TITLE_BAR_HEIGHT;
    
    for (int y = 0; y < content_height; y++) {
        for (int x = 0; x < window->width; x++) {
            int fb_x = window->x + x;
            int fb_y = content_y + y;
            
            if (fb_x >= 0 && fb_x < (int)g_wm.framebuffer->width &&
                fb_y >= 0 && fb_y < (int)g_wm.framebuffer->height) {
                fb_ptr[fb_y * (g_wm.framebuffer->pitch / 4) + fb_x] = 
                    window->content_buffer[y * window->width + x];
            }
        }
    }
    
    // Call custom draw callback if set
    if (window->draw_callback) {
        window->draw_callback(window);
    }
}

// Draw desktop background
void wm_draw_desktop(void) {
    volatile uint32_t *fb_ptr = g_wm.framebuffer->address;
    uint32_t desktop_color = 0x1E1E1E; // Dark gray desktop
    
    for (size_t i = 0; i < g_wm.framebuffer->width * g_wm.framebuffer->height; i++) {
        fb_ptr[i] = desktop_color;
    }
    
    // Make sure mouse cursor is drawn on top
    extern void update_mouse_cursor(void);
    update_mouse_cursor();
}

// Bring window to front
void wm_bring_window_to_front(window_t *window) {
    if (!window || g_wm.windows == window) return;
    
    // Remove from current position
    window_t *prev = g_wm.windows;
    while (prev && prev->next != window) {
        prev = prev->next;
    }
    if (prev) {
        prev->next = window->next;
    }
    
    // Add to front
    window->next = g_wm.windows;
    g_wm.windows = window;
}

// Drawing utilities
void wm_draw_line(int x1, int y1, int x2, int y2, uint32_t color) {
    volatile uint32_t *fb_ptr = g_wm.framebuffer->address;
    
    int dx = x2 - x1;
    int dy = y2 - y1;
    int steps = (dx > dy) ? dx : dy;
    if (steps < 0) steps = -steps;
    
    float x_inc = (float)dx / (float)steps;
    float y_inc = (float)dy / (float)steps;
    
    float x = x1;
    float y = y1;
    
    for (int i = 0; i <= steps; i++) {
        int px = (int)x;
        int py = (int)y;
        
        if (px >= 0 && px < (int)g_wm.framebuffer->width &&
            py >= 0 && py < (int)g_wm.framebuffer->height) {
            fb_ptr[py * (g_wm.framebuffer->pitch / 4) + px] = color;
        }
        
        x += x_inc;
        y += y_inc;
    }
}

void wm_draw_filled_rectangle(int x, int y, int width, int height, uint32_t color) {
    volatile uint32_t *fb_ptr = g_wm.framebuffer->address;
    
    for (int j = 0; j < height; j++) {
        for (int i = 0; i < width; i++) {
            int px = x + i;
            int py = y + j;
            
            if (px >= 0 && px < (int)g_wm.framebuffer->width &&
                py >= 0 && py < (int)g_wm.framebuffer->height) {
                fb_ptr[py * (g_wm.framebuffer->pitch / 4) + px] = color;
            }
        }
    }
}

void wm_draw_text(const char *text, int x, int y, uint32_t color) {
    // Use existing terminal drawing function
    draw_string(g_wm.framebuffer, text, x, y, color);
}

// Get window count
int wm_get_window_count(void) {
    int count = 0;
    window_t *window = g_wm.windows;
    while (window) {
        count++;
        window = window->next;
    }
    return count;
}

// Print window info
void wm_print_window_info(window_t *window) {
    if (!window) return;
    
    terminal_print("Window ID: ");
    char id_str[16];
    int_to_string(window->id, id_str);
    terminal_print(id_str);
    terminal_print("\n");
    
    terminal_print("Title: ");
    terminal_print(window->title);
    terminal_print("\n");
    
    terminal_print("Position: (");
    char pos_str[16];
    int_to_string(window->x, pos_str);
    terminal_print(pos_str);
    terminal_print(", ");
    int_to_string(window->y, pos_str);
    terminal_print(pos_str);
    terminal_print(")\n");
    
    terminal_print("Size: ");
    int_to_string(window->width, pos_str);
    terminal_print(pos_str);
    terminal_print("x");
    int_to_string(window->height, pos_str);
    terminal_print(pos_str);
    terminal_print("\n");
    
    terminal_print("Flags: ");
    if (window->flags & WINDOW_VISIBLE) terminal_print("VISIBLE ");
    if (window->flags & WINDOW_FOCUSED) terminal_print("FOCUSED ");
    if (window->flags & WINDOW_MINIMIZED) terminal_print("MINIMIZED ");
    if (window->flags & WINDOW_MAXIMIZED) terminal_print("MAXIMIZED ");
    terminal_print("\n");
}

// Invalidate window for redraw (placeholder implementation)
void wm_invalidate_window(window_t *window) {
    // Simple invalidation - in a more complex system this would mark
    // the window for redraw, but for now we can just ignore it
    // since our widgets redraw themselves when needed
    (void)window; // Mark parameter as used
} 