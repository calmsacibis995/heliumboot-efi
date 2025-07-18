ARM_PREFIX = arm-linux-gnueabihf-
ARM_CC = $(ARM_PREFIX)gcc
ARM_LD = $(ARM_PREFIX)ld
ARM_OBJCOPY = $(ARM_PREFIX)objcopy

AARCH64_PREFIX = aarch64-linux-gnu-
AARCH64_CC = $(AARCH64_PREFIX)gcc
AARCH64_LD = $(AARCH64_PREFIX)ld
AARCH64_OBJCOPY = $(AARCH64_PREFIX)objcopy

all: x86_64_build aarch64_build

mkdisk:
	if [ ! -e fat.img ]; then \
		dd if=/dev/zero of=fat.img bs=1k count=1440; \
		mformat -i fat.img -f 1440 ::; \
		mmd -i fat.img ::/EFI; \
		mmd -i fat.img ::/EFI/BOOT; \
	else \
		echo "fat.img already exists, skipping creation."; \
	fi

mkver_x86_64:
	./newvers.sh x86_64

mkver_aarch64:
	./newvers.sh aarch64

ia32_build:
	@echo "Not available yet!"

x86_64_build: mkdisk mkver_x86_64
	@if [ ! -d x86_64 ]; then mkdir x86_64; fi

	gcc \
		-I$(HOME)/gnu-efi/inc \
		-fpic \
		-ffreestanding \
		-fno-stack-protector \
		-fno-stack-check \
		-fshort-wchar \
		-mno-red-zone \
		-maccumulate-outgoing-args \
		-c commands.c -o x86_64/commands.o

	gcc \
		-I$(HOME)/gnu-efi/inc \
		-fpic \
		-ffreestanding \
		-fno-stack-protector \
		-fno-stack-check \
		-fshort-wchar \
		-mno-red-zone \
		-maccumulate-outgoing-args \
		-c loadfile.c -o x86_64/loadfile.o

	gcc \
		-I$(HOME)/gnu-efi/inc \
		-fpic \
		-ffreestanding \
		-fno-stack-protector \
		-fno-stack-check \
		-fshort-wchar \
		-mno-red-zone \
		-maccumulate-outgoing-args \
		-c boot.c -o x86_64/boot.o

	gcc \
		-I$(HOME)/gnu-efi/inc \
		-fpic \
		-ffreestanding \
		-fno-stack-protector \
		-fno-stack-check \
		-fshort-wchar \
		-mno-red-zone \
		-maccumulate-outgoing-args \
		-c cmd_table.c -o x86_64/cmd_table.o

	gcc \
		-I$(HOME)/gnu-efi/inc \
		-fpic \
		-ffreestanding \
		-fno-stack-protector \
		-fno-stack-check \
		-fshort-wchar \
		-mno-red-zone \
		-maccumulate-outgoing-args \
		-c video.c -o x86_64/video.o

	gcc \
		-I$(HOME)/gnu-efi/inc \
		-fpic \
		-ffreestanding \
		-fno-stack-protector \
		-fno-stack-check \
		-fshort-wchar \
		-mno-red-zone \
		-maccumulate-outgoing-args \
		-c vers.c -o x86_64/vers.o

	ld \
		-shared \
		-Bsymbolic \
		-L$(HOME)/gnu-efi/x86_64/lib \
		-L$(HOME)/gnu-efi/x86_64/gnuefi \
		-T$(HOME)/gnu-efi/gnuefi/elf_x86_64_efi.lds \
		$(HOME)/gnu-efi/x86_64/gnuefi/crt0-efi-x86_64.o \
		x86_64/commands.o \
		x86_64/loadfile.o \
		x86_64/cmd_table.o \
		x86_64/boot.o \
		x86_64/video.o \
		x86_64/vers.o \
		-o x86_64/boot.so \
		-lgnuefi -lefi

	objcopy -j .text -j .sdata -j .data -j .rodata -j .dynamic -j .dynsym  -j .rel -j .rela -j .rel.* -j .rela.* -j .reloc --target efi-app-x86_64 --subsystem=10 x86_64/boot.so x86_64/boot.efi
	mcopy -i fat.img x86_64/boot.efi ::/EFI/BOOT/BOOTX64.EFI

	gcc -I$(HOME)/gnu-efi/inc -fpic -ffreestanding -fno-stack-protector -fno-stack-check -fshort-wchar -mno-red-zone -maccumulate-outgoing-args -c heliuminst.c -o x86_64/heliuminst.o
	ld -shared -Bsymbolic -L$(HOME)/gnu-efi/x86_64/lib -L$(HOME)/gnu-efi/x86_64/gnuefi -T$(HOME)/gnu-efi/gnuefi/elf_x86_64_efi.lds $(HOME)/gnu-efi/x86_64/gnuefi/crt0-efi-x86_64.o x86_64/heliuminst.o -o x86_64/heliuminst.so -lgnuefi -lefi
	objcopy -j .text -j .sdata -j .data -j .rodata -j .dynamic -j .dynsym  -j .rel -j .rela -j .rel.* -j .rela.* -j .reloc --target efi-app-x86_64 --subsystem=10 x86_64/heliuminst.so x86_64/heliuminst.efi
	mcopy -i fat.img x86_64/heliuminst.efi ::/hinst_x64.efi

