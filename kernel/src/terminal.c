#include "terminal.h"
#include "fs/filesystem.h"
#include "string.h"
#include <stddef.h>

// Simple 8x8 bitmap font for basic characters (default embedded font)
static const uint8_t font_8x8[][8] = {
    [' '] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    ['!'] = {0x18, 0x18, 0x18, 0x18, 0x00, 0x18, 0x18, 0x00},
    ['A'] = {0x3C, 0x66, 0x66, 0x7E, 0x66, 0x66, 0x66, 0x00},
    ['B'] = {0x7C, 0x66, 0x66, 0x7C, 0x66, 0x66, 0x7C, 0x00},
    ['C'] = {0x3C, 0x66, 0x60, 0x60, 0x60, 0x66, 0x3C, 0x00},
    ['D'] = {0x7C, 0x66, 0x66, 0x66, 0x66, 0x66, 0x7C, 0x00},
    ['E'] = {0x7E, 0x60, 0x60, 0x7C, 0x60, 0x60, 0x7E, 0x00},
    ['F'] = {0x7E, 0x60, 0x60, 0x7C, 0x60, 0x60, 0x60, 0x00},
    ['G'] = {0x3C, 0x66, 0x60, 0x6E, 0x66, 0x66, 0x3C, 0x00},
    ['H'] = {0x66, 0x66, 0x66, 0x7E, 0x66, 0x66, 0x66, 0x00},
    ['I'] = {0x3C, 0x18, 0x18, 0x18, 0x18, 0x18, 0x3C, 0x00},
    ['J'] = {0x1E, 0x0C, 0x0C, 0x0C, 0x0C, 0x6C, 0x38, 0x00},
    ['K'] = {0x66, 0x6C, 0x78, 0x70, 0x78, 0x6C, 0x66, 0x00},
    ['L'] = {0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x7E, 0x00},
    ['M'] = {0x63, 0x77, 0x7F, 0x6B, 0x63, 0x63, 0x63, 0x00},
    ['N'] = {0x66, 0x76, 0x7E, 0x7E, 0x6E, 0x66, 0x66, 0x00},
    ['O'] = {0x3C, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3C, 0x00},
    ['P'] = {0x7C, 0x66, 0x66, 0x7C, 0x60, 0x60, 0x60, 0x00},
    ['Q'] = {0x3C, 0x66, 0x66, 0x66, 0x66, 0x3C, 0x0E, 0x00},
    ['R'] = {0x7C, 0x66, 0x66, 0x7C, 0x78, 0x6C, 0x66, 0x00},
    ['S'] = {0x3C, 0x66, 0x60, 0x3C, 0x06, 0x66, 0x3C, 0x00},
    ['T'] = {0x7E, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x00},
    ['U'] = {0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3C, 0x00},
    ['V'] = {0x66, 0x66, 0x66, 0x66, 0x66, 0x3C, 0x18, 0x00},
    ['W'] = {0x63, 0x63, 0x63, 0x6B, 0x7F, 0x77, 0x63, 0x00},
    ['X'] = {0x66, 0x66, 0x3C, 0x18, 0x3C, 0x66, 0x66, 0x00},
    ['Y'] = {0x66, 0x66, 0x66, 0x3C, 0x18, 0x18, 0x18, 0x00},
    ['Z'] = {0x7E, 0x06, 0x0C, 0x18, 0x30, 0x60, 0x7E, 0x00},
    ['a'] = {0x00, 0x00, 0x3C, 0x06, 0x3E, 0x66, 0x3E, 0x00},
    ['b'] = {0x60, 0x60, 0x7C, 0x66, 0x66, 0x66, 0x7C, 0x00},
    ['c'] = {0x00, 0x00, 0x3C, 0x60, 0x60, 0x60, 0x3C, 0x00},
    ['d'] = {0x06, 0x06, 0x06, 0x3E, 0x66, 0x66, 0x3E, 0x00},
    ['e'] = {0x00, 0x00, 0x3C, 0x66, 0x7E, 0x60, 0x3C, 0x00},
    ['f'] = {0x1C, 0x36, 0x30, 0x7C, 0x30, 0x30, 0x30, 0x00},
    ['g'] = {0x00, 0x00, 0x3E, 0x66, 0x66, 0x3E, 0x06, 0x7C},
    ['h'] = {0x60, 0x60, 0x7C, 0x66, 0x66, 0x66, 0x66, 0x00},
    ['i'] = {0x18, 0x00, 0x38, 0x18, 0x18, 0x18, 0x3C, 0x00},
    ['j'] = {0x06, 0x00, 0x0E, 0x06, 0x06, 0x06, 0x66, 0x3C},
    ['k'] = {0x60, 0x60, 0x66, 0x6C, 0x78, 0x6C, 0x66, 0x00},
    ['l'] = {0x38, 0x18, 0x18, 0x18, 0x18, 0x18, 0x3C, 0x00},
    ['m'] = {0x00, 0x00, 0x66, 0x7F, 0x7F, 0x6B, 0x63, 0x00},
    ['n'] = {0x00, 0x00, 0x7C, 0x66, 0x66, 0x66, 0x66, 0x00},
    ['o'] = {0x00, 0x00, 0x3C, 0x66, 0x66, 0x66, 0x3C, 0x00},
    ['p'] = {0x00, 0x00, 0x7C, 0x66, 0x66, 0x7C, 0x60, 0x60},
    ['q'] = {0x00, 0x00, 0x3E, 0x66, 0x66, 0x3E, 0x06, 0x06},
    ['r'] = {0x00, 0x00, 0x7C, 0x66, 0x60, 0x60, 0x60, 0x00},
    ['s'] = {0x00, 0x00, 0x3E, 0x60, 0x3C, 0x06, 0x7C, 0x00},
    ['t'] = {0x18, 0x18, 0x7E, 0x18, 0x18, 0x18, 0x0E, 0x00},
    ['u'] = {0x00, 0x00, 0x66, 0x66, 0x66, 0x66, 0x3E, 0x00},
    ['v'] = {0x00, 0x00, 0x66, 0x66, 0x66, 0x3C, 0x18, 0x00},
    ['w'] = {0x00, 0x00, 0x63, 0x6B, 0x7F, 0x3E, 0x36, 0x00},
    ['x'] = {0x00, 0x00, 0x66, 0x3C, 0x18, 0x3C, 0x66, 0x00},
    ['y'] = {0x00, 0x00, 0x66, 0x66, 0x66, 0x3E, 0x0C, 0x78},
    ['z'] = {0x00, 0x00, 0x7E, 0x0C, 0x18, 0x30, 0x7E, 0x00},
    ['0'] = {0x3C, 0x66, 0x6E, 0x76, 0x66, 0x66, 0x3C, 0x00},
    ['1'] = {0x18, 0x18, 0x38, 0x18, 0x18, 0x18, 0x7E, 0x00},
    ['2'] = {0x3C, 0x66, 0x06, 0x0C, 0x30, 0x60, 0x7E, 0x00},
    ['3'] = {0x3C, 0x66, 0x06, 0x1C, 0x06, 0x66, 0x3C, 0x00},
    ['4'] = {0x06, 0x0E, 0x1E, 0x66, 0x7F, 0x06, 0x06, 0x00},
    ['5'] = {0x7E, 0x60, 0x7C, 0x06, 0x06, 0x66, 0x3C, 0x00},
    ['6'] = {0x3C, 0x66, 0x60, 0x7C, 0x66, 0x66, 0x3C, 0x00},
    ['7'] = {0x7E, 0x66, 0x0C, 0x18, 0x18, 0x18, 0x18, 0x00},
    ['8'] = {0x3C, 0x66, 0x66, 0x3C, 0x66, 0x66, 0x3C, 0x00},
    ['9'] = {0x3C, 0x66, 0x66, 0x3E, 0x06, 0x66, 0x3C, 0x00},
    [':'] = {0x00, 0x00, 0x18, 0x00, 0x00, 0x18, 0x00, 0x00},
    ['>'] = {0x00, 0x30, 0x0C, 0x03, 0x0C, 0x30, 0x00, 0x00},
    ['_'] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF},
};

