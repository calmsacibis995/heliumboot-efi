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

#include <efi.h>
#include <efilib.h>
#include <stdarg.h>

#include "boot.h"
#include "part.h"

typedef struct BLOCK_IO {
    EFI_BLOCK_IO_PROTOCOL BlockIo;
    EFI_BLOCK_IO_PROTOCOL *ParentBlockIo;
    EFI_LBA StartLba; // Start LBA of the partition
} PARTITION_BLOCK_IO;

void
SplitCommandLine(CHAR16 *line, CHAR16 **command, CHAR16 **arguments)
{
	CHAR16 *p;

	*command = line;
	*arguments = L"";

	p = line;
	while (*p != L'\0' && *p != L' ')
		p++;

	if (*p == L' ') {
		*p = L'\0';
		p++;

		while (*p == L' ')
			p++;

		*arguments = p;
	}
}

UINTN
StrDecimalToUintn(CHAR16 *str)
{
    UINTN result = 0;

    while (*str >= L'0' && *str <= L'9') {
        result = result * 10 + (*str - L'0');
        str++;
    }

    return result;
}

static EFI_STATUS
PartitionReadBlocks(EFI_BLOCK_IO_PROTOCOL *This, UINT32 MediaId, EFI_LBA Lba, UINTN BufferSize, VOID *Buffer)
{
    PARTITION_BLOCK_IO *PartitionBlockIo = (PARTITION_BLOCK_IO *)This;
    EFI_BLOCK_IO_PROTOCOL *ParentBlockIo = PartitionBlockIo->ParentBlockIo;

    EFI_LBA ParentLba = PartitionBlockIo->StartLba + Lba;

    return uefi_call_wrapper(ParentBlockIo->ReadBlocks, 5, ParentBlockIo, MediaId, ParentLba, BufferSize, Buffer);
}

EFI_STATUS
MountAtLba(EFI_BLOCK_IO_PROTOCOL *ParentBlockIo, EFI_LBA StartLba, EFI_SIMPLE_FILE_SYSTEM_PROTOCOL **FileSystem)
{
    EFI_STATUS Status;
    PARTITION_BLOCK_IO *PartitionBlockIo = NULL;
    EFI_HANDLE PartitionHandle = NULL;

    PartitionBlockIo = AllocateZeroPool(sizeof(PARTITION_BLOCK_IO));
    if (!PartitionBlockIo)
        return EFI_OUT_OF_RESOURCES;

    PartitionBlockIo->ParentBlockIo = ParentBlockIo;
    PartitionBlockIo->StartLba = StartLba;

    PartitionBlockIo->BlockIo.Media = ParentBlockIo->Media;
    PartitionBlockIo->BlockIo.Revision = ParentBlockIo->Revision;
    PartitionBlockIo->BlockIo.Reset = ParentBlockIo->Reset;
    PartitionBlockIo->BlockIo.ReadBlocks = PartitionReadBlocks;
    PartitionBlockIo->BlockIo.FlushBlocks = ParentBlockIo->FlushBlocks;

    PartitionHandle = NULL;
    Status = uefi_call_wrapper(BS->InstallMultipleProtocolInterfaces, 5, &PartitionHandle, &BlockIoProtocol,
        &PartitionBlockIo->BlockIo, NULL);
    if (EFI_ERROR(Status)) {
        PrintToScreen(L"No filesystem detected at LBA %llu: %r\n", StartLba, Status);
        uefi_call_wrapper(BS->UninstallMultipleProtocolInterfaces, 5, PartitionHandle, &BlockIoProtocol,
            &PartitionBlockIo->BlockIo, NULL);
        return Status;
    }

    return EFI_SUCCESS;
}

EFI_STATUS
FindPartitionStart(EFI_BLOCK_IO_PROTOCOL *BlockIo, UINT32 *PartitionStart)
{
	EFI_STATUS Status;
	UINTN BlockSize = BlockIo->Media->BlockSize;
	UINT8 *MbrBuffer;
	struct mbr_partition *Partitions;

	if (BlockIo->Media->LogicalPartition) {
		PrintToScreen(L"The drive chosen is not a drive.\n");
		return EFI_UNSUPPORTED;
	}

	Status = uefi_call_wrapper(BS->AllocatePool, 3, EfiLoaderData, BlockSize, (void**)&MbrBuffer);
	if (EFI_ERROR(Status)) {
		Print(L"Failed to allocate memory for MBR: %r\n", Status);
		return Status;
	}

	Status = uefi_call_wrapper(BlockIo->ReadBlocks, 5, BlockIo,
		BlockIo->Media->MediaId, 0, BlockSize, MbrBuffer);
	if (EFI_ERROR(Status)) {
		PrintToScreen(L"Failed to read MBR: %r\n", Status);
		FreePool(MbrBuffer);
		return Status;
	}

	// Check for valid MBR signature.
	if (MbrBuffer[510] != 0x55 || MbrBuffer[511] != 0xAA) {
		PrintToScreen(L"Invalid MBR signature.\n");
		FreePool(MbrBuffer);
		return EFI_NOT_FOUND;
	}

	Partitions = (struct mbr_partition *)(MbrBuffer + 0x1BE);

	for (int i = 0; i < 4; i++) {
		if (Partitions[i].os_type == 0x63) {
			*PartitionStart = Partitions[i].starting_lba;
			PrintToScreen(L"Found System V partition at LBA %u\n", *PartitionStart);
			FreePool(MbrBuffer);
			return EFI_SUCCESS;
		}
	}

	PrintToScreen(L"No System V partition found in MBR\n");
	FreePool(MbrBuffer);
	return EFI_NOT_FOUND;
}

