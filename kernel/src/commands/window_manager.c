#include "window_manager.h"
#include "system.h"
#include "../terminal.h"
#include "../window_manager.h"
#include "../string.h"
#include "../audio.h"
#include "../mouse.h"
#include <stddef.h>
#include <stdbool.h>

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

// Sample window draw callback - simple test pattern
static void sample_window_callback(window_t *window) {
    // Draw a simple pattern in the window
    for (int y = 0; y < 50; y++) {
        for (int x = 0; x < 100; x++) {
            uint32_t color = ((x + y) % 20 < 10) ? 0xFF0000 : 0x00FF00;
            wm_draw_pixel_to_window(window, x + 10, y + 10, color);
        }
    }
}

// Calculator window draw callback
static void calculator_window_callback(window_t *window) {
    // Draw a simple calculator interface
    wm_draw_filled_rect_to_window(window, 10, 10, 200, 30, 0xFFFFFF); // Display area
    
    // Draw buttons
    const char *buttons[] = {"7", "8", "9", "/", "4", "5", "6", "*", "1", "2", "3", "-", "0", ".", "=", "+"};
    
    for (int i = 0; i < 16; i++) {
        int row = i / 4;
        int col = i % 4;
        int x = 10 + col * 45;
        int y = 50 + row * 35;
        
        // Draw button background
        wm_draw_filled_rect_to_window(window, x, y, 40, 30, 0xCCCCCC);
        wm_draw_rect_to_window(window, x, y, 40, 30, 0x000000);
        
        // Draw button text (simplified)
        wm_draw_pixel_to_window(window, x + 18, y + 12, 0x000000);
    }
}

// Command: window create
void cmd_window_create(const char *args) {
    if (!args || strlen(args) == 0) {
        terminal_print("Usage: window create <title> [x y width height]\n");
        terminal_print("Example: window create \"My Window\" 100 100 300 200\n");
        return;
    }
    
    char title[64];
    const char *ptr = args;
    
    // Parse title (handle quoted strings)
    if (*ptr == '"') {
        ptr++; // Skip opening quote
        size_t i = 0;
        while (*ptr && *ptr != '"' && i < sizeof(title) - 1) {
            title[i++] = *ptr++;
        }
        title[i] = '\0';
        if (*ptr == '"') ptr++; // Skip closing quote
    } else {
        ptr = parse_word(ptr, title, sizeof(title));
    }
    
    // Parse optional position and size
    int x = 50, y = 50, width = 300, height = 200;
    
    char word[32];
    ptr = parse_word(ptr, word, sizeof(word));
    if (strlen(word) > 0) {
        x = parse_int(word);
        ptr = parse_word(ptr, word, sizeof(word));
        if (strlen(word) > 0) {
            y = parse_int(word);
            ptr = parse_word(ptr, word, sizeof(word));
            if (strlen(word) > 0) {
                width = parse_int(word);
                ptr = parse_word(ptr, word, sizeof(word));
                if (strlen(word) > 0) {
                    height = parse_int(word);
                }
            }
        }
    }
    
    // Create the window
    window_t *window = wm_create_window(title, x, y, width, height, 
                                       WINDOW_MOVABLE | WINDOW_RESIZABLE | WINDOW_CLOSABLE);
    
    if (window) {
        terminal_print("Created window '");
        terminal_print(title);
        terminal_print("' with ID ");
        char id_str[16];
        int_to_string(window->id, id_str);
        terminal_print(id_str);
        terminal_print("\n");
        
        // Set sample callback for demonstration
        window->draw_callback = sample_window_callback;
        
        // Play creation sound
        audio_play_event(AUDIO_SYSTEM_BEEP);
        
        // Refresh the display
        wm_draw_all();
    } else {
        terminal_print("Failed to create window: Out of memory\n");
        audio_play_event(AUDIO_ERROR_BEEP);
    }
}

