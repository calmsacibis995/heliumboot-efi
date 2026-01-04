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

static UINT32
s5_addr3(const UINT8 *p)
{
    return ((UINT32)p[0]) | ((UINT32)p[1] << 8) | ((UINT32)p[2] << 16);
}

static EFI_STATUS
s5_read_block(struct s5_mount *mp, UINT32 blk, VOID *buf)
{
    UINTN lba = mp->slice_start_lba + (blk * (mp->bsize / SECSIZE));
    ASSERT((mp->bsize % SECSIZE) == 0);

#if defined(DEBUG_BLD)
    PrintToScreen(L"LBA=%lu size=%u\n", lba, mp->bsize);
#endif

    return uefi_call_wrapper(mp->bio->ReadBlocks, 5, mp->bio, mp->bio->Media->MediaId, lba, mp->bsize, buf);
}

static EFI_STATUS
s5_bmap(struct s5_mount *mp, struct s5_inode *ip, UINT32 lbn, UINT32 *phys)
{
    UINT8 buf[FsMAXBSIZE];
    EFI_STATUS Status;

    if (lbn < 10) {
        *phys = ip->i_addr[lbn];
        return (*phys != 0) ? EFI_SUCCESS : EFI_NOT_FOUND;
    }

    lbn -= 10;

    /* single indirect */
    if (lbn < mp->nindir) {
        if (ip->i_addr[10] == 0)
            return EFI_NOT_FOUND;

        Status = s5_read_block(mp, ip->i_addr[10], buf);
        if (EFI_ERROR(Status))
            return Status;

        *phys = s5_addr3(&buf[lbn * 3]);
        return (*phys != 0) ? EFI_SUCCESS : EFI_NOT_FOUND;
    }

    lbn -= mp->nindir;

    /* double indirect */
    if (ip->i_addr[11] == 0)
        return EFI_NOT_FOUND;

    Status = s5_read_block(mp, ip->i_addr[11], buf);
    if (EFI_ERROR(Status))
        return Status;

    UINT32 i1 = lbn / mp->nindir;
    UINT32 i2 = lbn % mp->nindir;

    UINT32 ind1 = s5_addr3(&buf[i1 * 3]);
    if (ind1 == 0)
        return EFI_NOT_FOUND;

    Status = s5_read_block(mp, ind1, buf);
    if (EFI_ERROR(Status))
        return Status;

    *phys = s5_addr3(&buf[i2 * 3]);
    return (*phys != 0) ? EFI_SUCCESS : EFI_NOT_FOUND;
}

static EFI_STATUS
s5_read_inode(struct s5_mount *mp, UINT32 ino, struct s5_inode *ip)
{
    EFI_STATUS Status;
    VOID *buf;
    UINT32 blk, off;
    struct s5_dinode *dp;

    if (ino < 1)
        return EFI_VOLUME_CORRUPTED;

    blk = FsITOD(mp, ino);
    off = FsITOO(mp, ino);

    buf = AllocateZeroPool(mp->bsize);
    if (!buf)
        return EFI_OUT_OF_RESOURCES;

    Status = s5_read_block(mp, blk, buf);
    if (EFI_ERROR(Status))
        goto out;

    dp = (struct s5_dinode *)buf + off;

    ip->i_mode  = dp->di_mode;
    ip->i_nlink = dp->di_nlink;
    ip->i_uid   = dp->di_uid;
    ip->i_gid   = dp->di_gid;
    ip->i_size  = dp->di_size;
    ip->i_atime = dp->di_atime;
    ip->i_mtime = dp->di_mtime;
    ip->i_ctime = dp->di_ctime;
    ip->i_number = ino;

    for (UINTN i = 0; i < NADDR; i++)
        ip->i_addr[i] = s5_addr3((const UINT8 *)&dp->di_addr[i * 3]);

#if defined(DEBUG_BLD)
    PrintToScreen(L"inode %u, i_addr[0]=%u, i_size=%u\n", ino, ip->i_addr[0], ip->i_size);
#endif

out:
    FreePool(buf);
    return Status;
}