/*
 * Function:
 * HeliumBootPanic()
 *
 * Description:
 * The equivalent of panic() in Unix and its clones. It is called on unresolvable
 * fatal errors.
 *
 * Arguments:
 * Status: EFI status code
 * fmt: Formatted message
 *
 * Return value:
 * None.
 */
void
HeliumBootPanic(EFI_STATUS Status, const CHAR16 *fmt, ...)
{
	EFI_EVENT ev;
	EFI_INPUT_KEY Key;
	UINTN index;
	va_list va;

	va_start(va, fmt);

	if (VideoInitFlag) {
		CHAR16 Buffer[1024];

		PrintToScreen(L"HeliumBoot panic: ");
		UnicodeVSPrint(Buffer, sizeof(Buffer), fmt, va);
		PrintToScreen(Buffer);
		PrintToScreen(L"\nEFI status: 0x%X (%r)\n", Status, Status);
		PrintToScreen(L"Press any key to exit HeliumBoot...\n");
	} else {
		Print(L"HeliumBoot panic: ");
		VPrint(fmt, va);
		Print(L"\nEFI status: 0x%X (%r)\n", Status, Status);
		Print(L"Press any key to exit HeliumBoot...\n");
	}

	va_end(va);

	ev = ST->ConIn->WaitForKey;
	uefi_call_wrapper(BS->WaitForEvent, 3, 1, &ev, &index);
	uefi_call_wrapper(ST->ConIn->ReadKeyStroke, 2, ST->ConIn, &Key);
	uefi_call_wrapper(ST->ConIn->Reset, 2, ST->ConIn, FALSE);
	uefi_call_wrapper(BS->Exit, 3, gImageHandle, Status, 0, NULL);

	// Infinite loop in case we fail to exit.
	for (;;)
		;
}

UINT32
SwapBytes32(UINT32 val)
{
    return ((val & 0x000000FF) << 24) | ((val & 0x0000FF00) << 8) | ((val & 0x00FF0000) >> 8) | ((val & 0xFF000000) >> 24);
}

UINT16
SwapBytes16(UINT16 val)
{
    return (val >> 8) | (val << 8);
}

UINT64
GetTimeSeconds(void)
{
	EFI_STATUS Status;
	EFI_TIME Time;

	Status = uefi_call_wrapper(RT->GetTime, 2, &Time, NULL);
	if (EFI_ERROR(Status))
		return 0;

	return (UINT64)Time.Hour * 3600 + (UINT64)Time.Minute * 60 + (UINT64)Time.Second;
}

void *
MemMove(void *dst, const void *src, UINTN len)
{
	UINT8 *d = (UINT8 *)dst;
	const UINT8 *s = (const UINT8 *)src;

	if (d == s || len == 0)
		return dst;

	if (d < s) {
		// Forward copy
		for (UINTN i = 0; i < len; i++)
			d[i] = s[i];
	} else {
		// Backward copy
		for (UINTN i = len; i != 0; i--)
			d[i - 1] = s[i - 1];
	}

	return dst;
}

EFI_STATUS
ReadFile(EFI_FILE_PROTOCOL *File, CHAR16 *Buffer, UINTN BufferSize, UINTN *Actual)
{
	EFI_STATUS Status;
	UINTN ReadSize;

	if (File == NULL) {
		*Actual = 0;
		return EFI_NOT_READY;
	}

	ReadSize = (UINTN)BufferSize;

	Status = uefi_call_wrapper(File->Read, 3, File, &ReadSize, Buffer);
	*Actual = ReadSize;
	if (EFI_ERROR(Status))
		return Status;

	if (*Actual == BufferSize)
		return EFI_SUCCESS;
	else
		return (*Actual == 0) ? EFI_NOT_FOUND : EFI_END_OF_FILE;
}
