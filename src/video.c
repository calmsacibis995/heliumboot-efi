/*
 * Copyright (c) 2025 Stefanos Stefanidis.
 * All rights reserved.
 */

#include <efi.h>
#include <efilib.h>

#include "boot.h"

EFI_GRAPHICS_OUTPUT_PROTOCOL *gop = NULL;

void
InitVideo(void)
{
	EFI_STATUS Status;
	EFI_GUID GraphicsOutputProtocolGuid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
	UINT32 ModeNumber = 12;		// Default resolution of 1280x768

	// Locate the GOP.
	Status = uefi_call_wrapper(BS->LocateProtocol, 3,
		&GraphicsOutputProtocolGuid, NULL, (void **)&gop);
	if (EFI_ERROR(Status)) {
		Print(L"InitVideo: Failed to locate GOP: %r\n", Status);
		gop = NULL;
		return;
	}

	// Set the video mode.
	Status = uefi_call_wrapper(gop->SetMode, ModeNumber, gop, 0);
	if (EFI_ERROR(Status)) {
		Print(L"Failed to set video mode: %r\n", Status);
		return;
	}

	// Reset console output.
	Status = uefi_call_wrapper(ST->ConOut->Reset, 2, ST->ConOut, TRUE);
	if (EFI_ERROR(Status))
		Print(L"Warning: Failed to reset console: %r\n", Status);

	// Clear the screen.
	Status = uefi_call_wrapper(ST->ConOut->ClearScreen, 1, ST->ConOut);
	if (EFI_ERROR(Status))
		Print(L"Failed to clear screen: %r\n", Status);
}