static EFI_STATUS
s5_iget(struct s5_mount *mp, UINT32 ino, struct vnode **vpp)
{
    EFI_STATUS Status;
    struct s5_inode *ip;
    struct vnode *vp;

    ip = AllocateZeroPool(sizeof(*ip));
    if (!ip)
        return EFI_OUT_OF_RESOURCES;

    Status = s5_read_inode(mp, ino, ip);
    if (EFI_ERROR(Status)) {
        FreePool(ip);
        return Status;
    }

    vp = AllocateZeroPool(sizeof(*vp));
    if (!vp) {
        FreePool(ip);
        return EFI_OUT_OF_RESOURCES;
    }

    vp->fs_private = ip;
    vp->type = IFTOVT(ip->i_mode);

    *vpp = vp;
    return EFI_SUCCESS;
}

static BOOLEAN
s5_name_match(const INT8 ondisk[DIRSIZ], const CHAR16 *lookup)
{
    UINTN i;

    for (i = 0; i < DIRSIZ; i++) {
        if (lookup[i] == L'\0')
            return ondisk[i] == '\0';

        if ((UINT8)lookup[i] != (UINT8)ondisk[i])
            return FALSE;
    }

    return lookup[i] == L'\0';
}

static BOOLEAN
is_dot_or_dotdot(const struct s5_direct *de)
{
    if (de->d_name[0] == '.' && de->d_name[1] == '\0')
        return TRUE;

    if (de->d_name[0] == '.' && de->d_name[1] == '.' && de->d_name[2] == '\0')
        return TRUE;

    return FALSE;
}

/*
 * Convers the filenames and directory names from ASCII to UTF-16.
 */
static void
s5_name_to_ucs2(const INT8 ondisk[DIRSIZ], CHAR16 out[DIRSIZ + 1])
{
    INTN i;

    for (i = 0; i < DIRSIZ; i++)
        out[i] = (CHAR16)(UINT8)ondisk[i];

    for (i = DIRSIZ - 1; i >= 0; i--) {
        if (out[i] != L'\0' && out[i] != L' ')
            break;
        out[i] = L'\0';
    }

    out[DIRSIZ] = L'\0';
}

EFI_STATUS
ReadS5(struct s5_mount *mp, struct s5_inode *ip, UINT64 off, VOID *dst, UINTN len)
{
    UINT8 buf[FsMAXBSIZE];
    UINTN done = 0;

    while (done < len && off < ip->i_size) {
        UINT32 lbn = off / mp->bsize;
        UINT32 boff = off % mp->bsize;
        UINT32 blk;
        EFI_STATUS Status;

        Status = s5_bmap(mp, ip, lbn, &blk);
        if (EFI_ERROR(Status))
            return Status;

        Status = s5_read_block(mp, blk, buf);
        if (EFI_ERROR(Status))
            return Status;

        UINTN n = mp->bsize - boff;
        if (n > len - done)
            n = len - done;
        if (n > ip->i_size - off)
            n = ip->i_size - off;

        CopyMem((UINT8 *)dst + done, buf + boff, n);

        off += n;
        done += n;
    }

    return EFI_SUCCESS;
}

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

