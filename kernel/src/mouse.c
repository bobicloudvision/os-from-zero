#include "mouse.h"
#include "keyboard.h" // For inb/outb functions

#include "terminal.h" // For debug output

// PS/2 Controller ports
#define PS2_DATA_PORT    0x60
#define PS2_STATUS_PORT  0x64
#define PS2_COMMAND_PORT 0x64

// PS/2 Controller commands
#define PS2_CMD_DISABLE_MOUSE    0xA7
#define PS2_CMD_ENABLE_MOUSE     0xA8
#define PS2_CMD_MOUSE_WRITE      0xD4
#define PS2_CMD_MOUSE_READ       0xD3

// PS/2 Status register bits
#define PS2_STATUS_OUTPUT_FULL   0x01
#define PS2_STATUS_INPUT_FULL    0x02
#define PS2_STATUS_AUXILIARY     0x20  // Bit 5: 1 = auxiliary device (mouse), 0 = keyboard

// Mouse state
static mouse_state_t mouse_state = {0};
static int max_x = 1024, max_y = 768;  // Default screen bounds

// Packet handling
static uint8_t packet_buffer[3];
static int packet_byte = 0;
static bool packet_ready = false;

// Wait for PS/2 controller to be ready for input (with timeout)
static bool wait_for_input(void) {
    volatile uint32_t timeout = 100000; // Timeout counter
    while ((inb(PS2_STATUS_PORT) & PS2_STATUS_INPUT_FULL) && timeout > 0) {
        timeout--;
        __asm__ volatile ("nop");
    }
    return timeout > 0; // Return true if succeeded, false if timed out
}

// Wait for PS/2 controller to have output ready (with timeout)
static bool wait_for_output(void) {
    volatile uint32_t timeout = 100000; // Timeout counter
    while (!(inb(PS2_STATUS_PORT) & PS2_STATUS_OUTPUT_FULL) && timeout > 0) {
        timeout--;
        __asm__ volatile ("nop");
    }
    return timeout > 0; // Return true if succeeded, false if timed out
}

// Send command to PS/2 controller
static bool ps2_send_command(uint8_t cmd) {
    if (!wait_for_input()) {
        return false; // Timeout
    }
    outb(PS2_COMMAND_PORT, cmd);
    return true;
}

// Send data to PS/2 controller
static bool ps2_send_data(uint8_t data) {
    if (!wait_for_input()) {
        return false; // Timeout
    }
    outb(PS2_DATA_PORT, data);
    return true;
}

// Read data from PS/2 controller
static bool ps2_read_data(uint8_t *data) {
    if (!wait_for_output()) {
        return false; // Timeout
    }
    *data = inb(PS2_DATA_PORT);
    return true;
}

// Send command to mouse
bool mouse_send_command(uint8_t cmd) {
    if (!ps2_send_command(PS2_CMD_MOUSE_WRITE)) {
        return false;
    }
    return ps2_send_data(cmd);
}

// Read data from mouse
bool mouse_read_data(uint8_t *data) {
    return ps2_read_data(data);
}

// Initialize PS/2 mouse
void mouse_init(void) {
    // Initialize mouse state
    mouse_state.x = max_x / 2;
    mouse_state.y = max_y / 2;
    mouse_state.left_button = false;
    mouse_state.right_button = false;
    mouse_state.middle_button = false;
    mouse_state.x_overflow = false;
    mouse_state.y_overflow = false;
    mouse_state.x_negative = false;
    mouse_state.y_negative = false;
    
    packet_byte = 0;
    packet_ready = false;
    
    // Try to enable auxiliary device (mouse)
    if (!ps2_send_command(PS2_CMD_ENABLE_MOUSE)) {
        // PS2 controller not available, mouse will not work
        return;
    }
    
    // Try to reset mouse
    if (!mouse_send_command(MOUSE_CMD_RESET)) {
        // Mouse reset failed, mouse will not work
        return;
    }
    
    // Wait for acknowledgment
    uint8_t response;
    if (!mouse_read_data(&response) || response != MOUSE_ACK) {
        // Reset failed, mouse will not work
        return;
    }
    
    // Wait for self-test completion (0xAA)
    if (!mouse_read_data(&response) || response != 0xAA) {
        // Self-test failed, mouse will not work
        return;
    }
    
    // Wait for device ID (0x00 for standard mouse)
    if (!mouse_read_data(&response)) {
        // Failed to get device ID, mouse will not work
        return;
    }
    
    // Set mouse defaults
    if (!mouse_send_command(MOUSE_CMD_SET_DEFAULTS)) {
        // Failed to set defaults, mouse will not work
        return;
    }
    
    if (!mouse_read_data(&response)) {
        // Failed to get response, mouse will not work
        return;
    }
    
    // Enable data reporting
    if (!mouse_send_command(MOUSE_CMD_ENABLE_DATA_REPORTING)) {
        // Failed to enable data reporting, mouse will not work
        return;
    }
    
    if (!mouse_read_data(&response)) {
        // Failed to get response, mouse will not work
        return;
    }
    
    // Mouse initialization successful
}