// Command: window list
void cmd_window_list(const char *args) {
    (void)args; // Unused parameter
    
    int count = wm_get_window_count();
    if (count == 0) {
        terminal_print("No windows open\n");
        return;
    }
    
    terminal_print("Open windows:\n");
    terminal_print("ID  Title                     Position    Size        Flags\n");
    terminal_print("--  ----                     --------    ----        -----\n");
    
    window_t *window = wm_get_window_list();
    while (window) {
        char id_str[8], pos_str[32], size_str[32];
        
        int_to_string(window->id, id_str);
        terminal_print(id_str);
        terminal_print("   ");
        
        // Print title (truncate if too long)
        char title[26];
        strncpy(title, window->title, 25);
        title[25] = '\0';
        terminal_print(title);
        
        // Pad to align columns
        int title_len = strlen(title);
        for (int i = title_len; i < 25; i++) {
            terminal_print(" ");
        }
        
        // Position
        int_to_string(window->x, pos_str);
        terminal_print("(");
        terminal_print(pos_str);
        terminal_print(",");
        int_to_string(window->y, pos_str);
        terminal_print(pos_str);
        terminal_print(")");
        
        // Size
        terminal_print("      ");
        int_to_string(window->width, size_str);
        terminal_print(size_str);
        terminal_print("x");
        int_to_string(window->height, size_str);
        terminal_print(size_str);
        
        // Flags
        terminal_print("      ");
        if (window->flags & WINDOW_VISIBLE) terminal_print("V");
        if (window->flags & WINDOW_FOCUSED) terminal_print("F");
        if (window->flags & WINDOW_MINIMIZED) terminal_print("M");
        if (window->flags & WINDOW_MAXIMIZED) terminal_print("X");
        
        terminal_print("\n");
        window = window->next;
    }
}

// Command: window close
void cmd_window_close(const char *args) {
    if (!args || strlen(args) == 0) {
        terminal_print("Usage: window close <window_id>\n");
        terminal_print("Use 'window list' to see window IDs\n");
        return;
    }
    
    uint32_t window_id = parse_int(args);
    window_t *window = wm_find_window_by_id(window_id);
    
    if (window) {
        terminal_print("Closing window '");
        terminal_print(window->title);
        terminal_print("'\n");
        
        wm_destroy_window(window);
        audio_play_event(AUDIO_SYSTEM_BEEP);
        wm_draw_all();
    } else {
        terminal_print("Window with ID ");
        char id_str[16];
        int_to_string(window_id, id_str);
        terminal_print(id_str);
        terminal_print(" not found\n");
        audio_play_event(AUDIO_ERROR_BEEP);
    }
}

// Command: window move
void cmd_window_move(const char *args) {
    if (!args || strlen(args) == 0) {
        terminal_print("Usage: window move <window_id> <x> <y>\n");
        return;
    }
    
    char word[32];
    const char *ptr = args;
    
    ptr = parse_word(ptr, word, sizeof(word));
    uint32_t window_id = parse_int(word);
    
    ptr = parse_word(ptr, word, sizeof(word));
    if (strlen(word) == 0) {
        terminal_print("Usage: window move <window_id> <x> <y>\n");
        return;
    }
    int x = parse_int(word);
    
    ptr = parse_word(ptr, word, sizeof(word));
    if (strlen(word) == 0) {
        terminal_print("Usage: window move <window_id> <x> <y>\n");
        return;
    }
    int y = parse_int(word);
    
    window_t *window = wm_find_window_by_id(window_id);
    if (window) {
        wm_move_window(window, x, y);
        terminal_print("Moved window to (");
        char pos_str[16];
        int_to_string(x, pos_str);
        terminal_print(pos_str);
        terminal_print(", ");
        int_to_string(y, pos_str);
        terminal_print(pos_str);
        terminal_print(")\n");
        
        wm_draw_all();
    } else {
        terminal_print("Window not found\n");
        audio_play_event(AUDIO_ERROR_BEEP);
    }
}

