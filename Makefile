# Makefile for os-from-zero

BOOTLOADER = bootloader/boot.asm
BOOTBIN = bootloader/boot.bin

ISO = os-from-zero.iso

all: $(BOOTBIN)

$(BOOTBIN): $(BOOTLOADER)
	nasm -f bin $(BOOTLOADER) -o $(BOOTBIN)

check-bootbin:
	@if [ ! -f $(BOOTBIN) ]; then \
		echo "Error: $(BOOTBIN) not found."; exit 1; \
	fi
	@if [ $$(stat -f%z $(BOOTBIN)) -ne 512 ]; then \
		echo "Error: $(BOOTBIN) is not 512 bytes."; exit 1; \
	fi
	@if [ $$(hexdump -v -s 510 -n 2 -e '2/1 "%02x"' $(BOOTBIN)) != "55aa" ]; then \
		echo "Error: $(BOOTBIN) does not end with 0x55AA."; exit 1; \
	fi

iso: all check-bootbin
	mkdir -p isofiles/boot
	cp $(BOOTBIN) isofiles/boot/boot.bin
	mkisofs -quiet -V 'OSFROMZERO' -input-charset iso8859-1 -boot-load-size 4 -boot-info-table -no-emul-boot -b boot/boot.bin -o $(ISO) isofiles

run: all
	qemu-system-i386 -drive format=raw,file=$(BOOTBIN)

run-iso: iso
	qemu-system-i386 -cdrom $(ISO)

run-bootbin: all check-bootbin
	qemu-system-i386 -drive format=raw,file=$(BOOTBIN)

clean:
	rm -f $(BOOTBIN) $(ISO)
	rm -rf isofiles 