// Check if mouse has data available
bool mouse_has_data(void) {
    uint8_t status = inb(PS2_STATUS_PORT);
    // Check if output is full AND it's from auxiliary device (mouse)
    return (status & PS2_STATUS_OUTPUT_FULL) && (status & PS2_STATUS_AUXILIARY);
}

// Process a complete mouse packet
void mouse_process_packet(mouse_packet_t* packet) {
    // Extract button states
    mouse_state.left_button = (packet->flags & MOUSE_LEFT_BUTTON) != 0;
    mouse_state.right_button = (packet->flags & MOUSE_RIGHT_BUTTON) != 0;
    mouse_state.middle_button = (packet->flags & MOUSE_MIDDLE_BUTTON) != 0;
    
    // Extract movement flags
    mouse_state.x_overflow = (packet->flags & 0x40) != 0;
    mouse_state.y_overflow = (packet->flags & 0x80) != 0;
    mouse_state.x_negative = (packet->flags & 0x10) != 0;
    mouse_state.y_negative = (packet->flags & 0x20) != 0;
    
    // Calculate movement deltas
    int delta_x = packet->x_movement;
    int delta_y = packet->y_movement;
    
    // Handle sign extension for negative movement
    if (mouse_state.x_negative) {
        delta_x = delta_x - 256;
    }
    if (mouse_state.y_negative) {
        delta_y = delta_y - 256;
    }
    
    // Don't update position if overflow occurred
    if (!mouse_state.x_overflow && !mouse_state.y_overflow) {
        // Update mouse position
        mouse_state.x += delta_x;
        mouse_state.y -= delta_y;  // Invert Y axis (screen coordinates)
        
        // Clamp to screen bounds
        if (mouse_state.x < 0) mouse_state.x = 0;
        if (mouse_state.x >= max_x) mouse_state.x = max_x - 1;
        if (mouse_state.y < 0) mouse_state.y = 0;
        if (mouse_state.y >= max_y) mouse_state.y = max_y - 1;
    }
}

// Handle mouse interrupt (should be called from interrupt handler)
void mouse_handle_interrupt(void) {
    if (!mouse_has_data()) {
        return;
    }
    
    uint8_t data;
    if (!mouse_read_data(&data)) {
        return; // Failed to read data
    }
    
    // PS/2 mouse sends 3-byte packets
    if (packet_byte == 0) {
        // First byte: must have bit 3 set (sync bit)
        if (data & 0x08) {
            packet_buffer[packet_byte++] = data;
        }
        // If sync bit not set, discard and wait for next byte
    } else {
        packet_buffer[packet_byte++] = data;
        
        if (packet_byte >= 3) {
            // Complete packet received
            mouse_packet_t packet;
            packet.flags = packet_buffer[0];
            packet.x_movement = packet_buffer[1];
            packet.y_movement = packet_buffer[2];
            
            mouse_process_packet(&packet);
            
            packet_byte = 0;
            packet_ready = true;
        }
    }
}

// Get current mouse state
mouse_state_t* mouse_get_state(void) {
    return &mouse_state;
}

// Set screen bounds for mouse movement
void mouse_set_bounds(int max_x_bound, int max_y_bound) {
    max_x = max_x_bound;
    max_y = max_y_bound;
    
    // Ensure current position is within new bounds
    if (mouse_state.x >= max_x) mouse_state.x = max_x - 1;
    if (mouse_state.y >= max_y) mouse_state.y = max_y - 1;
} 