// Command: window resize
void cmd_window_resize(const char *args) {
    if (!args || strlen(args) == 0) {
        terminal_print("Usage: window resize <window_id> <width> <height>\n");
        return;
    }
    
    char word[32];
    const char *ptr = args;
    
    ptr = parse_word(ptr, word, sizeof(word));
    uint32_t window_id = parse_int(word);
    
    ptr = parse_word(ptr, word, sizeof(word));
    if (strlen(word) == 0) {
        terminal_print("Usage: window resize <window_id> <width> <height>\n");
        return;
    }
    int width = parse_int(word);
    
    ptr = parse_word(ptr, word, sizeof(word));
    if (strlen(word) == 0) {
        terminal_print("Usage: window resize <window_id> <width> <height>\n");
        return;
    }
    int height = parse_int(word);
    
    window_t *window = wm_find_window_by_id(window_id);
    if (window) {
        wm_resize_window(window, width, height);
        terminal_print("Resized window to ");
        char size_str[16];
        int_to_string(width, size_str);
        terminal_print(size_str);
        terminal_print("x");
        int_to_string(height, size_str);
        terminal_print(size_str);
        terminal_print("\n");
        
        wm_draw_all();
    } else {
        terminal_print("Window not found\n");
        audio_play_event(AUDIO_ERROR_BEEP);
    }
}

// Command: window focus
void cmd_window_focus(const char *args) {
    if (!args || strlen(args) == 0) {
        terminal_print("Usage: window focus <window_id>\n");
        return;
    }
    
    uint32_t window_id = parse_int(args);
    window_t *window = wm_find_window_by_id(window_id);
    
    if (window) {
        wm_focus_window(window);
        terminal_print("Focused window '");
        terminal_print(window->title);
        terminal_print("'\n");
        
        wm_draw_all();
    } else {
        terminal_print("Window not found\n");
        audio_play_event(AUDIO_ERROR_BEEP);
    }
}

// Command: window maximize
void cmd_window_maximize(const char *args) {
    if (!args || strlen(args) == 0) {
        terminal_print("Usage: window maximize <window_id>\n");
        return;
    }
    
    uint32_t window_id = parse_int(args);
    window_t *window = wm_find_window_by_id(window_id);
    
    if (window) {
        wm_maximize_window(window);
        terminal_print("Maximized window '");
        terminal_print(window->title);
        terminal_print("'\n");
        
        wm_draw_all();
    } else {
        terminal_print("Window not found\n");
        audio_play_event(AUDIO_ERROR_BEEP);
    }
}

// Command: window minimize
void cmd_window_minimize(const char *args) {
    if (!args || strlen(args) == 0) {
        terminal_print("Usage: window minimize <window_id>\n");
        return;
    }
    
    uint32_t window_id = parse_int(args);
    window_t *window = wm_find_window_by_id(window_id);
    
    if (window) {
        wm_minimize_window(window);
        terminal_print("Minimized window '");
        terminal_print(window->title);
        terminal_print("'\n");
        
        wm_draw_all();
    } else {
        terminal_print("Window not found\n");
        audio_play_event(AUDIO_ERROR_BEEP);
    }
}

// Command: window restore
void cmd_window_restore(const char *args) {
    if (!args || strlen(args) == 0) {
        terminal_print("Usage: window restore <window_id>\n");
        return;
    }
    
    uint32_t window_id = parse_int(args);
    window_t *window = wm_find_window_by_id(window_id);
    
    if (window) {
        wm_restore_window(window);
        terminal_print("Restored window '");
        terminal_print(window->title);
        terminal_print("'\n");
        
        wm_draw_all();
    } else {
        terminal_print("Window not found\n");
        audio_play_event(AUDIO_ERROR_BEEP);
    }
}

// Command: window info
void cmd_window_info(const char *args) {
    if (!args || strlen(args) == 0) {
        terminal_print("Usage: window info <window_id>\n");
        return;
    }
    
    uint32_t window_id = parse_int(args);
    window_t *window = wm_find_window_by_id(window_id);
    
    if (window) {
        wm_print_window_info(window);
    } else {
        terminal_print("Window not found\n");
        audio_play_event(AUDIO_ERROR_BEEP);
    }
}

