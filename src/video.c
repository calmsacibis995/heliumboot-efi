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
