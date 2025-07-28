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
    EFI_LBA VtocLba;
    UINTN BlockSize;
    EFI_LBA PdinfoLba = PartitionStart + VTOC_SEC;

    if (!BlockIo || !BlockIo->Media || BlockIo->Media->BlockSize == 0) {
        Print(L"OutVtoc and BlockIo are NULL\n");
        return EFI_INVALID_PARAMETER;
    }

    Media = BlockIo->Media;
    BlockSize = Media->BlockSize;

    Status = gBS->AllocatePool(EfiBootServicesData, BlockSize, &Buffer);
    if (EFI_ERROR(Status)) {
        Print(L"Failed to allocate memory: %r\n", Status);
        return Status;
    }

    Status = BlockIo->ReadBlocks(BlockIo, Media->MediaId, PdinfoLba, BlockSize, Buffer);
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
        Print(L"Out of memory allocating VTOC\n");
        gBS->FreePool(Buffer);
        return EFI_OUT_OF_RESOURCES;
    }

    CopyMem(OutVtoc, Vtoc, sizeof(struct svr4_vtoc));
    gBS->FreePool(Buffer);

    return EFI_SUCCESS;
}
