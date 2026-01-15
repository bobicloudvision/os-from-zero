#include "logger_rust.h"
#include "logger.h"
#include <stdarg.h>

void logger_rust_log(uint32_t level, const char *module, const char *message) {
    log_level_t log_level;
    switch (level) {
        case 0: log_level = LOG_DEBUG; break;
        case 1: log_level = LOG_INFO; break;
        case 2: log_level = LOG_WARN; break;
        case 3: log_level = LOG_ERROR; break;
        default: log_level = LOG_INFO; break;
    }
    
    logger_log(log_level, module, "%s", message);
}

void logger_rust_log_fmt(uint32_t level, const char *module, const char *format, ...) {
    log_level_t log_level;
    switch (level) {
        case 0: log_level = LOG_DEBUG; break;
        case 1: log_level = LOG_INFO; break;
        case 2: log_level = LOG_WARN; break;
        case 3: log_level = LOG_ERROR; break;
        default: log_level = LOG_INFO; break;
    }
    
    va_list args;
    va_start(args, format);
    
    // For now, we'll use a simple approach - just pass to logger_log
    // which handles va_list internally
    // We need to create a wrapper that formats the message first
    // For simplicity, let's just call logger_log with a single format string
    // and let it handle the formatting
    
    // Actually, logger_log already takes a format string and va_list-like arguments
    // But it uses its own va_list. We need to create a formatted string first.
    // For now, let's use a simple approach with a buffer
    
    char buffer[512];
    const char *p = format;
    char *buf_ptr = buffer;
    int buf_remaining = sizeof(buffer) - 1;
    
    while (*p && buf_remaining > 0) {
        if (*p == '%') {
            p++;
            switch (*p) {
                case 'd': {
                    int32_t val = va_arg(args, int32_t);
                    char num_buf[32];
                    int i = 0;
                    if (val < 0) {
                        if (buf_remaining > 0) {
                            *buf_ptr++ = '-';
                            buf_remaining--;
                        }
                        val = -val;
                    }
                    uint64_t uval = val;
                    if (uval == 0) {
                        if (buf_remaining > 0) {
                            *buf_ptr++ = '0';
                            buf_remaining--;
                        }
                    } else {
                        char temp[32];
                        int j = 0;
                        while (uval > 0 && j < 31) {
                            temp[j++] = '0' + (uval % 10);
                            uval /= 10;
                        }
                        while (j > 0 && buf_remaining > 0) {
                            *buf_ptr++ = temp[--j];
                            buf_remaining--;
                        }
                    }
                    break;
                }
                case 'u': {
                    uint32_t val = va_arg(args, uint32_t);
                    char num_buf[32];
                    int i = 0;
                    uint64_t uval = val;
                    if (uval == 0) {
                        if (buf_remaining > 0) {
                            *buf_ptr++ = '0';
                            buf_remaining--;
                        }
                    } else {
                        char temp[32];
                        int j = 0;
                        while (uval > 0 && j < 31) {
                            temp[j++] = '0' + (uval % 10);
                            uval /= 10;
                        }
                        while (j > 0 && buf_remaining > 0) {
                            *buf_ptr++ = temp[--j];
                            buf_remaining--;
                        }
                    }
                    break;
                }
                case 'x':
                case 'X': {
                    uint32_t val = va_arg(args, uint32_t);
                    const char hex_chars[] = "0123456789ABCDEF";
                    if (buf_remaining >= 2) {
                        *buf_ptr++ = '0';
                        *buf_ptr++ = 'x';
                        buf_remaining -= 2;
                    }
                    if (val == 0 && buf_remaining > 0) {
                        *buf_ptr++ = '0';
                        buf_remaining--;
                    } else {
                        char temp[32];
                        int j = 0;
                        uint64_t uval = val;
                        while (uval > 0 && j < 31) {
                            temp[j++] = hex_chars[uval & 0xF];
                            uval >>= 4;
                        }
                        while (j > 0 && buf_remaining > 0) {
                            *buf_ptr++ = temp[--j];
                            buf_remaining--;
                        }
                    }
                    break;
                }
                case 'p': {
                    void *ptr = va_arg(args, void *);
                    const char hex_chars[] = "0123456789ABCDEF";
                    uint64_t addr = (uint64_t)ptr;
                    if (buf_remaining >= 2) {
                        *buf_ptr++ = '0';
                        *buf_ptr++ = 'x';
                        buf_remaining -= 2;
                    }
                    char temp[32];
                    int j = 0;
                    while (addr > 0 && j < 31) {
                        temp[j++] = hex_chars[addr & 0xF];
                        addr >>= 4;
                    }
                    if (j == 0 && buf_remaining > 0) {
                        *buf_ptr++ = '0';
                        buf_remaining--;
                    } else {
                        while (j > 0 && buf_remaining > 0) {
                            *buf_ptr++ = temp[--j];
                            buf_remaining--;
                        }
                    }
                    break;
                }
                case 's': {
                    const char *str = va_arg(args, const char *);
                    if (!str) str = "(null)";
                    while (*str && buf_remaining > 0) {
                        *buf_ptr++ = *str++;
                        buf_remaining--;
                    }
                    break;
                }
                case '%': {
                    if (buf_remaining > 0) {
                        *buf_ptr++ = '%';
                        buf_remaining--;
                    }
                    break;
                }
                default: {
                    if (buf_remaining > 0) {
                        *buf_ptr++ = '%';
                        buf_remaining--;
                    }
                    if (*p && buf_remaining > 0) {
                        *buf_ptr++ = *p;
                        buf_remaining--;
                    }
                    break;
                }
            }
            if (*p) p++;
        } else {
            *buf_ptr++ = *p++;
            buf_remaining--;
        }
    }
    *buf_ptr = '\0';
    
    va_end(args);
    
    logger_log(log_level, module, "%s", buffer);
}
