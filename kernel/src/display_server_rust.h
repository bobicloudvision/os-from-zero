#ifndef DISPLAY_SERVER_RUST_H
#define DISPLAY_SERVER_RUST_H

#include <stdint.h>
#include <stdbool.h>
#include <limine.h>

// Surface structure (must match Rust definition)
typedef struct surface {
    uint32_t id;
    int32_t x;
    int32_t y;
    uint32_t width;
    uint32_t height;
    uint32_t *buffer;
    int32_t z_order;
} surface_t;

// Display server functions
void ds_init(struct limine_framebuffer *framebuffer);
surface_t* ds_create_surface(int x, int y, uint32_t width, uint32_t height, int z_order);
void ds_destroy_surface(surface_t *surface);
void ds_set_surface_position(surface_t *surface, int x, int y);
void ds_set_surface_z_order(surface_t *surface, int z_order);
void ds_set_surface_size(surface_t *surface, uint32_t width, uint32_t height);
uint32_t* ds_get_surface_buffer(surface_t *surface);
void ds_mark_dirty(int x, int y, uint32_t width, uint32_t height);
void ds_update_cursor_position(int x, int y);
void ds_render(void);

#endif // DISPLAY_SERVER_RUST_H
