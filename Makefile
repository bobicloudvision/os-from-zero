# Makefile for os-from-zero (64-bit, two-stage bootloader)

BOOT64 = bootloader/boot64.asm
BOOT64BIN = bootloader/boot64.bin
LOADER64 = bootloader/loader64.asm
LOADER64BIN = bootloader/loader64.bin
KERNEL64_SRC = kernel/kernel64.c
KERNEL64_BIN = kernel/kernel64.bin
KERNEL64_LD = kernel/linker64.ld
OS_IMAGE64 = os-image64.bin

all: $(BOOT64BIN) $(LOADER64BIN) $(KERNEL64_BIN) $(OS_IMAGE64)

$(BOOT64BIN): $(BOOT64)
	nasm -f bin $(BOOT64) -o $(BOOT64BIN)

$(LOADER64BIN): $(LOADER64)
	nasm -f bin $(LOADER64) -o $(LOADER64BIN)

$(KERNEL64_BIN): $(KERNEL64_SRC) $(KERNEL64_LD)
	x86_64-elf-gcc -ffreestanding -m64 -c $(KERNEL64_SRC) -o kernel/kernel64.o
	x86_64-elf-ld -T $(KERNEL64_LD) kernel/kernel64.o -o $(KERNEL64_BIN)

$(OS_IMAGE64): $(BOOT64BIN) $(LOADER64BIN) $(KERNEL64_BIN)
	cat $(BOOT64BIN) $(LOADER64BIN) $(KERNEL64_BIN) > $(OS_IMAGE64)

run-os-image64: $(OS_IMAGE64)
	qemu-system-x86_64 -drive format=raw,file=$(OS_IMAGE64)

clean:
	rm -f $(BOOT64BIN) $(LOADER64BIN) $(KERNEL64_BIN) $(OS_IMAGE64) kernel/kernel64.o 