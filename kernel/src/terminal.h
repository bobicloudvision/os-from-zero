#ifndef TERMINAL_H
#define TERMINAL_H

#include <stdint.h>
#include <limine.h>

// Terminal constants
#define CHAR_WIDTH 8
#define CHAR_HEIGHT 8
#define TEXT_COLOR 0xFFFFFF
#define BG_COLOR 0x000000

// Terminal functions
void terminal_init(struct limine_framebuffer *framebuffer);
void terminal_putchar(char c);
void terminal_print(const char *str);
void clear_screen(void);

// Drawing functions
void draw_char(struct limine_framebuffer *framebuffer, char c, int x, int y, uint32_t color);
void draw_string(struct limine_framebuffer *framebuffer, const char *str, int x, int y, uint32_t color);

#endif 