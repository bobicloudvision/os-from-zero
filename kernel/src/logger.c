#include "logger.h"
#include "terminal.h"
#include "string.h"
#include <stdarg.h>

static log_level_t current_log_level = LOG_INFO;  // Default to INFO level

void logger_init(void) {
    current_log_level = LOG_INFO;
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
    
    // Print log level and module
    terminal_print("[");
    terminal_print(get_level_string(level));
    terminal_print("] [");
    if (module) {
        terminal_print(module);
    } else {
        terminal_print("UNKNOWN");
    }
    terminal_print("] ");
    
    // Parse format string and arguments
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
                    } else {
                        terminal_print("(null)");
                    }
                    break;
                }
                case 'd': {
                    // Decimal integer
                    int32_t val = va_arg(args, int32_t);
                    char num_buf[32];
                    if (val < 0) {
                        terminal_print("-");
                        val = -val;
                    }
                    uint_to_string((uint64_t)val, num_buf, sizeof(num_buf));
                    terminal_print(num_buf);
                    break;
                }
                case 'u': {
                    // Unsigned decimal
                    uint32_t val = va_arg(args, uint32_t);
                    char num_buf[32];
                    uint_to_string(val, num_buf, sizeof(num_buf));
                    terminal_print(num_buf);
                    break;
                }
                case 'x': {
                    // Hexadecimal (lowercase)
                    uint32_t val = va_arg(args, uint32_t);
                    char hex_buf[32];
                    uint_to_hex_string(val, hex_buf, sizeof(hex_buf));
                    terminal_print(hex_buf);
                    break;
                }
                case 'X': {
                    // Hexadecimal (uppercase) - same as lowercase for now
                    uint32_t val = va_arg(args, uint32_t);
                    char hex_buf[32];
                    uint_to_hex_string(val, hex_buf, sizeof(hex_buf));
                    terminal_print(hex_buf);
                    break;
                }
                case 'p': {
                    // Pointer
                    void *ptr = va_arg(args, void *);
                    char hex_buf[32];
                    uint_to_hex_string((uint64_t)ptr, hex_buf, sizeof(hex_buf));
                    terminal_print(hex_buf);
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
                            val = -val;
                        }
                        uint_to_string((uint64_t)val, num_buf, sizeof(num_buf));
                        terminal_print(num_buf);
                    } else if (*p == 'u') {
                        uint64_t val = va_arg(args, uint64_t);
                        char num_buf[32];
                        uint_to_string(val, num_buf, sizeof(num_buf));
                        terminal_print(num_buf);
                    } else if (*p == 'x' || *p == 'X') {
                        uint64_t val = va_arg(args, uint64_t);
                        char hex_buf[32];
                        uint_to_hex_string(val, hex_buf, sizeof(hex_buf));
                        terminal_print(hex_buf);
                    } else {
                        // Unknown, just print the characters
                        terminal_print("%l");
                        p--;  // Back up to re-process
                    }
                    break;
                }
                case '%': {
                    // Literal %
                    terminal_print("%");
                    break;
                }
                default: {
                    // Unknown format, print as-is
                    terminal_print("%");
                    if (*p) {
                        char c = *p;
                        terminal_putchar(c);
                    }
                    break;
                }
            }
            p++;
        } else {
            terminal_putchar(*p);
            p++;
        }
    }
    
    va_end(args);
    terminal_print("\n");
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
