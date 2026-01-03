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

enum vtype iftovt_tab[] = {
	VNON, VFIFO, VCHR, VNON,
    VDIR, VXNAM, VBLK, VNON,
	VREG, VNON, VLNK, VNON,
    VNON, VNON, VNON, VNON
};

#define S_IFMT      0xF000
#define IFTOVT(M)   (iftovt_tab[((M) & S_IFMT) >> 12])

static EFI_STATUS
s5_read_inode(struct s5_mount *mp, UINT32 ino, struct s5_inode *ip)
{
    EFI_STATUS Status;
    UINTN BlockSize = mp->bsize, i;
    UINT32 inode_start_block, inode_block, inode_offset;
    VOID *Buffer;
    struct s5_dinode *dp;

    inode_start_block = mp->slice_start_lba + 1;
    inode_block = inode_start_block + ((ino - 1) / mp->inopb);
    inode_offset = (ino - 1) % mp->inopb;

    PrintToScreen(L"inode_block=%u inode_offset=%u\n", inode_block, inode_offset);

    Buffer = AllocateZeroPool(BlockSize);
    if (!Buffer)
        return EFI_OUT_OF_RESOURCES;

    Status = uefi_call_wrapper(mp->bio->ReadBlocks, 5, mp->bio, mp->bio->Media->MediaId, inode_block, BlockSize, Buffer);
    if (EFI_ERROR(Status)) {
        FreePool(Buffer);
        return Status;
    }

    UINT8 *b = (UINT8 *)Buffer;
    PrintToScreen(L"first 16 bytes of inode block: %02x %02x %02x %02x ...\n", b[0], b[1], b[2], b[3]);

    dp = (struct s5_dinode *)((UINT8 *)Buffer + inode_offset * sizeof(struct s5_dinode));
    PrintToScreen(L"raw di_mode bytes: %02x %02x\n", ((UINT8*)dp)[0], ((UINT8*)dp)[1]);

    // Copy inode data.
    ip->i_mode = dp->di_mode;
    ip->i_size = dp->di_size;
    ip->i_nlink = dp->di_nlink;
    ip->i_uid = dp->di_uid;
    ip->i_gid = dp->di_gid;
    ip->i_atime = dp->di_atime;
    ip->i_mtime = dp->di_mtime;
    ip->i_ctime = dp->di_ctime;
    ip->i_number = ino;

    PrintToScreen(L"i_mode=0x%04x -> v_type=%d\n", ip->i_mode, IFTOVT(ip->i_mode));

    for (i = 0; i < NADDR; i++) {
        UINT32 off = i * 3;
        ip->i_addr[i] = ((UINT32)(UINT8)dp->di_addr[off + 0] << 16) | ((UINT32)(UINT8)dp->di_addr[off + 1] << 8) | ((UINT32)(UINT8)dp->di_addr[off + 2]);
    }

    FreePool(Buffer);
    return EFI_SUCCESS;
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

    return EFI_SUCCESS;
}

EFI_STATUS
MountS5(EFI_BLOCK_IO_PROTOCOL *BlockIo, UINT32 SliceStartLBA, void *sb_buffer, void **mount_out)
{
    EFI_STATUS Status;
    struct s5_superblock *sb;
    struct s5_mount *mp;

    if (!BlockIo || !sb_buffer || !mount_out)
        return EFI_INVALID_PARAMETER;

    sb = (struct s5_superblock *)sb_buffer;
    if (sb->s_magic != FsMAGIC)
        return EFI_UNSUPPORTED;

    mp = AllocateZeroPool(sizeof(*mp));
    if (!mp)
        return EFI_OUT_OF_RESOURCES;

    CopyMem(&mp->sb, sb, sizeof(*sb));

    // Set the location of the filesystem on the disk.
    mp->bio = BlockIo;
    mp->slice_start_lba = SliceStartLBA;

    // Set the BSHIFT value.
    switch (sb->s_type) {
        case Fs1b:
            mp->bshift = FsMINBSHIFT;
            break;
        case Fs2b:
            mp->bshift = FsMINBSHIFT + 1;
            break;
        case Fs4b:
            mp->bshift = FsMINBSHIFT + 2;
            break;
        default:
            FreePool(mp);
            return EFI_UNSUPPORTED;
    }

    mp->bsize = FsBSIZE(mp->bshift);
    mp->bmask = mp->bsize - 1;
    mp->inopb = mp->bsize / sizeof(struct s5_dinode);
    mp->nindir = mp->bsize / sizeof(INT32);

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
    struct s5_direct *de;
    struct vnode *vp, *dir_vp;
    CHAR16 *next, *token;
    CHAR16 PathBuf[256];
    CHAR16 Name16[DIRSIZ + 1];
    BOOLEAN found = FALSE;
    INTN i, j;

    if (!mount_ctx || !path)
        return EFI_INVALID_PARAMETER;

    mp = (struct s5_mount *)mount_ctx;
    vp = mp->root_vnode;

    if (vp->type != VDIR) {
        PrintToScreen(L"s5: Root vnode is not a directory\n");
        return EFI_UNSUPPORTED;
    }

    StrnCpy(PathBuf, path, ARRAY_SIZE(PathBuf));
    PathBuf[ARRAY_SIZE(PathBuf) - 1] = L'\0';
    token = PathBuf;

    dir_vp = vp;

    if (*token == L'\\')
        token++;

    while (*token != L'\0') {
        next = StrStr(token, L"\\");
        if (next)
            *next = L'\0';

        dip = dir_vp->fs_private;
        if (!dip) {
            PrintToScreen(L"s5: Directory inode not found\n");
            return EFI_NOT_FOUND;
        }

        for (i = 0; i < dip->i_size / sizeof(struct s5_direct); i++) {
            de = (struct s5_dirent *)((UINT8 *)dip->i_map + i * sizeof(struct s5_direct));
            for (j = 0; j < DIRSIZ; j++)
                Name16[j] = (CHAR16)de->d_name[j];
            Name16[DIRSIZ] = L'\0';

            if (StrCmp(Name16, token) == 0) {
                Status = s5_iget(mp, de->d_ino, &dir_vp);
                if (EFI_ERROR(Status))
                    return Status;
                found = TRUE;
                break;
            }
        }

        if (!found) {
            PrintToScreen(L"Directory component '%s' not found\n", token);
            return EFI_NOT_FOUND;
        }

        if (next)
            token = next + 1;
        else
            break;
    }

    PrintToScreen(L"Listing directory: %s\n", path);

    for (i = 0; i < dip->i_size / sizeof(struct s5_direct); i++) {
        de = (struct s5_dirent *)((UINT8 *)dip->i_map + i * sizeof(struct s5_direct));
        for (j = 0; j < DIRSIZ; j++)
            Name16[j] = (CHAR16)de->d_name[j];
        Name16[DIRSIZ] = L'\0';

        if (de->d_ino == 0)
            continue;
        if (StrCmp(Name16, L".") == 0 || StrCmp(Name16, L"..") == 0)
            continue;

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

    if (mp->root_vnode)
        FreePool(mp->root_vnode);

    FreePool(mp);

    return EFI_SUCCESS;
}
