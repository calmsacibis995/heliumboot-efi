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

#include "bfs.h"
#include "boot.h"

EFI_STATUS
DetectBFS(EFI_BLOCK_IO_PROTOCOL *BlockIo, UINT32 SliceStartLBA, void *sb_void)
{
    EFI_STATUS Status;
    UINTN BlockSize = BlockIo->Media->BlockSize;
    VOID *Buffer = AllocateZeroPool(BlockSize);
    struct bfs_superblock *sb = (struct bfs_superblock *)sb_void;

    if (!Buffer)
        return EFI_OUT_OF_RESOURCES;

    Status = uefi_call_wrapper(BlockIo->ReadBlocks, 5, BlockIo, BlockIo->Media->MediaId, SliceStartLBA + BFS_SUPEROFF, BlockSize, Buffer);
    if (EFI_ERROR(Status)) {
        FreePool(Buffer);
        return Status;
    }

    MemMove(sb, Buffer, sizeof(struct bfs_superblock));
    FreePool(Buffer);

    if (sb->bdsup_bfsmagic != BFS_MAGIC) {
        PrintToScreen(L"Error: Invalid magic number: 0x%08x\n", sb->bdsup_bfsmagic);
        return EFI_NOT_FOUND;
    }

    return EFI_SUCCESS;
}

/*
 * Read 'len' bytes starting at byte offset 'off' (relative to start of BFS device)
 * into buf. Handles block alignment by reading whole blocks behind the scenes.
 */
static EFI_STATUS
bfs_read_at(struct bfs_mount *mnt, UINT64 off, UINTN len, VOID *buf)
{
    EFI_STATUS Status;
    EFI_BLOCK_IO_PROTOCOL *bio = mnt->bio;
    UINTN blksz = bio->Media->BlockSize;

    UINT64 start_lba = (UINT64)mnt->slice_start_lba + (off / blksz);
    UINTN first_off = off % blksz;
    UINTN to_read = len;
    UINT8 *out = (UINT8 *)buf;

    while (to_read) {
        /* read one block */
        VOID *bblock = AllocatePool(blksz);
        if (!bblock)
            return EFI_OUT_OF_RESOURCES;

        Status = uefi_call_wrapper(bio->ReadBlocks, 5, bio, bio->Media->MediaId, start_lba, blksz, bblock);
        if (EFI_ERROR(Status)) {
            FreePool(bblock);
            return Status;
        }

        UINTN chunk = blksz - first_off;
        if (chunk > to_read)
            chunk = to_read;

        MemMove(out, (UINT8 *)bblock + first_off, chunk);
        FreePool(bblock);

        out += chunk;
        to_read -= chunk;
        start_lba++;
        first_off = 0;
    }

    return EFI_SUCCESS;
}

EFI_STATUS
MountBFS(EFI_BLOCK_IO_PROTOCOL *BlockIo, UINT32 SliceStartLBA, void *sb_buffer, void **mount_out)
{
    if (!BlockIo || !sb_buffer || !mount_out)
        return EFI_INVALID_PARAMETER;

    struct bfs_mount *mnt = AllocateZeroPool(sizeof(*mnt));
    if (!mnt)
        return EFI_OUT_OF_RESOURCES;

    MemMove(&mnt->bfs_sb, sb_buffer, sizeof(mnt->bfs_sb));
    mnt->bio = BlockIo;
    mnt->slice_start_lba = SliceStartLBA;

    *mount_out = mnt;
    return EFI_SUCCESS;
}

/*
 * Very small helper to check if a buffer contains a printable, NUL-terminated
 * ASCII name of reasonable length. Used by the heuristic scanner below.
 */
static BOOLEAN
is_printable_name(const CHAR8 *s, UINTN maxlen)
{
    if (maxlen == 0) return FALSE;
    UINTN i = 0;
    for (; i < maxlen; i++) {
        CHAR8 c = s[i];
        if (c == '\0')
            return (i > 0); /* accept only non-empty names */
        if (c < 32 || c == 0x7f)
            return FALSE;
    }
    return FALSE;
}

