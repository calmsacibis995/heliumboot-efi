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
#include "cmd.h"
#include "config.h"
#include "disk.h"
#include "menu.h"
#include "serial.h"

static CHAR16 filepath[256] = {0};			// bootloader file path
UINTN bufsize = sizeof(filepath);

BOOLEAN exit_flag = FALSE;
EFI_HANDLE gImageHandle = NULL;

#ifdef _LP64
UINT64 *ActualDestinationAddress;
#else
UINT32 *ActualDestinationAddress;
#endif

#define KRamTargetSize  (8 * 1024 * 1024)		// 8 megabytes.

EFI_STATUS
GetChunk(void)
{
    EFI_STATUS Status;
    EFI_PHYSICAL_ADDRESS PhysAddr = 0;

    Status = uefi_call_wrapper(gBS->AllocatePages, 4, AllocateAnyPages, EfiLoaderData, EFI_SIZE_TO_PAGES(KRamTargetSize), &PhysAddr);
    if (EFI_ERROR(Status)) {
        Print(L"FAULT due to AllocatePages (%r)\n", Status);
        return Status;
    }

#if _LP64
    ActualDestinationAddress = (UINT64 *)(UINTN)PhysAddr;
#else
    ActualDestinationAddress = (UINT32 *)(UINTN)PhysAddr;
#endif

    return EFI_SUCCESS;
}

EFI_STATUS EFIAPI
efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable)
{
	EFI_STATUS Status;
	BOOLEAN IsInternalBoot = FALSE;

	// These must be first. DO NOT PLACE ANY FUNCTION BEFORE THEM!
	InitializeLib(ImageHandle, SystemTable);
	gImageHandle = ImageHandle;

	// Disable UEFI watchdog timer.
	uefi_call_wrapper(BS->SetWatchdogTimer, 4, 0, 0, 0, NULL);

	// Get the memory download area mapped into a chunk.
	Status = GetChunk();
	if (EFI_ERROR(Status))
		HeliumBootPanic(Status, L"GetChunk() failed!\n");

	// Initialize video output, so that we can use graphics.
	Status = InitVideo();
	if (EFI_ERROR(Status))
		HeliumBootPanic(Status, L"Could not initialize video!\n");

#if defined(DEBUG_BLD)
	EFI_LOADED_IMAGE *LoadedImage;

	// Get info about our loaded image.
	Status = uefi_call_wrapper(SystemTable->BootServices->HandleProtocol,
		3, ImageHandle, &LoadedImageProtocol, (void**)&LoadedImage);
	if (EFI_ERROR(Status))
		HeliumBootPanic(Status, L"Could not get loaded image protocol!\n");

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

	Status = ReadConfig(CONFIG_FILE, NULL);
	if (EFI_ERROR(Status))
		HeliumBootPanic(Status, L"Cannot load config file!\n");

	if (!NoMenuLoad)
		StartMenu();

	IsInternalBoot = BootedFromInternalFlash();
	if (IsInternalBoot) {
		if (SearchDrivesRaw())
			DoDownload();
	}

	switch (SerialBaud) {
		case 115200:
			PrintToScreen(L"Initiating YModem-G on port %u @ 115200 baud\n", SerialDownloadPort);
			break;
		case 230400:
			PrintToScreen(L"Initiating YModem-G on port %u @ 230400 baud\n", SerialDownloadPort);
			break;
		default:
			HeliumBootPanic(EFI_INVALID_PARAMETER, L"Invalid baud rate!\n");
			break;
	}

	Status = InitSerialDownload(SerialDownloadPort);
	if (EFI_ERROR(Status))
		HeliumBootPanic(Status, L"Failed to initialize serial download!\n");

	Status = DoDownload();
	if (EFI_ERROR(Status))
		HeliumBootPanic(Status, L"Download failed!\n");

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
