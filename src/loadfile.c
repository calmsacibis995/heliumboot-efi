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

/*
 * Common file loading routines.
 */

#include <efi.h>
#include <efilib.h>

#include "boot.h"
#include "elf.h"

#define MINHDRSZ	1

EFI_STATUS
LoadFile(CHAR16 *args)
{
	EFI_STATUS Status;
	EFI_FILE_PROTOCOL *File;
	UINTN ReadSize;
	CHAR16 Buffer[MINHDRSZ];
	UINTN HeaderMagic;
	struct BootHeader *bhdr;

	Status = ReadFile(File, Buffer, MINHDRSZ, &ReadSize);
	if (EFI_ERROR(Status)) {
		Print(L"Failed to read file: %r\n", Status);
		return;
	}
}

EFI_STATUS
LoadFileEFI(CHAR16 *path)
{
	EFI_STATUS Status;
	EFI_HANDLE KernelImage;
	EFI_DEVICE_PATH *FilePath;
	EFI_LOADED_IMAGE *LoadedImage;
	EFI_FILE_HANDLE File, RootFS;

	Status = uefi_call_wrapper(gST->BootServices->HandleProtocol,
		3, gImageHandle, &LoadedImageProtocol, (void**)&LoadedImage);
	if (EFI_ERROR(Status)) {
		Print(L"Could not get loaded image protocol\n");
		return Status;
	}

	Status = uefi_call_wrapper(gST->BootServices->HandleProtocol, 3, LoadedImage->DeviceHandle,
		&FileSystemProtocol, (void**)&RootFS);
	if (EFI_ERROR(Status)) {
		Print(L"Could not get file system protocol: %r\n", Status);
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
	Status = uefi_call_wrapper(gST->BootServices->LoadImage,
		6, FALSE, gImageHandle, FilePath, NULL, 0, &KernelImage);
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
	Status = uefi_call_wrapper(gST->BootServices->StartImage,
		3, KernelImage, NULL, NULL);
	if (EFI_ERROR(Status)) {
		Print(L"Failed to start EFI image: %r\n", Status);
		return Status;
	}

	return EFI_SUCCESS;
}
