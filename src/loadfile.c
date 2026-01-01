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

/*
 * Common file loading routines.
 */

#include <efi.h>
#include <efilib.h>

#include <elf.h>

#include "boot.h"

/*
 * Function:
 * LoadFile()
 *
 * Description:
 * Load a binary file to execute. It can be one of:
 *	- a.out
 *	- COFF
 *	- ELF
 *	- EFI PE/COFF
 */
EFI_STATUS
LoadFile(CHAR16 *args)
{
	EFI_STATUS Status;
	EFI_FILE_HANDLE File, RootFS;
	EFI_LOADED_IMAGE *LoadedImage;
	UINTN ReadSize;
	UINT8 Header[64];
	CHAR16 *Path;
	CHAR16 *ProgArgs;
	UINTN DriveIndex = 0;
	UINTN SliceIndex = 0;

	// Process sd(x,y). We also support X:\, where X is a UEFI drive or partition number.
	// sd(x,y) should be used by disks with VTOC, X:\ by everything else.
	if (args && StrnCmp(args, L"sd(", 3) == 0) {
		CHAR16 *p = args + 3;
		UINTN X = 0, Y = 0;

		while (*p >= L'0' && *p <= L'9')
			X = X * 10 + (*p++ - L'0');

		if (*p == L',' || *p == L' ')
			p++;

		while (*p >= L'0' && *p <= L'9')
			Y = Y * 10 + (*p++ - L'0');

		if (*p != L')') {
			PrintToScreen(L"Invalid sd(X,Y) format\n");
			return EFI_INVALID_PARAMETER;
		}

		p++;
		DriveIndex = X;
		SliceIndex = Y;

		if (*p == L':' && *(p+1) == L'\\')
			Path = p + 2;
		else
			Path = L"\\";
	} else if (args && StrLen(args) > 2 && args[1] == L':' && args[2] == L'\\') {
		DriveIndex = args[0] - L'0';
		Path = &args[3];
	}

	Status = uefi_call_wrapper(gBS->HandleProtocol, 3, gImageHandle, &gEfiLoadedImageProtocolGuid, (VOID **)&LoadedImage);
	if (EFI_ERROR(Status)) {
		PrintToScreen(L"Cannot get loaded image protocol: %r\n", Status);
		return Status;
	}

	Status = uefi_call_wrapper(gBS->HandleProtocol, 3, LoadedImage->DeviceHandle, &gEfiSimpleFileSystemProtocolGuid, (VOID **)&RootFS);
	if (EFI_ERROR(Status)) {
		PrintToScreen(L"Cannot get Simple File System protocol: %r\n", Status);
		return Status;
	}

	Status = uefi_call_wrapper(RootFS->Open, 5, RootFS, &File, Path, EFI_FILE_MODE_READ, 0);
	if (EFI_ERROR(Status)) {
		PrintToScreen(L"Cannot open file %s: %r\n", Path, Status);
		return Status;
	}

	ReadSize = sizeof(Header);
	Status = uefi_call_wrapper(File->Read, 3, File, &ReadSize, Header);
	if (EFI_ERROR(Status)) {
		PrintToScreen(L"Cannot read file %s: %r\n", Path, Status);
		uefi_call_wrapper(File->Close, 1, File);
		return Status;
	}

	uefi_call_wrapper(File->SetPosition, 2, File, 0);

	if (IsElf64(Header)) {
		Status = LoadElfBinary(File);
		if (EFI_ERROR(Status)) {
			PrintToScreen(L"Failed to load ELF file %s: %r\n", Status);
			return Status;
		}
	} else if (IsEfiBinary(Header)) {
		Status = LoadEfiBinary(Path, LoadedImage->DeviceHandle);
		if (EFI_ERROR(Status)) {
			PrintToScreen(L"Failed to load EFI binary %s: %r\n", Status);
			return Status;
		}
	} else {
		PrintToScreen(L"Unknown binary format\n");
		return EFI_UNSUPPORTED;
	}

	uefi_call_wrapper(File->Close, 1, File);
	return EFI_SUCCESS;
}