// Font management
#define MAX_FONTS 4
static font_t loaded_fonts[MAX_FONTS];
static font_t *current_font = NULL;
static int font_count = 0;

// Global variables for terminal
static struct limine_framebuffer *g_framebuffer;
static int cursor_x = 0;
static int cursor_y = 0;

// Get character data from current font or fallback to default
static const uint8_t* get_char_data(char c) {
    if (current_font && current_font->loaded && c < 128) {
        return current_font->char_data[(unsigned char)c];
    }
    // Fallback to embedded font
    if (c >= 32 && c <= 126) {
        return font_8x8[(unsigned char)c];
    }
    return font_8x8[' ']; // Default to space
}

// Load font from file
bool load_font_from_file(const char *filename, font_t *font) {
    if (!filename || !font) return false;
    
    uint8_t buffer[1024];
    size_t size;
    
    // Read font file from filesystem
    if (!fs_read_file(filename, buffer, &size)) {
        return false;
    }
    
    // Simple font format: name(32) + width(1) + height(1) + char_data(128*8)
    if (size < 32 + 1 + 1 + 128 * 8) {
        return false;
    }
    
    // Parse font data
    size_t offset = 0;
    
    // Copy name
    for (int i = 0; i < 32 && offset < size; i++) {
        font->name[i] = buffer[offset++];
    }
    font->name[31] = '\0'; // Ensure null termination
    
    // Read width and height
    font->width = buffer[offset++];
    font->height = buffer[offset++];
    
    // Read character data
    for (int i = 0; i < 128 && offset + 8 <= size; i++) {
        for (int j = 0; j < 8; j++) {
            font->char_data[i][j] = buffer[offset++];
        }
    }
    
    font->loaded = true;
    return true;
}

