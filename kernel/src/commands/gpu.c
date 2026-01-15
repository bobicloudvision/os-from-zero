#include "gpu.h"
#include "../gpu_rust.h"
#include "../window_manager_rust.h"
#include "../terminal.h"
#include "../shell.h"
#include "../pci.h"
#include <stddef.h>

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
    
    terminal_print("\nGPU test window created!\n");
    terminal_print("The window shows:\n");
    terminal_print("  - Color rectangles (GPU-accelerated fill)\n");
    terminal_print("  - Gradient pattern (GPU-accelerated pixel operations)\n");
    terminal_print("  - GPU status information\n");
    terminal_print("\nYou can drag the window to test GPU-accelerated blitting.\n");
    terminal_print("Close the window to finish the test.\n");
}

// Register GPU commands
void register_gpu_commands(void) {
    register_command("gpu-test", cmd_gpu_test, 
                     "Test GPU rendering capabilities", 
                     "gpu-test", 
                     "Graphics");
}
