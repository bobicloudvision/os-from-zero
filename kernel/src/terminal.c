#include "terminal.h"
#include "font.h"
#include "string.h"
#include "mouse.h"
#include <stddef.h>

// Global variables for terminal
static struct limine_framebuffer *g_framebuffer;
static int cursor_x = 0;
static int cursor_y = 0;

// Mouse cursor state
static int last_mouse_x = -1;
static int last_mouse_y = -1;
static uint32_t cursor_backup[(CURSOR_WIDTH + 2) * (CURSOR_HEIGHT + 2)]; // Backup pixels under cursor (including outline)
static bool cursor_backup_valid = false;

// Improved mouse cursor bitmap (12x16 pixels) - using 2 bytes per row for 12 pixels
static const uint16_t mouse_cursor_bitmap[16] = {
    0b110000000000,  // ##
    0b111000000000,  // ###
    0b111100000000,  // ####
    0b111110000000,  // #####
    0b111111000000,  // ######
    0b111111100000,  // #######
    0b111111110000,  // ########
    0b111111111000,  // #########
    0b111111100000,  // #######
    0b111111100000,  // #######
    0b110110000000,  // ## ##
    0b110011000000,  // ##  ##
    0b100001100000,  // #    ##
    0b000001100000,  //      ##
    0b000000110000,  //       ##
    0b000000110000   //       ##
};

// Initialize terminal
void terminal_init(struct limine_framebuffer *framebuffer) {
    g_framebuffer = framebuffer;
    cursor_x = 0;
    cursor_y = 0;
    last_mouse_x = -1;
    last_mouse_y = -1;
    cursor_backup_valid = false;
}

// Function to draw a character at a specific position with high quality
void draw_char(struct limine_framebuffer *framebuffer, char c, int x, int y, uint32_t color) {
    if (c < 32 || c > 126) return; // Only printable ASCII
    
    const uint8_t *glyph = font_8x8[(unsigned char)c];
    volatile uint32_t *fb_ptr = framebuffer->address;
    
    // Clean 2x scaling from 8x8 to 16x16 - each pixel becomes a 2x2 block
    for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 8; col++) {
            if (glyph[row] & (1 << (7 - col))) {
                // Each source pixel becomes a 2x2 block in the output
                for (int scale_y = 0; scale_y < 2; scale_y++) {
                    for (int scale_x = 0; scale_x < 2; scale_x++) {
                        int pixel_x = x + (col * 2) + scale_x;
                        int pixel_y = y + (row * 2) + scale_y;
                        
                        if (pixel_x < (int)framebuffer->width && pixel_y < (int)framebuffer->height) {
                            fb_ptr[pixel_y * (framebuffer->pitch / 4) + pixel_x] = color;
                        }
                    }
                }
            }
        }
    }
}

// Function to draw a string
void draw_string(struct limine_framebuffer *framebuffer, const char *str, int x, int y, uint32_t color) {
    int current_x = x;
    
    while (*str) {
        draw_char(framebuffer, *str, current_x, y, color);
        current_x += CHAR_WIDTH; // Move to next character position (14 pixels wide)
        str++;
    }
}

// Clear screen
void clear_screen(void) {
    volatile uint32_t *fb_ptr = g_framebuffer->address;
    for (size_t i = 0; i < g_framebuffer->width * g_framebuffer->height; i++) {
        fb_ptr[i] = BG_COLOR;
    }
    cursor_x = 0;
    cursor_y = 0;
    
    // Invalidate cursor backup since screen was cleared
    cursor_backup_valid = false;
    last_mouse_x = -1;
    last_mouse_y = -1;
}

// Print character to terminal
void terminal_putchar(char c) {
    if (c == '\n') {
        cursor_x = 0;
        cursor_y += CHAR_HEIGHT;
        if (cursor_y >= (int)g_framebuffer->height - CHAR_HEIGHT) {
            cursor_y = (int)g_framebuffer->height - CHAR_HEIGHT;
            // Simple scroll: clear screen and start from top
            clear_screen();
        }
    } else if (c == '\b') {
        if (cursor_x > 0) {
            cursor_x -= CHAR_WIDTH;
            // Clear the character
            for (int y = cursor_y; y < cursor_y + CHAR_HEIGHT; y++) {
                for (int x = cursor_x; x < cursor_x + CHAR_WIDTH; x++) {
                    if (x < (int)g_framebuffer->width && y < (int)g_framebuffer->height) {
                        volatile uint32_t *fb_ptr = g_framebuffer->address;
                        fb_ptr[y * (g_framebuffer->pitch / 4) + x] = BG_COLOR;
                    }
                }
            }
        }
    } else {
        draw_char(g_framebuffer, c, cursor_x, cursor_y, TEXT_COLOR);
        cursor_x += CHAR_WIDTH;
        if (cursor_x >= (int)g_framebuffer->width - CHAR_WIDTH) {
            cursor_x = 0;
            cursor_y += CHAR_HEIGHT;
            if (cursor_y >= (int)g_framebuffer->height - CHAR_HEIGHT) {
                cursor_y = (int)g_framebuffer->height - CHAR_HEIGHT;
                clear_screen();
            }
        }
    }
}

// Print string to terminal
void terminal_print(const char *str) {
    static int char_count = 0;
    while (*str) {
        terminal_putchar(*str);
        str++;
        
        // Check for mouse events every 10 characters to keep cursor responsive
        if (++char_count % 10 == 0) {
            extern bool mouse_has_data(void);
            extern void mouse_handle_interrupt(void);
            
            // Quick mouse check - don't do too many iterations to avoid slowing down text output
            for (int i = 0; i < 3; i++) {
                if (mouse_has_data()) {
                    mouse_handle_interrupt();
                    update_mouse_cursor();
                }
            }
        }
    }
} 

