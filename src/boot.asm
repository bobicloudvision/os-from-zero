bits 64
global start
extern kmain

section .boot
start:
    call kmain
    hlt 