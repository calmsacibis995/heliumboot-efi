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
#include "disk.h"

EFI_STATUS
FindSysVPartition(struct mbr_partition *Partitions, UINT32 *PartitionStart)
{
	UINTN i;

	if (!Partitions || !PartitionStart)
		return EFI_INVALID_PARAMETER;

	for (i = 0; i < 4; i++) {
		if (Partitions[i].os_type == 0x63) {	// 0x63 is for vanilla SysV, OpenServer, UnixWare, and older versions of GNU Hurd.
			*PartitionStart = Partitions[i].starting_lba;
			PrintToScreen(L"Found System V partition at LBA %u\n", *PartitionStart);
			return EFI_SUCCESS;
		}
	}

	return EFI_NOT_FOUND;
}

/*
 * Find the start of the partition list.
 */
EFI_STATUS
GetPartitionData(EFI_BLOCK_IO_PROTOCOL *BlockIo, struct mbr_partition *Partitions)
{
	EFI_STATUS Status;
	UINTN BlockSize = BlockIo->Media->BlockSize;
	UINT8 *MbrBuffer;
    INTN i;

	if (BlockIo->Media->LogicalPartition) {
		PrintToScreen(L"The drive chosen is not a drive.\n");
		return EFI_UNSUPPORTED;
	}

    if (BlockSize < 512)
        return EFI_UNSUPPORTED;

    MbrBuffer = AllocateZeroPool(BlockSize);
    if (!MbrBuffer) {
        PrintToScreen(L"Failed to allocate memory for partition table\n");
        return EFI_OUT_OF_RESOURCES;
    }

	Status = uefi_call_wrapper(BlockIo->ReadBlocks, 5, BlockIo, BlockIo->Media->MediaId, 0, BlockSize, MbrBuffer);
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

	for (i = 0; i < 4; i++)
        MemMove(&Partitions[i], MbrBuffer + 0x1BE + i * sizeof(struct mbr_partition), sizeof(struct mbr_partition));

	FreePool(MbrBuffer);

	return EFI_SUCCESS;
}

EFI_STATUS
GetWholeDiskByIndex(UINTN DiskIndex, EFI_BLOCK_IO_PROTOCOL **DiskBio)
{
    EFI_STATUS Status;
    EFI_HANDLE *Handles = NULL;
    EFI_BLOCK_IO_PROTOCOL *Bio;
    UINTN HandleCount;
    UINTN Found = 0;
    UINTN i;

    Status = uefi_call_wrapper(BS->LocateHandleBuffer, 5, ByProtocol, &BlockIoProtocol, NULL, &HandleCount, &Handles);
    if (EFI_ERROR(Status))
        return Status;

    for (i = 0; i < HandleCount; i++) {
        Status = uefi_call_wrapper(BS->HandleProtocol, 3, Handles[i], &BlockIoProtocol, (void **)&Bio);
        if (EFI_ERROR(Status))
            continue;

        if (!Bio->Media->LogicalPartition) {
            if (Found == DiskIndex) {
                *DiskBio = Bio;
                FreePool(Handles);
                return EFI_SUCCESS;
            }
            Found++;
        }
    }

    FreePool(Handles);
    return EFI_NOT_FOUND;
}

EFI_STATUS
GetWholeDiskBlockIo(EFI_HANDLE *HandleBuffer, UINTN HandleCount,EFI_BLOCK_IO_PROTOCOL **DiskBio)
{
	EFI_STATUS Status;
	EFI_BLOCK_IO_PROTOCOL *Bio;
	UINTN i;

	for (i = 0; i < HandleCount; i++) {
		Status = uefi_call_wrapper(BS->HandleProtocol, 3, HandleBuffer[i], &BlockIoProtocol, (void **)&Bio);
		if (EFI_ERROR(Status))
			continue;

		if (!Bio->Media->LogicalPartition) {
			*DiskBio = Bio;
			return EFI_SUCCESS;
		}
	}

	return EFI_NOT_FOUND;
}
