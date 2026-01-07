#
# Environment variables for cross-compiling HeliumBoot.
#

HOST_OS := $(shell uname -s)
HOST_ARCH := $(shell uname -m)

TARGET_ARCH ?= $(HOST_ARCH)

X86_64_PREFIX = x86_64-linux-gnu-
X86_64_CC = $(X86_64_PREFIX)gcc
X86_64_LD = $(X86_64_PREFIX)ld
X86_64_OBJCOPY = $(X86_64_PREFIX)objcopy

AARCH64_PREFIX = aarch64-linux-gnu-
AARCH64_CC = $(AARCH64_PREFIX)gcc
AARCH64_LD = $(AARCH64_PREFIX)ld
AARCH64_OBJCOPY = $(AARCH64_PREFIX)objcopy

RISCV64_PREFIX = riscv64-linux-gnu-
RISCV64_CC = $(RISCV64_PREFIX)gcc
RISCV64_LD = $(RISCV64_PREFIX)ld
RISCV64_OBJCOPY = $(RISCV64_PREFIX)objcopy

ifeq ($(HOST_OS),Darwin)
	X86_64_PREFIX = x86_64-elf-
	AARCH64_PREFIX = aarch64-none-elf-
endif

ifeq ($(TARGET_ARCH),$(HOST_ARCH))
    CROSS_PREFIX :=
else ifeq ($(TARGET_ARCH),x86_64)
    CROSS_PREFIX := $(X86_64_PREFIX)
else ifeq ($(TARGET_ARCH),aarch64)
    CROSS_PREFIX := $(AARCH64_PREFIX)
else ifeq ($(TARGET_ARCH),arm64)
    CROSS_PREFIX := $(AARCH64_PREFIX)
else ifeq ($(TARGET_ARCH),riscv64)
    CROSS_PREFIX := $(RISCV64_PREFIX)
else
    $(error Unsupported TARGET_ARCH: $(TARGET_ARCH))
endif

GNU_EFI_DIR = gnu-efi
EFI_INC_DIR = $(GNU_EFI_DIR)/inc
X86_64_LIB_DIR = $(GNU_EFI_DIR)/x86_64
AARCH64_LIB_DIR = $(GNU_EFI_DIR)/aarch64
RISCV64_LIB_DIR = $(GNU_EFI_DIR)/riscv64

COMMON_CFLAGS = -Iinclude -I$(EFI_INC_DIR) -fpic -ffreestanding -fno-stack-protector -fno-stack-check -Wall
X86_64_CFLAGS = $(COMMON_CFLAGS) -fshort-wchar -mno-red-zone -maccumulate-outgoing-args -DX86_64_BLD
AARCH64_CFLAGS = $(COMMON_CFLAGS) -fshort-wchar -DAARCH64_BLD
RISCV64_CFLAGS = $(COMMON_CFLAGS) -fshort-wchar -DRISCV64_BLD

X86_64_DEBUG_CFLAGS = $(X86_64_CFLAGS) -DDEBUG_BLD
AARCH64_DEBUG_CFLAGS = $(AARCH64_CFLAGS) -DDEBUG_BLD
RISCV64_DEBUG_CFLAGS = $(RISCV64_CFLAGS) -DDEBUG_BLD

X86_64_DEV_CFLAGS = $(X86_64_CFLAGS) -DDEV_BLD
AARCH64_DEV_CFLAGS = $(AARCH64_CFLAGS) -DDEV_BLD
RISCV64_DEV_CFLAGS = $(RISCV64_CFLAGS) -DDEV_BLD

X86_64_LINK_SCRIPT = $(GNU_EFI_DIR)/gnuefi/elf_x86_64_efi.lds
AARCH64_LINK_SCRIPT = $(GNU_EFI_DIR)/gnuefi/elf_aarch64_efi.lds
RISCV64_LINK_SCRIPT = $(GNU_EFI_DIR)/gnuefi/elf_riscv64_efi.lds

EFI_OBJCOPY_FLAGS = -j .text -j .sdata -j .data -j .rodata -j .dynamic \
	-j .dynsym -j .rel -j .rela -j .rel.* -j .rela.* -j .reloc --subsystem=10
