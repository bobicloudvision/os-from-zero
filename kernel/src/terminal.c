#include "terminal.h"
#include "font.h"
#include "string.h"
#include <stddef.h>

// Global variables for terminal
static struct limine_framebuffer *g_framebuffer;
static int cursor_x = 0;
static int cursor_y = 0;

// Initialize terminal
void terminal_init(struct limine_framebuffer *framebuffer) {
    g_framebuffer = framebuffer;
    cursor_x = 0;
    cursor_y = 0;
}

// Function to draw a character at a specific position
void draw_char(struct limine_framebuffer *framebuffer, char c, int x, int y, uint32_t color) {
    if (c < 32 || c > 126) return; // Only printable ASCII
    
    const uint8_t *glyph = font_8x8[(unsigned char)c];
    volatile uint32_t *fb_ptr = framebuffer->address;
    
    // Scale from 8x8 to 14x14
    for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 8; col++) {
            if (glyph[row] & (1 << (7 - col))) {
                // Scale each pixel to approximately 1.75x1.75 pixels
                int scale_start_x = (col * 14) / 8;
                int scale_end_x = ((col + 1) * 14) / 8;
                int scale_start_y = (row * 14) / 8;
                int scale_end_y = ((row + 1) * 14) / 8;
                
                for (int scaled_y = scale_start_y; scaled_y < scale_end_y; scaled_y++) {
                    for (int scaled_x = scale_start_x; scaled_x < scale_end_x; scaled_x++) {
                        int pixel_x = x + scaled_x;
                        int pixel_y = y + scaled_y;
                        
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
    while (*str) {
        terminal_putchar(*str);
        str++;
    }
} 