#ifndef LOGGER_RUST_H
#define LOGGER_RUST_H

#include <stdint.h>
#include <stdbool.h>

// Rust FFI interface for logger
// These functions can be called from Rust code

// Log a message from Rust
// level: 0=DEBUG, 1=INFO, 2=WARN, 3=ERROR
// module: module name (null-terminated string)
// message: log message (null-terminated string)
void logger_rust_log(uint32_t level, const char *module, const char *message);

// Log with format (simplified - just supports %d, %u, %x, %p, %s)
// level: 0=DEBUG, 1=INFO, 2=WARN, 3=ERROR
// module: module name
// format: format string
// ...: arguments
void logger_rust_log_fmt(uint32_t level, const char *module, const char *format, ...);

#endif // LOGGER_RUST_H
