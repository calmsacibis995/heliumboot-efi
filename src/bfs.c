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

/*
 * bfs.c
 * SVR4/UnixWare Boot File System driver.
 */

#include <efi.h>
#include <efilib.h>

#include "boot.h"
#include "bfs.h"

EFI_STATUS
DetectBFS(EFI_BLOCK_IO_PROTOCOL *BlockIo, UINT32 SliceStartLBA, void *sb_void)
{
    EFI_STATUS Status;
    UINTN BlockSize;
    VOID *Buffer = NULL;
    EFI_LBA SuperBlkLba;
    struct bfs_superblock *on_disk_sb = NULL;
    struct bfs_superblock_info *sb = (struct bfs_superblock_info *)sb_void;

    if (BlockIo == NULL || BlockIo->Media == NULL || BlockIo->Media->BlockSize == 0)
        return EFI_INVALID_PARAMETER;

    BlockSize = BlockIo->Media->BlockSize;
    Buffer = AllocateZeroPool(BlockSize);

    if (!Buffer)
        return EFI_OUT_OF_RESOURCES;

    SuperBlkLba = (EFI_LBA)(SliceStartLBA + BFS_SUPEROFF);

    Status = uefi_call_wrapper(BlockIo->ReadBlocks, 5, BlockIo, BlockIo->Media->MediaId, SuperBlkLba,
        BlockSize, Buffer);
    if (EFI_ERROR(Status)) {
        Print(L"Cannot read BFS superblock at LBA %lu: %r\n", SuperBlkLba, Status);
        FreePool(Buffer);
        return Status;
    }

    on_disk_sb = (struct bfs_superblock *)Buffer;

    if (on_disk_sb->bdsup_bfsmagic != BFS_MAGIC) {
        Print(L"BFS magic number mismatch: expected 0x%08X, found 0x%08X\n", BFS_MAGIC, on_disk_sb->bdsup_bfsmagic);
        FreePool(Buffer);
        return EFI_UNSUPPORTED;
    }

    // Fill in the in-memory superblock structure.

    FreePool(Buffer);
    return EFI_SUCCESS;
}
