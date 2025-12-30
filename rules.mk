# Object rules for normal builds
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

# Object rules for debug/dev builds.
x86_64/debug_vers.o: x86_64/vers.c
	$(CC) $(X86_64_DEBUG_CFLAGS) -c $< -o $@

aarch64/debug_vers.o: aarch64/vers.c
	$(AARCH64_CC) $(AARCH64_DEBUG_CFLAGS) -c $< -o $@

x86_64/dev_vers.o: x86_64/vers.c
	$(CC) $(X86_64_DEV_CFLAGS) -c $< -o $@

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
	$(CC) $(X86_64_DEBUG_CFLAGS) -c $< -o $@

aarch64/debug_%.o: src/%.c
	$(AARCH64_CC) $(AARCH64_DEBUG_CFLAGS) -c $< -o $@

x86_64/dev_%.o: src/%.c
	$(CC) $(X86_64_DEV_CFLAGS) -c $< -o $@

aarch64/dev_%.o: src/%.c
	$(AARCH64_CC) $(AARCH64_DEV_CFLAGS) -c $< -o $@

x86_64/boot_debug.efi: x86_64/boot_debug.so
	objcopy $(EFI_OBJCOPY_FLAGS) --target efi-app-x86_64 $< $@

aarch64/boot_debug.efi: aarch64/boot_debug.so
	$(AARCH64_OBJCOPY) $(EFI_OBJCOPY_FLAGS) --target efi-app-aarch64 $< $@

x86_64/boot_dev.efi: x86_64/boot_dev.so
	objcopy $(EFI_OBJCOPY_FLAGS) --target efi-app-x86_64 $< $@

aarch64/boot_dev.efi: aarch64/boot_dev.so
	$(AARCH64_OBJCOPY) $(EFI_OBJCOPY_FLAGS) --target efi-app-aarch64 $< $@

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

	./newvers.sh x86_64 x86_64/vers.c
	./newvers.sh aarch64 aarch64/vers.c

x86_64_build: prep_build x86_64/boot.efi
	$(call link_x86_64)
	mcopy -i fat.img x86_64/boot.efi ::/EFI/BOOT/BOOTX64.EFI

aarch64_build: prep_build aarch64/boot.efi
	$(call link_aarch64)
	mcopy -i fat.img aarch64/boot.efi ::/EFI/BOOT/BOOTAA64.EFI

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

iso_prep:
	@rm -rf iso_root
	@mkdir -p iso_root/EFI/BOOT
	@cp x86_64/boot.efi iso_root/EFI/BOOT/BOOTX64.EFI
	@cp aarch64/boot.efi iso_root/EFI/BOOT/BOOTAA64.EFI

heliumboot.iso: iso_prep
	xorriso -as mkisofs -no-emul-boot -iso-level 3 -full-iso9660-filenames \
		-volid "HELIUM_BOOT" -output heliumboot.iso iso_root
