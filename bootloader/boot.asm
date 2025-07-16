; bootloader/boot.asm - Minimal boot sector that prints 'Hello, OS!'
; Assemble with: nasm -f bin boot.asm -o boot.bin

BITS 16
ORG 0x7C00

start:
    mov si, message
    call print_string
    jmp $

print_string:
    lodsb
    or al, al
    jz .done
    mov ah, 0x0E
    mov bh, 0x00
    mov bl, 0x07
    int 0x10
    jmp print_string
.done:
    ret

message db 'Hello, OS!', 0

times 510-($-$$) db 0  ; Pad to 510 bytes
DW 0xAA55              ; Boot signature 