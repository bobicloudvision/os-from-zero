#ifndef LOGGER_H
#define LOGGER_H

#include <stdint.h>
#include <stdbool.h>

// Log levels
typedef enum {
    LOG_DEBUG = 0,
    LOG_INFO = 1,
    LOG_WARN = 2,
    LOG_ERROR = 3,
    LOG_NONE = 4  // Disable all logging
} log_level_t;

// Initialize logger
void logger_init(void);

// Set log level (only messages at or above this level will be shown)
void logger_set_level(log_level_t level);

// Get current log level
log_level_t logger_get_level(void);

// Core logging functions
void logger_log(log_level_t level, const char *module, const char *format, ...);

// Convenience macros
#define LOG_DEBUG(module, ...) logger_log(LOG_DEBUG, module, __VA_ARGS__)
#define LOG_INFO(module, ...) logger_log(LOG_INFO, module, __VA_ARGS__)
#define LOG_WARN(module, ...) logger_log(LOG_WARN, module, __VA_ARGS__)
#define LOG_ERROR(module, ...) logger_log(LOG_ERROR, module, __VA_ARGS__)

// Helper functions for formatting
void logger_print_hex(uint64_t value);
void logger_print_dec(uint64_t value);
void logger_print_ptr(void *ptr);

#endif // LOGGER_H
