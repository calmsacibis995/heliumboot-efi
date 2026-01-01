/*
 * HeliumBoot/EFI - A simple UEFI bootloader.
 *
 * Copyright (c) 2026 Stefanos Stefanidis.
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

#include <efi.h>
#include <efilib.h>

#include "boot.h"

typedef struct {
    UINT16 e_magic;     /* "MZ" */
    UINT8 _pad[58];
    UINT32 e_lfanew;    /* Offset to PE header */
} DOS_HEADER;

typedef struct {
    UINT32 Signature;   /* "PE\0\0" */
    UINT16 Machine;
    UINT16 NumberOfSections;
    UINT32 _pad[3];
    UINT16 OptionalHeaderSize;
    UINT16 Characteristics;
} PE_HEADER;

typedef struct {
    UINT16 Magic;       /* PE32 = 0x10B, PE32+ = 0x20B */
} OPTIONAL_HEADER;

#define IMAGE_FILE_MACHINE_X64   0x8664
#define IMAGE_FILE_MACHINE_ARM64 0xAA64

BOOLEAN
IsEfiBinary(const void *Buffer)
{
    const DOS_HEADER *Dos;
    const PE_HEADER *Pe;
    const OPTIONAL_HEADER *Opt;

    Dos = (const DOS_HEADER *)Buffer;

    // DOS header.
    if (Dos->e_magic != 0x5A4D)
        return FALSE;

    // PE header must be inside first page.
    if (Dos->e_lfanew > 0x1000)
        return FALSE;

    Pe = (const PE_HEADER *)((const UINT8 *)Buffer + Dos->e_lfanew);

    if (Pe->Signature != 0x00004550)
        return FALSE;

#if defined(X86_64_BLD) || defined(__x86_64__)
    if (Pe->Machine != IMAGE_FILE_MACHINE_X64)
        return FALSE;
#elif defined(AARCH64_BLD) || defined(__aarch64__)
    if (Pe->Machine != IMAGE_FILE_MACHINE_ARM64)
        return FALSE;
#endif

    Opt = (const OPTIONAL_HEADER *)(Pe + 1);

    // Must be PE32+ (64-bit)
    if (Opt->Magic != 0x20B)
        return FALSE;

    return TRUE;
}

EFI_STATUS
LoadEfiBinary(CHAR16 *Path, EFI_HANDLE DeviceHandle)
{
    EFI_STATUS Status;
    EFI_HANDLE Image;
    EFI_DEVICE_PATH *FilePath;

    // Get file path.
	FilePath = FileDevicePath(DeviceHandle, Path);
	if (FilePath == NULL) {
		PrintToScreen(L"Could not create device path for file.\n");
		return EFI_NOT_FOUND;
	}

	// Load the file.
	Status = uefi_call_wrapper(gST->BootServices->LoadImage,
		6, FALSE, gImageHandle, FilePath, NULL, 0, &Image);

    FreePool(FilePath);

	if (EFI_ERROR(Status)) {
		PrintToScreen(L"Failed to load image (%r)\n", Status);
		return Status;
	}

    Status = gBS->StartImage(Image, NULL, NULL);
    if (EFI_ERROR(Status)) {
        PrintToScreen(L"Failed to start image (%r)\n", Status);
        return Status;
    }

    return EFI_SUCCESS;
}