static EFI_STATUS
bfs_read_dirent_at(struct bfs_mount *mnt, UINTN idx, struct bfs_dirent *de)
{
    UINT64 off = (UINT64)BFS_DIRSTART + (UINT64)idx * sizeof(struct bfs_dirent);
    INT32 start = mnt->bfs_sb.bdsup_start;

    /* dirent table must lie entirely before filesystem data (start) */
    if (off + sizeof(struct bfs_dirent) > (UINT64)start)
        return EFI_NOT_FOUND;

    return bfs_read_at(mnt, off, sizeof(*de), de);
}

/*
 * Find name for inode 'ino' by scanning the linear directory list at [bdsup_start, bdsup_end).
 * Limit scan to a sane max to avoid walking arbitrary large areas when superblock values are bogus.
 */
static EFI_STATUS
bfs_find_name_for_ino(struct bfs_mount *mnt, UINT16 ino, CHAR8 *name_out, UINTN name_out_sz)
{
    const UINTN MAX_SCAN = 1 << 20; /* 1 MiB max scan to avoid huge walks */
    INT32 start = mnt->bfs_sb.bdsup_start;
    INT32 end = mnt->bfs_sb.bdsup_end;

    if (start <= 0 || end <= start)
        return EFI_DEVICE_ERROR;

    UINT64 scan_end = (UINT64)end;
    if ((UINT64)end - (UINT64)start > MAX_SCAN)
        scan_end = (UINT64)start + MAX_SCAN;

    struct bfs_ldirs lentry;
    for (UINT64 off = (UINT64)start; off + sizeof(lentry) <= scan_end; off += sizeof(lentry)) {
        EFI_STATUS Status = bfs_read_at(mnt, off, sizeof(lentry), &lentry);
        if (EFI_ERROR(Status))
            return Status;

        if (lentry.l_ino != ino)
            continue;

        /* copy and NUL-terminate */
        MemCopy(name_out, lentry.l_name, BFS_MAXFNLEN);
        name_out[BFS_MAXFNLEN] = '\0';

        /* validate printable */
        if (!is_printable_name(name_out, BFS_MAXFNLEN))
            return EFI_NOT_FOUND;

        return EFI_SUCCESS;
    }

    return EFI_NOT_FOUND;
}

static const CHAR16 *
bfs_type_str(vtype_t t)
{
    switch (t) {
        case VDIR:
            return L"DIR";
        case VREG:
            return L"FILE";
        case VLNK:
            return L"LINK";
        case VCHR:
            return L"CHR";
        case VBLK:
            return L"BLK";
        case VFIFO:
            return L"FIFO";
        case VXNAM:
            return L"XNAM";
        default:
            return L"OTHR";
    }
}

/*
 * ReadBFSDir: list directory contents for the provided path.
 * This implementation currently performs a heuristic scan of the BFS data
 * area and prints plausible file names.
 */
EFI_STATUS
ReadBFSDir(void *mount_ctx, const CHAR16 *path)
{
    struct bfs_mount *mnt = (struct bfs_mount *)mount_ctx;
    if (!mnt || !path)
        return EFI_INVALID_PARAMETER;

    PrintToScreen(L"Listing BFS directory: %s\n", path);

    INT32 start = mnt->bfs_sb.bdsup_start;
    INT32 end = mnt->bfs_sb.bdsup_end;
    if (start <= 0 || end <= start) {
        PrintToScreen(L"Invalid BFS data region (start=%d end=%d)\n", start, end);
        return EFI_DEVICE_ERROR;
    }

    /* number of dirent entries stored between BFS_DIRSTART and start */
    UINT64 dirent_space = (UINT64)start - (UINT64)BFS_DIRSTART;
    if (dirent_space < sizeof(struct bfs_dirent)) {
        PrintToScreen(L"No dirent table present\n");
        return EFI_SUCCESS;
    }
    UINTN num_dirents = dirent_space / sizeof(struct bfs_dirent);

    struct bfs_dirent de;
    CHAR8 namebuf[BFS_MAXFNLENN];
    UINTN printed = 0;

    for (UINTN idx = 0; idx < num_dirents; idx++) {
        EFI_STATUS Status = bfs_read_dirent_at(mnt, idx, &de);
        if (EFI_ERROR(Status))
            continue;

        if (de.d_ino == 0)
            continue;

        /* Try to find a name for this inode */
        Status = bfs_find_name_for_ino(mnt, de.d_ino, namebuf, sizeof(namebuf));
        if (EFI_ERROR(Status)) {
            /* fallback to printing inode only */
            PrintToScreen(L"  ino %d  (no name)\n", (UINT32)de.d_ino);
        } else {
            const CHAR16 *type = bfs_type_str(de.d_fattr.va_type);
            PrintToScreen(L"  ino %d  %s  %a\n", (UINT32)de.d_ino, type, namebuf);
        }
        printed++;
    }

    if (printed == 0)
        PrintToScreen(L"  (no directory entries)\n");

    return EFI_SUCCESS;
}

