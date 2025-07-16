bits 64
global start
extern kmain

section .text
start:
    call kmain
    hlt 