// Command: window demo
void cmd_window_demo(const char *args) {
    (void)args; // Unused parameter
    
    terminal_print("Creating window manager demo...\n");
    
    // Create several demo windows
    window_t *win1 = wm_create_window("Demo Window 1", 50, 50, 250, 150, 
                                     WINDOW_MOVABLE | WINDOW_RESIZABLE | WINDOW_CLOSABLE);
    if (win1) {
        win1->draw_callback = sample_window_callback;
    }
    
    window_t *win2 = wm_create_window("Calculator", 320, 80, 220, 200, 
                                     WINDOW_MOVABLE | WINDOW_CLOSABLE);
    if (win2) {
        win2->draw_callback = calculator_window_callback;
    }
    
    window_t *win3 = wm_create_window("Terminal", 100, 200, 300, 180, 
                                     WINDOW_MOVABLE | WINDOW_RESIZABLE | WINDOW_CLOSABLE);
    if (win3) {
        // Fill with terminal-like content
        wm_clear_window(win3, 0x000000);
        for (int i = 0; i < 10; i++) {
            for (int j = 0; j < 20; j++) {
                wm_draw_pixel_to_window(win3, j * 2, i * 2, 0x00FF00);
            }
        }
    }
    
    terminal_print("Demo windows created! Use mouse to interact:\n");
    terminal_print("- Click title bar to drag windows\n");
    terminal_print("- Click X button to close windows\n");
    terminal_print("- Use 'window list' to see all windows\n");
    terminal_print("- Use 'window close <id>' to close specific windows\n");
    
    // Play demo sound
    audio_play_event(AUDIO_STARTUP_SOUND);
    
    // Refresh display
    wm_draw_all();
}

// Command: desktop
void cmd_desktop(const char *args) {
    (void)args; // Unused parameter
    
    terminal_print("Switching to desktop mode...\n");
    terminal_print("Desktop features:\n");
    terminal_print("- Click windows to focus them\n");
    terminal_print("- Drag windows by their title bars\n");
    terminal_print("- Close windows with the X button\n");
    terminal_print("- Use 'window' commands for more control\n");
    terminal_print("- Type 'terminal' to return to terminal mode\n");
    
    wm_draw_all();
}

// Command: terminal (return to terminal mode)
void cmd_terminal_mode(const char *args) {
    (void)args; // Unused parameter
    
    terminal_print("Returning to terminal mode...\n");
    terminal_print("Window manager is still active in background.\n");
    terminal_print("Use 'desktop' to switch back to desktop mode.\n");
    
    // Clear screen and redraw terminal
    clear_screen();
    terminal_print("DEA OS - Terminal Mode\n");
    terminal_print("Type 'help' for available commands\n");
    terminal_print("Type 'desktop' to return to desktop mode\n");
}

// Command: mouse test
void cmd_mouse_test(const char *args) {
    (void)args; // Unused parameter
    
    terminal_print("Mouse Test Mode\n");
    terminal_print("===============\n");
    terminal_print("Move mouse and click buttons to test functionality.\n");
    terminal_print("Press 'q' to quit.\n\n");
    
    while (1) {
        // Check for mouse data
        if (mouse_has_data()) {
            mouse_handle_interrupt();
        }
        
        // Get current mouse state
        mouse_state_t *mouse = mouse_get_state();
        
        // Print mouse status
        terminal_print("Mouse: (");
        char pos_str[16];
        int_to_string(mouse->x, pos_str);
        terminal_print(pos_str);
        terminal_print(", ");
        int_to_string(mouse->y, pos_str);
        terminal_print(pos_str);
        terminal_print(") Buttons: ");
        
        if (mouse->left_button) terminal_print("L");
        else terminal_print("-");
        if (mouse->right_button) terminal_print("R");
        else terminal_print("-");
        if (mouse->middle_button) terminal_print("M");
        else terminal_print("-");
        
        terminal_print("    \r");
        
        // Simple delay
        for (volatile int i = 0; i < 100000; i++) {
            // Check for more mouse data
            if (mouse_has_data()) {
                mouse_handle_interrupt();
            }
        }
        
        // Check for 'q' key (simplified)
        // In real implementation, you'd need proper keyboard handling
        // For now, just run for a bit then exit
        static int test_count = 0;
        if (++test_count > 100) {
            terminal_print("\nMouse test completed.\n");
            break;
        }
    }
}

