/*
 * HeliumBoot/EFI - A simple UEFI bootloader.
 *
 * Copyright (c) 2025 Stefanos Stefanidis.
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
#include "vtoc.h"
#include "part.h"
#include "menu.h"

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

	// Disable UEFI watchdog timer.
	uefi_call_wrapper(BS->SetWatchdogTimer, 4, 0, 0, 0, NULL);

	// Initialize video output, so that we can use graphics.
	Status = InitVideo();
	if (EFI_ERROR(Status))
		HeliumBootPanic(Status, L"Could not initialize video!\n");

	// Get info about our loaded image.
	Status = uefi_call_wrapper(SystemTable->BootServices->HandleProtocol,
		3, ImageHandle, &LoadedImageProtocol, (void**)&LoadedImage);
	if (EFI_ERROR(Status))
		HeliumBootPanic(Status, L"Could not get loaded image protocol\n");

#if defined(DEBUG_BLD)
#if _LP64
	PrintToScreen(L"LoadedImage       : 0x%lX\n", LoadedImage);
	PrintToScreen(L"FilePath          : 0x%lX (%s)\n", LoadedImage->FilePath, LoadedImage->FilePath);
	PrintToScreen(L"ImageBase         : 0x%lX\n", LoadedImage->ImageBase);
	PrintToScreen(L"ImageSize         : 0x%lX\n", LoadedImage->ImageSize);
#else
	PrintToScreen(L"LoadedImage       : 0x%X\n", LoadedImage);
	PrintToScreen(L"FilePath          : 0x%X (%s)\n", LoadedImage->FilePath, LoadedImage->FilePath);
	PrintToScreen(L"ImageBase         : 0x%X\n", LoadedImage->ImageBase);
	PrintToScreen(L"ImageSize         : 0x%X\n", LoadedImage->ImageSize);
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
		PrintToScreen(L"Failed to find partition start: %r\n", Status);
		PartitionStart = 0;		// Default to 0 if no partition found
	} else {
		// Get the VTOC structure.
		Status = ReadVtoc(Vtoc, BlockIo, PartitionStart);
		if (EFI_ERROR(Status))
			PrintToScreen(L"Warning: VTOC not found on boot volume.\n");
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

	StartMenu();

	uefi_call_wrapper(BS->FreePool, 1, HandleBuffer);

	return EFI_SUCCESS;
}

void
CommandMonitor(void)
{
	ClearScreen();
	PrintToScreen(L"You are now in the Command Monitor. Type '?' or 'help' for a list of commands.\n\n");

	for (;;) {
		BOOLEAN matched_command = FALSE;
		CHAR16 *command, *arguments;
		struct boot_command_tab *cmd_table_ptr;

		PrintToScreen(L">> ");
		InputToScreen(L"", filepath, sizeof(filepath) / sizeof(CHAR16));
		PrintToScreen(L"\n");

		if (StrCmp(L"", filepath) == 0)
			continue;

		// Split the command line to fetch command/program filename,
		// as well as any arguments.
		SplitCommandLine(filepath, &command, &arguments);

		for (cmd_table_ptr = cmd_tab; cmd_table_ptr->cmd_name != NULL; cmd_table_ptr++) {
			if (StrCmp(command, cmd_table_ptr->cmd_name) == 0) {
				if (cmd_table_ptr->cmd_arg_type == CMD_REQUIRED_ARGS && (arguments == NULL || *arguments == L'\0')) {
					PrintToScreen(L"Error: %s requires arguments.\n", cmd_table_ptr->cmd_name);
					matched_command = TRUE;
					break;
				} else {
					cmd_table_ptr->cmd_func(arguments);
					matched_command = TRUE;
					break;
				}
			}
		}

		if (exit_flag) {
			exit_flag = FALSE;	// Reset the exit flag.
			break;
		}

		if (!matched_command)
			PrintToScreen(L"Unknown command. Type '?' or 'help' for a list of built-in commands.\n");
	}
}
