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
#include "s5fs.h"
#include "vnode.h"

#define SECSIZE 512

enum vtype iftovt_tab[] = {
	VNON, VFIFO, VCHR, VNON,
    VDIR, VXNAM, VBLK, VNON,
	VREG, VNON, VLNK, VNON,
    VNON, VNON, VNON, VNON
};

#define S_IFMT      0xF000
#define IFTOVT(M)   (iftovt_tab[((M) & S_IFMT) >> 12])

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

    MemMove(sb, Buffer, sizeof(struct s5_superblock));
    FreePool(Buffer);

    if (sb->s_magic != FsMAGIC) {
        PrintToScreen(L"Error: Invalid magic number: 0x%08x\n", sb->s_magic);
        return EFI_NOT_FOUND;
    }

    switch (sb->s_type) {
        case Fs1b:
            PrintToScreen(L"512 byte blocks\n");
            break;
        case Fs2b:
            PrintToScreen(L"1024 byte blocks\n");
            break;
        case Fs4b:
            PrintToScreen(L"2048 byte blocks (s52k)\n");
            break;
        default:
            PrintToScreen(L"Unsupported type.\n");
            return EFI_UNSUPPORTED;
    }

    return EFI_SUCCESS;
}

/*
 * Helpers for mount/list implementation
 */
static EFI_STATUS
s5_read_block(struct s5_mount *mnt, UINT32 block, VOID *buf)
{
    EFI_STATUS Status;
    EFI_BLOCK_IO_PROTOCOL *bio = mnt->bio;
    UINT64 sectors_per_block = mnt->bsize / bio->Media->BlockSize;
    UINT64 start_lba = (UINT64)mnt->slice_start_lba + (UINT64)block * sectors_per_block;

    Status = uefi_call_wrapper(bio->ReadBlocks, 5, bio, bio->Media->MediaId, start_lba, mnt->bsize, buf);
    return Status;
}

static EFI_STATUS
s5_read_inode(struct s5_mount *mnt, UINT32 ino, struct s5_dinode *din)
{
    EFI_STATUS Status;
    UINT32 blk = FsITOD(mnt, ino);
    VOID *buf = AllocatePool(mnt->bsize);
    if (!buf)
        return EFI_OUT_OF_RESOURCES;

    Status = s5_read_block(mnt, blk, buf);
    if (EFI_ERROR(Status)) {
        FreePool(buf);
        return Status;
    }

    /* index within block */
    UINT32 idx = FsITOO(mnt, ino);
    UINT32 offset = idx * sizeof(struct s5_dinode);
    if (offset + sizeof(struct s5_dinode) > mnt->bsize) {
        FreePool(buf);
        return EFI_DEVICE_ERROR;
    }

    MemMove(din, (UINT8 *)buf + offset, sizeof(struct s5_dinode));
    FreePool(buf);
    return EFI_SUCCESS;
}

/* Compare a UTF-16 component with the on-disk DIR name (INT8[DIRSIZ]) */
static BOOLEAN
s5_name_cmp(const CHAR16 *comp, const INT8 *dname)
{
    UINTN i = 0;
    /* skip leading slashes in comp */
    while (*comp == L'/' || *comp == L'\\')
        comp++;

    /* component is empty -> no match */
    if (*comp == L'\0')
        return FALSE;

    for (i = 0; i < DIRSIZ; i++) {
        CHAR8 c = dname[i];
        if (c == '\0' || c == ' ')
            break;
        if ((CHAR16)c != comp[i])
            return FALSE;
    }

    /* ensure component length matches dname length */
    /* check remaining chars in comp */
    if (comp[i] != L'\0')
        return FALSE;

    return TRUE;
}

static INT32
s5_daddr(const struct s5_dinode *din, UINTN idx)
{
    UINTN off = idx * 3;
    UINT8 b0 = (UINT8)din->di_addr[off];
    UINT8 b1 = (UINT8)din->di_addr[off + 1];
    UINT8 b2 = (UINT8)din->di_addr[off + 2];

    return (INT32)((UINT32)b0 | ((UINT32)b1 << 8) | ((UINT32)b2 << 16));
}

/*
 * MountS5: build mount context from block device and superblock buffer.
 */