EFI_STATUS
MountS5(EFI_BLOCK_IO_PROTOCOL *BlockIo, UINT32 SliceStartLBA, void *sb_buffer, void **mount_out)
{
    EFI_STATUS Status;
    struct s5_superblock *sb;
    struct s5_mount *mp;
    UINTN i;

    if (!BlockIo || !sb_buffer || !mount_out)
        return EFI_INVALID_PARAMETER;

    sb = (struct s5_superblock *)sb_buffer;
    if (sb->s_magic != FsMAGIC)
        return EFI_UNSUPPORTED;

    mp = AllocateZeroPool(sizeof(*mp));
    if (!mp)
        return EFI_OUT_OF_RESOURCES;

    MemMove(&mp->sb, sb, sizeof(*sb));

    // Set the location of the filesystem on the disk.
    mp->bio = BlockIo;
    mp->slice_start_lba = SliceStartLBA;

    // Set the block size.
    switch (mp->sb.s_type) {
        case Fs1b:
            mp->bsize = 512;
            break;
        case Fs2b:
            mp->bsize = 1024;
            break;
        case Fs4b:
            mp->bsize = 2048;
            break;
        default:
            FreePool(mp);
            return EFI_UNSUPPORTED;
    }

    mp->bmask = mp->bsize - 1;
    mp->inopb = mp->bsize / sizeof(struct s5_dinode);
    mp->nindir = mp->bsize / sizeof(INT32);
    mp->nmask = mp->nindir - 1;

    for (i = mp->bsize, mp->bshift = 0; i > 1; i >>= 1)
        mp->bshift++;

    for (i = mp->nindir, mp->nshift = 0; i > 1; i >>= 1)
        mp->nshift++;

    for (i = mp->inopb, mp->inoshift = 0; i > 1; i >>= 1)
		mp->inoshift++;

    Status = s5_iget(mp, S5ROOTINO, &mp->root_vnode);
    if (EFI_ERROR(Status)) {
        FreePool(mp);
        return Status;
    }

    *mount_out = mp;

    return EFI_SUCCESS;
}

EFI_STATUS
ReadS5Dir(void *mount_ctx, const CHAR16 *path)
{
    EFI_STATUS Status;
    struct s5_mount *mp;
    struct s5_inode *dip;
    struct s5_direct de;
    struct vnode *dir_vp;
    CHAR16 *next, *token;
    CHAR16 PathBuf[256];
    CHAR16 Name16[DIRSIZ + 1];

    if (!mount_ctx || !path)
        return EFI_INVALID_PARAMETER;

    mp = (struct s5_mount *)mount_ctx;
    dir_vp = mp->root_vnode;

    if (dir_vp->type != VDIR) {
        PrintToScreen(L"s5: Root vnode is not a directory\n");
        return EFI_UNSUPPORTED;
    }

    StrnCpy(PathBuf, path, ARRAY_SIZE(PathBuf));
    PathBuf[ARRAY_SIZE(PathBuf) - 1] = L'\0';

    token = PathBuf;
    if (*token == L'\\')
        token++;

    while (*token != L'\0') {
        BOOLEAN found = FALSE;
        UINT64 off = 0;

        next = StrStr(token, L"\\");
        if (next)
            *next = L'\0';

        dip = dir_vp->fs_private;
        if (!dip)
            return EFI_NOT_FOUND;

        while (off < dip->i_size) {
            Status = ReadS5(mp, dip, off, &de, sizeof(de));
            if (EFI_ERROR(Status))
                return Status;

            off += sizeof(de);

            if (de.d_ino == 0)
                continue;

            if (!s5_name_match(de.d_name, token))
                continue;

            Status = s5_iget(mp, de.d_ino, &dir_vp);
            if (EFI_ERROR(Status))
                return Status;

            found = TRUE;
            break;
        }

        if (!found)
            return EFI_NOT_FOUND;

        if (next)
            token = next + 1;
        else
            break;
    }

    PrintToScreen(L"Listing directory: %s\n", path);

    dip = dir_vp->fs_private;
    if (!dip)
        return EFI_NOT_FOUND;

    UINT64 off = 0;
    while (off < dip->i_size) {
        Status = ReadS5(mp, dip, off, &de, sizeof(de));
        if (EFI_ERROR(Status))
            return Status;

        off += sizeof(de);

        if (de.d_ino == 0)
            continue;

        if (is_dot_or_dotdot(&de))
            continue;

        s5_name_to_ucs2(de.d_name, Name16);
        PrintToScreen(L"  %s\n", Name16);
    }

    return EFI_SUCCESS;
}

EFI_STATUS
UmountS5(void *mount)
{
    struct s5_mount *mp = mount;

    if (!mp)
        return EFI_INVALID_PARAMETER;

    if (mp->root_vnode) {
        if (mp->root_vnode->fs_private)
            FreePool(mp->root_vnode->fs_private);
        FreePool(mp->root_vnode);
    }

    FreePool(mp);

    return EFI_SUCCESS;
}