// Set current font by name
bool set_current_font(const char *font_name) {
    if (!font_name) return false;
    
    // Search for font in loaded fonts
    for (int i = 0; i < font_count; i++) {
        if (loaded_fonts[i].loaded && strcmp(loaded_fonts[i].name, font_name) == 0) {
            current_font = &loaded_fonts[i];
            return true;
        }
    }
    
    // Try to load font from file
    if (font_count < MAX_FONTS) {
        char filename[64];
        strcpy(filename, "fonts/");
        strcat(filename, font_name);
        strcat(filename, ".font");
        
        if (load_font_from_file(filename, &loaded_fonts[font_count])) {
            current_font = &loaded_fonts[font_count];
            font_count++;
            return true;
        }
    }
    
    return false;
}

// List available fonts
void list_available_fonts(void) {
    terminal_print("Loaded fonts:\n");
    for (int i = 0; i < font_count; i++) {
        if (loaded_fonts[i].loaded) {
            terminal_print("  ");
            terminal_print(loaded_fonts[i].name);
            if (current_font == &loaded_fonts[i]) {
                terminal_print(" (current)");
            }
            terminal_print("\n");
        }
    }
}

// Create default fonts in filesystem
void create_default_fonts(void) {
    // Create fonts directory structure in filesystem
    // For now, we'll create a simple font file with our default 8x8 font
    
    uint8_t font_data[32 + 1 + 1 + 128 * 8];
    size_t offset = 0;
    
    // Font name
    const char *font_name = "default";
    for (int i = 0; i < 32; i++) {
        font_data[offset++] = (i < (int)strlen(font_name)) ? font_name[i] : 0;
    }
    
    // Width and height
    font_data[offset++] = 8;
    font_data[offset++] = 8;
    
    // Character data
    for (int i = 0; i < 128; i++) {
        for (int j = 0; j < 8; j++) {
            if (i >= 32 && i <= 126) {
                font_data[offset++] = font_8x8[i][j];
            } else {
                font_data[offset++] = 0;
            }
        }
    }
    
    // Write font file
    fs_write_file("fonts/default.font", font_data, offset);
    
    // Create a bold variant
    strcpy((char*)font_data, "bold");
    font_data[30] = 0; // Ensure null termination
    
    // Make bold by duplicating some pixels
    offset = 34; // Skip name, width, height
    for (int i = 0; i < 128; i++) {
        for (int j = 0; j < 8; j++) {
            if (i >= 32 && i <= 126) {
                uint8_t original = font_8x8[i][j];
                // Simple bold effect: OR with right-shifted version
                font_data[offset++] = original | (original >> 1);
            } else {
                font_data[offset++] = 0;
            }
        }
    }
    
    fs_write_file("fonts/bold.font", font_data, 32 + 1 + 1 + 128 * 8);
}

