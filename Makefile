# Makefile for our OS

# Set the compiler and flags
CC = x86_64-elf-gcc
AS = nasm
LD = x86_64-elf-ld
CFLAGS = -Wall -Wextra -std=c11 -ffreestanding -O2 -g -no-pie
ASFLAGS = -f elf64
LDFLAGS = -T src/linker.ld -nostdlib -melf_x86_64

# Set the directories
SRC_DIR = src
BUILD_DIR = build
DIST_DIR = dist

# Set the files
KERNEL_C = $(SRC_DIR)/kernel.c
BOOT_ASM = $(SRC_DIR)/boot.asm
KERNEL_OBJ = $(BUILD_DIR)/kernel.o
BOOT_OBJ = $(BUILD_DIR)/boot.o
KERNEL_BIN = $(BUILD_DIR)/kernel.bin

# Set the Limine files
LIMINE_DIR = limine
LIMINE_CFG = src/limine.cfg
LIMINE_BOOT = $(LIMINE_DIR)/limine-bios.sys
LIMINE_UEFI_CD = $(LIMINE_DIR)/limine-uefi-cd.bin
LIMINE_BIOS_CD = $(LIMINE_DIR)/limine-bios-cd.bin
LIMINE_EFI = $(LIMINE_DIR)/BOOTX64.EFI
LIMINE_IMG = $(DIST_DIR)/myos.iso

# Default target
all: $(LIMINE_IMG)

# Compile the kernel
$(KERNEL_OBJ): $(KERNEL_C)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

# Assemble the boot file
$(BOOT_OBJ): $(BOOT_ASM)
	@mkdir -p $(@D)
	$(AS) $(ASFLAGS) $< -o $@

# Link the kernel
$(KERNEL_BIN): $(KERNEL_OBJ) $(BOOT_OBJ)
	@mkdir -p $(@D)
	$(LD) $(LDFLAGS) -o $@ $(BOOT_OBJ) $(KERNEL_OBJ)

# Create the ISO image
$(LIMINE_IMG): $(KERNEL_BIN) $(LIMINE_CFG)
	make -C $(LIMINE_DIR)
	@mkdir -p $(DIST_DIR)/iso_root/boot
	@mkdir -p $(DIST_DIR)/iso_root/EFI/BOOT
	cp $(KERNEL_BIN) $(DIST_DIR)/iso_root/boot/kernel.bin
	cp $(LIMINE_CFG) $(DIST_DIR)/iso_root/limine.cfg
	cp $(LIMINE_BOOT) $(DIST_DIR)/iso_root/boot/
	cp $(LIMINE_UEFI_CD) $(DIST_DIR)/iso_root/boot/
	cp $(LIMINE_BIOS_CD) $(DIST_DIR)/iso_root/boot/
	cp $(LIMINE_EFI) $(DIST_DIR)/iso_root/EFI/BOOT/
	xorriso -as mkisofs -b boot/limine-bios-cd.bin \
		-no-emul-boot -boot-load-size 4 -boot-info-table \
		--no-emul-boot -eltorito-alt-boot \
		-e boot/limine-uefi-cd.bin -no-emul-boot \
		-o $(LIMINE_IMG) $(DIST_DIR)/iso_root
	$(LIMINE_DIR)/limine bios-install $(LIMINE_IMG)

# Run the OS in QEMU
run: $(LIMINE_IMG)
	qemu-system-x86_64 -cdrom $(LIMINE_IMG)

# Clean the build files
clean:
	rm -rf $(BUILD_DIR) $(DIST_DIR) 