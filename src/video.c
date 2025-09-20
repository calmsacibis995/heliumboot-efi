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
	UINT32 ModeNumber, BestMode;
	UINTN InfoSize;
	EFI_GRAPHICS_OUTPUT_PROTOCOL *gop = NULL;
	EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *Info;
	UINT32 Res, MaxRes;

	// Locate the GOP.
	Status = uefi_call_wrapper(BS->LocateProtocol, 3,
		&GraphicsOutputProtocolGuid, NULL, (void **)&gop);
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

	// Set the video mode.
	Status = uefi_call_wrapper(gop->SetMode, BestMode, gop, 0);
	if (EFI_ERROR(Status)) {
		Print(L"Failed to set video mode: %r\n", Status);
		return Status;
	}

	// Reset console output.
	Status = uefi_call_wrapper(ST->ConOut->Reset, 2, ST->ConOut, TRUE);
	if (EFI_ERROR(Status)) {
		Print(L"Warning: Failed to reset console: %r\n", Status);
		return Status;
	}

	// Clear the screen.
	Status = uefi_call_wrapper(ST->ConOut->ClearScreen, 1, ST->ConOut);
	if (EFI_ERROR(Status)) {
		Print(L"Failed to clear screen: %r\n", Status);
		return Status;
	}

	return EFI_SUCCESS;
}