EFI_STATUS
MountS5(EFI_BLOCK_IO_PROTOCOL *BlockIo, UINT32 SliceStartLBA, void *sb_buffer, void **mount_out)
{
    struct s5_mount *mnt;
    struct s5_superblock *sb = (struct s5_superblock *)sb_buffer;

    if (!sb || !mount_out)
        return EFI_INVALID_PARAMETER;

    mnt = AllocateZeroPool(sizeof(*mnt));
    if (!mnt)
        return EFI_OUT_OF_RESOURCES;

    /* copy superblock */
    MemMove(&mnt->sb, sb, sizeof(mnt->sb));

    /* block size and parameters */
    switch (mnt->sb.s_type) {
        case Fs1b:
            mnt->bshift = Fs1b;
            break;
        case Fs2b:
            mnt->bshift = Fs2b;
            break;
        case Fs4b:
            mnt->bshift = Fs4b;
            break;
        default:
            FreePool(mnt);
            return EFI_UNSUPPORTED;
    }
    mnt->bsize = FsBSIZE(mnt->bshift);
    mnt->inopb = mnt->bsize / sizeof(struct s5_dinode);

    /* compute inoshift (log2(inopb)) */
    mnt->inoshift = 0;
    while ((1U << mnt->inoshift) < mnt->inopb)
        mnt->inoshift++;

    mnt->nindir = mnt->bsize / sizeof(INT32);
    mnt->bmask = mnt->bsize - 1;
    mnt->bio = BlockIo;
    mnt->slice_start_lba = SliceStartLBA;
    mnt->root_vnode = NULL; /* not used by current implementation */

    *mount_out = mnt;
    return EFI_SUCCESS;
}

/*
 * ReadS5Dir: list directory contents for the provided path.
 * Path is a UTF-16 string ('\' or '/' separated). If path is root ("\"), list root.
 */
