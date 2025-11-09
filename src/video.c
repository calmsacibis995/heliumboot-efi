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

EFI_STATUS
InitVideo(void)
{
	EFI_STATUS Status;
	EFI_GUID GraphicsOutputProtocolGuid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
	EFI_GRAPHICS_OUTPUT_PROTOCOL *gop = NULL;
	EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *Info;
	UINT32 ModeNumber, Res;
	UINTN InfoSize;
	UINT32 MaxRes = 0;
	UINT32 BestMode = 0;

	// Locate the GOP.
	Status = uefi_call_wrapper(BS->LocateProtocol, 3, &GraphicsOutputProtocolGuid, NULL, (void **)&gop);
	if (EFI_ERROR(Status)) {
		Print(L"InitVideo: Failed to locate GOP: %r\n", Status);
		gop = NULL;
		return Status;
	}

	// Find the best video mode.
	for (ModeNumber = 0; ModeNumber < gop->Mode->MaxMode; ModeNumber++) {
		Status = uefi_call_wrapper(gop->QueryMode, 4, gop, ModeNumber, &InfoSize, &Info);
		if (!EFI_ERROR(Status)) {
			Res = Info->HorizontalResolution * Info->VerticalResolution;
			if (Res > MaxRes) {
				MaxRes = Res;
				BestMode = ModeNumber;
			}
		}
	}

	// Reset console output.
	Status = uefi_call_wrapper(ST->ConOut->Reset, 2, ST->ConOut, TRUE);
	if (EFI_ERROR(Status)) {
		Print(L"Failed to reset console: %r\n", Status);
		return Status;
	}

	// Set the video mode.
	Status = uefi_call_wrapper(gop->SetMode, 2, gop, BestMode);
	if (EFI_ERROR(Status)) {
		Print(L"Failed to set video mode: %r\n", Status);
		return Status;
	}

	// Enable the UEFI blinking text cursor.
	Status = uefi_call_wrapper(ST->ConOut->EnableCursor, 2, ST->ConOut, TRUE);
	if (EFI_ERROR(Status)) {
		Print(L"Failed to enable the cursor: %r\n", Status);
		return Status;
	}

	// Set text color attributes (yellow on black).
	Status = uefi_call_wrapper(ST->ConOut->SetAttribute, 2, ST->ConOut, EFI_TEXT_ATTR(EFI_LIGHTGRAY, EFI_BLACK));
	if (EFI_ERROR(Status)) {
		Print(L"Failed to set text color attributes: %r\n", Status);
		return Status;
	}

	// Clear the screen.
	Status = uefi_call_wrapper(ST->ConOut->ClearScreen, 1, ST->ConOut);
	if (EFI_ERROR(Status)) {
		Print(L"Failed to clear screen: %r\n", Status);
		return Status;
	}

#if defined(DEV_BLD) || defined(DEBUG_BLD)
	UINTN cols = Info->HorizontalResolution / 8;
	UINTN rows = Info->VerticalResolution / 16;

	Print(L"GOP initialized: %ux%u, %u pixels/scanline\n", gop->Mode->Info->HorizontalResolution,
		gop->Mode->Info->VerticalResolution, gop->Mode->Info->PixelsPerScanLine);
	Print(L"Framebuffer base: 0x%lx, size: %lu bytes\n", gop->Mode->FrameBufferBase,
		gop->Mode->FrameBufferSize);
	Print(L"Text mode: %ux%u\n", cols, rows);
#endif

	return EFI_SUCCESS;
}
