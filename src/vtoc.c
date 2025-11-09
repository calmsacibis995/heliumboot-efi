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
#include "vtoc.h"

EFI_STATUS
ReadVtoc(struct svr4_vtoc *OutVtoc, EFI_BLOCK_IO_PROTOCOL *BlockIo, UINT32 PartitionStart)
{
    EFI_STATUS Status;
    EFI_BLOCK_IO_MEDIA *Media;
    void *Buffer = NULL;
    UINTN BlockSize;
    EFI_LBA PdinfoLba = PartitionStart + VTOC_SEC;

    if (!BlockIo || !BlockIo->Media || BlockIo->Media->BlockSize == 0) {
        Print(L"OutVtoc and BlockIo are NULL\n");
        return EFI_INVALID_PARAMETER;
    }

    Media = BlockIo->Media;
    BlockSize = Media->BlockSize;

    Status = uefi_call_wrapper(gBS->AllocatePool, 3, EfiBootServicesData, BlockSize, &Buffer);
    if (EFI_ERROR(Status)) {
        Print(L"Failed to allocate memory: %r\n", Status);
        return Status;
    }

    Status = uefi_call_wrapper(BlockIo->ReadBlocks, 5, BlockIo, Media->MediaId, PdinfoLba,
    	BlockSize, Buffer);
    if (EFI_ERROR(Status)) {
        Print(L"Failed to read VTOC: %r\n", Status);
        gBS->FreePool(Buffer);
        return Status;
    }

    struct svr4_pdinfo *Pdinfo = (struct svr4_pdinfo *)Buffer;
    if (Pdinfo->sanity != VALID_PD) {
        Print(L"Error: pdinfo is not sane (0x%X)\n", Pdinfo->sanity);
        gBS->FreePool(Buffer);
        return EFI_COMPROMISED_DATA;
    }

    struct svr4_vtoc *Vtoc = (struct svr4_vtoc *)(Buffer + sizeof(struct svr4_pdinfo));
    if (Vtoc->v_sanity != VTOC_SANE) {
        Print(L"Error: VTOC is not sane (0x%X)\n", Vtoc->v_sanity);
        gBS->FreePool(Buffer);
        return EFI_COMPROMISED_DATA;

    }

    if (Vtoc->v_version != V_VERSION) {
        Print(L"Error: VTOC version mismatch (0x%X)\n", Vtoc->v_version);
        gBS->FreePool(Buffer);
        return EFI_COMPROMISED_DATA;
    }

    OutVtoc = AllocateZeroPool(sizeof(struct svr4_vtoc));
    if (!OutVtoc) {
        Print(L"Failed to allocate memory for VTOC\n");
        gBS->FreePool(Buffer);
        return EFI_OUT_OF_RESOURCES;
    }

    CopyMem(OutVtoc, Vtoc, sizeof(struct svr4_vtoc));
    uefi_call_wrapper(gBS->FreePool, 1, Buffer);

    return EFI_SUCCESS;
}
