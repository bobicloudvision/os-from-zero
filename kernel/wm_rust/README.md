# Rust Window Manager

This is a window manager implementation in Rust for the DEA OS kernel.

## Building

The Rust window manager is automatically built when you compile the kernel using `make`.

## Requirements

- Rust toolchain (rustc, cargo)
- `x86_64-unknown-none` target: `rustup target add x86_64-unknown-none`

## Structure

- `src/lib.rs` - Main Rust implementation
- `Cargo.toml` - Rust project configuration
- `.cargo/config.toml` - Rust compiler configuration for bare metal

## FFI Interface

The Rust code exposes C-compatible functions through `#[no_mangle] extern "C"` functions that can be called from C code. The C wrapper in `src/window_manager_rust.c` provides a compatibility layer.

## Features

- Window creation and management
- Mouse event handling
- Window dragging
- Focus management
- Desktop drawing
