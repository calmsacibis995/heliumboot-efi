AARCH64_PREFIX = aarch64-linux-gnu-
AARCH64_CC = $(AARCH64_PREFIX)gcc
AARCH64_LD = $(AARCH64_PREFIX)ld
AARCH64_OBJCOPY = $(AARCH64_PREFIX)objcopy

GNU_EFI_DIR = gnu-efi
EFI_INC_DIR = $(GNU_EFI_DIR)/inc
X86_64_LIB_DIR = $(GNU_EFI_DIR)/x86_64
AARCH64_LIB_DIR = $(GNU_EFI_DIR)/aarch64

COMMON_CFLAGS = -Iinclude -I$(EFI_INC_DIR) -fpic -ffreestanding -fno-stack-protector -fno-stack-check
X86_64_CFLAGS = $(COMMON_CFLAGS) -fshort-wchar -mno-red-zone -maccumulate-outgoing-args
AARCH64_CFLAGS = $(COMMON_CFLAGS) -fshort-wchar

EFI_OBJCOPY_FLAGS = -j .text -j .sdata -j .data -j .rodata -j .dynamic -j .dynsym \
                    -j .rel -j .rela -j .rel.* -j .rela.* -j .reloc --subsystem=10

X86_64_LINK_SCRIPT = $(GNU_EFI_DIR)/gnuefi/elf_x86_64_efi.lds
AARCH64_LINK_SCRIPT = $(GNU_EFI_DIR)/gnuefi/elf_aarch64_efi.lds

SOURCES =	src/commands.c \
			src/loadfile.c \
			src/cmd_table.c \
			src/boot.c \
			src/video.c \
			src/vtoc.c \
			src/helpers.c

X86_64_OBJS = $(patsubst src/%.c,x86_64/%.o,$(SOURCES)) x86_64/vers.o
AARCH64_OBJS = $(patsubst src/%.c,aarch64/%.o,$(SOURCES)) aarch64/vers.o

all: sel_build

x86_64/vers.o: x86_64/vers.c
	$(CC) $(X86_64_CFLAGS) -c $< -o $@

aarch64/vers.o: aarch64/vers.c
	$(AARCH64_CC) $(AARCH64_CFLAGS) -c $< -o $@

x86_64/%.o: src/%.c
	$(CC) $(X86_64_CFLAGS) -c $< -o $@

aarch64/%.o: src/%.c
	$(AARCH64_CC) $(AARCH64_CFLAGS) -c $< -o $@

x86_64/boot.so: $(X86_64_OBJS)
	$(call link_x86_64)

aarch64/boot.so: $(AARCH64_OBJS)
	$(call link_aarch64)

define link_x86_64
ld -shared -Bsymbolic \
	-L$(X86_64_LIB_DIR)/lib -L$(X86_64_LIB_DIR)/gnuefi \
	-T$(X86_64_LINK_SCRIPT) \
	$(X86_64_LIB_DIR)/gnuefi/crt0-efi-x86_64.o \
	$(X86_64_OBJS) -o x86_64/boot.so -lgnuefi -lefi
endef

define link_aarch64
$(AARCH64_LD) -shared -Bsymbolic -nostdlib \
	-L$(AARCH64_LIB_DIR)/lib -L$(AARCH64_LIB_DIR)/gnuefi \
	-T$(AARCH64_LINK_SCRIPT) \
	$(AARCH64_LIB_DIR)/gnuefi/crt0-efi-aarch64.o \
	$(AARCH64_OBJS) -o aarch64/boot.so -lgnuefi -lefi
endef

x86_64/boot.efi: x86_64/boot.so
	objcopy $(EFI_OBJCOPY_FLAGS) --target efi-app-x86_64 $< $@

aarch64/boot.efi: aarch64/boot.so
	$(AARCH64_OBJCOPY) $(EFI_OBJCOPY_FLAGS) --target efi-app-aarch64 $< $@

sel_build:
	@echo "You have to select a target."
	@echo "Make sure you have built GNU-EFI first!"
	@echo ""
	@echo "make x86_64_build:   Build HeliumBoot for x86_64"
	@echo "make aarch64_build:  Build HeliumBoot for AArch64"

prep_build:
	if [ ! -e fat.img ]; then \
		dd if=/dev/zero of=fat.img bs=1k count=1440; \
		mformat -i fat.img -f 1440 ::; \
		mmd -i fat.img ::/EFI; \
		mmd -i fat.img ::/EFI/BOOT; \
	else \
		echo "fat.img already exists, skipping creation."; \
	fi

	@if [ ! -d x86_64 ]; then mkdir x86_64; fi
	@if [ ! -d aarch64 ]; then mkdir aarch64; fi

	./newvers.sh x86_64 x86_64/vers.c
	./newvers.sh aarch64 aarch64/vers.c

x86_64_build: prep_build x86_64/boot.efi
	$(call link_x86_64)
	mcopy -i fat.img x86_64/boot.efi ::/EFI/BOOT/BOOTX64.EFI

aarch64_build: prep_build x86_64/boot.efi
	$(call link_aarch64)
	mcopy -i fat.img aarch64/boot.efi ::/EFI/BOOT/BOOTAA64.EFI

iso_prep:
	@rm -rf iso_root
	@mkdir -p iso_root/EFI/BOOT
	@cp x86_64/boot.efi iso_root/EFI/BOOT/BOOTX64.EFI
	@cp aarch64/boot.efi iso_root/EFI/BOOT/BOOTAA64.EFI

heliumboot.iso: iso_prep
	xorriso -as mkisofs \
		-no-emul-boot \
		-iso-level 3 \
		-full-iso9660-filenames \
		-volid "HELIUM_BOOT" \
		-output heliumboot.iso \
		iso_root

clean:
	rm -rf x86_64 aarch64 fat.img heliumboot.iso iso_root

.PHONY: all sel_build prep_build x86_64_build aarch64_build iso_prep heliumboot.iso clean
