; bootloader/loader64.asm - Stage 2 loader for 64-bit OS
; Assemble with: nasm -f bin loader64.asm -o loader64.bin

BITS 16
ORG 0x8000

start:
    cli
    xor ax, ax
    mov ds, ax
    mov ss, ax
    mov sp, 0x9000

    ; Load kernel (assume fits in 64 sectors) to 0x200000
    mov si, 64            ; Number of sectors
    mov bx, 0x0000        ; Offset
    mov ax, 0x2000        ; Segment for 0x200000
    mov es, ax
    mov di, 22            ; Start at sector 22 (after boot + loader)
.load_kernel:
    mov ah, 0x02          ; BIOS read sectors
    mov al, 1             ; Read 1 sector
    mov ch, 0             ; Cylinder
    mov cl, di            ; Sector number
    mov dh, 0             ; Head
    mov dl, 0x00          ; Drive
    int 0x13
    jc disk_error
    add bx, 0x200         ; Next sector in memory
    cmp bx, 0x0000
    jne .skip_inc_es
    add ax, 0x10          ; Next segment (for >64KB)
    mov es, ax
.skip_inc_es:
    inc di
    dec si
    jnz .load_kernel

    ; Set up GDT for long mode
    lgdt [gdt_desc]

    ; Enter protected mode
    mov eax, cr0
    or eax, 1
    mov cr0, eax
    jmp 0x08:protected_mode

[bits 32]
protected_mode:
    ; Enable PAE
    mov eax, cr4
    or eax, 1 << 5
    mov cr4, eax

    ; Set up PML4 (identity map 0x200000)
    mov eax, pml4_table
    mov cr3, eax

    ; Enable long mode
    mov ecx, 0xC0000080
    rdmsr
    or eax, 1 << 8
    wrmsr

    ; Enable paging
    mov eax, cr0
    or eax, 0x80000000
    mov cr0, eax

    ; Far jump to 64-bit code
    jmp 0x18:long_mode

align 16
[bits 64]
long_mode:
    mov rsp, 0x300000
    mov rdi, message
    call print_string64
    jmp 0x200000          ; Jump to kernel entry

print_string64:
    mov al, [rdi]
    test al, al
    jz .done
    mov ah, 0x0E
    mov bh, 0x00
    mov bl, 0x07
    int 0x10
    inc rdi
    jmp print_string64
.done:
    ret

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

message db 'Entering 64-bit long mode...', 0
error_msg db 'Disk read error!', 0

align 16
pml4_table: times 4096 db 0

gdt_start:
    dq 0x0000000000000000 ; Null
    dq 0x00af9a000000ffff ; Code 32-bit
    dq 0x00af92000000ffff ; Data 32-bit
    dq 0x00af9a000000ffff ; Code 64-bit
    dq 0x00af92000000ffff ; Data 64-bit
gdt_end:

gdt_desc:
    dw gdt_end - gdt_start - 1
    dq gdt_start 