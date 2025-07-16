#include "keyboard.h"

// PS/2 keyboard scancode to ASCII mapping (US layout)
static const char scancode_to_ascii[128] = {
    0, 0, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
    '*', 0, ' '
};

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
    while (1) {
        uint8_t status = inb(0x64);
        if (status & 0x01) {
            uint8_t scancode = inb(0x60);
            if (scancode < 128) {
                char c = scancode_to_ascii[scancode];
                if (c != 0) {
                    return c;
                }
            }
        }
    }
} 