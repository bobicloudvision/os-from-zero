#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <stdint.h>

// Keyboard functions
void keyboard_init(void);
char read_key(void);

// Port I/O functions
uint8_t inb(uint16_t port);
void outb(uint16_t port, uint8_t data);

#endif 