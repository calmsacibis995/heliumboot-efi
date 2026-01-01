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
        PrintToScreen(L"OutVtoc and BlockIo are NULL\n");
        return EFI_INVALID_PARAMETER;
    }

    Media = BlockIo->Media;
    BlockSize = Media->BlockSize;

    Status = uefi_call_wrapper(gBS->AllocatePool, 3, EfiBootServicesData, BlockSize, &Buffer);
    if (EFI_ERROR(Status)) {
        PrintToScreen(L"Failed to allocate memory: %r\n", Status);
        return Status;
    }

    Status = uefi_call_wrapper(BlockIo->ReadBlocks, 5, BlockIo, Media->MediaId, PdinfoLba,
    	BlockSize, Buffer);
    if (EFI_ERROR(Status)) {
        PrintToScreen(L"Failed to read VTOC: %r\n", Status);
        uefi_call_wrapper(gBS->FreePool, 1, Buffer);
        return Status;
    }

    struct svr4_pdinfo *Pdinfo = (struct svr4_pdinfo *)Buffer;
    if (Pdinfo->sanity != VALID_PD) {
        PrintToScreen(L"Error: pdinfo is not sane (0x%X)\n", Pdinfo->sanity);
        uefi_call_wrapper(gBS->FreePool, 1, Buffer);
        return EFI_COMPROMISED_DATA;
    }

    struct svr4_vtoc *Vtoc = (struct svr4_vtoc *)(Buffer + sizeof(struct svr4_pdinfo));
    if (Vtoc->v_sanity != VTOC_SANE) {
        PrintToScreen(L"Error: VTOC is not sane (0x%X)\n", Vtoc->v_sanity);
        uefi_call_wrapper(gBS->FreePool, 1, Buffer);
        return EFI_COMPROMISED_DATA;

    }

    if (Vtoc->v_version != V_VERSION) {
        PrintToScreen(L"Error: VTOC version mismatch (0x%X)\n", Vtoc->v_version);
        uefi_call_wrapper(gBS->FreePool, 1, Buffer);
        return EFI_COMPROMISED_DATA;
    }

    OutVtoc = AllocateZeroPool(sizeof(struct svr4_vtoc));
    if (!OutVtoc) {
        PrintToScreen(L"Failed to allocate memory for VTOC\n");
        uefi_call_wrapper(gBS->FreePool, 1, Buffer);
        return EFI_OUT_OF_RESOURCES;
    }

    CopyMem(OutVtoc, Vtoc, sizeof(struct svr4_vtoc));
    uefi_call_wrapper(gBS->FreePool, 1, Buffer);

    return EFI_SUCCESS;
}
