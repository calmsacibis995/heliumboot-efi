#include <efi.h>
#include <efilib.h>

#include "boot.h"
#include "vtoc.h"

EFI_STATUS
ReadVtoc(struct svr4_vtoc **OutVtoc, EFI_BLOCK_IO_PROTOCOL *BlockIo, UINT32 PartitionStart)
{
    EFI_STATUS Status;
    UINTN BlockSize = BlockIo->Media->BlockSize;
    struct svr4_pdinfo *Pdinfo;

    Status = uefi_call_wrapper(BS->AllocatePool, 3, EfiLoaderData, BlockSize, (void**)&Pdinfo);
    if (EFI_ERROR(Status)) {
        Print(L"Failed to allocate memory for pdinfo: %r\n", Status);
        return Status;
    }

    Status = uefi_call_wrapper(BlockIo->ReadBlocks, 5, BlockIo, BlockIo->Media->MediaId,
        PartitionStart, BlockSize, Pdinfo);
    if (EFI_ERROR(Status)) {
        Print(L"Failed to read pdinfo: %r\n", Status);
        FreePool(Pdinfo);
        return Status;
    }

    EFI_LBA VtocLBA = PartitionStart + (Pdinfo->vtoc_ptr / BlockSize);
    UINTN VtocSize = Pdinfo->vtoc_len;

    if (VtocSize % BlockSize != 0)
        VtocSize = ((VtocSize + BlockSize - 1) / BlockSize) * BlockSize;

    Status = uefi_call_wrapper(BS->AllocatePool, 3, EfiLoaderData, VtocSize, (void**)OutVtoc);
    if (EFI_ERROR(Status)) {
        Print(L"Failed to allocate memory for VTOC: %r\n", Status);
        FreePool(Pdinfo);
        return Status;
    }

    Status = uefi_call_wrapper(BlockIo->ReadBlocks, 5, BlockIo, BlockIo->Media->MediaId, VtocLBA,
        VtocSize, *OutVtoc);
    if (EFI_ERROR(Status)) {
        Print(L"Failed to read VTOC: %r\n", Status);
        FreePool(*OutVtoc);
        FreePool(Pdinfo);
        return Status;
    }

    Print(L"pdinfo->vtoc_ptr = 0x%08x\n", Pdinfo->vtoc_ptr);

    if ((*OutVtoc)->v_sanity != VTOC_SANE) {
        Print(L"Invalid VTOC: sanity check failed (0x%X)\n", (*OutVtoc)->v_sanity);
        FreePool(*OutVtoc);
        FreePool(Pdinfo);
        return EFI_NOT_FOUND;
    }

    if ((*OutVtoc)->v_version != V_VERSION) {
        Print(L"Invalid VTOC: version mismatch (0x%08X)\n", (*OutVtoc)->v_version);
        FreePool(*OutVtoc);
        FreePool(Pdinfo);
        return EFI_NOT_FOUND;
    }

    Print(L"VTOC read successfully: Volume Name: %a, Partitions: %d\n", (*OutVtoc)->v_volume, (*OutVtoc)->v_nparts);

    FreePool(Pdinfo);
    return Status;
}
