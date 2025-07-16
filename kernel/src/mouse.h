#ifndef MOUSE_H
#define MOUSE_H

#include <stdint.h>
#include <stdbool.h>

// PS/2 mouse commands
#define MOUSE_CMD_ENABLE_DATA_REPORTING 0xF4
#define MOUSE_CMD_DISABLE_DATA_REPORTING 0xF5
#define MOUSE_CMD_SET_DEFAULTS 0xF6
#define MOUSE_CMD_RESET 0xFF
#define MOUSE_CMD_SET_SAMPLE_RATE 0xF3
#define MOUSE_CMD_GET_DEVICE_ID 0xF2

// PS/2 mouse responses
#define MOUSE_ACK 0xFA
#define MOUSE_RESEND 0xFE

// Mouse button flags
#define MOUSE_LEFT_BUTTON   0x01
#define MOUSE_RIGHT_BUTTON  0x02
#define MOUSE_MIDDLE_BUTTON 0x04

// Mouse packet structure (3 bytes for standard PS/2 mouse)
typedef struct {
    uint8_t flags;      // Button states and movement flags
    uint8_t x_movement; // X movement delta
    uint8_t y_movement; // Y movement delta
} mouse_packet_t;

// Mouse state structure
typedef struct {
    int x;              // Current X coordinate
    int y;              // Current Y coordinate
    bool left_button;   // Left button state
    bool right_button;  // Right button state
    bool middle_button; // Middle button state
    bool x_overflow;    // X movement overflow flag
    bool y_overflow;    // Y movement overflow flag
    bool x_negative;    // X movement is negative
    bool y_negative;    // Y movement is negative
} mouse_state_t;

// Mouse driver functions
void mouse_init(void);
bool mouse_has_data(void);
void mouse_handle_interrupt(void);
mouse_state_t* mouse_get_state(void);
void mouse_set_bounds(int max_x, int max_y);

// Internal functions
bool mouse_send_command(uint8_t cmd);
bool mouse_read_data(uint8_t *data);
void mouse_process_packet(mouse_packet_t* packet);

#endif // MOUSE_H 