#include "gpu.h"
#include "../gpu_rust.h"
#include "../window_manager_rust.h"
#include "../terminal.h"
#include "../shell.h"
#include "../pci.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

// 3D Animation state
typedef struct {
    float angle_x;
    float angle_y;
    float angle_z;
    uint32_t frame_count;
    bool animating;
} gpu_3d_state_t;

// Simple delay function for animation
static void delay_animation(uint32_t iterations) {
    for (volatile uint32_t i = 0; i < iterations; i++) {
        __asm__ volatile ("nop");
    }
}

// 3D Vector structure
typedef struct {
    float x, y, z;
} vec3_t;

// 3D point structure
typedef struct {
    float x, y, z;
} point3d_t;

// Simple sin/cos approximations using Taylor series (first few terms)
// sin(x) ≈ x - x³/6 + x⁵/120
// cos(x) ≈ 1 - x²/2 + x⁴/24
static float fast_sin(float x) {
    // Normalize to [-π, π]
    while (x > 3.14159f) x -= 6.28318f;
    while (x < -3.14159f) x += 6.28318f;
    
    float x2 = x * x;
    float x3 = x2 * x;
    float x5 = x3 * x2;
    return x - (x3 / 6.0f) + (x5 / 120.0f);
}

static float fast_cos(float x) {
    // Normalize to [-π, π]
    while (x > 3.14159f) x -= 6.28318f;
    while (x < -3.14159f) x += 6.28318f;
    
    float x2 = x * x;
    float x4 = x2 * x2;
    return 1.0f - (x2 / 2.0f) + (x4 / 24.0f);
}

// Rotate point around X axis
static void rotate_x(point3d_t *p, float angle) {
    float cos_a = fast_cos(angle);
    float sin_a = fast_sin(angle);
    float y = p->y * cos_a - p->z * sin_a;
    float z = p->y * sin_a + p->z * cos_a;
    p->y = y;
    p->z = z;
}

// Rotate point around Y axis
static void rotate_y(point3d_t *p, float angle) {
    float cos_a = fast_cos(angle);
    float sin_a = fast_sin(angle);
    float x = p->x * cos_a + p->z * sin_a;
    float z = -p->x * sin_a + p->z * cos_a;
    p->x = x;
    p->z = z;
}

// Rotate point around Z axis
static void rotate_z(point3d_t *p, float angle) {
    float cos_a = fast_cos(angle);
    float sin_a = fast_sin(angle);
    float x = p->x * cos_a - p->y * sin_a;
    float y = p->x * sin_a + p->y * cos_a;
    p->x = x;
    p->y = y;
}

// Project 3D point to 2D screen coordinates
static void project_3d(point3d_t *p3d, int *x, int *y, int center_x, int center_y, float scale) {
    // Simple perspective projection
    float distance = 5.0f;
    float z = p3d->z + distance;
    if (z > 0.1f) {
        *x = center_x + (int)(p3d->x * scale / z);
        *y = center_y + (int)(p3d->y * scale / z);
    } else {
        *x = center_x;
        *y = center_y;
    }
}

// Draw a line between two 3D points
static void draw_3d_line(window_t *win, point3d_t p1, point3d_t p2, 
                         int center_x, int center_y, float scale, uint32_t color) {
    int x1, y1, x2, y2;
    project_3d(&p1, &x1, &y1, center_x, center_y, scale);
    project_3d(&p2, &x2, &y2, center_x, center_y, scale);
    
    // Clamp to window bounds
    if (x1 < 0 || x1 >= (int)win->width || y1 < 20 || y1 >= (int)win->height) return;
    if (x2 < 0 || x2 >= (int)win->width || y2 < 20 || y2 >= (int)win->height) return;
    
    // Bresenham's line algorithm
    int dx = (x2 > x1) ? (x2 - x1) : (x1 - x2);
    int dy = (y2 > y1) ? (y2 - y1) : (y1 - y2);
    int sx = (x1 < x2) ? 1 : -1;
    int sy = (y1 < y2) ? 1 : -1;
    int err = dx - dy;
    
    int x = x1, y = y1;
    
    while (1) {
        // Draw pixel if within bounds
        if (x >= 0 && x < (int)win->width && y >= 20 && y < (int)win->height) {
            wm_draw_pixel_to_window(win, x, y, color);
        }
        
        if (x == x2 && y == y2) break;
        
        int e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x += sx;
        }
        if (e2 < dx) {
            err += dx;
            y += sy;
        }
    }
}

