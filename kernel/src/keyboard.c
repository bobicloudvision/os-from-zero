#include "keyboard.h"
#include "mouse.h"
#include "window_manager_rust.h"
#include <stdbool.h>

// PS/2 keyboard scancode to ASCII mapping (US layout)
static const char scancode_to_ascii[128] = {
    0, 0, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
    '*', 0, ' '
};

// PS/2 Status register bits
#define PS2_STATUS_OUTPUT_FULL   0x01
#define PS2_STATUS_AUXILIARY     0x20  // Bit 5: 1 = auxiliary device (mouse), 0 = keyboard

// Port I/O functions
uint8_t inb(uint16_t port) {
    uint8_t result;
    asm volatile("inb %1, %0" : "=a"(result) : "Nd"(port));
    return result;
}

void outb(uint16_t port, uint8_t data) {
    asm volatile("outb %0, %1" : : "a"(data), "Nd"(port));
}

// Initialize keyboard
void keyboard_init(void) {
    // For now, we don't need special initialization
    // The PS/2 keyboard should be ready to use
}

// Read keyboard input
char read_key(void) {
    static uint32_t update_counter = 0;
    
    while (1) {
        uint8_t status = inb(0x64);
        if (status & PS2_STATUS_OUTPUT_FULL) {
            // Check if this data is from the auxiliary device (mouse)
            if (status & PS2_STATUS_AUXILIARY) {
                // This is mouse data, handle it properly instead of discarding
                mouse_handle_interrupt();
                
                // Always update window manager (handles cursor rendering)
                mouse_state_t *mouse = mouse_get_state();
                wm_handle_mouse(mouse->x, mouse->y, mouse->left_button);
                wm_update();
                
                continue;   // Keep waiting for keyboard data
            }
            
            // This is keyboard data
            uint8_t scancode = inb(0x60);
            if (scancode < 128) {
                char c = scancode_to_ascii[scancode];
                if (c != 0) {
                    return c;
                }
            }
        } else {
            // No input available - periodically update window manager
            // This allows animating windows to update independently
            update_counter++;
            // Update more frequently to keep animations smooth (~60 FPS target)
            // At high CPU speeds, 1000 iterations happens very quickly
            if (update_counter >= 1000) {  // Update every ~1000 iterations for smoother animation
                update_counter = 0;
                extern void wm_update(void);
                wm_update();
            }
        }
    }
} 