// Draw a single pixel
void draw_pixel(int x, int y, uint32_t color) {
    if (x < 0 || y < 0 || x >= (int)g_framebuffer->width || y >= (int)g_framebuffer->height) {
        return;
    }
    
    volatile uint32_t *fb_ptr = g_framebuffer->address;
    fb_ptr[y * (g_framebuffer->pitch / 4) + x] = color;
}

// Save pixels under cursor before drawing
static void save_cursor_background(int x, int y) {
    int backup_width = CURSOR_WIDTH + 2;  // Include outline
    int backup_height = CURSOR_HEIGHT + 2; // Include outline
    
    volatile uint32_t *fb_ptr = g_framebuffer->address;
    
    for (int row = 0; row < backup_height; row++) {
        for (int col = 0; col < backup_width; col++) {
            int pixel_x = x + col - 1; // -1 for outline
            int pixel_y = y + row - 1; // -1 for outline
            
            if (pixel_x >= 0 && pixel_y >= 0 && 
                pixel_x < (int)g_framebuffer->width && 
                pixel_y < (int)g_framebuffer->height) {
                cursor_backup[row * backup_width + col] = 
                    fb_ptr[pixel_y * (g_framebuffer->pitch / 4) + pixel_x];
            } else {
                // If outside screen bounds, save background color
                cursor_backup[row * backup_width + col] = BG_COLOR;
            }
        }
    }
    cursor_backup_valid = true;
}

// Draw mouse cursor at given position
void draw_mouse_cursor(int x, int y) {
    // Save background pixels before drawing
    save_cursor_background(x, y);
    
    // First draw black outline (border)
    for (int row = 0; row < CURSOR_HEIGHT; row++) {
        uint16_t bitmap_row = mouse_cursor_bitmap[row];
        for (int col = 0; col < 12; col++) {
            if (bitmap_row & (1 << (11 - col))) {
                // Draw black pixels around the white pixel for outline
                for (int dy = -1; dy <= 1; dy++) {
                    for (int dx = -1; dx <= 1; dx++) {
                        if (dx == 0 && dy == 0) continue; // Skip center pixel
                        draw_pixel(x + col + dx, y + row + dy, 0x00000000); // Black outline
                    }
                }
            }
        }
    }
    
    // Then draw the white cursor on top
    for (int row = 0; row < CURSOR_HEIGHT; row++) {
        uint16_t bitmap_row = mouse_cursor_bitmap[row];
        for (int col = 0; col < 12; col++) {
            if (bitmap_row & (1 << (11 - col))) {
                draw_pixel(x + col, y + row, CURSOR_COLOR); // White cursor
            }
        }
    }
}

// Clear mouse cursor at given position
void clear_mouse_cursor(int x, int y) {
    if (!cursor_backup_valid) {
        // If no backup available, just clear to background color (fallback)
        for (int row = -1; row <= CURSOR_HEIGHT; row++) {
            for (int col = -1; col <= 12; col++) {
                draw_pixel(x + col, y + row, BG_COLOR);
            }
        }
        return;
    }
    
    // Restore saved background pixels
    int backup_width = CURSOR_WIDTH + 2;  // Include outline
    int backup_height = CURSOR_HEIGHT + 2; // Include outline
    
    volatile uint32_t *fb_ptr = g_framebuffer->address;
    
    for (int row = 0; row < backup_height; row++) {
        for (int col = 0; col < backup_width; col++) {
            int pixel_x = x + col - 1; // -1 for outline
            int pixel_y = y + row - 1; // -1 for outline
            
            if (pixel_x >= 0 && pixel_y >= 0 && 
                pixel_x < (int)g_framebuffer->width && 
                pixel_y < (int)g_framebuffer->height) {
                fb_ptr[pixel_y * (g_framebuffer->pitch / 4) + pixel_x] = 
                    cursor_backup[row * backup_width + col];
            }
        }
    }
    
    cursor_backup_valid = false; // Mark backup as used
}

// Update mouse cursor position
void update_mouse_cursor(void) {
    mouse_state_t *mouse = mouse_get_state();
    
    // Debug: Print cursor position (only if position changed significantly)
    static int debug_last_x = -1, debug_last_y = -1;
    if (debug_last_x == -1 || 
        (mouse->x - debug_last_x) * (mouse->x - debug_last_x) + 
        (mouse->y - debug_last_y) * (mouse->y - debug_last_y) > 100) {
        // Only print debug info if cursor moved significantly (more than 10 pixels)
        debug_last_x = mouse->x;
        debug_last_y = mouse->y;
        // Note: We don't print debug here as it would interfere with normal operation
    }
    
    // Clear previous cursor position only if cursor actually moved
    if (last_mouse_x >= 0 && last_mouse_y >= 0 && 
        (last_mouse_x != mouse->x || last_mouse_y != mouse->y)) {
        clear_mouse_cursor(last_mouse_x, last_mouse_y);
    }
    
    // Draw cursor at new position (only if position changed or first time)
    if (last_mouse_x != mouse->x || last_mouse_y != mouse->y || 
        last_mouse_x == -1 || last_mouse_y == -1) {
        draw_mouse_cursor(mouse->x, mouse->y);
        
        // Update last position
        last_mouse_x = mouse->x;
        last_mouse_y = mouse->y;
    }
} 