// Draw 3D cube wireframe
static void draw_3d_cube(window_t *win, gpu_3d_state_t *state, 
                         int center_x, int center_y, float size, float scale) {
    // Define cube vertices (centered at origin)
    point3d_t vertices[8] = {
        {-size, -size, -size}, {size, -size, -size},
        {size, size, -size}, {-size, size, -size},
        {-size, -size, size}, {size, -size, size},
        {size, size, size}, {-size, size, size}
    };
    
    // Rotate all vertices
    for (int i = 0; i < 8; i++) {
        rotate_x(&vertices[i], state->angle_x);
        rotate_y(&vertices[i], state->angle_y);
        rotate_z(&vertices[i], state->angle_z);
    }
    
    // Draw edges of the cube
    uint32_t color = 0x00ffff; // Cyan
    
    // Front face
    draw_3d_line(win, vertices[0], vertices[1], center_x, center_y, scale, color);
    draw_3d_line(win, vertices[1], vertices[2], center_x, center_y, scale, color);
    draw_3d_line(win, vertices[2], vertices[3], center_x, center_y, scale, color);
    draw_3d_line(win, vertices[3], vertices[0], center_x, center_y, scale, color);
    
    // Back face
    draw_3d_line(win, vertices[4], vertices[5], center_x, center_y, scale, color);
    draw_3d_line(win, vertices[5], vertices[6], center_x, center_y, scale, color);
    draw_3d_line(win, vertices[6], vertices[7], center_x, center_y, scale, color);
    draw_3d_line(win, vertices[7], vertices[4], center_x, center_y, scale, color);
    
    // Connecting edges
    draw_3d_line(win, vertices[0], vertices[4], center_x, center_y, scale, color);
    draw_3d_line(win, vertices[1], vertices[5], center_x, center_y, scale, color);
    draw_3d_line(win, vertices[2], vertices[6], center_x, center_y, scale, color);
    draw_3d_line(win, vertices[3], vertices[7], center_x, center_y, scale, color);
}

// Draw callback for 3D animation window
static void draw_3d_animation(window_t *win) {
    if (!win || !win->user_data) {
        return;
    }
    
    gpu_3d_state_t *state = (gpu_3d_state_t *)win->user_data;
    
    // Clear window content area (below title bar)
    wm_clear_window(win, 0x000000);
    
    // Calculate center and scale
    int center_x = win->width / 2;
    int center_y = (win->height / 2) + 10; // Offset for title bar
    float cube_size = 1.0f;
    float scale = 200.0f;
    
    // Draw the 3D cube
    draw_3d_cube(win, state, center_x, center_y, cube_size, scale);
    
    // Update rotation angles for animation
    if (state->animating) {
        state->angle_x += 0.05f;
        state->angle_y += 0.03f;
        state->angle_z += 0.02f;
        
        // Keep angles in reasonable range
        if (state->angle_x > 6.28f) state->angle_x -= 6.28f;
        if (state->angle_y > 6.28f) state->angle_y -= 6.28f;
        if (state->angle_z > 6.28f) state->angle_z -= 6.28f;
        
        state->frame_count++;
    }
    
    // Draw info text
    wm_draw_text_to_window(win, "3D Cube Animation", 10, 25, 0xffffff);
    
    // Draw frame counter
    char frame_str[32] = "Frame: ";
    int frame_num = state->frame_count;
    int pos = 7;
    if (frame_num == 0) {
        frame_str[pos++] = '0';
    } else {
        char temp[16];
        int j = 0;
        while (frame_num > 0) {
            temp[j++] = '0' + (frame_num % 10);
            frame_num /= 10;
        }
        while (j > 0) {
            frame_str[pos++] = temp[--j];
        }
    }
    frame_str[pos] = '\0';
    wm_draw_text_to_window(win, frame_str, 10, win->height - 30, 0x00ff00);
}