// Command: debug window
void cmd_window_debug(const char *args) {
    if (!args || strlen(args) == 0) {
        terminal_print("Usage: wdebug <window_id>\n");
        terminal_print("Shows detailed debug info for a window\n");
        return;
    }
    
    uint32_t window_id = parse_int(args);
    extern window_t *wm_find_window_by_id(uint32_t id);
    window_t *window = wm_find_window_by_id(window_id);
    
    if (!window) {
        terminal_print("Window not found\n");
        return;
    }
    
    terminal_print("=== Window Debug Info ===\n");
    terminal_print("Window ID: ");
    char str[16];
    int_to_string(window->id, str);
    terminal_print(str);
    terminal_print("\n");
    
    terminal_print("Title: ");
    terminal_print(window->title);
    terminal_print("\n");
    
    terminal_print("Position: (");
    int_to_string(window->x, str);
    terminal_print(str);
    terminal_print(", ");
    int_to_string(window->y, str);
    terminal_print(str);
    terminal_print(")\n");
    
    terminal_print("Size: ");
    int_to_string(window->width, str);
    terminal_print(str);
    terminal_print("x");
    int_to_string(window->height, str);
    terminal_print(str);
    terminal_print("\n");
    
    // Calculate close button position
    int close_x = window->x + window->width - 22;
    int close_y = window->y + 2;
    
    terminal_print("Close button: (");
    int_to_string(close_x, str);
    terminal_print(str);
    terminal_print(", ");
    int_to_string(close_y, str);
    terminal_print(str);
    terminal_print(") to (");
    int_to_string(close_x + 20, str);
    terminal_print(str);
    terminal_print(", ");
    int_to_string(close_y + 20, str);
    terminal_print(str);
    terminal_print(")\n");
    
    terminal_print("Title bar: (");
    int_to_string(window->x, str);
    terminal_print(str);
    terminal_print(", ");
    int_to_string(window->y, str);
    terminal_print(str);
    terminal_print(") to (");
    int_to_string(window->x + window->width, str);
    terminal_print(str);
    terminal_print(", ");
    int_to_string(window->y + 24, str);  // WINDOW_TITLE_BAR_HEIGHT
    terminal_print(str);
    terminal_print(")\n");
}

// Register all window manager commands
void register_window_manager_commands(void) {
    register_command("window", cmd_window_create, "Create and manage windows", 
                    "window create <title> [x y width height]", "Window Manager");
    register_command("wlist", cmd_window_list, "List all open windows", 
                    "wlist", "Window Manager");
    register_command("wclose", cmd_window_close, "Close a window", 
                    "wclose <window_id>", "Window Manager");
    register_command("wmove", cmd_window_move, "Move a window", 
                    "wmove <window_id> <x> <y>", "Window Manager");
    register_command("wresize", cmd_window_resize, "Resize a window", 
                    "wresize <window_id> <width> <height>", "Window Manager");
    register_command("wfocus", cmd_window_focus, "Focus a window", 
                    "wfocus <window_id>", "Window Manager");
    register_command("wmax", cmd_window_maximize, "Maximize a window", 
                    "wmax <window_id>", "Window Manager");
    register_command("wmin", cmd_window_minimize, "Minimize a window", 
                    "wmin <window_id>", "Window Manager");
    register_command("wrestore", cmd_window_restore, "Restore a window", 
                    "wrestore <window_id>", "Window Manager");
    register_command("winfo", cmd_window_info, "Show window information", 
                    "winfo <window_id>", "Window Manager");
    register_command("wdemo", cmd_window_demo, "Create window manager demo", 
                    "wdemo", "Window Manager");
    register_command("desktop", cmd_desktop, "Switch to desktop mode", 
                    "desktop", "Window Manager");
    register_command("terminal", cmd_terminal_mode, "Switch to terminal mode", 
                    "terminal", "Window Manager");
    register_command("mousetest", cmd_mouse_test, "Test mouse functionality", 
                    "mousetest", "Window Manager");
    register_command("wdebug", cmd_window_debug, "Debug window information", 
                    "wdebug <window_id>", "Window Manager");
} 