/* kernel/linker64.ld - Linker script for 64-bit kernel */
OUTPUT_FORMAT("binary")
ENTRY(kernel_main)
SECTIONS {
    . = 0x200000;
    .text : { *(.text*) }
    .rodata : { *(.rodata*) }
    .data : { *(.data*) }
    .bss : { *(.bss*) }
} 