/* Minimal in-memory file handle for BFS that implements the EFI file methods
 * used by the loader (Read, SetPosition, Close). */
struct bfs_file {
    EFI_FILE_PROTOCOL File;
    struct bfs_mount *mnt;
    UINT64 start; /* byte offset on device where file data begins */
    UINT64 size;  /* file size in bytes */
    UINT64 pos;   /* current file position */
    UINT16 ino;   /* inode number */
};

#define BFS_MAX_SCAN (1 << 20) /* reuse heuristic from bfs_find_name_for_ino */

/* Helper: find inode for a given ASCII name (filename without leading backslash).
 * filename is a CHAR16 string (UEFI) and may start with '\' or '/'. */
static EFI_STATUS
bfs_find_inode_by_name(struct bfs_mount *mnt, const CHAR16 *filename, UINT16 *ino_out)
{
    if (!mnt || !filename || !ino_out)
        return EFI_INVALID_PARAMETER;

    /* strip leading slashes */
    while (*filename == L'\\' || *filename == L'/')
        filename++;

    /* convert to ASCII, limit to BFS_MAXFNLEN */
    CHAR8 namebuf[BFS_MAXFNLENN];
    UINTN ni = 0;
    while (*filename != L'\0' && ni < BFS_MAXFNLEN && *filename != L'\\' && *filename != L'/') {
        CHAR16 c = *filename++;
        if (c == L'\0' || c > 0x7f) /* only ASCII names supported here */
            return EFI_UNSUPPORTED;
        namebuf[ni++] = (CHAR8)c;
    }
    if (ni == 0)
        return EFI_INVALID_PARAMETER;
    if (*filename != L'\0' && *filename != L'\\' && *filename != L'/')
        return EFI_UNSUPPORTED; /* name too long */

    namebuf[ni] = '\0';

    const UINT64 start = (UINT64)mnt->bfs_sb.bdsup_start;
    const UINT64 end = (UINT64)mnt->bfs_sb.bdsup_end;
    if (start <= 0 || end <= start)
        return EFI_DEVICE_ERROR;

    UINT64 scan_end = end;
    if (end - start > BFS_MAX_SCAN)
        scan_end = start + BFS_MAX_SCAN;

    struct bfs_ldirs lentry;
    for (UINT64 off = start; off + sizeof(lentry) <= scan_end; off += sizeof(lentry)) {
        EFI_STATUS Status = bfs_read_at(mnt, off, sizeof(lentry), &lentry);
        if (EFI_ERROR(Status))
            return Status;

        /* lentry.l_name is not necessarily NUL-terminated */
        UINTN lname_len = 0;
        while (lname_len < BFS_MAXFNLEN && lentry.l_name[lname_len] != '\0')
            lname_len++;

        if (lname_len == ni && MemCmp(lentry.l_name, namebuf, ni) == 0) {
            *ino_out = lentry.l_ino;
            return EFI_SUCCESS;
        }
    }

    return EFI_NOT_FOUND;
}

/* Read a dirent by inode number. */
static EFI_STATUS
bfs_read_dirent_by_inode(struct bfs_mount *mnt, UINT16 inode, struct bfs_dirent *de)
{
    if (inode < BFSROOTINO)
        return EFI_INVALID_PARAMETER;
    UINTN idx = (UINTN)(inode - BFSROOTINO);
    return bfs_read_dirent_at(mnt, idx, de);
}

