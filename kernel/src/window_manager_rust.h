#ifndef WINDOW_MANAGER_RUST_H
#define WINDOW_MANAGER_RUST_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <limine.h>

// Window flags
#define WINDOW_MOVABLE    0x01
#define WINDOW_CLOSABLE   0x02
#define WINDOW_RESIZABLE  0x04

// Window structure (must match Rust definition)
typedef struct window {
    uint32_t id;
    int32_t x;
    int32_t y;
    uint32_t width;
    uint32_t height;
    char title[64];
    uint32_t flags;
    bool focused;
    bool invalidated;
    void (*draw_callback)(struct window *window);
    void *user_data;
    uint32_t *buffer;
} window_t;

// Window manager functions
void wm_init(struct limine_framebuffer *framebuffer);
window_t* wm_create_window(const char *title, int x, int y, uint32_t width, uint32_t height, uint32_t flags);
void wm_destroy_window(window_t *window);
void wm_invalidate_window(window_t *window);
void wm_clear_window(window_t *window, uint32_t color);
void wm_draw_pixel_to_window(window_t *window, int x, int y, uint32_t color);
void wm_draw_filled_rect_to_window(window_t *window, int x, int y, uint32_t width, uint32_t height, uint32_t color);
void wm_draw_rect_to_window(window_t *window, int x, int y, uint32_t width, uint32_t height, uint32_t color);
void wm_draw_text_to_window(window_t *window, const char *text, int x, int y, uint32_t color);
void wm_handle_mouse(int mouse_x, int mouse_y, bool left_button);
void wm_update(void);
int wm_get_window_count(void);
void wm_get_window_info(int index, int *x, int *y, int *w, int *h, char *title);
bool wm_load_wallpaper(const char *filename);

#endif // WINDOW_MANAGER_RUST_H
