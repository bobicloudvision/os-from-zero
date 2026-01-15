#include "shell.h"
#include "commands/system.h"
#include "commands/filesystem.h"
#include "commands/audio.h"
#include "commands/game.h"
#include "commands/execution.h"
#include "commands/window_example.h"
#include "terminal.h"
#include "keyboard.h"
#include "mouse.h"
#include "string.h"
#include "audio.h"
#include <stddef.h>
#include <stdbool.h>

// Input handling
static char input_buffer[256];
static size_t input_pos = 0;

// Helper function to check and handle mouse events
static void check_mouse_events(void) {
    // Check for mouse events multiple times for better responsiveness
    for (int i = 0; i < 10; i++) {
        if (mouse_has_data()) {
            mouse_handle_interrupt();
        }
    }
    
    // Handle window manager mouse and update
    extern void wm_handle_mouse(int mouse_x, int mouse_y, bool left_button);
    extern void wm_update(void);
    
    mouse_state_t *mouse = mouse_get_state();
    
    // Always handle mouse and update window manager
    // Window manager now handles cursor rendering in all cases
    wm_handle_mouse(mouse->x, mouse->y, mouse->left_button);
    wm_update();
}

// Command registry
command_t commands[MAX_COMMANDS];
size_t command_count = 0;

// Register a new command
bool register_command(const char *name, command_func_t func, const char *description, const char *usage, const char *category) {
    if (command_count >= MAX_COMMANDS) {
        return false;
    }
    
    commands[command_count].name = name;
    commands[command_count].func = func;
    commands[command_count].description = description;
    commands[command_count].usage = usage;
    commands[command_count].category = category;
    command_count++;
    
    return true;
}

// Find a command by name
static command_t* find_command(const char *name) {
    for (size_t i = 0; i < command_count; i++) {
        if (strcmp(commands[i].name, name) == 0) {
            return &commands[i];
        }
    }
    return NULL;
}

// Parse and execute commands
void execute_command(const char *cmd) {
    if (strlen(cmd) == 0) {
        return;
    }
    
    // Find the first space to separate command from arguments
    const char *space = cmd;
    while (*space && *space != ' ') {
        space++;
    }
    
    // Extract command name
    char command_name[64];
    size_t cmd_len = space - cmd;
    if (cmd_len >= sizeof(command_name)) {
        terminal_print("Command name too long.\n");
        return;
    }
    
    for (size_t i = 0; i < cmd_len; i++) {
        command_name[i] = cmd[i];
    }
    command_name[cmd_len] = '\0';
    
    // Skip spaces to get arguments
    while (*space == ' ') {
        space++;
    }
    
    // Find and execute the command
    command_t *command = find_command(command_name);
    if (command) {
        command->func((*space != '\0') ? space : NULL);
    } else {
        terminal_print("Unknown command: ");
        terminal_print(command_name);
        terminal_print("\n");
        terminal_print("Type 'help' for available commands.\n");
    }
}

// Initialize shell
void shell_init(void) {
    input_pos = 0;
    command_count = 0;
    
    // Register system commands
    register_system_commands();
    
    // Register filesystem commands
    register_filesystem_commands();
    
    // Register audio commands
    register_audio_commands();
    
    // Register game commands
    init_game_commands();
    
    // Register execution commands
    register_execution_commands();
    
    // Register window example commands
    register_window_example_commands();
}

// Shell main loop
void shell_loop(void) {
    terminal_print("Welcome to DEA OS Shell!\n");
    terminal_print("Type 'help' for available commands.\n");
    terminal_print("Try 'ls' to see some sample files!\n");
    terminal_print("Move your mouse to see the cursor!\n");
    terminal_print("Try the 'mouse' command to check position!\n");
    terminal_print("Try 'beep', 'play', or 'audiotest' for audio!\n");
    terminal_print("🎮 NEW: Play games with 'guess', 'tictactoe', or 'rps'!\n");
    terminal_print("💻 NEW: Program Execution! Try 'compile hello.elf' then 'exec hello.elf'!\n");
    terminal_print("🪟 NEW: Desktop Environment! Try 'windows' to see window examples!\n\n");
    
    // Play startup sound
    audio_play_event(AUDIO_STARTUP_SOUND);
    
    // Initial window manager update to render cursor
    extern void wm_update(void);
    wm_update();
    
    while (1) {
        // Check for mouse events before showing prompt
        check_mouse_events();
        
        terminal_print("DEA> ");
        
        // Read command
        input_pos = 0;
        while (1) {
            // Check for mouse events during input
            check_mouse_events();
            
            char c = read_key();
            
            if (c == '\n') {
                input_buffer[input_pos] = '\0';
                terminal_putchar('\n');
                break;
            } else if (c == '\b') {
                if (input_pos > 0) {
                    input_pos--;
                    terminal_putchar('\b');
                }
            } else if (c >= 32 && c <= 126 && input_pos < sizeof(input_buffer) - 1) {
                input_buffer[input_pos++] = c;
                terminal_putchar(c);
            }
        }
        
        // Check for mouse events before command execution
        check_mouse_events();
        
        // Execute command
        execute_command(input_buffer);
        
        // Check for mouse events after command execution
        check_mouse_events();
    }
} 