/* EFI file methods (minimal) */
static EFI_STATUS EFIAPI
bfs_file_read(EFI_FILE_PROTOCOL *This, UINTN *BufferSize, VOID *Buffer)
{
    if (!This || !BufferSize)
        return EFI_INVALID_PARAMETER;

    struct bfs_file *bf = (struct bfs_file *)This;
    if (*BufferSize == 0)
        return EFI_SUCCESS;

    if (bf->pos >= bf->size) {
        *BufferSize = 0; /* EOF */
        return EFI_SUCCESS;
    }

    UINTN to_read = *BufferSize;
    UINT64 remaining = bf->size - bf->pos;
    if ((UINT64)to_read > remaining)
        to_read = (UINTN)remaining;

    EFI_STATUS Status = bfs_read_at(bf->mnt, bf->start + bf->pos, to_read, Buffer);
    if (EFI_ERROR(Status))
        return Status;

    bf->pos += to_read;
    *BufferSize = to_read;
    return EFI_SUCCESS;
}

static EFI_STATUS EFIAPI
bfs_file_setpos(EFI_FILE_PROTOCOL *This, UINT64 Position)
{
    if (!This)
        return EFI_INVALID_PARAMETER;

    struct bfs_file *bf = (struct bfs_file *)This;

    /* UEFI uses (UINT64)-1 to set position to EOF */
    if (Position == (UINT64)-1) {
        bf->pos = bf->size;
        return EFI_SUCCESS;
    }

    if (Position > bf->size)
        return EFI_INVALID_PARAMETER;

    bf->pos = Position;
    return EFI_SUCCESS;
}

static EFI_STATUS EFIAPI
bfs_file_close(EFI_FILE_PROTOCOL *This)
{
    if (!This)
        return EFI_INVALID_PARAMETER;

    struct bfs_file *bf = (struct bfs_file *)This;
    FreePool(bf);
    return EFI_SUCCESS;
}

/*
 * OpenBFS:
 * - locate inode for filename
 * - read dirent and verify it's a regular file
 * - create an in-memory file handle that implements Read/SetPosition/Close
 */
EFI_STATUS
OpenBFS(void *mount_ctx, const CHAR16 *filename, UINTN mode, void **file_out)
{
    if (!mount_ctx || !filename || !file_out)
        return EFI_INVALID_PARAMETER;

    /* only read mode supported for now */
    if ((mode & EFI_FILE_MODE_READ) == 0)
        return EFI_WRITE_PROTECTED;

    struct bfs_mount *mnt = (struct bfs_mount *)mount_ctx;
    UINT16 ino;
    EFI_STATUS Status = bfs_find_inode_by_name(mnt, filename, &ino);
    if (EFI_ERROR(Status))
        return Status;

    struct bfs_dirent de;
    Status = bfs_read_dirent_by_inode(mnt, ino, &de);
    if (EFI_ERROR(Status))
        return Status;

    if (de.d_fattr.va_type != VREG)
        return EFI_UNSUPPORTED;

    /* compute file start and size; d_sblock is block start, d_eoffset is EOF disk offset */
    UINT64 start = (UINT64)de.d_sblock * (UINT64)BFS_BSIZE;
    UINT64 eof = (UINT64)de.d_eoffset;
    UINT64 size = 0;

    if (eof >= start)
        size = eof - start;
    else if (de.d_eblock >= de.d_sblock) /* fallback to block range */
        size = (UINT64)(de.d_eblock - de.d_sblock + 1) * (UINT64)BFS_BSIZE;
    else
        size = 0;

    struct bfs_file *bf = AllocateZeroPool(sizeof(*bf));
    if (!bf)
        return EFI_OUT_OF_RESOURCES;

    bf->mnt = mnt;
    bf->start = start;
    bf->size = size;
    bf->pos = 0;
    bf->ino = ino;

    /* fill EFI_FILE_PROTOCOL fields (only methods we need) */
    bf->File.Revision = EFI_FILE_PROTOCOL_REVISION;
    bf->File.Open = NULL;
    bf->File.Close = bfs_file_close;
    bf->File.Delete = NULL;
    bf->File.Read = bfs_file_read;
    bf->File.Write = NULL;
    bf->File.GetPosition = NULL;
    bf->File.SetPosition = bfs_file_setpos;
    bf->File.GetInfo = NULL;
    bf->File.SetInfo = NULL;
    bf->File.Flush = NULL;

    *file_out = &bf->File;
    return EFI_SUCCESS;
}

EFI_STATUS
UmountBFS(void *mount)
{
    if (!mount)
        return EFI_INVALID_PARAMETER;
    FreePool(mount);
    return EFI_SUCCESS;
}
