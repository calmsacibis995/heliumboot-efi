/*
 * Copyright (c) 2025 Stefanos Stefanidis.
 * All rights reserved.
 */

#include <efi.h>
#include <efilib.h>

#include "boot.h"
#include "vtoc.h"
#include "part.h"

CHAR16 filepath[100] = {0};			// bootloader file path
UINTN bufsize = sizeof(filepath);

EFI_FILE_HANDLE RootFS, File;
BOOLEAN exit_flag = FALSE;
EFI_HANDLE gImageHandle = NULL;

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
	EFI_LOADED_IMAGE *LoadedImage;
	EFI_FILE_IO_INTERFACE *FileSystem;
	EFI_INPUT_KEY key;
	BOOLEAN matched_command;
	EFI_GUID LoadedImageProtocol = EFI_LOADED_IMAGE_PROTOCOL_GUID;
	EFI_GUID FileSystemProtocol = EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID;
	struct svr4_vtoc *Vtoc;
	UINTN HandleCount;
	EFI_HANDLE *HandleBuffer;
	EFI_BLOCK_IO_PROTOCOL *BlockIo;

	InitializeLib(ImageHandle, SystemTable);
	gImageHandle = ImageHandle;

	InitVideo();

	// Get info about our loaded image.
	Status = uefi_call_wrapper(SystemTable->BootServices->HandleProtocol,
		3, ImageHandle, &LoadedImageProtocol, (void**)&LoadedImage);
	if (EFI_ERROR(Status)) {
		Print(L"Could not get loaded image protocol\n");
		return Status;
	}

#ifdef DEBUG_BLD
#if _LP64
	Print(L"Loaded image      : 0x%lX\n", LoadedImage);
	Print(L"FilePath          : 0x%lX\n", LoadedImage->FilePath);
	Print(L"ImageBase         : 0x%lX\n", LoadedImage->ImageBase);
	Print(L"ImageSize         : 0x%lX\n", LoadedImage->ImageSize);
#else
	Print(L"Loaded image      : 0x%X\n", LoadedImage);
	Print(L"FilePath          : 0x%X\n", LoadedImage->FilePath);
	Print(L"ImageBase         : 0x%X\n", LoadedImage->ImageBase);
	Print(L"ImageSize         : 0x%X\n", LoadedImage->ImageSize);
#endif /* _LP64 */
#endif /* DEBUG_BLD */

	Status = LibLocateHandle(ByProtocol, &BlockIoProtocol, NULL, &HandleCount, &HandleBuffer);
	if (EFI_ERROR(Status)) {
		Print(L"Failed to locate block I/O handles: %r\n", Status);
		return Status;
	}

	Status = uefi_call_wrapper(BS->HandleProtocol, 3, HandleBuffer[0], &BlockIoProtocol, (void**)&BlockIo);
	if (EFI_ERROR(Status)) {
		Print(L"Failed to get Block I/O protocol: %r\n", Status);
		FreePool(HandleBuffer);
		return Status;
	}

	UINT32 PartitionStart;
	Status = FindPartitionStart(BlockIo, &PartitionStart);
	if (EFI_ERROR(Status)) {
		Print(L"Failed to find partition start: %r\n", Status);
		PartitionStart = 0;		// Default to 0 if no partition found
	} else {
		// Get the VTOC structure.
		Status = ReadVtoc(Vtoc, BlockIo, PartitionStart);
		if (EFI_ERROR(Status))
			Print(L"Warning: VTOC not found on boot volume.\n");
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

	Print(L"\n%s\n", getversion());
	Print(L"Commit %s\n", getcommitno());

	// HeliumBoot main loop.
	for (;;) {
		matched_command = FALSE;
		CHAR16 *command, *arguments;
		struct boot_command_tab *cmd_table_ptr;

		Print(L">> ");
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

		if (exit_flag)
			break;

		if (!matched_command)
			Print(L"Unknown command. Type '?' or 'help' for a list of built-in commands.\n");
	}

	return EFI_SUCCESS;
}
