#include "window_example.h"
#include "../window_manager_rust.h"
#include "../string.h"
#include "../shell.h"
#include "../terminal.h"
#include "../mouse.h"
#include <stddef.h>

// Example: Create a simple window with text
void create_simple_window_example(void) {
    window_t *win = wm_create_window("Simple Window", 100, 100, 300, 200, 
                                     WINDOW_MOVABLE | WINDOW_CLOSABLE);
    
    if (win) {
        wm_clear_window(win, 0x2d2d2d); // Dark gray background
        wm_draw_text_to_window(win, "Hello from Rust WM!", 10, 30, 0xffffff);
        wm_draw_text_to_window(win, "This window was created", 10, 50, 0x00ff00);
        wm_draw_text_to_window(win, "using the Rust window manager!", 10, 70, 0x00ff00);
        wm_draw_text_to_window(win, "You can drag this window", 10, 90, 0xffff00);
        wm_draw_text_to_window(win, "by clicking the title bar.", 10, 110, 0xffff00);
        wm_invalidate_window(win); // Force redraw
    }
}

// Example: Create a window with colored rectangles
void create_colored_rectangles_example(void) {
    window_t *win = wm_create_window("Colored Rectangles", 150, 150, 250, 180, 
                                     WINDOW_MOVABLE | WINDOW_CLOSABLE);
    
    if (win) {
        wm_clear_window(win, 0x1a1a1a);
        
        // Draw colored rectangles
        wm_draw_filled_rect_to_window(win, 10, 30, 50, 30, 0xff0000);  // Red
        wm_draw_filled_rect_to_window(win, 70, 30, 50, 30, 0x00ff00);  // Green
        wm_draw_filled_rect_to_window(win, 130, 30, 50, 30, 0x0000ff); // Blue
        
        wm_draw_filled_rect_to_window(win, 10, 70, 50, 30, 0xffff00);  // Yellow
        wm_draw_filled_rect_to_window(win, 70, 70, 50, 30, 0xff00ff);  // Magenta
        wm_draw_filled_rect_to_window(win, 130, 70, 50, 30, 0x00ffff); // Cyan
        
        // Draw borders around rectangles
        wm_draw_rect_to_window(win, 10, 30, 50, 30, 0xffffff);
        wm_draw_rect_to_window(win, 70, 30, 50, 30, 0xffffff);
        wm_draw_rect_to_window(win, 130, 30, 50, 30, 0xffffff);
        
        wm_draw_text_to_window(win, "Color Palette", 10, 110, 0xffffff);
    }
}

// Example: Create a window with a pattern
void create_pattern_example(void) {
    window_t *win = wm_create_window("Pattern Example", 200, 200, 220, 160, 
                                     WINDOW_MOVABLE | WINDOW_CLOSABLE);
    
    if (win) {
        wm_clear_window(win, 0x000000);
        
        // Draw a checkerboard pattern
        for (int y = 0; y < 8; y++) {
            for (int x = 0; x < 8; x++) {
                uint32_t color = ((x + y) % 2 == 0) ? 0xffffff : 0x000000;
                wm_draw_filled_rect_to_window(win, 10 + x * 20, 30 + y * 15, 20, 15, color);
            }
        }
        
        wm_draw_text_to_window(win, "Checkerboard", 10, 150, 0xffffff);
    }
}

// Example: Create multiple windows
void create_multiple_windows_example(void) {
    // Create several windows at different positions
    window_t *win1 = wm_create_window("Window 1", 50, 50, 200, 150, 
                                      WINDOW_MOVABLE | WINDOW_CLOSABLE);
    window_t *win2 = wm_create_window("Window 2", 300, 100, 200, 150, 
                                      WINDOW_MOVABLE | WINDOW_CLOSABLE);
    window_t *win3 = wm_create_window("Window 3", 150, 250, 200, 150, 
                                      WINDOW_MOVABLE | WINDOW_CLOSABLE);
    
    if (win1) {
        wm_clear_window(win1, 0x2d2d2d);
        wm_draw_text_to_window(win1, "First Window", 10, 30, 0xffffff);
        wm_draw_text_to_window(win1, "Position: (50, 50)", 10, 50, 0xaaaaaa);
    }
    
    if (win2) {
        wm_clear_window(win2, 0x2d4d2d);
        wm_draw_text_to_window(win2, "Second Window", 10, 30, 0xffffff);
        wm_draw_text_to_window(win2, "Position: (300, 100)", 10, 50, 0xaaaaaa);
    }
    
    if (win3) {
        wm_clear_window(win3, 0x2d2d4d);
        wm_draw_text_to_window(win3, "Third Window", 10, 30, 0xffffff);
        wm_draw_text_to_window(win3, "Position: (150, 250)", 10, 50, 0xaaaaaa);
    }
}

