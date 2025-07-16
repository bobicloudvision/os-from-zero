# os-from-zero

A minimal x86 operating system from scratch.

## Prerequisites
- NASM (Netwide Assembler)
- QEMU (for emulation)

## Building and Running

1. Assemble the bootloader:
   ```sh
   make
   ```
2. Run in QEMU:
   ```sh
   make run
   ```
3. Clean build artifacts:
   ```sh
   make clean
   ```

## Project Structure
- `bootloader/boot.asm` — Boot sector assembly code
- `Makefile` — Build and run automation

## Next Steps
- Add a kernel and load it from the bootloader
- Implement basic kernel functionality