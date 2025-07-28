#include <efi.h>
#include <efilib.h>

#include "boot.h"
#include "s5fs.h"

EFI_STATUS
DetectS5(EFI_BLOCK_IO_PROTOCOL *BlockIo, UINT32 SliceStartLBA, struct s5_superblock *sb)
{
    EFI_STATUS Status;
    UINTN BlockSize = BlockIo->Media->BlockSize;
    VOID *Buffer = AllocateZeroPool(BlockSize);

    if (!Buffer)
        return EFI_OUT_OF_RESOURCES;

    Status = BlockIo->ReadBlocks(BlockIo, BlockIo->Media->MediaId, SliceStartLBA + SUPERB, BlockSize, Buffer);
    if (EFI_ERROR(Status)) {
        FreePool(Buffer);
        return Status;
    }

    CopyMem(sb, Buffer, sizeof(struct s5_superblock));
    FreePool(Buffer);

    if (sb->s_magic != FsMAGIC) {
        Print(L"Filesystem in slice is not s5\n");
        return EFI_NOT_FOUND;
    }

    return EFI_SUCCESS;
}
