# Object rules for normal builds
x86_64/vers.o: x86_64/vers.c
	$(X86_64_CC) $(X86_64_CFLAGS) -c $< -o $@

aarch64/vers.o: aarch64/vers.c
	$(AARCH64_CC) $(AARCH64_CFLAGS) -c $< -o $@

riscv64/vers.o: riscv64/vers.c
	$(RISCV64_CC) $(RISCV64_CFLAGS) -c $< -o $@

x86_64/%.o: src/%.c
	$(X86_64_CC) $(X86_64_CFLAGS) -c $< -o $@

aarch64/%.o: src/%.c
	$(AARCH64_CC) $(AARCH64_CFLAGS) -c $< -o $@

riscv64/%.o: src/%.c
	$(RISCV64_CC) $(RISCV64_CFLAGS) -c $< -o $@

x86_64/boot.so: $(X86_64_OBJS)
	$(call link_x86_64)

aarch64/boot.so: $(AARCH64_OBJS)
	$(call link_aarch64)

riscv64/boot.so: $(RISCV64_OBJS)
	$(call link_riscv64)

# Object rules for debug/dev builds.
x86_64/debug_vers.o: x86_64/vers.c
	$(X86_64_CC) $(X86_64_DEBUG_CFLAGS) -c $< -o $@

aarch64/debug_vers.o: aarch64/vers.c
	$(AARCH64_CC) $(AARCH64_DEBUG_CFLAGS) -c $< -o $@

x86_64/dev_vers.o: x86_64/vers.c
	$(X86_64_CC) $(X86_64_DEV_CFLAGS) -c $< -o $@

aarch64/dev_vers.o: aarch64/vers.c
	$(AARCH64_CC) $(AARCH64_DEV_CFLAGS) -c $< -o $@

x86_64/boot_debug.so: $(X86_64_DEBUG_OBJS)
	$(call link_x86_64_debug)

aarch64/boot_debug.so: $(AARCH64_DEBUG_OBJS)
	$(call link_aarch64_debug)

x86_64/boot_dev.so: $(X86_64_DEV_OBJS)
	$(call link_x86_64_dev)

aarch64/boot_dev.so: $(AARCH64_DEV_OBJS)
	$(call link_aarch64_dev)

x86_64/debug_%.o: src/%.c
	$(X86_64_CC) $(X86_64_DEBUG_CFLAGS) -c $< -o $@

aarch64/debug_%.o: src/%.c
	$(AARCH64_CC) $(AARCH64_DEBUG_CFLAGS) -c $< -o $@

x86_64/dev_%.o: src/%.c
	$(X86_64_CC) $(X86_64_DEV_CFLAGS) -c $< -o $@

aarch64/dev_%.o: src/%.c
	$(AARCH64_CC) $(AARCH64_DEV_CFLAGS) -c $< -o $@

x86_64/boot_debug.efi: x86_64/boot_debug.so
	$(X86_64_OBJCOPY) $(EFI_OBJCOPY_FLAGS) --target efi-app-x86_64 $< $@

aarch64/boot_debug.efi: aarch64/boot_debug.so
	$(AARCH64_OBJCOPY) $(EFI_OBJCOPY_FLAGS) --target efi-app-aarch64 $< $@

x86_64/boot_dev.efi: x86_64/boot_dev.so
	objcopy $(EFI_OBJCOPY_FLAGS) --target efi-app-x86_64 $< $@

aarch64/boot_dev.efi: aarch64/boot_dev.so
	$(AARCH64_OBJCOPY) $(EFI_OBJCOPY_FLAGS) --target efi-app-aarch64 $< $@

# Linking macros.
define link_x86_64
$(X86_64_LD) -shared -Bsymbolic \
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

define link_riscv64
$(RISCV64_LD) -shared -Bsymbolic -nostdlib \
	-L$(RISCV64_LIB_DIR)/lib -L$(RISCV64_LIB_DIR)/gnuefi \
	-T$(RISCV64_LINK_SCRIPT) \
	$(RISCV64_LIB_DIR)/gnuefi/crt0-efi-riscv64.o \
	$(RISCV64_OBJS) -o riscv64/boot.so -lgnuefi -lefi
endef

# Link macros for dev/debug builds.
define link_x86_64_dev
ld -shared -Bsymbolic \
	-L$(X86_64_LIB_DIR)/lib -L$(X86_64_LIB_DIR)/gnuefi \
	-T$(X86_64_LINK_SCRIPT) \
	$(X86_64_LIB_DIR)/gnuefi/crt0-efi-x86_64.o \
	$(X86_64_DEV_OBJS) -o x86_64/boot_dev.so -lgnuefi -lefi
endef

define link_aarch64_dev
$(AARCH64_LD) -shared -Bsymbolic -nostdlib \
	-L$(AARCH64_LIB_DIR)/lib -L$(AARCH64_LIB_DIR)/gnuefi \
	-T$(AARCH64_LINK_SCRIPT) \
	$(AARCH64_LIB_DIR)/gnuefi/crt0-efi-aarch64.o \
	$(AARCH64_DEV_OBJS) -o aarch64/boot_dev.so -lgnuefi -lefi
endef

