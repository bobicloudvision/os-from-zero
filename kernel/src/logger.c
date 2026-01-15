#include "logger.h"
#include "terminal.h"
#include "string.h"
#include "fs/filesystem.h"
#include <stdarg.h>

// Log file path (using simple filename since filesystem doesn't support paths)
#define LOG_FILE_PATH "system.log"

// Serial port for logging (COM1 - 0x3F8)
#define SERIAL_PORT_BASE 0x3F8
#define SERIAL_DATA_PORT (SERIAL_PORT_BASE)
#define SERIAL_INTERRUPT_ENABLE (SERIAL_PORT_BASE + 1)
#define SERIAL_FIFO_CONTROL (SERIAL_PORT_BASE + 2)
#define SERIAL_LINE_CONTROL (SERIAL_PORT_BASE + 3)
#define SERIAL_MODEM_CONTROL (SERIAL_PORT_BASE + 4)
#define SERIAL_LINE_STATUS (SERIAL_PORT_BASE + 5)

// Port I/O functions
static inline void outb(uint16_t port, uint8_t value) {
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t value;
    __asm__ volatile ("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

// Initialize serial port for logging
static void serial_init(void) {
    // Disable interrupts
    outb(SERIAL_INTERRUPT_ENABLE, 0x00);
    
    // Set baud rate divisor (115200 baud)
    outb(SERIAL_LINE_CONTROL, 0x80); // Enable DLAB
    outb(SERIAL_DATA_PORT, 0x01);    // Low byte (115200 = 1)
    outb(SERIAL_DATA_PORT + 1, 0x00); // High byte
    outb(SERIAL_LINE_CONTROL, 0x03);  // 8 bits, no parity, 1 stop bit
    
    // Enable FIFO, clear buffers
    outb(SERIAL_FIFO_CONTROL, 0xC7);
    
    // Enable DTR, RTS, and OUT2
    outb(SERIAL_MODEM_CONTROL, 0x0B);
}

// Write character to serial port
static void serial_putchar(char c) {
    // Wait for transmit buffer to be empty
    while ((inb(SERIAL_LINE_STATUS) & 0x20) == 0) {
        // Busy wait
    }
    outb(SERIAL_DATA_PORT, c);
}

// Write string to serial port
static void serial_print(const char *str) {
    while (*str) {
        if (*str == '\n') {
            serial_putchar('\r'); // Add carriage return for newline
            serial_putchar('\n');
        } else {
            serial_putchar(*str);
        }
        str++;
    }
}

static log_level_t current_log_level = LOG_INFO;  // Default to INFO level
static bool log_file_initialized = false;

void logger_init(void) {
    current_log_level = LOG_INFO;
    
    // Initialize serial port for file logging
    serial_init();
    
    // Create the log file in filesystem (in-memory)
    // Note: The filesystem uses simple names, so we'll use "system.log" directly
    if (!fs_file_exists("system.log")) {
        // Create the log file
        fs_create_file("system.log", FILE_TYPE_REGULAR);
        log_file_initialized = true;
    } else {
        log_file_initialized = true;
    }
}

void logger_set_level(log_level_t level) {
    current_log_level = level;
}

log_level_t logger_get_level(void) {
    return current_log_level;
}

// Convert number to string (decimal)
static void uint_to_string(uint64_t num, char *buffer, int buffer_size) {
    if (num == 0) {
        if (buffer_size > 0) {
            buffer[0] = '0';
            buffer[1] = '\0';
        }
        return;
    }
    
    char temp[32];
    int i = 0;
    uint64_t n = num;
    
    while (n > 0 && i < 31) {
        temp[i++] = '0' + (n % 10);
        n /= 10;
    }
    
    int j = 0;
    for (int k = i - 1; k >= 0 && j < buffer_size - 1; k--) {
        buffer[j++] = temp[k];
    }
    buffer[j] = '\0';
}

// Convert number to hex string
static void uint_to_hex_string(uint64_t num, char *buffer, int buffer_size) {
    const char hex_chars[] = "0123456789ABCDEF";
    
    if (num == 0) {
        if (buffer_size > 0) {
            buffer[0] = '0';
            buffer[1] = '\0';
        }
        return;
    }
    
    char temp[32];
    int i = 0;
    uint64_t n = num;
    
    while (n > 0 && i < 31) {
        temp[i++] = hex_chars[n & 0xF];
        n >>= 4;
    }
    
    int j = 0;
    if (buffer_size > 2) {
        buffer[j++] = '0';
        buffer[j++] = 'x';
    }
    for (int k = i - 1; k >= 0 && j < buffer_size - 1; k--) {
        buffer[j++] = temp[k];
    }
    buffer[j] = '\0';
}

// Get log level string
static const char *get_level_string(log_level_t level) {
    switch (level) {
        case LOG_DEBUG: return "DEBUG";
        case LOG_INFO:  return "INFO ";
        case LOG_WARN:  return "WARN ";
        case LOG_ERROR: return "ERROR";
        default: return "UNKN ";
    }
}

void logger_log(log_level_t level, const char *module, const char *format, ...) {
    // Check if we should log this message
    if (level < current_log_level) {
        return;
    }
    
    // Build header
    const char *level_str = get_level_string(level);
    const char *module_str = module ? module : "UNKNOWN";
    
    // Output to terminal
    terminal_print("[");
    terminal_print(level_str);
    terminal_print("] [");
    terminal_print(module_str);
    terminal_print("] ");
    
    // Output to serial port (for file logging)
    serial_print("[");
    serial_print(level_str);
    serial_print("] [");
    serial_print(module_str);
    serial_print("] ");
    
    // Parse format string and arguments - output to both terminal and serial
    va_list args;
    va_start(args, format);
    
    const char *p = format;
    while (*p) {
        if (*p == '%') {
            p++;
            switch (*p) {
                case 's': {
                    // String
                    const char *str = va_arg(args, const char *);
                    if (str) {
                        terminal_print(str);
                        serial_print(str);
                    } else {
                        terminal_print("(null)");
                        serial_print("(null)");
                    }
                    break;
                }
                case 'd': {
                    // Decimal integer
                    int32_t val = va_arg(args, int32_t);
                    char num_buf[32];
                    if (val < 0) {
                        terminal_print("-");
                        serial_print("-");
                        val = -val;
                    }
                    uint_to_string((uint64_t)val, num_buf, sizeof(num_buf));
                    terminal_print(num_buf);
                    serial_print(num_buf);
                    break;
                }
                case 'u': {
                    // Unsigned decimal
                    uint32_t val = va_arg(args, uint32_t);
                    char num_buf[32];
                    uint_to_string(val, num_buf, sizeof(num_buf));
                    terminal_print(num_buf);
                    serial_print(num_buf);
                    break;
                }
                case 'x': {
                    // Hexadecimal (lowercase)
                    uint32_t val = va_arg(args, uint32_t);
                    char hex_buf[32];
                    uint_to_hex_string(val, hex_buf, sizeof(hex_buf));
                    terminal_print(hex_buf);
                    serial_print(hex_buf);
                    break;
                }
                case 'X': {
                    // Hexadecimal (uppercase) - same as lowercase for now
                    uint32_t val = va_arg(args, uint32_t);
                    char hex_buf[32];
                    uint_to_hex_string(val, hex_buf, sizeof(hex_buf));
                    terminal_print(hex_buf);
                    serial_print(hex_buf);
                    break;
                }
                case 'p': {
                    // Pointer
                    void *ptr = va_arg(args, void *);
                    char hex_buf[32];
                    uint_to_hex_string((uint64_t)ptr, hex_buf, sizeof(hex_buf));
                    terminal_print(hex_buf);
                    serial_print(hex_buf);
                    break;
                }
                case 'l': {
                    // Check for 'ld' or 'lu' or 'lx'
                    p++;
                    if (*p == 'd') {
                        int64_t val = va_arg(args, int64_t);
                        char num_buf[32];
                        if (val < 0) {
                            terminal_print("-");
                            serial_print("-");
                            val = -val;
                        }
                        uint_to_string((uint64_t)val, num_buf, sizeof(num_buf));
                        terminal_print(num_buf);
                        serial_print(num_buf);
                    } else if (*p == 'u') {
                        uint64_t val = va_arg(args, uint64_t);
                        char num_buf[32];
                        uint_to_string(val, num_buf, sizeof(num_buf));
                        terminal_print(num_buf);
                        serial_print(num_buf);
                    } else if (*p == 'x' || *p == 'X') {
                        uint64_t val = va_arg(args, uint64_t);
                        char hex_buf[32];
                        uint_to_hex_string(val, hex_buf, sizeof(hex_buf));
                        terminal_print(hex_buf);
                        serial_print(hex_buf);
                    } else {
                        // Unknown, just print the characters
                        terminal_print("%l");
                        serial_print("%l");
                        p--;  // Back up to re-process
                    }
                    break;
                }
                case '%': {
                    // Literal %
                    terminal_print("%");
                    serial_print("%");
                    break;
                }
                default: {
                    // Unknown format, print as-is
                    terminal_print("%");
                    serial_print("%");
                    if (*p) {
                        char c = *p;
                        terminal_putchar(c);
                        serial_putchar(c);
                    }
                    break;
                }
            }
            p++;
        } else {
            terminal_putchar(*p);
            serial_putchar(*p);
            p++;
        }
    }
    
    va_end(args);
    terminal_print("\n");
    serial_print("\n");
    
    // Also write to log file (append mode) - in-memory filesystem
    // Note: This is redundant since serial port already captures everything
    // But keeping it for in-memory access via 'cat system.log'
    if (log_file_initialized) {
        // Build the log message by re-parsing (need to restart va_list)
        va_list args2;
        va_start(args2, format);
        
        char log_buffer[512];
        int pos = 0;
        
        // Format: [LEVEL] [MODULE] message
        const char *level_str = get_level_string(level);
        const char *module_str = module ? module : "UNKNOWN";
        
        // Write header
        log_buffer[pos++] = '[';
        for (int i = 0; level_str[i] && pos < (int)sizeof(log_buffer) - 1; i++) {
            log_buffer[pos++] = level_str[i];
        }
        log_buffer[pos++] = ']';
        log_buffer[pos++] = ' ';
        log_buffer[pos++] = '[';
        for (int i = 0; module_str[i] && pos < (int)sizeof(log_buffer) - 1; i++) {
            log_buffer[pos++] = module_str[i];
        }
        log_buffer[pos++] = ']';
        log_buffer[pos++] = ' ';
        
        // Re-parse format string to build the message
        p = format;
        p = format;
        while (*p && pos < (int)sizeof(log_buffer) - 1) {
            if (*p == '%') {
                p++;
                switch (*p) {
                    case 's': {
                        const char *str = va_arg(args2, const char *);
                        if (str) {
                            for (int i = 0; str[i] && pos < (int)sizeof(log_buffer) - 1; i++) {
                                log_buffer[pos++] = str[i];
                            }
                        }
                        break;
                    }
                    case 'd': {
                        int32_t val = va_arg(args2, int32_t);
                        char num_buf[32];
                        if (val < 0) {
                            if (pos < (int)sizeof(log_buffer) - 1) log_buffer[pos++] = '-';
                            val = -val;
                        }
                        uint_to_string((uint64_t)val, num_buf, sizeof(num_buf));
                        for (int i = 0; num_buf[i] && pos < (int)sizeof(log_buffer) - 1; i++) {
                            log_buffer[pos++] = num_buf[i];
                        }
                        break;
                    }
                    case 'u': {
                        uint32_t val = va_arg(args2, uint32_t);
                        char num_buf[32];
                        uint_to_string(val, num_buf, sizeof(num_buf));
                        for (int i = 0; num_buf[i] && pos < (int)sizeof(log_buffer) - 1; i++) {
                            log_buffer[pos++] = num_buf[i];
                        }
                        break;
                    }
                    case 'x':
                    case 'X':
                    case 'p': {
                        uint32_t val = (uint32_t)va_arg(args2, uintptr_t);
                        char hex_buf[32];
                        uint_to_hex_string(val, hex_buf, sizeof(hex_buf));
                        for (int i = 0; hex_buf[i] && pos < (int)sizeof(log_buffer) - 1; i++) {
                            log_buffer[pos++] = hex_buf[i];
                        }
                        break;
                    }
                    default:
                        if (pos < (int)sizeof(log_buffer) - 1) log_buffer[pos++] = '%';
                        if (*p && pos < (int)sizeof(log_buffer) - 1) log_buffer[pos++] = *p;
                        break;
                }
                if (*p) p++;
            } else {
                if (pos < (int)sizeof(log_buffer) - 1) {
                    log_buffer[pos++] = *p;
                }
                p++;
            }
        }
        va_end(args2);
        
        if (pos < (int)sizeof(log_buffer) - 1) {
            log_buffer[pos++] = '\n';
        }
        log_buffer[pos] = '\0';
        
        // Append to log file (read existing, append, write back)
        uint8_t existing_buffer[1024];
        size_t existing_size = 0;
        
        if (fs_read_file(LOG_FILE_PATH, existing_buffer, &existing_size)) {
            // Append new content
            size_t total_size = existing_size + pos;
            if (total_size < 1024) {
                uint8_t combined_buffer[1024];
                for (size_t i = 0; i < existing_size; i++) {
                    combined_buffer[i] = existing_buffer[i];
                }
                for (int i = 0; i < pos; i++) {
                    combined_buffer[existing_size + i] = log_buffer[i];
                }
                fs_write_file(LOG_FILE_PATH, combined_buffer, total_size);
            }
        } else {
            // File doesn't exist or is empty, just write the new content
            fs_write_file(LOG_FILE_PATH, (const uint8_t*)log_buffer, pos);
        }
    }
}

// Helper functions
void logger_print_hex(uint64_t value) {
    char hex_buf[32];
    uint_to_hex_string(value, hex_buf, sizeof(hex_buf));
    terminal_print(hex_buf);
}

void logger_print_dec(uint64_t value) {
    char num_buf[32];
    uint_to_string(value, num_buf, sizeof(num_buf));
    terminal_print(num_buf);
}

void logger_print_ptr(void *ptr) {
    char hex_buf[32];
    uint_to_hex_string((uint64_t)ptr, hex_buf, sizeof(hex_buf));
    terminal_print(hex_buf);
}
