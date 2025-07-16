#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <limine.h>
#include "terminal.h"
#include "keyboard.h"
#include "mouse.h"
#include "shell.h"
// #include "fs/filesystem.h"
#include "audio.h"

// Set the base revision to 3, this is recommended as this is the latest
// base revision described by the Limine boot protocol specification.
// See specification for further info.

__attribute__((used, section(".limine_requests")))
static volatile LIMINE_BASE_REVISION(3);

// The Limine requests can be placed anywhere, but it is important that
// the compiler does not optimise them away, so, usually, they should
// be made volatile or equivalent, _and_ they should be accessed at least
// once or marked as used with the "used" attribute as done here.

__attribute__((used, section(".limine_requests")))
static volatile struct limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST,
    .revision = 0
};

// Finally, define the start and end markers for the Limine requests.
// These can also be moved anywhere, to any .c file, as seen fit.

__attribute__((used, section(".limine_requests_start")))
static volatile LIMINE_REQUESTS_START_MARKER;

__attribute__((used, section(".limine_requests_end")))
static volatile LIMINE_REQUESTS_END_MARKER;

// GCC and Clang reserve the right to generate calls to the following
// 4 functions even if they are not directly called.
// Implement them as the C specification mandates.
// DO NOT remove or rename these functions, or stuff will eventually break!
// They CAN be moved to a different .c file.

void *memcpy(void *restrict dest, const void *restrict src, size_t n) {
    uint8_t *restrict pdest = (uint8_t *restrict)dest;
    const uint8_t *restrict psrc = (const uint8_t *restrict)src;

    for (size_t i = 0; i < n; i++) {
        pdest[i] = psrc[i];
    }

    return dest;
}

void *memset(void *s, int c, size_t n) {
    uint8_t *p = (uint8_t *)s;

    for (size_t i = 0; i < n; i++) {
        p[i] = (uint8_t)c;
    }

    return s;
}

void *memmove(void *dest, const void *src, size_t n) {
    uint8_t *pdest = (uint8_t *)dest;
    const uint8_t *psrc = (const uint8_t *)src;

    if (src > dest) {
        for (size_t i = 0; i < n; i++) {
            pdest[i] = psrc[i];
        }
    } else if (src < dest) {
        for (size_t i = n; i > 0; i--) {
            pdest[i-1] = psrc[i-1];
        }
    }

    return dest;
}

int memcmp(const void *s1, const void *s2, size_t n) {
    const uint8_t *p1 = (const uint8_t *)s1;
    const uint8_t *p2 = (const uint8_t *)s2;

    for (size_t i = 0; i < n; i++) {
        if (p1[i] != p2[i]) {
            return p1[i] < p2[i] ? -1 : 1;
        }
    }

    return 0;
}

// Halt and catch fire function.
static void hcf(void) {
    for (;;) {
        asm ("hlt");
    }
}

// Simple delay for early boot
static void early_delay(uint32_t milliseconds) {
    volatile uint32_t count = milliseconds * 100000;
    while (count--) {
        __asm__ volatile ("nop");
    }
}



// VGA text mode fallback for error messages
static void vga_write_string(const char* str) {
    volatile uint16_t* vga_buffer = (volatile uint16_t*)0xB8000;
    static int vga_pos = 0;
    
    while (*str) {
        if (*str == '\n') {
            vga_pos = (vga_pos / 80 + 1) * 80; // Move to next line
        } else {
            vga_buffer[vga_pos] = (uint16_t)*str | 0x0700; // White text on black background
            vga_pos++;
        }
        str++;
        
        // Wrap around if we reach the end of screen
        if (vga_pos >= 80 * 25) {
            vga_pos = 0;
        }
    }
}

// Show error message when video is not supported
static void show_video_error(void) {
    // Clear VGA text buffer
    volatile uint16_t* vga_buffer = (volatile uint16_t*)0xB8000;
    for (int i = 0; i < 80 * 25; i++) {
        vga_buffer[i] = 0x0700; // Space with white text on black background
    }
    
    // Show error message
    vga_write_string("DEA OS - Boot Error\n");
    vga_write_string("==================\n\n");
    vga_write_string("ERROR: Video/Framebuffer not supported!\n\n");
    vga_write_string("This could be due to:\n");
    vga_write_string("- Unsupported graphics hardware\n");
    vga_write_string("- Missing UEFI GOP support\n");
    vga_write_string("- Incorrect bootloader configuration\n");
    vga_write_string("- VM/Emulator compatibility issues\n\n");
    vga_write_string("Possible solutions:\n");
    vga_write_string("- Try different graphics settings in your VM\n");
    vga_write_string("- Enable UEFI GOP in VM settings\n");
    vga_write_string("- Check if your hardware supports framebuffer\n");
    vga_write_string("- Update your bootloader configuration\n\n");
    vga_write_string("System halted. Press Ctrl+Alt+Del to restart.\n");
    
    // Play error sound sequence
    audio_init();
    audio_play_event(AUDIO_ERROR_BEEP);
}

// The following will be our kernel's entry point.
// If renaming kmain() to something else, make sure to change the
// linker script accordingly.
void kmain(void) {
    // Ensure the bootloader actually understands our base revision (see spec).
    if (LIMINE_BASE_REVISION_SUPPORTED == false) {
        // Early error - show message and halt
        vga_write_string("DEA OS - Boot Error: Unsupported bootloader revision\n");
        hcf();
    }

    // Ensure we got a framebuffer.
    if (framebuffer_request.response == NULL
     || framebuffer_request.response->framebuffer_count < 1) {
        // Show detailed error message for video not supported
        show_video_error();
        hcf();
    }

    // Fetch the first framebuffer.
    struct limine_framebuffer *framebuffer = framebuffer_request.response->framebuffers[0];

    // Initialize subsystems in order
    terminal_init(framebuffer);
    keyboard_init();
    
    // Initialize mouse and set screen bounds
    mouse_init();
    mouse_set_bounds(framebuffer->width, framebuffer->height);
    
    // Initialize audio system
    audio_init();
    
    // Success beep to indicate video is working
    audio_play_event(AUDIO_STARTUP_SOUND);
    
    shell_init();
    
    // Clear the screen
    clear_screen();
    
    // Show boot success message
    terminal_print("DEA OS - Boot Successful!\n");
    terminal_print("Video: ");
    terminal_print("Framebuffer detected: ");
    // Print framebuffer info
    char width_str[16], height_str[16];
    // Simple integer to string conversion
    int width = framebuffer->width;
    int height = framebuffer->height;
    int i = 0;
    if (width == 0) {
        width_str[i++] = '0';
    } else {
        char temp[16];
        int j = 0;
        while (width > 0) {
            temp[j++] = '0' + (width % 10);
            width /= 10;
        }
        while (j > 0) {
            width_str[i++] = temp[--j];
        }
    }
    width_str[i] = '\0';
    
    i = 0;
    if (height == 0) {
        height_str[i++] = '0';
    } else {
        char temp[16];
        int j = 0;
        while (height > 0) {
            temp[j++] = '0' + (height % 10);
            height /= 10;
        }
        while (j > 0) {
            height_str[i++] = temp[--j];
        }
    }
    height_str[i] = '\0';
    
    terminal_print(width_str);
    terminal_print("x");
    terminal_print(height_str);
    terminal_print("\n");
    terminal_print("PS2 Controller: Initialized successfully\n");
    terminal_print("============================\n\n");
    
    // Initialize filesystem after other subsystems are ready
    // fs_init(); // Commented out to avoid boot issues

    // Start the shell
    shell_loop();
}
