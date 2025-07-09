/*
 * Copyright (c) 1982, 1983 The Regents of the University of California.
 * All rights reserved.
 *
 * Derived from 4.2BSD's /usr/src/sys/stand/boot.c
 * Revision 6.1, 83/07/29
 *
 * New features:
 *	- Machine independent.
 *	- UEFI support.
 *	- A built-in command interpreter.
 *	- Executes ELF and EFI binaries.
 *	- Small size, yet with comfort features.
 */

#include <efi.h>
#include <efilib.h>

#include "boot.h"

CHAR16 filepath[100] = {0};			// bootloader file path
UINTN bufsize = sizeof(filepath);

EFI_FILE_HANDLE RootFS, File;
EFI_LOADED_IMAGE *LoadedImage;

CHAR16 *ArchNames[] = {
	L"ia32",
	L"x86_64",
	L"arm",
	L"aarch64",
	L"ia64",
	L"riscv64",
	NULL
};

#if defined(__i386__)
UINTN Platform = 0;
#elif defined(__x86_64__)
UINTN Platform = 1;
#elif defined(__arm__)
UINTN Platform = 2;
#elif defined(__aarch64__)
UINTN Platform = 3;
#elif defined(__ia64__)
UINTN Platform = 4;
#elif defined(__riscv) && __riscv_xlen == 64
UINTN Platform = 5;
#endif

static void
SplitCommandLine(CHAR16 *line, CHAR16 **command, CHAR16 **arguments)
{
	CHAR16 *p;

	*command = line;
	*arguments = L"";

	p = line;
	while (*p != L'\0' && *p != L' ')
		p++;

	if (*p == L' ') {
		*p = L'\0';
		p++;

		while (*p == L' ')
			p++;

		*arguments = p;
	}
}

EFI_STATUS EFIAPI
efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable)
{
	EFI_STATUS Status;
	EFI_FILE_IO_INTERFACE *FileSystem;
	EFI_GUID LoadedImageProtocol = EFI_LOADED_IMAGE_PROTOCOL_GUID;
	EFI_GUID FileSystemProtocol = EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID;
	EFI_INPUT_KEY key;
	BOOLEAN matched_command;

	InitializeLib(ImageHandle, SystemTable);

	Print(L"\nHeliumBoot/EFI (%s)\n", ArchNames[Platform]);
	Print(L"%s - %s\n", version, revision);
	Print(L"Built %s\n\n", blddate);

	// Get info about our loaded image.
	Status = uefi_call_wrapper(SystemTable->BootServices->HandleProtocol,
		3, ImageHandle, &LoadedImageProtocol, (void**)&LoadedImage);
	if (EFI_ERROR(Status)) {
		Print(L"Could not get loaded image protocol\n");
		return Status;
	}

	// Get filesystem type and open the device.
	Status = uefi_call_wrapper(SystemTable->BootServices->HandleProtocol,
		3, LoadedImage->DeviceHandle, &FileSystemProtocol, (void**)&FileSystem);
	if (EFI_ERROR(Status)) {
		Print(L"Could not get filesystem protocol\n");
		return Status;
	}

	Status = uefi_call_wrapper(FileSystem->OpenVolume, 2, FileSystem, &RootFS);
	if (EFI_ERROR(Status)) {
		Print(L"Could not open root filesystem\n");
		return Status;
	}

	// HeliumBoot main loop.
	for (;;) {
		matched_command = FALSE;
		CHAR16 *command, *arguments;
		struct boot_command_tab *cmd_table_ptr;

		Print(L": ");
		Input(L"", filepath, sizeof(filepath) / sizeof(CHAR16));
		Print(L"\n");

		if (StrCmp(L"", filepath) == 0)
			continue;

		// Split the command line to fetch command/program filename,
		// as well as any arguments.
		SplitCommandLine(filepath, &command, &arguments);

		for (cmd_table_ptr = cmd_tab; cmd_table_ptr->cmd_name != NULL; cmd_table_ptr++) {
			if (StrCmp(command, cmd_table_ptr->cmd_name) == 0) {
				if (cmd_table_ptr->cmd_arg_type == CMD_REQUIRED_ARGS && (arguments == NULL || *arguments == L'\0')) {
					Print(L"Error: %s requires arguments.\n", cmd_table_ptr->cmd_name);
					matched_command = TRUE;
					break;
				} else {
					cmd_table_ptr->cmd_func(arguments);
					matched_command = TRUE;
					break;
				}
			}
		}

		if (!matched_command) {
			Status = LoadFile(ImageHandle, SystemTable);
			if (EFI_ERROR(Status))
				Print(L"Failed to load file (%r)\n", Status);
		}
	}

	return EFI_SUCCESS;
}
