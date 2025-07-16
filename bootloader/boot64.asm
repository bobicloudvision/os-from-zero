; bootloader/boot64.asm - Minimal 64-bit boot sector (stage 1)
; Assemble with: nasm -f bin boot64.asm -o boot64.bin

BITS 16
ORG 0x7C00

start:
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00

    mov si, 20            ; Number of sectors to load (stage 2 loader)
    mov bx, 0x8000        ; Load address
    mov dh, 0             ; Head
    mov dl, 0x00          ; Drive (0x00 = floppy, 0x80 = hdd)
    mov ch, 0             ; Cylinder
    mov cl, 2             ; Start at sector 2 (LBA 1)

.load:
    mov ah, 0x02          ; BIOS read sectors
    mov al, 1             ; Read 1 sector
    int 0x13
    jc disk_error
    add bx, 0x200         ; Next sector in memory
    inc cl                ; Next sector on disk
    dec si
    jnz .load

    jmp 0x0000:0x8000     ; Jump to stage 2 loader

disk_error:
    mov si, error_msg
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

error_msg db 'Disk read error!', 0

times 510-($-$$) db 0
DW 0xAA55 