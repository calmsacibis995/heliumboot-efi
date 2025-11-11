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
#include "s5fs.h"

static const char s5_tag[] = "SysV FilesystemDriver (C) 2025 Stefanos Stefanidis, Build: " __DATE__ " " __TIME__ "\n";

EFI_STATUS
DetectS5(EFI_BLOCK_IO_PROTOCOL *BlockIo, UINT32 SliceStartLBA, void *sb_void)
{
    EFI_STATUS Status;
    UINTN BlockSize = BlockIo->Media->BlockSize;
    VOID *Buffer = AllocateZeroPool(BlockSize);
    struct s5_superblock *sb = (struct s5_superblock *)sb_void;

    if (!Buffer)
        return EFI_OUT_OF_RESOURCES;

    Status = uefi_call_wrapper(BlockIo->ReadBlocks, 5, BlockIo, BlockIo->Media->MediaId, SliceStartLBA + SUPERB, BlockSize, Buffer);
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

EFI_STATUS
MountS5(EFI_BLOCK_IO_PROTOCOL *BlockIo, UINT32 SliceStartLBA, void *sb_buffer, void **mount_out)
{
    return EFI_SUCCESS;
}
