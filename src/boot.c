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
#include "vtoc.h"
#include "part.h"

static CHAR16 filepath[256] = {0};			// bootloader file path
UINTN bufsize = sizeof(filepath);

BOOLEAN exit_flag = FALSE;
EFI_HANDLE gImageHandle = NULL;

EFI_STATUS EFIAPI
efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable)
{
	EFI_STATUS Status;
	EFI_LOADED_IMAGE *LoadedImage;
	EFI_FILE_IO_INTERFACE *FileSystem;
	BOOLEAN matched_command;
	EFI_GUID LoadedImageProtocol = EFI_LOADED_IMAGE_PROTOCOL_GUID;
	EFI_GUID FileSystemProtocol = EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID;
	struct svr4_vtoc *Vtoc = NULL;
	UINTN HandleCount;
	UINT32 PartitionStart;
	EFI_HANDLE *HandleBuffer;
	EFI_BLOCK_IO_PROTOCOL *BlockIo;
	EFI_GUID BlockIoProtocolGuid = EFI_BLOCK_IO_PROTOCOL_GUID;
	EFI_FILE_HANDLE RootFS;

	// These must be first. DO NOT PLACE ANY FUNCTION BEFORE THEM!
	InitializeLib(ImageHandle, SystemTable);
	gImageHandle = ImageHandle;

	// Initialize video output, so that we can use graphics.
	Status = InitVideo();
	if (EFI_ERROR(Status)) {
		Print(L"Failed to initialize video: %r\n", Status);
		return Status;
	}

	// Get info about our loaded image.
	Status = uefi_call_wrapper(SystemTable->BootServices->HandleProtocol,
		3, ImageHandle, &LoadedImageProtocol, (void**)&LoadedImage);
	if (EFI_ERROR(Status))
		HeliumBootPanic(Status, L"Could not get loaded image protocol\n");

#if defined(DEV_BLD) || defined(DEBUG_BLD)
#if _LP64
	Print(L"LoadedImage       : 0x%lX\n", LoadedImage);
	Print(L"FilePath          : 0x%lX (%s)\n", LoadedImage->FilePath, LoadedImage->FilePath);
	Print(L"ImageBase         : 0x%lX\n", LoadedImage->ImageBase);
	Print(L"ImageSize         : 0x%lX\n", LoadedImage->ImageSize);
#else
	Print(L"LoadedImage       : 0x%X\n", LoadedImage);
	Print(L"FilePath          : 0x%X\n", LoadedImage->FilePath);
	Print(L"ImageBase         : 0x%X\n", LoadedImage->ImageBase);
	Print(L"ImageSize         : 0x%X\n", LoadedImage->ImageSize);
#endif /* _LP64 */
#endif /* DEBUG_BLD */

	Status = uefi_call_wrapper(BS->LocateHandleBuffer, 5, ByProtocol, &BlockIoProtocolGuid,
		NULL, &HandleCount, &HandleBuffer);
	if (EFI_ERROR(Status))
		HeliumBootPanic(Status, L"Failed to locate block I/O handles\n");

	Status = uefi_call_wrapper(BS->HandleProtocol, 3, HandleBuffer[0], &BlockIoProtocolGuid, (void**)&BlockIo);
	if (EFI_ERROR(Status)) {
		uefi_call_wrapper(BS->FreePool, 1, HandleBuffer);
		HeliumBootPanic(Status, L"Failed to get block I/O protocol\n");
	}

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
		uefi_call_wrapper(BS->FreePool, 1, HandleBuffer);
		HeliumBootPanic(Status, L"Could not get filesystem protocol\n");
	}

	Status = uefi_call_wrapper(FileSystem->OpenVolume, 2, FileSystem, &RootFS);
	if (EFI_ERROR(Status)) {
		uefi_call_wrapper(BS->FreePool, 1, HandleBuffer);
		HeliumBootPanic(Status, L"Could not open filesystem\n");
	}

#if defined(DEBUG_BLD) || defined(DEV_BLD)
	Print(L"\nKyasarin %s (commit %s)\n", getrevision(), getcommitno());	// Yes, we name stuff after Nintendo characters.
#else
	Print(L"\n%s\n", getversion());
#endif

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

	uefi_call_wrapper(BS->FreePool, 1, HandleBuffer);

	return EFI_SUCCESS;
}
