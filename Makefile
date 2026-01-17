#
# HeliumBoot/EFI - A simple UEFI bootloader.
#
# Copyright (c) 2025, 2026 Stefanos Stefanidis.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1. Redistributions of source code must retain the above copyright notice,
# this list of conditions and the following disclaimer.
#
# 2. Redistributions in binary form must reproduce the above copyright notice,
# this list of conditions and the following disclaimer in the documentation
# and/or other materials provided with the distribution.
#
# 3. Neither the name of the copyright holder nor the names of its contributors
# may be used to endorse or promote products derived from this software
# without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#

include cross.mk

# Common source files.
SOURCES = src/commands.c src/loadfile.c src/cmd_table.c src/fs_table.c \
	src/main.c src/video.c src/vtoc.c src/bfs.c src/ufs.c src/s5fs.c \
	src/helpers.c src/menu.c src/exec_efi.c src/exec_elf.c src/exec_aout.c \
	src/exec_coff.c src/config.c src/disk.c src/download.c src/vnode.c

X86_64_OBJS = $(patsubst src/%.c,x86_64/%.o,$(SOURCES)) x86_64/vers.o
AARCH64_OBJS = $(patsubst src/%.c,aarch64/%.o,$(SOURCES)) aarch64/vers.o
RISCV64_OBJS = $(patsubst src/%.c,riscv64/%.o,$(SOURCES)) riscv64/vers.o

X86_64_DEBUG_OBJS = $(patsubst src/%.c,x86_64/debug_%.o,$(SOURCES)) x86_64/debug_vers.o
AARCH64_DEBUG_OBJS = $(patsubst src/%.c,aarch64/debug_%.o,$(SOURCES)) aarch64/debug_vers.o
RISCV64_DEBUG_OBJS = $(patsubst src/%.c,riscv64/debug_%.o,$(SOURCES)) riscv64/debug_vers.o

X86_64_DEV_OBJS = $(patsubst src/%.c,x86_64/dev_%.o,$(SOURCES)) x86_64/dev_vers.o
AARCH64_DEV_OBJS = $(patsubst src/%.c,aarch64/dev_%.o,$(SOURCES)) aarch64/dev_vers.o
RISCV64_DEV_OBJS = $(patsubst src/%.c,riscv64/dev_%.o,$(SOURCES)) riscv64/dev_vers.o

all: sel_build

include rules.mk

sel_build:
	@echo "You have to select a target."
	@echo "Make sure you have built GNU-EFI first!"
	@echo ""
	@echo "make x86_64_build:         Build HeliumBoot for x86_64"
	@echo "make aarch64_build:        Build HeliumBoot for AArch64"
	@echo "make riscv64_build:        Build HeliumBoot for 64-bit RISC-V"
	@echo "make x86_64_dev_build:     Build HeliumBoot/Dev for x86_64"
	@echo "make aarch64_dev_build:    Build HeliumBoot/Dev for AArch64"
	@echo "make riscv64_dev_build:    Build HeliumBoot/Dev for 64-bit RISC-V"
	@echo "make x86_64_debug_build:   Build HeliumBoot/Debug for x86_64"
	@echo "make aarch64_debug_build:  Build HeliumBoot/Debug for AArch64"
	@echo "make riscv64_debug_build:  Build HeliumBoot/Debug for 64-bit RISC-V"
	@echo "make x86_64_iso:           Build HeliumBoot ISO for x86_64"
	@echo "make aarch64_iso:          Build HeliumBoot ISO for AArch64"

clean:
	$(HIDE)rm -rf x86_64 aarch64 riscv64 fat.img heliumboot_x86_64.iso heliumboot_aarch64.iso iso_root efi.img mnt

.PHONY: all sel_build prep_build x86_64_build aarch64_build riscv64_build x86_64_dev_build aarch64_dev_build x86_64_debug_build aarch64_debug_build x86_64_iso aarch64_iso clean
