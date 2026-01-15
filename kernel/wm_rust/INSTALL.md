# Rust Window Manager - Installation

The window manager requires Rust to be installed.

## Installing Rust

1. Install Rust using rustup:
```bash
curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh
```

2. **IMPORTANT**: Source the Rust environment in your current shell:
```bash
. "$HOME/.cargo/env"
```

3. Add the bare metal target (requires network access):
```bash
rustup target add x86_64-unknown-none
```

4. Verify installation:
```bash
rustc --version
cargo --version
rustup target list | grep x86_64-unknown-none
```

**Note**: If you get "can't find crate for `core`" errors, the target wasn't fully installed. Make sure you:
- Have network access
- Source the cargo env: `. "$HOME/.cargo/env"`
- Run `rustup target add x86_64-unknown-none` again

## Building

Once Rust is installed, the window manager will be built automatically when you run `make` in the kernel directory.

## Troubleshooting

If you see "undefined reference to `wm_init`" errors:
- Make sure Rust is installed and in your PATH
- Verify the target is installed: `rustup target list | grep x86_64-unknown-none`
- Try building manually: `cd wm_rust && cargo build --release --target x86_64-unknown-none`