// GPU test command - demonstrates GPU rendering capabilities
void cmd_gpu_test(const char *args) {
    (void)args; // Unused parameter
    
    terminal_print("=== GPU Rendering Test ===\n\n");
    
    // Check if GPU is available
    bool gpu_available = gpu_is_available();
    terminal_print("GPU Status: ");
    if (gpu_available) {
        terminal_print("AVAILABLE (Hardware acceleration enabled)\n");
    } else {
        terminal_print("NOT AVAILABLE (Using CPU fallback)\n");
    }
    
    // Check PCI devices
    terminal_print("\nPCI Device Scan:\n");
    int device_count = pci_get_device_count();
    terminal_print("Total PCI devices found: ");
    
    // Simple integer to string conversion
    char count_str[16];
    int temp = device_count;
    int i = 0;
    if (temp == 0) {
        count_str[i++] = '0';
    } else {
        char temp_str[16];
        int j = 0;
        while (temp > 0) {
            temp_str[j++] = '0' + (temp % 10);
            temp /= 10;
        }
        while (j > 0) {
            count_str[i++] = temp_str[--j];
        }
    }
    count_str[i] = '\0';
    terminal_print(count_str);
    terminal_print("\n");
    
    // Look for display devices
    struct pci_device *display = pci_find_class(0x03, 0x00); // Display class
    if (display) {
        terminal_print("Display device found on PCI bus ");
        // Print bus number
        char bus_str[16];
        temp = display->bus;
        i = 0;
        if (temp == 0) {
            bus_str[i++] = '0';
        } else {
            char temp_str[16];
            int j = 0;
            while (temp > 0) {
                temp_str[j++] = '0' + (temp % 10);
                temp /= 10;
            }
            while (j > 0) {
                bus_str[i++] = temp_str[--j];
            }
        }
        bus_str[i] = '\0';
        terminal_print(bus_str);
        terminal_print("\n");
    } else {
        terminal_print("No display device found on PCI bus\n");
    }
    
    // Create a test window to demonstrate GPU rendering
    terminal_print("\nCreating GPU test window...\n");
    window_t *test_window = wm_create_window("GPU Test Window", 200, 150, 400, 300, 
                                             WINDOW_MOVABLE | WINDOW_CLOSABLE);
    
    if (!test_window) {
        terminal_print("Error: Failed to create test window\n");
        return;
    }
    
    // Clear window with dark background
    wm_clear_window(test_window, 0x1a1a1a);
    
    // Draw test pattern using GPU-accelerated operations
    terminal_print("Drawing GPU test pattern...\n");
    
    // Draw colored rectangles (these will use GPU acceleration if available)
    wm_draw_filled_rect_to_window(test_window, 20, 40, 100, 60, 0xff0000);  // Red
    wm_draw_filled_rect_to_window(test_window, 140, 40, 100, 60, 0x00ff00);  // Green
    wm_draw_filled_rect_to_window(test_window, 260, 40, 100, 60, 0x0000ff); // Blue
    
    wm_draw_filled_rect_to_window(test_window, 20, 120, 100, 60, 0xffff00);  // Yellow
    wm_draw_filled_rect_to_window(test_window, 140, 120, 100, 60, 0xff00ff); // Magenta
    wm_draw_filled_rect_to_window(test_window, 260, 120, 100, 60, 0x00ffff);  // Cyan
    
    // Draw borders
    wm_draw_rect_to_window(test_window, 20, 40, 100, 60, 0xffffff);
    wm_draw_rect_to_window(test_window, 140, 40, 100, 60, 0xffffff);
    wm_draw_rect_to_window(test_window, 260, 40, 100, 60, 0xffffff);
    wm_draw_rect_to_window(test_window, 20, 120, 100, 60, 0xffffff);
    wm_draw_rect_to_window(test_window, 140, 120, 100, 60, 0xffffff);
    wm_draw_rect_to_window(test_window, 260, 120, 100, 60, 0xffffff);
    
    // Draw text information
    if (gpu_available) {
        wm_draw_text_to_window(test_window, "GPU: ENABLED", 20, 200, 0x00ff00);
        wm_draw_text_to_window(test_window, "Hardware acceleration active", 20, 220, 0xffffff);
    } else {
        wm_draw_text_to_window(test_window, "GPU: DISABLED", 20, 200, 0xff0000);
        wm_draw_text_to_window(test_window, "Using CPU rendering", 20, 220, 0xffffff);
    }
    
    wm_draw_text_to_window(test_window, "Test Pattern", 20, 20, 0xffffff);
    
    // Draw a gradient pattern to test GPU performance
    terminal_print("Drawing gradient pattern (GPU stress test)...\n");
    for (int y = 0; y < 50; y++) {
        for (int x = 0; x < 340; x++) {
            uint32_t r = (x * 255) / 340;
            uint32_t g = (y * 255) / 50;
            uint32_t b = 128;
            uint32_t color = (r << 16) | (g << 8) | b;
            wm_draw_pixel_to_window(test_window, 20 + x, 190 + y, color);
        }
    }
    
    // Force window update
    wm_invalidate_window(test_window);
    wm_update();
    
    // Create 3D animation window
    terminal_print("\nCreating 3D animation window...\n");
    window_t *anim_window = wm_create_window("3D GPU Animation", 250, 200, 400, 350, 
                                             WINDOW_MOVABLE | WINDOW_CLOSABLE);
    
    if (anim_window) {
        // Allocate animation state
        static gpu_3d_state_t anim_state = {
            .angle_x = 0.0f,
            .angle_y = 0.0f,
            .angle_z = 0.0f,
            .frame_count = 0,
            .animating = true
        };
        
        // Set draw callback and user data
        anim_window->draw_callback = draw_3d_animation;
        anim_window->user_data = &anim_state;
        
        // Initial draw
        wm_invalidate_window(anim_window);
        wm_update();
        
        terminal_print("Running 3D animation (60 frames)...\n");
        
        // Animate for 60 frames
        for (int frame = 0; frame < 60; frame++) {
            wm_invalidate_window(anim_window);
            wm_update();
            
            // Small delay for animation speed
            delay_animation(500000);
            
            // Check if window still exists
            extern int wm_get_window_count(void);
            if (wm_get_window_count() == 0) {
                break; // Window was closed
            }
        }
        
        terminal_print("3D animation complete!\n");
    }
    
    terminal_print("\nGPU test windows created!\n");
    terminal_print("The windows show:\n");
    terminal_print("  - Color rectangles (GPU-accelerated fill)\n");
    terminal_print("  - Gradient pattern (GPU-accelerated pixel operations)\n");
    terminal_print("  - 3D rotating cube animation (GPU-accelerated rendering)\n");
    terminal_print("  - GPU status information\n");
    terminal_print("\nYou can drag the windows to test GPU-accelerated blitting.\n");
    terminal_print("Close the windows to finish the test.\n");
}

// Register GPU commands
void register_gpu_commands(void) {
    register_command("gpu-test", cmd_gpu_test, 
                     "Test GPU rendering capabilities", 
                     "gpu-test", 
                     "Graphics");
}
