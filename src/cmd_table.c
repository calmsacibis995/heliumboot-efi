/*
 * HeliumBoot/EFI - A simple UEFI bootloader.
 *
 * Copyright (c) 2025 Stefanos Stefanidis.
 * All rights reserved.
 *
 * This file is part of HeliumBoot/EFI.
 *
 * HeliumBoot/EFI is free software: you can redistribute it and/or modify it under the terms of the GNU General
 * Public License as published by the Free Software Foundation, either version 3 of the License, or (at your
 * option) any later version.
 *
 * HeliumBoot/EFI is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even
 * the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with HeliumBoot/EFI. If not, see
 * <https://www.gnu.org/licenses/>.
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
	{ L"ls", ls, CMD_OPTIONAL_ARGS, L"ls: sd(x,y)" },
	{ L"lsblk", lsblk, CMD_NO_ARGS, L"lsblk: lsblk" },
	{ L"reboot", reboot, CMD_NO_ARGS, L"reboot: reboot" },
	{ L"revision", print_revision, CMD_NO_ARGS, L"revision: revision" },
	{ L"version", print_version, CMD_NO_ARGS, L"version: version" },
	{ NULL, NULL, CMD_NO_ARGS, NULL }
};
