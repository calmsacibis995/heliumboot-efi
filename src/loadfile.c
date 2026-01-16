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

#include "aout.h"
#include "boot.h"
#include "disk.h"
#include "vtoc.h"

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
	EFI_BLOCK_IO_PROTOCOL *BlockIo = NULL;
	UINTN ReadSize;
	UINT8 Header[64];
	CHAR16 *Path;
	CHAR16 *ProgArgs = NULL;
	UINTN DriveIndex = 0;
	UINTN SliceIndex = 0;

	// Process sd(x,y) only.
	if (!args || StrnCmp(args, L"sd(", 3) != 0) {
		PrintToScreen(L"Only sd(X,Y)[path] syntax is supported\n");
		return EFI_INVALID_PARAMETER;
	}

	CHAR16 *p = args + 3;
	UINTN X = 0, Y = 0;
	BOOLEAN have_x = FALSE;
	BOOLEAN have_y = FALSE;

	while (*p >= L'0' && *p <= L'9') {
		have_x = TRUE;
		X = X * 10 + (*p++ - L'0');
	}

	if (!have_x || *p != L',') {
		PrintToScreen(L"Invalid sd(X,Y) format\n");
		return EFI_INVALID_PARAMETER;
	}

	p++;

	while (*p >= L'0' && *p <= L'9') {
		have_y = TRUE;
		Y = Y * 10 + (*p++ - L'0');
	}

	if (!have_y || *p != L')') {
		PrintToScreen(L"Invalid sd(X,Y) format\n");
		return EFI_INVALID_PARAMETER;
	}

	p++;

	DriveIndex = X;
	SliceIndex = Y;

	if (SliceIndex > V_NUMPAR - 1) {
		PrintToScreen(L"Invalid slice number %d\n", SliceIndex);
		return EFI_INVALID_PARAMETER;
	}

	/* Split path token and optional program arguments */
	if (*p == L'\0') {
		Path = L"\\";
		ProgArgs = NULL;
	} else {
		CHAR16 *q = p;
		/* find end of path token (space/tab separate args) */
		while (*q != L'\0' && *q != L' ' && *q != L'\t')
			q++;
		if (*q != L'\0') {
			*q++ = L'\0'; /* terminate path */
			while (*q == L' ' || *q == L'\t')
				q++;
			ProgArgs = (*q != L'\0') ? q : NULL;
		}

		if (*p == L'\\')
			Path = p;
		else {
			static CHAR16 PathBuf[256];
			PathBuf[0] = L'\\';
			StrnCpy(PathBuf + 1, p, ARRAY_SIZE(PathBuf) - 2);
			PathBuf[ARRAY_SIZE(PathBuf) - 1] = L'\0';
			Path = PathBuf;
		}
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

	Status = GetWholeDiskByIndex(DriveIndex, &BlockIo);
	if (EFI_ERROR(Status)) {
		PrintToScreen(L"Cannot get whole disk by index: %r\n", Status);
		return Status;
	}

	if (BlockIo->Media->RemovableMedia && !BlockIo->Media->LogicalPartition)
		goto open_volume;

open_volume:
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

	if (IsAOut(Header)) {
		Status = LoadAOutBinary(File);
		if (EFI_ERROR(Status)) {
			PrintToScreen(L"Failed to load a.out file %s: %r\n", Status);
			return Status;
		}
	} else if (IsElf64(Header)) {
		Status = LoadElfBinary(File);
		if (EFI_ERROR(Status)) {
			PrintToScreen(L"Failed to load ELF file %s: %r\n", Status);
			return Status;
		}
	} else if (IsEfiBinary(Header)) {
		/* Pass ProgArgs to the EFI loader so it can set LoadOptions */
		Status = LoadEfiBinary(Path, LoadedImage, LoadedImage->DeviceHandle, ProgArgs);
        if (EFI_ERROR(Status)) {
			PrintToScreen(L"Failed to load EFI binary %s: %r\n", Path, Status);
			return Status;
        }
     } else {
		PrintToScreen(L"Unknown binary format\n");
		return EFI_UNSUPPORTED;
	}

	uefi_call_wrapper(File->Close, 1, File);
	return EFI_SUCCESS;
}