EFI_STATUS
ReadS5Dir(void *mount_ctx, const CHAR16 *path)
{
    struct s5_mount *mnt = (struct s5_mount *)mount_ctx;
    EFI_STATUS Status;
    UINT32 cur_ino = S5ROOTINO;
    CHAR16 compbuf[DIRSIZ + 1];
    const CHAR16 *p = path;
    BOOLEAN last_component = FALSE;

    if (!mnt || !path)
        return EFI_INVALID_PARAMETER;

    /* start from root if path begins with slash */
    while (*p == L'/' || *p == L'\\')
        p++;

    /* If path is empty -> list root */
    if (*p == L'\0') {
        /* cur_ino already root */
    } else {
        /* walk components */
        while (*p != L'\0') {
            UINTN ci = 0;
            while (*p != L'/' && *p != L'\\' && *p != L'\0' && ci < DIRSIZ) {
                compbuf[ci++] = *p++;
            }
            compbuf[ci] = L'\0';
            while (*p == L'/' || *p == L'\\')
                p++;
            last_component = (*p == L'\0');

            /* search compbuf in directory cur_ino */
            UINT32 found_ino = 0;
            /* iterate directory blocks */
            UINTN i;
            for (i = 0; i < NADDR; i++) {
                INT32 b = 0;
                /* read block number from inode - need inode to get addr list */
                struct s5_dinode din;
                Status = s5_read_inode(mnt, cur_ino, &din);
                if (EFI_ERROR(Status))
                    return Status;

                b = s5_daddr(&din, i);
                if (b == 0)
                    continue;

                VOID *dbuf = AllocatePool(mnt->bsize);
                if (!dbuf)
                    return EFI_OUT_OF_RESOURCES;

                Status = s5_read_block(mnt, b, dbuf);
                if (EFI_ERROR(Status)) {
                    FreePool(dbuf);
                    return Status;
                }

                UINTN entries = mnt->bsize / SDSIZ;
                UINTN e;
                for (e = 0; e < entries; e++) {
                    struct s5_direct *de = (struct s5_direct *)((UINT8 *)dbuf + e * SDSIZ);
                    if (de->d_ino == 0)
                        continue;
                    /* compare names: convert compbuf to ASCII compare */
                    /* build a temporary CHAR8 name from compbuf */
                    CHAR8 tmp[DIRSIZ + 1];
                    UINTN k;
                    for (k = 0; k < DIRSIZ; k++) {
                        if (k >= StrLen(compbuf))
                            tmp[k] = '\0';
                        else
                            tmp[k] = (CHAR8)compbuf[k];
                    }
                    tmp[DIRSIZ] = '\0';

                    /* compare with de->d_name (note: de->d_name not nul-terminated) */
                    BOOLEAN match = TRUE;
                    for (k = 0; k < DIRSIZ; k++) {
                        CHAR8 dc = de->d_name[k];
                        if (dc == '\0' || dc == ' ') {
                            /* end of name */
                            if (tmp[k] != '\0')
                                match = FALSE;
                            break;
                        }
                        if (tmp[k] != dc) {
                            match = FALSE;
                            break;
                        }
                    }

                    if (match) {
                        found_ino = de->d_ino;
                        break;
                    }
                }

                FreePool(dbuf);
                if (found_ino != 0)
                    break;
            } /* for each block */

            if (found_ino == 0) {
                PrintToScreen(L"No such file or directory: %s\n", compbuf);
                return EFI_NOT_FOUND;
            }

            if (last_component) {
                cur_ino = found_ino;
                break;
            }

            /* ensure it's a directory */
            struct s5_dinode nd;
            Status = s5_read_inode(mnt, found_ino, &nd);
            if (EFI_ERROR(Status))
                return Status;
            if (IFTOVT(nd.di_mode) != VDIR) {
                PrintToScreen(L"%s is not a directory\n", compbuf);
                return EFI_NOT_FOUND;
            }

            cur_ino = found_ino;
        } /* while components */
    }

    /* Now list directory cur_ino */
    {
        struct s5_dinode din;
        Status = s5_read_inode(mnt, cur_ino, &din);
        if (EFI_ERROR(Status))
            return Status;
        if (IFTOVT(din.di_mode) != VDIR) {
            PrintToScreen(L"Not a directory\n");
            return EFI_UNSUPPORTED;
        }

        PrintToScreen(L"Listing s5 directory: %s\n", path);

        UINTN i;
        for (i = 0; i < NADDR; i++) {
            INT32 b = s5_daddr(&din, i);
            if (b == 0)
                continue;

            VOID *dbuf = AllocatePool(mnt->bsize);
            if (!dbuf)
                return EFI_OUT_OF_RESOURCES;

            Status = s5_read_block(mnt, b, dbuf);
            if (EFI_ERROR(Status)) {
                FreePool(dbuf);
                return Status;
            }

            UINTN entries = mnt->bsize / SDSIZ;
            UINTN e;
            for (e = 0; e < entries; e++) {
                struct s5_direct *de = (struct s5_direct *)((UINT8 *)dbuf + e * SDSIZ);
                if (de->d_ino == 0)
                    continue;
                CHAR16 namew[DIRSIZ + 1];
                UINTN k;
                for (k = 0; k < DIRSIZ; k++) {
                    CHAR8 c = de->d_name[k];
                    if (c == '\0' || c == ' ')
                        break;
                    namew[k] = (CHAR16)c;
                }
                namew[k] = L'\0';

                /* read inode to get type/size */
                struct s5_dinode fi;
                if (s5_read_inode(mnt, de->d_ino, &fi) == EFI_SUCCESS) {
                    vtype_t vt = IFTOVT(fi.di_mode);
                    switch (vt) {
                        case VDIR:
                            PrintToScreen(L"   <DIR>    %s\n", namew);
                            break;
                        case VREG:
                            PrintToScreen(L"  <FILE>    %s  %u bytes\n", namew, fi.di_size);
                            break;
                        case VBLK:
                            PrintToScreen(L"<BLKDEV>    %s\n", namew);
                            break;
                        case VCHR:
                            PrintToScreen(L"<CHRDEV>    %s\n", namew);
                            break;
                        case VLNK:
                            PrintToScreen(L"  <LINK>    %s  %u bytes\n", namew, fi.di_size);
                            break;
                        default:
                            PrintToScreen(L"  <OTHR>    %s\n", namew);
                            break;
                    }
                } else {
                    PrintToScreen(L"  <???>  %s\n", namew);
                }
            }
            FreePool(dbuf);
        }
    }

    return EFI_SUCCESS;
}

/*
 * UmountS5: release mount resources.
 */
EFI_STATUS
UmountS5(void *mount)
{
    struct s5_mount *mnt = (struct s5_mount *)mount;
    if (!mnt)
        return EFI_INVALID_PARAMETER;

    if (mnt->root_vnode)
        mnt->root_vnode = NULL;

    FreePool(mnt);
    return EFI_SUCCESS;
}
