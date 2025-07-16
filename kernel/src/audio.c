#include "audio.h"
#include <stdint.h>
#include <stddef.h>

// PIT (Programmable Interval Timer) ports
#define PIT_CHANNEL_0   0x40    // Channel 0 data port (IRQ0)
#define PIT_CHANNEL_1   0x41    // Channel 1 data port (RAM refresh)
#define PIT_CHANNEL_2   0x42    // Channel 2 data port (PC Speaker)
#define PIT_COMMAND     0x43    // Command port

// Keyboard controller port for speaker control
#define KB_CONTROLLER_PORT  0x61

// PIT frequency (1.193182 MHz)
#define PIT_FREQUENCY   1193182

// Port I/O functions
static inline void outb(uint16_t port, uint8_t value) {
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t value;
    __asm__ volatile ("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

// Simple delay function (approximate)
static void delay(uint32_t milliseconds) {
    // More accurate delay - calibrated for typical x86_64 systems
    volatile uint32_t count = milliseconds * 100000;  // Increased multiplier
    while (count--) {
        __asm__ volatile ("nop");
    }
}

void audio_init(void) {
    // PC Speaker is ready to use - no special initialization needed
    // Just make sure it's initially off
    audio_stop();
    
    // Test if PC Speaker is working with a very brief test tone
    // This helps verify hardware is accessible
    uint8_t port_value = inb(KB_CONTROLLER_PORT);
    outb(KB_CONTROLLER_PORT, port_value | 0x03);  // Enable speaker
    
    // Very brief test (just a few milliseconds)
    for (volatile int i = 0; i < 10000; i++);
    
    // Disable speaker
    audio_stop();
}

void audio_beep(uint16_t frequency, uint32_t duration_ms) {
    if (frequency == 0) {
        audio_stop();
        return;
    }

    // Ensure frequency is within valid range
    if (frequency < 37 || frequency > 32767) {
        return;
    }

    // Calculate divisor for the PIT
    uint16_t divisor = PIT_FREQUENCY / frequency;
    
    // Ensure divisor is not zero
    if (divisor == 0) {
        divisor = 1;
    }
    
    // Configure PIT Channel 2 for square wave generation
    // Command: 10110110 (0xB6)
    // - Channel 2 (bits 7-6: 10)
    // - Access mode: Low byte then high byte (bits 5-4: 11)
    // - Operating mode: Square wave (bits 3-1: 011)
    // - Binary mode (bit 0: 0)
    outb(PIT_COMMAND, 0xB6);
    
    // Small delay to ensure command is processed
    for (volatile int i = 0; i < 10; i++);
    
    // Send the divisor (low byte first, then high byte)
    outb(PIT_CHANNEL_2, divisor & 0xFF);
    outb(PIT_CHANNEL_2, (divisor >> 8) & 0xFF);
    
    // Small delay to ensure data is processed
    for (volatile int i = 0; i < 10; i++);
    
    // Enable speaker output
    uint8_t port_value = inb(KB_CONTROLLER_PORT);
    outb(KB_CONTROLLER_PORT, port_value | 0x03);  // Set bits 0 and 1
    
    // Wait for the specified duration
    if (duration_ms > 0) {
        delay(duration_ms);
        audio_stop();
    }
}

void audio_stop(void) {
    // Disable speaker output
    uint8_t port_value = inb(KB_CONTROLLER_PORT);
    outb(KB_CONTROLLER_PORT, port_value & 0xFC);  // Clear bits 0 and 1
}

void audio_play_tone(uint16_t frequency) {
    // Play continuous tone until stopped
    audio_beep(frequency, 0);
}

void audio_play_melody(const audio_note_t *notes, size_t count) {
    for (size_t i = 0; i < count; i++) {
        if (notes[i].frequency == 0) {
            // Rest note
            audio_stop();
            delay(notes[i].duration_ms);
        } else {
            // Play note
            audio_beep(notes[i].frequency, notes[i].duration_ms);
        }
    }
}

void audio_system_beep(void) {
    // Standard system beep: 1000 Hz for 200ms
    audio_beep(1000, 200);
}

void audio_error_beep(void) {
    // Error beep: 500 Hz for 500ms
    audio_beep(500, 500);
}

void audio_startup_sound(void) {
    // Simple startup melody
    audio_note_t startup[] = {
        {523, 200},  // C5
        {659, 200},  // E5
        {784, 200},  // G5
        {1047, 400}  // C6
    };
    audio_play_melody(startup, sizeof(startup) / sizeof(startup[0]));
}

void audio_shutdown_sound(void) {
    // Simple shutdown melody
    audio_note_t shutdown[] = {
        {1047, 200}, // C6
        {784, 200},  // G5
        {659, 200},  // E5
        {523, 400}   // C5
    };
    audio_play_melody(shutdown, sizeof(shutdown) / sizeof(shutdown[0]));
}

// Debug function to test hardware directly
void audio_debug_test(void) {
    // Test 1: Direct port access
    uint8_t port_value = inb(KB_CONTROLLER_PORT);
    
    // Test 2: Simple square wave at 1000 Hz
    uint16_t divisor = PIT_FREQUENCY / 1000;
    
    // Configure PIT
    outb(PIT_COMMAND, 0xB6);
    for (volatile int i = 0; i < 100; i++);
    
    outb(PIT_CHANNEL_2, divisor & 0xFF);
    outb(PIT_CHANNEL_2, (divisor >> 8) & 0xFF);
    for (volatile int i = 0; i < 100; i++);
    
    // Enable speaker for longer duration
    outb(KB_CONTROLLER_PORT, port_value | 0x03);
    
    // Long delay to ensure we hear it
    for (volatile int i = 0; i < 10000000; i++);
    
    // Disable speaker
    outb(KB_CONTROLLER_PORT, port_value & 0xFC);
}