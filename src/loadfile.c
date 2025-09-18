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

/*
 * Common file loading routines.
 */

#include <efi.h>
#include <efilib.h>

#include "boot.h"

EFI_STATUS
LoadFile(CHAR16 *args)
{
}

EFI_STATUS
LoadFileEFI(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable, CHAR16 *path)
{
	EFI_STATUS Status;
	EFI_HANDLE KernelImage;
	EFI_DEVICE_PATH *FilePath;		// EFI file path
	EFI_LOADED_IMAGE *LoadedImage;
	EFI_FILE_HANDLE File, RootFS;

	Status = uefi_call_wrapper(gST->BootServices->HandleProtocol,
		3, gImageHandle, &LoadedImageProtocol, (void**)&LoadedImage);
	if (EFI_ERROR(Status)) {
		Print(L"Could not get loaded image protocol\n");
		return Status;
	}

	// Open the file.
	Status = uefi_call_wrapper(RootFS->Open, 5, RootFS, &File, path, EFI_FILE_MODE_READ, 0);
	if (EFI_ERROR(Status)) {
		Print(L"Could not open file (%r)\n", Status);
		return Status;
	}

	// Get file path.
	FilePath = FileDevicePath(LoadedImage->DeviceHandle, path);
	if (FilePath == NULL) {
		Print(L"Could not create device path for file\n");
		return EFI_NOT_FOUND;
	}

	// Load the file.
	Status = uefi_call_wrapper(SystemTable->BootServices->LoadImage,
		6, FALSE, ImageHandle, FilePath, NULL, 0, &KernelImage);
	if (EFI_ERROR(Status)) {
		Print(L"Failed to load image (%r)\n", Status);
		FreePool(FilePath);
		uefi_call_wrapper(File->Close, 1, File);
		return Status;
	}

	// Free the pool.
	FreePool(FilePath);
	uefi_call_wrapper(File->Close, 1, File);

	// Execute the file.
	Status = uefi_call_wrapper(SystemTable->BootServices->StartImage,
		3, KernelImage, NULL, NULL);
	if (EFI_ERROR(Status)) {
		Print(L"Failed to start kernel image\n");
		return Status;
	}

	return Status;
}
