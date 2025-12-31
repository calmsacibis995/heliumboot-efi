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
        PrintToScreen(L"Cannot read BFS superblock at LBA %lu: %r\n", SuperBlkLba, Status);
        FreePool(Buffer);
        return Status;
    }

    on_disk_sb = (struct bfs_superblock *)Buffer;

    if (on_disk_sb->bdsup_bfsmagic != BFS_MAGIC) {
        PrintToScreen(L"BFS magic number mismatch: expected 0x%08X, found 0x%08X\n", BFS_MAGIC, on_disk_sb->bdsup_bfsmagic);
        FreePool(Buffer);
        return EFI_UNSUPPORTED;
    }

    // Fill in the in-memory superblock structure.

    FreePool(Buffer);
    return EFI_SUCCESS;
}