arm_build: mkdisk mkver
	@if [ ! -d arm ]; then mkdir arm; fi
	$(ARM_CC) \
		-I$(HOME)/gnu-efi/inc \
		-fpic \
		-ffreestanding \
		-fno-stack-protector \
		-fno-stack-check \
		-fshort-wchar \
		-c boot.c -o arm/boot.o

	$(ARM_LD) \
		-shared \
		-Bsymbolic \
		-nostdlib \
		-L$(HOME)/gnu-efi/arm/lib \
		-L$(HOME)/gnu-efi/arm/gnuefi \
		-T$(HOME)/gnu-efi/gnuefi/elf_arm_efi.lds \
		$(HOME)/gnu-efi/arm/gnuefi/crt0-efi-arm.o \
		arm/boot.o -o arm/boot.so \
		-lgnuefi -lefi

	$(ARM_OBJCOPY) \
		-j .text \
		-j .sdata \
		-j .data \
		-j .rodata \
		-j .dynamic \
		-j .dynsym  \
		-j .rel \
		-j .rela \
		-j .rel.* \
		-j .rela.* \
		-j .reloc \
		--target efi-app-arm \
		--subsystem=10 \
		arm/boot.so arm/boot.efi

	mcopy -i fat.img arm/boot.efi ::/EFI/BOOT/BOOTARM.EFI

aarch64_build: mkdisk mkver_aarch64
	@if [ ! -d aarch64 ]; then mkdir aarch64; fi

	$(AARCH64_CC) \
		-I$(HOME)/gnu-efi/inc \
		-fpic \
		-ffreestanding \
		-fno-stack-protector \
		-fno-stack-check \
		-fshort-wchar \
		-c boot.c -o aarch64/boot.o

	$(AARCH64_CC) \
		-I$(HOME)/gnu-efi/inc \
		-fpic \
		-ffreestanding \
		-fno-stack-protector \
		-fno-stack-check \
		-fshort-wchar \
		-c commands.c -o aarch64/commands.o

	$(AARCH64_CC) \
		-I$(HOME)/gnu-efi/inc \
		-fpic \
		-ffreestanding \
		-fno-stack-protector \
		-fno-stack-check \
		-fshort-wchar \
		-c loadfile.c -o aarch64/loadfile.o

	$(AARCH64_CC) \
		-I$(HOME)/gnu-efi/inc \
		-fpic \
		-ffreestanding \
		-fno-stack-protector \
		-fno-stack-check \
		-fshort-wchar \
		-c cmd_table.c -o aarch64/cmd_table.o

	$(AARCH64_CC) \
		-I$(HOME)/gnu-efi/inc \
		-fpic \
		-ffreestanding \
		-fno-stack-protector \
		-fno-stack-check \
		-fshort-wchar \
		-c video.c -o aarch64/video.o

	$(AARCH64_CC) \
		-I$(HOME)/gnu-efi/inc \
		-fpic \
		-ffreestanding \
		-fno-stack-protector \
		-fno-stack-check \
		-fshort-wchar \
		-c vers.c -o aarch64/vers.o

	$(AARCH64_LD) \
		-shared \
		-Bsymbolic \
		-nostdlib \
		-L$(HOME)/gnu-efi/aarch64/lib \
		-L$(HOME)/gnu-efi/aarch64/gnuefi \
		-T$(HOME)/gnu-efi/gnuefi/elf_aarch64_efi.lds \
		$(HOME)/gnu-efi/aarch64/gnuefi/crt0-efi-aarch64.o \
		aarch64/commands.o \
		aarch64/loadfile.o \
		aarch64/cmd_table.o \
		aarch64/boot.o \
		aarch64/video.o \
		aarch64/vers.o \
		-o aarch64/boot.so \
		-lgnuefi -lefi

	$(AARCH64_OBJCOPY) \
		-j .text \
		-j .sdata \
		-j .data \
		-j .rodata \
		-j .dynamic \
		-j .dynsym  \
		-j .rel \
		-j .rela \
		-j .rel.* \
		-j .rela.* \
		-j .reloc \
		--target efi-app-aarch64 \
		--subsystem=10 \
		aarch64/boot.so aarch64/boot.efi

	mcopy -i fat.img aarch64/boot.efi ::/EFI/BOOT/BOOTAA64.EFI

	$(AARCH64_CC) \
		-I$(HOME)/gnu-efi/inc \
		-fpic \
		-ffreestanding \
		-fno-stack-protector \
		-fno-stack-check \
		-fshort-wchar \
		-c heliuminst.c -o aarch64/heliuminst.o

	$(AARCH64_LD) \
		-shared \
		-Bsymbolic \
		-nostdlib \
		-L$(HOME)/gnu-efi/aarch64/lib \
		-L$(HOME)/gnu-efi/aarch64/gnuefi \
		-T$(HOME)/gnu-efi/gnuefi/elf_aarch64_efi.lds \
		$(HOME)/gnu-efi/aarch64/gnuefi/crt0-efi-aarch64.o \
		aarch64/heliuminst.o \
		-o aarch64/heliuminst.so \
		-lgnuefi -lefi

	$(AARCH64_OBJCOPY) \
		-j .text \
		-j .sdata \
		-j .data \
		-j .rodata \
		-j .dynamic \
		-j .dynsym  \
		-j .rel \
		-j .rela \
		-j .rel.* \
		-j .rela.* \
		-j .reloc \
		--target efi-app-aarch64 \
		--subsystem=10 \
		aarch64/heliuminst.so aarch64/heliuminst.efi

	mcopy -i fat.img aarch64/heliuminst.efi ::/hinst_aarch64.efi

clean:
	rm -rf ia32 x86_64 arm aarch64 fat.img vers.c heliumboot.iso

.PHONY: all mkdisk ia32_build x86_64_build arm_build
