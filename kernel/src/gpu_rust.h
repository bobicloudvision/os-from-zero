#ifndef GPU_RUST_H
#define GPU_RUST_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// GPU command structure
typedef struct {
    uint32_t command_type;
    uint32_t data[16];
} gpu_command_t;

// Initialize GPU rendering context
void gpu_init(void *framebuffer, uint32_t width, uint32_t height, uint32_t pitch);

// Check if GPU is available
bool gpu_is_available(void);

// GPU-accelerated blitting
void gpu_blit(
    uint32_t *dst,
    uint32_t dst_pitch,
    const uint32_t *src,
    uint32_t src_pitch,
    uint32_t width,
    uint32_t height
);

// GPU-accelerated fill rectangle
void gpu_fill_rect(
    uint32_t *buffer,
    uint32_t pitch,
    int32_t x,
    int32_t y,
    uint32_t width,
    uint32_t height,
    uint32_t color
);

// GPU-accelerated alpha blending
void gpu_alpha_blend(
    uint32_t *dst,
    const uint32_t *src,
    uint32_t width,
    uint32_t height,
    uint8_t alpha
);

// GPU-accelerated rectangle copy
void gpu_copy_rect(
    uint32_t *dst,
    uint32_t dst_pitch,
    int32_t dst_x,
    int32_t dst_y,
    const uint32_t *src,
    uint32_t src_pitch,
    int32_t src_x,
    int32_t src_y,
    uint32_t width,
    uint32_t height
);

// Clear entire framebuffer
void gpu_clear(uint32_t *buffer, uint32_t width, uint32_t height, uint32_t color);

// Render to framebuffer
bool gpu_render_to_framebuffer(
    const uint32_t *src,
    uint32_t src_width,
    uint32_t src_height,
    int32_t dst_x,
    int32_t dst_y
);

// GPU command queue
bool gpu_submit_command(const gpu_command_t *cmd);
void gpu_process_commands(void);

#endif // GPU_RUST_H