// Initialize terminal
void terminal_init(struct limine_framebuffer *framebuffer) {
    g_framebuffer = framebuffer;
    cursor_x = 0;
    cursor_y = 0;
    
    // Initialize font system
    font_count = 0;
    current_font = NULL;
    
    // Create default fonts
    create_default_fonts();
    
    // Load default font
    set_current_font("default");
}

// Function to draw a character at a specific position
void draw_char(struct limine_framebuffer *framebuffer, char c, int x, int y, uint32_t color) {
    if (c < 32 || c > 126) return; // Only printable ASCII
    
    const uint8_t *glyph = get_char_data(c);
    volatile uint32_t *fb_ptr = framebuffer->address;
    
    uint8_t char_width = current_font ? current_font->width : 8;
    uint8_t char_height = current_font ? current_font->height : 8;
    
    for (int row = 0; row < char_height; row++) {
        for (int col = 0; col < char_width; col++) {
            if (glyph[row] & (1 << (7 - col))) {
                int pixel_x = x + col;
                int pixel_y = y + row;
                
                if (pixel_x < (int)framebuffer->width && pixel_y < (int)framebuffer->height) {
                    fb_ptr[pixel_y * (framebuffer->pitch / 4) + pixel_x] = color;
                }
            }
        }
    }
}

// Function to draw a string
void draw_string(struct limine_framebuffer *framebuffer, const char *str, int x, int y, uint32_t color) {
    int current_x = x;
    uint8_t char_width = current_font ? current_font->width : 8;
    
    while (*str) {
        draw_char(framebuffer, *str, current_x, y, color);
        current_x += char_width; // Move to next character position
        str++;
    }
}

// Clear screen
void clear_screen(void) {
    volatile uint32_t *fb_ptr = g_framebuffer->address;
    for (size_t i = 0; i < g_framebuffer->width * g_framebuffer->height; i++) {
        fb_ptr[i] = BG_COLOR;
    }
    cursor_x = 0;
    cursor_y = 0;
}

// Print character to terminal
void terminal_putchar(char c) {
    uint8_t char_width = current_font ? current_font->width : CHAR_WIDTH;
    uint8_t char_height = current_font ? current_font->height : CHAR_HEIGHT;
    
    if (c == '\n') {
        cursor_x = 0;
        cursor_y += char_height;
        if (cursor_y >= (int)g_framebuffer->height - char_height) {
            cursor_y = (int)g_framebuffer->height - char_height;
            // Simple scroll: clear screen and start from top
            clear_screen();
        }
    } else if (c == '\b') {
        if (cursor_x > 0) {
            cursor_x -= char_width;
            // Clear the character
            for (int y = cursor_y; y < cursor_y + char_height; y++) {
                for (int x = cursor_x; x < cursor_x + char_width; x++) {
                    if (x < (int)g_framebuffer->width && y < (int)g_framebuffer->height) {
                        volatile uint32_t *fb_ptr = g_framebuffer->address;
                        fb_ptr[y * (g_framebuffer->pitch / 4) + x] = BG_COLOR;
                    }
                }
            }
        }
    } else {
        draw_char(g_framebuffer, c, cursor_x, cursor_y, TEXT_COLOR);
        cursor_x += char_width;
        if (cursor_x >= (int)g_framebuffer->width - char_width) {
            cursor_x = 0;
            cursor_y += char_height;
            if (cursor_y >= (int)g_framebuffer->height - char_height) {
                cursor_y = (int)g_framebuffer->height - char_height;
                clear_screen();
            }
        }
    }
}

// Print string to terminal
void terminal_print(const char *str) {
    while (*str) {
        terminal_putchar(*str);
        str++;
    }
} 