// Example: Create a window with text information
void create_info_window_example(void) {
    window_t *win = wm_create_window("Info Window", 400, 100, 280, 200, 
                                     WINDOW_MOVABLE | WINDOW_CLOSABLE);
    
    if (win) {
        wm_clear_window(win, 0x1e1e1e);
        
        wm_draw_text_to_window(win, "Window Manager Info", 10, 30, 0x4a90e2);
        wm_draw_text_to_window(win, "Built with Rust", 10, 50, 0xffffff);
        wm_draw_text_to_window(win, "Features:", 10, 70, 0xffff00);
        wm_draw_text_to_window(win, "- Window creation", 10, 90, 0xaaaaaa);
        wm_draw_text_to_window(win, "- Mouse dragging", 10, 110, 0xaaaaaa);
        wm_draw_text_to_window(win, "- Text rendering", 10, 130, 0xaaaaaa);
        wm_draw_text_to_window(win, "- Rectangle drawing", 10, 150, 0xaaaaaa);
    }
}

// Main example function - creates all examples
void run_window_examples(void) {
    // Create windows with some spacing
    create_simple_window_example();
    create_colored_rectangles_example();
    create_pattern_example();
    create_info_window_example();
    // Uncomment to create multiple windows:
    // create_multiple_windows_example();
    
    // Force immediate render
    extern void wm_update(void);
    wm_update();
    // Cursor is now rendered by the window manager
}

// Command handler for 'windows' command
static void cmd_windows(const char *args) {
    if (args == NULL || strlen(args) == 0) {
        // No arguments - show all examples
        terminal_print("Creating window examples...\n");
        run_window_examples();
        terminal_print("Windows created! Try moving them with your mouse.\n");
        terminal_print("Click and drag the title bar (top 20px) to move windows.\n");
        terminal_print("Click the red X button to close windows.\n");
        // Force an update to render the windows
        extern void wm_update(void);
        wm_update();
        // Cursor is now rendered by the window manager
    } else {
        // Parse specific example
        if (strcmp(args, "simple") == 0) {
            create_simple_window_example();
            terminal_print("Simple window created!\n");
        } else if (strcmp(args, "colors") == 0) {
            create_colored_rectangles_example();
            terminal_print("Colored rectangles window created!\n");
        } else if (strcmp(args, "pattern") == 0) {
            create_pattern_example();
            terminal_print("Pattern window created!\n");
        } else if (strcmp(args, "info") == 0) {
            create_info_window_example();
            terminal_print("Info window created!\n");
        } else if (strcmp(args, "multiple") == 0) {
            create_multiple_windows_example();
            terminal_print("Multiple windows created!\n");
        } else {
            terminal_print("Usage: windows [simple|colors|pattern|info|multiple]\n");
            terminal_print("  windows        - Create all example windows\n");
            terminal_print("  windows simple - Create a simple text window\n");
            terminal_print("  windows colors - Create a window with colored rectangles\n");
            terminal_print("  windows pattern - Create a window with a pattern\n");
            terminal_print("  windows info   - Create an info window\n");
            terminal_print("  windows multiple - Create multiple windows\n");
        }
    }
}

// Debug command to show mouse and window info
static void cmd_wmdebug(const char *args) {
    (void)args;
    extern void wm_get_debug_info(char *buffer, size_t buffer_size);
    extern int wm_get_window_count(void);
    extern void wm_get_window_info(int index, int *x, int *y, int *w, int *h, char *title);
    
    terminal_print("Window Manager Debug Info:\n");
    
    int window_count = wm_get_window_count();
    char count_str[32];
    int_to_string(window_count, count_str);
    terminal_print("Window count: ");
    terminal_print(count_str);
    terminal_print("\n");
    
    // Get mouse position
    extern mouse_state_t* mouse_get_state(void);
    mouse_state_t *mouse = mouse_get_state();
    char mouse_x_str[32], mouse_y_str[32];
    int_to_string(mouse->x, mouse_x_str);
    int_to_string(mouse->y, mouse_y_str);
    terminal_print("Mouse position: (");
    terminal_print(mouse_x_str);
    terminal_print(", ");
    terminal_print(mouse_y_str);
    terminal_print(")\n");
    terminal_print("Mouse button: ");
    terminal_print(mouse->left_button ? "PRESSED" : "RELEASED");
    terminal_print("\n");
    
    // Show window positions
    for (int i = 0; i < window_count && i < 10; i++) {
        int x, y, w, h;
        char title[64];
        wm_get_window_info(i, &x, &y, &w, &h, title);
        
        char x_str[32], y_str[32], w_str[32], h_str[32];
        int_to_string(x, x_str);
        int_to_string(y, y_str);
        int_to_string(w, w_str);
        int_to_string(h, h_str);
        
        terminal_print("Window ");
        char idx_str[16];
        int_to_string(i, idx_str);
        terminal_print(idx_str);
        terminal_print(": ");
        terminal_print(title);
        terminal_print(" at (");
        terminal_print(x_str);
        terminal_print(", ");
        terminal_print(y_str);
        terminal_print(") size ");
        terminal_print(w_str);
        terminal_print("x");
        terminal_print(h_str);
        terminal_print("\n");
    }
}

// Register window example commands
void register_window_example_commands(void) {
    register_command("windows", cmd_windows, 
                     "Create example windows to demonstrate the window manager",
                     "windows [simple|colors|pattern|info|multiple]",
                     "Desktop");
    register_command("wmdebug", cmd_wmdebug,
                     "Show window manager debug information",
                     "wmdebug",
                     "Desktop");
}
