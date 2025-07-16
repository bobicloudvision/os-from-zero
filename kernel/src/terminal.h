#ifndef TERMINAL_H
#define TERMINAL_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <limine.h>

// Display constants
#define CHAR_WIDTH 16
#define CHAR_HEIGHT 16
#define TEXT_COLOR 0x003fb950
#define BG_COLOR 0x0d1117

// Font structure for loaded fonts
typedef struct {
    char name[32];
    uint8_t width;
    uint8_t height;
    uint8_t char_data[128][8];  // Support for 128 characters, 8 bytes each
    bool loaded;
} font_t;

// Terminal functions
void terminal_init(struct limine_framebuffer *framebuffer);
void terminal_putchar(char c);
void terminal_print(const char *str);
void clear_screen(void);

// Font functions
bool load_font_from_file(const char *filename, font_t *font);
bool set_current_font(const char *font_name);
void create_default_fonts(void);
void list_available_fonts(void);

// Drawing functions
void draw_char(struct limine_framebuffer *framebuffer, char c, int x, int y, uint32_t color);
void draw_string(struct limine_framebuffer *framebuffer, const char *str, int x, int y, uint32_t color);

#endif // TERMINAL_H 