define link_x86_64_debug
ld -shared -Bsymbolic \
	-L$(X86_64_LIB_DIR)/lib -L$(X86_64_LIB_DIR)/gnuefi \
	-T$(X86_64_LINK_SCRIPT) \
	$(X86_64_LIB_DIR)/gnuefi/crt0-efi-x86_64.o \
	$(X86_64_DEBUG_OBJS) -o x86_64/boot_debug.so -lgnuefi -lefi
endef

define link_aarch64_debug
$(AARCH64_LD) -shared -Bsymbolic -nostdlib \
	-L$(AARCH64_LIB_DIR)/lib -L$(AARCH64_LIB_DIR)/gnuefi \
	-T$(AARCH64_LINK_SCRIPT) \
	$(AARCH64_LIB_DIR)/gnuefi/crt0-efi-aarch64.o \
	$(AARCH64_DEBUG_OBJS) -o aarch64/boot_debug.so -lgnuefi -lefi
endef

x86_64/boot.efi: x86_64/boot.so
	$(X86_64_OBJCOPY) $(EFI_OBJCOPY_FLAGS) --target efi-app-x86_64 $< $@

aarch64/boot.efi: aarch64/boot.so
	$(AARCH64_OBJCOPY) $(EFI_OBJCOPY_FLAGS) --target efi-app-aarch64 $< $@

riscv64/boot.efi: riscv64/boot.so
	$(RISCV64_OBJCOPY) $(EFI_OBJCOPY_FLAGS) --target efi-app-riscv64 $< $@

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
	@if [ ! -d riscv64 ]; then mkdir riscv64; fi

	./newvers.sh x86_64 x86_64/vers.c
	./newvers.sh aarch64 aarch64/vers.c
	./newvers.sh riscv64 riscv64/vers.c

x86_64_build: prep_build x86_64/boot.efi
	$(call link_x86_64)
	mcopy -i fat.img x86_64/boot.efi ::/EFI/BOOT/BOOTX64.EFI

aarch64_build: prep_build aarch64/boot.efi
	$(call link_aarch64)
	mcopy -i fat.img aarch64/boot.efi ::/EFI/BOOT/BOOTAA64.EFI

riscv64_build: prep_build riscv64/boot.efi
	$(call link_riscv64)
	mcopy -i fat.img riscv64/boot.efi ::/EFI/BOOT/BOOTRISCV.EFI

x86_64_debug_build: prep_build x86_64/boot_debug.efi
	$(call link_x86_64_debug)
	mcopy -i fat.img x86_64/boot_debug.efi ::/EFI/BOOT/BOOTX64.EFI

aarch64_debug_build: prep_build aarch64/boot_debug.efi
	$(call link_aarch64_debug)
	mcopy -i fat.img aarch64/boot_debug.efi ::/EFI/BOOT/BOOTAA64.EFI

x86_64_dev_build: prep_build x86_64/boot_dev.efi
	$(call link_x86_64_dev)
	mcopy -i fat.img x86_64/boot_dev.efi ::/EFI/BOOT/BOOTX64.EFI

aarch64_dev_build: prep_build aarch64/boot_dev.efi
	$(call link_aarch64_dev)
	mcopy -i fat.img aarch64/boot_dev.efi ::/EFI/BOOT/BOOTAA64.EFI

x86_64_iso_prep:
	@rm -rf iso_root
	@mkdir -p iso_root/EFI/BOOT
	@cp x86_64/boot.efi iso_root/EFI/BOOT/BOOTX64.EFI
	@dd if=/dev/zero of=efi.img bs=1M count=64
	@mkfs.vfat -F 12 efi.img
	@mkdir -p mnt
	@sudo mount -o loop efi.img mnt
	@sudo mkdir -p mnt/EFI/BOOT
	@sudo cp iso_root/EFI/BOOT/BOOTX64.EFI mnt/EFI/BOOT/
	@sudo umount mnt
	@rmdir mnt
	@sudo cp efi.img iso_root

aarch64_iso_prep:
	@rm -rf iso_root
	@mkdir -p iso_root/EFI/BOOT
	@cp aarch64/boot.efi iso_root/EFI/BOOT/BOOTAA64.EFI
	@dd if=/dev/zero of=efi.img bs=1M count=64
	@mkfs.vfat -F 12 efi.img
	@mkdir -p mnt
	@sudo mount -o loop efi.img mnt
	@sudo mkdir -p mnt/EFI/BOOT
	@sudo cp iso_root/EFI/BOOT/BOOTAA64.EFI mnt/EFI/BOOT/
	@sudo umount mnt
	@rmdir mnt
	@sudo cp efi.img iso_root

x86_64_iso: x86_64_iso_prep
	xorriso -as mkisofs -no-emul-boot -iso-level 3 -full-iso9660-filenames \
		-eltorito-platform efi -eltorito-boot efi.img -volid "HELIUMBOOT" \
		-output heliumboot_x86_64.iso iso_root

aarch64_iso: aarch64_iso_prep
	xorriso -as mkisofs -no-emul-boot -iso-level 3 -full-iso9660-filenames \
		-eltorito-platform efi -eltorito-boot EFI/BOOT/BOOTAA64.EFI -volid "HELIUMBOOT" \
		-output heliumboot_aarch64.iso iso_root

