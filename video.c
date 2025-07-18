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

	Status = uefi_call_wrapper(BS->LocateProtocol, 3,
		&GraphicsOutputProtocolGuid, NULL, (void **)&gop);

	if (EFI_ERROR(Status)) {
		Print(L"InitVideo: Failed to locate GOP: %r\n", Status);
		gop = NULL;
		return;
	}

	Print(L"Current resolution: %ux%u\n",
		gop->Mode->Info->HorizontalResolution,
		gop->Mode->Info->VerticalResolution);
	Print(L"Pixel format: %d\n", gop->Mode->Info->PixelFormat);

	Status = uefi_call_wrapper(gop->SetMode, ModeNumber, gop, 0);
	if (EFI_ERROR(Status)) {
		Print(L"Failed to set video mode: %r\n", Status);
		return;
	}
}
