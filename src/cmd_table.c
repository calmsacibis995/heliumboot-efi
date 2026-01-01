/*
 * HeliumBoot/EFI - A simple UEFI bootloader.
 *
 * Copyright (c) 2025, 2026 Stefanos Stefanidis.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * Neither the name of the copyright holder nor the names of its contributors
 * may be used to endorse or promote products derived from this software
 * without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <efi.h>
#include <efilib.h>

#include "boot.h"

struct boot_command_tab cmd_tab[] = {
	{ L"?", help, CMD_NO_ARGS, L"?: help" },
	{ L"about", about, CMD_NO_ARGS, L"about: about" },
	{ L"boot", boot, CMD_REQUIRED_ARGS, L"boot: sd(x,y)FILE [ARGS]" },
	{ L"boot_efi", boot_efi, CMD_REQUIRED_ARGS, L"boot_efi: sd(x,y)FILE [ARGS]" },
	{ L"clear", cls, CMD_NO_ARGS, L"clear: cls" },
	{ L"cls", cls, CMD_NO_ARGS, L"cls: cls" },
	{ L"dir", ls, CMD_OPTIONAL_ARGS, L"dir: ls" },
	{ L"echo", echo, CMD_REQUIRED_ARGS, L"echo: STRING" },
	{ L"exit", exit, CMD_NO_ARGS, L"exit: exit" },
	{ L"help", help, CMD_NO_ARGS, L"help: help" },
	{ L"hinv", hinv, CMD_NO_ARGS, L"hinv: hinv" },
	{ L"ls", ls, CMD_OPTIONAL_ARGS, L"ls: sd(x,y) or DRIVE:\\" },
	{ L"lsblk", lsblk, CMD_NO_ARGS, L"lsblk: lsblk" },
	{ L"reboot", reboot, CMD_NO_ARGS, L"reboot: reboot" },
	{ L"revision", print_revision, CMD_NO_ARGS, L"revision: revision" },
	{ L"version", print_version, CMD_NO_ARGS, L"version: version" },
	{ NULL, NULL, CMD_NO_ARGS, NULL }
};
