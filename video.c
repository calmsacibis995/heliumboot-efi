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
	UINT32 ModeNumber;

	Status = uefi_call_wrapper(BS->LocateProtocol, 3,
		&GraphicsOutputProtocolGuid, NULL, (void **)&gop);

	if (EFI_ERROR(Status)) {
		Print(L"InitVideo(): Failed to locate GOP: %r\n", Status);
		gop = NULL;
		return;
	}

	Print(L"InitVideo(): GOP located\n");
	Print(L"  Current resolution: %ux%u\n",
		gop->Mode->Info->HorizontalResolution,
		gop->Mode->Info->VerticalResolution);
	Print(L"  Pixel format: %d\n", gop->Mode->Info->PixelFormat);

	Print(L"Available video modes:\n");
	for (ModeNumber = 0; ModeNumber < gop->Mode->MaxMode; ++ModeNumber) {
		EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *Info;
		UINTN SizeOfInfo;

		Status = uefi_call_wrapper(gop->QueryMode, 4, gop, ModeNumber,
			&SizeOfInfo, &Info);

		if (EFI_ERROR(Status)) {
			Print(L"  Mode %u: Failed to query (%r)\n", ModeNumber, Status);
			continue;
		}

		Print(L"  Mode %u: %ux%u, PixelFormat=%d\n", ModeNumber,
			Info->HorizontalResolution, Info->VerticalResolution,
			Info->PixelFormat);
	}

	Status = uefi_call_wrapper(gop->SetMode, 12, gop, 0);
	if (EFI_ERROR(Status)) {
		Print(L"Failed to set video mode: %r\n", Status);
		return;
	}
}
